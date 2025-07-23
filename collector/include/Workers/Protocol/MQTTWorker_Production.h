/**
 * @file MQTTWorker_Production.h
 * @brief 프로덕션용 MQTT 워커 클래스 - 완성본
 * @author PulseOne Development Team
 * @date 2025-01-22
 */

#ifndef WORKERS_PROTOCOL_MQTT_WORKER_PRODUCTION_H
#define WORKERS_PROTOCOL_MQTT_WORKER_PRODUCTION_H

#include "Workers/Protocol/MQTTWorker.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 타입 정의
// =============================================================================

struct MessageMetadata {
    std::string message_id;
    std::string correlation_id;
    uint64_t timestamp;
    std::string source;
    
    MessageMetadata() : timestamp(0) {}
};

struct AdvancedMqttConfig {
    bool circuit_breaker_enabled = false;
    int max_failures = 10;
    int circuit_timeout_seconds = 60;
    bool offline_mode_enabled = false;
    size_t max_offline_messages = 1000;
};

struct OfflineMessage {
    std::string topic;
    std::string payload;
    MqttQoS qos;
    bool retain;
    std::chrono::system_clock::time_point timestamp;
    int priority = 5;  // ✅ 우선순위 추가
    
    OfflineMessage(const std::string& t, const std::string& p, MqttQoS q = MqttQoS::AT_LEAST_ONCE, bool r = false, int prio = 5)
        : topic(t), payload(p), qos(q), retain(r), timestamp(std::chrono::system_clock::now()), priority(prio) {}
    
    // ✅ priority_queue를 위한 비교 연산자 (높은 우선순위가 먼저)
    bool operator<(const OfflineMessage& other) const {
        if (priority != other.priority) {
            return priority < other.priority;  // 높은 숫자가 높은 우선순위
        }
        return timestamp > other.timestamp;    // 같은 우선순위면 먼저 생성된 것이 우선
    }
    
    bool operator>(const OfflineMessage& other) const {
        return other < *this;
    }
    
    bool operator<=(const OfflineMessage& other) const {
        return !(other < *this);
    }
    
    bool operator>=(const OfflineMessage& other) const {
        return !(*this < other);
    }
    
    bool operator==(const OfflineMessage& other) const {
        return priority == other.priority && timestamp == other.timestamp;
    }
    
    bool operator!=(const OfflineMessage& other) const {
        return !(*this == other);
    }
};

struct PerformanceMetrics {
    // 메시지 관련 메트릭스
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_dropped{0};      // ✅ 추가
    
    // 데이터 전송량 메트릭스
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> peak_throughput_bps{0};
    std::atomic<uint64_t> avg_throughput_bps{0};    // ✅ 추가
    
    // 연결 관련 메트릭스
    std::atomic<uint32_t> connection_count{0};
    std::atomic<uint32_t> error_count{0};
    std::atomic<uint32_t> retry_count{0};
    std::atomic<uint64_t> connection_uptime_seconds{0};
    std::atomic<uint64_t> last_activity_time{0};
    
    // 성능 메트릭스
    std::atomic<uint32_t> avg_latency_ms{0};
    std::atomic<uint32_t> max_latency_ms{0};        // ✅ 추가
    std::atomic<uint32_t> min_latency_ms{999999};   // ✅ 추가
    
    // 큐 관련 메트릭스
    std::atomic<uint32_t> queue_size{0};
    std::atomic<uint32_t> max_queue_size{0};        // ✅ 추가
    
    // 시간 메트릭스
    std::atomic<uint64_t> last_error_time{0};
    std::atomic<uint32_t> throughput_samples{0};
    std::atomic<uint32_t> latency_samples{0};
    
    // 기본 생성자
    PerformanceMetrics() = default;
    
