// =============================================================================
// collector/include/Drivers/Mqtt/MqttLoadBalancer.h
// MQTT 로드밸런싱 및 다중 브로커 관리 (컴파일 에러 해결)
// =============================================================================

#ifndef PULSEONE_MQTT_LOADBALANCER_H
#define PULSEONE_MQTT_LOADBALANCER_H

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <thread>         // 누락된 헤더 추가
#include <condition_variable>  // 누락된 헤더 추가

namespace PulseOne {
namespace Drivers {

// 전방 선언
class MqttDriver;

// =============================================================================
// 로드밸런싱 관련 구조체들
// =============================================================================

/**
 * @brief 로드밸런싱 알고리즘 타입
 */
enum class LoadBalanceAlgorithm {
    ROUND_ROBIN,        // 순환 방식
    LEAST_CONNECTIONS,  // 최소 연결 방식
    WEIGHTED_ROUND_ROBIN, // 가중 순환 방식
    RESPONSE_TIME,      // 응답시간 기반
    RANDOM,            // 랜덤 선택
    HASH_BASED         // 해시 기반 (토픽별)
};

/**
 * @brief 브로커 부하 정보
 */
struct BrokerLoad {
    std::string broker_url;
    std::string name;
    int weight = 1;                             // 가중치 (1-100)
    std::atomic<uint64_t> active_connections{0}; // 활성 연결 수
    std::atomic<uint64_t> total_messages{0};     // 총 메시지 수
    std::atomic<uint64_t> pending_messages{0};   // 대기 중인 메시지 수
    std::atomic<double> avg_response_time_ms{0.0}; // 평균 응답 시간
    std::atomic<double> cpu_usage{0.0};         // CPU 사용률 (0-100)
    std::atomic<double> memory_usage{0.0};      // 메모리 사용률 (0-100)
    std::atomic<bool> is_healthy{true};         // 헬스 상태
    std::chrono::system_clock::time_point last_update;
    
    // 복사 생성자와 대입 연산자 삭제 (atomic 멤버 때문에)
    BrokerLoad() = default;
    BrokerLoad(const BrokerLoad&) = delete;
    BrokerLoad& operator=(const BrokerLoad&) = delete;
    
    // 이동 생성자와 이동 대입 연산자 정의
    BrokerLoad(BrokerLoad&& other) noexcept
        : broker_url(std::move(other.broker_url))
        , name(std::move(other.name))
        , weight(other.weight)
        , active_connections(other.active_connections.load())
        , total_messages(other.total_messages.load())
        , pending_messages(other.pending_messages.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , cpu_usage(other.cpu_usage.load())
        , memory_usage(other.memory_usage.load())
        , is_healthy(other.is_healthy.load())
        , last_update(other.last_update)
    {}
    
    BrokerLoad& operator=(BrokerLoad&& other) noexcept {
        if (this != &other) {
            broker_url = std::move(other.broker_url);
            name = std::move(other.name);
            weight = other.weight;
            active_connections = other.active_connections.load();
            total_messages = other.total_messages.load();
            pending_messages = other.pending_messages.load();
            avg_response_time_ms = other.avg_response_time_ms.load();
            cpu_usage = other.cpu_usage.load();
            memory_usage = other.memory_usage.load();
            is_healthy = other.is_healthy.load();
            last_update = other.last_update;
        }
        return *this;
    }
    
    // 편의 생성자
    BrokerLoad(const std::string& url, const std::string& broker_name = "", int w = 1)
        : broker_url(url), name(broker_name.empty() ? url : broker_name), weight(w)
        , last_update(std::chrono::system_clock::now()) {}
    
    // 부하 점수 계산 (낮을수록 좋음)
    double CalculateLoadScore() const {
        double base_score = 0.0;
        
        // 연결 수 기반 점수 (30%)
        base_score += active_connections.load() * 0.3;
        
        // 응답 시간 기반 점수 (25%)
        base_score += avg_response_time_ms.load() * 0.25;
        
        // CPU 사용률 기반 점수 (25%)
        base_score += cpu_usage.load() * 0.25;
        
        // 메모리 사용률 기반 점수 (20%)
        base_score += memory_usage.load() * 0.20;
        
        // 가중치 반영 (가중치가 높을수록 점수 낮음)
        base_score = base_score / weight;
        
        // 건강하지 않으면 패널티
        if (!is_healthy.load()) {
            base_score += 1000.0;
        }
        
        return base_score;
    }
};

/**
 * @brief 메시지 라우팅 규칙
 */
struct RoutingRule {
    std::string rule_name;
    std::string topic_pattern;                  // 토픽 패턴 (와일드카드 지원)
    std::vector<std::string> preferred_brokers; // 선호 브로커 목록
    LoadBalanceAlgorithm algorithm = LoadBalanceAlgorithm::ROUND_ROBIN;
    int priority = 0;                           // 규칙 우선순위
    bool enabled = true;
    
