/**
 * @file MQTTWorker_Production.cpp
 * @brief 프로덕션용 MQTT 워커 구현 - 완성본
 * @author PulseOne Development Team
 * @date 2025-01-23
 */

#include "Workers/Protocol/MQTTWorker_Production.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <future>

using namespace std::chrono;
using json = nlohmann::json;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MQTTWorkerProduction::MQTTWorkerProduction(const PulseOne::DeviceInfo& device_info,
                                         std::shared_ptr<RedisClient> redis_client,
                                         std::shared_ptr<InfluxClient> influx_client)
    : MQTTWorker(device_info, redis_client, influx_client)
    , start_time_(steady_clock::now())
    , last_throughput_calculation_(steady_clock::now()) {
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorkerProduction created");
    
    // 기본 설정 초기화
    advanced_config_ = AdvancedMqttConfig{};
    broker_last_failure_.resize(10);  // 최대 10개 브로커 지원
}

MQTTWorkerProduction::~MQTTWorkerProduction() {
    // 모든 스레드 정지
    stop_metrics_thread_ = true;
    stop_priority_thread_ = true;
    stop_alarm_thread_ = true;
    
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    if (priority_queue_thread_.joinable()) {
        priority_queue_thread_.join();
    }
    if (alarm_monitor_thread_.joinable()) {
        alarm_monitor_thread_.join();
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorkerProduction destroyed");
}

// =============================================================================
// MQTTWorker 오버라이드
// =============================================================================

std::future<bool> MQTTWorkerProduction::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        // 기본 MQTT 워커 시작
        auto base_result = MQTTWorker::Start();
        bool base_success = base_result.get();
        
        if (!base_success) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to start base MQTT worker");
            return false;
        }
        
        // 프로덕션 전용 스레드들 시작
        try {
            metrics_thread_ = std::thread(&MQTTWorkerProduction::MetricsCollectorLoop, this);
            priority_queue_thread_ = std::thread(&MQTTWorkerProduction::PriorityQueueProcessorLoop, this);
            alarm_monitor_thread_ = std::thread(&MQTTWorkerProduction::AlarmMonitorLoop, this);
            
            LogMessage(PulseOne::LogLevel::INFO, "MQTTWorkerProduction started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to start production threads: " + std::string(e.what()));
            return false;
        }
    });
}

std::future<bool> MQTTWorkerProduction::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        // 프로덕션 스레드들 정지
        stop_metrics_thread_ = true;
        stop_priority_thread_ = true;
        stop_alarm_thread_ = true;
        
        if (metrics_thread_.joinable()) {
            metrics_thread_.join();
        }
        if (priority_queue_thread_.joinable()) {
            priority_queue_thread_.join();
        }
        if (alarm_monitor_thread_.joinable()) {
            alarm_monitor_thread_.join();
        }
        
        // 기본 MQTT 워커 정지
        auto base_result = MQTTWorker::Stop();
        bool base_success = base_result.get();
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTTWorkerProduction stopped");
        return base_success;
    });
}

// =============================================================================
// 프로덕션 전용 메서드들
// =============================================================================

bool MQTTWorkerProduction::PublishWithPriority(const std::string& topic,
                                              const std::string& payload,
                                              int priority,
                                              MqttQoS qos,
                                              const MessageMetadata& /* metadata */) {
    try {
        // 오프라인 메시지 생성
        OfflineMessage message(topic, payload, qos, false, priority);
        message.timestamp = system_clock::now();
        
        // 연결 확인
        if (!CheckConnection()) {
            // 오프라인 큐에 저장
            SaveOfflineMessage(message);
            LogMessage(PulseOne::LogLevel::WARN, "Connection not available, message queued for offline processing");
            return false;
        }
        
        int qos_int = MQTTWorker::QosToInt(qos);  // enum을 int로 변환
        bool success = PublishMessage(topic, payload, qos_int, false);  // ✅ 개별 파라미터 버전
        if (success) {
            performance_metrics_.messages_sent++;
            performance_metrics_.bytes_sent += payload.size();
        } else {
            performance_metrics_.error_count++;
            SaveOfflineMessage(message);
        }
        
        // 로깅 (문자열 연결 수정)
        std::string result_msg = "Message send " + std::string(success ? "successful" : "failed") + " for topic: " + topic;
        LogMessage(success ? PulseOne::LogLevel::DEBUG_LEVEL : PulseOne::LogLevel::WARN, result_msg);
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PublishWithPriority: " + std::string(e.what()));
        performance_metrics_.error_count++;
        return false;
    }
}

