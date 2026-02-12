/**
 * @file FailureProtector.h
 * @brief 실패 방지기 - CircuitBreaker 패턴 (ExportTypes.h 사용)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 4.0.0 (using 선언 추가)
 */

#ifndef FAILURE_PROTECTOR_H
#define FAILURE_PROTECTOR_H

#include "Export/ExportTypes.h"
#include "Export/FailureProtectorTypes.h"
#include "Export/GatewayExportTypes.h"
#include <chrono>
#include <mutex>
#include <string>

namespace PulseOne {
namespace CSP {

// =============================================================================
// Export 네임스페이스 타입 사용 (명시적 using 선언)
// =============================================================================

using PulseOne::Export::FailureProtectorConfig;
using PulseOne::Export::FailureProtectorStats;

// =============================================================================
// FailureProtector 클래스
// =============================================================================

/**
 * @brief 실패 방지기 - CircuitBreaker 패턴 구현
 */
class FailureProtector {
public:
  /**
   * @brief CircuitBreaker 상태
   */
  enum class State {
    CLOSED,   // 정상 상태 - 요청 허용
    OPEN,     // 실패 상태 - 요청 차단
    HALF_OPEN // 복구 테스트 상태 - 제한적 요청 허용
  };

private:
  // 기본 설정 및 상태
  std::string target_name_;
  FailureProtectorConfig config_;
  std::atomic<State> state_;

  // 통계 정보 (atomic)
  std::atomic<uint32_t> failure_count_;
  std::atomic<uint32_t> success_count_;
  std::atomic<std::chrono::steady_clock::time_point> last_failure_time_;
  std::atomic<std::chrono::steady_clock::time_point> last_state_change_;
  std::atomic<uint32_t> half_open_attempts_;
  std::atomic<uint32_t> total_attempts_;
  std::atomic<uint32_t> total_successes_;
  std::atomic<uint32_t> total_failures_;

  // 동시성 제어
  mutable std::mutex state_mutex_;

public:
  // =======================================================================
  // 생성자 및 소멸자
  // =======================================================================

  /**
   * @brief 생성자
   * @param target_name 타겟 이름
   * @param config 설정 구조체
   */
  FailureProtector(const std::string &target_name,
                   const FailureProtectorConfig &config);

  /**
   * @brief 소멸자
   */
  ~FailureProtector();

  // =======================================================================
  // 핵심 기능 메서드들
  // =======================================================================

  /**
   * @brief 요청 실행 가능 여부 확인
   */
  bool canExecute();

  /**
   * @brief 성공 기록
   */
  void recordSuccess();

  /**
   * @brief 실패 기록
   */
  void recordFailure();

  /**
   * @brief 수동 리셋
   */
  void reset();

  // =======================================================================
  // 상태 조회 메서드들
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
  // 통계 메서드들
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
  // 설정 관리
  // =======================================================================

  /**
   * @brief 설정 업데이트
   */
  void updateConfig(const FailureProtectorConfig &new_config);

  /**
   * @brief 설정 반환
   */
  const FailureProtectorConfig &getConfig() const;

private:
  // =======================================================================
  // 내부 헬퍼 메서드들
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