    RoutingRule() = default;
    RoutingRule(const std::string& name, const std::string& pattern, 
                const std::vector<std::string>& brokers, LoadBalanceAlgorithm algo = LoadBalanceAlgorithm::ROUND_ROBIN)
        : rule_name(name), topic_pattern(pattern), preferred_brokers(brokers), algorithm(algo) {}
};

/**
 * @brief 로드밸런싱 통계
 */
struct LoadBalancingStats {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_routes{0};
    std::atomic<uint64_t> failed_routes{0};
    std::map<std::string, uint64_t> broker_usage_count;
    std::map<LoadBalanceAlgorithm, uint64_t> algorithm_usage_count;
    std::chrono::system_clock::time_point start_time;
    
    // 복사 생성자와 대입 연산자 삭제 (atomic 멤버 때문에)
    LoadBalancingStats() : start_time(std::chrono::system_clock::now()) {}
    LoadBalancingStats(const LoadBalancingStats&) = delete;
    LoadBalancingStats& operator=(const LoadBalancingStats&) = delete;
    
    // 이동 생성자와 이동 대입 연산자 정의
    LoadBalancingStats(LoadBalancingStats&& other) noexcept
        : total_requests(other.total_requests.load())
        , successful_routes(other.successful_routes.load())
        , failed_routes(other.failed_routes.load())
        , broker_usage_count(std::move(other.broker_usage_count))
        , algorithm_usage_count(std::move(other.algorithm_usage_count))
        , start_time(other.start_time)
    {}
    
    LoadBalancingStats& operator=(LoadBalancingStats&& other) noexcept {
        if (this != &other) {
            total_requests = other.total_requests.load();
            successful_routes = other.successful_routes.load();
            failed_routes = other.failed_routes.load();
            broker_usage_count = std::move(other.broker_usage_count);
            algorithm_usage_count = std::move(other.algorithm_usage_count);
            start_time = other.start_time;
        }
        return *this;
    }
    
    double GetSuccessRate() const {
        uint64_t total = total_requests.load();
        if (total == 0) return 0.0;
        return (double)successful_routes.load() / total * 100.0;
    }
};

/**
 * @brief MQTT 로드밸런싱 및 다중 브로커 관리 클래스
 * @details 여러 MQTT 브로커 간의 부하 분산 및 최적 라우팅 제공
 * 
 * 🎯 주요 기능:
 * - 다양한 로드밸런싱 알고리즘 지원
 * - 토픽별 라우팅 규칙 설정
 * - 브로커 부하 모니터링
 * - 동적 가중치 조정
 * - 실시간 성능 메트릭
 * - 자동 부하 재분산
 */
class MqttLoadBalancer {
public:
    // 콜백 함수 타입들
    using LoadUpdateCallback = std::function<void(const std::string& broker, const BrokerLoad& load)>;
    using RouteDecisionCallback = std::function<void(const std::string& topic, const std::string& selected_broker, LoadBalanceAlgorithm algorithm)>;
    
    // =======================================================================
    // 생성자/소멸자
    // =======================================================================
    
    /**
     * @brief MqttLoadBalancer 생성자
     * @param parent_driver 부모 MqttDriver 포인터
     */
    explicit MqttLoadBalancer(MqttDriver* parent_driver);
    
