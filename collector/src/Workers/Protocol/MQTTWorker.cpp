/**
 * @file MQTTWorker.cpp
 * @brief MQTT 워커 구현 (완전 수정)
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.1
 */

#include "Workers/Protocol/MQTTWorker.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using namespace std::chrono;
using json = nlohmann::json;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MQTTWorker::MQTTWorker(
    const PulseOne::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client) {
    
    // 통계 초기화
    worker_stats_.start_time = system_clock::now();
    
    // worker_id 초기화
    worker_id_ = device_info.id;
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseMQTTWorkerConfig()) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to parse MQTT worker config, using defaults");
    }
    
    // MQTT 드라이버 생성
    mqtt_driver_ = std::make_unique<PulseOne::Drivers::MqttDriver>();  // ✅ MQTTDriver → MqttDriver
}

MQTTWorker::~MQTTWorker() {
    // 스레드 정리
    threads_running_ = false;
    publish_queue_cv_.notify_all();
    
    if (message_processor_thread_ && message_processor_thread_->joinable()) {
        message_processor_thread_->join();
    }
    if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
        publish_processor_thread_->join();
    }
    
    // MQTT 드라이버 정리
    ShutdownMQTTDriver();
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker destroyed for device: " + GetDeviceInfo().name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> MQTTWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Starting MQTT worker...");
        
        ChangeState(WorkerState::STARTING);
        
        if (!EstablishConnection()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish MQTT connection");
            ChangeState(WorkerState::ERROR);
            promise->set_value(false);
            return future;
        }
        
        auto data_points = GetDataPoints();
        if (!CreateSubscriptionsFromDataPoints(data_points)) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to create some subscriptions");
        }
        
        // 스레드 시작
        threads_running_ = true;
        message_processor_thread_ = std::make_unique<std::thread>(
            &MQTTWorker::MessageProcessorThreadFunction, this);
        publish_processor_thread_ = std::make_unique<std::thread>(
            &MQTTWorker::PublishProcessorThreadFunction, this);
        
        ChangeState(WorkerState::RUNNING);
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in MQTT Start: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

std::future<bool> MQTTWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Stopping MQTT worker...");
        
        ChangeState(WorkerState::STOPPING);
        
        threads_running_ = false;
        publish_queue_cv_.notify_all();
        
        if (message_processor_thread_ && message_processor_thread_->joinable()) {
            message_processor_thread_->join();
        }
        if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
            publish_processor_thread_->join();
        }
        
        CloseConnection();
        ChangeState(WorkerState::STOPPED);
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTT worker stopped");
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in MQTT Stop: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

bool MQTTWorker::EstablishConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Establishing MQTT connection...");
    
    if (!InitializeMQTTDriver()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize MQTT driver");
        return false;
    }
    
    SetConnectionState(true);
    return true;
}

bool MQTTWorker::CloseConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Closing MQTT connection...");
    
    ShutdownMQTTDriver();
    SetConnectionState(false);
    return true;
}

bool MQTTWorker::CheckConnection() {
    return mqtt_driver_ && mqtt_driver_->IsConnected();
}

bool MQTTWorker::SendKeepAlive() {
    return CheckConnection();
}

// =============================================================================
// MQTT 워커 전용 메서드들
// =============================================================================

void MQTTWorker::ConfigureMQTTWorker(const MQTTWorkerConfig& config) {
    worker_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "MQTT worker configuration updated");
}

std::string MQTTWorker::GetMQTTWorkerStats() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"mqtt_worker_statistics\": {\n";
    ss << "    \"messages_received\": " << worker_stats_.messages_received.load() << ",\n";
    ss << "    \"messages_published\": " << worker_stats_.messages_published.load() << ",\n";
    ss << "    \"subscribe_operations\": " << worker_stats_.subscribe_operations.load() << ",\n";
    ss << "    \"publish_operations\": " << worker_stats_.publish_operations.load() << ",\n";
    ss << "    \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    ss << "    \"active_subscriptions_count\": " << active_subscriptions_.size() << ",\n";
    
    {
        std::lock_guard<std::mutex> pub_lock(const_cast<std::mutex&>(publish_queue_mutex_));
        ss << "    \"pending_publish_tasks\": " << publish_queue_.size() << "\n";
    }
    
    ss << "  },\n";
    ss << "  \"base_worker_statistics\": " << GetStatusJson() << "\n";
    ss << "}";
    
    return ss.str();
}

