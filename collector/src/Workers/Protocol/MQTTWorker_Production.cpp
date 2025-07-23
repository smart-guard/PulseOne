/**
 * @file MQTTWorker_Production.cpp
 * @brief 프로덕션용 MQTT 워커 구현 - 완성본
 */

#include "Workers/Protocol/MQTTWorker_Production.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <future>

using namespace std::chrono;
using json = nlohmann::json;

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MQTTWorkerProduction::MQTTWorkerProduction(const Drivers::DeviceInfo& device_info,
                                         std::shared_ptr<RedisClient> redis_client,
                                         std::shared_ptr<InfluxClient> influx_client)
    : MQTTWorker(device_info, redis_client, influx_client)
    , start_time_(steady_clock::now())
    , last_throughput_calculation_(steady_clock::now()) {
    
    LogMessage(LogLevel::INFO, "MQTTWorkerProduction created");
    
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
    
    LogMessage(LogLevel::INFO, "MQTTWorkerProduction destroyed");
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
            LogMessage(LogLevel::ERROR, "Failed to start base MQTT worker");
            return false;
        }
        
        // 프로덕션 전용 스레드들 시작
        stop_metrics_thread_ = false;
        stop_priority_thread_ = false;
        stop_alarm_thread_ = false;
        
        start_time_ = steady_clock::now();
        
        metrics_thread_ = std::thread(&MQTTWorkerProduction::MetricsCollectorLoop, this);
        priority_queue_thread_ = std::thread(&MQTTWorkerProduction::PriorityQueueProcessorLoop, this);
        alarm_monitor_thread_ = std::thread(&MQTTWorkerProduction::AlarmMonitorLoop, this);
        
        LogMessage(LogLevel::INFO, "MQTTWorkerProduction started successfully");
        return true;
    });
}

std::future<bool> MQTTWorkerProduction::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        // 프로덕션 스레드들 중지
        stop_metrics_thread_ = true;
        stop_priority_thread_ = true;
        stop_alarm_thread_ = true;
        
        priority_queue_cv_.notify_all();
        
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
        
        LogMessage(LogLevel::INFO, "MQTTWorkerProduction stopped");
        return base_success;
    });
}

// =============================================================================
// 프로덕션 전용 메시지 처리
// =============================================================================

bool MQTTWorkerProduction::PublishWithPriority(const std::string& topic,
                                              const std::string& payload,
                                              int priority,
                                              MqttQoS qos,
                                              const MessageMetadata& metadata) {
    try {
        // 백프레셔 처리
        if (!HandleBackpressure()) {
            LogMessage(LogLevel::WARN, "Backpressure detected, message queued for later");
            SaveOfflineMessage(OfflineMessage(topic, payload, qos, false));
            return false;
        }
        
        // 중복 메시지 확인
        if (!metadata.message_id.empty() && IsDuplicateMessage(metadata.message_id)) {
            LogMessage(LogLevel::DEBUG, "Duplicate message filtered: " + metadata.message_id);
            return true;
        }
        
        // MQTT 메시지 생성
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(QosToInt(qos));
        
        // ✅ 올바른 MQTT Properties 사용법
        mqtt::properties props;
        if (!metadata.message_id.empty()) {
            props.add(mqtt::property(mqtt::property::USER_PROPERTY, "message_id", metadata.message_id));
        }
        props.add(mqtt::property(mqtt::property::USER_PROPERTY, "priority", std::to_string(priority)));
        if (!metadata.correlation_id.empty()) {
            props.add(mqtt::property(mqtt::property::USER_PROPERTY, "correlation_id", metadata.correlation_id));
        }
        msg->set_properties(props);
        
        // 메시지 발송
        auto start_time = steady_clock::now();
        auto token = mqtt_client_->publish(msg);
        bool success = token->wait_for(std::chrono::seconds(10));
        auto end_time = steady_clock::now();
        
        // 레이턴시 계산
        auto latency = duration_cast<milliseconds>(end_time - start_time).count();
        UpdateLatencyMetrics(static_cast<uint32_t>(latency));
        
        // 메트릭스 업데이트
        if (success) {
            performance_metrics_.messages_sent++;
            performance_metrics_.bytes_sent += payload.length();
            
            // 중복 방지 캐시에 추가
            if (!metadata.message_id.empty()) {
                std::lock_guard<std::mutex> lock(dedup_mutex_);
                processed_message_ids_.insert(metadata.message_id);
                
                // 캐시 크기 제한
                if (processed_message_ids_.size() > max_dedup_cache_size_) {
                    auto it = processed_message_ids_.begin();
                    std::advance(it, max_dedup_cache_size_ / 2);
                    processed_message_ids_.erase(processed_message_ids_.begin(), it);
                }
            }
        } else {
            performance_metrics_.error_count++;
            failure_count_++;
            last_failure_time_ = system_clock::now();
            
            // 오프라인 메시지로 저장
            if (advanced_config_.offline_mode_enabled) {
                SaveOfflineMessage(OfflineMessage(topic, payload, qos, false));
            }
        }
        
        UpdateMqttStats("send", success);
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in priority publish: " + std::string(e.what()));
        performance_metrics_.error_count++;
        return false;
    }
}

