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
#include <climits>
#include <map>

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
      // 필터링 MinGW 크로스컴파일에서 nlohmann iterator 사용 시 wide_string
      // 에러 발생. 안전하게 문자열 파싱으로 처리.
      try {
        // getConfig()는 nlohmann::json& 반환
        // DB 저장 방식에 따라 json string vs json object 타입 분기
        const auto &cfg_json = gateway_entity->getConfig();
        std::string config_str;
        if (cfg_json.is_string()) {
          // DB에서 TEXT 컬럼으로 저장된 경우 — 문자열 안에 JSON이 있음
          config_str = cfg_json.get<std::string>();
        } else if (cfg_json.is_object()) {
          // 이미 파싱된 JSON 오브젝트인 경우
          config_str = cfg_json.dump();
        } else {
          config_str = "";
        }
        auto tp_pos = config_str.find("\"target_priorities\"");
        if (tp_pos != std::string::npos) {
          // "target_priorities":{"18":1,"19":2} → map<target_id,
          // priority_value>
          std::map<int, int> priority_map;
          auto brace_pos = config_str.find('{', tp_pos);
          auto end_pos = config_str.find('}', brace_pos);
          if (brace_pos != std::string::npos && end_pos != std::string::npos) {
            std::string tp_str =
                config_str.substr(brace_pos, end_pos - brace_pos);
            // "18":1,"19":2 파싱 — 큰따옴표 key와 뒤따르는 : 이후의 숫자 value
            std::size_t pos = 0;
            while ((pos = tp_str.find('"', pos)) != std::string::npos) {
              auto end_q = tp_str.find('"', pos + 1);
              if (end_q == std::string::npos)
                break;
              std::string key = tp_str.substr(pos + 1, end_q - pos - 1);
              // 콜론 이후 priority value 추출
              auto colon_pos = tp_str.find(':', end_q);
              int priority_val = INT_MAX;
              if (colon_pos != std::string::npos) {
                try {
                  priority_val = std::stoi(tp_str.substr(colon_pos + 1));
                } catch (...) {
                }
              }
              try {
                int id = std::stoi(key);
                priority_map[id] = priority_val;
              } catch (...) {
              }
              pos = end_q + 1;
            }
          }
          if (!priority_map.empty()) {
            context_->getRegistry().setTargetPriorities(priority_map);
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
