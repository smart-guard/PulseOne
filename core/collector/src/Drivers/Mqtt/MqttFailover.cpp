// =============================================================================
// collector/src/Drivers/Mqtt/MqttFailover.cpp
// MQTT 페일오버 및 재연결 강화 기능 구현
// =============================================================================

#include "Drivers/Mqtt/MqttFailover.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef HAVE_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Drivers {

using namespace std::chrono;

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttFailover::MqttFailover(MqttDriver* parent_driver)
    : parent_driver_(parent_driver)
{
    // 기본 재연결 전략 설정
    strategy_.enabled = true;
    strategy_.max_attempts = 10;
    strategy_.initial_delay_ms = 1000;
    strategy_.max_delay_ms = 30000;
    strategy_.backoff_multiplier = 2.0;
    strategy_.exponential_backoff = true;
    strategy_.connection_timeout_ms = 10000;
    strategy_.health_check_interval_ms = 30000;
    strategy_.enable_jitter = true;
    
    // 헬스 체크 스레드 시작
    if (health_check_enabled_) {
        health_check_running_ = true;
        health_check_thread_ = std::thread(&MqttFailover::HealthCheckLoop, this);
    }
}

MqttFailover::~MqttFailover() {
    // 모든 작업 중지
    should_stop_ = true;
    is_reconnecting_ = false;
    health_check_running_ = false;
    
    // 조건 변수들 깨우기
    reconnect_cv_.notify_all();
    health_check_cv_.notify_all();
    
    // 스레드들 정리
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }
    
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
}

// =============================================================================
// 설정 및 제어
// =============================================================================

void MqttFailover::SetReconnectStrategy(const ReconnectStrategy& strategy) {
    strategy_ = strategy;
}

void MqttFailover::SetBrokers(const std::vector<BrokerInfo>& brokers) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    brokers_ = brokers;
    
    // 우선순위 기준으로 정렬
    std::sort(brokers_.begin(), brokers_.end(),
              [](const BrokerInfo& a, const BrokerInfo& b) {
                  return a.priority < b.priority;
              });
}

void MqttFailover::AddBroker(const std::string& broker_url, const std::string& name, int priority) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    // 중복 체크
    auto it = std::find_if(brokers_.begin(), brokers_.end(),
                          [&broker_url](const BrokerInfo& info) {
                              return info.url == broker_url;
                          });
    
    if (it == brokers_.end()) {
        brokers_.emplace_back(broker_url, name, priority);
        
        // 우선순위 기준으로 재정렬
        std::sort(brokers_.begin(), brokers_.end(),
                  [](const BrokerInfo& a, const BrokerInfo& b) {
                      return a.priority < b.priority;
                  });
    }
}

void MqttFailover::RemoveBroker(const std::string& broker_url) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    brokers_.erase(
        std::remove_if(brokers_.begin(), brokers_.end(),
                      [&broker_url](const BrokerInfo& info) {
                          return info.url == broker_url;
                      }),
        brokers_.end()
    );
}

void MqttFailover::EnableFailover(bool enable) {
    failover_enabled_ = enable;
}

void MqttFailover::EnableHealthCheck(bool enable) {
    bool was_enabled = health_check_enabled_.exchange(enable);
    
    if (enable && !was_enabled) {
        // 헬스 체크 스레드 시작
        if (!health_check_running_) {
            health_check_running_ = true;
            if (health_check_thread_.joinable()) {
                health_check_thread_.join();
            }
            health_check_thread_ = std::thread(&MqttFailover::HealthCheckLoop, this);
        }
    } else if (!enable && was_enabled) {
        // 헬스 체크 스레드 중지
        health_check_running_ = false;
        health_check_cv_.notify_all();
    }
}

// =============================================================================
// 콜백 설정
// =============================================================================

void MqttFailover::SetFailoverCallback(FailoverCallback callback) {
    failover_callback_ = callback;
}

void MqttFailover::SetReconnectCallback(ReconnectCallback callback) {
    reconnect_callback_ = callback;
}

void MqttFailover::SetHealthCheckCallback(HealthCheckCallback callback) {
    health_check_callback_ = callback;
}

// =============================================================================
// 페일오버 및 재연결 제어
// =============================================================================

