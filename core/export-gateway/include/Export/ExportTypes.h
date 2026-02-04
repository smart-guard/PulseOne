/**
 * @file ExportTypes.h
 * @brief Export Gateway 공통 타입 정의 (완전 통합)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 4.0.0 (CSPDynamicTargets.h → ExportTypes.h 이름 변경)
 * 저장 위치: core/export-gateway/include/Export/ExportTypes.h
 *
 * 🎯 Export Gateway 시스템의 모든 공통 타입을 이 파일에 정의:
 * - TargetSendResult (전송 결과) - ITargetHandler보다 먼저 정의
 * - FailureProtectorConfig/Stats (Circuit Breaker 패턴)
 * - DynamicTarget (타겟 정보 - atomic 복사 문제 해결)
 * - ITargetHandler (공통 인터페이스)
 * - TargetHandlerFactory (팩토리 패턴)
 * - BatchTargetResult (배치 처리)
 * - 유틸리티 함수들
 *
 * 🔄 변경 이력:
 * - v4.0.0 (2025-10-23): CSPDynamicTargets.h → ExportTypes.h 이름 변경
 * - v3.0.0 (2025-09-29): 모든 타입 통합 + 타입 정의 순서 수정
 */

#ifndef EXPORT_TYPES_H
#define EXPORT_TYPES_H

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

// JSON Alias (Global for this header as per original design, moved up for
// visibility/usage)
using json = nlohmann::json;

#include "CSP/AlarmMessage.h"

namespace PulseOne {
namespace CSP {

/**
 * @brief Value Message Structure for Data Scanning (C# ValueMessage
 * compatibility)
 */
struct ValueMessage {
  int bd = 0;     // Building ID
  std::string nm; // Point Name
  std::string vl; // Value (String format to support Double/String flexibility)
  std::string tm; // Timestamp (yyyy-MM-dd HH:mm:ss.fff)
  int st = 0;     // Status (Communication Status)
  std::string ty = "dbl"; // Type (dbl or str), default: dbl

  // JSON Serialization
  json to_json() const {
    return json{{"bd", bd}, {"nm", nm}, {"vl", vl},
                {"tm", tm}, {"st", st}, {"ty", ty}};
  }
};

} // namespace CSP
} // namespace PulseOne

// [REMOVE] json을 헤더에서 제거하여 컴파일 메모리 사용량 절감 (Original Comment
// preserved but alias moved up)

namespace PulseOne {
namespace Export {

// =============================================================================
// 전방 선언
// =============================================================================
class ITargetHandler;
class DynamicTargetManager;
class TargetHandlerFactory;
class FailureProtector;

// =============================================================================
// 타겟 전송 결과 (ITargetHandler보다 먼저 정의해야 함)
// =============================================================================

/**
 * @brief 타겟 전송 결과 - 모든 Handler에서 공통 사용
 */
struct TargetSendResult {
public:
  // 기본 결과 필드들
  bool success = false;
  std::string error_message = "";
  std::chrono::milliseconds response_time{0};
  size_t content_size = 0;
  int retry_count = 0;

  // 타겟 정보
  std::string target_name = "";
  std::string target_type = "";

  // HTTP 관련 필드들
  int status_code = 0;
  std::string response_body = "";

  // 파일/경로 관련 필드들
  std::string file_path = "";     // 파일 타겟용
  std::string s3_object_key = ""; // S3 타겟용
  std::string mqtt_topic = "";    // MQTT 타겟용

  // 타임스탬프
  std::chrono::system_clock::time_point timestamp =
      std::chrono::system_clock::now();

  // 편의 생성자들
  TargetSendResult() = default;

  TargetSendResult(const std::string &name, const std::string &type,
                   bool result)
      : success(result), target_name(name), target_type(type) {}

  // 상태 확인 메서드들
  bool isHttpSuccess() const { return status_code >= 200 && status_code < 300; }

  bool isClientError() const { return status_code >= 400 && status_code < 500; }

  bool isServerError() const { return status_code >= 500 && status_code < 600; }

  // JSON 변환
  json toJson() const {
    return json{{"success", success},
                {"error_message", error_message},
                {"response_time_ms", response_time.count()},
                {"content_size", content_size},
                {"retry_count", retry_count},
                {"target_name", target_name},
                {"target_type", target_type},
                {"status_code", status_code},
                {"response_body", response_body},
                {"file_path", file_path},
                {"s3_object_key", s3_object_key},
                {"mqtt_topic", mqtt_topic}};
  }
};

// =============================================================================
// 실패 방지기 관련 타입들 (Circuit Breaker) - ITargetHandler보다 먼저 정의
// =============================================================================

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

