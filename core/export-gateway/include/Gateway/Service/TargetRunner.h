/**
 * @file TargetRunner.h
 * @brief Target Runner Implementation - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_TARGET_RUNNER_H
#define GATEWAY_SERVICE_TARGET_RUNNER_H

#include "Gateway/Service/FailureProtector.h"
#include "Gateway/Service/ITargetRegistry.h"
#include "Gateway/Service/ITargetRunner.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief Implementation of ITargetRunner
 */
class TargetRunner : public ITargetRunner {
private:
  ITargetRegistry &registry_;

  mutable std::mutex protectors_mutex_;
  std::unordered_map<std::string, std::unique_ptr<FailureProtector>>
      failure_protectors_;

public:
  explicit TargetRunner(ITargetRegistry &registry);
  ~TargetRunner() override;

  std::vector<TargetSendResult>
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm) override;
  TargetSendResult sendAlarmToTarget(
      const std::string &target_name,
      const PulseOne::Gateway::Model::AlarmMessage &alarm) override;

  std::vector<TargetSendResult>
  sendFile(const std::string &local_path) override;

  BatchTargetResult sendAlarmBatch(
      const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
      const std::string &specific_target = "") override;

  BatchTargetResult sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const std::string &specific_target = "") override;

  void resetFailureProtector(const std::string &target_name) override;
  void resetAllFailureProtectors() override;

  // Registry access (for ScheduledExporter failure persistence)
  ITargetRegistry &getRegistry() { return registry_; }
  const ITargetRegistry &getRegistry() const { return registry_; }

  // Statistics
  GatewayStats getStats() const override;
  void resetStats() override;

private:
  FailureProtector *getOrCreateProtector(const std::string &target_name,
                                         const nlohmann::json &config);

  bool executeSend(const DynamicTarget &target, const nlohmann::json &payload,
                   const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   TargetSendResult &result);

  PulseOne::Gateway::Model::AlarmMessage
  applyMappings(const DynamicTarget &target,
                const PulseOne::Gateway::Model::AlarmMessage &alarm);

  // Statistics members
  mutable std::mutex stats_mutex_;
  GatewayStats stats_;
  void updateStats(const TargetSendResult &result);
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_TARGET_RUNNER_H