bool MqttFailover::StartAutoReconnect(const std::string& reason) {
    if (!strategy_.enabled || is_reconnecting_) {
        return false;
    }
    
    is_reconnecting_ = true;
    
    // 재연결 스레드 시작
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }
    
    reconnect_thread_ = std::thread(&MqttFailover::ReconnectLoop, this);
    
    // 이벤트 기록
    RecordEvent(FailoverEvent("reconnect_start", current_broker_url_, "", reason, true));
    
    return true;
}

void MqttFailover::StopAutoReconnect() {
    is_reconnecting_ = false;
    reconnect_cv_.notify_all();
    
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }
    
    RecordEvent(FailoverEvent("reconnect_stop", current_broker_url_, "", "Manual stop", true));
}

bool MqttFailover::TriggerFailover(const std::string& reason) {
    if (!failover_enabled_) {
        return false;
    }
    
    std::string old_broker = current_broker_url_;
    std::string new_broker = SelectNextBroker(true);
    
    if (new_broker.empty()) {
        RecordEvent(FailoverEvent("failover", old_broker, "", reason + " - No alternative broker", false));
        return false;
    }
    
    auto start_time = high_resolution_clock::now();
    bool success = AttemptConnection(new_broker);
    auto end_time = high_resolution_clock::now();
    
    double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
    
    RecordEvent(FailoverEvent("failover", old_broker, new_broker, reason, success, duration_ms));
    
    if (success) {
        current_broker_url_ = new_broker;
        
        if (failover_callback_) {
            failover_callback_(old_broker, new_broker, reason);
        }
    }
    
    return success;
}

bool MqttFailover::SwitchToBestBroker() {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    if (brokers_.empty()) {
        return false;
    }
    
    // 가장 좋은 브로커 찾기 (우선순위 + 성공률 기반)
    auto best_broker = std::max_element(brokers_.begin(), brokers_.end(),
        [](const BrokerInfo& a, const BrokerInfo& b) {
            // 우선순위가 같으면 성공률로 비교
            if (a.priority == b.priority) {
                return a.GetSuccessRate() < b.GetSuccessRate();
            }
            return a.priority > b.priority; // 낮은 우선순위가 더 좋음
        });
    
    if (best_broker->url == current_broker_url_) {
        return true; // 이미 최적 브로커에 연결됨
    }
    
    return SwitchToBroker(best_broker->url);
}

bool MqttFailover::SwitchToBroker(const std::string& broker_url) {
    if (broker_url == current_broker_url_) {
        return true; // 이미 해당 브로커에 연결됨
    }
    
    std::string old_broker = current_broker_url_;
    auto start_time = high_resolution_clock::now();
    
    bool success = AttemptConnection(broker_url);
    
    auto end_time = high_resolution_clock::now();
    double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
    
    RecordEvent(FailoverEvent("switch", old_broker, broker_url, "Manual switch", success, duration_ms));
    
    if (success) {
        current_broker_url_ = broker_url;
    }
    
    return success;
}

// =============================================================================
// 상태 조회
// =============================================================================

BrokerInfo MqttFailover::GetCurrentBroker() const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    auto it = std::find_if(brokers_.begin(), brokers_.end(),
                          [this](const BrokerInfo& info) {
                              return info.url == current_broker_url_;
                          });
    
    if (it != brokers_.end()) {
        return *it;
    }
    
    return BrokerInfo{}; // 빈 브로커 정보 반환
}

std::vector<BrokerInfo> MqttFailover::GetAllBrokers() const {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    return brokers_;
}

bool MqttFailover::IsReconnecting() const {
    return is_reconnecting_.load();
}

bool MqttFailover::IsFailoverEnabled() const {
    return failover_enabled_.load();
}

std::vector<FailoverEvent> MqttFailover::GetRecentEvents(size_t max_count) const {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    std::vector<FailoverEvent> recent_events;
    
    size_t count = std::min(max_count, failover_events_.size());
    for (size_t i = 0; i < count; ++i) {
        recent_events.push_back(failover_events_[failover_events_.size() - 1 - i]);
    }
    
    return recent_events;
}