size_t MQTTWorkerProduction::PublishBatch(const std::vector<OfflineMessage>& messages) {
    size_t success_count = 0;
    
    for (const auto& msg : messages) {
        MessageMetadata metadata;
        metadata.timestamp = duration_cast<milliseconds>(msg.timestamp.time_since_epoch()).count();
        
        bool success = PublishWithPriority(msg.topic, msg.payload, 5, msg.qos, metadata);
        if (success) {
            success_count++;
        }
        
        // 배치 처리 간 짧은 대기 (과부하 방지)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LogMessage(LogLevel::INFO, "Batch publish completed: " + 
               std::to_string(success_count) + "/" + std::to_string(messages.size()));
    
    return success_count;
}

bool MQTTWorkerProduction::PublishIfQueueAvailable(const std::string& topic,
                                                  const std::string& payload,
                                                  size_t max_queue_size) {
    if (performance_metrics_.queue_size.load() >= max_queue_size) {
        performance_metrics_.messages_dropped++;
        return false;
    }
    
    return PublishMessage(topic, payload);
}

// =============================================================================
// 성능 모니터링 및 메트릭스
// =============================================================================

PerformanceMetrics MQTTWorkerProduction::GetPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return performance_metrics_;
}

std::string MQTTWorkerProduction::GetPerformanceMetricsJson() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    json metrics;
    
    // 기본 메트릭스
    metrics["messages"] = {
        {"sent", performance_metrics_.messages_sent.load()},
        {"received", performance_metrics_.messages_received.load()},
        {"dropped", performance_metrics_.messages_dropped.load()},
        {"queued", performance_metrics_.queue_size.load()}
    };
    
    // 데이터 전송량
    metrics["throughput"] = {
        {"bytes_sent", performance_metrics_.bytes_sent.load()},
        {"bytes_received", performance_metrics_.bytes_received.load()},
        {"peak_bps", performance_metrics_.peak_throughput_bps.load()},
        {"samples", performance_metrics_.throughput_samples.load()}
    };
    
    // 연결 상태
    metrics["connection"] = {
        {"count", performance_metrics_.connection_count.load()},
        {"uptime_seconds", performance_metrics_.connection_uptime_seconds.load()},
        {"is_healthy", IsConnectionHealthy()}
    };
    
    // 성능 지표
    metrics["performance"] = {
        {"avg_latency_ms", performance_metrics_.avg_latency_ms.load()},
        {"error_count", performance_metrics_.error_count.load()},
        {"retry_count", performance_metrics_.retry_count.load()},
        {"error_rate_percent", CalculateErrorRate()}
    };
    
    // 시간 정보
    auto now = system_clock::now();
    auto uptime = duration_cast<seconds>(steady_clock::now() - start_time_).count();
    metrics["timing"] = {
        {"uptime_seconds", uptime},
        {"last_activity", performance_metrics_.last_activity_time.load()},
        {"timestamp", duration_cast<milliseconds>(now.time_since_epoch()).count()}
    };
    
    return metrics.dump(2);
}

