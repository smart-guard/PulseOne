/**
 * @file HeartbeatService.h
 * @brief Heartbeat Service - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_HEARTBEAT_SERVICE_H
#define GATEWAY_SERVICE_HEARTBEAT_SERVICE_H

#include "Client/RedisClient.h"
#include <atomic>
#include <string>
#include <thread>

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief Service responsible for updating gateway heartbeats (DB and Redis)
 */
class HeartbeatService {
private:
  int gateway_id_{0};
  RedisClient *redis_client_{nullptr};

  std::thread thread_;
  std::atomic<bool> is_running_{false};
  int interval_seconds_{30};

public:
  HeartbeatService(int gateway_id, RedisClient *redis_client);
  ~HeartbeatService();

  bool start();
  void stop();
  void updateOnce();

private:
  void run();
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_HEARTBEAT_SERVICE_H
