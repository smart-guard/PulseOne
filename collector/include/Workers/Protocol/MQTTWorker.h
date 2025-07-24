/**
 * @file MQTTWorker.h
 * @brief MQTT 프로토콜 워커 클래스 (완전 수정)
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.1
 */

#ifndef MQTT_WORKER_H
#define MQTT_WORKER_H

// =============================================================================
// 필수 헤더 include (순서 중요)
// =============================================================================
#include "Common/UnifiedCommonTypes.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Mqtt/MQTTDriver.h"
#include "Utils/LogManager.h"  // ✅ 로그 매니저 추가
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <future>
#include <chrono>
#include <condition_variable>

namespace PulseOne {
namespace Workers {

// =============================================================================
// MQTT QoS 레벨 정의
// =============================================================================

/**
 * @brief MQTT QoS 레벨
 */
enum class MqttQoS : int {
    AT_MOST_ONCE = 0,      ///< 최대 한번 전송
    AT_LEAST_ONCE = 1,     ///< 최소 한번 전송  
    EXACTLY_ONCE = 2       ///< 정확히 한번 전송
};

// =============================================================================
// MQTT 워커 전용 구조체들
// =============================================================================

/**
 * @brief MQTT 토픽 구독 정보
 */
struct MQTTSubscription {
    std::string topic;
    std::string json_path;
    int qos = 1;
    bool enabled = true;
    DataType data_type = DataType::UNKNOWN;
    std::string point_id;
    double scaling_factor = 1.0;
    double scaling_offset = 0.0;
    uint64_t messages_received = 0;
    std::chrono::system_clock::time_point last_received;
    
    MQTTSubscription() : last_received(std::chrono::system_clock::now()) {}
};

/**
 * @brief MQTT 발행 작업
 */
struct MQTTPublishTask {
    std::string topic;
    std::string payload;
    int qos = 1;
    bool retained = false;
    std::chrono::system_clock::time_point scheduled_time;
    int retry_count = 0;
    
    MQTTPublishTask() : scheduled_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief MQTT 워커 설정
 */
struct MQTTWorkerConfig {
    std::string broker_url;
    std::string client_id;
    std::string username;
    std::string password;
    bool use_ssl = false;
    int keep_alive_interval = 60;
    bool clean_session = true;
    bool auto_reconnect = true;
    uint32_t message_timeout_ms = 30000;
    uint32_t publish_retry_count = 3;
    
    MQTTWorkerConfig() = default;
};

/**
 * @brief MQTT 워커 통계
 */
struct MQTTWorkerStatistics {
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_published{0};
    std::atomic<uint64_t> subscribe_operations{0};
    std::atomic<uint64_t> publish_operations{0};
    std::atomic<uint64_t> successful_publishes{0};
    std::atomic<uint64_t> failed_operations{0};
    std::atomic<uint64_t> json_parse_errors{0};
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_reset;
    
    MQTTWorkerStatistics() : start_time(std::chrono::system_clock::now()) {
        last_reset = start_time;
    }
};

// =============================================================================
// MQTTWorker 클래스 정의
// =============================================================================

/**
 * @brief MQTT 프로토콜 워커 클래스
 */
class MQTTWorker : public BaseDeviceWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    explicit MQTTWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );
    
    virtual ~MQTTWorker();

    // =============================================================================
    // 유틸리티 정적 메서드
    // =============================================================================
    
    static int QosToInt(MqttQoS qos) {
        return static_cast<int>(qos);
    }
    
    static MqttQoS IntToQos(int qos_int) {
        switch (qos_int) {
            case 0: return MqttQoS::AT_MOST_ONCE;
            case 2: return MqttQoS::EXACTLY_ONCE;
            default: return MqttQoS::AT_LEAST_ONCE;
        }
    }

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;
    bool SendKeepAlive() override;

    // =============================================================================
    // MQTT 워커 전용 인터페이스
    // =============================================================================
    
    void ConfigureMQTTWorker(const MQTTWorkerConfig& config);
    std::string GetMQTTWorkerStats() const;
    void ResetMQTTWorkerStats();
    bool AddSubscription(const MQTTSubscription& subscription);
    bool RemoveSubscription(const std::string& topic);
    bool PublishMessage(const std::string& topic, const std::string& payload, 
                       int qos = 1, bool retained = false);
    std::string GetActiveSubscriptions() const;

private:
    // =============================================================================
    // 내부 멤버 변수들
    // =============================================================================
    
    // MQTT 설정 및 통계
    MQTTWorkerConfig worker_config_;
    mutable MQTTWorkerStatistics worker_stats_;
    
    // MQTT 드라이버
    std::unique_ptr<PulseOne::Drivers::MqttDriver> mqtt_driver_;  // ✅ MQTTDriver → MqttDriver
    
    // 구독 및 발행 관리
    std::map<std::string, MQTTSubscription> active_subscriptions_;
    mutable std::mutex subscriptions_mutex_;  // ✅ mutable 추가
    
    // 발행 큐 관리
    std::queue<MQTTPublishTask> publish_queue_;
    mutable std::mutex publish_queue_mutex_;  // ✅ mutable 추가
    std::condition_variable publish_queue_cv_;
    
    // 스레드 관리
    std::atomic<bool> threads_running_{false};
    std::unique_ptr<std::thread> message_processor_thread_;
    std::unique_ptr<std::thread> publish_processor_thread_;
    
    // 간단한 MQTT 설정
    struct {
        std::string broker_host = "localhost";
        int broker_port = 1883;
        std::string username = "";
        std::string password = "";
        std::string client_id = "";
        bool clean_session = true;
        int keepalive_interval_sec = 60;
        bool use_ssl = false;
        int connection_timeout_sec = 30;
        int max_retry_count = 3;
    } mqtt_config_;
    
    std::string worker_id_;
    int poll_interval_ms_ = 1000;

    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    bool InitializeMQTTDriver();
    void ShutdownMQTTDriver();
    bool CreateSubscriptionsFromDataPoints(const std::vector<PulseOne::DataPoint>& points);
    bool ProcessReceivedMessage(const std::string& topic, const std::string& payload);
    bool ExtractValueFromJSON(const std::string& payload, 
                             const std::string& json_path, 
                             DataValue& extracted_value);
    bool ParseMQTTTopic(const PulseOne::DataPoint& point, 
                       std::string& topic, std::string& json_path, int& qos);
    bool ParseMQTTWorkerConfig();
    void MessageProcessorThreadFunction();
    void PublishProcessorThreadFunction();
    DriverConfig CreateDriverConfig();
    void UpdateWorkerStats(const std::string& operation, bool success);
    
    static void MessageCallback(MQTTWorker* worker, 
                               const std::string& topic, const std::string& payload);
    
#ifdef HAS_NLOHMANN_JSON
    bool ConvertJsonToDataValue(const nlohmann::json& json_val, DataValue& data_value);
#endif
};

} // namespace Workers
} // namespace PulseOne

#endif // MQTT_WORKER_H