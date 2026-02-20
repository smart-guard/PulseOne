/**
 * @file EventSubscriber.cpp
 * @brief Generic Redis Pub/Sub Event Subscriber Implementation
 * @author PulseOne Development Team
 * @date 2026-02-17
 */

#include "Event/EventSubscriber.h"
#include "Client/RedisClientImpl.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <chrono>

namespace PulseOne {
namespace Event {

EventSubscriber::EventSubscriber(const EventSubscriberConfig &config)
    : config_(config) {}

EventSubscriber::~EventSubscriber() { stop(); }

bool EventSubscriber::start() {
  if (is_running_.load()) {
    return false;
  }

  if (!initializeRedisConnection()) {
    LogManager::getInstance().Error(
        "EventSubscriber - Redis connection failed");
    return false;
  }

  should_stop_ = false;
  is_running_ = true;

  // ✅ [Fix] Subscribe all pre-registered channels before starting loop
  subscribeAllChannels();

  subscribe_thread_ =
      std::make_unique<std::thread>(&EventSubscriber::subscribeLoop, this);

  for (int i = 0; i < config_.worker_thread_count; ++i) {
    worker_threads_.emplace_back(
        std::make_unique<std::thread>(&EventSubscriber::workerLoop, this, i));
  }

  if (config_.auto_reconnect) {
    reconnect_thread_ =
        std::make_unique<std::thread>(&EventSubscriber::reconnectLoop, this);
  }

  LogManager::getInstance().Info("EventSubscriber started with " +
                                 std::to_string(config_.worker_thread_count) +
                                 " workers");
  return true;
}

void EventSubscriber::stop() {
  if (!is_running_.load()) {
    return;
  }

  should_stop_ = true;
  is_connected_ = false;
  queue_cv_.notify_all();

  if (subscribe_thread_ && subscribe_thread_->joinable()) {
    subscribe_thread_->join();
  }

  for (auto &thread : worker_threads_) {
    if (thread && thread->joinable()) {
      thread->join();
    }
  }
  worker_threads_.clear();

  if (reconnect_thread_ && reconnect_thread_->joinable()) {
    reconnect_thread_->join();
  }

  is_running_ = false;
}

bool EventSubscriber::restart() {
  stop();
  return start();
}

void EventSubscriber::waitUntilStopped() {
  if (subscribe_thread_ && subscribe_thread_->joinable()) {
    subscribe_thread_->join();
  }
}

bool EventSubscriber::subscribeChannel(const std::string &channel) {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(),
                      channel);
  if (it == subscribed_channels_.end()) {
    subscribed_channels_.push_back(channel);
    if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
      return redis_client_->subscribe(channel);
    }
  }
  return true;
}

bool EventSubscriber::unsubscribeChannel(const std::string &channel) {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(),
                      channel);
  if (it != subscribed_channels_.end()) {
    subscribed_channels_.erase(it);
    if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
      return redis_client_->unsubscribe(channel);
    }
  }
  return true;
}

bool EventSubscriber::subscribePattern(const std::string &pattern) {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  auto it = std::find(subscribed_patterns_.begin(), subscribed_patterns_.end(),
                      pattern);
  if (it == subscribed_patterns_.end()) {
    subscribed_patterns_.push_back(pattern);
    if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
      return redis_client_->psubscribe(pattern);
    }
  }
  return true;
}

bool EventSubscriber::unsubscribePattern(const std::string &pattern) {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  auto it = std::find(subscribed_patterns_.begin(), subscribed_patterns_.end(),
                      pattern);
  if (it != subscribed_patterns_.end()) {
    subscribed_patterns_.erase(it);
    if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
      return redis_client_->punsubscribe(pattern);
    }
  }
  return true;
}

std::vector<std::string> EventSubscriber::getSubscribedChannels() const {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  return subscribed_channels_;
}

std::vector<std::string> EventSubscriber::getSubscribedPatterns() const {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  return subscribed_patterns_;
}

void EventSubscriber::registerHandler(const std::string &channel_pattern,
                                      std::shared_ptr<IEventHandler> handler) {
  std::lock_guard<std::mutex> lock(handler_mutex_);
  event_handlers_[channel_pattern] = handler;
}

void EventSubscriber::unregisterHandler(const std::string &channel_pattern) {
  std::lock_guard<std::mutex> lock(handler_mutex_);
  event_handlers_.erase(channel_pattern);
}

std::vector<std::string> EventSubscriber::getRegisteredHandlers() const {
  std::lock_guard<std::mutex> lock(handler_mutex_);
  std::vector<std::string> handlers;
  for (const auto &pair : event_handlers_) {
    handlers.push_back(pair.first);
  }
  return handlers;
}

