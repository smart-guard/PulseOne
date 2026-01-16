#include "Pipeline/Stages/AlarmStage.h"
#include "Alarm/AlarmManager.h"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineContext.h"

namespace PulseOne::Pipeline::Stages {

AlarmStage::AlarmStage() {}

bool AlarmStage::Process(PipelineContext &context) {
  if (!context.should_evaluate_alarms) {
    LogManager::getInstance().Debug(
        "AlarmStage: Skipped (evaluation disabled)");
    return true;
  }

  try {
    auto &alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();

    if (!alarm_manager.isInitialized()) {
      LogManager::getInstance().Error(
          "AlarmStage: AlarmManager not initialized");
      return true;
    }

    LogManager::getInstance().log(
        "alarm", PulseOne::Enums::LogLevel::DEBUG,
        "AlarmStage: Processing for Tenant " +
            std::to_string(context.enriched_message.tenant_id) + ", Device " +
            context.enriched_message.device_id + ", Points: " +
            std::to_string(context.enriched_message.points.size()));

    auto alarm_events =
        alarm_manager.evaluateForMessage(context.enriched_message);

    if (!alarm_events.empty()) {
      LogManager::getInstance().Info("AlarmStage: Generated " +
                                     std::to_string(alarm_events.size()) +
                                     " alarm events");
      context.stats.alarms_triggered = alarm_events.size();
      context.alarm_events = alarm_events;
    } else {
      LogManager::getInstance().log("alarm", PulseOne::Enums::LogLevel::DEBUG,
                                    "AlarmStage: No alarms generated");
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("AlarmStage Error: " +
                                    std::string(e.what()));
  }
  return true;
}

} // namespace PulseOne::Pipeline::Stages