    /**
     * @brief 소멸자
     */
    ~MqttLoadBalancer();
    
    // 복사/이동 비활성화
    MqttLoadBalancer(const MqttLoadBalancer&) = delete;
    MqttLoadBalancer& operator=(const MqttLoadBalancer&) = delete;
    
    // =======================================================================
    // 브로커 관리
    // =======================================================================
    
    /**
     * @brief 브로커 추가
     * @param broker_url 브로커 URL
     * @param name 브로커 이름 (선택사항)
     * @param weight 가중치 (1-100, 기본값: 1)
     */
    void AddBroker(const std::string& broker_url, const std::string& name = "", int weight = 1);
    
    /**
     * @brief 브로커 제거
     * @param broker_url 제거할 브로커 URL
     */
    void RemoveBroker(const std::string& broker_url);
    
    /**
     * @brief 라우팅 규칙 추가
     * @param rule 라우팅 규칙
     */
    void AddRoutingRule(const RoutingRule& rule);
    
    /**
     * @brief 라우팅 규칙 제거
     * @param rule_name 제거할 규칙 이름
     */
    void RemoveRoutingRule(const std::string& rule_name);
    
    /**
     * @brief 부하 모니터링 활성화/비활성화
     * @param enable 활성화 여부
     * @param interval_ms 모니터링 간격 (기본값: 5초)
     */
    void EnableLoadMonitoring(bool enable, int interval_ms = 5000);
    
    // =======================================================================
    // 로드밸런싱 제어
    // =======================================================================
    
    /**
     * @brief 로드밸런싱 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnableLoadBalancing(bool enable);
    
    /**
     * @brief 기본 로드밸런싱 알고리즘 설정
     * @param algorithm 로드밸런싱 알고리즘
     */
    void SetDefaultAlgorithm(LoadBalanceAlgorithm algorithm);
    
    /**
     * @brief 토픽에 대한 최적 브로커 선택
     * @param topic 토픽명
     * @param message_size 메시지 크기 (선택사항)
     * @return 선택된 브로커 URL
     */
    std::string SelectBroker(const std::string& topic, size_t message_size = 0);
    
    /**
     * @brief 브로커 부하 정보 업데이트
     * @param broker_url 브로커 URL
     * @param connections 활성 연결 수
     * @param response_time_ms 평균 응답 시간
     * @param cpu_usage CPU 사용률
     * @param memory_usage 메모리 사용률
     */
    void UpdateBrokerLoad(const std::string& broker_url, uint64_t connections,
                         double response_time_ms, double cpu_usage, double memory_usage);
    
    // =======================================================================
    // 상태 조회
    // =======================================================================
    
    /**
     * @brief 사용 가능한 브로커 목록 조회
     * @param preferred_brokers 선호 브로커 목록 (선택사항)
     * @return 사용 가능한 브로커 URL 목록
     */
    std::vector<std::string> GetAvailableBrokers(const std::vector<std::string>& preferred_brokers = {}) const;
    
    /**
     * @brief 특정 브로커의 부하 정보 조회
     * @param broker_url 브로커 URL
     * @return 브로커 부하 정보
     */
    BrokerLoad GetBrokerLoad(const std::string& broker_url) const;
    
    /**
     * @brief 로드밸런싱 통계 조회
     * @return 로드밸런싱 통계 정보
     */
    LoadBalancingStats GetStatistics() const;
    
    /**
     * @brief 라우팅 규칙 목록 조회
     * @return 라우팅 규칙 리스트
     */
    std::vector<RoutingRule> GetRoutingRules() const;
    
    /**
     * @brief 로드밸런싱 상태 JSON으로 조회
     * @return JSON 형태의 상태 정보
     */
    std::string GetStatusJSON() const;
    
    /**
     * @brief 로드밸런싱 활성화 여부 확인
     * @return 활성화 상태
     */
    bool IsLoadBalancingEnabled() const;
    
    /**
     * @brief 기본 로드밸런싱 알고리즘 조회
     * @return 현재 기본 알고리즘
     */
    LoadBalanceAlgorithm GetDefaultAlgorithm() const;
    
