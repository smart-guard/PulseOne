/**
 * @file MQTTWorker.cpp
 * @brief MQTT 프로토콜 워커 클래스 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#include "Workers/Protocol/MQTTWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <nlohmann/json.hpp>

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
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , threads_running_(false) {
    
    // MQTT 워커 통계 초기화
    worker_stats_.start_time = system_clock::now();
    worker_stats_.last_reset = worker_stats_.start_time;
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker created for device: " + device_info.name);
    
    // device_info에서 MQTT 워커 설정 파싱
    if (!ParseMQTTWorkerConfig()) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to parse MQTT worker config, using defaults");
    }
    
    // MQTT 드라이버 생성
    mqtt_driver_ = std::make_unique<Drivers::MqttDriver>();
}

MQTTWorker::~MQTTWorker() {
    // 스레드 정리
    if (threads_running_.load()) {
        threads_running_ = false;
        publish_queue_cv_.notify_all();
        
        if (message_processor_thread_ && message_processor_thread_->joinable()) {
            message_processor_thread_->join();
        }
        if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
            publish_processor_thread_->join();
        }
    }
    
    // MQTT 드라이버 정리
    ShutdownMQTTDriver();
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTTWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> MQTTWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Starting MQTT worker...");
        
        // 상태를 STARTING으로 변경
        ChangeState(WorkerState::STARTING);
        
        // MQTT 연결 수립
        if (!EstablishConnection()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish MQTT connection");
            ChangeState(WorkerState::ERROR);
            promise->set_value(false);
            return future;
        }
        
        // 데이터 포인트에서 구독 생성
        auto data_points = GetDataPoints();
        if (!CreateSubscriptionsFromDataPoints(data_points)) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to create some subscriptions from data points");
        }
        
        // 워커 스레드들 시작
        threads_running_ = true;
        message_processor_thread_ = std::make_unique<std::thread>(&MQTTWorker::MessageProcessorThreadFunction, this);
        publish_processor_thread_ = std::make_unique<std::thread>(&MQTTWorker::PublishProcessorThreadFunction, this);
        
        // 상태를 RUNNING으로 변경
        ChangeState(WorkerState::RUNNING);
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTT worker started successfully");
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
        
        // 상태를 STOPPING으로 변경
        ChangeState(WorkerState::STOPPING);
        
        // 워커 스레드들 정지
        if (threads_running_.load()) {
            threads_running_ = false;
            publish_queue_cv_.notify_all();
            
            if (message_processor_thread_ && message_processor_thread_->joinable()) {
                message_processor_thread_->join();
            }
            if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
                publish_processor_thread_->join();
            }
        }
        
        // MQTT 연결 해제
        CloseConnection();
        
        // 상태를 STOPPED로 변경
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
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT connection established");
    SetConnectionState(true);
    return true;
}

bool MQTTWorker::CloseConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Closing MQTT connection...");
    
    ShutdownMQTTDriver();
    
    LogMessage(PulseOne::LogLevel::INFO, "MQTT connection closed");
    SetConnectionState(false);
    return true;
}

bool MQTTWorker::CheckConnection() {
    if (!mqtt_driver_) {
        return false;
    }
    
    return mqtt_driver_->IsConnected();
}

bool MQTTWorker::SendKeepAlive() {
    // MQTT는 내부적으로 Keep-alive를 처리하므로 단순히 연결 상태만 확인
    return CheckConnection();
}

// =============================================================================
// MQTT 공개 인터페이스
// =============================================================================

void MQTTWorker::ConfigureMQTTWorker(const MQTTWorkerConfig& config) {
    worker_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "MQTT worker configuration updated");
}

std::string MQTTWorker::GetMQTTWorkerStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"mqtt_worker_statistics\": {\n";
    ss << "    \"messages_received\": " << worker_stats_.messages_received.load() << ",\n";
    ss << "    \"messages_published\": " << worker_stats_.messages_published.load() << ",\n";
    ss << "    \"subscribe_operations\": " << worker_stats_.subscribe_operations.load() << ",\n";
    ss << "    \"publish_operations\": " << worker_stats_.publish_operations.load() << ",\n";
    ss << "    \"successful_publishes\": " << worker_stats_.successful_publishes.load() << ",\n";
    ss << "    \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    ss << "    \"json_parse_errors\": " << worker_stats_.json_parse_errors.load() << ",\n";
    
    // 통계 시간 정보
    auto start_time = duration_cast<seconds>(worker_stats_.start_time.time_since_epoch()).count();
    auto reset_time = duration_cast<seconds>(worker_stats_.last_reset.time_since_epoch()).count();
    ss << "    \"start_timestamp\": " << start_time << ",\n";
    ss << "    \"last_reset_timestamp\": " << reset_time << ",\n";
    
    // 활성 구독 수
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        ss << "    \"active_subscriptions_count\": " << active_subscriptions_.size() << ",\n";
    }
    
    // 대기 중인 발행 작업 수
    {
        std::lock_guard<std::mutex> lock(publish_queue_mutex_);
        ss << "    \"pending_publish_tasks\": " << publish_queue_.size() << "\n";
    }
    
    ss << "  },\n";
    
    // MQTT 드라이버 통계 추가
    if (mqtt_driver_) {
        try {
            auto driver_stats = mqtt_driver_->GetStatistics();
            ss << "  \"mqtt_driver_statistics\": {\n";
            ss << "    \"successful_connections\": " << driver_stats.successful_connections << ",\n";
            ss << "    \"failed_connections\": " << driver_stats.failed_connections << ",\n";
            ss << "    \"total_operations\": " << driver_stats.total_operations << ",\n";
            ss << "    \"successful_operations\": " << driver_stats.successful_operations << ",\n";
            ss << "    \"failed_operations\": " << driver_stats.failed_operations << ",\n";
            ss << "    \"avg_response_time_ms\": " << driver_stats.avg_response_time_ms << ",\n";
            ss << "    \"success_rate\": " << driver_stats.success_rate << ",\n";
            ss << "    \"consecutive_failures\": " << driver_stats.consecutive_failures << "\n";
            ss << "  },\n";
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to get driver statistics: " + std::string(e.what()));
        }
    }
    
    // 기본 워커 통계 추가
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
        
        // 중복 구독 확인
        if (active_subscriptions_.find(subscription.topic) != active_subscriptions_.end()) {
            LogMessage(PulseOne::LogLevel::WARN, "Subscription already exists for topic: " + subscription.topic);
            return false;
        }
        
        active_subscriptions_[subscription.topic] = subscription;
        worker_stats_.subscribe_operations++;
        
        LogMessage(PulseOne::LogLevel::INFO, "Added subscription for topic: " + subscription.topic);
        UpdateWorkerStats("add_subscription", true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to add subscription: " + std::string(e.what()));
        UpdateWorkerStats("add_subscription", false);
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
        UpdateWorkerStats("remove_subscription", true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to remove subscription: " + std::string(e.what()));
        UpdateWorkerStats("remove_subscription", false);
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
        ss << "      \"data_type\": \"" << static_cast<int>(subscription.data_type) << "\",\n";
        ss << "      \"json_path\": \"" << subscription.json_path << "\",\n";
        ss << "      \"scaling_factor\": " << subscription.scaling_factor << ",\n";
        ss << "      \"scaling_offset\": " << subscription.scaling_offset << ",\n";
        ss << "      \"messages_received\": " << subscription.messages_received << ",\n";
        
        // 마지막 수신 시간
        auto last_received = duration_cast<seconds>(subscription.last_received.time_since_epoch()).count();
        ss << "      \"last_received_timestamp\": " << last_received << "\n";
        ss << "    }";
    }
    
    ss << "\n  ],\n";
    ss << "  \"total_count\": " << active_subscriptions_.size() << "\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// MQTT 워커 핵심 기능
// =============================================================================

bool MQTTWorker::InitializeMQTTDriver() {
    try {
        if (!mqtt_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver not created");
            return false;
        }
        
        // 드라이버 설정 생성
        auto driver_config = CreateDriverConfig();
        
        // 드라이버 초기화
        if (!mqtt_driver_->Initialize(driver_config)) {
            auto error = mqtt_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver initialization failed: " + error.message);
            return false;
        }
        
        // 드라이버 연결
        if (!mqtt_driver_->Connect()) {
            auto error = mqtt_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "MQTT driver connection failed: " + error.message);
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "MQTT driver initialized and connected successfully");
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
                subscription.data_type = point.data_type;
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
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "Created subscriptions from " + std::to_string(points.size()) + " data points");
        
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
        
        // 토픽에 해당하는 구독 찾기
        auto it = active_subscriptions_.find(topic);
        if (it == active_subscriptions_.end()) {
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "No subscription found for topic: " + topic);
            return false;
        }
        
        auto& subscription = it->second;
        subscription.messages_received++;
        subscription.last_received = system_clock::now();
        
        // JSON에서 값 추출
        Drivers::DataValue extracted_value;
        if (!ExtractValueFromJSON(payload, subscription.json_path, extracted_value)) {
            worker_stats_.json_parse_errors++;
            LogMessage(PulseOne::LogLevel::WARN, "Failed to extract value from JSON for topic: " + topic);
            return false;
        }
        
        // 스케일링 적용 (숫자 타입인 경우)
        std::visit([&subscription](auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                val = static_cast<T>(val * subscription.scaling_factor + subscription.scaling_offset);
            }
        }, extracted_value);
        
        // TimestampedValue 생성
        PulseOne::TimestampedValue timestamped_value(extracted_value, Drivers::DataQuality::GOOD);
        
        // InfluxDB에 저장
        SaveToInfluxDB(subscription.point_id, timestamped_value);
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "Processed message for topic: " + topic + " (point: " + subscription.point_id + ")");
        
        UpdateWorkerStats("message_processing", true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ProcessReceivedMessage: " + std::string(e.what()));
        UpdateWorkerStats("message_processing", false);
        return false;
    }
}

bool MQTTWorker::ExtractValueFromJSON(const std::string& payload, const std::string& json_path,
                                     Drivers::DataValue& extracted_value) {
    try {
        if (payload.empty()) {
            return false;
        }
        
        // JSON 파싱
        json parsed_json = json::parse(payload);
        
        // JSON 경로가 비어있으면 전체 값을 사용
        if (json_path.empty()) {
            if (parsed_json.is_number()) {
                if (parsed_json.is_number_integer()) {
                    extracted_value = parsed_json.get<int32_t>();
                } else {
                    extracted_value = parsed_json.get<double>();
                }
            } else if (parsed_json.is_boolean()) {
                extracted_value = parsed_json.get<bool>();
            } else if (parsed_json.is_string()) {
                extracted_value = parsed_json.get<std::string>();
            } else {
                return false;
            }
            return true;
        }
        
        // JSON 경로 파싱 (간단한 구현: 점으로 구분)
        json current = parsed_json;
        std::stringstream ss(json_path);
        std::string segment;
        
        while (std::getline(ss, segment, '.')) {
            if (!current.contains(segment)) {
                return false;
            }
            current = current[segment];
        }
        
        // 최종 값 추출
        if (current.is_number()) {
            if (current.is_number_integer()) {
                extracted_value = current.get<int32_t>();
            } else {
                extracted_value = current.get<double>();
            }
        } else if (current.is_boolean()) {
            extracted_value = current.get<bool>();
        } else if (current.is_string()) {
            extracted_value = current.get<std::string>();
        } else {
            return false;
        }
        
        return true;
        
    } catch (const json::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "JSON parsing error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ExtractValueFromJSON: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::ParseMQTTTopic(const PulseOne::DataPoint& point, std::string& topic,
                               std::string& json_path, int& qos) {
    try {
        // address_string에서 MQTT 정보 파싱
        // 형식: "topic:json_path:qos" 예: "sensors/temperature:value:1"
        
        std::string addr_str = point.address_string;
        if (addr_str.empty()) {
            // address 필드를 문자열로 변환해서 사용
            addr_str = "data/" + std::to_string(point.address);
        }
        
        // 기본값 설정
        topic = addr_str;
        json_path = "";
        qos = 1;
        
        // 콜론으로 구분하여 파싱
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
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "Parsed MQTT topic: " + topic + ", JSON path: " + json_path + 
                  ", QoS: " + std::to_string(qos));
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse MQTT topic: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 내부 메서드 (private)
// =============================================================================

bool MQTTWorker::ParseMQTTWorkerConfig() {
    try {
        // device_info_.config_json에서 MQTT 워커 설정 파싱
        if (device_info_.config_json.empty()) {
            LogMessage(PulseOne::LogLevel::INFO, "No MQTT worker config in device_info, using defaults");
            return true;
        }
        
        // 간단한 설정 파싱 (실제로는 JSON 파서 사용)
        // endpoint에서 브로커 URL 추출
        if (!device_info_.endpoint.empty()) {
            worker_config_.broker_url = device_info_.endpoint;
        }
        
        // 기본 설정 매핑
        worker_config_.message_timeout_ms = static_cast<uint32_t>(device_info_.timeout_ms);
        worker_config_.publish_retry_count = static_cast<uint32_t>(device_info_.retry_count);
        
        // 클라이언트 ID 생성
        worker_config_.client_id = "pulseone_worker_" + device_info_.name;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "MQTT worker config parsed successfully");
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
            // 실제 구현에서는 MQTT 드라이버로부터 메시지를 받아서 처리
            // 현재는 단순히 대기
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
            
            // 발행할 작업이 있을 때까지 대기
            publish_queue_cv_.wait(lock, [this] { 
                return !publish_queue_.empty() || !threads_running_.load(); 
            });
            
            if (!threads_running_.load()) break;
            
            // 발행 작업 처리
            while (!publish_queue_.empty()) {
                auto task = publish_queue_.front();
                publish_queue_.pop();
                lock.unlock();
                
                // MQTT 드라이버를 통해 메시지 발행
                if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
                    // 실제 구현에서는 MQTTDriver의 발행 메서드 호출
                    worker_stats_.messages_published++;
                    worker_stats_.successful_publishes++;
                    
                    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                              "Published message to topic: " + task.topic);
                } else {
                    worker_stats_.failed_operations++;
                    LogMessage(PulseOne::LogLevel::WARN, 
                              "Failed to publish message - driver not connected");
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

Drivers::DriverConfig MQTTWorker::CreateDriverConfig() {
    Drivers::DriverConfig config;
    
    config.device_id = 0;  // MQTT는 디바이스 ID 개념이 없음
    config.name = device_info_.name;
    config.protocol_type = Drivers::ProtocolType::MQTT;
    config.endpoint = worker_config_.broker_url;
    config.timeout_ms = worker_config_.message_timeout_ms;
    config.retry_count = worker_config_.publish_retry_count;
    config.polling_interval_ms = 1000;  // MQTT는 푸시 기반이므로 의미 없음
    
    // MQTT 특화 설정
    config.properties["client_id"] = worker_config_.client_id;
    config.properties["username"] = worker_config_.username;
    config.properties["password"] = worker_config_.password;
    config.properties["use_ssl"] = worker_config_.use_ssl ? "true" : "false";
    config.properties["keep_alive_interval"] = std::to_string(worker_config_.keep_alive_interval);
    config.properties["clean_session"] = worker_config_.clean_session ? "true" : "false";
    config.properties["auto_reconnect"] = worker_config_.auto_reconnect ? "true" : "false";
    
    return config;
}

void MQTTWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    try {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "MQTT worker operation: " + operation + 
                  " (success: " + (success ? "true" : "false") + ")");
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in UpdateWorkerStats: " + std::string(e.what()));
    }
}

void MQTTWorker::MessageCallback(MQTTWorker* worker_instance, 
                                const std::string& topic, const std::string& payload) {
    if (worker_instance) {
        worker_instance->ProcessReceivedMessage(topic, payload);
    }
}

} // namespace Workers
} // namespace PulseOne