/**
 * @file FailureProtectorTypes.h
 * @brief 실패 방지기(Circuit Breaker) 공통 타입 정의
 */

#ifndef EXPORT_FAILURE_PROTECTOR_TYPES_H
#define EXPORT_FAILURE_PROTECTOR_TYPES_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace PulseOne {
namespace Export {

using json = nlohmann::json;

/**
 * @brief 실패 방지기 설정
 */
struct FailureProtectorConfig {
  uint32_t failure_threshold = 5;       // 실패 임계치
  uint32_t recovery_timeout_ms = 60000; // 복구 대기 시간 (밀리초)
  uint32_t half_open_max_attempts = 3;  // HALF_OPEN에서 최대 시도 횟수
  uint32_t half_open_success_threshold =
      2; // HALF_OPEN에서 CLOSED로 전환하기 위한 성공 횟수
  double backoff_multiplier = 2.0;            // 백오프 배수
  uint32_t max_recovery_timeout_ms = 1800000; // 최대 복구 대기 시간 (30분)

  FailureProtectorConfig() = default;

  FailureProtectorConfig(uint32_t threshold, uint32_t timeout_ms,
                         uint32_t max_attempts)
      : failure_threshold(threshold), recovery_timeout_ms(timeout_ms),
        half_open_max_attempts(max_attempts) {}
};

/**
 * @brief 실패 방지기 통계
 */
struct FailureProtectorStats {
  std::string target_name;
  std::string current_state; // "CLOSED", "OPEN", "HALF_OPEN"
  uint32_t failure_count = 0;
  uint32_t success_count = 0;
  uint32_t total_attempts = 0;
  uint32_t total_successes = 0;
  uint32_t total_failures = 0;
  uint32_t half_open_attempts = 0;
  double success_rate = 0.0;
  int64_t state_duration_ms = 0;

  json toJson() const {
    return json{{"target_name", target_name},
                {"current_state", current_state},
                {"failure_count", failure_count},
                {"success_count", success_count},
                {"total_attempts", total_attempts},
                {"total_successes", total_successes},
                {"total_failures", total_failures},
                {"half_open_attempts", half_open_attempts},
                {"success_rate", success_rate},
                {"state_duration_ms", state_duration_ms}};
  }
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_FAILURE_PROTECTOR_TYPES_H
