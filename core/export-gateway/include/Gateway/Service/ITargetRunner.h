/**
 * @file ITargetRunner.h
 * @brief Target Runner Interface - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_I_TARGET_RUNNER_H
#define GATEWAY_SERVICE_I_TARGET_RUNNER_H

#include "Export/ExportTypes.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Gateway/Model/ValueMessage.h"
#include <string>
#include <vector>

namespace PulseOne {
namespace Gateway {
namespace Service {

using PulseOne::Export::BatchTargetResult;
using PulseOne::Export::TargetSendResult;

/**
 * @brief Statistics for the Gateway Service
 */
struct GatewayStats {
  size_t total_exports = 0;
  size_t successful_exports = 0;
  size_t failed_exports = 0;
  double avg_processing_time_ms = 0.0;
  size_t alarm_events = 0;
  size_t alarm_exports = 0;
  size_t schedule_executions = 0;
  size_t schedule_exports = 0;
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point last_export_time;
};

/**
 * @brief Handles the execution of data exports to targets
 */
class ITargetRunner {
public:
  virtual ~ITargetRunner() = default;

  virtual std::vector<TargetSendResult>
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm) = 0;
  virtual TargetSendResult
  sendAlarmToTarget(const std::string &target_name,
                    const PulseOne::Gateway::Model::AlarmMessage &alarm) = 0;

  virtual std::vector<TargetSendResult>
  sendFile(const std::string &local_path) = 0;

  virtual BatchTargetResult sendAlarmBatch(
      const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
      const std::string &specific_target = "") = 0;

  virtual BatchTargetResult sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const std::string &specific_target = "") = 0;

  // Failure Protector operations
  virtual void resetFailureProtector(const std::string &target_name) = 0;
  virtual void resetAllFailureProtectors() = 0;

  // Statistics
  virtual GatewayStats getStats() const = 0;
  virtual void resetStats() = 0;
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_I_TARGET_RUNNER_H