std::string MqttFailover::GetStatisticsJSON() const {
#ifdef HAVE_JSON
    std::lock_guard<std::mutex> brokers_lock(brokers_mutex_);
    std::lock_guard<std::mutex> events_lock(events_mutex_);
    
    try {
        json stats;
        
        // 기본 상태
        stats["failover_enabled"] = failover_enabled_.load();
        stats["health_check_enabled"] = health_check_enabled_.load();
        stats["is_reconnecting"] = is_reconnecting_.load();
        stats["current_broker"] = current_broker_url_;
        
        // 재연결 전략
        json strategy;
        strategy["enabled"] = strategy_.enabled;
        strategy["max_attempts"] = strategy_.max_attempts;
        strategy["initial_delay_ms"] = strategy_.initial_delay_ms;
        strategy["max_delay_ms"] = strategy_.max_delay_ms;
        strategy["backoff_multiplier"] = strategy_.backoff_multiplier;
        strategy["exponential_backoff"] = strategy_.exponential_backoff;
        strategy["connection_timeout_ms"] = strategy_.connection_timeout_ms;
        strategy["health_check_interval_ms"] = strategy_.health_check_interval_ms;
        strategy["enable_jitter"] = strategy_.enable_jitter;
        stats["strategy"] = strategy;
        
        // 브로커 정보
        json brokers;
        for (const auto& broker : brokers_) {
            json broker_data;
            broker_data["url"] = broker.url;
            broker_data["name"] = broker.name;
            broker_data["priority"] = broker.priority;
            broker_data["is_available"] = broker.is_available;
            broker_data["connection_attempts"] = broker.connection_attempts;
            broker_data["successful_connections"] = broker.successful_connections;
            broker_data["failed_connections"] = broker.failed_connections;
            broker_data["success_rate"] = broker.GetSuccessRate();
            broker_data["avg_response_time_ms"] = broker.avg_response_time_ms;
            
            brokers[broker.url] = broker_data;
        }
        stats["brokers"] = brokers;
        
        // 최근 이벤트들
        json events;
        size_t event_count = std::min<size_t>(10, failover_events_.size());
        for (size_t i = 0; i < event_count; ++i) {
            const auto& event = failover_events_[failover_events_.size() - 1 - i];
            
            json event_data;
            auto time_t = system_clock::to_time_t(event.timestamp);
            std::ostringstream time_oss;
            time_oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            
            event_data["timestamp"] = time_oss.str();
            event_data["type"] = event.event_type;
            event_data["from_broker"] = event.from_broker;
            event_data["to_broker"] = event.to_broker;
            event_data["reason"] = event.reason;
            event_data["success"] = event.success;
            event_data["duration_ms"] = event.duration_ms;
            
            events.push_back(event_data);
        }
        stats["recent_events"] = events;
        
        return stats.dump(2);
        
    } catch (const std::exception& e) {
        return "{\"error\":\"Failed to generate failover statistics: " + std::string(e.what()) + "\"}";
    }
#else
    // JSON 라이브러리가 없는 경우 간단한 문자열 반환
    std::ostringstream oss;
    oss << "{"
        << "\"failover_enabled\":" << (failover_enabled_.load() ? "true" : "false") << ","
        << "\"is_reconnecting\":" << (is_reconnecting_.load() ? "true" : "false") << ","
        << "\"current_broker\":\"" << current_broker_url_ << "\","
        << "\"broker_count\":" << brokers_.size()
        << "}";
    return oss.str();
#endif
}

// =============================================================================
// 내부 메서드들
// =============================================================================

void MqttFailover::ReconnectLoop() {
    int attempt = 0;
    
    while (is_reconnecting_ && !should_stop_ && attempt < strategy_.max_attempts) {
        attempt++;
        
        // 다음 브로커 선택
        std::string target_broker = SelectNextBroker(false);
        if (target_broker.empty()) {
            RecordEvent(FailoverEvent("reconnect", current_broker_url_, "", 
                                    "No available broker - attempt " + std::to_string(attempt), false));
            break;
        }
        
        // 재연결 콜백 호출
        if (reconnect_callback_) {
            reconnect_callback_(false, attempt, target_broker);
        }
        
        // 연결 시도
        auto start_time = high_resolution_clock::now();
        bool success = AttemptConnection(target_broker);
        auto end_time = high_resolution_clock::now();
        
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        if (success) {
            current_broker_url_ = target_broker;
            is_reconnecting_ = false;
            
            RecordEvent(FailoverEvent("reconnect", "", target_broker, 
                                    "Reconnect successful - attempt " + std::to_string(attempt), 
                                    true, duration_ms));
            
            if (reconnect_callback_) {
                reconnect_callback_(true, attempt, target_broker);
            }
            
            return;
        }
        
        RecordEvent(FailoverEvent("reconnect", "", target_broker, 
                                "Reconnect failed - attempt " + std::to_string(attempt), 
                                false, duration_ms));
        
        // 백오프 지연
        if (attempt < strategy_.max_attempts) {
            int delay_ms = CalculateBackoffDelay(attempt);
            
            std::unique_lock<std::mutex> lock(reconnect_mutex_);
            reconnect_cv_.wait_for(lock, milliseconds(delay_ms), [this] {
                return !is_reconnecting_ || should_stop_;
            });
        }
    }
    
    // 재연결 실패
    is_reconnecting_ = false;
    RecordEvent(FailoverEvent("reconnect", current_broker_url_, "", 
                            "Reconnect exhausted after " + std::to_string(attempt) + " attempts", false));
}