std::string MQTTWorkerProduction::GetRealtimeDashboardData() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    json dashboard;
    
    // ✅ device_info_ 멤버 확인 후 수정
    dashboard["worker_info"] = {
        {"id", device_info_.id},           // device_id → id로 수정
        {"name", device_info_.name},       // 또는 적절한 멤버명 사용
        {"type", "MQTT_Production"},
        {"state", static_cast<int>(GetState())},
        {"version", "1.0.0"}
    };
    
    // 연결 상태
    bool is_connected = IsConnectionHealthy();
    dashboard["connection"] = {
        {"status", is_connected ? "connected" : "disconnected"},
        {"broker_url", broker_url_},
        {"uptime_seconds", performance_metrics_.connection_uptime_seconds.load()},
        {"circuit_breaker", IsCircuitOpen() ? "open" : "closed"}
    };
    
    // 실시간 메트릭스
    dashboard["metrics"] = {
        {"messages_sent", performance_metrics_.messages_sent.load()},
        {"messages_received", performance_metrics_.messages_received.load()},
        {"queue_size", performance_metrics_.queue_size.load()},
        {"error_count", performance_metrics_.error_count.load()},
        {"throughput_bps", performance_metrics_.peak_throughput_bps.load()},
        {"avg_latency_ms", performance_metrics_.avg_latency_ms.load()}
    };
    
    // 시스템 상태
    double system_load = GetSystemLoad();
    dashboard["system"] = {
        {"load", system_load},
        {"healthy", is_connected && system_load < 0.8 && performance_metrics_.error_count.load() < 100},
        {"backpressure_active", system_load > backpressure_threshold_},
        {"offline_messages", offline_messages_.size()}
    };
    
    // 알람 상태
    dashboard["alarms"] = {
        {"high_error_rate", CalculateErrorRate() > 10.0},
        {"high_latency", performance_metrics_.avg_latency_ms.load() > 1000},
        {"queue_overflow", performance_metrics_.queue_size.load() > max_queue_size_ * 0.9},
        {"connection_unstable", failure_count_ > 5}
    };
    
    return dashboard.dump(2);
}

std::string MQTTWorkerProduction::GetDetailedDiagnostics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    json diagnostics;
    
    // 시스템 정보
    diagnostics["system_info"] = {
        {"start_time", duration_cast<milliseconds>(
            system_clock::time_point(start_time_.time_since_epoch()).time_since_epoch()).count()},
        {"running_time_seconds", duration_cast<seconds>(steady_clock::now() - start_time_).count()},
        {"thread_count", 3},  // metrics, priority_queue, alarm_monitor
        {"memory_usage_estimate", sizeof(*this)}
    };
    
    // 설정 정보
    diagnostics["configuration"] = {
        {"metrics_interval", metrics_collection_interval_seconds_},
        {"max_queue_size", max_queue_size_},
        {"backpressure_threshold", backpressure_threshold_},
        {"circuit_breaker_enabled", advanced_config_.circuit_breaker_enabled},
        {"offline_mode_enabled", advanced_config_.offline_mode_enabled}
    };
    
    // 브로커 정보
    diagnostics["broker_info"] = {
        {"primary_broker", broker_url_},
        {"backup_brokers_count", backup_brokers_.size()},
        {"current_broker_index", current_broker_index_},
        {"failure_count", failure_count_}
    };
    
    // 상세 성능 분석
    diagnostics["performance_analysis"] = {
        {"message_throughput_per_second", 
         performance_metrics_.throughput_samples.load() > 0 ? 
         performance_metrics_.messages_sent.load() / (duration_cast<seconds>(steady_clock::now() - start_time_).count() + 1) : 0},
        {"average_message_size", 
         performance_metrics_.messages_sent.load() > 0 ? 
         performance_metrics_.bytes_sent.load() / performance_metrics_.messages_sent.load() : 0},
        {"success_rate_percent", 100.0 - CalculateErrorRate()},
        {"retry_rate_percent", 
         performance_metrics_.messages_sent.load() > 0 ? 
         (double)performance_metrics_.retry_count.load() / performance_metrics_.messages_sent.load() * 100.0 : 0.0}
    };
    
    return diagnostics.dump(2);
}

