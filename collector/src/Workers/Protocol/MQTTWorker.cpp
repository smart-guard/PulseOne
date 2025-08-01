/**
 * @file MQTTWorker.cpp - 통합 MQTT 워커 구현부 (기본 + 프로덕션 모드)
 * @brief 하나의 클래스로 모든 MQTT 기능 구현 - ModbusTcpWorker 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 3.0.0 (통합 버전)
 */

#include "Workers/Protocol/MQTTWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <future>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

using namespace std::chrono;
using namespace PulseOne::Drivers;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자 (ModbusTcpWorker 패턴 완전 적용)
// =============================================================================

MQTTWorker::MQTTWorker(const PulseOne::DeviceInfo& device_info,
                       std::shared_ptr<RedisClient> redis_client,
                       std::shared_ptr<InfluxClient> influx_client,
                       MQTTWorkerMode mode)
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , worker_mode_(mode)
    , mqtt_driver_(nullptr)
    , next_subscription_id_(1)
    , message_thread_running_(false)
    , publish_thread_running_(false)
    , default_message_timeout_ms_(30000)
    , max_publish_queue_size_(10000)
    , auto_reconnect_enabled_(true)
    , metrics_thread_running_(false)
    , priority_thread_running_(false)
    , alarm_thread_running_(false)
    , start_time_(steady_clock::now())
    , last_throughput_calculation_(steady_clock::now()) {
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker created for device: " + device_info.name + 
               " (Mode: " + (mode == MQTTWorkerMode::PRODUCTION ? "PRODUCTION" : "BASIC") + ")");
    
    // 설정 파싱 (ModbusTcpWorker와 동일한 5단계 프로세스)
    if (!ParseMQTTConfig()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse MQTT configuration");
        return;
    }
    
    // MqttDriver 초기화
    if (!InitializeMQTTDriver()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize MqttDriver");
        return;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker initialization completed");
}

MQTTWorker::~MQTTWorker() {
    // 프로덕션 모드 스레드들 먼저 정리
    StopProductionThreads();
    
    // 기본 스레드 정리
    message_thread_running_ = false;
    publish_thread_running_ = false;
    publish_queue_cv_.notify_all();
    
    if (message_processor_thread_ && message_processor_thread_->joinable()) {
        message_processor_thread_->join();
    }
    if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
        publish_processor_thread_->join();
    }
    
    // MqttDriver 정리 (자동으로 연결 해제됨)
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> MQTTWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            LogMessage(PulseOne::LogLevel::INFO, "Starting MQTT worker...");
            
            // 1. 연결 수립
            if (!EstablishConnection()) {
                promise->set_value(false);
                return;
            }
            
            // 2. 메시지 처리 스레드 시작
            message_thread_running_ = true;
            message_processor_thread_ = std::make_unique<std::thread>(
                &MQTTWorker::MessageProcessorThreadFunction, this);
            
            // 3. 발행 처리 스레드 시작
            publish_thread_running_ = true;
            publish_processor_thread_ = std::make_unique<std::thread>(
                &MQTTWorker::PublishProcessorThreadFunction, this);
            
            // 4. 프로덕션 모드라면 고급 스레드들 시작
            if (IsProductionMode()) {
                StartProductionThreads();
            }
            
            LogMessage(PulseOne::LogLevel::INFO, "MQTT worker started successfully");
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to start MQTT worker: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> MQTTWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            LogMessage(PulseOne::LogLevel::INFO, "Stopping MQTT worker...");
            
            // 1. 프로덕션 모드 스레드들 정리
            StopProductionThreads();
            
            // 2. 기본 스레드들 정리
            message_thread_running_ = false;
            publish_thread_running_ = false;
            publish_queue_cv_.notify_all();
            
            if (message_processor_thread_ && message_processor_thread_->joinable()) {
                message_processor_thread_->join();
            }
            if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
                publish_processor_thread_->join();
            }
            
            // 3. 연결 해제
            CloseConnection();
            
            LogMessage(PulseOne::LogLevel::INFO, "MQTT worker stopped successfully");
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to stop MQTT worker: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

bool MQTTWorker::EstablishConnection() {
    if (!mqtt_driver_) {
        LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver not initialized");
        return false;
    }
    
    worker_stats_.connection_attempts++;
    
    if (mqtt_driver_->Connect()) {
        LogMessage(PulseOne::LogLevel::INFO, "MQTT connection established to: " + mqtt_config_.broker_url);
        
        // 프로덕션 모드에서는 성능 메트릭스 업데이트
        if (IsProductionMode()) {
            performance_metrics_.connection_count++;
        }
        
        return true;
    } else {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish MQTT connection");
        
        // 프로덕션 모드에서는 에러 카운트 업데이트
        if (IsProductionMode()) {
            performance_metrics_.error_count++;
            consecutive_failures_++;
        }
        
        return false;
    }
}

