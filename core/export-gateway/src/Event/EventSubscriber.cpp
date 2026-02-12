/**
 * @file EventSubscriber.cpp
 * @brief Redis Pub/Sub 통합 이벤트 구독자 구현 (v3.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 3.0.0
 *
 * 주요 변경사항:
 * ✅ 이름 변경: AlarmSubscriber → EventSubscriber
 * ✅ routeMessage() 채널 라우팅 추가
 * ✅ handleScheduleEvent() 스케줄 이벤트 처리
 * ✅ handleSystemEvent() 시스템 이벤트 처리
 * ✅ 기존 알람 처리 로직 100% 유지
 */

#include "Event/EventSubscriber.h"
#include "CSP/DynamicTargetManager.h"
#include "Client/RedisClientImpl.h"
#include "Export/ExportTypes.h"
#include "Export/GatewayExportTypes.h"
#include "Logging/LogManager.h"
#include "Schedule/ScheduledExporter.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

namespace PulseOne {
namespace Event {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

EventSubscriber::EventSubscriber(const EventSubscriberConfig &config)
    : config_(config) {

  // noise logs removed
}

EventSubscriber::~EventSubscriber() {
  stop();
  LogManager::getInstance().Info("EventSubscriber 종료 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool EventSubscriber::start() {
  if (is_running_.load()) {
    LogManager::getInstance().Warn("EventSubscriber가 이미 실행 중입니다");
    return false;
  }

  LogManager::getInstance().Info("EventSubscriber 시작 중...");

  // 1. DynamicTargetManager 확인
  try {
    auto &manager = PulseOne::CSP::DynamicTargetManager::getInstance();
    if (!manager.isRunning()) {
      LogManager::getInstance().Error("DynamicTargetManager가 실행되지 않음");
      return false;
    }
    LogManager::getInstance().Info("✅ DynamicTargetManager 연결 확인");
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("DynamicTargetManager 접근 실패: " +
                                    std::string(e.what()));
    return false;
  }

  // 2. Redis 연결
  if (!initializeRedisConnection()) {
    LogManager::getInstance().Error("Redis 연결 초기화 실패");
    return false;
  }

  // 3. 구독 채널 설정
  {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    for (const auto &channel : config_.subscribe_channels) {
      if (std::find(subscribed_channels_.begin(), subscribed_channels_.end(),
                    channel) == subscribed_channels_.end()) {
        subscribed_channels_.push_back(channel);
      }
    }

    if (subscribed_channels_.empty()) {
      subscribed_channels_.push_back("alarms:all");
      LogManager::getInstance().Info(
          "구독 채널이 비어 있어 기본값(alarms:all) 추가");
    }
    for (const auto &pattern : config_.subscribe_patterns) {
      if (std::find(subscribed_patterns_.begin(), subscribed_patterns_.end(),
                    pattern) == subscribed_patterns_.end()) {
        subscribed_patterns_.push_back(pattern);
      }
    }
  }

  // 4. 스레드 시작
  should_stop_ = false;
  is_running_ = true;

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

  LogManager::getInstance().Info("EventSubscriber 시작 완료 ✅");
  return true;
}

void EventSubscriber::stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().Info("EventSubscriber 중지 중...");

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
  LogManager::getInstance().Info("EventSubscriber 중지 완료");
}

bool EventSubscriber::restart() {
  stop();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return start();
}

void EventSubscriber::waitUntilStopped() {
  if (subscribe_thread_ && subscribe_thread_->joinable()) {
    subscribe_thread_->join();
  }
}

// =============================================================================
// 이벤트 핸들러 등록
// =============================================================================

void EventSubscriber::registerHandler(const std::string &channel_pattern,
                                      std::shared_ptr<IEventHandler> handler) {
  std::lock_guard<std::mutex> lock(handler_mutex_);

  event_handlers_[channel_pattern] = handler;

  LogManager::getInstance().Info("핸들러 등록: " + channel_pattern + " → " +
                                 handler->getName());
}

void EventSubscriber::unregisterHandler(const std::string &channel_pattern) {
  std::lock_guard<std::mutex> lock(handler_mutex_);

  auto it = event_handlers_.find(channel_pattern);
  if (it != event_handlers_.end()) {
    LogManager::getInstance().Info("핸들러 제거: " + channel_pattern);
    event_handlers_.erase(it);
  }
}

void EventSubscriber::updateSubscriptions(
    const std::set<std::string> &device_ids) {
  LogManager::getInstance().Info("구독 채널 대량 업데이트 시작 (디바이스 수: " +
                                 std::to_string(device_ids.size()) + ")");

  std::vector<std::string> current_channels;
  {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    current_channels = subscribed_channels_;
  }

  // 1. 기존 디바이스 채널 중 더 이상 필요 없는 것 제거
  for (const auto &channel : current_channels) {
    if (channel.find("device:") == 0 &&
        channel.find(":alarms") != std::string::npos) {
      std::string id = channel.substr(7, channel.find(":alarms") - 7);
      if (device_ids.find(id) == device_ids.end()) {
        LogManager::getInstance().Info("더 이상 필요 없는 채널 구독 해제: " +
                                       channel);
        unsubscribeChannel(channel);
      }
    }
  }

  // 2. 새로운 디바이스 채널 추가
  for (const auto &id : device_ids) {
    if (id.empty())
      continue;
    std::string channel = "device:" + id + ":alarms";
    bool already_subscribed = false;
    {
      std::lock_guard<std::mutex> lock(channel_mutex_);
      already_subscribed =
          std::find(subscribed_channels_.begin(), subscribed_channels_.end(),
                    channel) != subscribed_channels_.end();
    }

    if (!already_subscribed) {
      LogManager::getInstance().Info("새로운 디바이스 채널 구독: " + channel);
      subscribeChannel(channel);
    }
  }

  LogManager::getInstance().Info("✅ 구독 채널 업데이트 완료");
}

std::vector<std::string> EventSubscriber::getRegisteredHandlers() const {
  std::lock_guard<std::mutex> lock(handler_mutex_);

  std::vector<std::string> handlers;
  handlers.reserve(event_handlers_.size());

  for (const auto &pair : event_handlers_) {
    handlers.push_back(pair.first + " → " + pair.second->getName());
  }

  return handlers;
}

// =============================================================================
// 메시지 라우팅
// =============================================================================

void EventSubscriber::routeMessage(const std::string &channel,
                                   const std::string &message) {
  LogManager::getInstance().Info("[TRACE-0-REDIS-RECEIVE] Channel: " + channel +
                                 " | Msg: " + message);
  // 1. 커스텀 핸들러 검색 (우선순위 부여)
  {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    for (const auto &pair : event_handlers_) {
      if (matchChannelPattern(pair.first, channel)) {
        try {
          if (pair.second->handleEvent(channel, message)) {
            LogManager::getInstance().Debug("커스텀 핸들러 처리 성공: " +
                                            pair.second->getName());
            return; // 처리 완료 시 반환
          }
        } catch (const std::exception &e) {
          LogManager::getInstance().Error("커스텀 핸들러 예외: " +
                                          std::string(e.what()));
        }
      }
    }
  }

  // 2. 기본 내장 핸들러 (커스텀 핸들러가 없거나 실패한 경우)
  if (channel.find("alarms:") == 0 || channel.find("alarm:") == 0 ||
      channel.find("device:") == 0) {
    handleAlarmEvent(channel, message);
  } else if (channel.find("schedule:") == 0) {
    handleScheduleEvent(channel, message);
  } else if (channel.find("cmd:gateway:") == 0) {
    handleManualExportEvent(channel, message);
  } else {
    LogManager::getInstance().Debug("처리되지 않은 채널: " + channel);
  }
}

void EventSubscriber::handleManualExportEvent(const std::string &channel,
                                              const std::string &message) {
  try {
    LogManager::getInstance().Info("수동 전송 명령 수신: " + channel);

    // CommandEventHandler가 이미 cmd:* 패턴을 통해
    // Coordinator::handleCommandEvent를 호출함. 하지만 EventSubscriber 수준에서
    // 추가 로깅이나 Gateway ID 검증이 필요할 수 있음. 여기서는 기본적으로
    // 파싱해서 유효성만 체크하고, AlarmMessage로 변환 가능한 경우
    // processAlarm으로 넘기거나 별도 핸들러를 호출할 수 있음.

    // Coordinator에서 handleCommandEvent를 통해 처리하도록 위임됨.
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Manual Export 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }
}

bool EventSubscriber::matchChannelPattern(const std::string &pattern,
                                          const std::string &channel) const {
  // 간단한 와일드카드 매칭 (*만 지원)
  if (pattern == "*") {
    return true;
  }

  size_t star_pos = pattern.find('*');
  if (star_pos == std::string::npos) {
    // 와일드카드 없음 - 정확히 일치
    return pattern == channel;
  }

  // 앞부분 매칭 (예: "schedule:*")
  std::string prefix = pattern.substr(0, star_pos);
  return channel.find(prefix) == 0;
}

// =============================================================================
// 기본 핸들러들
// =============================================================================

void EventSubscriber::handleAlarmEvent(const std::string & /*channel*/,
                                       const std::string &message) {
  // 기존 알람 처리 로직 호출
  try {
    auto alarm = parseAlarmMessage(message);

    // 큐에 추가 (기존 방식)
    if (!enqueueAlarm(alarm)) {
      LogManager::getInstance().Warn("알람 큐 가득참 - 메시지 드롭");
      total_failed_.fetch_add(1);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("알람 파싱 실패: " + std::string(e.what()));
    total_failed_.fetch_add(1);
  }
}

void EventSubscriber::handleScheduleEvent(const std::string &channel,
                                          const std::string & /*message*/) {
  LogManager::getInstance().Info("🔄 스케줄 이벤트 수신: " + channel);

  try {
    // 채널 파싱: schedule:reload, schedule:execute:123
    if (channel == "schedule:reload") {
      LogManager::getInstance().Info("🔄 스케줄 리로드 이벤트");

      // ScheduledExporter 싱글턴 접근 후 reloadSchedules() 호출
      auto &exporter = ::PulseOne::Schedule::ScheduledExporter::getInstance();
      int loaded = exporter.reloadSchedules();
      LogManager::getInstance().Info(
          "스케줄 리로드 완료: " + std::to_string(loaded) + "개");

    } else if (channel.find("schedule:execute:") == 0) {
      // 특정 스케줄 실행
      std::string id_str = channel.substr(17); // "schedule:execute:" 이후
      int schedule_id = std::stoi(id_str);

      LogManager::getInstance().Info("⚡ 스케줄 실행 이벤트: ID=" +
                                     std::to_string(schedule_id));

      // ScheduledExporter.executeSchedule(schedule_id) 호출
      auto &exporter = ::PulseOne::Schedule::ScheduledExporter::getInstance();
      auto result = exporter.executeSchedule(schedule_id);

    } else if (channel.find("schedule:stop:") == 0) {
      // 특정 스케줄 중지
      std::string id_str = channel.substr(14); // "schedule:stop:" 이후
      int schedule_id = std::stoi(id_str);

      LogManager::getInstance().Info("⏸️ 스케줄 중지 이벤트: ID=" +
                                     std::to_string(schedule_id));

      // TODO: 스케줄 중지 로직
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("스케줄 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }
}

void EventSubscriber::handleSystemEvent(const std::string &channel,
                                        const std::string &message) {
  LogManager::getInstance().Info("⚙️ 시스템 이벤트 수신: " + channel);

  try {
    if (channel == "system:shutdown") {
      LogManager::getInstance().Info("🛑 시스템 종료 이벤트 수신");
      // 실제 종료는 main의 g_shutdown_requested 등을 통해 처리해야 하므로
      // 로그만 남김
    } else if (channel == "system:restart") {
      LogManager::getInstance().Info("🔄 시스템 재시작 이벤트 수신");
    } else if (channel == "system:reload_config") {
      LogManager::getInstance().Info("⚙️ 설정 리로드 이벤트 수신");
    } else {
      LogManager::getInstance().Info("❓ 정의되지 않은 시스템 이벤트: " +
                                     channel + " (Payload: " + message + ")");
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("시스템 이벤트 처리 실패: " +
                                    std::string(e.what()));
  }
}

// =============================================================================
// 백그라운드 스레드 루프
// =============================================================================

void EventSubscriber::subscribeLoop() {
  LogManager::getInstance().Info("구독 루프 시작");

  while (!should_stop_.load()) {
    if (!is_connected_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    try {
      // Redis에 채널 구독
      if (!subscribeAllChannels()) {
        is_connected_ = false;
        continue;
      }

      // 메시지 콜백에서 routeMessage() 호출
      auto message_callback = [this](const std::string &channel,
                                     const std::string &message) {
        total_received_.fetch_add(1);
        last_received_timestamp_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        // ✅ 채널별 라우팅
        this->routeMessage(channel, message);
      };

      // 구독 모드 활성화 (Watchdog 간섭 방지)
      redis_client_->setSubscriberMode(true);

      redis_client_->setMessageCallback(message_callback);

      LogManager::getInstance().Info("Redis Pub/Sub 구독 시작됨");

      // 메시지 수신 대기 루프
      while (!should_stop_.load() && is_connected_.load()) {
        if (!redis_client_->waitForMessage(100)) {
          if (redis_client_ && !redis_client_->isConnected()) {
            is_connected_ = false;
            LogManager::getInstance().Warn("Redis 연결 끊김");
            break;
          }
          continue;
        }
      }

    } catch (const std::exception &e) {
      if (redis_client_)
        redis_client_->setSubscriberMode(false);
      is_connected_ = false;
      LogManager::getInstance().Error("구독 루프 에러: " +
                                      std::string(e.what()));
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  LogManager::getInstance().Info("구독 루프 종료");
}

void EventSubscriber::workerLoop(int thread_index) {
  LogManager::getInstance().Info("워커 스레드 [" +
                                 std::to_string(thread_index) + "] 시작");

  while (!should_stop_.load()) {
    PulseOne::CSP::AlarmMessage alarm;

    // 큐에서 가져오기
    if (!dequeueAlarm(alarm)) {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return should_stop_.load() || !alarm_queue_.empty();
      });
      continue;
    }

    try {

      // 알람 처리 및 타겟 전송
      processAlarm(alarm);

      total_processed_.fetch_add(1);

      last_processed_timestamp_ =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("워커 [" + std::to_string(thread_index) +
                                      "] 에러: " + std::string(e.what()));
      total_failed_.fetch_add(1);
    }
  }

  LogManager::getInstance().Info("워커 스레드 [" +
                                 std::to_string(thread_index) + "] 종료");
}

void EventSubscriber::reconnectLoop() {
  LogManager::getInstance().Info("재연결 루프 시작");

  while (!should_stop_.load()) {
    std::this_thread::sleep_for(
        std::chrono::seconds(config_.reconnect_interval_seconds));

    if (!should_stop_.load() && !is_connected_.load()) {
      LogManager::getInstance().Info("Redis 재연결 시도 중...");

      if (initializeRedisConnection()) {
        is_connected_ = true;
        LogManager::getInstance().Info("Redis 재연결 성공");
      }
    }
  }

  LogManager::getInstance().Info("재연결 루프 종료");
}

// =============================================================================
// 메시지 큐 관리
// =============================================================================

bool EventSubscriber::enqueueMessage(const std::string &channel,
                                     const std::string &message) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (message_queue_.size() >= config_.max_queue_size) {
    return false;
  }

  QueuedMessage msg;
  msg.channel = channel;
  msg.payload = message;
  msg.received_time = std::chrono::steady_clock::now();

  message_queue_.push(msg);

  size_t current_size = message_queue_.size();
  size_t max_size = max_queue_size_.load();
  if (current_size > max_size) {
    max_queue_size_ = current_size;
  }

  queue_cv_.notify_one();
  return true;
}

bool EventSubscriber::dequeueMessage(QueuedMessage &msg) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (message_queue_.empty()) {
    return false;
  }

  msg = message_queue_.front();
  message_queue_.pop();
  return true;
}

bool EventSubscriber::enqueueAlarm(const PulseOne::CSP::AlarmMessage &alarm) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (alarm_queue_.size() >= config_.max_queue_size) {
    return false;
  }

  alarm_queue_.push(alarm);

  size_t current_size = alarm_queue_.size();
  size_t max_size = max_queue_size_.load();
  if (current_size > max_size) {
    max_queue_size_ = current_size;
  }

  queue_cv_.notify_one();
  return true;
}

bool EventSubscriber::dequeueAlarm(PulseOne::CSP::AlarmMessage &alarm) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (alarm_queue_.empty()) {
    return false;
  }

  alarm = alarm_queue_.front();
  alarm_queue_.pop();
  return true;
}

// =============================================================================
// 알람 처리 (기존 로직 100% 유지)
// =============================================================================

PulseOne::CSP::AlarmMessage
EventSubscriber::parseAlarmMessage(const std::string &json_str) {
  PulseOne::CSP::AlarmMessage alarm;

  try {
    auto j = json::parse(json_str);

    alarm.from_json(j);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("JSON 파싱 실패: " + std::string(e.what()));
  }

  return alarm;
}

void EventSubscriber::processAlarm(const PulseOne::CSP::AlarmMessage &alarm) {
  if (!filterAlarm(alarm)) {
    return;
  }

  // ✅ 콜백이 등록되어 있으면 콜백 호출 (ExportCoordinator 연동)
  if (alarm_callback_) {
    alarm_callback_(alarm);
    return;
  }

  // 기본 처리 (Fallback)
  auto &manager = PulseOne::CSP::DynamicTargetManager::getInstance();
  std::vector<PulseOne::Export::TargetSendResult> results =
      manager.sendAlarmToTargets(alarm);

  int success_count = 0;
  for (const auto &result : results) {
    if (result.success) {
      success_count++;
    }
  }

  LogManager::getInstance().Info("알람 처리 완료: " + alarm.point_name + " (" +
                                 std::to_string(success_count) + "/" +
                                 std::to_string(results.size()) + " 타겟)");
}

bool EventSubscriber::filterAlarm(
    const PulseOne::CSP::AlarmMessage &alarm) const {
  if (alarm.point_name.empty()) {
    return false;
  }

  return true;
}

// =============================================================================
// Redis 연결 관리
// =============================================================================

bool EventSubscriber::initializeRedisConnection() {
  try {
    LogManager::getInstance().Info("Redis 연결 초기화 중...");

    redis_client_ = std::make_shared<RedisClientImpl>();

    if (!redis_client_->isConnected()) {
      LogManager::getInstance().Error("Redis 자동 연결 실패");
      is_connected_ = false; // ✅ 명시적 설정
      return false;
    }

    is_connected_ = true;
    LogManager::getInstance().Info("Redis 연결 성공");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Redis 연결 실패: " +
                                    std::string(e.what()));
    is_connected_ = false; // ✅ 명시적 설정
    return false;
  }
}

bool EventSubscriber::subscribeAllChannels() {
  bool success = true;

  try {
    std::lock_guard<std::mutex> lock(channel_mutex_);

    for (const auto &channel : subscribed_channels_) {
      if (!redis_client_->subscribe(channel)) {
        LogManager::getInstance().Error("채널 구독 실패: " + channel);
        success = false;
      }
    }

    for (const auto &pattern : subscribed_patterns_) {
      if (!redis_client_->psubscribe(pattern)) {
        LogManager::getInstance().Error("패턴 구독 실패: " + pattern);
        success = false;
      }
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("구독 실패: " + std::string(e.what()));
    return false;
  }

  return success;
}

// =============================================================================
// 채널 관리
// =============================================================================

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

// =============================================================================
// 통계 정보
// =============================================================================

SubscriptionStats EventSubscriber::getStats() const {
  SubscriptionStats stats;

  stats.total_received = total_received_.load();
  stats.total_processed = total_processed_.load();
  stats.total_failed = total_failed_.load();

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stats.queue_size = alarm_queue_.size();
  }

  stats.max_queue_size_reached = max_queue_size_.load();
  stats.last_received_timestamp = last_received_timestamp_.load();
  stats.last_processed_timestamp = last_processed_timestamp_.load();

  return stats;
}

void EventSubscriber::resetStats() {
  total_received_ = 0;
  total_processed_ = 0;
  total_failed_ = 0;
  max_queue_size_ = 0;
  last_received_timestamp_ = 0;
  last_processed_timestamp_ = 0;

  LogManager::getInstance().Info("통계 초기화 완료");
}

json EventSubscriber::getDetailedStats() const {
  auto stats = getStats();

  json j;
  j["total_received"] = stats.total_received;
  j["total_processed"] = stats.total_processed;
  j["total_failed"] = stats.total_failed;
  j["queue_size"] = stats.queue_size;
  j["max_queue_size_reached"] = stats.max_queue_size_reached;
  j["is_running"] = is_running_.load();
  j["is_connected"] = is_connected_.load();
  j["worker_threads"] = config_.worker_thread_count;

  {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    j["subscribed_channels"] = subscribed_channels_;
    j["subscribed_patterns"] = subscribed_patterns_;
  }

  {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    j["registered_handlers"] = event_handlers_.size();
  }

  return j;
}

} // namespace Event
} // namespace PulseOne