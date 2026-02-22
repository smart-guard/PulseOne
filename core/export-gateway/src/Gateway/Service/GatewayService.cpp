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

  // [FIX] Populate EventSubscriberConfig from environment variables
  // Previously used defaults (redis_host="localhost") which fails in Docker
  PulseOne::Event::EventSubscriberConfig sub_config;
  const char *redis_host_env = std::getenv("REDIS_HOST");
  const char *redis_port_env = std::getenv("REDIS_PORT");
  const char *redis_pass_env = std::getenv("REDIS_PASSWORD");
  sub_config.redis_host =
      (redis_host_env && redis_host_env[0] != '\0') ? redis_host_env : "redis";
  sub_config.redis_port = (redis_port_env && redis_port_env[0] != '\0')
                              ? std::stoi(redis_port_env)
                              : 6379;
  sub_config.redis_password =
      (redis_pass_env && redis_pass_env[0] != '\0') ? redis_pass_env : "";
  LogManager::getInstance().Info("GatewayService: EventSubscriber Redis -> " +
                                 sub_config.redis_host + ":" +
                                 std::to_string(sub_config.redis_port));

  event_subscriber_ =
      std::make_unique<PulseOne::Event::GatewayEventSubscriber>(sub_config);
}

GatewayService::~GatewayService() { stop(); }

bool GatewayService::start() {
  if (is_running_.load())
    return false;

  LogManager::getInstance().Info("GatewayService starting...");

  // 1. Load Registry — EdgeServer 설정 로드 후에 호출 (target_priorities 필터링
  // 적용) NOTE: setAllowedTargetIds()는 아래 EdgeServer config 파싱 블록에서
  // 호출됨

  // 2. Start Heartbeat
  heartbeat_service_->start();

  // 3. Register and Start Event Subscriber
  // [FIX] target_runner_ was removed from GatewayService header.
  // TargetRunner lives in GatewayContext — must use context_->getRunner().
  PulseOne::Schedule::ScheduledExporter::getInstance().setTargetRunner(
      &context_->getRunner());
  event_dispatcher_->registerHandlers(*event_subscriber_);

  // ✅ Instance specific command channel (v3.2.1)
  event_subscriber_->subscribeChannel("cmd:gateway:" +
                                      std::to_string(context_->getGatewayId()));

  // ✅ DB-driven Subscription Mode + target_priorities 기반 필터링
  std::string sub_mode = "selective"; // default
  try {
    auto repo = PulseOne::Database::RepositoryFactory::getInstance()
                    .getEdgeServerRepository();
    auto gateway_entity = repo->findById(context_->getGatewayId());

    if (gateway_entity) {
      // ✅ Populate tenant identity (v3.2.1)
      context_->setTenantId(gateway_entity->getTenantId());

      sub_mode = gateway_entity->getSubscriptionMode();
      LogManager::getInstance().Info("Gateway Subscription Mode: " + sub_mode);

      // [v3.2 FIX] edge_servers.config의 target_priorities로 허용 타겟 ID
      // 필터링 nlohmann::json 직접 파싱 방식으로 교체 (중첩 JSON 파싱 오류
      // 수정)
      try {
        const auto &cfg_json = gateway_entity->getConfig();
        nlohmann::json parsed_cfg;

        if (cfg_json.is_string()) {
          // DB에서 TEXT로 저장된 경우 — 문자열 안의 JSON 재파싱
          parsed_cfg = nlohmann::json::parse(cfg_json.get<std::string>());
        } else if (cfg_json.is_object()) {
          parsed_cfg = cfg_json;
        }

        if (parsed_cfg.contains("target_priorities") &&
            parsed_cfg["target_priorities"].is_object()) {
          std::map<int, int> priority_map;
          for (const auto &item : parsed_cfg["target_priorities"].items()) {
            try {
              int target_id = std::stoi(item.key());
              int priority_val = item.value().get<int>();
              priority_map[target_id] = priority_val;
            } catch (...) {
            }
          }
          if (!priority_map.empty()) {
            context_->getRegistry().setTargetPriorities(priority_map);
            LogManager::getInstance().Info(
                "GatewayService: target_priorities loaded (" +
                std::to_string(priority_map.size()) + " targets allowed)");
          }
        }
      } catch (...) {
        LogManager::getInstance().Warn(
            "Failed to parse target_priorities from edge_servers.config");
      }

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

  // [v3.2 FIX] setAllowedTargetIds() 호출 후 Registry 로드 (필터링 적용)
  if (!context_->getRegistry().loadFromDatabase()) {
    LogManager::getInstance().Error(
        "GatewayService: Failed to load target registry");
    return false;
  }

  // Update selective subscription only when NOT in "all" mode
  if (sub_mode != "all") {
    auto device_ids = context_->getRegistry().getAssignedDeviceIds();
    if (!device_ids.empty()) {
      event_subscriber_->updateSubscriptions(device_ids);
      LogManager::getInstance().Info("Selective subscriptions updated: " +
                                     std::to_string(device_ids.size()) +
                                     " devices");
    }
  }

  if (!event_subscriber_->start()) {
    LogManager::getInstance().Warn(
        "GatewayService: EventSubscriber start failed (Redis unavailable?). "
        "Gateway will continue and retry in background.");
    // Redis 없어도 프로세스는 유지 — 내부 재연결 루프가 계속 시도
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
