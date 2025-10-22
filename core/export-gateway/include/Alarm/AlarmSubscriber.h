/**
 * @file AlarmSubscriber.h
 * @brief Redis Pub/Sub 알람 구독 및 전송 클래스
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 * 
 * 기능:
 * - Redis Pub/Sub 채널 구독 (alarms:all, alarms:critical 등)
 * - 알람 메시지 실시간 수신
 * - JSON 파싱하여 AlarmMessage 객체 생성
 * - CSPGateway를 통해 타겟들에 전송
 * - 멀티스레드 안전
 * 
 * 저장 위치: core/export-gateway/include/Alarm/AlarmSubscriber.h
 */

#ifndef ALARM_SUBSCRIBER_H
#define ALARM_SUBSCRIBER_H

#include "Client/RedisClient.h"
#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <queue>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 설정 구조체
// =============================================================================

struct AlarmSubscriberConfig {
    // Redis 연결 정보
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password = "";
    
    // 구독 채널 설정
    std::vector<std::string> subscribe_channels = {"alarms:all"};
    std::vector<std::string> subscribe_patterns;  // 패턴 구독 (예: "tenant:*:alarms")
    
    // CSPGateway 설정
    PulseOne::CSP::CSPGatewayConfig gateway_config;
    
    // 처리 옵션
    int worker_thread_count = 1;          // 처리 워커 스레드 수
    size_t max_queue_size = 10000;        // 최대 큐 크기
    bool auto_reconnect = true;           // 자동 재연결
    int reconnect_interval_seconds = 5;   // 재연결 간격 (초)
    
    // 필터링
    bool filter_by_severity = false;      // 심각도별 필터링
    std::vector<std::string> allowed_severities;  // 허용된 심각도 (예: ["CRITICAL", "HIGH"])
    
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
        return json{
            {"total_received", total_received},
            {"total_processed", total_processed},
            {"total_failed", total_failed},
            {"queue_size", queue_size},
            {"max_queue_size_reached", max_queue_size_reached},
            {"last_received_timestamp", last_received_timestamp},
            {"last_processed_timestamp", last_processed_timestamp},
            {"avg_processing_time_ms", avg_processing_time_ms}
        };
    }
};

// =============================================================================
// AlarmSubscriber 클래스
// =============================================================================

class AlarmSubscriber {
public:
    // =========================================================================
    // 생성자 및 소멸자
    // =========================================================================
    
    explicit AlarmSubscriber(const AlarmSubscriberConfig& config);
    ~AlarmSubscriber();
    
    // 복사/이동 방지
    AlarmSubscriber(const AlarmSubscriber&) = delete;
    AlarmSubscriber& operator=(const AlarmSubscriber&) = delete;
    AlarmSubscriber(AlarmSubscriber&&) = delete;
    AlarmSubscriber& operator=(AlarmSubscriber&&) = delete;
    
    // =========================================================================
    // 라이프사이클 관리
    // =========================================================================
    
    /**
     * @brief 구독 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief 구독 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     * @return 실행 중이면 true
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =========================================================================
    // 채널 관리
    // =========================================================================
    
    /**
     * @brief 채널 구독 추가
     * @param channel 채널명
     * @return 성공 시 true
     */
    bool subscribeChannel(const std::string& channel);
    
    /**
     * @brief 채널 구독 해제
     * @param channel 채널명
     * @return 성공 시 true
     */
    bool unsubscribeChannel(const std::string& channel);
    
    /**
     * @brief 패턴 구독 추가
     * @param pattern 패턴 (예: "tenant:*:alarms")
     * @return 성공 시 true
     */
    bool subscribePattern(const std::string& pattern);
    
    /**
     * @brief 패턴 구독 해제
     * @param pattern 패턴
     * @return 성공 시 true
     */
    bool unsubscribePattern(const std::string& pattern);
    
    /**
     * @brief 구독 중인 채널 목록
     * @return 채널 목록
     */
    std::vector<std::string> getSubscribedChannels() const;
    
    /**
     * @brief 구독 중인 패턴 목록
     * @return 패턴 목록
     */
    std::vector<std::string> getSubscribedPatterns() const;
    
    // =========================================================================
    // 콜백 설정
    // =========================================================================
    
    using AlarmCallback = std::function<void(const PulseOne::CSP::AlarmMessage&)>;
    
