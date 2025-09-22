// =============================================================================
// collector/src/Drivers/Mqtt/MqttLoadBalancer.cpp
// MQTT 로드밸런싱 및 다중 브로커 관리 구현 (LogManager 호출 수정)
// =============================================================================

#include "Drivers/Mqtt/MqttLoadBalancer.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <cmath>
#include <functional>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttLoadBalancer::MqttLoadBalancer(MqttDriver* parent_driver)
    : parent_driver_(parent_driver)
    , statistics_()
{
    if (!parent_driver_) {
        throw std::invalid_argument("MqttLoadBalancer: parent_driver cannot be null");
    }
    
    LogManager::getInstance().Info("로드밸런서 초기화 완료 - 기본 알고리즘: Round Robin");
}

MqttLoadBalancer::~MqttLoadBalancer() {
    // 모니터링 스레드 종료
    if (monitoring_running_.load()) {
        EnableLoadMonitoring(false);
    }
    
    // 브로커 정리
    {
        std::lock_guard<std::mutex> lock(brokers_mutex_);
        brokers_.clear();
    }
    
    LogManager::getInstance().Info("로드밸런서 정리 완료");
}

// =============================================================================
// 브로커 관리 (헤더 파일 시그니처에 맞춰 void 반환)
// =============================================================================

void MqttLoadBalancer::AddBroker(const std::string& broker_url, const std::string& name, int weight) {
    if (broker_url.empty()) {
        LogManager::getInstance().Error("브로커 URL이 비어있음");
        return;
    }
    
    if (weight < 1 || weight > 100) {
        LogManager::getInstance().Warn("잘못된 가중치 값 (" + std::to_string(weight) + "), 기본값 1로 설정");
        weight = 1;
    }
    
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    // 이미 존재하는 브로커인지 확인
    auto it = brokers_.find(broker_url);
    if (it != brokers_.end()) {
        // 기존 브로커 정보 업데이트
        it->second.weight = weight;
        it->second.name = name.empty() ? broker_url : name;
        it->second.last_update = std::chrono::system_clock::now();
        
        LogManager::getInstance().Info("브로커 정보 업데이트: " + broker_url + " (가중치: " + std::to_string(weight) + ")");
        return;
    }
    
    // 새 브로커 추가
    BrokerLoad new_broker(broker_url, name.empty() ? broker_url : name, weight);
    brokers_[broker_url] = std::move(new_broker);
    
    LogManager::getInstance().Info("새 브로커 추가: " + broker_url + " (가중치: " + std::to_string(weight) + ")");
}

void MqttLoadBalancer::RemoveBroker(const std::string& broker_url) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    auto it = brokers_.find(broker_url);
    if (it == brokers_.end()) {
        LogManager::getInstance().Warn("제거할 브로커를 찾을 수 없음: " + broker_url);
        return;
    }
    
    brokers_.erase(it);
    
    // 해시 캐시에서도 제거
    {
        std::lock_guard<std::mutex> cache_lock(hash_cache_mutex_);
        for (auto cache_it = topic_broker_cache_.begin(); cache_it != topic_broker_cache_.end();) {
            if (cache_it->second == broker_url) {
                cache_it = topic_broker_cache_.erase(cache_it);
            } else {
                ++cache_it;
            }
        }
    }
    
    LogManager::getInstance().Info("브로커 제거 완료: " + broker_url);
}

std::vector<std::string> MqttLoadBalancer::GetAvailableBrokers(const std::vector<std::string>& preferred_brokers) const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    std::vector<std::string> available_brokers;
    
    // 선호 브로커가 지정된 경우 우선 확인
    if (!preferred_brokers.empty()) {
        for (const auto& preferred : preferred_brokers) {
            auto it = brokers_.find(preferred);
            if (it != brokers_.end() && it->second.is_healthy.load()) {
                available_brokers.push_back(preferred);
            }
        }
        
        // 선호 브로커 중 사용 가능한 것이 있으면 반환
        if (!available_brokers.empty()) {
            return available_brokers;
        }
    }
    
    // 모든 사용 가능한 브로커 반환
    for (const auto& [url, broker] : brokers_) {
        if (broker.is_healthy.load()) {
            available_brokers.push_back(url);
        }
    }
    
    return available_brokers;
}

