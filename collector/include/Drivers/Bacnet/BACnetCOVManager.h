// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCOVManager.h
// 🔥 BACnet COV (Change of Value) 관리자 - 실시간 변화 감지
// =============================================================================

#ifndef BACNET_COV_MANAGER_H
#define BACNET_COV_MANAGER_H

#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <queue>

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
}
#endif

namespace PulseOne {
namespace Drivers {

class BACnetDriver; // 전방 선언

/**
 * @brief BACnet COV 구독 관리자
 * 
 * 기능:
 * - COV 구독/해제 자동 관리
 * - 구독 갱신 및 만료 처리
 * - COV 알림 처리 및 콜백
 * - 구독 상태 모니터링
 * - 실패 시 자동 재구독
 */
class BACnetCOVManager {
public:
    // ==========================================================================
    // 타입 정의들
    // ==========================================================================
    
    /**
     * @brief COV 구독 정보
     */
    struct COVSubscription {
        uint32_t device_id;
        uint32_t object_instance;
        BACNET_OBJECT_TYPE object_type;
        uint32_t subscriber_process_id;
        uint32_t lifetime_seconds;
        bool confirmed_notifications;
        
        // 상태 정보
        std::chrono::system_clock::time_point subscription_time;
        std::chrono::system_clock::time_point expiry_time;
        std::chrono::system_clock::time_point last_notification;
        std::chrono::system_clock::time_point last_renewal_attempt;
        
        // 통계
        std::atomic<uint64_t> notifications_received{0};
        std::atomic<uint64_t> renewal_attempts{0};
        std::atomic<uint64_t> renewal_failures{0};
        std::atomic<bool> is_active{false};
        std::atomic<bool> needs_renewal{false};
        
        COVSubscription();
        COVSubscription(uint32_t dev_id, uint32_t obj_inst, BACNET_OBJECT_TYPE obj_type);
        
        bool IsExpired() const;
        bool NeedsRenewal() const;
        std::chrono::seconds GetRemainingTime() const;
        std::string GetKey() const;
    };
    
    /**
     * @brief COV 알림 콜백 타입
     */
    using COVNotificationCallback = std::function<void(
        uint32_t device_id,
        uint32_t object_instance,
        BACNET_OBJECT_TYPE object_type,
        const TimestampedValue& value,
        const std::vector<std::pair<BACNET_PROPERTY_ID, TimestampedValue>>& changed_properties
    )>;
    
    /**
     * @brief COV 구독 설정
     */
    struct COVConfig {
        uint32_t default_lifetime_seconds = 300;    // 기본 구독 수명 (5분)
        uint32_t renewal_threshold_percent = 20;    // 갱신 임계값 (20% 남았을 때)
        uint32_t max_renewal_attempts = 3;          // 최대 갱신 시도 횟수
        uint32_t renewal_retry_delay_seconds = 30;  // 갱신 재시도 지연
        bool auto_renewal_enabled = true;           // 자동 갱신 활성화
        bool confirmed_notifications = true;        // 확인된 알림 사용
        uint32_t monitoring_interval_seconds = 10;  // 모니터링 주기
        uint32_t max_active_subscriptions = 100;    // 최대 활성 구독 수
    };

public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    explicit BACnetCOVManager(BACnetDriver* driver);
    ~BACnetCOVManager();
    
    // 복사/이동 방지
    BACnetCOVManager(const BACnetCOVManager&) = delete;
    BACnetCOVManager& operator=(const BACnetCOVManager&) = delete;
    
    // ==========================================================================
    // 🔥 COV 구독 관리
    // ==========================================================================
    
    /**
     * @brief COV 구독 요청
     */
    bool Subscribe(uint32_t device_id,
                  uint32_t object_instance,
                  BACNET_OBJECT_TYPE object_type,
                  uint32_t lifetime_seconds = 0); // 0 = 기본값 사용
    
    /**
     * @brief COV 구독 해제
     */
    bool Unsubscribe(uint32_t device_id,
                    uint32_t object_instance,
                    BACNET_OBJECT_TYPE object_type);
    
    /**
     * @brief 모든 COV 구독 해제
     */
    void UnsubscribeAll();
    
    /**
     * @brief 특정 디바이스의 모든 COV 구독 해제
     */
    bool UnsubscribeDevice(uint32_t device_id);
    