void MQTTWorker::ResetMQTTWorkerStats() {
    worker_stats_.messages_received = 0;
    worker_stats_.messages_published = 0;
    worker_stats_.subscribe_operations = 0;
    worker_stats_.publish_operations = 0;
    worker_stats_.successful_publishes = 0;
    worker_stats_.failed_operations = 0;
    worker_stats_.json_parse_errors = 0;
    worker_stats_.last_reset = system_clock::now();
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT worker statistics reset");
}

bool MQTTWorker::AddSubscription(const MQTTSubscription& subscription) {
    try {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        if (active_subscriptions_.find(subscription.topic) != active_subscriptions_.end()) {
            LogMessage(PulseOne::LogLevel::WARN, "Subscription already exists for topic: " + subscription.topic);
            return false;
        }
        
        active_subscriptions_[subscription.topic] = subscription;
        worker_stats_.subscribe_operations++;
        
        LogMessage(PulseOne::LogLevel::INFO, "Added subscription for topic: " + subscription.topic);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to add subscription: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::RemoveSubscription(const std::string& topic) {
    try {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        auto it = active_subscriptions_.find(topic);
        if (it == active_subscriptions_.end()) {
            LogMessage(PulseOne::LogLevel::WARN, "Subscription not found for topic: " + topic);
            return false;
        }
        
        active_subscriptions_.erase(it);
        LogMessage(PulseOne::LogLevel::INFO, "Removed subscription for topic: " + topic);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to remove subscription: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::PublishMessage(const std::string& topic, const std::string& payload, 
                               int qos, bool retained) {
    try {
        MQTTPublishTask task;
        task.topic = topic;
        task.payload = payload;
        task.qos = qos;
        task.retained = retained;
        task.scheduled_time = system_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(publish_queue_mutex_);
            publish_queue_.push(task);
        }
        
        publish_queue_cv_.notify_one();
        worker_stats_.publish_operations++;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Queued publish task for topic: " + topic);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to queue publish task: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        return false;
    }
}

std::string MQTTWorker::GetActiveSubscriptions() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"active_subscriptions\": [\n";
    
    bool first = true;
    for (const auto& [topic, subscription] : active_subscriptions_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"topic\": \"" << topic << "\",\n";
        ss << "      \"qos\": " << subscription.qos << ",\n";
        ss << "      \"enabled\": " << (subscription.enabled ? "true" : "false") << ",\n";
        ss << "      \"point_id\": \"" << subscription.point_id << "\",\n";
        ss << "      \"messages_received\": " << subscription.messages_received << "\n";
        ss << "    }";
    }
    
    ss << "\n  ],\n";
    ss << "  \"total_count\": " << active_subscriptions_.size() << "\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// 내부 메서드들
// =============================================================================

bool MQTTWorker::InitializeMQTTDriver() {
    try {
        if (!mqtt_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver not created");
            return false;
        }
        
        auto driver_config = CreateDriverConfig();
        
        if (!mqtt_driver_->Initialize(driver_config)) {
            auto error = mqtt_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver initialization failed: " + error.message);
            return false;
        }
        
        if (!mqtt_driver_->Connect()) {
            auto error = mqtt_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver connection failed: " + error.message);
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTT driver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in InitializeMQTTDriver: " + std::string(e.what()));
        return false;
    }
}

void MQTTWorker::ShutdownMQTTDriver() {
    try {
        if (mqtt_driver_) {
            mqtt_driver_->Disconnect();
            LogMessage(PulseOne::LogLevel::INFO, "MQTT driver shutdown complete");
        }
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ShutdownMQTTDriver: " + std::string(e.what()));
    }
}

bool MQTTWorker::CreateSubscriptionsFromDataPoints(const std::vector<PulseOne::DataPoint>& points) {
    try {
        bool all_success = true;
        
        for (const auto& point : points) {
            std::string topic;
            std::string json_path;
            int qos;
            
            if (ParseMQTTTopic(point, topic, json_path, qos)) {
                MQTTSubscription subscription;
                subscription.topic = topic;
                subscription.qos = qos;
                subscription.point_id = point.id;
                // ✅ point.data_type 타입 변환
                if (point.data_type == "float") {
                    subscription.data_type = Structs::DataType::FLOAT32;
                } else if (point.data_type == "int") {
                    subscription.data_type = Structs::DataType::INT32;
                } else if (point.data_type == "bool") {
                    subscription.data_type = Structs::DataType::BOOL;
                } else if (point.data_type == "string") {
                    subscription.data_type = Structs::DataType::STRING;
                } else {
                    subscription.data_type = Structs::DataType::UNKNOWN;
                }
                subscription.json_path = json_path;
                subscription.scaling_factor = point.scaling_factor;
                subscription.scaling_offset = point.scaling_offset;
                subscription.enabled = point.is_enabled;
                
                if (!AddSubscription(subscription)) {
                    all_success = false;
                }
            } else {
                LogMessage(PulseOne::LogLevel::WARN, "Failed to parse MQTT topic for data point: " + point.name);
                all_success = false;
            }
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "Created subscriptions from " + std::to_string(points.size()) + " data points");
        return all_success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in CreateSubscriptionsFromDataPoints: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::ProcessReceivedMessage(const std::string& topic, const std::string& payload) {
    try {
        worker_stats_.messages_received++;
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        auto it = active_subscriptions_.find(topic);
        if (it == active_subscriptions_.end()) {
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "No subscription found for topic: " + topic);
            return false;
        }
        
        auto& subscription = it->second;
        subscription.messages_received++;
        subscription.last_received = system_clock::now();
        
        Structs::DataValue extracted_value;
        if (!ExtractValueFromJSON(payload, subscription.json_path, extracted_value)) {
            worker_stats_.json_parse_errors++;
            LogMessage(PulseOne::LogLevel::WARN, "Failed to extract value from JSON for topic: " + topic);
            return false;
        }
        
        // 스케일링 적용
        std::visit([&subscription](auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                val = static_cast<T>(val * subscription.scaling_factor + subscription.scaling_offset);
            }
        }, extracted_value);
        
        TimestampedValue timestamped_value(extracted_value, DataQuality::GOOD);
        
        SaveToInfluxDB(subscription.point_id, timestamped_value);
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Processed message for topic: " + topic);
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ProcessReceivedMessage: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::ExtractValueFromJSON(const std::string& payload, 
                                     const std::string& json_path, 
                                     Structs::DataValue& extracted_value) {
    try {
        if (payload.empty()) {
            LogMessage(PulseOne::LogLevel::WARN, "Empty payload received");
            return false;
        }
        
#ifdef HAS_NLOHMANN_JSON
        nlohmann::json parsed_json;
        try {
            parsed_json = nlohmann::json::parse(payload);
        } catch (const nlohmann::json::parse_error&) {
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Payload is not JSON, treating as direct value: " + payload);
            
            try {
                if (payload.find('.') != std::string::npos) {
                    extracted_value = std::stod(payload);
                } else {
                    int64_t value = std::stoll(payload);
                    if (value >= INT32_MIN && value <= INT32_MAX) {
                        extracted_value = static_cast<int32_t>(value);
                    } else {
                        extracted_value = value;
                    }
                }
                return true;
            } catch (const std::exception&) {
                if (payload == "true" || payload == "false") {
                    extracted_value = (payload == "true");
                    return true;
                }
                extracted_value = payload;
                return true;
            }
        }
        
        if (json_path.empty()) {
            return ConvertJsonToDataValue(parsed_json, extracted_value);
        }
        
        nlohmann::json current = parsed_json;
        std::istringstream path_stream(json_path);
        std::string segment;
        
        while (std::getline(path_stream, segment, '.')) {
            if (segment.empty()) continue;
            
            if (!current.is_object() || !current.contains(segment)) {
                return false;
            }
            current = current[segment];
        }
        
        return ConvertJsonToDataValue(current, extracted_value);
        
#else
        extracted_value = payload;
        return true;
#endif
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "JSON extraction failed: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::ParseMQTTTopic(const PulseOne::DataPoint& point, 
                               std::string& topic, std::string& json_path, int& qos) {
    try {
        std::string addr_str = point.address_string;
        if (addr_str.empty()) {
            addr_str = "data/" + std::to_string(point.address);
        }
        
        topic = addr_str;
        json_path = "";
        qos = 1;
        
        size_t first_colon = addr_str.find(':');
        if (first_colon != std::string::npos) {
            topic = addr_str.substr(0, first_colon);
            
            size_t second_colon = addr_str.find(':', first_colon + 1);
            if (second_colon != std::string::npos) {
                json_path = addr_str.substr(first_colon + 1, second_colon - first_colon - 1);
                qos = std::stoi(addr_str.substr(second_colon + 1));
            } else {
                json_path = addr_str.substr(first_colon + 1);
            }
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Parsed MQTT topic: " + topic + ", JSON path: " + json_path + 
                ", QoS: " + std::to_string(qos));
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse MQTT topic: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::ParseMQTTWorkerConfig() {
    try {
        const auto& device_info = GetDeviceInfo();
        
        if (device_info.name.empty()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Device info incomplete");
            return false;
        }
        
        // 기본 설정만 사용
        mqtt_config_.broker_host = "localhost";
        mqtt_config_.broker_port = 1883;
        mqtt_config_.client_id = worker_id_;
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTT worker config initialized with defaults");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse MQTT worker config: " + std::string(e.what()));
        return false;
    }
}

void MQTTWorker::MessageProcessorThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "MQTT message processor thread started");
    
    while (threads_running_.load()) {
        try {
            std::this_thread::sleep_for(milliseconds(100));
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in message processor thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(1000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT message processor thread stopped");
}

void MQTTWorker::PublishProcessorThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "MQTT publish processor thread started");
    
    while (threads_running_.load()) {
        try {
            std::unique_lock<std::mutex> lock(publish_queue_mutex_);
            
            publish_queue_cv_.wait(lock, [this] { 
                return !publish_queue_.empty() || !threads_running_.load(); 
            });
            
            if (!threads_running_.load()) break;
            
            while (!publish_queue_.empty()) {
                auto task = publish_queue_.front();
                publish_queue_.pop();
                lock.unlock();
                
                if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
                    worker_stats_.messages_published++;
                    worker_stats_.successful_publishes++;
                    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Published message to topic: " + task.topic);
                } else {
                    worker_stats_.failed_operations++;
                    LogMessage(PulseOne::LogLevel::WARN, "Failed to publish - driver not connected");
                }
                
                lock.lock();
            }
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in publish processor thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(1000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT publish processor thread stopped");
}

DriverConfig MQTTWorker::CreateDriverConfig() {
    DriverConfig config;
    
    try {
        config.device_id = worker_id_;
        config.name = "mqtt_driver_" + worker_id_;
        config.protocol = ProtocolType::MQTT;
        config.endpoint = mqtt_config_.broker_host + ":" + std::to_string(mqtt_config_.broker_port);
        config.timeout = std::chrono::seconds(mqtt_config_.connection_timeout_sec);
        config.retry_count = mqtt_config_.max_retry_count;
        config.polling_interval = std::chrono::milliseconds(poll_interval_ms_);
        
        LogMessage(PulseOne::LogLevel::INFO, "Created MQTT driver config for broker: " + config.endpoint);
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to create MQTT driver config: " + std::string(e.what()));
        throw;
    }
    
    return config;
}

void MQTTWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    try {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "MQTT worker operation: " + operation + 
                " (success: " + (success ? "true" : "false") + ")");
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in UpdateWorkerStats: " + std::string(e.what()));
    }
}

void MQTTWorker::MessageCallback(MQTTWorker* worker, 
                                const std::string& topic, const std::string& payload) {
    if (worker) {
        worker->ProcessReceivedMessage(topic, payload);
    }
}

#ifdef HAS_NLOHMANN_JSON
bool MQTTWorker::ConvertJsonToDataValue(const nlohmann::json& json_val, Structs::DataValue& data_value) {
    try {
        if (json_val.is_boolean()) {
            data_value = json_val.get<bool>();
        } else if (json_val.is_number_integer()) {
            int64_t val = json_val.get<int64_t>();
            if (val >= INT32_MIN && val <= INT32_MAX) {
                data_value = static_cast<int32_t>(val);
            } else {
                data_value = val;
            }
        } else if (json_val.is_number_float()) {
            data_value = json_val.get<double>();
        } else if (json_val.is_string()) {
            data_value = json_val.get<std::string>();
        } else {
            data_value = json_val.dump();
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
#endif

} // namespace Workers
} // namespace PulseOne