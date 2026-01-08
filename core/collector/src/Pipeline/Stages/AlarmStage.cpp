#include "Pipeline/Stages/AlarmStage.h"
#include "Alarm/AlarmManager.h"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineContext.h"

namespace PulseOne::Pipeline::Stages {

AlarmStage::AlarmStage() {
}

bool AlarmStage::Process(PipelineContext& context) {
    if (!context.should_evaluate_alarms) return true;

    try {
        auto& alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();
        
        if (!alarm_manager.isInitialized()) {
            return true;
        }

        auto alarm_events = alarm_manager.evaluateForMessage(context.enriched_message);
        
        if (!alarm_events.empty()) {
            context.stats.alarms_triggered = alarm_events.size();
            context.alarm_events = alarm_events;
        }

    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmStage Error: " + std::string(e.what()));
    }
    return true;
}

} // namespace PulseOne::Pipeline::Stages