    /**
     * @brief COV 구독 갱신
     */
    bool RenewSubscription(const std::string& subscription_key);
    
    // ==========================================================================
    // 🔥 콜백 및 이벤트 처리
    // ==========================================================================
    
    /**
     * @brief COV 알림 콜백 등록
     */
    void SetNotificationCallback(COVNotificationCallback callback);
    
    /**
     * @brief COV 알림 처리 (BACnet 스택 콜백에서 호출)
     */
    void ProcessCOVNotification(uint32_t device_id,
                               uint32_t object_instance,
                               BACNET_OBJECT_TYPE object_type,
                               const std::vector<std::pair<BACNET_PROPERTY_ID, TimestampedValue>>& properties);
    
    // ==========================================================================
    // 🔥 구독 상태 및 모니터링
    // ==========================================================================
    
    /**
     * @brief 활성 구독 목록 조회
     */
    std::vector<COVSubscription> GetActiveSubscriptions() const;
    
    /**
     * @brief 특정 구독 정보 조회
     */
    bool GetSubscriptionInfo(uint32_t device_id,
                           uint32_t object_instance,
                           BACNET_OBJECT_TYPE object_type,
                           COVSubscription& info) const;
    
    /**
     * @brief 구독 상태 확인
     */
    bool IsSubscribed(uint32_t device_id,
                     uint32_t object_instance,
                     BACNET_OBJECT_TYPE object_type) const;
    
    /**
     * @brief 만료된 구독 정리
     */
    size_t CleanupExpiredSubscriptions();
    
    /**
     * @brief COV 통계 정보
     */
    struct COVStatistics {
        std::atomic<uint32_t> active_subscriptions{0};
        std::atomic<uint64_t> total_subscriptions{0};
        std::atomic<uint64_t> total_unsubscriptions{0};
        std::atomic<uint64_t> notifications_received{0};
        std::atomic<uint64_t> notifications_processed{0};
        std::atomic<uint64_t> renewal_attempts{0};
        std::atomic<uint64_t> renewal_successes{0};
        std::atomic<uint64_t> expired_subscriptions{0};
        std::atomic<uint64_t> failed_subscriptions{0};
        
        void Reset();
    };
    
    const COVStatistics& GetCOVStatistics() const { return cov_stats_; }
    
    // ==========================================================================
    // 설정 관리
    // ==========================================================================
    
    void SetCOVConfig(const COVConfig& config);
    const COVConfig& GetCOVConfig() const { return config_; }
    
    /**
     * @brief COV 관리자 시작
     */
    bool Start();
    
