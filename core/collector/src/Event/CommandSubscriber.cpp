/**
 * @file CommandSubscriber.cpp
 * @brief Collector-specific Command Subscriber Implementation
 * @author PulseOne Development Team
 * @date 2026-02-17
 */

#include "Event/CommandSubscriber.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Workers/WorkerManager.h"

namespace PulseOne {
namespace Core {

CommandSubscriber::CommandSubscriber(
    const PulseOne::Event::EventSubscriberConfig &config)
    : EventSubscriber(config) {
  collector_id_ = ConfigManager::getInstance().getCollectorId();
}

bool CommandSubscriber::start() {
  // Subscribe to broadcast channels
  subscribeChannel("config:reload");
  subscribeChannel("target:reload"); // Mirror export-gateway

  // Subscribe to collector-specific channel
  if (collector_id_ > 0) {
    std::string my_channel = "cmd:collector:" + std::to_string(collector_id_);
    subscribeChannel(my_channel);
    LogManager::getInstance().Info("CommandSubscriber - Subscribed to " +
                                   my_channel);
  } else {
    LogManager::getInstance().Warn(
        "CommandSubscriber - Collector ID not set, specific commands disabled");
  }

  return EventSubscriber::start();
}

void CommandSubscriber::routeMessage(const std::string &channel,
                                     const std::string &message) {
  LogManager::getInstance().Info("[C2-COMMAND] Received on channel: " +
                                 channel);

  if (channel == "config:reload") {
    handleConfigReload(message);
  } else if (channel.find("cmd:collector:") == 0) {
    handleCollectorCommand(channel, message);
  } else {
    // Fallback to base class handlers (registered via registerHandler)
    EventSubscriber::routeMessage(channel, message);
  }
}

void CommandSubscriber::handleConfigReload(const std::string &message) {
  LogManager::getInstance().Info(
      "🔄 CommandSubscriber - Reloading configuration...");

  try {
    // 1. Reload ConfigManager
    ConfigManager::getInstance().reload();
    LogManager::getInstance().Info("✅ Configuration reloaded successfully");

    // 2. Restart or Update Workers if needed
    // Note: WorkerManager might need a hook to react to config changes
    // PulseOne::Workers::WorkerManager::getInstance().reloadWorkers();

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to reload configuration: " +
                                    std::string(e.what()));
  }
}

void CommandSubscriber::handleCollectorCommand(const std::string &channel,
                                               const std::string &message) {
  LogManager::getInstance().Info(
      "⚡ CommandSubscriber - Specific command received: " + message);

  try {
    auto j = json::parse(message);
    std::string command = j.value("command", "");

    if (command == "scan") {
      LogManager::getInstance().Info("🚀 Executing network scan...");
      // Trigger scan logic (e.g., via WorkerManager)
    } else if (command == "stop") {
      LogManager::getInstance().Info("🛑 Stopping collector via C2...");
      // Trigger application shutdown
    } else {
      LogManager::getInstance().Warn("Unknown command: " + command);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to parse command JSON: " +
                                    std::string(e.what()));
  }
}

} // namespace Core
} // namespace PulseOne
