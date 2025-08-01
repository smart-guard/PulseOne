/**
 * @file MQTTWorker.h - 통합 MQTT 워커 (기본 + 프로덕션 기능)
 * @brief 하나의 클래스로 기본부터 프로덕션까지 모든 MQTT 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 3.0.0 (통합 버전)
 */

#ifndef PULSEONE_WORKERS_PROTOCOL_MQTT_WORKER_H
#define PULSEONE_WORKERS_PROTOCOL_MQTT_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include "Common/Structs.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>
#include <set>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace PulseOne {
namespace Workers {

// =============================================================================
// MQTT 전용 데이터 구조체들
// =============================================================================

/**
 * @brief MQTT QoS 레벨 열거형
 */
enum class MqttQoS : int {
    AT_MOST_ONCE = 0,   ///< QoS 0: 최대 한 번 전송
    AT_LEAST_ONCE = 1,  ///< QoS 1: 최소 한 번 전송
    EXACTLY_ONCE = 2    ///< QoS 2: 정확히 한 번 전송
};

/**
 * @brief MQTT 워커 동작 모드
 */
enum class MQTTWorkerMode {
    BASIC = 0,        ///< 기본 모드 (개발/테스트용)
    PRODUCTION = 1    ///< 프로덕션 모드 (고급 기능 활성화)
};

/**
 * @brief MQTT 구독 정보
 */
struct MQTTSubscription {
    uint32_t subscription_id;                           ///< 구독 ID (고유)
    std::string topic;                                  ///< MQTT 토픽
    MqttQoS qos;                                       ///< QoS 레벨
    std::vector<PulseOne::DataPoint> data_points;      ///< 연결된 데이터 포인트들
    std::string json_path;                             ///< JSON 경로 (예: "sensors.temperature")
    bool is_active;                                    ///< 구독 활성화 상태
    
    // 통계 정보
    std::chrono::system_clock::time_point last_message_time;
    uint64_t total_messages_received;
    uint64_t json_parse_errors;
    
    MQTTSubscription()
        : subscription_id(0), qos(MqttQoS::AT_LEAST_ONCE), is_active(false)
        , last_message_time(std::chrono::system_clock::now())
        , total_messages_received(0), json_parse_errors(0) {}
};

/**
 * @brief MQTT 발행 작업
 */
struct MQTTPublishTask {
    std::string topic;                                 ///< 발행 토픽
    std::string payload;                               ///< 메시지 내용
    MqttQoS qos;                                      ///< QoS 레벨
    bool retained;                                     ///< Retained 메시지 여부
    std::chrono::system_clock::time_point scheduled_time; ///< 예정 시간
    uint32_t retry_count;                             ///< 재시도 횟수
    int priority;                                      ///< 우선순위 (프로덕션 모드용)
    
    MQTTPublishTask()
        : qos(MqttQoS::AT_LEAST_ONCE), retained(false)
        , scheduled_time(std::chrono::system_clock::now()), retry_count(0), priority(5) {}
};

/**
 * @brief 오프라인 메시지 (프로덕션 모드용)
 */
struct OfflineMessage {
    std::string topic;
    std::string payload;
    MqttQoS qos;
    bool retain;
    std::chrono::system_clock::time_point timestamp;
    int priority = 5;
    
    OfflineMessage(const std::string& t, const std::string& p, MqttQoS q = MqttQoS::AT_LEAST_ONCE, bool r = false, int prio = 5)
        : topic(t), payload(p), qos(q), retain(r), timestamp(std::chrono::system_clock::now()), priority(prio) {}
    