void MqttLoadBalancer::UpdateBrokerLoad(const std::string& broker_url, uint64_t connections,
                                      double response_time_ms, double cpu_usage, double memory_usage) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    auto it = brokers_.find(broker_url);
    if (it != brokers_.end()) {
        // 부하 정보 업데이트
        it->second.active_connections = connections;
        it->second.avg_response_time_ms = response_time_ms;
        it->second.cpu_usage = cpu_usage;
        it->second.memory_usage = memory_usage;
        it->second.last_update = std::chrono::system_clock::now();
        
        // 콜백 호출
        if (load_update_callback_) {
            load_update_callback_(broker_url, it->second);
        }
    }
}

// =============================================================================
// 로드밸런싱 브로커 선택 (헤더 파일 시그니처에 맞춤)
// =============================================================================

std::string MqttLoadBalancer::SelectBroker(const std::string& topic, size_t /* message_size */) {
    statistics_.total_requests.fetch_add(1);
    
    // 사용 가능한 브로커 목록 가져오기
    auto available_brokers = GetAvailableBrokers();
    if (available_brokers.empty()) {
        statistics_.failed_routes.fetch_add(1);
        LogManager::getInstance().Error("사용 가능한 브로커가 없음");
        return "";
    }
    
    std::string selected_broker;
    LoadBalanceAlgorithm algorithm = default_algorithm_;
    
    // 라우팅 규칙 확인
    selected_broker = ApplyRoutingRules(topic, available_brokers);
    if (!selected_broker.empty()) {
        algorithm = GetRuleAlgorithm(topic);
    } else {
        // 기본 알고리즘 사용
        switch (default_algorithm_) {
            case LoadBalanceAlgorithm::ROUND_ROBIN:
                selected_broker = SelectByRoundRobin(available_brokers);
                break;
            case LoadBalanceAlgorithm::LEAST_CONNECTIONS:
                selected_broker = SelectByLeastConnections(available_brokers);
                break;
            case LoadBalanceAlgorithm::WEIGHTED_ROUND_ROBIN:
                selected_broker = SelectByWeightedRoundRobin(available_brokers);
                break;
            case LoadBalanceAlgorithm::RESPONSE_TIME:
                selected_broker = SelectByResponseTime(available_brokers);
                break;
            case LoadBalanceAlgorithm::RANDOM:
                selected_broker = SelectByRandom(available_brokers);
                break;
            case LoadBalanceAlgorithm::HASH_BASED:
                selected_broker = SelectByHash(topic, available_brokers);
                break;
            default:
                selected_broker = SelectByRoundRobin(available_brokers);
                break;
        }
    }
    
    if (selected_broker.empty()) {
        statistics_.failed_routes.fetch_add(1);
        LogManager::getInstance().Error("브로커 선택 실패 - 토픽: " + topic);
        return "";
    }
    
    // 통계 업데이트
    statistics_.successful_routes.fetch_add(1);
    statistics_.broker_usage_count[selected_broker]++;
    statistics_.algorithm_usage_count[algorithm]++;
    
    // 콜백 호출
    if (route_decision_callback_) {
        route_decision_callback_(topic, selected_broker, algorithm);
    }
    
    LogManager::getInstance().Debug("브로커 선택 완료 - 토픽: " + topic + ", 브로커: " + selected_broker);
    
    return selected_broker;
}

// =============================================================================
// 로드밸런싱 알고리즘 구현
// =============================================================================

std::string MqttLoadBalancer::SelectByRoundRobin(const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    size_t index = round_robin_index_.fetch_add(1) % available_brokers.size();
    return available_brokers[index];
}

std::string MqttLoadBalancer::SelectByLeastConnections(const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    std::string best_broker;
    uint64_t min_connections = UINT64_MAX;
    
    for (const auto& broker_url : available_brokers) {
        auto it = brokers_.find(broker_url);
        if (it != brokers_.end()) {
            uint64_t connections = it->second.active_connections.load();
            if (connections < min_connections) {
                min_connections = connections;
                best_broker = broker_url;
            }
        }
    }
    
    return best_broker;
}