void MqttFailover::HealthCheckLoop() {
    while (health_check_running_ && !should_stop_) {
        {
            std::unique_lock<std::mutex> lock(health_check_mutex_);
            health_check_cv_.wait_for(lock, milliseconds(strategy_.health_check_interval_ms), [this] {
                return !health_check_running_ || should_stop_;
            });
        }
        
        if (!health_check_running_ || should_stop_) {
            break;
        }
        
        // 모든 브로커 헬스 체크
        std::vector<BrokerInfo> brokers_copy;
        {
            std::lock_guard<std::mutex> lock(brokers_mutex_);
            brokers_copy = brokers_;
        }
        
        for (auto& broker : brokers_copy) {
            bool healthy = CheckBrokerHealth(broker.url);
            
            // 브로커 상태 업데이트
            {
                std::lock_guard<std::mutex> lock(brokers_mutex_);
                auto* broker_ptr = FindBroker(broker.url);
                if (broker_ptr) {
                    broker_ptr->is_available = healthy;
                }
            }
            
            // 헬스 체크 콜백 호출
            if (health_check_callback_) {
                health_check_callback_(broker.url, healthy);
            }
            
            // 현재 브로커가 unhealthy하면 페일오버 트리거
            if (!healthy && broker.url == current_broker_url_ && failover_enabled_) {
                TriggerFailover("Current broker unhealthy");
            }
        }
    }
}

std::string MqttFailover::SelectNextBroker(bool exclude_current) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    if (brokers_.empty()) {
        return "";
    }
    
    // 사용 가능한 브로커들 필터링
    std::vector<BrokerInfo*> available_brokers;
    for (auto& broker : brokers_) {
        if (broker.is_available && (!exclude_current || broker.url != current_broker_url_)) {
            available_brokers.push_back(&broker);
        }
    }
    
    if (available_brokers.empty()) {
        return "";
    }
    
    // 우선순위 기준으로 정렬 (이미 정렬되어 있지만 확실히)
    std::sort(available_brokers.begin(), available_brokers.end(),
              [](const BrokerInfo* a, const BrokerInfo* b) {
                  if (a->priority == b->priority) {
                      return a->GetSuccessRate() > b->GetSuccessRate();
                  }
                  return a->priority < b->priority;
              });
    
    return available_brokers[0]->url;
}

bool MqttFailover::AttemptConnection(const std::string& broker_url) {
    if (!parent_driver_) {
        return false;
    }
    
    auto start_time = high_resolution_clock::now();
    
    // 실제 연결 시도 (MqttDriver의 메서드 호출)
    // 여기서는 간소화된 구현 - 실제로는 parent_driver_의 연결 메서드 호출
    bool success = false; // parent_driver_->ConnectToBroker(broker_url);
    
    auto end_time = high_resolution_clock::now();
    double response_time_ms = duration_cast<milliseconds>(end_time - start_time).count();
    
    // 브로커 통계 업데이트
    UpdateBrokerStats(broker_url, success, response_time_ms);
    
    return success;
}

bool MqttFailover::CheckBrokerHealth(const std::string& broker_url) {
    (void)broker_url;
    // 간단한 헬스 체크 구현
    // 실제로는 ping, 연결 테스트 등을 수행
    auto start_time = high_resolution_clock::now();
    
    // 여기서는 간소화된 구현
    bool healthy = true; // 실제 헬스 체크 로직
    
    auto end_time = high_resolution_clock::now();
    double response_time_ms = duration_cast<milliseconds>(end_time - start_time).count();
    
    // 응답 시간이 너무 길면 unhealthy로 판단
    if (response_time_ms > strategy_.connection_timeout_ms) {
        healthy = false;
    }
    
    return healthy;
}