bool MQTTWorkerProduction::IsConnectionHealthy() const {
    if (!mqtt_client_ || !mqtt_client_->is_connected()) {
        return false;
    }
    
    // Circuit breaker 상태 확인
    if (IsCircuitOpen()) {
        return false;
    }
    
    // 최근 에러율 확인
    if (CalculateErrorRate() > 50.0) {
        return false;
    }
    
    return true;
}

double MQTTWorkerProduction::GetSystemLoad() const {
    // 큐 사용률과 에러율을 기반으로 시스템 부하 계산
    double queue_load = static_cast<double>(performance_metrics_.queue_size.load()) / max_queue_size_;
    double error_load = CalculateErrorRate() / 100.0;
    
    return std::min(1.0, (queue_load * 0.7) + (error_load * 0.3));
}

// =============================================================================
// 설정 및 제어
// =============================================================================

void MQTTWorkerProduction::SetMetricsCollectionInterval(int interval_seconds) {
    if (interval_seconds > 0 && interval_seconds <= 300) {
        metrics_collection_interval_seconds_ = interval_seconds;
        LogMessage(LogLevel::INFO, "Metrics collection interval set to " + 
                   std::to_string(interval_seconds) + " seconds");
    }
}

void MQTTWorkerProduction::SetMaxQueueSize(size_t max_size) {
    max_queue_size_ = max_size;
    LogMessage(LogLevel::INFO, "Max queue size set to " + std::to_string(max_size));
}

void MQTTWorkerProduction::ResetMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    performance_metrics_.Reset();
    failure_count_ = 0;
    
    LogMessage(LogLevel::INFO, "Performance metrics reset");
}

void MQTTWorkerProduction::SetBackpressureThreshold(double threshold) {
    if (threshold >= 0.0 && threshold <= 1.0) {
        backpressure_threshold_ = threshold;
        LogMessage(LogLevel::INFO, "Backpressure threshold set to " + 
                   std::to_string(threshold));
    }
}

void MQTTWorkerProduction::ConfigureAdvanced(const AdvancedMqttConfig& config) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    advanced_config_ = config;
    LogMessage(LogLevel::INFO, "Advanced MQTT configuration applied");
}

void MQTTWorkerProduction::EnableAutoFailover(const std::vector<std::string>& backup_brokers, 
                                             int max_failures) {
    backup_brokers_ = backup_brokers;
    advanced_config_.max_failures = max_failures;
    broker_last_failure_.resize(backup_brokers.size());
    
    LogMessage(LogLevel::INFO, "Auto failover enabled with " + 
               std::to_string(backup_brokers.size()) + " backup brokers");
}

// =============================================================================
// 내부 스레드 함수들
// =============================================================================

