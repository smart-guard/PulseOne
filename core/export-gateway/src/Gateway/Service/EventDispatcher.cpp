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
  subscriber.registerHandler(
      "cmd:*",
      std::make_shared<GenericHandlerBridge>(
          *this, "CommandHandler", &EventDispatcher::handleCommandEvent));

  subscriber.subscribePattern("cmd:*");
}

void EventDispatcher::handleAlarm(const PulseOne::CSP::AlarmMessage &alarm) {
  // 1. 전송 실행
  auto results = context_.getRunner().sendAlarm(alarm);

  // 2. 결과 로깅
  for (const auto &result : results) {
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

    bool is_all = std::find(target_names.begin(), target_names.end(), "all") !=
                  target_names.end();

    if (is_all) {
      auto targets = context_.getRegistry().getAllTargets();
      target_names.clear();
      for (const auto &target : targets) {
        if (target.enabled) {
          target_names.push_back(target.name);
        }
      }
    }

    if (target_names.empty()) {
      LogManager::getInstance().Warn("Manual export: No available targets");
      return;
    }

    // Prepare AlarmMessage for enrichment
    PulseOne::CSP::AlarmMessage alarm;
    int point_id = payload.value("point_id", 0);

    if (point_id > 0) {
      alarm.point_id = point_id;

      // Redis에서 최신값 및 기본 메타데이터 조회
      if (context_.getRedisClient() &&
          context_.getRedisClient()->isConnected()) {
        std::string redis_key = "point:" + std::to_string(point_id) + ":latest";
        std::string val_json_str = context_.getRedisClient()->get(redis_key);
        if (!val_json_str.empty()) {
          try {
            auto val_json = nlohmann::json::parse(val_json_str);
            alarm.nm = val_json.value("nm", val_json.value("point_name", ""));
            alarm.site_id = val_json.value("bd", val_json.value("site_id", 0));
            alarm.bd = alarm.site_id;

            // Handle both 'vl' (standard) and 'value' (snapshots)
            std::string val_key = val_json.contains("value") ? "value" : "vl";
            if (val_json.contains(val_key)) {
              if (val_json[val_key].is_number()) {
                alarm.vl = val_json[val_key].get<double>();
              } else if (val_json[val_key].is_string()) {
                alarm.vl = std::stod(val_json[val_key].get<std::string>());
              }
            }
          } catch (...) {
            LogManager::getInstance().Warn(
                "Manual export: Failed to parse Redis value for point " +
                std::to_string(point_id));
          }
        }
      }

      // Database fallback for base metadata if still missing
      if (alarm.nm.empty() || alarm.site_id == 0) {
        try {
          auto &repo_factory =
              PulseOne::Database::RepositoryFactory::getInstance();
          if (repo_factory.isInitialized()) {
            auto point_repo = repo_factory.getDataPointRepository();
            auto point_opt = point_repo->findById(point_id);
            if (point_opt.has_value()) {
              if (alarm.nm.empty())
                alarm.nm = point_opt->getName();
              if (alarm.site_id == 0) {
                auto device_repo = repo_factory.getDeviceRepository();
                auto device_opt =
                    device_repo->findById(point_opt->getDeviceId());
                if (device_opt.has_value()) {
                  alarm.site_id = device_opt->getSiteId();
                  alarm.bd = alarm.site_id;
                }
              }
            }
          }
        } catch (const std::exception &e) {
          LogManager::getInstance().Error("Manual export: DB fallback error: " +
                                          std::string(e.what()));
        }
      }
    }

    // Payload overrides (Manual trigger bypass)
    if (payload.contains("value")) {
      alarm.vl = payload["value"].is_number() ? payload["value"].get<double>()
                                              : alarm.vl;
    }
    alarm.des = payload.value("des", "Manual Export Triggered");
    alarm.manual_override = true;

    // Execute export for each target
    for (const auto &name : target_names) {
      LogManager::getInstance().Info("Executing manual export: Target=" + name +
                                     ", Point=" + std::to_string(point_id));

      // Create a dedicated alarm message for this target to apply specific
      // overrides
      PulseOne::CSP::AlarmMessage target_alarm = alarm;

      // Apply UI/DB Mapping Overrides specific to this target
      auto target_opt = context_.getRegistry().getTarget(name);
      if (target_opt.has_value()) {
        int target_id = target_opt->id;

        // 1. Target Key (FieldName) Override
        std::string mapped_name =
            context_.getRegistry().getTargetFieldName(target_id, point_id);
        if (!mapped_name.empty()) {
          target_alarm.nm = mapped_name;
        }

        // 2. Site ID Override
        int override_site_id =
            context_.getRegistry().getOverrideSiteId(target_id, point_id);
        if (override_site_id > 0) {
          target_alarm.site_id = override_site_id;
          target_alarm.bd = override_site_id;
        }
      }

      // Execute Send
      auto res = context_.getRunner().sendAlarmToTarget(name, target_alarm);

      // Ensure target_id is populated in result for enrichment (Manual export
      // specific)
      if (target_opt.has_value()) {
        res.target_id = target_opt->id;
      }

      // Log and Notify
      logExportResult(res, &target_alarm);
      sendManualExportResult(name, res.success, res.error_message, payload);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Manual export internal error: " +
                                    std::string(e.what()));
  }
}

void EventDispatcher::sendManualExportResult(const std::string &target_name,
                                             bool success,
                                             const std::string &error_message,
                                             const nlohmann::json &payload) {
  if (context_.getRedisClient() && context_.getRedisClient()->isConnected()) {
    json result;
    result["targetName"] = target_name;
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

    ExportLogEntity log_entity;
    bool is_manual = alarm && alarm->manual_override;
    log_entity.setLogType(is_manual
                              ? "manual_export"
                              : (alarm ? "alarm_export" : "manual_export"));
    log_entity.setServiceId(context_.getGatewayId());

    // Target ID 조회
    if (result.target_id > 0) {
      log_entity.setTargetId(result.target_id);
    } else if (!result.target_name.empty()) {
      auto target_opt = context_.getRegistry().getTarget(result.target_name);
      if (target_opt) {
        log_entity.setTargetId(target_opt->id);
      }
    }

    if (alarm) {
      log_entity.setPointId(alarm->point_id);
      log_entity.setSourceValue(alarm->to_json().dump());
    } else {
      // Manual Export: Use a simple indicator as the source
      log_entity.setSourceValue("{\"manual\":true}");
    }

    log_entity.setConvertedValue(result.sent_payload);
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
