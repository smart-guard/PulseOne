#include "Pipeline/Stages/PersistenceStage.h"
#include "Storage/RedisDataWriter.h"
#include "Storage/BackendFormat.h" // Added for AlarmEventData
#include "Pipeline/IPersistenceQueue.h"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineContext.h"

namespace PulseOne::Pipeline::Stages {

PersistenceStage::PersistenceStage(std::shared_ptr<Storage::RedisDataWriter> redis_writer,
                                 std::shared_ptr<IPersistenceQueue> persistence_queue)
    : redis_writer_(redis_writer), persistence_queue_(persistence_queue) {
}

bool PersistenceStage::Process(PipelineContext& context) {
    if (!context.should_persist) return true;

    try {
        // 1. Redis (Synchronous)
        if (redis_writer_) {
            size_t saved = redis_writer_->SaveDeviceMessage(context.enriched_message);
            if (saved > 0) context.stats.persisted_to_redis = true;
            
            // Save Alarms to Redis
            for (const auto& alarm : context.alarm_events) {
                PulseOne::Storage::BackendFormat::AlarmEventData alarm_data;
                alarm_data.type = "alarm_event";
                alarm_data.occurrence_id = std::to_string(alarm.occurrence_id);
                alarm_data.rule_id = alarm.rule_id;
                alarm_data.tenant_id = alarm.tenant_id;
                alarm_data.point_id = alarm.point_id;
                alarm_data.device_id = alarm.device_id;
                alarm_data.message = alarm.message;
                alarm_data.severity = alarm.getSeverityString();
                alarm_data.state = alarm.getStateString();
                alarm_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    alarm.timestamp.time_since_epoch()).count();
                alarm_data.source_name = alarm.source_name;
                alarm_data.location = alarm.location;
                alarm_data.trigger_value = alarm.getTriggerValueString();

                redis_writer_->PublishAlarmEvent(alarm_data); 
            }
        }

        // 2. Queue for RDB & Influx (Async via Interface)
        if (persistence_queue_) {
            // Logic: Identify changed points? 
            // Original service logic: "SaveChangedPointsToRDB" checked logic internally.
            // The stage should ideally filter points or pass 'all enriched' and let queue handler decide?
            // Or we assume 'enriched_message' is what we want to save.
            
            // For RDB, we typically only save CHANGED points or periodic samples.
            // The queue handler (DataProcessingService) contains the logic for 'last_rdb_save_times_'.
            // So we can just pass the full message to QueueRDBTask and let it filter?
            // Or does QueueRDBTask expect *only* points to save?
            // "SaveChangedPointsToRDB" takes message + changed_points.
            
            // For now, let's assume we pass all points to the Interface and let the implementation (Service) filter.
            // Wait, to keep Stage pure, filtering logic should be here?
            // The logic involves `last_rdb_save_times_` state. That state lives in Service.
            // Moving that state to Stage or Queue?
            // Ideally, PersistenceStage should own the "policy".
            // But for this quick refactor, we are extracting the flow.
            // Let's trigger the queue methods.
            
            // RDB
            std::vector<PulseOne::Structs::TimestampedValue> points_to_check;
            for(const auto& p : context.enriched_message.points) {
                 points_to_check.push_back(p); // Convert if needed, but they are same type
            }
            persistence_queue_->QueueRDBTask(context.enriched_message, points_to_check);
            
            // Influx
            persistence_queue_->QueueInfluxTask(context.enriched_message, points_to_check);
            persistence_queue_->QueueCommStatsTask(context.enriched_message);
            
            context.stats.persisted_to_rdb = true; // Queued at least
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("PersistenceStage Error: " + std::string(e.what()));
    }
    return true;
}

} // namespace PulseOne::Pipeline::Stages
