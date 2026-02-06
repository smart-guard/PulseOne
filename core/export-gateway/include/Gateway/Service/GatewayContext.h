/**
 * @file GatewayContext.h
 * @brief Gateway Context - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_GATEWAY_CONTEXT_H
#define GATEWAY_SERVICE_GATEWAY_CONTEXT_H

#include "Client/RedisClient.h"
#include "Gateway/Service/ITargetRegistry.h"
#include "Gateway/Service/ITargetRunner.h"
#include <memory>
#include <mutex>

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief Context for the Export Gateway service
 */
class GatewayContext {
private:
  int gateway_id_{0};
  std::unique_ptr<RedisClient> redis_client_;
  std::unique_ptr<ITargetRegistry> registry_;
  std::unique_ptr<ITargetRunner> runner_;

public:
  GatewayContext(int gateway_id, std::unique_ptr<RedisClient> redis_client,
                 std::unique_ptr<ITargetRegistry> registry,
                 std::unique_ptr<ITargetRunner> runner)
      : gateway_id_(gateway_id), redis_client_(std::move(redis_client)),
        registry_(std::move(registry)), runner_(std::move(runner)) {}

  int getGatewayId() const { return gateway_id_; }
  RedisClient *getRedisClient() const { return redis_client_.get(); }
  ITargetRegistry &getRegistry() const { return *registry_; }
  ITargetRunner &getRunner() const { return *runner_; }
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_GATEWAY_CONTEXT_H
