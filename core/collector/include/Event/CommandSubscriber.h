/**
 * @file CommandSubscriber.h
 * @brief Collector-specific Command Subscriber for C2
 * @author PulseOne Development Team
 * @date 2026-02-17
 */

#ifndef COLLECTOR_COMMAND_SUBSCRIBER_H
#define COLLECTOR_COMMAND_SUBSCRIBER_H

#include "Client/RedisClientImpl.h"
#include "Event/EventSubscriber.h" // Shared base class
#include <functional>
#include <memory>
#include <string>

namespace PulseOne {
namespace Core {

/**
 * @brief Command Subscriber for the Collector
 *
 * Subscribes to broadcast commands (config:reload) and specific commands
 * for this collector instance (cmd:collector:{id}).
 */
class CommandSubscriber : public PulseOne::Event::EventSubscriber {
public:
  explicit CommandSubscriber(
      const PulseOne::Event::EventSubscriberConfig &config);
  virtual ~CommandSubscriber() = default;

  /**
   * @brief Initialize and start the subscriber
   */
  bool start() override;

protected:
  /**
   * @brief Handle raw messages and route them to specific logic
   */
  void routeMessage(const std::string &channel,
                    const std::string &message) override;

  /**
   * @brief Handle configuration reload command
   */
  void handleConfigReload(const std::string &message);

  /**
   * @brief Handle collector-specific command
   */
  void handleCollectorCommand(const std::string &channel,
                              const std::string &message);

private:
  int collector_id_;
  std::shared_ptr<PulseOne::RedisClient>
      publisher_client_; ///< publish 전용 (subscriber 모드 분리)
};

} // namespace Core
} // namespace PulseOne

#endif // COLLECTOR_COMMAND_SUBSCRIBER_H