std::string MqttLoadBalancer::SelectByWeightedRoundRobin(const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    // 가중치 합계 계산
    int total_weight = 0;
    std::vector<std::pair<std::string, int>> weighted_brokers;
    
    for (const auto& broker_url : available_brokers) {
        auto it = brokers_.find(broker_url);
        if (it != brokers_.end()) {
            total_weight += it->second.weight;
            weighted_brokers.emplace_back(broker_url, it->second.weight);
        }
    }
    
    if (total_weight == 0) return available_brokers[0];
    
    // 가중치 기반 선택
    int random_weight = std::rand() % total_weight;
    int current_weight = 0;
    
    for (const auto& [broker_url, weight] : weighted_brokers) {
        current_weight += weight;
        if (random_weight < current_weight) {
            return broker_url;
        }
    }
    
    return weighted_brokers.back().first;
}

std::string MqttLoadBalancer::SelectByResponseTime(const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    std::string best_broker;
    double min_response_time = std::numeric_limits<double>::max();
    
    for (const auto& broker_url : available_brokers) {
        auto it = brokers_.find(broker_url);
        if (it != brokers_.end()) {
            double response_time = it->second.avg_response_time_ms.load();
            if (response_time < min_response_time) {
                min_response_time = response_time;
                best_broker = broker_url;
            }
        }
    }
    
    return best_broker;
}

std::string MqttLoadBalancer::SelectByRandom(const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, available_brokers.size() - 1);
    
    return available_brokers[dis(gen)];
}

std::string MqttLoadBalancer::SelectByHash(const std::string& topic, const std::vector<std::string>& available_brokers) {
    if (available_brokers.empty()) return "";
    
    // 캐시에서 확인
    {
        std::lock_guard<std::mutex> cache_lock(hash_cache_mutex_);
        auto it = topic_broker_cache_.find(topic);
        if (it != topic_broker_cache_.end()) {
            // 캐시된 브로커가 여전히 사용 가능한지 확인
            if (std::find(available_brokers.begin(), available_brokers.end(), it->second) != available_brokers.end()) {
                return it->second;
            } else {
                // 캐시에서 제거
                topic_broker_cache_.erase(it);
            }
        }
    }
    
    // 해시 기반 선택
    std::hash<std::string> hasher;
    size_t hash_value = hasher(topic);
    size_t index = hash_value % available_brokers.size();
    std::string selected_broker = available_brokers[index];
    
    // 캐시 업데이트
    {
        std::lock_guard<std::mutex> cache_lock(hash_cache_mutex_);
        topic_broker_cache_[topic] = selected_broker;
    }
    
    return selected_broker;
}

// =============================================================================
// 라우팅 규칙 관리 (헤더 파일 시그니처에 맞춰 void 반환)
// =============================================================================

void MqttLoadBalancer::AddRoutingRule(const RoutingRule& rule) {
    if (rule.rule_name.empty() || rule.topic_pattern.empty()) {
        LogManager::getInstance().Error("라우팅 규칙 이름 또는 토픽 패턴이 비어있음");
        return;
    }
    
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    // 중복 규칙 확인
    auto it = std::find_if(routing_rules_.begin(), routing_rules_.end(),
        [&rule](const RoutingRule& existing_rule) {
            return existing_rule.rule_name == rule.rule_name;
        });
    
    if (it != routing_rules_.end()) {
        // 기존 규칙 업데이트
        *it = rule;
        LogManager::getInstance().Info("라우팅 규칙 업데이트: " + rule.rule_name);
    } else {
        // 새 규칙 추가
        routing_rules_.push_back(rule);
        LogManager::getInstance().Info("새 라우팅 규칙 추가: " + rule.rule_name);
    }
    
    // 우선순위별 정렬
    std::sort(routing_rules_.begin(), routing_rules_.end(),
        [](const RoutingRule& a, const RoutingRule& b) {
            return a.priority > b.priority;
        });
}

void MqttLoadBalancer::RemoveRoutingRule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    auto it = std::find_if(routing_rules_.begin(), routing_rules_.end(),
        [&rule_name](const RoutingRule& rule) {
            return rule.rule_name == rule_name;
        });
    
    if (it == routing_rules_.end()) {
        LogManager::getInstance().Warn("제거할 라우팅 규칙을 찾을 수 없음: " + rule_name);
        return;
    }
    
    routing_rules_.erase(it);
    LogManager::getInstance().Info("라우팅 규칙 제거 완료: " + rule_name);
}