    /**
     * @brief 알람 처리 전 콜백 설정
     * @param callback 콜백 함수
     */
    void setPreProcessCallback(AlarmCallback callback);
    
    /**
     * @brief 알람 처리 후 콜백 설정
     * @param callback 콜백 함수
     */
    void setPostProcessCallback(AlarmCallback callback);
    
    // =========================================================================
    // 통계
    // =========================================================================
    
    /**
     * @brief 통계 조회
     * @return 통계 정보
     */
    SubscriptionStats getStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStatistics();
    
    /**
     * @brief 상세 통계 (JSON)
     * @return 통계 정보 JSON
     */
    json getDetailedStatistics() const;
    
private:
    // =========================================================================
    // 내부 메서드
    // =========================================================================
    
    /**
     * @brief 구독 루프 (구독 스레드)
     */
    void subscribeLoop();
    
    /**
     * @brief 워커 루프 (처리 스레드)
     * @param thread_index 스레드 인덱스
     */
    void workerLoop(int thread_index);
    
    /**
     * @brief 재연결 루프 (재연결 스레드)
     */
    void reconnectLoop();
    
    /**
     * @brief 메시지 핸들러 (Redis 콜백)
     * @param channel 채널명
     * @param message 메시지
     */
    void handleMessage(const std::string& channel, const std::string& message);
    
    /**
     * @brief JSON 메시지 파싱
     * @param json_str JSON 문자열
     * @return AlarmMessage 객체 (실패 시 empty optional)
     */
    std::optional<PulseOne::CSP::AlarmMessage> parseAlarmMessage(const std::string& json_str);
    
    /**
     * @brief 알람 메시지 처리
     * @param alarm 알람 메시지
     */
    void processAlarm(const PulseOne::CSP::AlarmMessage& alarm);
    
    /**
     * @brief 알람 필터링 (심각도 기준)
     * @param alarm 알람 메시지
     * @return 통과 시 true
     */
    bool filterAlarm(const PulseOne::CSP::AlarmMessage& alarm) const;
    
    /**
     * @brief Redis 연결 초기화
     * @return 성공 시 true
     */
    bool initializeRedisConnection();
    
    /**
     * @brief CSPGateway 초기화
     * @return 성공 시 true
     */
    bool initializeCSPGateway();
    
    /**
     * @brief 모든 채널 구독
     * @return 성공 시 true
     */
    bool subscribeAllChannels();
    
    /**
     * @brief 큐에 알람 추가
     * @param alarm 알람 메시지
     * @return 성공 시 true
     */
    bool enqueueAlarm(const PulseOne::CSP::AlarmMessage& alarm);
    
    /**
     * @brief 큐에서 알람 가져오기
     * @param alarm 알람 메시지 (출력)
     * @return 성공 시 true
     */
    bool dequeueAlarm(PulseOne::CSP::AlarmMessage& alarm);
    
    // =========================================================================
    // 멤버 변수
    // =========================================================================
    
    // 설정
    AlarmSubscriberConfig config_;
    
    // Redis 클라이언트
    std::shared_ptr<RedisClient> redis_client_;
    
    // CSPGateway
    std::unique_ptr<PulseOne::CSP::CSPGateway> csp_gateway_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> subscribe_thread_;
    std::vector<std::unique_ptr<std::thread>> worker_threads_;
    std::unique_ptr<std::thread> reconnect_thread_;
    
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_connected_{false};
    
    // 메시지 큐
    std::queue<PulseOne::CSP::AlarmMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 구독 채널 관리
    std::vector<std::string> subscribed_channels_;
    std::vector<std::string> subscribed_patterns_;
    mutable std::mutex channel_mutex_;
    
    // 콜백
    AlarmCallback pre_process_callback_;
    AlarmCallback post_process_callback_;
    mutable std::mutex callback_mutex_;
    
    // 통계
    std::atomic<size_t> total_received_{0};
    std::atomic<size_t> total_processed_{0};
    std::atomic<size_t> total_failed_{0};
    std::atomic<size_t> total_filtered_{0};
    std::atomic<size_t> max_queue_size_reached_{0};
    std::atomic<int64_t> last_received_timestamp_{0};
    std::atomic<int64_t> last_processed_timestamp_{0};
    std::atomic<int64_t> total_processing_time_ms_{0};
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_SUBSCRIBER_H