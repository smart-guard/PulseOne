/**
 * @file GatewayService.cpp
 * @brief Main Gateway Service Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/GatewayService.h"
#include "Database/Repositories/EdgeServerRepository.h"
#include "Database/RepositoryFactory.h"
#include "Gateway/Service/EventDispatcher.h"
#include "Logging/LogManager.h"
#include "Schedule/ScheduledExporter.h"

namespace PulseOne {
namespace Gateway {
namespace Service {

GatewayService::GatewayService(std::unique_ptr<GatewayContext> context)
    : context_(std::move(context)) {

  heartbeat_service_ = std::make_unique<HeartbeatService>(
      context_->getGatewayId(), context_->getRedisClient());
  event_dispatcher_ = std::make_unique<EventDispatcher>(*context_);

  // EventSubscriberConfig initialization (simplified for this context)
  PulseOne::Event::EventSubscriberConfig sub_config;
  // Note: We'd normally populate this from a common config, but here we assume
  // context_ has enough info or we use defaults

  event_subscriber_ =
      std::make_unique<PulseOne::Event::EventSubscriber>(sub_config);
}

GatewayService::~GatewayService() { stop(); }

bool GatewayService::start() {
  if (is_running_.load())
    return false;

  LogManager::getInstance().Info("GatewayService starting...");

  // 1. Load Registry
  if (!context_->getRegistry().loadFromDatabase()) {
    LogManager::getInstance().Error(
        "GatewayService: Failed to load target registry");
    return false;
  }

  // 2. Start Heartbeat
  heartbeat_service_->start();

  // 3. Register and Start Event Subscriber
  event_dispatcher_->registerHandlers(*event_subscriber_);

  // ✅ Instance specific command channel (v3.2.1)
  event_subscriber_->subscribeChannel("cmd:gateway:" +
                                      std::to_string(context_->getGatewayId()));

  // ✅ DB-driven Subscription Mode
  try {
    auto repo = PulseOne::Database::RepositoryFactory::getInstance()
                    .getEdgeServerRepository();
    auto gateway_entity = repo->findById(context_->getGatewayId());

    if (gateway_entity) {
      std::string sub_mode = gateway_entity->getSubscriptionMode();
      LogManager::getInstance().Info("Gateway Subscription Mode: " + sub_mode);

      if (sub_mode == "all") { // Explicit global subscription
        event_subscriber_->subscribeChannel("alarms:all");
        LogManager::getInstance().Info(
            "Subscribed to global alarms (alarms:all)");
      } else {
        LogManager::getInstance().Info(
            "Running in SELECTIVE subscription mode (assigned devices only)");
      }
    } else {
      LogManager::getInstance().Warn(
          "Failed to load Gateway Entity from DB. ID: " +
          std::to_string(context_->getGatewayId()));
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to load subscription config: " +
                                    std::string(e.what()));
  }

  // Update selective subscription if needed
  auto device_ids = context_->getRegistry().getAssignedDeviceIds();
  if (!device_ids.empty()) {
    event_subscriber_->updateSubscriptions(device_ids);
  }

  if (!event_subscriber_->start()) {
    LogManager::getInstance().Error(
        "GatewayService: Failed to start EventSubscriber");
    return false;
  }

  is_running_ = true;
  LogManager::getInstance().Info("GatewayService started successfully ✅");
  return true;
}

void GatewayService::stop() {
  if (!is_running_.load())
    return;

  LogManager::getInstance().Info("GatewayService stopping...");

  event_subscriber_->stop();
  heartbeat_service_->stop();

  is_running_ = false;
  LogManager::getInstance().Info("GatewayService stopped");
}

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