    // 복사 생성자 (모든 멤버 포함)
    PerformanceMetrics(const PerformanceMetrics& other) 
        : messages_sent(other.messages_sent.load())
        , messages_received(other.messages_received.load())
        , messages_dropped(other.messages_dropped.load())  // ✅ 추가
        , bytes_sent(other.bytes_sent.load())
        , bytes_received(other.bytes_received.load())
        , peak_throughput_bps(other.peak_throughput_bps.load())
        , avg_throughput_bps(other.avg_throughput_bps.load())  // ✅ 추가
        , connection_count(other.connection_count.load())
        , error_count(other.error_count.load())
        , retry_count(other.retry_count.load())
        , connection_uptime_seconds(other.connection_uptime_seconds.load())
        , last_activity_time(other.last_activity_time.load())
        , avg_latency_ms(other.avg_latency_ms.load())
        , max_latency_ms(other.max_latency_ms.load())        // ✅ 추가
        , min_latency_ms(other.min_latency_ms.load())        // ✅ 추가
        , queue_size(other.queue_size.load())
        , max_queue_size(other.max_queue_size.load())        // ✅ 추가
        , last_error_time(other.last_error_time.load())
        , throughput_samples(other.throughput_samples.load())
        , latency_samples(other.latency_samples.load()) {}
    
    // 대입 연산자 (모든 멤버 포함)
    PerformanceMetrics& operator=(const PerformanceMetrics& other) {
        if (this != &other) {
            messages_sent = other.messages_sent.load();
            messages_received = other.messages_received.load();
            messages_dropped = other.messages_dropped.load();  // ✅ 추가
            bytes_sent = other.bytes_sent.load();
            bytes_received = other.bytes_received.load();
            peak_throughput_bps = other.peak_throughput_bps.load();
            avg_throughput_bps = other.avg_throughput_bps.load();  // ✅ 추가
            connection_count = other.connection_count.load();
            error_count = other.error_count.load();
            retry_count = other.retry_count.load();
            connection_uptime_seconds = other.connection_uptime_seconds.load();
            last_activity_time = other.last_activity_time.load();
            avg_latency_ms = other.avg_latency_ms.load();
            max_latency_ms = other.max_latency_ms.load();        // ✅ 추가
            min_latency_ms = other.min_latency_ms.load();        // ✅ 추가
            queue_size = other.queue_size.load();
            max_queue_size = other.max_queue_size.load();        // ✅ 추가
            last_error_time = other.last_error_time.load();
            throughput_samples = other.throughput_samples.load();
            latency_samples = other.latency_samples.load();
        }
        return *this;
    }
    
    // 이동 생성자/대입 연산자 삭제
    PerformanceMetrics(PerformanceMetrics&&) = delete;
    PerformanceMetrics& operator=(PerformanceMetrics&&) = delete;
    
    void Reset() {
        messages_sent = 0;
        messages_received = 0;
        messages_dropped = 0;           // ✅ 추가
        bytes_sent = 0;
        bytes_received = 0;
        peak_throughput_bps = 0;
        avg_throughput_bps = 0;         // ✅ 추가
        connection_count = 0;
        error_count = 0;
        retry_count = 0;
        connection_uptime_seconds = 0;
        last_activity_time = 0;
        avg_latency_ms = 0;
        max_latency_ms = 0;             // ✅ 추가
        min_latency_ms = 999999;        // ✅ 추가
        queue_size = 0;
        max_queue_size = 0;             // ✅ 추가
        last_error_time = 0;
        throughput_samples = 0;
        latency_samples = 0;
    }
};

// =============================================================================
// MQTTWorkerProduction 클래스
// =============================================================================

class MQTTWorkerProduction : public MQTTWorker {
public:
    // 생성자 및 소멸자
    MQTTWorkerProduction(const Drivers::DeviceInfo& device_info,
                        std::shared_ptr<RedisClient> redis_client,
                        std::shared_ptr<InfluxClient> influx_client);
    
    virtual ~MQTTWorkerProduction();
    
