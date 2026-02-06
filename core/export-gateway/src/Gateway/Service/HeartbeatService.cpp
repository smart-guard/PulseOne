/**
 * @file HeartbeatService.cpp
 * @brief Heartbeat Service Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/HeartbeatService.h"
#include "Constants/ExportConstants.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <chrono>

namespace PulseOne {
namespace Gateway {
namespace Service {

HeartbeatService::HeartbeatService(int gateway_id, RedisClient *redis_client)
    : gateway_id_(gateway_id), redis_client_(redis_client) {}

HeartbeatService::~HeartbeatService() { stop(); }

bool HeartbeatService::start() {
  if (is_running_.load())
    return false;

  is_running_ = true;
  thread_ = std::thread(&HeartbeatService::run, this);
  return true;
}

void HeartbeatService::stop() {
  if (!is_running_.load())
    return;

  is_running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void HeartbeatService::updateOnce() {
  if (gateway_id_ <= 0)
    return;

  try {
    auto &db_manager = DbLib::DatabaseManager::getInstance();
    std::string query =
        "UPDATE edge_servers SET last_seen = CURRENT_TIMESTAMP, status = "
        "'active' WHERE id = " +
        std::to_string(gateway_id_);

    db_manager.executeNonQuery(query);

    // Redis 하트비트 업데이트
    if (redis_client_ && redis_client_->isConnected()) {
      nlohmann::json status_json;
      status_json["status"] = PulseOne::Constants::Export::Redis::STATUS_ONLINE;
      status_json[PulseOne::Constants::Export::Redis::KEY_LAST_SEEN] =
          std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::now());
      status_json["gatewayId"] = gateway_id_;
      status_json["hostname"] = "docker-container";

      redis_client_->setex(
          PulseOne::Constants::Export::Redis::KEY_GATEWAY_STATUS_PREFIX +
              std::to_string(gateway_id_),
          status_json.dump(), interval_seconds_ * 3);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Warn("HeartbeatService: Update failed: " +
                                   std::string(e.what()));
  }
}

void HeartbeatService::run() {
  LogManager::getInstance().Info("HeartbeatService started for Gateway: " +
                                 std::to_string(gateway_id_));

  while (is_running_) {
    updateOnce();

    // Sleep with interrupt check
    for (int i = 0; i < interval_seconds_ && is_running_; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  LogManager::getInstance().Info("HeartbeatService stopped");
}

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