    bool operator<(const OfflineMessage& other) const {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return timestamp > other.timestamp;
    }
};

/**
 * @brief 성능 메트릭스 (프로덕션 모드용)
 */
struct PerformanceMetrics {
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_dropped{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> peak_throughput_bps{0};
    std::atomic<uint64_t> avg_throughput_bps{0};
    std::atomic<uint32_t> connection_count{0};
    std::atomic<uint32_t> error_count{0};
    std::atomic<uint32_t> retry_count{0};
    std::atomic<uint32_t> avg_latency_ms{0};
    std::atomic<uint32_t> max_latency_ms{0};
    std::atomic<uint32_t> min_latency_ms{999999};
    std::atomic<uint32_t> queue_size{0};
    std::atomic<uint32_t> max_queue_size{0};
    
    void Reset() {
        messages_sent = 0;
        messages_received = 0;
        messages_dropped = 0;
        bytes_sent = 0;
        bytes_received = 0;
        peak_throughput_bps = 0;
        avg_throughput_bps = 0;
        connection_count = 0;
        error_count = 0;
        retry_count = 0;
        avg_latency_ms = 0;
        max_latency_ms = 0;
        min_latency_ms = 999999;
        queue_size = 0;
        max_queue_size = 0;
    }
};

/**
 * @brief MQTT Worker 기본 통계
 */
struct MQTTWorkerStatistics {
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_published{0};
    std::atomic<uint64_t> successful_subscriptions{0};
    std::atomic<uint64_t> failed_operations{0};
    std::atomic<uint64_t> json_parse_errors{0};
    std::atomic<uint64_t> connection_attempts{0};
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_reset;
    
    MQTTWorkerStatistics() : start_time(std::chrono::system_clock::now()) {
        last_reset = start_time;
    }
};

/**
 * @brief 고급 MQTT 설정 (프로덕션 모드용)
 */
struct AdvancedMqttConfig {
    bool circuit_breaker_enabled = false;
    int max_failures = 10;
    int circuit_timeout_seconds = 60;
    bool offline_mode_enabled = false;
    size_t max_offline_messages = 1000;
};

// =============================================================================
// 통합 MQTTWorker 클래스 정의
// =============================================================================

/**
 * @brief 통합 MQTT 프로토콜 워커 클래스
 * @details 기본 기능부터 프로덕션 고급 기능까지 모든 MQTT 기능을 하나의 클래스로 제공
 * 
 * 사용 방법:
 * - 기본 모드: 간단한 MQTT 통신 (개발/테스트용)
 * - 프로덕션 모드: 고급 기능 활성화 (성능 모니터링, 장애 조치, 우선순위 큐 등)
 */
class MQTTWorker : public BaseDeviceWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     * @param mode 워커 모드 (기본값: BASIC)
     */
    explicit MQTTWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr,
        MQTTWorkerMode mode = MQTTWorkerMode::BASIC
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~MQTTWorker();

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
    // 기본 MQTT 기능 (모든 모드에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief 구독 추가
     */
    bool AddSubscription(const MQTTSubscription& subscription);
    
    /**
     * @brief 구독 제거
     */
    bool RemoveSubscription(uint32_t subscription_id);
    
    /**
     * @brief 메시지 발행 (구조체 버전)
     */
    bool PublishMessage(const MQTTPublishTask& task);
    
    /**
     * @brief 메시지 발행 (개별 파라미터 버전)
     */
    bool PublishMessage(const std::string& topic, const std::string& payload, 
                       int qos = 1, bool retained = false);
    
    /**
     * @brief 기본 통계 조회
     */
    std::string GetMQTTWorkerStats() const;
    
    /**
     * @brief 통계 초기화
     */
    void ResetMQTTWorkerStats();
    
    // =============================================================================
    // 모드 제어 및 설정
    // =============================================================================
    
    /**
     * @brief 워커 모드 설정
     */
    void SetWorkerMode(MQTTWorkerMode mode);
    
    /**
     * @brief 현재 워커 모드 조회
     */
    MQTTWorkerMode GetWorkerMode() const { return worker_mode_; }
    
    /**
     * @brief 프로덕션 모드 활성화/비활성화
     */
    void EnableProductionMode(bool enable) {
        SetWorkerMode(enable ? MQTTWorkerMode::PRODUCTION : MQTTWorkerMode::BASIC);
    }
    