void MQTTWorkerProduction::MetricsCollectorLoop() {
    LogMessage(LogLevel::INFO, "Metrics collector thread started");
    
    while (!stop_metrics_thread_) {
        try {
            CollectPerformanceMetrics();
            UpdateThroughputMetrics();
            
            std::this_thread::sleep_for(std::chrono::seconds(metrics_collection_interval_seconds_));
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Metrics collector error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LogMessage(LogLevel::INFO, "Metrics collector thread stopped");
}

void MQTTWorkerProduction::PriorityQueueProcessorLoop() {
    LogMessage(LogLevel::INFO, "Priority queue processor thread started");
    
    while (!stop_priority_thread_) {
        try {
            std::unique_lock<std::mutex> lock(priority_queue_mutex_);
            
            // 큐에 메시지가 있을 때까지 대기
            priority_queue_cv_.wait(lock, [this] { 
                return !priority_message_queue_.empty() || stop_priority_thread_; 
            });
            
            if (stop_priority_thread_) break;
            
            // 우선순위가 높은 메시지부터 처리
            while (!priority_message_queue_.empty() && !stop_priority_thread_) {
                auto message = priority_message_queue_.top();
                priority_message_queue_.pop();
                
                lock.unlock();
                
                // 메시지 처리
                MessageMetadata metadata;
                metadata.timestamp = duration_cast<milliseconds>(
                    message.timestamp.time_since_epoch()).count();
                
                PublishWithPriority(message.topic, message.payload, 8, message.qos, metadata);
                
                lock.lock();
            }
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Priority queue processor error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LogMessage(LogLevel::INFO, "Priority queue processor thread stopped");
}

void MQTTWorkerProduction::AlarmMonitorLoop() {
    LogMessage(LogLevel::INFO, "Alarm monitor thread started");
    
    while (!stop_alarm_thread_) {
        try {
            // 알람 조건 확인
            double error_rate = CalculateErrorRate();
            uint32_t avg_latency = performance_metrics_.avg_latency_ms.load();
            uint32_t queue_size = performance_metrics_.queue_size.load();
            
            // 높은 에러율 알람
            if (error_rate > 20.0) {
                LogMessage(LogLevel::WARN, "High error rate detected: " + 
                           std::to_string(error_rate) + "%");
            }
            
            // 높은 레이턴시 알람
            if (avg_latency > 2000) {
                LogMessage(LogLevel::WARN, "High latency detected: " + 
                           std::to_string(avg_latency) + "ms");
            }
            
            // 큐 오버플로우 알람
            if (queue_size > max_queue_size_ * 0.9) {
                LogMessage(LogLevel::WARN, "Queue near capacity: " + 
                           std::to_string(queue_size) + "/" + std::to_string(max_queue_size_));
            }
            
            // Circuit breaker 확인
            if (IsCircuitOpen()) {
                LogMessage(LogLevel::ERROR, "Circuit breaker is OPEN - blocking requests");
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30)); // 30초마다 확인
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Alarm monitor error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    LogMessage(LogLevel::INFO, "Alarm monitor thread stopped");
}

void MQTTWorkerProduction::CollectPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // 연결 업타임 업데이트
    if (IsConnectionHealthy()) {
        auto uptime = duration_cast<seconds>(steady_clock::now() - start_time_).count();
        performance_metrics_.connection_uptime_seconds = uptime;
    }
    
    // 큐 크기 업데이트
    {
        std::lock_guard<std::mutex> queue_lock(message_queue_mutex_);
        performance_metrics_.queue_size = incoming_messages_.size();
        
        // 최대 큐 크기 추적
        uint32_t current_queue_size = performance_metrics_.queue_size.load();
        uint32_t max_queue = performance_metrics_.max_queue_size.load();
        if (current_queue_size > max_queue) {
            performance_metrics_.max_queue_size = current_queue_size;
        }
    }
    
    // 활동 시간 업데이트
    performance_metrics_.last_activity_time = 
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void MQTTWorkerProduction::UpdateThroughputMetrics() {
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - last_throughput_calculation_);
    
    if (elapsed.count() > 1000) { // 1초마다 업데이트
        uint64_t current_bytes = performance_metrics_.bytes_sent.load() + 
                                performance_metrics_.bytes_received.load();
        uint64_t byte_diff = current_bytes - last_bytes_count_;
        
        // 초당 바이트 수 계산
        uint64_t throughput_bps = (byte_diff * 1000) / elapsed.count();
        
        // 평균 처리량 업데이트 (이동 평균)
        uint64_t samples = performance_metrics_.throughput_samples.load();
        uint64_t current_avg = performance_metrics_.avg_throughput_bps.load();
        uint64_t new_avg = (current_avg * samples + throughput_bps) / (samples + 1);
        performance_metrics_.avg_throughput_bps = new_avg;
        
        // 피크 처리량 업데이트
        uint64_t current_peak = performance_metrics_.peak_throughput_bps.load();
        if (throughput_bps > current_peak) {
            performance_metrics_.peak_throughput_bps = throughput_bps;
        }
        
        last_bytes_count_ = current_bytes;
        last_throughput_calculation_ = now;
        performance_metrics_.throughput_samples++;
    }
}

void MQTTWorkerProduction::UpdateLatencyMetrics(uint32_t latency_ms) {
    // 최소/최대 레이턴시 업데이트
    uint32_t current_min = performance_metrics_.min_latency_ms.load();
    uint32_t current_max = performance_metrics_.max_latency_ms.load();
    
    if (latency_ms < current_min) {
        performance_metrics_.min_latency_ms = latency_ms;
    }
    if (latency_ms > current_max) {
        performance_metrics_.max_latency_ms = latency_ms;
    }
    
    // 이동 평균으로 평균 레이턴시 업데이트
    uint32_t samples = performance_metrics_.latency_samples.load();
    uint32_t current_avg = performance_metrics_.avg_latency_ms.load();
    uint32_t new_avg = (current_avg * samples + latency_ms) / (samples + 1);
    
    performance_metrics_.avg_latency_ms = new_avg;
    performance_metrics_.latency_samples++;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::string MQTTWorkerProduction::SelectBroker() {
    if (backup_brokers_.empty()) {
        return broker_url_;
    }
    
    // 실패하지 않은 브로커 찾기
    auto now = system_clock::now();
    for (size_t i = 0; i < backup_brokers_.size(); ++i) {
        if (broker_last_failure_.size() > i) {
            auto time_since_failure = duration_cast<seconds>(now - broker_last_failure_[i]).count();
            if (time_since_failure > advanced_config_.circuit_timeout_seconds) {
                current_broker_index_ = i;
                return backup_brokers_[i];
            }
        }
    }
    
    // 모든 브로커가 실패한 경우 기본 브로커 반환
    return broker_url_;
}

bool MQTTWorkerProduction::IsCircuitOpen() const {
    if (!advanced_config_.circuit_breaker_enabled) {
        return false;
    }
    
    if (failure_count_ < advanced_config_.max_failures) {
        return false;
    }
    
    auto now = system_clock::now();
    auto time_since_failure = duration_cast<seconds>(now - last_failure_time_).count();
    
    return time_since_failure < advanced_config_.circuit_timeout_seconds;
}

bool MQTTWorkerProduction::IsTopicAllowed(const std::string& topic) const {
    // 블록된 토픽 확인
    for (const auto& blocked : blocked_topics_) {
        if (topic.find(blocked) != std::string::npos) {
            return false;
        }
    }
    
    // 허용된 토픽이 설정되어 있다면 확인
    if (!allowed_topics_.empty()) {
        for (const auto& allowed : allowed_topics_) {
            if (topic.find(allowed) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    return true;
}

bool MQTTWorkerProduction::HandleBackpressure() {
    double system_load = GetSystemLoad();
    
    if (system_load > backpressure_threshold_) {
        // 백프레셔 활성화 - 메시지 처리 속도 조절
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>((system_load - backpressure_threshold_) * 1000)));
        return false;
    }
    
    return true;
}

void MQTTWorkerProduction::SaveOfflineMessage(const OfflineMessage& message) {
    if (!advanced_config_.offline_mode_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(offline_mutex_);
    
    // 최대 오프라인 메시지 수 제한
    if (offline_messages_.size() >= advanced_config_.max_offline_messages) {
        offline_messages_.pop(); // 가장 오래된 메시지 제거
    }
    
    offline_messages_.push(message);
    
    LogMessage(LogLevel::DEBUG, "Offline message saved: " + message.topic);
}

bool MQTTWorkerProduction::IsDuplicateMessage(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(dedup_mutex_);
    return processed_message_ids_.find(message_id) != processed_message_ids_.end();
}

double MQTTWorkerProduction::CalculateErrorRate() const {
    uint64_t total_messages = performance_metrics_.messages_sent.load() + 
                             performance_metrics_.messages_received.load();
    
    if (total_messages == 0) {
        return 0.0;
    }
    
    return (double)performance_metrics_.error_count.load() / total_messages * 100.0;
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne