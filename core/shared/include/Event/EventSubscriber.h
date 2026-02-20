/**
 * @file EventSubscriber.h
 * @brief Generic Redis Pub/Sub Event Subscriber for PulseOne
 * @author PulseOne Development Team
 * @date 2026-02-17
 * @version 1.0.0
 *
 * This is a generic event subscriber refactored from export-gateway's
 * AlarmSubscriber. It handles Redis connections, channel/pattern subscriptions,
 * and routes messages to registered handlers in a multi-threaded worker pool.
 */

#ifndef PULSEONE_EVENT_SUBSCRIBER_H
#define PULSEONE_EVENT_SUBSCRIBER_H

#include "Client/RedisClient.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PulseOne {

using json = nlohmann::json;

namespace Event {

// =============================================================================
// Event Handler Interface
// =============================================================================

class IEventHandler {
public:
  virtual ~IEventHandler() = default;

  /**
   * @brief Handle an event received from Redis
   * @param channel The actual channel name
   * @param message The raw message payload (usually JSON)
   * @return true if handled successfully
   */
  virtual bool handleEvent(const std::string &channel,
                           const std::string &message) = 0;

  /**
   * @brief Handler name for logging
   */
  virtual std::string getName() const = 0;
};

// =============================================================================
// Configuration Structure
// =============================================================================

struct EventSubscriberConfig {
  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_password = "";

  std::vector<std::string> subscribe_channels;
  std::vector<std::string> subscribe_patterns;

  int worker_thread_count = 1;
  size_t max_queue_size = 10000;
  bool auto_reconnect = true;
  int reconnect_interval_seconds = 5;

  bool enable_debug_log = false;
};

// =============================================================================
// Statistics Structure
// =============================================================================

struct SubscriptionStats {
  size_t total_received = 0;
  size_t total_processed = 0;
  size_t total_failed = 0;
  size_t queue_size = 0;
  size_t max_queue_size_reached = 0;
  int64_t last_received_timestamp = 0;
  int64_t last_processed_timestamp = 0;

  json to_json() const {
    return json{{"total_received", total_received},
                {"total_processed", total_processed},
                {"total_failed", total_failed},
                {"queue_size", queue_size},
                {"max_queue_size_reached", max_queue_size_reached},
                {"last_received_timestamp", last_received_timestamp},
                {"last_processed_timestamp", last_processed_timestamp}};
  }
};

// =============================================================================
// EventSubscriber Class
// =============================================================================

class EventSubscriber {
public:
  explicit EventSubscriber(const EventSubscriberConfig &config);
  virtual ~EventSubscriber();

  // Disable copy/move
  EventSubscriber(const EventSubscriber &) = delete;
  EventSubscriber &operator=(const EventSubscriber &) = delete;

  virtual bool start();
  virtual void stop();
  virtual bool restart();

  bool isRunning() const { return is_running_.load(); }
  bool isConnected() const { return is_connected_.load(); }

  void waitUntilStopped();

  // Channel Management
  bool subscribeChannel(const std::string &channel);
  bool unsubscribeChannel(const std::string &channel);
  bool subscribePattern(const std::string &pattern);
  bool unsubscribePattern(const std::string &pattern);

  std::vector<std::string> getSubscribedChannels() const;
  std::vector<std::string> getSubscribedPatterns() const;

  // Handler Management
  void registerHandler(const std::string &channel_pattern,
                       std::shared_ptr<IEventHandler> handler);
  void unregisterHandler(const std::string &channel_pattern);
  std::vector<std::string> getRegisteredHandlers() const;

  SubscriptionStats getStats() const;
  void resetStats();

protected:
  virtual void routeMessage(const std::string &channel,
                            const std::string &message);
  bool matchChannelPattern(const std::string &pattern,
                           const std::string &channel) const;

  struct QueuedMessage {
    std::string channel;
    std::string payload;
    std::chrono::steady_clock::time_point received_time;
  };

  bool enqueueMessage(const std::string &channel, const std::string &message);
  bool dequeueMessage(QueuedMessage &msg);

  void subscribeLoop();
  void workerLoop(int thread_index);
  void reconnectLoop();

  bool initializeRedisConnection();
  bool subscribeAllChannels();

protected:
  EventSubscriberConfig config_;
  std::shared_ptr<PulseOne::RedisClient> redis_client_;

  std::unique_ptr<std::thread> subscribe_thread_;
  std::vector<std::unique_ptr<std::thread>> worker_threads_;
  std::unique_ptr<std::thread> reconnect_thread_;

  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_connected_{false};
  std::atomic<bool> should_stop_{false};

  std::queue<QueuedMessage> message_queue_;
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  std::vector<std::string> subscribed_channels_;
  std::vector<std::string> subscribed_patterns_;
  mutable std::mutex channel_mutex_;

  std::unordered_map<std::string, std::shared_ptr<IEventHandler>>
      event_handlers_;
  mutable std::mutex handler_mutex_;

  std::atomic<size_t> total_received_{0};
  std::atomic<size_t> total_processed_{0};
  std::atomic<size_t> total_failed_{0};
  std::atomic<size_t> max_queue_size_{0};
  std::atomic<int64_t> last_received_timestamp_{0};
  std::atomic<int64_t> last_processed_timestamp_{0};
};

} // namespace Event
} // namespace PulseOne

#endif // PULSEONE_EVENT_SUBSCRIBER_H
