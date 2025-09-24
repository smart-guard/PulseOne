/**
 * @file FailureProtector.h
 * @brief 실패 방지기 - CircuitBreaker 패턴 (컴파일 에러 완전 수정)
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 1.1.0 (구현부와 완전 일치하도록 수정)
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

// =============================================================================
// 설정 구조체 정의 (구현부에서 사용하는 것과 일치)
// =============================================================================

/**
 * @brief 실패 방지기 설정 구조체
 */
struct FailureProtectorConfig {
    uint32_t failure_threshold = 5;                // 실패 임계치
    uint32_t recovery_timeout_ms = 60000;          // 복구 대기 시간 (밀리초)
    uint32_t half_open_max_attempts = 3;           // HALF_OPEN에서 최대 시도 횟수
    uint32_t half_open_success_threshold = 2;      // HALF_OPEN에서 CLOSED로 전환하기 위한 성공 횟수
    double backoff_multiplier = 2.0;               // 백오프 배수
    uint32_t max_recovery_timeout_ms = 1800000;    // 최대 복구 대기 시간 (30분)
    
    // 기본 생성자
    FailureProtectorConfig() = default;
    
    // 편의 생성자
    FailureProtectorConfig(uint32_t threshold, uint32_t timeout_ms, uint32_t max_attempts)
        : failure_threshold(threshold)
        , recovery_timeout_ms(timeout_ms)
        , half_open_max_attempts(max_attempts) {}
};

/**
 * @brief 실패 방지기 통계 구조체
 */
struct FailureProtectorStats {
    std::string target_name;
    std::string current_state;
    uint32_t failure_count = 0;
    uint32_t success_count = 0;
    uint32_t total_attempts = 0;
    uint32_t total_successes = 0;
    uint32_t total_failures = 0;
    uint32_t half_open_attempts = 0;
    double success_rate = 0.0;
    int64_t state_duration_ms = 0;
    
    /**
     * @brief JSON으로 변환
     */
    json toJson() const {
        return json{
            {"target_name", target_name},
            {"current_state", current_state},
            {"failure_count", failure_count},
            {"success_count", success_count},
            {"total_attempts", total_attempts},
            {"total_successes", total_successes},
            {"total_failures", total_failures},
            {"half_open_attempts", half_open_attempts},
            {"success_rate", success_rate},
            {"state_duration_ms", state_duration_ms}
        };
    }
};

// =============================================================================
// FailureProtector 클래스 정의
// =============================================================================

/**
 * @brief 실패 방지기 - CircuitBreaker 패턴 구현
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

private:
    // ✅ 구현부와 일치하는 멤버 변수들
    std::string target_name_;
    FailureProtectorConfig config_;
    
    State state_;
    std::atomic<uint32_t> failure_count_{0};
    std::atomic<uint32_t> success_count_{0};
    std::chrono::steady_clock::time_point last_failure_time_;
    std::chrono::steady_clock::time_point last_state_change_;
    uint32_t half_open_attempts_ = 0;
    
    // 전체 통계
    std::atomic<uint32_t> total_attempts_{0};
    std::atomic<uint32_t> total_successes_{0};
    std::atomic<uint32_t> total_failures_{0};
    
    // 동시성 제어
    mutable std::mutex state_mutex_;

public:
    // =======================================================================
    // 생성자 및 소멸자 (구현부와 일치)
    // =======================================================================
    
    /**
     * @brief 생성자 - 구현부 시그니처와 일치
     * @param target_name 타겟 이름
     * @param config 설정 구조체
     */
    FailureProtector(const std::string& target_name, const FailureProtectorConfig& config);
    
    /**
     * @brief 소멸자 - 구현부에서 정의됨
     */
    ~FailureProtector();
    
    // =======================================================================
    // 핵심 기능 메서드들 (구현부와 시그니처 일치)
    // =======================================================================
    
    /**
     * @brief 요청 실행 가능 여부 확인
     */
    bool canExecute();
    
    /**
     * @brief 성공 기록 (구현부 시그니처와 일치)
     */
    void recordSuccess();
    
    /**
     * @brief 실패 기록 (구현부 시그니처와 일치)
     */
    void recordFailure();
    
    /**
     * @brief 수동 리셋
     */
    void reset();
    
    // =======================================================================
    // 상태 조회 메서드들 (구현부와 일치)
    // =======================================================================
    
    /**
     * @brief 현재 상태 반환
     */
    State getState() const;
    
    /**
     * @brief 상태 문자열 반환
     */
    std::string getStateString() const;
    
    /**
     * @brief 건강 상태 확인
     */
    bool isHealthy() const;
    
    // =======================================================================
    // 통계 메서드들 (구현부와 일치)
    // =======================================================================
    
    /**
     * @brief 통계 정보 반환
     */
    FailureProtectorStats getStats() const;
    
    /**
     * @brief 실패 횟수 반환
     */
    uint32_t getFailureCount() const;
    
    /**
     * @brief 성공 횟수 반환
     */
    uint32_t getSuccessCount() const;
    
    /**
     * @brief 성공률 반환
     */
    double getSuccessRate() const;
    
    // =======================================================================
    // 설정 관리 (구현부와 일치)
    // =======================================================================
    
    /**
     * @brief 설정 업데이트
     */
    void updateConfig(const FailureProtectorConfig& new_config);
    
    /**
     * @brief 설정 반환
     */
    const FailureProtectorConfig& getConfig() const;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (구현부와 일치)
    // =======================================================================
    
    /**
     * @brief 복구 시간 도달 여부 확인
     */
    bool isRecoveryTimeReached() const;
    
    /**
     * @brief CLOSED 상태로 전환
     */
    void transitionToClosed();
    
    /**
     * @brief OPEN 상태로 전환
     */
    void transitionToOpen();
    
    /**
     * @brief HALF_OPEN 상태로 전환
     */
    void transitionToHalfOpen();
    
    /**
     * @brief 상태를 문자열로 변환
     */
    std::string stateToString(State state) const;
    
    /**
     * @brief 다음 복구 대기 시간 계산
     */
    uint64_t calculateNextRecoveryTimeout() const;
};

} // namespace CSP
} // namespace PulseOne

#endif // FAILURE_PROTECTOR_H