    /**
     * @brief 프로덕션 모드 여부 확인
     */
    bool IsProductionMode() const { 
        return worker_mode_ == MQTTWorkerMode::PRODUCTION; 
    }

    // =============================================================================
    // 프로덕션 전용 기능 (PRODUCTION 모드에서만 활성화)
    // =============================================================================
    
    /**
     * @brief 우선순위 메시지 발행
     */
    bool PublishWithPriority(const std::string& topic,
                           const std::string& payload,
                           int priority,
                           MqttQoS qos = MqttQoS::AT_LEAST_ONCE);
    
    /**
     * @brief 배치 메시지 발행
     */
    size_t PublishBatch(const std::vector<OfflineMessage>& messages);
    
    /**
     * @brief 큐 여유 공간 확인 후 발행
     */
    bool PublishIfQueueAvailable(const std::string& topic,
                                const std::string& payload,
                                size_t max_queue_size = 1000);
    
    /**
     * @brief 성능 메트릭스 조회 (프로덕션 모드)
     */
    PerformanceMetrics GetPerformanceMetrics() const;
    
    /**
     * @brief 성능 메트릭스 JSON 형태 조회
     */
    std::string GetPerformanceMetricsJson() const;
    
    /**
     * @brief 실시간 대시보드 데이터
     */
    std::string GetRealtimeDashboardData() const;
    
    /**
     * @brief 상세 진단 정보
     */
    std::string GetDetailedDiagnostics() const;
    
    /**
     * @brief 연결 상태 건강성 확인
     */
    bool IsConnectionHealthy() const;
    
    /**
     * @brief 시스템 로드 조회
     */
    double GetSystemLoad() const;
    
    // =============================================================================
    // 프로덕션 모드 설정 및 제어
    // =============================================================================
    
    /**
     * @brief 메트릭스 수집 간격 설정
     */
    void SetMetricsCollectionInterval(int interval_seconds);
    
    /**
     * @brief 최대 큐 크기 설정
     */
    void SetMaxQueueSize(size_t max_size);
    
    /**
     * @brief 성능 메트릭스 리셋
     */
    void ResetMetrics();
    
    /**
     * @brief 백프레셔 임계값 설정
     */
    void SetBackpressureThreshold(double threshold);
    
    /**
     * @brief 고급 설정 적용
     */
    void ConfigureAdvanced(const AdvancedMqttConfig& config);
    
    /**
     * @brief 자동 장애 조치 활성화
     */
    void EnableAutoFailover(const std::vector<std::string>& backup_brokers, int max_failures = 5);
    
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

private:
    // =============================================================================
    // 내부 멤버 변수들
    // =============================================================================
    
    // 모드 및 기본 설정
    MQTTWorkerMode worker_mode_;
    
    // MQTT 드라이버 및 설정
    std::unique_ptr<PulseOne::Drivers::MqttDriver> mqtt_driver_;
    
    // 구독 관리
    std::map<uint32_t, MQTTSubscription> active_subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    uint32_t next_subscription_id_;
    
    // 발행 큐 관리
    std::queue<MQTTPublishTask> publish_queue_;
    mutable std::mutex publish_queue_mutex_;
    std::condition_variable publish_queue_cv_;
    
    // 스레드 관리
    std::atomic<bool> message_thread_running_;
    std::atomic<bool> publish_thread_running_;
    std::unique_ptr<std::thread> message_processor_thread_;
    std::unique_ptr<std::thread> publish_processor_thread_;
    
    // 기본 통계
    mutable MQTTWorkerStatistics worker_stats_;
    
    // MQTT 설정
    struct {
        std::string broker_url = "mqtt://localhost:1883";
        std::string client_id = "";
        std::string username = "";
        std::string password = "";
        bool clean_session = true;
        int keepalive_interval_sec = 60;
        bool use_ssl = false;
        int connection_timeout_sec = 30;
        int max_retry_count = 3;
        MqttQoS default_qos = MqttQoS::AT_LEAST_ONCE;
    } mqtt_config_;
    
