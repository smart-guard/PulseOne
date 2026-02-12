/**
 * @file TargetRunner.cpp
 * @brief Target Runner Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/TargetRunner.h"
#include "Constants/ExportConstants.h"
#include "Gateway/Target/ITargetHandler.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include <algorithm>
#include <thread>

namespace PulseOne {
namespace Gateway {
namespace Service {

namespace ExportConst = PulseOne::Constants::Export;

TargetRunner::TargetRunner(ITargetRegistry &registry) : registry_(registry) {
  stats_.start_time = std::chrono::system_clock::now();
  LogManager::getInstance().Info("TargetRunner initialized");
}

TargetRunner::~TargetRunner() {}

std::vector<TargetSendResult>
TargetRunner::sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm) {
  std::vector<TargetSendResult> results;
  auto targets = registry_.getAllTargets();

  for (const auto &target : targets) {
    if (!target.is_active)
      continue;

    const json &target_config = target.config;

    // Filter by export_mode
    std::string export_mode = target_config.value(
        ExportConst::ConfigKeys::EXPORT_MODE, ExportConst::ExportMode::ALARM);
    std::string mode_upper = export_mode;
    std::transform(mode_upper.begin(), mode_upper.end(), mode_upper.begin(),
                   ::toupper);

    if (mode_upper != "ALARM" && mode_upper != "EVENT" &&
        mode_upper != "REALTIME" && mode_upper != "BATCH") {
      continue;
    }

    results.push_back(sendAlarmToTarget(target.name, alarm));
  }

  return results;
}

TargetSendResult TargetRunner::sendAlarmToTarget(
    const std::string &target_name,
    const PulseOne::Gateway::Model::AlarmMessage &alarm) {
  TargetSendResult result;
  result.target_name = target_name;
  result.success = false;

  LogManager::getInstance().Info(
      "TargetRunner: Entering sendAlarmToTarget for target=" + target_name);
  auto target_opt = registry_.getTarget(target_name);
  if (!target_opt) {
    LogManager::getInstance().Error(
        "TargetRunner: Target not found in registry: " + target_name);
    result.error_message = "Target not found: " + target_name;
    return result;
  }

  const auto &target = *target_opt;
  LogManager::getInstance().Info(
      "TargetRunner: Found target ID=" + std::to_string(target.id) +
      " Type=" + target.type);
  result.target_id = target.id;
  result.target_type = target.type;

  const json &target_config = target.config;
  auto *protector = getOrCreateProtector(target.name, target_config);
  if (protector && !protector->canExecute()) {
    LogManager::getInstance().Warn(
        "TargetRunner: Circuit breaker open for target=" + target_name);
    result.error_message = "Circuit breaker open: " + target_name;
    return result;
  }

  // Pre-execution delay
  int delay_ms = target_config.value("execution_delay_ms", 0);
  if (delay_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }

  // [v3.2.1] Manual Override: Skip ALL mappings/enrichment for manual tests.
  PulseOne::Gateway::Model::AlarmMessage processed_alarm;
  if (alarm.manual_override) {
    LogManager::getInstance().Info(
        "TargetRunner: Manual Override active, bypassing mappings.");
    processed_alarm = alarm;
  } else {
    // [v4.1.0] Point-based Whitelist Filtering
    // Only proceed if the point is explicitly mapped for this target.
    if (!registry_.isPointMapped(target.id, alarm.point_id)) {
      LogManager::getInstance().Info("[WHITELIST] Point '" + alarm.point_name +
                                     "' (ID:" + std::to_string(alarm.point_id) +
                                     ") is NOT in Profile for Target '" +
                                     target_name + "'. Skipping.");
      result.error_message = "Skipped: Not in whitelist";
      result.skipped =
          true; // [v4.1.1] Mark as skipped to avoid logging in history
      result.success =
          true; // Mark as 'handled success' to avoid internal retry/error loops
      return result;
    }

    processed_alarm = applyMappings(target, alarm);
    LogManager::getInstance().Info(
        "[ALARM_ENRICH] Mappings applied for " + target_name +
        ": PointName=" + processed_alarm.point_name +
        ", SiteID=" + std::to_string(processed_alarm.site_id));
  }

  auto start_time = std::chrono::steady_clock::now();

  // [v4.0.0] Centralized Payload Transformation
  json payload;
  try {
    payload =
        PulseOne::Transform::PayloadTransformer::getInstance().buildPayload(
            processed_alarm, target_config);
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("TargetRunner: Transformation failed for " +
                                    target_name + ": " + e.what());
    result.error_message = "Transformation logic error";
    return result;
  }

  if (executeSend(target, payload, processed_alarm, result)) {
    if (protector)
      protector->recordSuccess();
    result.success = true;
  } else {
    if (protector)
      protector->recordFailure();
    result.success = false;
  }

  auto end_time = std::chrono::steady_clock::now();
  result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  updateStats(result);

  return result;
}

std::vector<TargetSendResult>
TargetRunner::sendFile(const std::string &local_path) {
  std::vector<TargetSendResult> results;
  auto targets = registry_.getAllTargets();

  for (const auto &target : targets) {
    if (!target.is_active)
      continue;

    const json &target_config = target.config;

    auto *handler = registry_.getHandler(target.name);
    if (handler) {
      auto result = handler->sendFile(local_path, target_config);
      result.target_name = target.name;
      result.target_type = target.type;
      results.push_back(result);
    }
  }

  return results;
}

BatchTargetResult TargetRunner::sendAlarmBatch(
    const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
    const std::string &specific_target) {
  BatchTargetResult batch_result;
  auto targets = registry_.getAllTargets();

  for (const auto &target : targets) {
    if (!target.is_active)
      continue;

    const json &target_config = target.config;
    if (!specific_target.empty() && target.name != specific_target)
      continue;

    auto *handler = registry_.getHandler(target.name);
    if (!handler)
      continue;

    std::vector<PulseOne::Gateway::Model::AlarmMessage> processed_batch;
    std::vector<json> payloads;
    auto &transformer = PulseOne::Transform::PayloadTransformer::getInstance();

    for (const auto &alarm : alarms) {
      // [v4.1.0] Point-based Whitelist Filtering for Batch
      if (!registry_.isPointMapped(target.id, alarm.point_id)) {
        continue;
      }

      auto processed = applyMappings(target, alarm);
      processed_batch.push_back(processed);
      payloads.push_back(transformer.buildPayload(processed, target_config));
    }

    auto results =
        handler->sendAlarmBatch(payloads, processed_batch, target_config);
    for (const auto &res : results) {
      batch_result.results.push_back(res);
      if (res.success)
        batch_result.success_count++;
      else
        batch_result.failure_count++;
    }
  }

  batch_result.total_count =
      batch_result.success_count + batch_result.failure_count;
  return batch_result;
}

BatchTargetResult TargetRunner::sendValueBatch(
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const std::string &specific_target) {
  BatchTargetResult batch_result;
  // Implementation for value batch...
  return batch_result;
}

void TargetRunner::resetFailureProtector(const std::string &target_name) {
  std::lock_guard<std::mutex> lock(protectors_mutex_);
  auto it = failure_protectors_.find(target_name);
  if (it != failure_protectors_.end()) {
    it->second->reset();
  }
}

void TargetRunner::resetAllFailureProtectors() {
  std::lock_guard<std::mutex> lock(protectors_mutex_);
  for (auto &pair : failure_protectors_) {
    pair.second->reset();
  }
}

PulseOne::CSP::FailureProtector *
TargetRunner::getOrCreateProtector(const std::string &target_name,
                                   const json &config) {
  std::lock_guard<std::mutex> lock(protectors_mutex_);
  auto it = failure_protectors_.find(target_name);
  if (it != failure_protectors_.end())
    return it->second.get();

  PulseOne::CSP::FailureProtectorConfig fp_config;
  if (config.contains(ExportConst::ConfigKeys::FAILURE_THRESHOLD))
    fp_config.failure_threshold =
        config[ExportConst::ConfigKeys::FAILURE_THRESHOLD];

  auto protector =
      std::make_unique<PulseOne::CSP::FailureProtector>(target_name, fp_config);
  auto *ptr = protector.get();
  failure_protectors_[target_name] = std::move(protector);
  return ptr;
}

bool TargetRunner::executeSend(
    const DynamicTarget &target, const json &payload,
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    TargetSendResult &result) {
  auto *handler = registry_.getHandler(target.name);
  if (!handler) {
    result.error_message = "Handler not found for target: " + target.name;
    return false;
  }

  const json &target_config = target.config;
  result = handler->sendAlarm(payload, alarm, target_config);

  // Restore target identity (handlers might overwrite/ignore these)
  result.target_id = target.id;
  result.target_name = target.name;
  result.target_type = target.type;

  return result.success;
}

PulseOne::Gateway::Model::AlarmMessage TargetRunner::applyMappings(
    const DynamicTarget &target,
    const PulseOne::Gateway::Model::AlarmMessage &alarm) {
  PulseOne::Gateway::Model::AlarmMessage processed = alarm;

  // Point name mapping
  std::string mapped_nm =
      registry_.getTargetFieldName(target.id, alarm.point_id);
  if (!mapped_nm.empty())
    processed.point_name = mapped_nm;

  // Site ID / Building ID mapping
  int site_id = registry_.getOverrideSiteId(target.id, alarm.point_id);
  if (site_id == -1)
    site_id = alarm.site_id;

  std::string ext_bd = registry_.getExternalBuildingId(target.id, site_id);
  if (!ext_bd.empty()) {
    try {
      processed.site_id = std::stoi(ext_bd);
    } catch (...) {
    }
  } else if (site_id != alarm.site_id) {
    processed.site_id = site_id;
  }

  return processed;
}

GatewayStats TargetRunner::getStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void TargetRunner::resetStats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = GatewayStats();
  stats_.start_time = std::chrono::system_clock::now();
}

void TargetRunner::updateStats(const TargetSendResult &result) {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_.total_exports++;
  if (result.success) {
    stats_.successful_exports++;
  } else {
    stats_.failed_exports++;
  }

  stats_.last_export_time = std::chrono::system_clock::now();

  // Average processing time calculation
  double current_ms = result.response_time.count();
  if (stats_.total_exports == 1) {
    stats_.avg_processing_time_ms = current_ms;
  } else {
    stats_.avg_processing_time_ms =
        (stats_.avg_processing_time_ms * (stats_.total_exports - 1) +
         current_ms) /
        stats_.total_exports;
  }

  if (result.target_type == "ALARM" || result.target_type == "EVENT") {
    stats_.alarm_exports++;
  }
}

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
