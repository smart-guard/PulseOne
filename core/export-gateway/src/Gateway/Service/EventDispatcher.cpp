/**
 * @file EventDispatcher.cpp
 * @brief Event Dispatcher Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/EventDispatcher.h"
#include "Constants/ExportConstants.h"
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
  // Logic translated from ExportCoordinator::handleAlarmEvent
  // (Ignoring batching for simplicity for now, will add later)
  context_.getRunner().sendAlarm(alarm);
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
    for (const auto &target : targets) {
      if (!target.enabled)
        continue;
      auto res = context_.getRunner().sendAlarmToTarget(
          target.name,
          PulseOne::CSP::AlarmMessage()); // Empty alarm for manual?
      // Actually manual export usually includes data.
    }
  } else {
    for (const auto &name : target_names) {
      context_.getRunner().sendAlarmToTarget(name,
                                             PulseOne::CSP::AlarmMessage());
    }
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

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
