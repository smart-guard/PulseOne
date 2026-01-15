#include "Pipeline/Stages/PersistenceStage.h"
#include "Common/Utils.h"
#include "Logging/LogManager.h"
#include "Pipeline/IPersistenceQueue.h"
#include "Pipeline/PipelineContext.h"
#include "Storage/BackendFormat.h" // Added for AlarmEventData
#include "Storage/RedisDataWriter.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne::Pipeline::Stages {

PersistenceStage::PersistenceStage(
    std::shared_ptr<Storage::RedisDataWriter> redis_writer,
    std::shared_ptr<IPersistenceQueue> persistence_queue)
    : redis_writer_(redis_writer), persistence_queue_(persistence_queue) {}

bool PersistenceStage::Process(PipelineContext &context) {
  if (!context.should_persist)
    return true;

  try {
    // 1. Redis (Synchronous)
    if (redis_writer_) {
      LogManager::getInstance().Info(
          "[Persistence] Saving Message - DeviceID: " +
          context.enriched_message.device_id + ", Points: " +
          std::to_string(context.enriched_message.points.size()));
      if (!context.enriched_message.points.empty()) {
        LogManager::getInstance().Info(
            "[Persistence] First Point Value: " +
            PulseOne::Utils::DataVariantToString(
                context.enriched_message.points[0].value));
      }

      size_t saved = redis_writer_->SaveDeviceMessage(context.enriched_message);
      if (saved > 0)
        context.stats.persisted_to_redis = true;

      // Save Virtual Points specifically (for E2E and individual access)
      for (const auto &point : context.enriched_message.points) {
        if (point.is_virtual_point) {
          redis_writer_->StoreVirtualPointToRedis(point);
        }
      }

      // Save Alarms to Redis
      for (const auto &alarm : context.alarm_events) {
        PulseOne::Storage::BackendFormat::AlarmEventData alarm_data;
        alarm_data.rule_id = alarm.rule_id;
        alarm_data.tenant_id = alarm.tenant_id;
        alarm_data.device_id = context.enriched_message.device_id;
        alarm_data.point_id = alarm.point_id;
        alarm_data.state = alarm.getStateString();
        alarm_data.severity = alarm.getSeverityString();
        alarm_data.message = alarm.message;
        alarm_data.trigger_value = alarm.getTriggerValueString();
        alarm_data.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                alarm.timestamp.time_since_epoch())
                .count();

        redis_writer_->PublishAlarmEvent(alarm_data);
      }

      // ✅ Fix: Save Worker Status to Redis (for Green/Red lamp in UI)
      std::string worker_status = "error";
      if (context.enriched_message.device_status ==
          PulseOne::Enums::DeviceStatus::ONLINE) {
        worker_status = "running";
      } else if (context.enriched_message.device_status ==
                 PulseOne::Enums::DeviceStatus::OFFLINE) {
        worker_status = "stopped";
      }

      json metadata;
      metadata["protocol"] = context.enriched_message.protocol;
      metadata["last_updated"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              context.enriched_message.timestamp.time_since_epoch())
              .count();

      redis_writer_->SaveWorkerStatus(context.enriched_message.device_id,
                                      worker_status, metadata);
    }

    // 2. Queue for RDB and InfluxDB (Asynchronous)
    if (persistence_queue_) {
      // RDB Task
      persistence_queue_->QueueRDBTask(context.enriched_message,
                                       context.enriched_message.points);

      // InfluxDB Task
      persistence_queue_->QueueInfluxTask(context.enriched_message,
                                          context.enriched_message.points);

      // Comm Stats Task
      persistence_queue_->QueueCommStatsTask(context.enriched_message);
    }

    return true;
  } catch (const std::exception &e) {
    LogManager::getInstance().log("PersistenceStage", LogLevel::LOG_ERROR,
                                  "지원 스테이지 처리 실패: " +
                                      std::string(e.what()));
    return false;
  }
}

} // namespace PulseOne::Pipeline::Stages