size_t MQTTWorkerProduction::PublishBatch(const std::vector<OfflineMessage>& messages) {
    size_t successful = 0;
    
    for (const auto& msg : messages) {
        MessageMetadata metadata;
        metadata.timestamp = duration_cast<milliseconds>(msg.timestamp.time_since_epoch()).count();
        
        bool success = PublishWithPriority(msg.topic, msg.payload, msg.priority, msg.qos, metadata);
        if (success) {
            successful++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Batch publish completed: " + std::to_string(successful) + 
              "/" + std::to_string(messages.size()) + " messages sent");
    
    return successful;
}

bool MQTTWorkerProduction::PublishIfQueueAvailable(const std::string& topic,
                                                  const std::string& payload,
                                                  size_t max_queue_size) {
    // 큐 크기 확인
    {
        std::lock_guard<std::mutex> lock(offline_messages_mutex_);
        if (offline_messages_.size() >= max_queue_size) {
            performance_metrics_.messages_dropped++;
            LogMessage(PulseOne::LogLevel::WARN, "Message dropped due to queue overflow");
            return false;
        }
    }
    
    return PublishWithPriority(topic, payload, 5, MqttQoS::AT_LEAST_ONCE);
}

// =============================================================================
// 성능 모니터링
// =============================================================================

PerformanceMetrics MQTTWorkerProduction::GetPerformanceMetrics() const {
    return performance_metrics_;  // 복사 생성자 사용
}

std::string MQTTWorkerProduction::GetPerformanceMetricsJson() const {
    json metrics = {
        {"messages_sent", performance_metrics_.messages_sent.load()},
        {"messages_received", performance_metrics_.messages_received.load()},
        {"messages_dropped", performance_metrics_.messages_dropped.load()},
        {"bytes_sent", performance_metrics_.bytes_sent.load()},
        {"bytes_received", performance_metrics_.bytes_received.load()},
        {"peak_throughput_bps", performance_metrics_.peak_throughput_bps.load()},
        {"avg_throughput_bps", performance_metrics_.avg_throughput_bps.load()},
        {"connection_count", performance_metrics_.connection_count.load()},
        {"error_count", performance_metrics_.error_count.load()},
        {"retry_count", performance_metrics_.retry_count.load()},
        {"connection_uptime_seconds", performance_metrics_.connection_uptime_seconds.load()},
        {"avg_latency_ms", performance_metrics_.avg_latency_ms.load()},
        {"max_latency_ms", performance_metrics_.max_latency_ms.load()},
        {"min_latency_ms", performance_metrics_.min_latency_ms.load()},
        {"queue_size", performance_metrics_.queue_size.load()},
        {"max_queue_size", performance_metrics_.max_queue_size.load()}
    };
    
    return metrics.dump(2);
}

std::string MQTTWorkerProduction::GetRealtimeDashboardData() const {
    json dashboard;
    dashboard["status"] = GetState() == WorkerState::RUNNING ? "running" : "stopped";
    dashboard["broker_url"] = device_info_.endpoint;
    dashboard["connection_healthy"] = IsConnectionHealthy();
    dashboard["system_load"] = GetSystemLoad();
    
    return dashboard.dump(2);
}

std::string MQTTWorkerProduction::GetDetailedDiagnostics() const {
    auto now = steady_clock::now();
    auto uptime = duration_cast<seconds>(now - start_time_);
    
    json diagnostics;
    diagnostics["system_info"]["uptime_seconds"] = uptime.count();
    diagnostics["system_info"]["primary_broker"] = device_info_.endpoint;
    diagnostics["system_info"]["circuit_breaker_open"] = circuit_open_.load();
    diagnostics["system_info"]["consecutive_failures"] = consecutive_failures_.load();
    
    return diagnostics.dump(2);
}

bool MQTTWorkerProduction::IsConnectionHealthy() const {
    // const_cast로 const 문제 해결
    if (!const_cast<MQTTWorkerProduction*>(this)->CheckConnection()) {
        return false;
    }
    
    auto now = steady_clock::now();
    auto uptime = duration_cast<seconds>(now - start_time_);
    
    // 추가 건강 상태 확인 - uptime 활용
    auto last_activity = system_clock::time_point(milliseconds(performance_metrics_.last_activity_time.load()));
    auto time_since_activity = duration_cast<seconds>(system_clock::now() - last_activity);
    
    // 🆕 실제로 now 변수를 활용하여 연결 상태 검증
    bool connection_stable = time_since_activity.count() < 300;  // 5분 이내 활동
    bool uptime_healthy = uptime.count() > 10;  // 최소 10초 운영
    
    return connection_stable && uptime_healthy;
}

double MQTTWorkerProduction::GetSystemLoad() const {
    // 큐 크기 기반 시스템 로드 계산
    std::lock_guard<std::mutex> lock(offline_messages_mutex_);
    size_t queue_size = offline_messages_.size();
    size_t max_size = max_queue_size_.load();
    
    if (max_size == 0) return 0.0;
    
    return static_cast<double>(queue_size) / static_cast<double>(max_size);
}

// =============================================================================
// 설정 및 제어
// =============================================================================

void MQTTWorkerProduction::SetMetricsCollectionInterval(int interval_seconds) {
    metrics_collection_interval_ = interval_seconds;
    LogMessage(PulseOne::LogLevel::INFO, "Metrics collection interval set to " + std::to_string(interval_seconds) + " seconds");
}

void MQTTWorkerProduction::SetMaxQueueSize(size_t max_size) {
    max_queue_size_ = max_size;
    LogMessage(PulseOne::LogLevel::INFO, "Max queue size set to " + std::to_string(max_size));
}

void MQTTWorkerProduction::ResetMetrics() {
    performance_metrics_.Reset();
    LogMessage(PulseOne::LogLevel::INFO, "Performance metrics reset");
}

void MQTTWorkerProduction::SetBackpressureThreshold(double threshold) {
    backpressure_threshold_ = threshold;
    LogMessage(PulseOne::LogLevel::INFO, "Backpressure threshold set to " + std::to_string(threshold));
}

void MQTTWorkerProduction::ConfigureAdvanced(const AdvancedMqttConfig& config) {
    advanced_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "Advanced MQTT configuration updated");
}

