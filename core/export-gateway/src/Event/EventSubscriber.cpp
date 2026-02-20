/**
 * @file EventSubscriber.cpp
 * @brief Gateway-specific Event Subscriber Implementation (v3.1)
 * @author PulseOne Development Team
 * @date 2026-02-17
 */

#include "Client/RedisClientImpl.h"
#include "Event/GatewayEventSubscriber.h"
#include "Logging/LogManager.h"
#include "Schedule/ScheduledExporter.h"
#include <algorithm>
#include <chrono>

namespace PulseOne {
namespace Event {

GatewayEventSubscriber::GatewayEventSubscriber(
    const EventSubscriberConfig &config)
    : EventSubscriber(config) {}

void GatewayEventSubscriber::updateSubscriptions(
    const std::set<std::string> &device_ids) {
  // [v3.2] Collector는 device:ID:alarms 채널에 정상 publish함.
  // 기존 설계(device 채널 구독)가 맞음.
  // 문제였던 것: TargetRegistry.getAssignedDeviceIds()가 빈 셋을 반환 → 수정됨.
  LogManager::getInstance().Info("[v3.2] Selective: subscribing " +
                                 std::to_string(device_ids.size()) +
                                 " device channels");

  std::vector<std::string> current_channels = getSubscribedChannels();

  for (const auto &channel : current_channels) {
    if (channel.find("device:") == 0 &&
        channel.find(":alarms") != std::string::npos) {
      std::string id = channel.substr(7, channel.find(":alarms") - 7);
      if (device_ids.find(id) == device_ids.end()) {
        unsubscribeChannel(channel);
      }
    }
  }

  for (const auto &id : device_ids) {
    if (id.empty())
      continue;
    std::string channel = "device:" + id + ":alarms";
    subscribeChannel(channel);
  }
}

void GatewayEventSubscriber::routeMessage(const std::string &channel,
                                          const std::string &message) {
  // 1. 커스텀 핸들러 (Base class logic)
  {
    std::lock_guard<std::mutex> lock(handler_mutex_);
    for (const auto &pair : event_handlers_) {
      if (matchChannelPattern(pair.first, channel)) {
        if (pair.second->handleEvent(channel, message))
          return;
      }
    }
  }

  // 2. 게이트웨이 전용 내장 핸들러
  // NOTE: cmd:gateway:* 는 EventDispatcher::registerHandlers()에서
  // 커스텀 핸들러로 등록되어 위 1번 루프에서 처리됨.
  // 따라서 아래는 cmd: 채널을 제외한 내장 알람/스케줄 이벤트만 처리.
  if (channel.find("alarms:") == 0 || channel.find("alarm:") == 0 ||
      channel.find("device:") == 0) {
    handleAlarmEvent(channel, message);
  } else if (channel.find("schedule:") == 0) {
    handleScheduleEvent(channel, message);
  }
  // cmd:gateway:* 는 의도적으로 여기서 처리하지 않음 (EventDispatcher 담당)
}

void GatewayEventSubscriber::handleAlarmEvent(const std::string &,
                                              const std::string &message) {
  try {
    auto alarm = parseAlarmMessage(message);
    if (!enqueueAlarm(alarm)) {
      LogManager::getInstance().Warn("Alarm Queue Full - message dropped");
      total_failed_++;
    }
  } catch (...) {
    total_failed_++;
  }
}

void GatewayEventSubscriber::handleScheduleEvent(const std::string &channel,
                                                 const std::string &) {
  try {
    auto &exporter = ::PulseOne::Schedule::ScheduledExporter::getInstance();
    if (channel == "schedule:reload") {
      exporter.reloadSchedules();
    } else if (channel.find("schedule:execute:") == 0) {
      int schedule_id = std::stoi(channel.substr(17));
      exporter.executeSchedule(schedule_id);
    }
  } catch (...) {
  }
}

void GatewayEventSubscriber::handleManualExportEvent(const std::string &,
                                                     const std::string &) {
  // Coordinator handles this via CommandEventHandler usually,
  // but we keep the hook for consistency with v3.0
}

void GatewayEventSubscriber::handleSystemEvent(const std::string &channel,
                                               const std::string &) {
  LogManager::getInstance().Info("System Event: " + channel);
}

PulseOne::Gateway::Model::AlarmMessage
GatewayEventSubscriber::parseAlarmMessage(const std::string &json_str) {
  PulseOne::Gateway::Model::AlarmMessage alarm;
  try {
    auto j = json::parse(json_str);
    alarm.from_json(j);
  } catch (...) {
  }
  return alarm;
}

void GatewayEventSubscriber::processAlarm(
    const PulseOne::Gateway::Model::AlarmMessage &alarm) {
  if (!filterAlarm(alarm))
    return;

  if (alarm_callback_) {
    alarm_callback_(alarm);
    return;
  }

  // Fallback logic - removed CSP dependency
  LogManager::getInstance().Warn(
      "Alarm processing skipped: No callback registered");
}

bool GatewayEventSubscriber::filterAlarm(
    const PulseOne::Gateway::Model::AlarmMessage &alarm) const {
  // [v3.2] 채널 기반 필터링(device:ID:alarms 구독)으로 selective 모드 처리.
  // 추가 application 관단 필터링 불필요. 기본 유효성 코드만 확인.
  return !alarm.point_name.empty();
}

bool GatewayEventSubscriber::enqueueAlarm(
    const PulseOne::Gateway::Model::AlarmMessage &alarm) {
  std::lock_guard<std::mutex> lock(gateway_queue_mutex_);
  if (alarm_queue_.size() >= config_.max_queue_size)
    return false;
  alarm_queue_.push(alarm);
  gateway_queue_cv_.notify_one();
  return true;
}

bool GatewayEventSubscriber::dequeueAlarm(
    PulseOne::Gateway::Model::AlarmMessage &alarm) {
  std::lock_guard<std::mutex> lock(gateway_queue_mutex_);
  if (alarm_queue_.empty())
    return false;
  alarm = alarm_queue_.front();
  alarm_queue_.pop();
  return true;
}

void GatewayEventSubscriber::workerLoop(int thread_index) {
  LogManager::getInstance().Info("Gateway Alarm worker [" +
                                 std::to_string(thread_index) + "] started");
  while (!should_stop_.load()) {
    PulseOne::Gateway::Model::AlarmMessage alarm;
    if (!dequeueAlarm(alarm)) {
      std::unique_lock<std::mutex> lock(gateway_queue_mutex_);
      gateway_queue_cv_.wait_for(lock, std::chrono::milliseconds(100));
      continue;
    }

    try {
      processAlarm(alarm);
      total_processed_++;
      last_processed_timestamp_ =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
    } catch (...) {
      total_failed_++;
    }
  }
}

} // namespace Event
} // namespace PulseOne