    /**
     * @brief 총 브로커 수 조회
     * @return 브로커 수
     */
    size_t GetBrokerCount() const;
    
    /**
     * @brief 건강한 브로커 수 조회
     * @return 건강한 브로커 수
     */
    size_t GetHealthyBrokerCount() const;
    
    /**
     * @brief 토픽 패턴 매칭 확인
     * @param pattern 패턴 문자열
     * @param topic 토픽 문자열
     * @return 매칭 여부
     */
    bool MatchTopicPattern(const std::string& pattern, const std::string& topic) const;
    
    // =======================================================================
    // 콜백 설정
    // =======================================================================
    
    /**
     * @brief 부하 업데이트 콜백 설정
     * @param callback 콜백 함수
     */
    void SetLoadUpdateCallback(LoadUpdateCallback callback);
    
    /**
     * @brief 라우팅 결정 콜백 설정
     * @param callback 콜백 함수
     */
    void SetRouteDecisionCallback(RouteDecisionCallback callback);

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 부모 드라이버
    MqttDriver* parent_driver_;
    
    // 브로커 관리
    mutable std::mutex brokers_mutex_;
    std::map<std::string, BrokerLoad> brokers_;
    
    // 로드밸런싱 설정
    LoadBalanceAlgorithm default_algorithm_{LoadBalanceAlgorithm::ROUND_ROBIN};
    std::atomic<bool> load_balancing_enabled_{true};
    std::atomic<bool> load_monitoring_enabled_{false};
    
    // 라우팅 규칙
    mutable std::mutex rules_mutex_;
    std::vector<RoutingRule> routing_rules_;
    
    // 알고리즘별 상태
    std::atomic<size_t> round_robin_index_{0};
    mutable std::mutex hash_cache_mutex_;
    std::map<std::string, std::string> topic_broker_cache_; // 토픽별 브로커 캐시
    
    // 통계
    mutable std::mutex stats_mutex_;
    LoadBalancingStats statistics_;
    
    // 모니터링 스레드
    std::thread load_monitoring_thread_;
    std::atomic<bool> monitoring_running_{false};
    std::condition_variable monitoring_cv_;
    std::mutex monitoring_mutex_;
    int monitoring_interval_ms_{5000};
    
    // 콜백들
    LoadUpdateCallback load_update_callback_;
    RouteDecisionCallback route_decision_callback_;
    
    // =======================================================================
    // 내부 메서드들
    // =======================================================================
    
    /**
     * @brief 부하 모니터링 스레드 메인 루프
     */
    void LoadMonitoringLoop();
    
    /**
     * @brief 브로커 헬스 상태 업데이트
     * @param broker_url 브로커 URL
     * @return 헬스 상태
     */
    bool UpdateBrokerHealth(const std::string& broker_url);
    
    /**
     * @brief 라운드 로빈 알고리즘으로 브로커 선택
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByRoundRobin(const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 최소 연결 알고리즘으로 브로커 선택
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByLeastConnections(const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 가중치 기반 라운드 로빈 알고리즘으로 브로커 선택
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByWeightedRoundRobin(const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 응답시간 기반 알고리즘으로 브로커 선택
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByResponseTime(const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 랜덤 알고리즘으로 브로커 선택
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByRandom(const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 해시 기반 알고리즘으로 브로커 선택
     * @param topic 토픽명
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 선택된 브로커 URL
     */
    std::string SelectByHash(const std::string& topic, const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 라우팅 규칙 적용
     * @param topic 토픽명
     * @param available_brokers 사용 가능한 브로커 목록
     * @return 규칙에 의해 선택된 브로커 URL (없으면 빈 문자열)
     */
    std::string ApplyRoutingRules(const std::string& topic, const std::vector<std::string>& available_brokers);
    
    /**
     * @brief 토픽에 대한 규칙의 알고리즘 조회
     * @param topic 토픽명
     * @return 적용될 알고리즘
     */
    LoadBalanceAlgorithm GetRuleAlgorithm(const std::string& topic) const;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MQTT_LOADBALANCER_H