    /**
     * @brief COV 관리자 정지
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }

private:
    // ==========================================================================
    // 내부 구조체들
    // ==========================================================================
    
    struct PendingCOVRequest {
        std::string subscription_key;
        uint8_t invoke_id;
        std::chrono::steady_clock::time_point request_time;
        std::chrono::steady_clock::time_point timeout_time;
        std::string operation_type; // "subscribe", "unsubscribe", "renew"
        
        PendingCOVRequest(const std::string& key, uint8_t id, const std::string& op);
    };
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    BACnetDriver* driver_;                    // 부모 드라이버
    COVConfig config_;                        // COV 설정
    COVStatistics cov_stats_;                 // COV 통계
    COVNotificationCallback notification_callback_; // 알림 콜백
    
    // 구독 관리
    mutable std::mutex subscriptions_mutex_;
    std::map<std::string, COVSubscription> active_subscriptions_;
    
    // 요청 관리
    std::mutex pending_requests_mutex_;
    std::map<uint8_t, PendingCOVRequest> pending_requests_;
    uint8_t next_invoke_id_{1};
    
    // 스레드 관리
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    std::thread monitoring_thread_;
    std::thread renewal_thread_;
    std::mutex thread_mutex_;
    std::condition_variable thread_condition_;
    
    // 갱신 큐
    std::mutex renewal_queue_mutex_;
    std::queue<std::string> renewal_queue_;
    std::condition_variable renewal_condition_;
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    // 스레드 함수들
    void MonitoringThreadFunction();
    void RenewalThreadFunction();
    
    // 구독 관리
    std::string CreateSubscriptionKey(uint32_t device_id, uint32_t object_instance, 
                                     BACNET_OBJECT_TYPE object_type) const;
    bool SendCOVSubscribeRequest(const COVSubscription& subscription, bool subscribe = true);
    bool SendCOVUnsubscribeRequest(const COVSubscription& subscription);
    
    // 요청 관리
    uint8_t GetNextInvokeId();
    void RegisterPendingRequest(const PendingCOVRequest& request);
    bool CompletePendingRequest(uint8_t invoke_id, bool success);
    void TimeoutPendingRequests();
    
    // 구독 모니터링
    void CheckSubscriptionExpirations();
    void CheckRenewalNeeds();
    void ProcessRenewalQueue();
    void UpdateSubscriptionStatistics();
    
    // 에러 처리
    void LogCOVError(const std::string& operation, const std::string& error_message);
    void HandleSubscriptionFailure(const std::string& subscription_key, const std::string& reason);
    
    // 유틸리티
    bool IsValidObjectForCOV(BACNET_OBJECT_TYPE object_type) const;
    uint32_t GetEffectiveLifetime(uint32_t requested_lifetime) const;
    
#ifdef HAS_BACNET_STACK
    // BACnet 스택 헬퍼들
    bool GetDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address);
    bool ParseCOVNotificationData(const uint8_t* service_data, uint16_t service_len,
                                 uint32_t& device_id, uint32_t& object_instance,
                                 BACNET_OBJECT_TYPE& object_type,
                                 std::vector<std::pair<BACNET_PROPERTY_ID, TimestampedValue>>& properties);
    TimestampedValue ConvertBACnetValueToTimestampedValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value);
#endif
    
    // 친구 클래스
    friend class BACnetDriver;
};

// =============================================================================
// 🔥 인라인 구현들
// =============================================================================

inline BACnetCOVManager::COVSubscription::COVSubscription()
    : device_id(0), object_instance(0), object_type(OBJECT_ANALOG_INPUT)
    , subscriber_process_id(0), lifetime_seconds(300), confirmed_notifications(true) {
    subscription_time = std::chrono::system_clock::now();
    expiry_time = subscription_time + std::chrono::seconds(lifetime_seconds);
}

inline BACnetCOVManager::COVSubscription::COVSubscription(uint32_t dev_id, uint32_t obj_inst, BACNET_OBJECT_TYPE obj_type)
    : device_id(dev_id), object_instance(obj_inst), object_type(obj_type)
    , subscriber_process_id(0), lifetime_seconds(300), confirmed_notifications(true) {
    subscription_time = std::chrono::system_clock::now();
    expiry_time = subscription_time + std::chrono::seconds(lifetime_seconds);
}

inline bool BACnetCOVManager::COVSubscription::IsExpired() const {
    return std::chrono::system_clock::now() > expiry_time;
}

inline bool BACnetCOVManager::COVSubscription::NeedsRenewal() const {
    if (!is_active.load()) return false;
    
    auto now = std::chrono::system_clock::now();
    auto renewal_time = expiry_time - std::chrono::seconds(lifetime_seconds / 5); // 20% 전에 갱신
    return now > renewal_time;
}

inline std::chrono::seconds BACnetCOVManager::COVSubscription::GetRemainingTime() const {
    auto now = std::chrono::system_clock::now();
    if (now > expiry_time) return std::chrono::seconds(0);
    return std::chrono::duration_cast<std::chrono::seconds>(expiry_time - now);
}

inline std::string BACnetCOVManager::COVSubscription::GetKey() const {
    return std::to_string(device_id) + "_" + std::to_string(object_type) + "_" + std::to_string(object_instance);
}

inline BACnetCOVManager::PendingCOVRequest::PendingCOVRequest(const std::string& key, uint8_t id, const std::string& op)
    : subscription_key(key), invoke_id(id), operation_type(op) {
    request_time = std::chrono::steady_clock::now();
    timeout_time = request_time + std::chrono::seconds(10); // 10초 타임아웃
}

inline void BACnetCOVManager::COVStatistics::Reset() {
    active_subscriptions = 0;
    total_subscriptions = 0;
    total_unsubscriptions = 0;
    notifications_received = 0;
    notifications_processed = 0;
    renewal_attempts = 0;
    renewal_successes = 0;
    expired_subscriptions = 0;
    failed_subscriptions = 0;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COV_MANAGER_H