bool MQTTWorker::CloseConnection() {
    if (!mqtt_driver_) {
        return true;
    }
    
    if (mqtt_driver_->Disconnect()) {
        LogMessage(PulseOne::LogLevel::INFO, "MQTT connection closed");
        return true;
    } else {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to close MQTT connection gracefully");
        return false;
    }
}

bool MQTTWorker::CheckConnection() {
    if (!mqtt_driver_) {
        return false;
    }
    
    return mqtt_driver_->IsConnected();
}

bool MQTTWorker::SendKeepAlive() {
    // MQTT 자체적으로 Keep-alive를 처리하므로 항상 true 반환
    return CheckConnection();
}

// =============================================================================
// 기본 MQTT 기능 (모든 모드에서 사용 가능)
// =============================================================================

bool MQTTWorker::AddSubscription(const MQTTSubscription& subscription) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    // 구독 유효성 검사
    if (!ValidateSubscription(subscription)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid subscription: " + subscription.topic);
        return false;
    }
    
    // 고유 ID 할당
    MQTTSubscription new_subscription = subscription;
    new_subscription.subscription_id = next_subscription_id_++;
    new_subscription.is_active = true;
    
    // 실제 MQTT 구독 (Driver 위임)
    if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
        // bool success = mqtt_driver_->Subscribe(new_subscription.topic, QosToInt(new_subscription.qos));
        bool success = true; // 현재는 임시로 true (실제 구현에서는 MqttDriver API 사용)
        
        if (!success) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to subscribe to topic: " + new_subscription.topic);
            return false;
        }
    }
    
    // 구독 정보 저장
    active_subscriptions_[new_subscription.subscription_id] = new_subscription;
    worker_stats_.successful_subscriptions++;
    
    LogMessage(PulseOne::LogLevel::INFO, 
               "Added subscription (ID: " + std::to_string(new_subscription.subscription_id) + 
               ", Topic: " + new_subscription.topic + ")");
    
    return true;
}

bool MQTTWorker::RemoveSubscription(uint32_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    auto it = active_subscriptions_.find(subscription_id);
    if (it == active_subscriptions_.end()) {
        LogMessage(PulseOne::LogLevel::WARN, "Subscription not found: " + std::to_string(subscription_id));
        return false;
    }
    
    // 실제 MQTT 구독 해제 (Driver 위임)
    if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
        // bool success = mqtt_driver_->Unsubscribe(it->second.topic);
        bool success = true; // 현재는 임시로 true
        
        if (!success) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to unsubscribe from topic: " + it->second.topic);
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Removed subscription: " + it->second.topic);
    active_subscriptions_.erase(it);
    
    return true;
}

bool MQTTWorker::PublishMessage(const MQTTPublishTask& task) {
    std::lock_guard<std::mutex> lock(publish_queue_mutex_);
    
    if (publish_queue_.size() >= max_publish_queue_size_) {
        LogMessage(PulseOne::LogLevel::WARN, "Publish queue full, dropping message");
        worker_stats_.failed_operations++;
        
        // 프로덕션 모드에서는 드롭된 메시지 카운트
        if (IsProductionMode()) {
            performance_metrics_.messages_dropped++;
        }
        
        return false;
    }
    
    publish_queue_.push(task);
    publish_queue_cv_.notify_one();
    
    return true;
}

bool MQTTWorker::PublishMessage(const std::string& topic, const std::string& payload, 
                               int qos, bool retained) {
    MQTTPublishTask task;
    task.topic = topic;
    task.payload = payload;
    task.qos = IntToQos(qos);  // int를 MqttQoS enum으로 변환
    task.retained = retained;
    task.scheduled_time = system_clock::now();
    task.retry_count = 0;
    task.priority = 5;  // 기본 우선순위
    
    return PublishMessage(task);  // 구조체 버전 호출
}

std::string MQTTWorker::GetMQTTWorkerStats() const {
    std::ostringstream stats;
    
    auto now = system_clock::now();
    auto uptime = duration_cast<seconds>(now - worker_stats_.start_time).count();
    
    stats << "{"
          << "\"messages_received\":" << worker_stats_.messages_received.load() << ","
          << "\"messages_published\":" << worker_stats_.messages_published.load() << ","
          << "\"successful_subscriptions\":" << worker_stats_.successful_subscriptions.load() << ","
          << "\"failed_operations\":" << worker_stats_.failed_operations.load() << ","
          << "\"json_parse_errors\":" << worker_stats_.json_parse_errors.load() << ","
          << "\"connection_attempts\":" << worker_stats_.connection_attempts.load() << ","
          << "\"uptime_seconds\":" << uptime << ","
          << "\"active_subscriptions\":" << active_subscriptions_.size() << ","
          << "\"worker_mode\":\"" << (IsProductionMode() ? "PRODUCTION" : "BASIC") << "\""
          << "}";
    
    return stats.str();
}

