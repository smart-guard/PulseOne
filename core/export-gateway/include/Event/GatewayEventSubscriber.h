/**
 * @file EventSubscriber.h
 * @brief Redis Pub/Sub 통합 이벤트 구독자 (v3.2)
 *
 * v3.2 변경사항:
 * ✅ all 모드: alarms:processed 채널 구독 (collector publish 채널과 일치)
 * ✅ selective 모드: alarms:processed 구독 후 point_id 기반 필터링
 */

#ifndef GATEWAY_EVENT_SUBSCRIBER_H
#define GATEWAY_EVENT_SUBSCRIBER_H

#include "Event/EventSubscriber.h" // Shared base class
#include "Gateway/Model/AlarmMessage.h"
#include <set>

namespace PulseOne {
namespace Event {

class GatewayEventSubscriber : public PulseOne::Event::EventSubscriber {
public:
  explicit GatewayEventSubscriber(const EventSubscriberConfig &config);
  virtual ~GatewayEventSubscriber() = default;

  // 알람 처리 콜백 (하위 호환용)
  using AlarmCallback =
      std::function<void(const PulseOne::CSP::AlarmMessage &)>;
  void setAlarmCallback(AlarmCallback callback) { alarm_callback_ = callback; }

  /**
   * @brief [v3.2] Selective 모드: 허용 point_id 셋 설정
   *        비어있으면 전체 허용 (all 모드)
   */
  void setAllowedPointIds(const std::set<int> &point_ids) {
    std::lock_guard<std::mutex> lock(filter_mutex_);
    allowed_point_ids_ = point_ids;
  }

  /**
   * @brief [DEPRECATED] device_id 목록으로 채널 일괄 구독
   *        v3.2부터는 alarms:processed 구독 + point_id 필터로 대체
   *        하위 호환을 위해 남겨둠 (내부적으로는 point_id 필터만 사용)
   */
  void updateSubscriptions(const std::set<std::string> &device_ids);

protected:
  void routeMessage(const std::string &channel,
                    const std::string &message) override;

  void handleAlarmEvent(const std::string &channel, const std::string &message);
  void handleScheduleEvent(const std::string &channel,
                           const std::string &message);
  void handleSystemEvent(const std::string &channel,
                         const std::string &message);
  void handleManualExportEvent(const std::string &channel,
                               const std::string &message);

  PulseOne::CSP::AlarmMessage parseAlarmMessage(const std::string &json_str);
  void processAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  bool filterAlarm(const PulseOne::CSP::AlarmMessage &alarm) const;

  bool enqueueAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  bool dequeueAlarm(PulseOne::CSP::AlarmMessage &alarm);

  void workerLoop(int thread_index);

private:
  AlarmCallback alarm_callback_;
  std::queue<PulseOne::CSP::AlarmMessage> alarm_queue_;
  mutable std::mutex gateway_queue_mutex_;
  std::condition_variable gateway_queue_cv_;

  // [v3.2] Selective 필터: 허용 point_id 셋 (비면 전체 허용)
  std::set<int> allowed_point_ids_;
  mutable std::mutex filter_mutex_;
};

} // namespace Event
} // namespace PulseOne

#endif // GATEWAY_EVENT_SUBSCRIBER_H