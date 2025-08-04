// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 - 컴파일 에러 수정 완료
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Common/UnifiedCommonTypes.h"
#include "Drivers/Common/DriverFactory.h"
#include <optional>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <chrono>
#include <iostream>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include <mqtt/connect_options.h>
#include <mqtt/ssl_options.h>

namespace PulseOne {
namespace Drivers {

using namespace std::chrono;

// 타입 별칭들 - 헤더와 동일하게
using ErrorCode = PulseOne::Structs::ErrorCode;
using ErrorInfo = PulseOne::Structs::ErrorInfo;
using ProtocolType = PulseOne::Enums::ProtocolType;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataQuality = PulseOne::Enums::DataQuality;  // ✅ 추가

// =============================================================================
// MQTT 콜백 클래스 구현
// =============================================================================

class MqttCallbackImpl : public virtual mqtt::callback {
public:
    explicit MqttCallbackImpl(MqttDriver* driver) : driver_(driver) {}
    
    void connected(const std::string& cause) override {
        if (driver_) {
            driver_->OnConnected(cause);
        }
    }
    
    void connection_lost(const std::string& cause) override {
        if (driver_) {
            driver_->OnConnectionLost(cause);
        }
    }
    
    void message_arrived(mqtt::const_message_ptr msg) override {
        if (driver_) {
            driver_->OnMessageArrived(msg);
        }
    }
    
    void delivery_complete(mqtt::delivery_token_ptr token) override {
        if (driver_) {
            driver_->OnDeliveryComplete(token);
        }
    }

private:
    MqttDriver* driver_;
};

// =============================================================================
// 액션 리스너 클래스 구현
// =============================================================================

class MqttActionListener : public virtual mqtt::iaction_listener {
public:
    explicit MqttActionListener(MqttDriver* driver) : driver_(driver) {}
    
    void on_failure(const mqtt::token& token) override {
        if (driver_) {
            driver_->OnActionFailure(token);
        }
    }
    
