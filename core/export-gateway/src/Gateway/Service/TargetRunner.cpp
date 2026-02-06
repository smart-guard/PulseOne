/**
 * @file TargetRunner.cpp
 * @brief Target Runner Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/TargetRunner.h"
#include "Constants/ExportConstants.h"
#include "Gateway/Target/ITargetHandler.h"
#include "Logging/LogManager.h"
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
    if (!target.enabled)
      continue;

    // Filter by export_mode
    std::string export_mode = target.config.value(
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

  auto target_opt = registry_.getTarget(target_name);
  if (!target_opt) {
    result.error_message = "Target not found: " + target_name;
    return result;
  }

  const auto &target = *target_opt;
  result.target_type = target.type;

  auto *protector = getOrCreateProtector(target.name, target.config);
  if (protector && !protector->canExecute()) {
    result.error_message = "Circuit breaker open: " + target_name;
    return result;
  }

  // Pre-execution delay
  if (target.execution_delay_ms > 0) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(target.execution_delay_ms));
  }

  auto processed_alarm = applyMappings(target, alarm);

  auto start_time = std::chrono::steady_clock::now();

  if (executeSend(target, processed_alarm, result)) {
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
    if (!target.enabled)
      continue;

    auto *handler = registry_.getHandler(target.name);
    if (handler) {
      auto result = handler->sendFile(local_path, target.config);
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
    if (!target.enabled)
      continue;
    if (!specific_target.empty() && target.name != specific_target)
      continue;

    auto *handler = registry_.getHandler(target.name);
    if (!handler)
      continue;

    std::vector<PulseOne::Gateway::Model::AlarmMessage> processed_batch;
    for (const auto &alarm : alarms) {
      processed_batch.push_back(applyMappings(target, alarm));
    }

    auto results = handler->sendAlarmBatch(processed_batch, target.config);
    for (const auto &res : results) {
      batch_result.results.push_back(res);
      if (res.success)
        batch_result.successful_targets++;
      else
        batch_result.failed_targets++;
    }
  }

  batch_result.total_targets =
      batch_result.successful_targets + batch_result.failed_targets;
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
    const DynamicTarget &target,
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    TargetSendResult &result) {
  auto *handler = registry_.getHandler(target.name);
  if (!handler) {
    result.error_message = "Handler not found for target: " + target.name;
    return false;
  }

  result = handler->sendAlarm(alarm, target.config);
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
    processed.nm = mapped_nm;

  // Site ID / Building ID mapping
  int site_id = registry_.getOverrideSiteId(target.id, alarm.point_id);
  if (site_id == -1)
    site_id = alarm.site_id;

  std::string ext_bd = registry_.getExternalBuildingId(target.id, site_id);
  if (!ext_bd.empty()) {
    try {
      processed.bd = std::stoi(ext_bd);
    } catch (...) {
    }
  } else if (site_id != alarm.site_id) {
    processed.bd = site_id;
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
