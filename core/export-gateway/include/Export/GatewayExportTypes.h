/**
 * @file ExportTypes.h
 * @brief Export Gateway 공통 타입 정의 (분할 및 경량화)
 * @author PulseOne Development Team
 * @date 2025-10-23
 */

#ifndef EXPORT_TYPES_H
#define EXPORT_TYPES_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// 분할된 타입 헤더 포함
#include "Export/FailureProtectorTypes.h"
#include "Export/TargetSendResult.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Gateway/Model/ValueMessage.h"

// CSP 네임스페이스 별칭 제공 (내부용)
namespace PulseOne {
namespace CSP {
using AlarmMessage = PulseOne::Gateway::Model::AlarmMessage;
using ValueMessage = PulseOne::Gateway::Model::ValueMessage;
} // namespace CSP
} // namespace PulseOne

namespace PulseOne {

using json = nlohmann::json;

namespace Export {

// =============================================================================
// 전방 선언
// =============================================================================
class ITargetHandler;
class DynamicTargetManager;
class TargetHandlerFactory;
class FailureProtector;

// =============================================================================
// 타겟 핸들러 인터페이스
// =============================================================================

class ITargetHandler {
public:
  virtual ~ITargetHandler() = default;

  virtual TargetSendResult
  sendAlarm(const json &payload,
            const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) = 0;

  virtual std::vector<TargetSendResult> sendAlarmBatch(
      const std::vector<json> &payloads,
      const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
      const json &config) = 0;

  virtual TargetSendResult
  sendValue(const json &payload,
            const PulseOne::Gateway::Model::ValueMessage &value,
            const json &config) = 0;

  virtual std::vector<TargetSendResult> sendValueBatch(
      const std::vector<json> &payloads,
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) = 0;

  virtual TargetSendResult sendFile(const std::string &file_path,
                                    const json &config) = 0;

  virtual bool initialize(const json &config) = 0;
  virtual std::string getTargetName() const = 0;
  virtual std::string getTargetType() const = 0;
};

// =============================================================================
// 타겟 정보 (DynamicTarget)
// =============================================================================

struct DynamicTarget {
  int id = 0;
  std::string name = "";
  std::string type = "";
  json config = json::object();
  bool enabled = true;
  bool is_active = true;
  int priority = 100;
  int execution_order = 100;
  int execution_delay_ms = 0;
  int success_count = 0;
  int failure_count = 0;
  bool handler_initialized = false;
  std::string description = "";

  json toJson() const {
    return json{{"id", id},
                {"name", name},
                {"type", type},
                {"config", config},
                {"enabled", enabled},
                {"is_active", is_active},
                {"priority", priority},
                {"execution_order", execution_order},
                {"execution_delay_ms", execution_delay_ms},
                {"description", description}};
  }
};

// =============================================================================
// 배치 처리 결과
// =============================================================================

struct BatchTargetResult {
  bool success = true;
  int total_count = 0;
  int success_count = 0;
  int failure_count = 0;
  int total_targets = 0;      // Aliased for compatibility
  int successful_targets = 0; // Aliased for compatibility
  int failed_targets = 0;     // Aliased for compatibility
  std::vector<TargetSendResult> results;
  std::string last_error = "";
  std::chrono::milliseconds total_time{0};

  json toJson() const {
    json j_results = json::array();
    for (const auto &r : results) {
      j_results.push_back(r.toJson());
    }
    return json{{"success", success},
                {"total_count", total_count},
                {"success_count", success_count},
                {"failure_count", failure_count},
                {"last_error", last_error},
                {"total_time_ms", total_time.count()},
                {"results", j_results}};
  }
};

// =============================================================================
// 팩토리 타입 정의
// =============================================================================

using TargetHandlerCreator =
    std::function<std::unique_ptr<ITargetHandler>(const std::string &config)>;

} // namespace Export

// CSP 네임스페이스 별칭 제공 (하위 호환성)
namespace CSP {
using TargetSendResult = Export::TargetSendResult;
using FailureProtectorConfig = Export::FailureProtectorConfig;
using FailureProtectorStats = Export::FailureProtectorStats;
using DynamicTarget = Export::DynamicTarget;
using BatchTargetResult = Export::BatchTargetResult;
using ITargetHandler = Export::ITargetHandler;
} // namespace CSP

} // namespace PulseOne

// Global Validation Functions
namespace PulseOne {
namespace Export {
bool isValidAlarmMessage(const PulseOne::Gateway::Model::AlarmMessage &alarm);
bool isValidTargetConfig(const nlohmann::json &config,
                         const std::string &target_type);
} // namespace Export
} // namespace PulseOne

#endif // EXPORT_TYPES_H