SubscriptionStats EventSubscriber::getStats() const {
  SubscriptionStats stats;
  stats.total_received = total_received_.load();
  stats.total_processed = total_processed_.load();
  stats.total_failed = total_failed_.load();
  stats.max_queue_size_reached = max_queue_size_.load();
  stats.last_received_timestamp = last_received_timestamp_.load();
  stats.last_processed_timestamp = last_processed_timestamp_.load();

  std::lock_guard<std::mutex> lock(queue_mutex_);
  stats.queue_size = message_queue_.size();

  return stats;
}

void EventSubscriber::resetStats() {
  total_received_ = 0;
  total_processed_ = 0;
  total_failed_ = 0;
  max_queue_size_ = 0;
  last_received_timestamp_ = 0;
  last_processed_timestamp_ = 0;
}

void EventSubscriber::routeMessage(const std::string &channel,
                                   const std::string &message) {
  std::lock_guard<std::mutex> lock(handler_mutex_);
  for (const auto &pair : event_handlers_) {
    if (matchChannelPattern(pair.first, channel)) {
      enqueueMessage(channel, message);
      return;
    }
  }
}

bool EventSubscriber::matchChannelPattern(const std::string &pattern,
                                          const std::string &channel) const {
  if (pattern == "*")
    return true;
  size_t star_pos = pattern.find('*');
  if (star_pos == std::string::npos)
    return pattern == channel;
  std::string prefix = pattern.substr(0, star_pos);
  return channel.find(prefix) == 0;
}

bool EventSubscriber::enqueueMessage(const std::string &channel,
                                     const std::string &message) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (message_queue_.size() >= config_.max_queue_size)
    return false;

  QueuedMessage msg;
  msg.channel = channel;
  msg.payload = message;
  msg.received_time = std::chrono::steady_clock::now();
  message_queue_.push(msg);

  if (message_queue_.size() > max_queue_size_)
    max_queue_size_ = message_queue_.size();

  queue_cv_.notify_one();
  return true;
}

bool EventSubscriber::dequeueMessage(QueuedMessage &msg) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (message_queue_.empty())
    return false;
  msg = message_queue_.front();
  message_queue_.pop();
  return true;
}

void EventSubscriber::subscribeLoop() {
  while (!should_stop_.load()) {
    if (!is_connected_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    try {
      if (!subscribeAllChannels()) {
        is_connected_ = false;
        continue;
      }

      redis_client_->setSubscriberMode(true);
      redis_client_->setMessageCallback(
          [this](const std::string &ch, const std::string &msg) {
            total_received_++;
            last_received_timestamp_ =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            this->routeMessage(ch, msg);
          });

      while (!should_stop_.load() && is_connected_.load()) {
        if (!redis_client_->waitForMessage(100)) {
          if (!redis_client_->isConnected()) {
            is_connected_ = false;
            break;
          }
        }
      }
    } catch (...) {
      is_connected_ = false;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void EventSubscriber::workerLoop(int thread_index) {
  while (!should_stop_.load()) {
    QueuedMessage msg;
    if (!dequeueMessage(msg)) {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait_for(lock, std::chrono::milliseconds(100));
      continue;
    }

    try {
      bool handled = false;
      std::shared_ptr<IEventHandler> handler;
      {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        for (const auto &pair : event_handlers_) {
          if (matchChannelPattern(pair.first, msg.channel)) {
            handler = pair.second;
            break;
          }
        }
      }

      if (handler) {
        if (handler->handleEvent(msg.channel, msg.payload)) {
          total_processed_++;
          handled = true;
        }
      }

      if (!handled)
        total_failed_++;

      last_processed_timestamp_ =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();

    } catch (...) {
      total_failed_++;
    }
  }
}

void EventSubscriber::reconnectLoop() {
  while (!should_stop_.load()) {
    std::this_thread::sleep_for(
        std::chrono::seconds(config_.reconnect_interval_seconds));
    if (!should_stop_.load() && !is_connected_.load()) {
      if (initializeRedisConnection()) {
        is_connected_ = true;
      }
    }
  }
}

bool EventSubscriber::initializeRedisConnection() {
  try {
    redis_client_ = std::make_shared<RedisClientImpl>();
    if (!redis_client_->isConnected()) {
      redis_client_->connect(config_.redis_host, config_.redis_port,
                             config_.redis_password);
    }
    is_connected_ = redis_client_->isConnected();
    return is_connected_;
  } catch (...) {
    return false;
  }
}

bool EventSubscriber::subscribeAllChannels() {
  std::lock_guard<std::mutex> lock(channel_mutex_);
  for (const auto &ch : subscribed_channels_)
    redis_client_->subscribe(ch);
  for (const auto &p : subscribed_patterns_)
    redis_client_->psubscribe(p);
  return true;
}

} // namespace Event
} // namespace PulseOne