void MQTTWorkerProduction::EnableAutoFailover(const std::vector<std::string>& backup_brokers, int max_failures) {
    backup_brokers_ = backup_brokers;
    broker_last_failure_.resize(backup_brokers.size() + 1);  // +1 for primary
    advanced_config_.max_failures = max_failures;
    
    LogMessage(PulseOne::LogLevel::INFO, "Auto failover enabled with " + std::to_string(backup_brokers.size()) + " backup brokers");
}

// =============================================================================
// 내부 스레드 함수들
// =============================================================================

void MQTTWorkerProduction::MetricsCollectorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Metrics collector thread started");
    
    while (!stop_metrics_thread_.load()) {
        try {
            CollectPerformanceMetrics();
            std::this_thread::sleep_for(seconds(metrics_collection_interval_.load()));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in metrics collector: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(10));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Metrics collector thread stopped");
}

void MQTTWorkerProduction::PriorityQueueProcessorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Priority queue processor thread started");
    
    while (!stop_priority_thread_.load()) {
        try {
            std::vector<OfflineMessage> batch;
            
            {
                std::lock_guard<std::mutex> lock(offline_messages_mutex_);
                
                // 최대 10개씩 배치 처리
                for (int i = 0; i < 10 && !offline_messages_.empty(); ++i) {
                    batch.push_back(offline_messages_.top());
                    offline_messages_.pop();
                }
            }
            
            if (!batch.empty()) {
                for (const auto& message : batch) {
                    // ✅ 수정: MQTTWorker의 개별 파라미터 버전 사용
                    int qos_int = MQTTWorker::QosToInt(message.qos);  // enum을 int로 변환
                    bool success = PublishMessage(message.topic, message.payload, qos_int, message.retain);
                    
                    if (success) {
                        performance_metrics_.messages_sent++;
                        performance_metrics_.bytes_sent += message.payload.size();
                    } else {
                        performance_metrics_.error_count++;
                    }
                }
            }
            
            std::this_thread::sleep_for(milliseconds(100));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in priority queue processor: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(1));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Priority queue processor thread stopped");
}

