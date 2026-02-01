#include "Utils/RedisManager.h"
#include "Client/RedisClientImpl.h"

namespace PulseOne {
namespace Utils {

// Initialize static members
std::unique_ptr<RedisManager> RedisManager::instance;
std::once_flag RedisManager::initInstanceFlag;

RedisManager &RedisManager::getInstance() {
  std::call_once(initInstanceFlag,
                 []() { instance.reset(new RedisManager()); });
  return *instance;
}

RedisManager::RedisManager() {
  // Initialize the RedisClient implementation
  client_ = std::make_shared<RedisClientImpl>();
}

std::shared_ptr<RedisClient> RedisManager::getClient() { return client_; }

} // namespace Utils
} // namespace PulseOne
