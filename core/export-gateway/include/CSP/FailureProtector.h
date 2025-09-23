/**
 * @file FailureProtector.h
 * @brief 실패 방지기 - 연속 실패로부터 시스템 보호 (CircuitBreaker 패턴)
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/FailureProtector.h
 */

#ifndef FAILURE_PROTECTOR_H
#define FAILURE_PROTECTOR_H

#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

/**
 * @brief 실패 방지기 - 연속 실패 시 타겟 보호
 * 
 * 동작 원리:
 * 1. CLOSED (정상): 모든 요청 허용
 * 2. OPEN (차단): 연속 실패 시 모든 요청 차단 
 * 3. HALF_OPEN (반개방): 제한적 요청으로 복구 시도
 * 
 * 사용 목적:
 * - 외부 시스템 장애 시 무한 재시도 방지
 * - 시스템 리소스 보호 (CPU, 메모리, 네트워크)
 * - 연쇄 장애 방지 (Cascade Failure Prevention)
 * - 자동 복구 지원 (Self-Healing)
 * - 성능 저하 방지
 */
class FailureProtector {
public:
    /**
     * @brief 보호기 상태
     */
    enum class State { 
        CLOSED,     // 정상 - 모든 요청 허용
        OPEN,       // 차단 - 모든 요청 차단
        HALF_OPEN   // 반개방 - 제한적 요청 허용
    };
    
    /**
     * @brief 통계 정보
     */
    struct Statistics {
        State current_state;
        size_t total_failures;
        size_t total_successes;
        size_t consecutive_failures;
        std::chrono::system_clock::time_point last_failure_time;
        std::chrono::system_clock::time_point last_success_time;
        std::chrono::system_clock::time_point state_change_time;
        std::chrono::milliseconds time_until_recovery;
        double failure_rate;
        std::chrono::milliseconds avg_failure_interval;
        
        /**
         * @brief JSON으로 변환
         */
        json toJson() const;
    };
    
private:
    std::atomic<State> state_{State::CLOSED};
    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> consecutive_failures_{0};
    std::atomic<size_t> half_open_attempts_{0};
    
    // 설정값들
    size_t failure_threshold_;              // 실패 임계치 (이 수를 넘으면 OPEN)
    std::chrono::milliseconds recovery_timeout_; // 복구 시도 간격
    size_t half_open_requests_;             // HALF_OPEN 상태에서 허용할 요청 수
    size_t max_consecutive_failures_;       // 최대 연속 실패 수
    
    // 시간 추적
    std::chrono::system_clock::time_point last_failure_time_;
    std::chrono::system_clock::time_point last_success_time_;
    std::chrono::system_clock::time_point state_change_time_;
    std::chrono::system_clock::time_point creation_time_;
    
    // 동시성 제어
    mutable std::mutex state_mutex_;
    mutable std::mutex stats_mutex_;
    
    // 추가 메트릭
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> rejected_requests_{0};
    std::vector<std::chrono::system_clock::time_point> recent_failures_;  // 최근 실패 시간들 (실패율 계산용)
    static constexpr size_t MAX_RECENT_FAILURES = 100;

public:
    /**
     * @brief 생성자
     * @param failure_threshold 실패 임계치 (예: 5회 연속 실패 시 차단)
     * @param recovery_timeout 복구 시도 간격 (예: 60초 후 복구 시도)
     * @param half_open_requests HALF_OPEN 상태에서 허용할 요청 수 (예: 3개)
     * @param max_consecutive_failures 최대 연속 실패 수 (기본: failure_threshold * 2)
     */
    FailureProtector(size_t failure_threshold = 5, 
                     std::chrono::milliseconds recovery_timeout = std::chrono::minutes(1),
                     size_t half_open_requests = 3,
                     size_t max_consecutive_failures = 0);
    
    /**
     * @brief 소멸자
     */
    ~FailureProtector() = default;
    
    // 복사/이동 생성자
    FailureProtector(const FailureProtector& other);
    FailureProtector& operator=(const FailureProtector& other);
    FailureProtector(FailureProtector&& other) noexcept;
    FailureProtector& operator=(FailureProtector&& other) noexcept;
    
    /**
     * @brief 요청 실행 가능 여부 확인
     * @return 실행 가능하면 true
     * 
     * 사용 예:
     * if (protector.canExecute()) {
     *     // 실제 외부 호출 수행
     *     auto result = callExternalAPI();
     *     if (result.success) {
     *         protector.recordSuccess();
     *     } else {
     *         protector.recordFailure();
     *     }
     * } else {
     *     // 차단됨 - 즉시 실패 반환
     *     return FailedResult("Service temporarily unavailable");
     * }
     */
    bool canExecute();
    
    /**
     * @brief 성공 기록
     * @param response_time 응답 시간 (선택사항)
     */
    void recordSuccess(std::chrono::milliseconds response_time = std::chrono::milliseconds(0));
    