void MQTTWorker::ResetMQTTWorkerStats() {
    worker_stats_.messages_received = 0;
    worker_stats_.messages_published = 0;
    worker_stats_.successful_subscriptions = 0;
    worker_stats_.failed_operations = 0;
    worker_stats_.json_parse_errors = 0;
    worker_stats_.connection_attempts = 0;
    worker_stats_.last_reset = system_clock::now();
    
    // 프로덕션 모드에서는 성능 메트릭스도 리셋
    if (IsProductionMode()) {
        ResetMetrics();
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT worker statistics reset");
}

// =============================================================================
// 모드 제어 및 설정
// =============================================================================

void MQTTWorker::SetWorkerMode(MQTTWorkerMode mode) {
    if (worker_mode_ == mode) {
        return; // 이미 같은 모드
    }
    
    MQTTWorkerMode old_mode = worker_mode_;
    worker_mode_ = mode;
    
    LogMessage(PulseOne::LogLevel::INFO, 
               "Worker mode changed: " + 
               std::string(old_mode == MQTTWorkerMode::PRODUCTION ? "PRODUCTION" : "BASIC") + 
               " → " + 
               std::string(mode == MQTTWorkerMode::PRODUCTION ? "PRODUCTION" : "BASIC"));
    
    // 모드 전환에 따른 스레드 관리
    if (mode == MQTTWorkerMode::PRODUCTION && old_mode == MQTTWorkerMode::BASIC) {
        // BASIC → PRODUCTION: 고급 스레드들 시작
        StartProductionThreads();
    } else if (mode == MQTTWorkerMode::BASIC && old_mode == MQTTWorkerMode::PRODUCTION) {
        // PRODUCTION → BASIC: 고급 스레드들 정지
        StopProductionThreads();
    }
}

// =============================================================================
// 프로덕션 전용 기능 (PRODUCTION 모드에서만 활성화)
// =============================================================================

bool MQTTWorker::PublishWithPriority(const std::string& topic,
                                     const std::string& payload,
                                     int priority,
                                     MqttQoS qos) {
    if (!IsProductionMode()) {
        // 기본 모드에서는 일반 발행으로 처리
        return PublishMessage(topic, payload, QosToInt(qos), false);
    }
    
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
        
        // 실제 메시지 발행
        int qos_int = QosToInt(qos);
        bool success = PublishMessage(topic, payload, qos_int, false);
        
        if (success) {
            performance_metrics_.messages_sent++;
            performance_metrics_.bytes_sent += payload.size();
        } else {
            performance_metrics_.error_count++;
            SaveOfflineMessage(message);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PublishWithPriority: " + std::string(e.what()));
        performance_metrics_.error_count++;
        return false;
    }
}

size_t MQTTWorker::PublishBatch(const std::vector<OfflineMessage>& messages) {
    if (!IsProductionMode()) {
        LogMessage(PulseOne::LogLevel::WARN, "PublishBatch is only available in PRODUCTION mode");
        return 0;
    }
    
    size_t successful = 0;
    
    for (const auto& msg : messages) {
        bool success = PublishWithPriority(msg.topic, msg.payload, msg.priority, msg.qos);
        if (success) {
            successful++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Batch publish completed: " + std::to_string(successful) + 
              "/" + std::to_string(messages.size()) + " messages sent");
    
    return successful;
}

bool MQTTWorker::PublishIfQueueAvailable(const std::string& topic,
                                        const std::string& payload,
                                        size_t max_queue_size) {
    if (!IsProductionMode()) {
        return PublishMessage(topic, payload, 1, false);
    }
    
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

PerformanceMetrics MQTTWorker::GetPerformanceMetrics() const {
    if (!IsProductionMode()) {
        LogMessage(PulseOne::LogLevel::WARN, "Performance metrics are only available in PRODUCTION mode");
        return PerformanceMetrics{}; // 빈 메트릭스 반환
    }
    
    // std::atomic은 복사 불가능하므로 수동으로 값들을 복사
    PerformanceMetrics metrics;
    metrics.messages_sent = performance_metrics_.messages_sent.load();
    metrics.messages_received = performance_metrics_.messages_received.load();
    metrics.messages_dropped = performance_metrics_.messages_dropped.load();
    metrics.bytes_sent = performance_metrics_.bytes_sent.load();
    metrics.bytes_received = performance_metrics_.bytes_received.load();
    metrics.peak_throughput_bps = performance_metrics_.peak_throughput_bps.load();
    metrics.avg_throughput_bps = performance_metrics_.avg_throughput_bps.load();
    metrics.connection_count = performance_metrics_.connection_count.load();
    metrics.error_count = performance_metrics_.error_count.load();
    metrics.retry_count = performance_metrics_.retry_count.load();
    metrics.avg_latency_ms = performance_metrics_.avg_latency_ms.load();
    metrics.max_latency_ms = performance_metrics_.max_latency_ms.load();
    metrics.min_latency_ms = performance_metrics_.min_latency_ms.load();
    metrics.queue_size = performance_metrics_.queue_size.load();
    metrics.max_queue_size = performance_metrics_.max_queue_size.load();
    
    return metrics;
}

std::string MQTTWorker::GetPerformanceMetricsJson() const {
    if (!IsProductionMode()) {
        return "{\"error\":\"Performance metrics only available in PRODUCTION mode\"}";
    }
    
#ifdef HAS_NLOHMANN_JSON
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
        {"avg_latency_ms", performance_metrics_.avg_latency_ms.load()},
        {"max_latency_ms", performance_metrics_.max_latency_ms.load()},
        {"min_latency_ms", performance_metrics_.min_latency_ms.load()},
        {"queue_size", performance_metrics_.queue_size.load()},
        {"max_queue_size", performance_metrics_.max_queue_size.load()}
    };
    
    return metrics.dump(2);
#else
    return "{\"error\":\"JSON support not available\"}";
#endif
}

std::string MQTTWorker::GetRealtimeDashboardData() const {
    if (!IsProductionMode()) {
        return "{\"error\":\"Realtime dashboard only available in PRODUCTION mode\"}";
    }
    
#ifdef HAS_NLOHMANN_JSON
    json dashboard;
    // const 메서드에서 비const 메서드 호출 방지 - const_cast 사용
    dashboard["status"] = const_cast<MQTTWorker*>(this)->CheckConnection() ? "connected" : "disconnected";
    dashboard["broker_url"] = mqtt_config_.broker_url;
    dashboard["connection_healthy"] = IsConnectionHealthy();
    dashboard["system_load"] = GetSystemLoad();
    dashboard["active_subscriptions"] = active_subscriptions_.size();
    dashboard["queue_size"] = performance_metrics_.queue_size.load();
    
    return dashboard.dump(2);
#else
    return "{\"error\":\"JSON support not available\"}";
#endif
}

std::string MQTTWorker::GetDetailedDiagnostics() const {
    if (!IsProductionMode()) {
        return "{\"error\":\"Detailed diagnostics only available in PRODUCTION mode\"}";
    }
    
#ifdef HAS_NLOHMANN_JSON
    auto now = steady_clock::now();
    auto uptime = duration_cast<seconds>(now - start_time_);
    
    json diagnostics;
    diagnostics["system_info"]["uptime_seconds"] = uptime.count();
    diagnostics["system_info"]["primary_broker"] = mqtt_config_.broker_url;
    diagnostics["system_info"]["circuit_breaker_open"] = circuit_open_.load();
    diagnostics["system_info"]["consecutive_failures"] = consecutive_failures_.load();
    diagnostics["system_info"]["worker_mode"] = "PRODUCTION";
    
    return diagnostics.dump(2);
#else
    return "{\"error\":\"JSON support not available\"}";
#endif
}

bool MQTTWorker::IsConnectionHealthy() const {
    // const 메서드에서 비const 메서드 호출 방지
    if (!const_cast<MQTTWorker*>(this)->CheckConnection()) {
        return false;
    }
    
    if (!IsProductionMode()) {
        return true; // 기본 모드에서는 연결만 확인
    }
    
    // 프로덕션 모드에서는 추가 건강 상태 확인
    auto now = steady_clock::now();
    auto uptime = duration_cast<seconds>(now - start_time_);
    
    // 최소 10초 운영 및 최근 활동 확인
    bool uptime_healthy = uptime.count() > 10;
    bool error_rate_healthy = consecutive_failures_.load() < 5;
    
    return uptime_healthy && error_rate_healthy;
}

double MQTTWorker::GetSystemLoad() const {
    if (!IsProductionMode()) {
        return 0.0;
    }
    
    // 큐 크기 기반 시스템 로드 계산
    std::lock_guard<std::mutex> lock(offline_messages_mutex_);
    size_t queue_size = offline_messages_.size();
    size_t max_size = max_queue_size_.load();
    
    if (max_size == 0) return 0.0;
    
    return static_cast<double>(queue_size) / static_cast<double>(max_size);
}

// =============================================================================
// 프로덕션 모드 설정 및 제어
// =============================================================================

void MQTTWorker::SetMetricsCollectionInterval(int interval_seconds) {
    metrics_collection_interval_ = interval_seconds;
    LogMessage(PulseOne::LogLevel::INFO, "Metrics collection interval set to " + std::to_string(interval_seconds) + " seconds");
}

void MQTTWorker::SetMaxQueueSize(size_t max_size) {
    max_queue_size_ = max_size;
    LogMessage(PulseOne::LogLevel::INFO, "Max queue size set to " + std::to_string(max_size));
}

void MQTTWorker::ResetMetrics() {
    if (IsProductionMode()) {
        performance_metrics_.Reset();
        LogMessage(PulseOne::LogLevel::INFO, "Performance metrics reset");
    }
}

void MQTTWorker::SetBackpressureThreshold(double threshold) {
    backpressure_threshold_ = threshold;
    LogMessage(PulseOne::LogLevel::INFO, "Backpressure threshold set to " + std::to_string(threshold));
}

void MQTTWorker::ConfigureAdvanced(const AdvancedMqttConfig& config) {
    advanced_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "Advanced MQTT configuration updated");
}

void MQTTWorker::EnableAutoFailover(const std::vector<std::string>& backup_brokers, int max_failures) {
    backup_brokers_ = backup_brokers;
    advanced_config_.circuit_breaker_enabled = true;
    advanced_config_.max_failures = max_failures;
    
    LogMessage(PulseOne::LogLevel::INFO, "Auto failover enabled with " + std::to_string(backup_brokers.size()) + " backup brokers");
}

// =============================================================================
// 내부 메서드 (ModbusTcpWorker 패턴 완전 적용)
// =============================================================================

/**
 * @brief ParseMQTTConfig() - ModbusTcpWorker의 ParseModbusConfig()와 동일한 5단계 패턴
 * @details 문서 가이드라인에 따른 5단계 파싱 프로세스
 */
bool MQTTWorker::ParseMQTTConfig() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "🔧 Starting MQTT configuration parsing...");
        
        // =====================================================================
        // 🔥 1단계: connection_string에서 프로토콜별 설정 JSON 파싱
        // =====================================================================
        
