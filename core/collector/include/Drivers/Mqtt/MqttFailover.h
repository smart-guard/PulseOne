// =============================================================================
// collector/include/Drivers/Mqtt/MqttFailover.h
// MQTT 페일오버 및 재연결 강화 기능
// =============================================================================

#ifndef PULSEONE_MQTT_FAILOVER_H
#define PULSEONE_MQTT_FAILOVER_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <functional>
#include <deque>

namespace PulseOne {
namespace Drivers {

// 전방 선언
class MqttDriver;

// =============================================================================
// 페일오버 관련 구조체들
// =============================================================================

/**
 * @brief 브로커 정보 및 상태
 */
struct BrokerInfo {
    std::string url;
    std::string name;
    int priority = 0;                           // 우선순위 (낮을수록 높은 우선순위)
    bool is_available = true;
    uint64_t connection_attempts = 0;
    uint64_t successful_connections = 0;
    uint64_t failed_connections = 0;
    double avg_response_time_ms = 0.0;
    std::chrono::system_clock::time_point last_attempt;
    std::chrono::system_clock::time_point last_success;
    
    // 성공률 계산
    double GetSuccessRate() const {
        if (connection_attempts == 0) return 0.0;
        return (double)successful_connections / connection_attempts * 100.0;
    }
    
    BrokerInfo() 
        : last_attempt(std::chrono::system_clock::now())
        , last_success(std::chrono::system_clock::now()) {}
        
    BrokerInfo(const std::string& broker_url, const std::string& broker_name = "", int prio = 0)
        : url(broker_url), name(broker_name.empty() ? broker_url : broker_name), priority(prio)
        , last_attempt(std::chrono::system_clock::now())
        , last_success(std::chrono::system_clock::now()) {}
};

/**
 * @brief 재연결 전략 설정
 */
struct ReconnectStrategy {
    bool enabled = true;
    int max_attempts = 10;                      // 최대 재시도 횟수 (-1 = 무제한)
    int initial_delay_ms = 1000;                // 초기 지연 시간
    int max_delay_ms = 30000;                   // 최대 지연 시간
    double backoff_multiplier = 2.0;            // 백오프 배수
    bool exponential_backoff = true;            // 지수 백오프 사용
    int connection_timeout_ms = 10000;          // 연결 타임아웃
    int health_check_interval_ms = 30000;       // 헬스 체크 간격
    bool enable_jitter = true;                  // 지터 추가 (동시 재연결 방지)
};

/**
 * @brief 페일오버 이벤트 정보
 */
struct FailoverEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string event_type;                     // "failover", "reconnect", "health_check"
    std::string from_broker;
    std::string to_broker;
    std::string reason;
    bool success;
    double duration_ms;
    
    FailoverEvent(const std::string& type, const std::string& from, const std::string& to,
                  const std::string& reason_str, bool result, double duration = 0.0)
        : timestamp(std::chrono::system_clock::now())
        , event_type(type), from_broker(from), to_broker(to), reason(reason_str)
        , success(result), duration_ms(duration) {}
};

/**
 * @brief MQTT 페일오버 및 재연결 강화 클래스
 * @details MqttDriver의 재연결 기능을 대폭 강화하는 고급 기능
 * 
 * 🎯 주요 기능:
 * - 지능형 백오프 재연결 (Exponential Backoff + Jitter)
 * - 다중 브로커 페일오버
 * - 브로커 헬스 체크 및 자동 복구
 * - 우선순위 기반 브로커 선택
 * - 네트워크 상태 감지
 * - 재연결 통계 및 이벤트 로깅
 */
class MqttFailover {
public:
    // 콜백 함수 타입들
    using FailoverCallback = std::function<void(const std::string& from_broker, const std::string& to_broker, const std::string& reason)>;
    using ReconnectCallback = std::function<void(bool success, int attempt, const std::string& broker)>;
    using HealthCheckCallback = std::function<void(const std::string& broker, bool healthy)>;
    
    // =======================================================================
    // 생성자/소멸자
    // =======================================================================
    