int MqttFailover::CalculateBackoffDelay(int attempt) {
    int delay_ms = strategy_.initial_delay_ms;
    
    if (strategy_.exponential_backoff) {
        for (int i = 1; i < attempt; ++i) {
            delay_ms = static_cast<int>(delay_ms * strategy_.backoff_multiplier);
        }
    } else {
        delay_ms = strategy_.initial_delay_ms * attempt;
    }
    
    // 최대 지연 시간 제한
    delay_ms = std::min(delay_ms, strategy_.max_delay_ms);
    
    // 지터 추가 (동시 재연결 방지)
    if (strategy_.enable_jitter) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(-delay_ms / 10, delay_ms / 10);
        delay_ms += dis(gen);
    }
    
    return std::max(delay_ms, 100); // 최소 100ms
}

void MqttFailover::UpdateBrokerStats(const std::string& broker_url, bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(brokers_mutex_);
    
    auto* broker = FindBroker(broker_url);
    if (!broker) {
        return;
    }
    
    broker->connection_attempts++;
    broker->last_attempt = system_clock::now();
    
    if (success) {
        broker->successful_connections++;
        broker->last_success = system_clock::now();
        
        // 평균 응답 시간 업데이트
        if (broker->avg_response_time_ms == 0.0) {
            broker->avg_response_time_ms = response_time_ms;
        } else {
            broker->avg_response_time_ms = (broker->avg_response_time_ms + response_time_ms) / 2.0;
        }
    } else {
        broker->failed_connections++;
    }
}

BrokerInfo* MqttFailover::FindBroker(const std::string& broker_url) {
    auto it = std::find_if(brokers_.begin(), brokers_.end(),
                          [&broker_url](const BrokerInfo& info) {
                              return info.url == broker_url;
                          });
    
    return (it != brokers_.end()) ? &(*it) : nullptr;
}

void MqttFailover::RecordEvent(const FailoverEvent& event) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    failover_events_.push_back(event);
    CleanOldEvents();
}

void MqttFailover::CleanOldEvents() {
    while (failover_events_.size() > max_events_history_) {
        failover_events_.pop_front();
    }
}

void MqttFailover::HandleConnectionSuccess() {
    // 재연결 프로세스 중지
    is_reconnecting_ = false;
    
    // 현재 브로커 성공 카운터 업데이트
    {
        std::lock_guard<std::mutex> lock(brokers_mutex_);
        auto it = std::find_if(brokers_.begin(), brokers_.end(),
                              [this](const BrokerInfo& broker) {
                                  return broker.url == current_broker_url_;
                              });
        
        if (it != brokers_.end()) {
            // ✅ 올바른 필드명 사용
            it->successful_connections++;              // success_count → successful_connections
            it->connection_attempts++;                 // 총 시도 횟수도 증가
            it->last_success = std::chrono::system_clock::now();
            it->is_available = true;
        }
    }
    
    // 성공 이벤트 기록
    RecordEvent(FailoverEvent("connection_success", "", current_broker_url_, 
                             "Connection established successfully", true));
    
    // 재연결 콜백 호출 (성공)
    if (reconnect_callback_) {
        reconnect_callback_(true, 0, current_broker_url_);
    }
}

void MqttFailover::HandleConnectionFailure(const std::string& reason) {
    // 현재 브로커 실패 카운터 업데이트
    {
        std::lock_guard<std::mutex> lock(brokers_mutex_);
        auto it = std::find_if(brokers_.begin(), brokers_.end(),
                              [this](const BrokerInfo& broker) {
                                  return broker.url == current_broker_url_;
                              });
        
        if (it != brokers_.end()) {
            // ✅ 올바른 필드명 사용
            it->failed_connections++;                  // failure_count → failed_connections
            it->connection_attempts++;                 // 총 시도 횟수도 증가
            it->last_attempt = std::chrono::system_clock::now();
            
            // 연속 실패가 임계값을 넘으면 브로커를 임시 비활성화
            if (it->failed_connections > 5) {         // failure_count → failed_connections
                it->is_available = false;
            }
        }
    }
    
    // 실패 이벤트 기록
    RecordEvent(FailoverEvent("connection_failure", current_broker_url_, "", 
                             "Connection failed: " + reason, false));
    
    // 자동 재연결이 활성화된 경우 시작
    if (strategy_.enabled && !is_reconnecting_) {
        StartAutoReconnect(reason);
    }
}

} // namespace Drivers
} // namespace PulseOne