/**
 * @file EventSubscriber.h
 * @brief Redis Pub/Sub 통합 이벤트 구독자 (v3.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 3.0.0
 *
 * 변경사항 (AlarmSubscriber v2.0 → EventSubscriber v3.0):
 * ✅ 이름 변경: AlarmSubscriber → EventSubscriber (명확한 네이밍)
 * ✅ 채널별 이벤트 라우팅 추가
 * ✅ EventHandler 인터페이스 추가
 * ✅ 알람, 스케줄, 시스템, 커스텀 이벤트 지원
 * ✅ 기존 알람 처리 로직 100% 유지 (하위 호환)
 *
 * 지원 채널:
 * - alarms:*    → 알람 이벤트 (기존 로직)
 * - schedule:*  → 스케줄 이벤트 (리로드, 실행 등)
 * - system:*    → 시스템 이벤트
 * - custom:*    → 커스텀 이벤트
 *
 * 저장 위치: core/export-gateway/include/Event/EventSubscriber.h
 */

#ifndef EVENT_SUBSCRIBER_H
#define EVENT_SUBSCRIBER_H

#include "CSP/AlarmMessage.h"
#include "Client/RedisClient.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace PulseOne {
namespace Event {

// =============================================================================
// 이벤트 핸들러 인터페이스
// =============================================================================

/**
 * @brief 이벤트 핸들러 인터페이스
 *
 * 각 채널 패턴에 대해 커스텀 핸들러를 등록할 수 있습니다.
 */
class IEventHandler {
public:
  virtual ~IEventHandler() = default;

  /**
   * @brief 이벤트 처리
   * @param channel Redis 채널 이름
   * @param message 메시지 내용 (JSON 문자열)
   * @return 처리 성공 여부
   */
  virtual bool handleEvent(const std::string &channel,
                           const std::string &message) = 0;

  /**
   * @brief 핸들러 이름 (로깅용)
   */
  virtual std::string getName() const = 0;
};

// =============================================================================
// 설정 구조체
// =============================================================================

struct EventSubscriberConfig {
  // Redis 연결 정보
  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_password = "";

  // 구독 채널 설정
  std::vector<std::string> subscribe_channels = {"alarms:all"};
  std::vector<std::string> subscribe_patterns;

  // 처리 옵션
  int worker_thread_count = 1;
  size_t max_queue_size = 10000;
  bool auto_reconnect = true;
  int reconnect_interval_seconds = 5;

  // 필터링
  bool filter_by_severity = false;
  std::vector<std::string> allowed_severities;

  // 전송 옵션
  bool use_parallel_send = false;
  int max_priority_filter = 1000;

  // 로깅
  bool enable_debug_log = false;
};

// =============================================================================
// 구독 통계 구조체
// =============================================================================

struct SubscriptionStats {
  size_t total_received = 0;
  size_t total_processed = 0;
  size_t total_failed = 0;
  size_t queue_size = 0;
  size_t max_queue_size_reached = 0;
  int64_t last_received_timestamp = 0;
  int64_t last_processed_timestamp = 0;
  double avg_processing_time_ms = 0.0;

  json to_json() const {
    return json{{"total_received", total_received},
                {"total_processed", total_processed},
                {"total_failed", total_failed},
                {"queue_size", queue_size},
                {"max_queue_size_reached", max_queue_size_reached},
                {"last_received_timestamp", last_received_timestamp},
                {"last_processed_timestamp", last_processed_timestamp},
                {"avg_processing_time_ms", avg_processing_time_ms}};
  }
};

// =============================================================================
// EventSubscriber 클래스
// =============================================================================

class EventSubscriber {
public:
  // =========================================================================
  // 생성자 및 소멸자
  // =========================================================================

  explicit EventSubscriber(const EventSubscriberConfig &config);
  ~EventSubscriber();

  // 복사/이동 방지
  EventSubscriber(const EventSubscriber &) = delete;
  EventSubscriber &operator=(const EventSubscriber &) = delete;
  EventSubscriber(EventSubscriber &&) = delete;
  EventSubscriber &operator=(EventSubscriber &&) = delete;

  // =========================================================================
  // 라이프사이클 관리
  // =========================================================================

  bool start();
  void stop();
  bool restart();

  bool isRunning() const { return is_running_.load(); }
  bool isConnected() const { return is_connected_.load(); }

  // =========================================================================
  // 알람 처리 콜백 (NEW!)
  // =========================================================================
  using AlarmCallback =
      std::function<void(const PulseOne::CSP::AlarmMessage &)>;
  void setAlarmCallback(AlarmCallback callback) { alarm_callback_ = callback; }

  void waitUntilStopped();

  // =========================================================================
  // 채널 관리
  // =========================================================================

  bool subscribeChannel(const std::string &channel);
  bool unsubscribeChannel(const std::string &channel);
  bool subscribePattern(const std::string &pattern);
  bool unsubscribePattern(const std::string &pattern);

  std::vector<std::string> getSubscribedChannels() const;
  std::vector<std::string> getSubscribedPatterns() const;

  // =========================================================================
  // 이벤트 핸들러 등록
  // =========================================================================