    /**
     * @brief MqttFailover 생성자
     * @param parent_driver 부모 MqttDriver 포인터
     */
    explicit MqttFailover(MqttDriver* parent_driver);
    
    /**
     * @brief 소멸자
     */
    ~MqttFailover();
    
    // 복사/이동 비활성화
    MqttFailover(const MqttFailover&) = delete;
    MqttFailover& operator=(const MqttFailover&) = delete;
    
    // =======================================================================
    // 설정 및 제어
    // =======================================================================
    
    /**
     * @brief 재연결 전략 설정
     * @param strategy 재연결 전략 설정
     */
    void SetReconnectStrategy(const ReconnectStrategy& strategy);
    
    /**
     * @brief 브로커 목록 설정
     * @param brokers 브로커 정보 리스트
     */
    void SetBrokers(const std::vector<BrokerInfo>& brokers);
    
    /**
     * @brief 브로커 추가
     * @param broker_url 브로커 URL
     * @param name 브로커 이름 (선택사항)
     * @param priority 우선순위 (선택사항)
     */
    void AddBroker(const std::string& broker_url, const std::string& name = "", int priority = 0);
    
    /**
     * @brief 브로커 제거
     * @param broker_url 브로커 URL
     */
    void RemoveBroker(const std::string& broker_url);
    
    /**
     * @brief 페일오버 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnableFailover(bool enable);
    
    /**
     * @brief 헬스 체크 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnableHealthCheck(bool enable);
    
    // =======================================================================
    // 콜백 설정
    // =======================================================================
    
    /**
     * @brief 페일오버 콜백 설정
     * @param callback 페일오버 발생 시 호출될 콜백
     */
    void SetFailoverCallback(FailoverCallback callback);
    
    /**
     * @brief 재연결 콜백 설정
     * @param callback 재연결 시도 시 호출될 콜백
     */
    void SetReconnectCallback(ReconnectCallback callback);
    
    /**
     * @brief 헬스 체크 콜백 설정
     * @param callback 헬스 체크 결과 콜백
     */
    void SetHealthCheckCallback(HealthCheckCallback callback);
    
    // =======================================================================
    // 페일오버 및 재연결 제어
    // =======================================================================
    
    /**
     * @brief 연결 끊김 시 자동 재연결 시작
     * @param reason 연결 끊김 원인
     * @return 재연결 시도 시작 여부
     */
    bool StartAutoReconnect(const std::string& reason = "Connection lost");
    
    /**
     * @brief 자동 재연결 중지
     */
    void StopAutoReconnect();
    
    /**
     * @brief 즉시 페일오버 수행
     * @param reason 페일오버 원인
     * @return 페일오버 성공 여부
     */
    bool TriggerFailover(const std::string& reason = "Manual failover");
    
    /**
     * @brief 최적 브로커로 전환
     * @return 전환 성공 여부
     */
    bool SwitchToBestBroker();
    
    /**
     * @brief 특정 브로커로 강제 전환
     * @param broker_url 대상 브로커 URL
     * @return 전환 성공 여부
     */
    bool SwitchToBroker(const std::string& broker_url);
    
    // =======================================================================
    // 상태 조회
    // =======================================================================
    
    /**
     * @brief 현재 활성 브로커 조회
     * @return 현재 브로커 정보
     */
    BrokerInfo GetCurrentBroker() const;
    
    /**
     * @brief 모든 브로커 상태 조회
     * @return 브로커 정보 리스트
     */
    std::vector<BrokerInfo> GetAllBrokers() const;
    
    /**
     * @brief 재연결 중인지 확인
     * @return 재연결 상태
     */
    bool IsReconnecting() const;
    
    /**
     * @brief 페일오버 활성화 상태 확인
     * @return 페일오버 활성화 여부
     */
    bool IsFailoverEnabled() const;
    
    /**
     * @brief 최근 페일오버 이벤트 조회
     * @param max_count 최대 조회 개수
     * @return 페일오버 이벤트 리스트
     */
    std::vector<FailoverEvent> GetRecentEvents(size_t max_count = 10) const;
    
