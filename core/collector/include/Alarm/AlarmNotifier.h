// =============================================================================
// collector/include/Alarm/AlarmNotifier.h
// 알람 알림 처리 (분리)
// =============================================================================

#ifndef ALARM_NOTIFIER_H
#define ALARM_NOTIFIER_H

#include "Alarm/AlarmTypes.h"
#include <queue>
#include <thread>
#include <condition_variable>
#include <functional>

namespace PulseOne {
class RedisClient;
class InfluxClient;

namespace Alarm {

using AlarmCallback = std::function<void(const AlarmOccurrence&)>;

class AlarmNotifier {
public:
    AlarmNotifier();
    ~AlarmNotifier();
    
    // 초기화
    bool initialize(const nlohmann::json& config);
    void shutdown();
    
    // 알림 전송
    void notifyAlarm(const AlarmOccurrence& occurrence);
    void queueNotification(const AlarmOccurrence& occurrence);
    
    // 채널별 전송
    bool sendEmail(const AlarmOccurrence& occurrence, const std::vector<std::string>& recipients);
    bool sendSMS(const AlarmOccurrence& occurrence, const std::vector<std::string>& recipients);
    bool sendToMessageQueue(const AlarmOccurrence& occurrence);
    bool sendWebhook(const AlarmOccurrence& occurrence, const std::string& url);
    
    // Redis 캐싱
    void updateRedisCache(const AlarmOccurrence& occurrence);
    void removeFromRedisCache(int64_t occurrence_id);
    
    // InfluxDB 저장
    void writeToInfluxDB(const AlarmOccurrence& occurrence);
    
    // 콜백 관리
    void registerCallback(AlarmCallback callback);
    void clearCallbacks();
    
private:
    // 백그라운드 처리
    void processNotificationQueue();
    
    // 메시지 포맷팅
    std::string formatMessage(const AlarmOccurrence& occurrence, const std::string& channel);
    
private:
    // 알림 큐
    std::queue<AlarmOccurrence> notification_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 콜백
    std::vector<AlarmCallback> callbacks_;
    std::mutex callback_mutex_;
    
    // 외부 연결
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    std::unique_ptr<void, std::function<void(void*)>> mq_connection_;
    
    // 백그라운드 스레드
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    
    // 설정
    nlohmann::json config_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_NOTIFIER_H