  // 편의 생성자
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

  // JSON 변환
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

// =============================================================================
// 타겟 핸들러 인터페이스 (TargetSendResult가 정의된 후에 배치)
// =============================================================================

/**
 * @brief 타겟 핸들러 공통 인터페이스
 */
class ITargetHandler {
public:
  virtual ~ITargetHandler() = default;

  // 필수 메서드들
  virtual TargetSendResult sendAlarm(const PulseOne::CSP::AlarmMessage &alarm,
                                     const json &config) = 0;
  virtual bool testConnection(const json &config) = 0;
  virtual std::string getHandlerType() const = 0;
  virtual bool validateConfig(const json &config,
                              std::vector<std::string> &errors) = 0;

  // 🆕 v3.2.0 추가: 파일 전송 메서드
  virtual TargetSendResult sendFile(const std::string &local_path,
                                    const json &config) {
    TargetSendResult result;
    result.error_message = "File export not supported by this handler";
    return result;
  }

  // 선택적 메서드들 (기본 구현 제공)
  virtual bool initialize(const json & /* config */) { return true; }
  virtual void cleanup() { /* 기본: 아무 작업 없음 */ }
  virtual json getStatus() const {
    return json{{"type", getHandlerType()}, {"status", "active"}};
  }

  // 배치 전송 메서드들 (기본 구현: 루프 전송)
  virtual std::vector<TargetSendResult>
  sendAlarmBatch(const std::vector<PulseOne::CSP::AlarmMessage> &alarms,
                 const json &config) {
    std::vector<TargetSendResult> results;
    for (const auto &alarm : alarms) {
      results.push_back(sendAlarm(alarm, config));
    }
    return results;
  }

  virtual std::vector<TargetSendResult>
  sendValueBatch(const std::vector<PulseOne::CSP::ValueMessage> & /* values */,
                 const json & /* config */) {
    // 기본적으로 값 전송은 배치만 지원하거나 미지원
    return {};
  }
};

// =============================================================================
// 동적 타겟 정보 (atomic 복사/이동 문제 완전 해결)
// =============================================================================

/**
 * @brief 동적 타겟 정보 - atomic 멤버 복사/이동 문제 해결
 */
struct DynamicTarget {
  // 기본 설정 필드들 (복사 가능)
  int id = 0;
  std::string name;
  std::string type;
  bool enabled = true;
  int priority = 100;
  int execution_order = 100;  // Runtime priority (Sourced from Gateway config)
  int execution_delay_ms = 0; // 🆕 v3.1.3 추가: 타겟 전송 전 지연 시간
  std::string description;
  json config;

  // 런타임 상태 (atomic 멤버들)
  mutable std::atomic<bool> healthy{true};
  mutable std::atomic<bool> handler_initialized{false};
  mutable std::atomic<size_t> success_count{0};
  mutable std::atomic<size_t> failure_count{0};
  mutable std::atomic<size_t> consecutive_failures{0};
  mutable std::atomic<double> avg_response_time_ms{0.0};
  mutable std::atomic<size_t> total_bytes_sent{0};
  mutable std::atomic<size_t> total_retries{0};

  // 시간 정보 (복사 가능)
  std::chrono::system_clock::time_point last_success_time;
  std::chrono::system_clock::time_point last_failure_time;
  std::chrono::system_clock::time_point created_time;

  // 기본 생성자
  DynamicTarget() {
    auto now = std::chrono::system_clock::now();
    last_success_time = now;
    last_failure_time = now;
    created_time = now;
  }

  // 이동 생성자
  DynamicTarget(DynamicTarget &&other) noexcept
      : id(other.id), name(std::move(other.name)), type(std::move(other.type)),
        enabled(other.enabled), priority(other.priority),
        execution_order(other.execution_order),
        execution_delay_ms(other.execution_delay_ms),
        description(std::move(other.description)),
        config(std::move(other.config)), healthy(other.healthy.load()),
        handler_initialized(other.handler_initialized.load()),
        success_count(other.success_count.load()),
        failure_count(other.failure_count.load()),
        consecutive_failures(other.consecutive_failures.load()),
        avg_response_time_ms(other.avg_response_time_ms.load()),
        total_bytes_sent(other.total_bytes_sent.load()),
        total_retries(other.total_retries.load()),
        last_success_time(other.last_success_time),
        last_failure_time(other.last_failure_time),
        created_time(other.created_time) {}