    // Worker 레벨 설정
    uint32_t default_message_timeout_ms_ = 30000;
    uint32_t max_publish_queue_size_ = 10000;
    bool auto_reconnect_enabled_ = true;
    
    // =============================================================================
    // 프로덕션 모드 전용 멤버 변수들
    // =============================================================================
    
    // 성능 메트릭스 (프로덕션 모드)
    mutable PerformanceMetrics performance_metrics_;
    
    // 오프라인 메시지 큐 (프로덕션 모드)
    std::priority_queue<OfflineMessage, std::vector<OfflineMessage>, std::greater<OfflineMessage>> offline_messages_;
    mutable std::mutex offline_messages_mutex_;
    
    // 프로덕션 전용 스레드
    std::atomic<bool> metrics_thread_running_{false};
    std::atomic<bool> priority_thread_running_{false};
    std::atomic<bool> alarm_thread_running_{false};
    std::unique_ptr<std::thread> metrics_thread_;
    std::unique_ptr<std::thread> priority_queue_thread_;
    std::unique_ptr<std::thread> alarm_monitor_thread_;
    
    // 프로덕션 설정
    std::atomic<int> metrics_collection_interval_{60};
    std::atomic<size_t> max_queue_size_{10000};
    std::atomic<double> backpressure_threshold_{0.8};
    AdvancedMqttConfig advanced_config_;
    
    // 장애 조치 관련
    std::vector<std::string> backup_brokers_;
    std::atomic<int> current_broker_index_{0};
    std::atomic<bool> circuit_open_{false};
    std::atomic<int> consecutive_failures_{0};
    std::chrono::steady_clock::time_point circuit_open_time_;
    mutable std::mutex circuit_mutex_;
    
    // 중복 메시지 방지
    std::set<std::string> processed_message_ids_;
    mutable std::mutex message_ids_mutex_;
    
    // 시간 추적
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_throughput_calculation_;
    
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    // 기본 기능 메서드들
    bool ParseMQTTConfig();
    bool InitializeMQTTDriver();
    void MessageProcessorThreadFunction();
    void PublishProcessorThreadFunction();
    bool ProcessReceivedMessage(const std::string& topic, const std::string& payload);
    bool ExtractValueFromJSON(const std::string& payload, 
                             const std::string& json_path,
                             PulseOne::Structs::DataValue& extracted_value);
    bool ParseMQTTTopic(const PulseOne::DataPoint& data_point,
                       std::string& topic, std::string& json_path, int& qos);
    bool SaveDataPointValue(const PulseOne::DataPoint& data_point,
                           const PulseOne::TimestampedValue& value);
    bool ValidateSubscription(const MQTTSubscription& subscription);
    
    // 프로덕션 모드 전용 메서드들
    void StartProductionThreads();
    void StopProductionThreads();
    void MetricsCollectorLoop();
    void PriorityQueueProcessorLoop();
    void AlarmMonitorLoop();
    void CollectPerformanceMetrics();
    void UpdateThroughputMetrics();
    void UpdateLatencyMetrics(uint32_t latency_ms);
    
    // 헬퍼 메서드들
    std::string SelectBroker();
    bool IsCircuitOpen() const;
    bool IsTopicAllowed(const std::string& topic) const;
    bool HandleBackpressure();
    void SaveOfflineMessage(const OfflineMessage& message);
    bool IsDuplicateMessage(const std::string& message_id);
    double CalculateMessagePriority(const std::string& topic, const std::string& payload);
    
    static void MessageCallback(MQTTWorker* worker, 
                               const std::string& topic, 
                               const std::string& payload);

#ifdef HAS_NLOHMANN_JSON
    bool ConvertJsonToDataValue(const nlohmann::json& json_val,
                               PulseOne::Structs::DataValue& data_value);
#endif
};

} // namespace Workers
} // namespace PulseOne

#endif // PULSEONE_WORKERS_PROTOCOL_MQTT_WORKER_H