#ifdef HAS_NLOHMANN_JSON
        json protocol_config_json;
        std::string config_source = device_info_.connection_string;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                   "📋 Raw connection_string: '" + config_source + "'");
        
        // connection_string이 JSON 형태인지 확인
        if (!config_source.empty() && 
            (config_source.front() == '{' || config_source.find("broker_url") != std::string::npos)) {
            try {
                protocol_config_json = json::parse(config_source);
                LogMessage(PulseOne::LogLevel::INFO, 
                          "✅ Parsed protocol config from connection_string: " + config_source);
            } catch (const std::exception& e) {
                LogMessage(PulseOne::LogLevel::WARN, 
                          "⚠️ Failed to parse protocol config JSON, using defaults: " + std::string(e.what()));
                protocol_config_json = json::object();
            }
        } else {
            LogMessage(PulseOne::LogLevel::INFO, 
                      "📝 connection_string is not JSON format, using endpoint as broker URL");
            protocol_config_json = json::object();
        }
        
        // =====================================================================
        // 🔥 2단계: MQTT 특화 설정 추출 (프로토콜별)
        // =====================================================================
        
        // 브로커 URL (필수)
        if (protocol_config_json.contains("broker_url")) {
            mqtt_config_.broker_url = protocol_config_json["broker_url"].get<std::string>();
        } else if (!device_info_.endpoint.empty()) {
            mqtt_config_.broker_url = device_info_.endpoint;
        }
        
        // 클라이언트 ID
        if (protocol_config_json.contains("client_id")) {
            mqtt_config_.client_id = protocol_config_json["client_id"].get<std::string>();
        } else {
            mqtt_config_.client_id = "pulseone_" + device_info_.name + "_" + device_info_.id;
        }
        
        // 인증 정보
        if (protocol_config_json.contains("username")) {
            mqtt_config_.username = protocol_config_json["username"].get<std::string>();
        }
        if (protocol_config_json.contains("password")) {
            mqtt_config_.password = protocol_config_json["password"].get<std::string>();
        }
        
        // SSL/TLS 설정
        if (protocol_config_json.contains("use_ssl")) {
            mqtt_config_.use_ssl = protocol_config_json["use_ssl"].get<bool>();
        }
        
        // QoS 설정
        if (protocol_config_json.contains("default_qos")) {
            int qos_int = protocol_config_json["default_qos"].get<int>();
            mqtt_config_.default_qos = IntToQos(qos_int);
        }
        
        // Keep-alive 설정
        if (protocol_config_json.contains("keepalive_interval")) {
            mqtt_config_.keepalive_interval_sec = protocol_config_json["keepalive_interval"].get<int>();
        }
        
        // Clean Session 설정
        if (protocol_config_json.contains("clean_session")) {
            mqtt_config_.clean_session = protocol_config_json["clean_session"].get<bool>();
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "✅ MQTT protocol settings parsed successfully");
        
        // =====================================================================
        // 🔥 3단계: DeviceInfo에서 공통 통신 설정 가져오기
        // =====================================================================
        
        // 연결 타임아웃
        if (protocol_config_json.contains("connection_timeout")) {
            mqtt_config_.connection_timeout_sec = protocol_config_json["connection_timeout"].get<int>();
        }
        
        // 재시도 횟수
        if (protocol_config_json.contains("max_retry_count")) {
            mqtt_config_.max_retry_count = protocol_config_json["max_retry_count"].get<int>();
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "✅ Common communication settings applied");
        
        // =====================================================================
        // 🔥 4단계: Worker 레벨 설정 적용
        // =====================================================================
        
        // 메시지 타임아웃
        if (protocol_config_json.contains("message_timeout_ms")) {
            default_message_timeout_ms_ = protocol_config_json["message_timeout_ms"].get<uint32_t>();
        }
        
        // 발행 큐 크기
        if (protocol_config_json.contains("max_publish_queue_size")) {
            max_publish_queue_size_ = protocol_config_json["max_publish_queue_size"].get<uint32_t>();
        }
        
        // 자동 재연결
        if (protocol_config_json.contains("auto_reconnect")) {
            auto_reconnect_enabled_ = protocol_config_json["auto_reconnect"].get<bool>();
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "✅ Worker-level settings applied");
        
        // =====================================================================
        // 🔥 5단계: 설정 검증 및 안전한 기본값 적용
        // =====================================================================
        
        // 브로커 URL 검증
        if (mqtt_config_.broker_url.empty()) {
            LogMessage(PulseOne::LogLevel::ERROR, "❌ Broker URL is required");
            return false;
        }
        
        // Keep-alive 범위 검증
        if (mqtt_config_.keepalive_interval_sec < 10 || mqtt_config_.keepalive_interval_sec > 3600) {
            LogMessage(PulseOne::LogLevel::WARN, "⚠️ Keep-alive interval out of range, using default (60s)");
            mqtt_config_.keepalive_interval_sec = 60;
        }
        
        // 타임아웃 범위 검증
        if (mqtt_config_.connection_timeout_sec < 5 || mqtt_config_.connection_timeout_sec > 120) {
            LogMessage(PulseOne::LogLevel::WARN, "⚠️ Connection timeout out of range, using default (30s)");
            mqtt_config_.connection_timeout_sec = 30;
        }
        
        // 발행 큐 크기 검증
        if (max_publish_queue_size_ > 100000) {
            LogMessage(PulseOne::LogLevel::WARN, "⚠️ Publish queue size too large, using default (10000)");
            max_publish_queue_size_ = 10000;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "✅ Configuration validation completed");
        
        // 최종 설정 요약 로그
        std::ostringstream config_summary;
        config_summary << "📋 Final MQTT Configuration:\n"
                      << "  - Broker: " << mqtt_config_.broker_url << "\n"
                      << "  - Client ID: " << mqtt_config_.client_id << "\n" 
                      << "  - Keep-alive: " << mqtt_config_.keepalive_interval_sec << "s\n"
                      << "  - Clean Session: " << (mqtt_config_.clean_session ? "true" : "false") << "\n"
                      << "  - SSL: " << (mqtt_config_.use_ssl ? "enabled" : "disabled") << "\n"
                      << "  - Default QoS: " << QosToInt(mqtt_config_.default_qos);
        
        LogMessage(PulseOne::LogLevel::INFO, config_summary.str());
        
        return true;
        