    void on_success(const mqtt::token& token) override {
        if (driver_) {
            driver_->OnActionSuccess(token);
        }
    }

private:
    MqttDriver* driver_;
};

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttDriver::MqttDriver()
    : driver_statistics_("MQTT")  // ✅ 표준 통계 초기화
    , status_(Structs::DriverStatus::UNINITIALIZED)
    , is_connected_(false)
    , connection_in_progress_(false)
    , last_error_()
    , broker_url_()
    , client_id_()
    , default_qos_(0)
    , keep_alive_seconds_(60)
    , timeout_ms_(30000)
    , clean_session_(true)
    , auto_reconnect_(true)
    , message_processor_running_(false)
    , connection_monitor_running_(false)
    , console_output_enabled_(false)
    , packet_logging_enabled_(false)
    , connection_start_time_(system_clock::now())
{
    // ✅ MQTT 특화 통계 카운터 초기화
    InitializeMqttCounters();
    
    // 기본 설정
    client_id_ = GenerateClientId();
    
    LogMessage("INFO", "MqttDriver created with client_id: " + client_id_, "MQTT");
}

MqttDriver::~MqttDriver() {
    // 정리 작업
    Stop();
    Disconnect();
    CleanupMqttClient();
    
    LogMessage("INFO", "MqttDriver destroyed", "MQTT");
}

void MqttDriver::InitializeMqttCounters() {
    // ✅ 표준 DriverStatistics에 MQTT 특화 카운터들 초기화
    driver_statistics_.IncrementProtocolCounter("mqtt_messages", 0);
    driver_statistics_.IncrementProtocolCounter("published_messages", 0);
    driver_statistics_.IncrementProtocolCounter("received_messages", 0);
    driver_statistics_.IncrementProtocolCounter("qos0_messages", 0);
    driver_statistics_.IncrementProtocolCounter("qos1_messages", 0);
    driver_statistics_.IncrementProtocolCounter("qos2_messages", 0);
    driver_statistics_.IncrementProtocolCounter("retained_messages", 0);
    driver_statistics_.IncrementProtocolCounter("connection_failures", 0);
    driver_statistics_.IncrementProtocolCounter("subscription_count", 0);
    
    // MQTT 특화 메트릭들 초기화
    driver_statistics_.SetProtocolMetric("avg_message_size_bytes", 0.0);
    driver_statistics_.SetProtocolMetric("max_message_size_bytes", 0.0);
    driver_statistics_.SetProtocolMetric("broker_latency_ms", 0.0);
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig& config) {
    try {
        LogMessage("INFO", "Initializing MQTT driver", "MQTT");
        
        // 설정 파싱
        if (!ParseDriverConfig(config)) {
            SetError("Failed to parse driver configuration");
            return false;
        }
        
        // MQTT 클라이언트 초기화
        if (!InitializeMqttClient()) {
            SetError("Failed to initialize MQTT client");
            return false;
        }
        
        status_ = Structs::DriverStatus::INITIALIZED;
        LogMessage("INFO", "MQTT driver initialized successfully", "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Connect() {
    return EstablishConnection();
}

bool MqttDriver::Disconnect() {
    try {
        if (mqtt_client_ && mqtt_client_->is_connected()) {
            auto token = mqtt_client_->disconnect();
            token->wait();
        }
        
        is_connected_ = false;
        status_ = Structs::DriverStatus::STOPPED;  // ✅ DISCONNECTED → STOPPED
        LogMessage("INFO", "MQTT driver disconnected", "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Exception during disconnect: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::IsConnected() const {
    return is_connected_.load() && mqtt_client_ && mqtt_client_->is_connected();
}

bool MqttDriver::ReadValues(const std::vector<DataPoint>& points, 
                           std::vector<TimestampedValue>& values) {
    if (!IsConnected()) {
        SetError("MQTT client not connected");
        return false;
    }
    
    auto start_time = steady_clock::now();
    
    try {
        values.reserve(points.size());
        
        for (const auto& point : points) {
            TimestampedValue value;
            // ✅ point_id → 없음, TimestampedValue는 value, quality, timestamp만 가짐
            value.timestamp = system_clock::now();
            
            // MQTT에서는 구독된 토픽의 마지막 값을 반환
            // 실제 구현에서는 메시지 캐시에서 값을 조회
            value.value = std::string("subscribed");  // ✅ DataValue는 variant 자체
            value.quality = DataQuality::GOOD;  // ✅ enum 사용
            
            values.push_back(value);
        }
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        // 통계 업데이트
        UpdateStats("read", true, duration_ms);
        
        return true;
        
    } catch (const std::exception& e) {
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        UpdateStats("read", false, duration_ms);
        SetError("Exception during read: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    if (!IsConnected()) {
        SetError("MQTT client not connected");
        return false;
    }
    
    try {
        // DataValue를 문자열로 변환 (variant는 직접 접근)
        std::string payload;
        if (std::holds_alternative<std::string>(value)) {
            payload = std::get<std::string>(value);
        } else if (std::holds_alternative<double>(value)) {
            payload = std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<int>(value)) {  // ✅ int64_t → int
            payload = std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<bool>(value)) {
            payload = std::get<bool>(value) ? "true" : "false";
        }
        
        return Publish(std::to_string(point.address), payload, default_qos_, false);  // ✅ address를 string으로 변환
        
    } catch (const std::exception& e) {
        SetError("Exception during write: " + std::string(e.what()));
        return false;
    }
}

const DriverStatistics& MqttDriver::GetStatistics() const {
    return driver_statistics_;
}

void MqttDriver::ResetStatistics() {
    driver_statistics_.ResetStatistics();
    InitializeMqttCounters();  // MQTT 카운터들 재초기화
    LogMessage("INFO", "MQTT driver statistics reset", "MQTT");
}

ProtocolType MqttDriver::GetProtocolType() const {
    return ProtocolType::MQTT;
}

Structs::DriverStatus MqttDriver::GetStatus() const {
    return status_.load();
}

ErrorInfo MqttDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

// =============================================================================
// MQTT 특화 메서드들
// =============================================================================

bool MqttDriver::Subscribe(const std::string& topic, int qos) {
    if (!IsConnected()) {
        SetError("MQTT client not connected");
        return false;
    }
    
    auto start_time = steady_clock::now();
    
    try {
        auto token = mqtt_client_->subscribe(topic, qos);
        token->wait();
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_[topic] = qos;
        }
        
        UpdateStats("subscribe", true, duration_ms);
        driver_statistics_.IncrementProtocolCounter("subscription_count", 1);
        
        LogMessage("INFO", "Subscribed to topic: " + topic + " with QoS: " + std::to_string(qos), "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        UpdateStats("subscribe", false, duration_ms);
        SetError("Failed to subscribe to topic " + topic + ": " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Unsubscribe(const std::string& topic) {
    if (!IsConnected()) {
        SetError("MQTT client not connected");
        return false;
    }
    
    try {
        auto token = mqtt_client_->unsubscribe(topic);
        token->wait();
        
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_.erase(topic);
        }
        
        driver_statistics_.IncrementProtocolCounter("subscription_count", -1);
        LogMessage("INFO", "Unsubscribed from topic: " + topic, "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to unsubscribe from topic " + topic + ": " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!IsConnected()) {
        SetError("MQTT client not connected");
        return false;
    }
    
    auto start_time = steady_clock::now();
    
    try {
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retain);
        
        auto token = mqtt_client_->publish(msg);
        bool success = token->wait_for(std::chrono::milliseconds(timeout_ms_));
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        if (success) {
            UpdateStats("publish", true, duration_ms);
            driver_statistics_.IncrementProtocolCounter("published_messages", 1);
            
            if (retain) {
                driver_statistics_.IncrementProtocolCounter("retained_messages", 1);
            }
            
            LogMessage("DEBUG", "Published to topic: " + topic, "MQTT");
            return true;
        } else {
            UpdateStats("publish", false, duration_ms);
            SetError("Publish timeout for topic: " + topic);
            return false;
        }
        
    } catch (const std::exception& e) {
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        UpdateStats("publish", false, duration_ms);
        SetError("Failed to publish to topic " + topic + ": " + std::string(e.what()));
        return false;
    }
}

void MqttDriver::UpdateStats(const std::string& operation, bool success, double duration_ms) {
    if (operation == "read") {
        driver_statistics_.total_reads.fetch_add(1);
        if (success) {
            driver_statistics_.successful_reads.fetch_add(1);
        } else {
            driver_statistics_.failed_reads.fetch_add(1);
        }
    } else if (operation == "write" || operation == "publish") {
        driver_statistics_.total_writes.fetch_add(1);
        if (success) {
            driver_statistics_.successful_writes.fetch_add(1);
        } else {
            driver_statistics_.failed_writes.fetch_add(1);
        }
    }
    
    // MQTT 메시지 카운터 업데이트
    driver_statistics_.IncrementProtocolCounter("mqtt_messages", 1);
    
    // ✅ total_errors 제거 - DriverStatistics에 없음
    if (!success) {
        driver_statistics_.failed_reads.fetch_add(1);  // 실패 카운터는 따로 관리
    }
    
    // 응답 시간 통계는 DriverStatistics에서 내부적으로 처리
    // duration_ms 사용 (경고 제거)
    (void)duration_ms;  // 경고 방지
}

// =============================================================================
// 콜백 핸들러들
// =============================================================================

void MqttDriver::OnConnected(const std::string& cause) {
    is_connected_ = true;
    status_ = Structs::DriverStatus::RUNNING;
    connection_start_time_ = system_clock::now();
    
    LogMessage("INFO", "MQTT connected: " + cause, "MQTT");
    
    // 구독 복원
    RestoreSubscriptions();
}

void MqttDriver::OnConnectionLost(const std::string& cause) {
    is_connected_ = false;
    status_ = Structs::DriverStatus::ERROR;
    
    LogMessage("WARN", "MQTT connection lost: " + cause, "MQTT");
    
    if (auto_reconnect_) {
        HandleConnectionLoss(cause);
    }
}

void MqttDriver::OnMessageArrived(mqtt::const_message_ptr msg) {
    if (!msg) return;
    
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        int qos = msg->get_qos();
        
        ProcessReceivedMessage(topic, payload, qos);
        
        driver_statistics_.IncrementProtocolCounter("received_messages", 1);
        
        // QoS별 통계
        switch(qos) {
            case 0: driver_statistics_.IncrementProtocolCounter("qos0_messages", 1); break;
            case 1: driver_statistics_.IncrementProtocolCounter("qos1_messages", 1); break;
            case 2: driver_statistics_.IncrementProtocolCounter("qos2_messages", 1); break;
        }
        
        LogPacket("RECV", topic, payload, qos);
        
    } catch (const std::exception& e) {
        LogMessage("ERROR", "Exception in message handler: " + std::string(e.what()), "MQTT");
    }
}

void MqttDriver::OnDeliveryComplete(mqtt::delivery_token_ptr token) {
    // 메시지 전송 완료
    LogMessage("DEBUG", "Message delivery complete", "MQTT");
    (void)token;  // 경고 방지
}

void MqttDriver::OnActionFailure(const mqtt::token& token) {
    std::string error_msg = "MQTT operation failed";
    LogMessage("ERROR", error_msg, "MQTT");
    SetError(error_msg);
    (void)token;  // 경고 방지
}

void MqttDriver::OnActionSuccess(const mqtt::token& token) {
    LogMessage("DEBUG", "MQTT operation successful", "MQTT");
    (void)token;  // 경고 방지
}

// =============================================================================
// 지원 메서드들
// =============================================================================

std::string MqttDriver::GenerateClientId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "PulseOne_MQTT_" << time_t;
    return oss.str();
}

bool MqttDriver::Start() {
    if (message_processor_running_) {
        return true;  // 이미 시작됨
    }
    
    try {
        message_processor_running_ = true;
        message_processor_thread_ = std::thread(&MqttDriver::MessageProcessorLoop, this);
        
        if (auto_reconnect_) {
            connection_monitor_running_ = true;
            connection_monitor_thread_ = std::thread(&MqttDriver::ConnectionMonitorLoop, this);
        }
        
        status_ = Structs::DriverStatus::RUNNING;
        LogMessage("INFO", "MQTT driver started", "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to start MQTT driver: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Stop() {
    message_processor_running_ = false;
    connection_monitor_running_ = false;
    
    // 조건 변수들 깨우기
    message_queue_cv_.notify_all();
    connection_cv_.notify_all();
    
    // 스레드들 정리
    if (message_processor_thread_.joinable()) {
        message_processor_thread_.join();
    }
    
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
    
    status_ = Structs::DriverStatus::STOPPED;
    LogMessage("INFO", "MQTT driver stopped", "MQTT");
    return true;
}

void MqttDriver::SetError(const std::string& error_message) {
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_.code = ErrorCode::DEVICE_ERROR;
        last_error_.message = error_message;
        // ✅ timestamp 필드 제거 - ErrorInfo에 occurred_at이 자동 설정됨
    }
    
    status_ = Structs::DriverStatus::ERROR;
    LogMessage("ERROR", error_message, "MQTT");
}

void MqttDriver::LogMessage(const std::string& level, const std::string& message, const std::string& category) const {
    // ✅ logger_는 객체이므로 포인터 체크 불가, 객체 존재 여부를 다른 방식으로 확인
    // 현재는 단순하게 콘솔 출력만 사용
    
    // 콘솔 출력
    if (console_output_enabled_) {
        std::cout << "[" << level << "] [" << category << "] " << message << std::endl;
    }  
}

void MqttDriver::LogPacket(const std::string& direction, const std::string& topic, 
                          const std::string& payload, int qos) const {
    if (packet_logging_enabled_) {
        std::ostringstream log_msg;
        log_msg << direction << " Topic: " << topic 
                << " QoS: " << qos 
                << " Payload: " << payload.substr(0, 100);
        if (payload.size() > 100) {
            log_msg << "... (" << payload.size() << " bytes total)";
        }
        
        LogMessage("DEBUG", log_msg.str(), "PACKET");
    }
}

// =============================================================================
// 개별 메서드들 (간소화 버전)
// =============================================================================

bool MqttDriver::EstablishConnection() {
    if (connection_in_progress_) {
        return false;
    }
    
    connection_in_progress_ = true;
    auto start_time = steady_clock::now();
    
    try {
        if (!mqtt_client_) {
            SetError("MQTT client not initialized");
            connection_in_progress_ = false;
            return false;
        }
        
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(keep_alive_seconds_);
        connOpts.set_clean_session(clean_session_);
        connOpts.set_automatic_reconnect(auto_reconnect_);
        
        auto token = mqtt_client_->connect(connOpts);
        bool success = token->wait_for(std::chrono::milliseconds(timeout_ms_));
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        UpdateStats("connect", success, duration_ms);
        
        if (success) {
            is_connected_ = true;
            status_ = Structs::DriverStatus::RUNNING;
            LogMessage("INFO", "Successfully connected to MQTT broker: " + broker_url_, "MQTT");
        } else {
            SetError("Failed to connect to MQTT broker");
        }
        
        connection_in_progress_ = false;
        return success;
        
    } catch (const std::exception& e) {
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        UpdateStats("connect", false, duration_ms);
        SetError("MQTT connection exception: " + std::string(e.what()));
        connection_in_progress_ = false;
        return false;
    }
}

bool MqttDriver::InitializeMqttClient() {
    try {
        if (broker_url_.empty()) {
            SetError("Broker URL not specified");
            return false;
        }
        
        mqtt_client_ = std::make_unique<mqtt::async_client>(broker_url_, client_id_);
        
        // 콜백 설정
        mqtt_callback_ = std::make_shared<MqttCallbackImpl>(this);
        mqtt_action_listener_ = std::make_shared<MqttActionListener>(this);
        
        mqtt_client_->set_callback(*mqtt_callback_);
        
        LogMessage("INFO", "MQTT client initialized for broker: " + broker_url_, "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to initialize MQTT client: " + std::string(e.what()));
        return false;
    }
}

void MqttDriver::CleanupMqttClient() {
    if (mqtt_client_) {
        try {
            if (mqtt_client_->is_connected()) {
                mqtt_client_->disconnect();
            }
        } catch (...) {
            // 무시
        }
        mqtt_client_.reset();
    }
    
    mqtt_callback_.reset();
    mqtt_action_listener_.reset();
}

bool MqttDriver::ParseDriverConfig(const DriverConfig& config) {
    try {
        if (!config.endpoint.empty()) {
            broker_url_ = config.endpoint;
        }
        
        if (config.name.empty()) {
            client_id_ = GenerateClientId();
        } else {
            client_id_ = config.name + "_" + std::to_string(std::time(nullptr));
        }
        
        // JSON 설정이 있다면 파싱
        if (!config.connection_string.empty()) {
            try {
                auto json_config = json::parse(config.connection_string);
                
                if (json_config.contains("qos")) {
                    default_qos_ = json_config["qos"];
                }
                if (json_config.contains("keep_alive")) {
                    keep_alive_seconds_ = json_config["keep_alive"];
                }
                if (json_config.contains("clean_session")) {
                    clean_session_ = json_config["clean_session"];
                }
                if (json_config.contains("auto_reconnect")) {
                    auto_reconnect_ = json_config["auto_reconnect"];
                }
                if (json_config.contains("timeout_ms")) {
                    timeout_ms_ = json_config["timeout_ms"];
                }
                
            } catch (const json::exception& e) {
                LogMessage("WARN", "Failed to parse JSON config: " + std::string(e.what()), "MQTT");
            }
        }
        
        LogMessage("INFO", "MQTT configuration parsed successfully", "MQTT");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to parse driver config: " + std::string(e.what()));
        return false;
    }
}

void MqttDriver::ProcessReceivedMessage(const std::string& topic, const std::string& payload, int qos) {
    // 기본 메시지 처리 로직
    LogMessage("DEBUG", "Processing message from topic: " + topic, "MQTT");
    
    // TODO: 실제 데이터 처리 로직 구현
    // 예: JSON 파싱, DataPoint로 변환, 데이터베이스 저장 등
    
    // 경고 방지
    (void)payload;
    (void)qos;
}

void MqttDriver::MessageProcessorLoop() {
    while (message_processor_running_) {
        std::unique_lock<std::mutex> lock(message_queue_mutex_);
        message_queue_cv_.wait(lock, [this] { 
            return !message_queue_.empty() || !message_processor_running_; 
        });
        
        while (!message_queue_.empty() && message_processor_running_) {
            auto [topic, payload] = message_queue_.front();
            message_queue_.pop();
            lock.unlock();
            
            ProcessReceivedMessage(topic, payload, 0);
            
            lock.lock();
        }
    }
}

void MqttDriver::ConnectionMonitorLoop() {
    while (connection_monitor_running_) {
        std::unique_lock<std::mutex> lock(connection_mutex_);
        connection_cv_.wait_for(lock, std::chrono::seconds(30), [this] { 
            return !connection_monitor_running_; 
        });
        
        if (!connection_monitor_running_) break;
        
        // 연결 상태 확인 및 재연결 시도
        if (!IsConnected() && ShouldReconnect()) {
            LogMessage("INFO", "Attempting automatic reconnection...", "MQTT");
            EstablishConnection();
        }
    }
}

bool MqttDriver::ShouldReconnect() const {
    return auto_reconnect_ && !connection_in_progress_;
}

bool MqttDriver::RestoreSubscriptions() {
    if (!IsConnected()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    bool all_success = true;
    for (const auto& [topic, qos] : subscriptions_) {
        try {
            auto token = mqtt_client_->subscribe(topic, qos);
            if (!token->wait_for(std::chrono::milliseconds(timeout_ms_))) {
                all_success = false;
            }
        } catch (const std::exception& e) {
            all_success = false;
        }
    }
    
    return all_success;
}

void MqttDriver::HandleConnectionLoss(const std::string& cause) {
    driver_statistics_.IncrementProtocolCounter("connection_failures", 1);
    
    LogMessage("INFO", "Will attempt to reconnect due to: " + cause, "MQTT");
    
    // 재연결 로직은 ConnectionMonitorLoop에서 처리
}

} // namespace Drivers
} // namespace PulseOne