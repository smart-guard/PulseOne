/**
 * @file GatewayService.h
 * @brief Main Gateway Service - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_GATEWAY_SERVICE_H
#define GATEWAY_SERVICE_GATEWAY_SERVICE_H

#include "Event/GatewayEventSubscriber.h"
#include "Gateway/Service/EventDispatcher.h"
#include "Gateway/Service/GatewayContext.h"
#include "Gateway/Service/HeartbeatService.h"
#include "Gateway/Service/TargetRunner.h"
#include <atomic>
#include <memory>

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief High-level service orchestrating the Export Gateway
 */
class GatewayService {
private:
  std::unique_ptr<GatewayContext> context_;
  std::unique_ptr<HeartbeatService> heartbeat_service_;
  std::unique_ptr<EventDispatcher> event_dispatcher_;
  std::unique_ptr<PulseOne::Event::GatewayEventSubscriber> event_subscriber_;

  std::atomic<bool> is_running_{false};

public:
  explicit GatewayService(std::unique_ptr<GatewayContext> context);
  ~GatewayService();

  bool start();
  void stop();
  bool isRunning() const { return is_running_.load(); }

  GatewayContext &getContext() { return *context_; }
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_GATEWAY_SERVICE_H
