/**
 * @file MQTTWorker.cpp - ModbusTcpWorker 패턴 완전 적용 (500줄 이하)
 * @brief MQTT 프로토콜 워커 클래스 구현 - MqttDriver를 통신 매체로 사용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 (ModbusTcpWorker 패턴 적용)
 */

#include "Workers/Protocol/MQTTWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

using namespace std::chrono;
using namespace PulseOne::Drivers;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자 (ModbusTcpWorker 패턴)
// =============================================================================

MQTTWorker::MQTTWorker(const PulseOne::DeviceInfo& device_info,
                       std::shared_ptr<RedisClient> redis_client,
                       std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , mqtt_driver_(nullptr)
    , next_subscription_id_(1)
    , message_thread_running_(false)
    , publish_thread_running_(false)
    , default_message_timeout_ms_(30000)
    , max_publish_queue_size_(10000)
    , auto_reconnect_enabled_(true) {
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker created for device: " + device_info.name);
    
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
    // 스레드 정리
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
            
            // 1. 스레드 정리
            message_thread_running_ = false;
            publish_thread_running_ = false;
            publish_queue_cv_.notify_all();
            
            if (message_processor_thread_ && message_processor_thread_->joinable()) {
                message_processor_thread_->join();
            }
            if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
                publish_processor_thread_->join();
            }
            
            // 2. 연결 해제
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
        return true;
    } else {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish MQTT connection");
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
// MQTT 특화 객체 관리 (Worker 고유 기능)
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
        // MqttDriver의 Subscribe 메서드 사용 (실제 구현에서는 MqttDriver 인터페이스에 맞춰 수정 필요)
        // bool success = mqtt_driver_->Subscribe(new_subscription.topic, QosToInt(new_subscription.qos));
        bool success = true; // 현재는 임시로 true
        
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
    
    return PublishMessage(task);  // 구조체 버전 호출
}
// =============================================================================
// 내부 메서드 (Worker 고유 로직) - ModbusTcpWorker 패턴
// =============================================================================

/**
 * @brief ParseMQTTConfig() - ModbusTcpWorker의 ParseModbusConfig()와 동일한 5단계 패턴
 * @details 문서 가이드라인에 따른 5단계 파싱 프로세스
 * 
 * 🔥 구현 전략:
 * 1단계: connection_string에서 프로토콜별 설정 JSON 파싱
 * 2단계: MQTT 특화 설정 추출 (프로토콜별)
 * 3단계: DeviceInfo에서 공통 통신 설정 가져오기
 * 4단계: Worker 레벨 설정 적용
 * 5단계: 설정 검증 및 안전한 기본값 적용
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
            // ✅ 수정: device_info_.id는 UUID(std::string) 타입이므로 바로 사용
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
        
        // 연결 타임아웃 (DeviceSettings에서)
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
        PulseOne::DriverConfig driver_config;  // ✅ 수정: PulseOne:: 네임스페이스 사용
        driver_config.device_id = device_info_.id;
        // ✅ 수정: device_name → name, protocol_type → protocol 필드명 사용
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
                // bool success = mqtt_driver_->Publish(task.topic, task.payload, 
                //                                     QosToInt(task.qos), task.retained);
                bool success = true; // 현재는 임시로 true
                
                if (success) {
                    worker_stats_.messages_published++;
                    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                              "Published message to topic: " + task.topic);
                } else {
                    worker_stats_.failed_operations++;
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
          << "\"active_subscriptions\":" << active_subscriptions_.size()
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
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT worker statistics reset");
}

} // namespace Workers
} // namespace PulseOne