  // 복사 생성자
  DynamicTarget(const DynamicTarget &other)
      : id(other.id), name(other.name), type(other.type),
        enabled(other.enabled), priority(other.priority),
        execution_order(other.execution_order), description(other.description),
        config(other.config), healthy(other.healthy.load()),
        handler_initialized(other.handler_initialized.load()),
        success_count(other.success_count.load()),
        failure_count(other.failure_count.load()),
        consecutive_failures(other.consecutive_failures.load()),
        avg_response_time_ms(other.avg_response_time_ms.load()),
        total_bytes_sent(other.total_bytes_sent.load()),
        total_retries(other.total_retries.load()),
        last_success_time(other.last_success_time),
        last_failure_time(other.last_failure_time),
        created_time(other.created_time) {}

  // 복사 대입 연산자
  DynamicTarget &operator=(const DynamicTarget &other) {
    if (this != &other) {
      id = other.id;
      name = other.name;
      type = other.type;
      enabled = other.enabled;
      priority = other.priority;
      execution_order = other.execution_order;
      execution_delay_ms = other.execution_delay_ms;
      description = other.description;
      config = other.config;
      healthy.store(other.healthy.load());
      handler_initialized.store(other.handler_initialized.load());
      success_count.store(other.success_count.load());
      failure_count.store(other.failure_count.load());
      consecutive_failures.store(other.consecutive_failures.load());
      avg_response_time_ms.store(other.avg_response_time_ms.load());
      total_bytes_sent.store(other.total_bytes_sent.load());
      total_retries.store(other.total_retries.load());
      last_success_time = other.last_success_time;
      last_failure_time = other.last_failure_time;
      created_time = other.created_time;
    }
    return *this;
  }

  // 이동 대입 연산자
  DynamicTarget &operator=(DynamicTarget &&other) noexcept {
    if (this != &other) {
      id = other.id;
      name = std::move(other.name);
      type = std::move(other.type);
      enabled = other.enabled;
      priority = other.priority;
      execution_order = other.execution_order;
      execution_delay_ms = other.execution_delay_ms;
      description = std::move(other.description);
      config = std::move(other.config);
      healthy.store(other.healthy.load());
      handler_initialized.store(other.handler_initialized.load());
      success_count.store(other.success_count.load());
      failure_count.store(other.failure_count.load());
      consecutive_failures.store(other.consecutive_failures.load());
      avg_response_time_ms.store(other.avg_response_time_ms.load());
      total_bytes_sent.store(other.total_bytes_sent.load());
      total_retries.store(other.total_retries.load());
      last_success_time = other.last_success_time;
      last_failure_time = other.last_failure_time;
      created_time = other.created_time;
    }
    return *this;
  }

  // 성공률 계산
  double getSuccessRate() const {
    size_t total = success_count.load() + failure_count.load();
    return (total > 0)
               ? (static_cast<double>(success_count.load()) / total * 100.0)
               : 0.0;
  }

  // JSON 변환
  json toJson() const {
    return json{{"id", id},
                {"name", name},
                {"type", type},
                {"enabled", enabled},
                {"priority", priority},
                {"execution_delay_ms", execution_delay_ms},
                {"description", description},
                {"healthy", healthy.load()},
                {"handler_initialized", handler_initialized.load()},
                {"success_count", success_count.load()},
                {"failure_count", failure_count.load()},
                {"consecutive_failures", consecutive_failures.load()},
                {"success_rate", getSuccessRate()},
                {"avg_response_time_ms", avg_response_time_ms.load()},
                {"total_bytes_sent", total_bytes_sent.load()},
                {"total_retries", total_retries.load()},
                {"config", config}};
  }
};

// =============================================================================
// 타겟 핸들러 팩토리
// =============================================================================

using TargetHandlerCreator = std::function<std::unique_ptr<ITargetHandler>()>;

/**
 * @brief 타겟 핸들러 팩토리 (싱글톤)
 */
class TargetHandlerFactory {
private:
  mutable std::mutex factory_mutex_;
  std::map<std::string, TargetHandlerCreator> creators_;

  TargetHandlerFactory() = default;

public:
  static TargetHandlerFactory &getInstance() {
    static TargetHandlerFactory instance;
    return instance;
  }

  // 복사/이동 생성자 비활성화
  TargetHandlerFactory(const TargetHandlerFactory &) = delete;
  TargetHandlerFactory &operator=(const TargetHandlerFactory &) = delete;
  TargetHandlerFactory(TargetHandlerFactory &&) = delete;
  TargetHandlerFactory &operator=(TargetHandlerFactory &&) = delete;