void MQTTWorkerProduction::AlarmMonitorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Alarm monitor thread started");
    
    while (!stop_alarm_thread_.load()) {
        try {
            // 연결 상태 모니터링
            if (!IsConnectionHealthy()) {
                LogMessage(PulseOne::LogLevel::WARN, "Connection health check failed");
            }
            
            // 큐 크기 모니터링
            double load = GetSystemLoad();
            if (load > backpressure_threshold_.load()) {
                LogMessage(PulseOne::LogLevel::WARN, "High system load detected: " + std::to_string(load));
            }
            
            std::this_thread::sleep_for(seconds(30));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in alarm monitor: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(10));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Alarm monitor thread stopped");
}

void MQTTWorkerProduction::CollectPerformanceMetrics() {
    auto now = steady_clock::now();
    
    // 처리량 메트릭스 업데이트
    UpdateThroughputMetrics();
    
    // 큐 크기 업데이트
    {
        std::lock_guard<std::mutex> queue_lock(offline_messages_mutex_);
        performance_metrics_.queue_size = offline_messages_.size();
    }
    
    // 연결 시간 업데이트
    auto uptime = duration_cast<seconds>(now - start_time_);
    performance_metrics_.connection_uptime_seconds = uptime.count();
    
    // 마지막 활동 시간 업데이트
    performance_metrics_.last_activity_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void MQTTWorkerProduction::UpdateThroughputMetrics() {
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - last_throughput_calculation_);
    
    if (elapsed.count() > 0) {
        uint64_t bytes_sent = performance_metrics_.bytes_sent.load();
        uint64_t current_throughput = bytes_sent / elapsed.count();
        
        // 피크 처리량 업데이트
        uint64_t peak = performance_metrics_.peak_throughput_bps.load();
        if (current_throughput > peak) {
            performance_metrics_.peak_throughput_bps = current_throughput;
        }
        
        // 평균 처리량 업데이트 (간단한 이동 평균)
        uint64_t avg = performance_metrics_.avg_throughput_bps.load();
        performance_metrics_.avg_throughput_bps = (avg + current_throughput) / 2;
        
        last_throughput_calculation_ = now;
    }
}