    /**
     * @brief 실패 기록
     * @param error_message 실패 메시지 (선택사항)
     */
    void recordFailure(const std::string& error_message = "");
    
    /**
     * @brief 현재 상태 확인
     */
    bool isOpen() const { return state_.load() == State::OPEN; }
    bool isHalfOpen() const { return state_.load() == State::HALF_OPEN; }
    bool isClosed() const { return state_.load() == State::CLOSED; }
    
    /**
     * @brief 현재 상태 반환
     */
    State getCurrentState() const { return state_.load(); }
    
    /**
     * @brief 상태 문자열 반환
     */
    std::string getStateString() const;
    
    /**
     * @brief 수동 리셋 (관리자 기능)
     */
    void reset();
    
    /**
     * @brief 강제로 OPEN 상태로 변경 (테스트/관리용)
     */
    void forceOpen();
    
    /**
     * @brief 통계 정보 반환
     */
    Statistics getStatistics() const;
    
    /**
     * @brief 설정 정보 반환
     */
    json getConfiguration() const;
    
    /**
     * @brief 실패율 계산 (최근 시간 기준)
     * @param time_window 시간 윈도우 (기본: 5분)
     * @return 실패율 (0.0 ~ 1.0)
     */
    double getFailureRate(std::chrono::minutes time_window = std::chrono::minutes(5)) const;
    
    /**
     * @brief 복구까지 남은 시간
     * @return 남은 시간 (OPEN 상태가 아니면 0)
     */
    std::chrono::milliseconds getTimeUntilRecovery() const;
    
    /**
     * @brief 건강 상태 확인
     * @return 건강하면 true (CLOSED 상태이고 최근 실패율이 낮음)
     */
    bool isHealthy() const;
    
    /**
     * @brief 설정 업데이트 (런타임 중)
     * @param failure_threshold 새로운 실패 임계치
     * @param recovery_timeout 새로운 복구 간격
     * @param half_open_requests 새로운 반개방 요청 수
     */
    void updateConfiguration(size_t failure_threshold,
                           std::chrono::milliseconds recovery_timeout,
                           size_t half_open_requests);

private:
    /**
     * @brief 복구 시도 시간 확인
     */
    bool isRecoveryTimeReached() const;
    
    /**
     * @brief CLOSED로 상태 변경
     */
    void transitionToClosed();
    
    /**
     * @brief OPEN으로 상태 변경
     */
    void transitionToOpen();
    
    /**
     * @brief HALF_OPEN으로 상태 변경
     */
    void transitionToHalfOpen();
    
    /**
     * @brief 상태 변경 시 공통 처리
     * @param new_state 새로운 상태
     */
    void onStateChange(State new_state);
    
    /**
     * @brief 오래된 실패 기록 정리
     */
    void cleanupOldFailures();
    
    /**
     * @brief 요청 카운터 증가
     */
    void incrementRequestCount();
    
    /**
     * @brief 거부된 요청 카운터 증가
     */
    void incrementRejectedCount();
    
    /**
     * @brief 현재 시간 반환
     */
    std::chrono::system_clock::time_point now() const {
        return std::chrono::system_clock::now();
    }
};

/**
 * @brief 실패 방지기 팩토리
 */
class FailureProtectorFactory {
private:
    static std::mutex factory_mutex_;
    static std::unordered_map<std::string, std::shared_ptr<FailureProtector>> protectors_;
    
public:
    /**
     * @brief 이름으로 실패 방지기 생성/조회
     * @param name 보호기 이름
     * @param failure_threshold 실패 임계치
     * @param recovery_timeout 복구 시간
     * @param half_open_requests 반개방 요청 수
     * @return 실패 방지기 인스턴스
     */
    static std::shared_ptr<FailureProtector> getOrCreate(
        const std::string& name,
        size_t failure_threshold = 5,
        std::chrono::milliseconds recovery_timeout = std::chrono::minutes(1),
        size_t half_open_requests = 3);
    
    /**
     * @brief 실패 방지기 제거
     * @param name 보호기 이름
     */
    static void remove(const std::string& name);
    
    /**
     * @brief 모든 실패 방지기 리셋
     */
    static void resetAll();
    
    /**
     * @brief 모든 실패 방지기 통계 반환
     */
    static json getAllStatistics();
    
    /**
     * @brief 등록된 보호기 이름 목록 반환
     */
    static std::vector<std::string> getProtectorNames();
};

// =============================================================================
// 편의 매크로
// =============================================================================

/**
 * @brief 실패 방지기와 함께 함수 실행 매크로
 */
#define EXECUTE_WITH_PROTECTION(protector, operation, success_result, failure_result) \
    do { \
        if ((protector).canExecute()) { \
            auto result = (operation); \
            if (result) { \
                (protector).recordSuccess(); \
                return success_result; \
            } else { \
                (protector).recordFailure(); \
                return failure_result; \
            } \
        } else { \
            return failure_result; \
        } \
    } while(0)

} // namespace CSP
} // namespace PulseOne

#endif // FAILURE_PROTECTOR_H