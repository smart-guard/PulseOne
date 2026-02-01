#pragma once

#include "Client/RedisClient.h"
#include <memory>
#include <mutex>

namespace PulseOne {
namespace Utils {

/**
 * @brief RedisManager Singleton Shim
 * @details Provides backward compatibility for Application.cpp by wrapping
 * RedisClient
 */
class RedisManager {
public:
  static RedisManager &getInstance();

  // Prevent copying
  RedisManager(const RedisManager &) = delete;
  RedisManager &operator=(const RedisManager &) = delete;

  /**
   * @brief Get the underlying Redis client
   * @return std::shared_ptr<RedisClient>
   */
  std::shared_ptr<RedisClient> getClient();

  ~RedisManager() = default;

private:
  RedisManager();

  std::shared_ptr<RedisClient> client_;
  static std::once_flag initInstanceFlag;
  static std::unique_ptr<RedisManager> instance;
};

} // namespace Utils
} // namespace PulseOne