std::string MqttLoadBalancer::ApplyRoutingRules(const std::string& topic, const std::vector<std::string>& available_brokers) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    for (const auto& rule : routing_rules_) {
        if (!rule.enabled) continue;
        
        // 토픽 패턴 매칭 (간단한 와일드카드 지원)
        if (MatchTopicPattern(rule.topic_pattern, topic)) {
            // 선호 브로커 중 사용 가능한 브로커 찾기
            for (const auto& preferred_broker : rule.preferred_brokers) {
                if (std::find(available_brokers.begin(), available_brokers.end(), preferred_broker) != available_brokers.end()) {
                    LogManager::getInstance().Debug("라우팅 규칙 적용: " + rule.rule_name + " -> " + preferred_broker);
                    return preferred_broker;
                }
            }
        }
    }
    
    return ""; // 적용된 규칙 없음
}

bool MqttLoadBalancer::MatchTopicPattern(const std::string& pattern, const std::string& topic) const {
    // 간단한 와일드카드 패턴 매칭
    // "*" = 모든 문자, "?" = 단일 문자
    
    if (pattern == "*") return true;
    if (pattern == topic) return true;
    
    // TODO: 더 복잡한 패턴 매칭 구현 (정규식 등)
    // 현재는 기본적인 매칭만 지원
    
    return false;
}

LoadBalanceAlgorithm MqttLoadBalancer::GetRuleAlgorithm(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    for (const auto& rule : routing_rules_) {
        if (rule.enabled && MatchTopicPattern(rule.topic_pattern, topic)) {
            return rule.algorithm;
        }
    }
    
    return default_algorithm_;
}

// =============================================================================
// 부하 모니터링 (헤더 파일 시그니처에 맞춤)
// =============================================================================

void MqttLoadBalancer::EnableLoadMonitoring(bool enable, int interval_ms) {
    if (enable) {
        if (monitoring_running_.load()) {
            LogManager::getInstance().Warn("부하 모니터링이 이미 실행 중");
            return;
        }
        
        monitoring_interval_ms_ = std::max(1000, interval_ms); // 최소 1초
        load_monitoring_enabled_ = true;
        monitoring_running_ = true;
        
        load_monitoring_thread_ = std::thread(&MqttLoadBalancer::LoadMonitoringLoop, this);
        
        LogManager::getInstance().Info("부하 모니터링 시작 - 간격: " + std::to_string(monitoring_interval_ms_) + "ms");
    } else {
        if (!monitoring_running_.load()) {
            return;
        }
        
        load_monitoring_enabled_ = false;
        monitoring_running_ = false;
        
        // 모니터링 스레드 깨우기
        {
            std::lock_guard<std::mutex> lock(monitoring_mutex_);
            monitoring_cv_.notify_all();
        }
        
        if (load_monitoring_thread_.joinable()) {
            load_monitoring_thread_.join();
        }
        
        LogManager::getInstance().Info("부하 모니터링 중지");
    }
}

void MqttLoadBalancer::LoadMonitoringLoop() {
    while (monitoring_running_.load()) {
        try {
            // 모든 브로커의 부하 상태 확인
            std::vector<std::string> broker_urls;
            {
                std::lock_guard<std::mutex> lock(brokers_mutex_);
                for (const auto& [url, broker] : brokers_) {
                    broker_urls.push_back(url);
                }
            }
            
            for (const auto& broker_url : broker_urls) {
                // 브로커 헬스 체크 (실제 구현은 MqttDriver와 연동 필요)
                bool is_healthy = UpdateBrokerHealth(broker_url);
                
                {
                    std::lock_guard<std::mutex> lock(brokers_mutex_);
                    auto it = brokers_.find(broker_url);
                    if (it != brokers_.end()) {
                        it->second.is_healthy = is_healthy;
                        it->second.last_update = std::chrono::system_clock::now();
                    }
                }
            }
            
            // 대기
            std::unique_lock<std::mutex> lock(monitoring_mutex_);
            monitoring_cv_.wait_for(lock, std::chrono::milliseconds(monitoring_interval_ms_));
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("부하 모니터링 오류: " + std::string(e.what()));
        }
    }
}

bool MqttLoadBalancer::UpdateBrokerHealth(const std::string& /* broker_url */) {
    // TODO: 실제 브로커 헬스 체크 구현
    // 현재는 항상 true 반환 (placeholder)
    return true;
}

// =============================================================================
// 설정 및 상태 조회
// =============================================================================

void MqttLoadBalancer::SetDefaultAlgorithm(LoadBalanceAlgorithm algorithm) {
    default_algorithm_ = algorithm;
    LogManager::getInstance().Info("기본 로드밸런싱 알고리즘 변경: " + std::to_string(static_cast<int>(algorithm)));
}