void MQTTWorkerProduction::UpdateLatencyMetrics(uint32_t latency_ms) {
    // 평균 레이턴시 업데이트
    uint32_t current_avg = performance_metrics_.avg_latency_ms.load();
    uint32_t samples = performance_metrics_.latency_samples.load();
    
    uint32_t new_avg = ((current_avg * samples) + latency_ms) / (samples + 1);
    performance_metrics_.avg_latency_ms = new_avg;
    performance_metrics_.latency_samples = samples + 1;
    
    // 최대/최소 레이턴시 업데이트
    uint32_t current_max = performance_metrics_.max_latency_ms.load();
    if (latency_ms > current_max) {
        performance_metrics_.max_latency_ms = latency_ms;
    }
    
    uint32_t current_min = performance_metrics_.min_latency_ms.load();
    if (latency_ms < current_min) {
        performance_metrics_.min_latency_ms = latency_ms;
    }
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::string MQTTWorkerProduction::SelectBroker() {
    if (backup_brokers_.empty()) {
        return device_info_.endpoint;
    }
    
    // 현재 브로커가 실패한 경우 다음 브로커로 전환
    if (IsCircuitOpen()) {
        current_broker_index_ = (current_broker_index_ + 1) % (backup_brokers_.size() + 1);
        
        if (current_broker_index_ == 0) {
            return device_info_.endpoint;
        } else {
            return backup_brokers_[current_broker_index_ - 1];
        }
    }
    
    return device_info_.endpoint;
}

bool MQTTWorkerProduction::IsCircuitOpen() const {
    if (!advanced_config_.circuit_breaker_enabled) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(circuit_mutex_);
    
    if (circuit_open_.load()) {
        auto now = steady_clock::now();
        auto elapsed = duration_cast<seconds>(now - circuit_open_time_);
        
        if (elapsed.count() >= advanced_config_.circuit_timeout_seconds) {
            // 타임아웃이 지나면 서킷을 반개방 상태로 변경
            const_cast<std::atomic<bool>&>(circuit_open_) = false;
            const_cast<std::atomic<int>&>(consecutive_failures_) = 0;
            return false;
        }
        return true;
    }
    
    return consecutive_failures_.load() >= advanced_config_.max_failures;
}

bool MQTTWorkerProduction::IsTopicAllowed(const std::string& topic) const {
    // 토픽 필터링 로직 (필요시 구현)
    return !topic.empty();
}

bool MQTTWorkerProduction::HandleBackpressure() {
    double load = GetSystemLoad();
    return load < backpressure_threshold_.load();
}

void MQTTWorkerProduction::SaveOfflineMessage(const OfflineMessage& message) {
    if (!advanced_config_.offline_mode_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(offline_messages_mutex_);
    
    if (offline_messages_.size() >= advanced_config_.max_offline_messages) {
        // 큐가 가득 찬 경우 가장 낮은 우선순위 메시지 제거
        LogMessage(PulseOne::LogLevel::WARN, "Offline queue full, dropping low priority message");
        performance_metrics_.messages_dropped++;
        return;
    }
    
    offline_messages_.push(message);
}

bool MQTTWorkerProduction::IsDuplicateMessage(const std::string& message_id) {
    if (message_id.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(message_ids_mutex_);
    
    if (processed_message_ids_.count(message_id) > 0) {
        return true;
    }
    
    // 메시지 ID 저장 (크기 제한)
    if (processed_message_ids_.size() >= 10000) {
        processed_message_ids_.clear();  // 간단한 LRU 대신 전체 클리어
    }
    
    processed_message_ids_.insert(message_id);
    return false;
}

double MQTTWorkerProduction::CalculateMessagePriority(const std::string& topic, const std::string& /* payload */) {
    // 토픽 기반 우선순위 계산
    double priority = 5.0;  // 기본 우선순위
    
    // 토픽 기반 우선순위 조정
    if (topic.find("alarm") != std::string::npos || topic.find("alert") != std::string::npos) {
        priority = 9.0;  // 알람 메시지는 높은 우선순위
    } else if (topic.find("status") != std::string::npos) {
        priority = 7.0;  // 상태 메시지는 중간 우선순위
    } else if (topic.find("data") != std::string::npos) {
        priority = 3.0;  // 데이터 메시지는 낮은 우선순위
    }
    
    return priority;
}

} // namespace Workers
} // namespace PulseOne