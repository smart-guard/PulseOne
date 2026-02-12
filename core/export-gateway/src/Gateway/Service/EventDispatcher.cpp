/**
 * @file EventDispatcher.cpp
 * @brief Event Dispatcher Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/EventDispatcher.h"
#include "Constants/ExportConstants.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/RepositoryFactory.h"
#include "Export/ExportLogService.h"
#include "Logging/LogManager.h"
#include <algorithm>

namespace PulseOne {
namespace Gateway {
namespace Service {

namespace ExportConst = PulseOne::Constants::Export;

// Bridge classes to connect EventSubscriber to EventDispatcher
class AlarmHandlerBridge {
  EventDispatcher &dispatcher_;

public:
  explicit AlarmHandlerBridge(EventDispatcher &d) : dispatcher_(d) {}
  void operator()(const PulseOne::CSP::AlarmMessage &alarm) {
    dispatcher_.handleAlarm(alarm);
  }
};

class GenericHandlerBridge : public PulseOne::Event::IEventHandler {
  EventDispatcher &dispatcher_;
  std::string name_;
  typedef void (EventDispatcher::*HandlerFunc)(const std::string &,
                                               const std::string &);
  HandlerFunc func_;

public:
  GenericHandlerBridge(EventDispatcher &d, const std::string &name,
                       HandlerFunc f)
      : dispatcher_(d), name_(name), func_(f) {}

  bool handleEvent(const std::string &channel,
                   const std::string &message) override {
    (dispatcher_.*func_)(channel, message);
    return true;
  }
  std::string getName() const override { return name_; }
};

EventDispatcher::EventDispatcher(GatewayContext &context) : context_(context) {}

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::registerHandlers(
    PulseOne::Event::EventSubscriber &subscriber) {
  subscriber.setAlarmCallback(AlarmHandlerBridge(*this));

  subscriber.registerHandler(
      "schedule:*",
      std::make_shared<GenericHandlerBridge>(
          *this, "ScheduleHandler", &EventDispatcher::handleScheduleEvent));
  subscriber.registerHandler(
      "config:*",
      std::make_shared<GenericHandlerBridge>(
          *this, "ConfigHandler", &EventDispatcher::handleConfigEvent));
  subscriber.registerHandler(
      "target:*",
      std::make_shared<GenericHandlerBridge>(
          *this, "ConfigHandler", &EventDispatcher::handleConfigEvent));
  // [v3.1.0] Precision Channel Subscription (Fix for Double Export)
  // Instead of subscribing to the global `cmd:*` pattern (which causes overlaps
  // and cross-talk), we subscribe ONLY to our specific gateway command channel.
  std::string cmd_channel =
      "cmd:gateway:" + std::to_string(context_.getGatewayId());

  subscriber.registerHandler(
      cmd_channel,
      std::make_shared<GenericHandlerBridge>(
          *this, "CommandHandler", &EventDispatcher::handleCommandEvent));

  subscriber.subscribeChannel(cmd_channel);
}

void EventDispatcher::handleAlarm(const PulseOne::CSP::AlarmMessage &alarm) {
  LogManager::getInstance().Info(
      "[ALARM_RECEIVE] From Redis: Point=" + std::to_string(alarm.point_id) +
      " (" + alarm.point_name + ")");

  LogManager::getInstance().Info(
      "[ALARM_PARSE] Internal State: Site=" + std::to_string(alarm.site_id) +
      ", Point=" + alarm.point_name +
      ", Value=" + std::to_string(alarm.measured_value) +
      ", Level=" + std::to_string(alarm.alarm_level) + ", Status=" +
      std::to_string(alarm.status_code) + ", Timestamp=" + alarm.timestamp);

  // 1. 전송 실행
  auto results = context_.getRunner().sendAlarm(alarm);

  // 2. 결과 로깅
  for (const auto &result : results) {
    LogManager::getInstance().Info(
        "[ALARM_SEND] Target Dispatch: " + result.target_name +
        " | Success: " + (result.success ? "YES" : "NO") +
        " | Code: " + std::to_string(result.status_code));
    logExportResult(result, &alarm);
  }
}

void EventDispatcher::handleScheduleEvent(const std::string &channel,
                                          const std::string &message) {
  LogManager::getInstance().Info("EventDispatcher: Schedule event: " + channel);
  // Integration with ScheduledExporter...
}

void EventDispatcher::handleConfigEvent(const std::string &channel,
                                        const std::string &message) {
  LogManager::getInstance().Info("EventDispatcher: Config event: " + channel);
  if (channel == ExportConst::Redis::CHANNEL_CONFIG_RELOAD ||
      channel == ExportConst::Redis::CHANNEL_TARGET_RELOAD) {
    context_.getRegistry().loadFromDatabase();
  }
}

void EventDispatcher::handleCommandEvent(const std::string &channel,
                                         const std::string &message) {
  try {
    auto j = nlohmann::json::parse(message);
    LogManager::getInstance().Info(
        "EventDispatcher: Received command on channel [" + channel +
        "]: " + message);

    LogManager::getInstance().Info("[TRACE-1-RECEIVE] Manual Export Command: " +
                                   channel + " | Raw: " + message);

    // ServerId check
    if (j.contains("serverId")) {
      std::string server_id_str =
          j["serverId"].is_string() ? j["serverId"].get<std::string>()
                                    : std::to_string(j["serverId"].get<int>());
      if (server_id_str != std::to_string(context_.getGatewayId()))
        return;
    }

    std::string command = j.value("command", "");
    nlohmann::json payload = j.value("payload", nlohmann::json::object());

    if (command == ExportConst::Command::MANUAL_EXPORT) {
      handleManualExport(payload);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("EventDispatcher: Command error: " +
                                    std::string(e.what()));
  }
}

void EventDispatcher::handleManualExport(const nlohmann::json &payload) {
  LogManager::getInstance().Info(
      "EventDispatcher: Starting handleManualExport with payload: " +
      payload.dump());
  try {
    std::vector<std::string> target_names;
    if (payload.contains("targets") && payload["targets"].is_array()) {
      for (const auto &t : payload["targets"])
        if (t.is_string())
          target_names.push_back(t.get<std::string>());
    } else {
      std::string name = payload.value(ExportConst::JsonKeys::TARGET_NAME, "");
      if (!name.empty())
        target_names.push_back(name);
    }

    LogManager::getInstance().Info(
        "[TRACE-2-PARSE] Manual Export Data: PointId=" +
        std::to_string(payload.value("point_id", 0)) +
        ", Targets=" + std::to_string(target_names.size()));

    bool is_all = false;
    for (const auto &name : target_names) {
      std::string upper_name = name;
      std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                     ::toupper);
      if (upper_name == "ALL") {
        is_all = true;
        break;
      }
    }

    if (is_all) {
      auto targets = context_.getRegistry().getAllTargets();
      target_names.clear();
      for (const auto &target : targets) {
        if (target.is_active) {
          target_names.push_back(target.name);
        }
      }
    }

    if (target_names.empty()) {
      LogManager::getInstance().Warn("Manual export: No available targets");
      return;
    }
    LogManager::getInstance().Info("Manual export: Expanded target list: " +
                                   std::to_string(target_names.size()) +
                                   " targets");

    // [v3.2.2] Manual Export Payload Integrity:
    // Prioritize 'raw_payload' as the absolute source of truth.
    nlohmann::json raw_source_full =
        payload.contains("raw_payload") ? payload["raw_payload"] : payload;
    nlohmann::json raw_source_for_struct = raw_source_full;

    if (raw_source_full.is_array() && !raw_source_full.empty()) {
      raw_source_for_struct =
          raw_source_full[0]; // Use first element for struct fields
    }

    PulseOne::CSP::AlarmMessage alarm;
    alarm.from_json(raw_source_for_struct);
    alarm.manual_override = true;
    alarm.extra_info = raw_source_full; // [CRITICAL] Preserve full array/object
                                        // for transmission

    if (alarm.description.empty()) {
      alarm.description = "Manual Export Triggered";
    }

    LogManager::getInstance().Info(
        "[TRACE-3-ENRICH] Manual Export Ready (RAW Bypass): Point=" +
        alarm.point_name + ", Site=" + std::to_string(alarm.site_id));
    LogManager::getInstance().Info(
        "Manual export: Starting execution loop for " +
        std::to_string(target_names.size()) + " targets");

    // Execute export for each target
    for (const auto &name : target_names) {
      LogManager::getInstance().Info("Executing manual export: Target=" + name +
                                     ", Point=" + alarm.point_name);

      // Create a dedicated alarm message for this target to apply specific
      // overrides
      PulseOne::CSP::AlarmMessage target_alarm = alarm;

      // Apply UI/DB Mapping Overrides specific to this target
      auto target_opt = context_.getRegistry().getTarget(name);
      if (target_opt.has_value()) {
        int target_id = target_opt->id;

        // 1. Target Key (FieldName) Override
        std::string mapped_name = context_.getRegistry().getTargetFieldName(
            target_id, alarm.point_id);
        if (!mapped_name.empty()) {
          target_alarm.point_name = mapped_name;
        }

        // 2. Site ID Override
        int override_site_id =
            context_.getRegistry().getOverrideSiteId(target_id, alarm.point_id);
        if (override_site_id > 0) {
          target_alarm.site_id = override_site_id;
        }
      }

      // Execute Send
      LogManager::getInstance().Info(
          "[TRACE-4-SEND] Dispatching to Target: " + name +
          " (MappedName=" + target_alarm.point_name + ")");
      auto res = context_.getRunner().sendAlarmToTarget(name, target_alarm);

      // Ensure target_id is populated in result for enrichment (Manual export
      // specific)
      if (target_opt.has_value()) {
        res.target_id = target_opt->id;
      }

      // Log and Notify
      logExportResult(res, &target_alarm);
      sendManualExportResult(name, res.success, res.error_message, payload,
                             res.target_id);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Manual export internal error: " +
                                    std::string(e.what()));
  }
}

void EventDispatcher::sendManualExportResult(const std::string &target_name,
                                             bool success,
                                             const std::string &error_message,
                                             const nlohmann::json &payload,
                                             int target_id) {
  if (context_.getRedisClient() && context_.getRedisClient()->isConnected()) {
    json result;
    result["targetName"] = target_name;
    result["targetId"] = target_id;
    result["success"] = success;
    result["errorMessage"] = error_message;
    result["payload"] = payload;
    result["gatewayId"] = context_.getGatewayId();
    result["timestamp"] =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    context_.getRedisClient()->publish(
        ExportConst::Redis::CHANNEL_CMD_GATEWAY_RESULT, result.dump());
  }
}

void EventDispatcher::logExportResult(
    const PulseOne::Export::TargetSendResult &result,
    const PulseOne::CSP::AlarmMessage *alarm) {
  try {
    using namespace PulseOne::Database::Entities;

    LogManager::getInstance().Info("[TRACE] EventDispatcher::logExportResult - "
                                   "Enqueuing log for target: " +
                                   result.target_name);

    // [v4.1.1] Skip logging for intentionally skipped transmissions (whitelist)
    if (result.skipped) {
      LogManager::getInstance().Info(
          "[TRACE] EventDispatcher::logExportResult - "
          "Skipping log save because result is marked as skipped.");
      return;
    }

    ExportLogEntity log_entity;
    bool is_manual = alarm && alarm->manual_override;
    log_entity.setLogType(is_manual
                              ? "manual_export"
                              : (alarm ? "alarm_export" : "manual_export"));
    log_entity.setServiceId(context_.getGatewayId());

    // Target ID 및 이름 저장 (ID 매칭 실패 시 에러 메시지에 상세 기록)
    if (result.target_id > 0) {
      log_entity.setTargetId(result.target_id);
    } else if (!result.target_name.empty()) {
      auto target_opt = context_.getRegistry().getTarget(result.target_name);
      if (target_opt) {
        log_entity.setTargetId(target_opt->id);
      }
    }

    if (alarm) {
      // [v3.1.0] Logging Consistency: Recording the ACTUAL transformed payload
      // as the source value so history reflects the refactored template format.
      if (alarm->manual_override) {
        log_entity.setSourceValue(
            alarm->extra_info.is_null() ? "{}" : alarm->extra_info.dump());
      } else {
        // [IMPORTANT] Record the SENT payload (templated) instead of RAW alarm
        log_entity.setSourceValue(result.sent_payload.empty()
                                      ? alarm->to_json().dump()
                                      : result.sent_payload);
      }
      log_entity.setPointId(alarm->point_id);
    } else {
      // Manual Export: Use a simple indicator as the source
      log_entity.setSourceValue("{\"manual\":true}");
    }

    log_entity.setConvertedValue(result.sent_payload);
    log_entity.setSentPayload(result.sent_payload);   // [v3.2.0] New field
    log_entity.setGatewayId(context_.getGatewayId()); // [v3.2.0] New field

    log_entity.setStatus(result.success ? "success" : "failed");
    log_entity.setHttpStatusCode(result.status_code);
    log_entity.setErrorMessage(result.error_message);
    log_entity.setResponseData(result.response_body);
    log_entity.setProcessingTimeMs(
        static_cast<int>(result.response_time.count()));

    // 비동기 서비스에 큐잉
    PulseOne::Export::ExportLogService::getInstance().enqueueLog(log_entity);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("로깅 실패: " + std::string(e.what()));
  }
}

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