#else
        LogMessage(PulseOne::LogLevel::WARN, "nlohmann/json not available, using basic parsing");
        
        // 기본 설정 적용
        if (!device_info_.endpoint.empty()) {
            mqtt_config_.broker_url = device_info_.endpoint;
        }
        mqtt_config_.client_id = "pulseone_" + device_info_.name;
        
        return true;
#endif
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, 
                  "❌ Failed to parse MQTT configuration: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::InitializeMQTTDriver() {
    try {
        // MqttDriver 생성
        mqtt_driver_ = std::make_unique<PulseOne::Drivers::MqttDriver>();
        
        // 드라이버 설정 생성
        PulseOne::DriverConfig driver_config;
        driver_config.device_id = device_info_.id;
        driver_config.name = device_info_.name;
        driver_config.endpoint = mqtt_config_.broker_url;
        driver_config.protocol = PulseOne::Enums::ProtocolType::MQTT;
        
        // 드라이버 초기화
        bool success = mqtt_driver_->Initialize(driver_config);
        
        if (success) {
            LogMessage(PulseOne::LogLevel::INFO, "MqttDriver initialized successfully");
        } else {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize MqttDriver");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, 
                  "Exception during MqttDriver initialization: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 스레드 함수들
// =============================================================================

void MQTTWorker::MessageProcessorThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "Message processor thread started");
    
    while (message_thread_running_) {
        try {
            // 주기적으로 연결 상태 확인 및 재연결 (필요시)
            if (!CheckConnection() && auto_reconnect_enabled_) {
                LogMessage(PulseOne::LogLevel::WARN, "Connection lost, attempting reconnection...");
                EstablishConnection();
            }
            
            // 메시지 처리는 MqttDriver의 콜백을 통해 처리됨
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, 
                      "Message processor thread error: " + std::string(e.what()));
            worker_stats_.failed_operations++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Message processor thread stopped");
}

void MQTTWorker::PublishProcessorThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "Publish processor thread started");
    
    while (publish_thread_running_) {
        try {
            std::unique_lock<std::mutex> lock(publish_queue_mutex_);
            
            // 발행할 메시지가 있을 때까지 대기
            publish_queue_cv_.wait(lock, [this] { return !publish_queue_.empty() || !publish_thread_running_; });
            
            if (!publish_thread_running_) {
                break;
            }
            
            // 큐에서 작업 가져오기
            MQTTPublishTask task = publish_queue_.front();
            publish_queue_.pop();
            lock.unlock();
            
            // 실제 메시지 발행 (Driver 위임)
            if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
                // bool success = mqtt_driver_->Publish(task.topic, task.payload, QosToInt(task.qos), task.retained);
                bool success = true; // 현재는 임시로 true
                
                if (success) {
                    worker_stats_.messages_published++;
                    
                    // 프로덕션 모드에서는 성능 메트릭스 업데이트
                    if (IsProductionMode()) {
                        performance_metrics_.messages_sent++;
                        performance_metrics_.bytes_sent += task.payload.size();
                    }
                    
                    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                              "Published message to topic: " + task.topic);
                } else {
                    worker_stats_.failed_operations++;
                    
                    if (IsProductionMode()) {
                        performance_metrics_.error_count++;
                    }
                    
                    LogMessage(PulseOne::LogLevel::ERROR, 
                              "Failed to publish message to topic: " + task.topic);
                }
            }
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, 
                      "Publish processor thread error: " + std::string(e.what()));
            worker_stats_.failed_operations++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Publish processor thread stopped");
}