LoadBalanceAlgorithm MqttLoadBalancer::GetDefaultAlgorithm() const {
    return default_algorithm_;
}

void MqttLoadBalancer::EnableLoadBalancing(bool enable) {
    load_balancing_enabled_ = enable;
    LogManager::getInstance().Info("로드밸런싱 " + std::string(enable ? "활성화" : "비활성화"));
}

bool MqttLoadBalancer::IsLoadBalancingEnabled() const {
    return load_balancing_enabled_.load();
}

size_t MqttLoadBalancer::GetBrokerCount() const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    return brokers_.size();
}

size_t MqttLoadBalancer::GetHealthyBrokerCount() const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    size_t healthy_count = 0;
    for (const auto& [url, broker] : brokers_) {
        if (broker.is_healthy.load()) {
            healthy_count++;
        }
    }
    
    return healthy_count;
}

// BrokerLoad 반환 시 복사 생성자 문제 해결 - 개별 값들만 복사하여 새 객체 생성
BrokerLoad MqttLoadBalancer::GetBrokerLoad(const std::string& broker_url) const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    auto it = brokers_.find(broker_url);
    if (it != brokers_.end()) {
        // atomic 멤버들의 값을 복사하여 새 BrokerLoad 객체 생성
        BrokerLoad copy_broker;
        copy_broker.broker_url = it->second.broker_url;
        copy_broker.name = it->second.name;
        copy_broker.weight = it->second.weight;
        copy_broker.active_connections = it->second.active_connections.load();
        copy_broker.total_messages = it->second.total_messages.load();
        copy_broker.pending_messages = it->second.pending_messages.load();
        copy_broker.avg_response_time_ms = it->second.avg_response_time_ms.load();
        copy_broker.cpu_usage = it->second.cpu_usage.load();
        copy_broker.memory_usage = it->second.memory_usage.load();
        copy_broker.is_healthy = it->second.is_healthy.load();
        copy_broker.last_update = it->second.last_update;
        
        return copy_broker;
    }
    
    return BrokerLoad(); // 빈 브로커 정보 반환
}

// LoadBalancingStats 반환 시 복사 생성자 문제 해결
LoadBalancingStats MqttLoadBalancer::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // atomic 멤버들의 값을 복사하여 새 LoadBalancingStats 객체 생성
    LoadBalancingStats copy_stats;
    copy_stats.total_requests = statistics_.total_requests.load();
    copy_stats.successful_routes = statistics_.successful_routes.load();
    copy_stats.failed_routes = statistics_.failed_routes.load();
    copy_stats.broker_usage_count = statistics_.broker_usage_count;
    copy_stats.algorithm_usage_count = statistics_.algorithm_usage_count;
    copy_stats.start_time = statistics_.start_time;
    
    return copy_stats;
}

std::vector<RoutingRule> MqttLoadBalancer::GetRoutingRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return routing_rules_;
}

std::string MqttLoadBalancer::GetStatusJSON() const {
    std::ostringstream json;
    json << "{";
    json << "\"load_balancing_enabled\":" << (load_balancing_enabled_.load() ? "true" : "false") << ",";
    json << "\"monitoring_enabled\":" << (load_monitoring_enabled_.load() ? "true" : "false") << ",";
    json << "\"default_algorithm\":" << static_cast<int>(default_algorithm_) << ",";
    json << "\"broker_count\":" << GetBrokerCount() << ",";
    json << "\"healthy_broker_count\":" << GetHealthyBrokerCount() << ",";
    json << "\"total_requests\":" << statistics_.total_requests.load() << ",";
    json << "\"successful_routes\":" << statistics_.successful_routes.load() << ",";
    json << "\"failed_routes\":" << statistics_.failed_routes.load() << ",";
    json << "\"success_rate\":" << statistics_.GetSuccessRate();
    json << "}";
    
    return json.str();
}

// =============================================================================
// 콜백 설정
// =============================================================================

void MqttLoadBalancer::SetLoadUpdateCallback(LoadUpdateCallback callback) {
    load_update_callback_ = callback;
}

void MqttLoadBalancer::SetRouteDecisionCallback(RouteDecisionCallback callback) {
    route_decision_callback_ = callback;
}

} // namespace Drivers
} // namespace PulseOne