    /**
     * @brief 재연결 통계 JSON 형태로 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetStatisticsJSON() const;
    /**
     * @brief 연결 성공 시 호출
     * @details MqttDriver에서 연결 성공 시 호출하여 페일오버 상태를 업데이트
     */
    void HandleConnectionSuccess();
    
    /**
     * @brief 연결 실패 시 호출
     * @param reason 실패 원인
     */
    void HandleConnectionFailure(const std::string& reason);
    
    /**
     * @brief 연결 복구 시 호출 (alias for HandleConnectionSuccess)
     * @deprecated HandleConnectionSuccess 사용 권장
     */
    void OnConnectionRestored() { HandleConnectionSuccess(); }    

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 부모 드라이버
    MqttDriver* parent_driver_;
    
    // 브로커 관리
    mutable std::mutex brokers_mutex_;
    std::vector<BrokerInfo> brokers_;
    std::string current_broker_url_;
    
    // 재연결 설정
    ReconnectStrategy strategy_;
    
    // 상태 관리
    std::atomic<bool> failover_enabled_{true};
    std::atomic<bool> health_check_enabled_{true};
    std::atomic<bool> is_reconnecting_{false};
    std::atomic<bool> should_stop_{false};
    
    // 재연결 스레드
    std::thread reconnect_thread_;
    std::condition_variable reconnect_cv_;
    std::mutex reconnect_mutex_;
    
    // 헬스 체크 스레드
    std::thread health_check_thread_;
    std::condition_variable health_check_cv_;
    std::mutex health_check_mutex_;
    std::atomic<bool> health_check_running_{false};
    
    // 통계 및 이벤트
    mutable std::mutex events_mutex_;
    std::deque<FailoverEvent> failover_events_;
    size_t max_events_history_{100};
    
    // 콜백들
    FailoverCallback failover_callback_;
    ReconnectCallback reconnect_callback_;
    HealthCheckCallback health_check_callback_;
    
    // =======================================================================
    // 내부 메서드들
    // =======================================================================
    
    /**
     * @brief 재연결 스레드 메인 루프
     */
    void ReconnectLoop();
    
    /**
     * @brief 헬스 체크 스레드 메인 루프
     */
    void HealthCheckLoop();
    
    /**
     * @brief 다음 브로커 선택 (우선순위 및 상태 기반)
     * @param exclude_current 현재 브로커 제외 여부
     * @return 선택된 브로커 URL (빈 문자열 = 없음)
     */
    std::string SelectNextBroker(bool exclude_current = true);
    
    /**
     * @brief 브로커 연결 시도
     * @param broker_url 브로커 URL
     * @return 연결 성공 여부
     */
    bool AttemptConnection(const std::string& broker_url);
    
    /**
     * @brief 브로커 헬스 체크
     * @param broker_url 브로커 URL
     * @return 헬스 상태
     */
    bool CheckBrokerHealth(const std::string& broker_url);
    
    /**
     * @brief 백오프 지연 계산
     * @param attempt 시도 횟수
     * @return 지연 시간 (밀리초)
     */
    int CalculateBackoffDelay(int attempt);
    
    /**
     * @brief 브로커 통계 업데이트
     * @param broker_url 브로커 URL
     * @param success 연결 성공 여부
     * @param response_time_ms 응답 시간
     */
    void UpdateBrokerStats(const std::string& broker_url, bool success, double response_time_ms);
    
    /**
     * @brief 브로커 찾기
     * @param broker_url 브로커 URL
     * @return 브로커 정보 포인터 (nullptr = 없음)
     */
    BrokerInfo* FindBroker(const std::string& broker_url);
    
    /**
     * @brief 페일오버 이벤트 기록
     * @param event 페일오버 이벤트
     */
    void RecordEvent(const FailoverEvent& event);
    
    /**
     * @brief 오래된 이벤트 정리
     */
    void CleanOldEvents();
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MQTT_FAILOVER_H