  /**
   * @brief 채널 패턴에 대한 커스텀 핸들러 등록
   *
   * @param channel_pattern 채널 패턴 (예: "schedule:*", "system:*")
   * @param handler 핸들러 인스턴스
   *
   * @example
   * ```cpp
   * class MyHandler : public IEventHandler {
   *     bool handleEvent(const std::string& channel, const std::string&
   * message) override {
   *         // 처리 로직
   *         return true;
   *     }
   *     std::string getName() const override { return "MyHandler"; }
   * };
   *
   * subscriber.registerHandler("schedule:*", std::make_shared<MyHandler>());
   * ```
   */
  void registerHandler(const std::string &channel_pattern,
                       std::shared_ptr<IEventHandler> handler);

  /**
   * @brief 핸들러 제거
   */
  void unregisterHandler(const std::string &channel_pattern);

  /**
   * @brief 등록된 핸들러 목록 조회
   */
  std::vector<std::string> getRegisteredHandlers() const;

  // =========================================================================
  // 통계 정보
  // =========================================================================

  SubscriptionStats getStats() const;
  void resetStats();
  json getDetailedStats() const;

private:
  // =========================================================================
  // 메시지 라우팅
  // =========================================================================

  /**
   * @brief 채널에 따라 적절한 핸들러로 라우팅
   *
   * @param channel Redis 채널 이름
   * @param message 메시지 내용
   */
  void routeMessage(const std::string &channel, const std::string &message);

  /**
   * @brief 채널 패턴 매칭
   *
   * @param pattern 패턴 (예: "schedule:*")
   * @param channel 채널 (예: "schedule:reload")
   * @return 매칭 여부
   */
  bool matchChannelPattern(const std::string &pattern,
                           const std::string &channel) const;

  // =========================================================================
  // 기본 핸들러들
  // =========================================================================

  /**
   * @brief 알람 이벤트 처리 (기존 로직)
   */
  void handleAlarmEvent(const std::string &channel, const std::string &message);

  /**
   * @brief 스케줄 이벤트 처리
   *
   * 지원 이벤트:
   * - schedule:reload    → ScheduledExporter.reloadSchedules()
   * - schedule:execute:{id} → ScheduledExporter.executeSchedule(id)
   * - schedule:stop:{id} → 특정 스케줄 중지
   */
  void handleScheduleEvent(const std::string &channel,
                           const std::string &message);

  /**
   * @brief 시스템 이벤트 처리
   *
   * 지원 이벤트:
   * - system:shutdown    → 시스템 종료
   * - system:restart     → 시스템 재시작
   * - system:reload_config → 설정 리로드
   */
  void handleSystemEvent(const std::string &channel,
                         const std::string &message);

  // =========================================================================
  // 백그라운드 스레드 루프
  // =========================================================================

  void subscribeLoop();
  void workerLoop(int thread_index);
  void reconnectLoop();

  // =========================================================================
  // 메시지 큐 관리
  // =========================================================================

  struct QueuedMessage {
    std::string channel;
    std::string payload;
    std::chrono::steady_clock::time_point received_time;
  };

  bool enqueueMessage(const std::string &channel, const std::string &message);
  bool dequeueMessage(QueuedMessage &msg);

  // 기존 알람 전용 큐 (하위 호환)
  bool enqueueAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  bool dequeueAlarm(PulseOne::CSP::AlarmMessage &alarm);

  // =========================================================================
  // 알람 처리 (기존 로직 100% 유지)
  // =========================================================================

  PulseOne::CSP::AlarmMessage parseAlarmMessage(const std::string &json_str);
  void processAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  bool filterAlarm(const PulseOne::CSP::AlarmMessage &alarm) const;

  // =========================================================================
  // Redis 연결 관리
  // =========================================================================

  bool initializeRedisConnection();
  bool subscribeAllChannels();

  // =========================================================================
  // 멤버 변수
  // =========================================================================

  // 설정
  EventSubscriberConfig config_;

  // Redis 클라이언트
  std::shared_ptr<RedisClient> redis_client_;

  // 스레드
  std::unique_ptr<std::thread> subscribe_thread_;
  std::vector<std::unique_ptr<std::thread>> worker_threads_;
  std::unique_ptr<std::thread> reconnect_thread_;

  // 상태 플래그
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_connected_{false};
  std::atomic<bool> should_stop_{false};

  // 메시지 큐 (통합)
  std::queue<QueuedMessage> message_queue_;
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // 기존 알람 큐 (하위 호환)
  std::queue<PulseOne::CSP::AlarmMessage> alarm_queue_;

  // 채널 관리
  std::vector<std::string> subscribed_channels_;
  std::vector<std::string> subscribed_patterns_;
  mutable std::mutex channel_mutex_;

  // 이벤트 핸들러 맵
  std::unordered_map<std::string, std::shared_ptr<IEventHandler>>
      event_handlers_;
  mutable std::mutex handler_mutex_;

  // 처리 통계
  std::atomic<size_t> total_received_{0};
  std::atomic<size_t> total_processed_{0};
  std::atomic<size_t> total_failed_{0};
  std::atomic<size_t> max_queue_size_{0};
  std::atomic<int64_t> last_received_timestamp_{0};
  std::atomic<int64_t> last_processed_timestamp_{0};

  // 알람 처리 콜백 (NEW!)
  AlarmCallback alarm_callback_;
};

} // namespace Event
} // namespace PulseOne

#endif // EVENT_SUBSCRIBER_H