  void registerHandler(const std::string &type_name,
                       TargetHandlerCreator creator) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    creators_[type_name] = creator;
    // Note: LogManager might not be initialized yet during static
    // initialization. Using std::cout for early registration debugging.
    std::cout << "[TargetHandlerFactory] Registered handler: " << type_name
              << std::endl;
  }

  std::unique_ptr<ITargetHandler> createHandler(const std::string &type_name) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    auto it = creators_.find(type_name);
    return (it != creators_.end()) ? it->second() : nullptr;
  }

  std::vector<std::string> getSupportedTypes() const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    std::vector<std::string> types;
    for (const auto &[type, _] : creators_) {
      types.push_back(type);
    }
    return types;
  }
};

// =============================================================================
// 배치 처리 결과
// =============================================================================

/**
 * @brief 배치 타겟 처리 결과
 */
struct BatchTargetResult {
  std::vector<TargetSendResult> results;
  size_t total_targets = 0;
  size_t successful_targets = 0;
  size_t failed_targets = 0;
  std::chrono::milliseconds total_time{0};

  // 편의 메서드들
  double getSuccessRate() const {
    return (total_targets > 0) ? (static_cast<double>(successful_targets) /
                                  total_targets * 100.0)
                               : 0.0;
  }

  json toJson() const {
    json result_array = json::array();
    for (const auto &result : results) {
      result_array.push_back(result.toJson());
    }

    return json{{"results", result_array},
                {"total_targets", total_targets},
                {"successful_targets", successful_targets},
                {"failed_targets", failed_targets},
                {"success_rate", getSuccessRate()},
                {"total_time_ms", total_time.count()}};
  }
};

// =============================================================================
// 유틸리티 매크로
// =============================================================================

/**
 * @brief 핸들러 등록 매크로
 */
#define REGISTER_TARGET_HANDLER(type_name, handler_class)                      \
  static bool register_##handler_class = []() {                                \
    TargetHandlerFactory::getInstance().registerHandler(                       \
        type_name, []() -> std::unique_ptr<ITargetHandler> {                   \
          return std::make_unique<handler_class>();                            \
        });                                                                    \
    return true;                                                               \
  }()

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * @brief 알람 메시지 유효성 검증
 */
inline bool isValidAlarmMessage(const PulseOne::CSP::AlarmMessage &alarm) {
  // 실제 AlarmMessage 필드: bd, nm, vl, tm, al, st, des
  return !alarm.nm.empty() && alarm.bd > 0;
}

/**
 * @brief 타겟 설정 유효성 검증
 */
inline bool isValidTargetConfig(const json &config,
                                const std::string &target_type) {
  if (config.empty())
    return false;

  if (target_type == "HTTP") {
    return config.contains("url") && config["url"].is_string();
  } else if (target_type == "S3") {
    return config.contains("bucket") && config.contains("access_key") &&
           config.contains("secret_key");
  } else if (target_type == "FILE") {
    return config.contains("base_path") && config["base_path"].is_string();
  }

  return true; // 기본적으로 허용
}

/**
 * @brief 알람 메시지 벡터를 JSON Array로 변환 (배치 전송용)
 */
json createAlarmJsonArray(
    const std::vector<PulseOne::CSP::AlarmMessage> &alarms);

/**
 * @brief Value 메시지 벡터를 JSON Array로 변환 (배치 전송용)
 */
json createValueJsonArray(
    const std::vector<PulseOne::CSP::ValueMessage> &values);

/**
 * @brief 현재 타임스탬프 문자열 생성 (용도별 포맷)
 */
std::string getCurrentTimestamp(const std::string &format_type = "iso8601");

// =============================================================================
// 전방 선언 (호환성 - CSP 네임스페이스)
// =============================================================================

// DynamicTargetManager는 아직 CSP 네임스페이스에 있음 (별도 파일)
class DynamicTargetManager;

} // namespace Export

// CSP 네임스페이스에 별칭 제공 (하위 호환성)
namespace CSP {
using TargetSendResult = Export::TargetSendResult;
using FailureProtectorConfig = Export::FailureProtectorConfig;
using FailureProtectorStats = Export::FailureProtectorStats;
using ITargetHandler = Export::ITargetHandler;
using DynamicTarget = Export::DynamicTarget;
using TargetHandlerFactory = Export::TargetHandlerFactory;
using TargetHandlerCreator = Export::TargetHandlerCreator;
using BatchTargetResult = Export::BatchTargetResult;

// DynamicTargetManager는 CSP 네임스페이스에 정의됨 (DynamicTargetManager.h)
// class DynamicTargetManager; // 이미 CSP에 있음
} // namespace CSP

} // namespace PulseOne

#endif // EXPORT_TYPES_H