    // MQTTWorker 오버라이드
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    
    // 프로덕션 전용 메서드들
    bool PublishWithPriority(const std::string& topic,
                           const std::string& payload,
                           int priority,
                           MqttQoS qos = MqttQoS::AT_LEAST_ONCE,
                           const MessageMetadata& metadata = MessageMetadata());
    
    size_t PublishBatch(const std::vector<OfflineMessage>& messages);
    
    bool PublishIfQueueAvailable(const std::string& topic,
                                const std::string& payload,
                                size_t max_queue_size = 1000);
    
    // 성능 모니터링
    PerformanceMetrics GetPerformanceMetrics() const;
    std::string GetPerformanceMetricsJson() const;
    std::string GetRealtimeDashboardData() const;
    std::string GetDetailedDiagnostics() const;
    
    bool IsConnectionHealthy() const;
    double GetSystemLoad() const;
    
    // 설정 및 제어
    void SetMetricsCollectionInterval(int interval_seconds);
    void SetMaxQueueSize(size_t max_size);
    void ResetMetrics();
    void SetBackpressureThreshold(double threshold);
    
    // 고급 기능
    void ConfigureAdvanced(const AdvancedMqttConfig& config);
    void EnableAutoFailover(const std::vector<std::string>& backup_brokers, int max_failures = 5);

protected:
    // 내부 스레드 함수들
    void MetricsCollectorLoop();
    void PriorityQueueProcessorLoop();
    void AlarmMonitorLoop();
    
    void CollectPerformanceMetrics();
    void UpdateThroughputMetrics();
    void UpdateLatencyMetrics(uint32_t latency_ms);
    
    // 내부 헬퍼 메서드들
    std::string SelectBroker();
    bool IsCircuitOpen() const;
    bool IsTopicAllowed(const std::string& topic) const;
    bool HandleBackpressure();
    void SaveOfflineMessage(const OfflineMessage& message);
    bool IsDuplicateMessage(const std::string& message_id);
    double CalculateErrorRate() const;

private:
    // 성능 메트릭스
    mutable std::mutex metrics_mutex_;
    mutable PerformanceMetrics performance_metrics_;
    
    // 스레드 관리
    std::thread metrics_thread_;
    std::thread priority_queue_thread_;
    std::thread alarm_monitor_thread_;
    
    std::atomic<bool> stop_metrics_thread_{false};
    std::atomic<bool> stop_priority_thread_{false};
    std::atomic<bool> stop_alarm_thread_{false};
    
    // 설정
    int metrics_collection_interval_seconds_{5};
    size_t max_queue_size_{1000};
    double backpressure_threshold_{0.8};
    
    // 고급 설정
    AdvancedMqttConfig advanced_config_;
    
    // 브로커 failover
    std::vector<std::string> backup_brokers_;
    std::vector<std::chrono::system_clock::time_point> broker_last_failure_;
    size_t current_broker_index_{0};
    
    // Circuit breaker
    int failure_count_{0};
    std::chrono::system_clock::time_point last_failure_time_;
    
    // 토픽 필터링
    std::vector<std::string> allowed_topics_;
    std::vector<std::string> blocked_topics_;
    
    // 오프라인 메시지
    std::queue<OfflineMessage> offline_messages_;
    mutable std::mutex offline_mutex_;
    
    // 중복 방지
    std::set<std::string> processed_message_ids_;
    mutable std::mutex dedup_mutex_;
    const size_t max_dedup_cache_size_{10000};
    
    // 우선순위 큐
    std::priority_queue<OfflineMessage> priority_message_queue_;
    mutable std::mutex priority_queue_mutex_;
    std::condition_variable priority_queue_cv_;
    
    // 처리량 계산용
    std::atomic<uint64_t> last_bytes_count_{0};
    std::chrono::steady_clock::time_point last_throughput_calculation_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MQTT_WORKER_PRODUCTION_H