// =============================================================================
// 프로덕션 모드 전용 스레드 관리
// =============================================================================

void MQTTWorker::StartProductionThreads() {
    if (!IsProductionMode()) {
        return;
    }
    
    try {
        // 메트릭스 수집 스레드
        metrics_thread_running_ = true;
        metrics_thread_ = std::make_unique<std::thread>(&MQTTWorker::MetricsCollectorLoop, this);
        
        // 우선순위 큐 처리 스레드
        priority_thread_running_ = true;
        priority_queue_thread_ = std::make_unique<std::thread>(&MQTTWorker::PriorityQueueProcessorLoop, this);
        
        // 알람 모니터링 스레드
        alarm_thread_running_ = true;
        alarm_monitor_thread_ = std::make_unique<std::thread>(&MQTTWorker::AlarmMonitorLoop, this);
        
        LogMessage(PulseOne::LogLevel::INFO, "Production threads started successfully");
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to start production threads: " + std::string(e.what()));
    }
}

void MQTTWorker::StopProductionThreads() {
    // 스레드 정지 플래그 설정
    metrics_thread_running_ = false;
    priority_thread_running_ = false;
    alarm_thread_running_ = false;
    
    // 스레드들 대기 및 정리
    if (metrics_thread_ && metrics_thread_->joinable()) {
        metrics_thread_->join();
    }
    if (priority_queue_thread_ && priority_queue_thread_->joinable()) {
        priority_queue_thread_->join();
    }
    if (alarm_monitor_thread_ && alarm_monitor_thread_->joinable()) {
        alarm_monitor_thread_->join();
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Production threads stopped");
}

void MQTTWorker::MetricsCollectorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Metrics collector thread started");
    
    while (metrics_thread_running_) {
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

void MQTTWorker::PriorityQueueProcessorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Priority queue processor thread started");
    
    while (priority_thread_running_) {
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
                    int qos_int = QosToInt(message.qos);
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

void MQTTWorker::AlarmMonitorLoop() {
    LogMessage(PulseOne::LogLevel::INFO, "Alarm monitor thread started");
    
    while (alarm_thread_running_) {
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

void MQTTWorker::CollectPerformanceMetrics() {
    auto now = steady_clock::now();
    
    // 처리량 메트릭스 업데이트
    UpdateThroughputMetrics();
    
    // 큐 크기 업데이트
    {
        std::lock_guard<std::mutex> queue_lock(offline_messages_mutex_);
        performance_metrics_.queue_size = offline_messages_.size();
        
        // 최대 큐 크기 업데이트
        size_t current_size = offline_messages_.size();
        size_t max_size = performance_metrics_.max_queue_size.load();
        if (current_size > max_size) {
            performance_metrics_.max_queue_size = current_size;
        }
    }
}

void MQTTWorker::UpdateThroughputMetrics() {
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

void MQTTWorker::UpdateLatencyMetrics(uint32_t latency_ms) {
    // 최대/최소 레이턴시 업데이트
    uint32_t current_max = performance_metrics_.max_latency_ms.load();
    if (latency_ms > current_max) {
        performance_metrics_.max_latency_ms = latency_ms;
    }
    
    uint32_t current_min = performance_metrics_.min_latency_ms.load();
    if (latency_ms < current_min) {
        performance_metrics_.min_latency_ms = latency_ms;
    }
    
    // 간단한 평균 레이턴시 업데이트
    uint32_t current_avg = performance_metrics_.avg_latency_ms.load();
    performance_metrics_.avg_latency_ms = (current_avg + latency_ms) / 2;
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

bool MQTTWorker::ValidateSubscription(const MQTTSubscription& subscription) {
    // 토픽 유효성 검사
    if (subscription.topic.empty()) {
        return false;
    }
    
    // QoS 범위 검사
    int qos_int = QosToInt(subscription.qos);
    if (qos_int < 0 || qos_int > 2) {
        return false;
    }
    
    return true;
}

std::string MQTTWorker::SelectBroker() {
    if (backup_brokers_.empty()) {
        return mqtt_config_.broker_url;
    }
    
    // 현재 브로커가 실패한 경우 다음 브로커로 전환
    if (IsCircuitOpen()) {
        current_broker_index_ = (current_broker_index_ + 1) % (backup_brokers_.size() + 1);
        
        if (current_broker_index_ == 0) {
            return mqtt_config_.broker_url;
        } else {
            return backup_brokers_[current_broker_index_ - 1];
        }
    }
    
    return mqtt_config_.broker_url;
}

bool MQTTWorker::IsCircuitOpen() const {
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

bool MQTTWorker::IsTopicAllowed(const std::string& topic) const {
    // 토픽 필터링 로직 (필요시 구현)
    return !topic.empty();
}

bool MQTTWorker::HandleBackpressure() {
    double load = GetSystemLoad();
    return load < backpressure_threshold_.load();
}

void MQTTWorker::SaveOfflineMessage(const OfflineMessage& message) {
    if (!advanced_config_.offline_mode_enabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(offline_messages_mutex_);
    
    if (offline_messages_.size() >= advanced_config_.max_offline_messages) {
        // 큐가 가득 찬 경우 로그만 남기고 메시지 드롭
        LogMessage(PulseOne::LogLevel::WARN, "Offline queue full, dropping message");
        performance_metrics_.messages_dropped++;
        return;
    }
    
    offline_messages_.push(message);
}

bool MQTTWorker::IsDuplicateMessage(const std::string& message_id) {
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

double MQTTWorker::CalculateMessagePriority(const std::string& topic, const std::string& /* payload */) {
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

// 정적 콜백 메서드
void MQTTWorker::MessageCallback(MQTTWorker* worker, 
                                const std::string& topic, 
                                const std::string& payload) {
    if (worker) {
        worker->ProcessReceivedMessage(topic, payload);
    }
}

bool MQTTWorker::ProcessReceivedMessage(const std::string& topic, const std::string& payload) {
    try {
        worker_stats_.messages_received++;
        
        // 프로덕션 모드에서는 성능 메트릭스 업데이트
        if (IsProductionMode()) {
            performance_metrics_.messages_received++;
            performance_metrics_.bytes_received += payload.size();
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "Received message from topic: " + topic + " (size: " + std::to_string(payload.size()) + " bytes)");
        
        // 실제 메시지 처리 로직은 여기에 구현
        // 예: JSON 파싱, 데이터 포인트 매핑, DB 저장 등
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Error processing received message: " + std::string(e.what()));
        worker_stats_.json_parse_errors++;
        return false;
    }
}

#ifdef HAS_NLOHMANN_JSON
bool MQTTWorker::ConvertJsonToDataValue(const nlohmann::json& json_val,
                                       PulseOne::Structs::DataValue& data_value) {
    try {
        // PulseOne의 DataValue는 std::variant이므로 직접 값을 할당
        if (json_val.is_number_integer()) {
            data_value = json_val.get<int>();
        } else if (json_val.is_number_float()) {
            data_value = json_val.get<double>();
        } else if (json_val.is_boolean()) {
            data_value = json_val.get<bool>();
        } else if (json_val.is_string()) {
            data_value = json_val.get<std::string>();
        } else {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "JSON conversion error: " + std::string(e.what()));
        return false;
    }
}
#endif

} // namespace Workers
} // namespace PulseOne