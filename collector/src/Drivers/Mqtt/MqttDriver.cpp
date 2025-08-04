// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 완성본 - 표준화 + 확장성 준비
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Common/UnifiedCommonTypes.h"
#include "Drivers/Common/DriverFactory.h"
#include <optional>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

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

// =============================================================================
// MQTT 콜백 클래스 구현 (기존 유지)
// =============================================================================

class MqttCallbackImpl : public virtual mqtt::callback {
public:
    explicit MqttCallbackImpl(MqttDriver* driver) : driver_(driver) {}
    
    void connected(const std::string& cause) override {
        if (driver_) {
            driver_->connected(cause);
        }
    }
    
    void connection_lost(const std::string& cause) override {
        if (driver_) {
            driver_->connection_lost(cause);
        }
    }
    
    void message_arrived(mqtt::const_message_ptr msg) override {
        if (driver_) {
            driver_->message_arrived(msg);
        }
    }
    
    void delivery_complete(mqtt::delivery_token_ptr token) override {
        if (driver_) {
            driver_->delivery_complete(token);
        }
    }

private:
    MqttDriver* driver_;
};

// =============================================================================
// 액션 리스너 클래스 구현 (기존 유지)
// =============================================================================

class MqttActionListener : public virtual mqtt::iaction_listener {
public:
    explicit MqttActionListener(MqttDriver* driver) : driver_(driver) {}
    
    void on_failure(const mqtt::token& token) override {
        if (driver_) {
            driver_->on_failure(token);
        }
    }
    
    void on_success(const mqtt::token& token) override {
        if (driver_) {
            driver_->on_success(token);
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
    , stop_workers_(false)
    , need_reconnect_(false)
    , last_error_()
    , default_qos_(0)
    , keep_alive_seconds_(60)
    , retry_count_(3)
    , timeout_ms_(30000)
    , clean_session_(true)
    , auto_reconnect_(true)
    , connection_monitor_running_(false)
    , message_processor_running_(false)
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false)
    , console_output_enabled_(false)
    , last_successful_operation_(system_clock::now())
{
    // ✅ MQTT 특화 통계 카운터 초기화 (표준 DriverStatistics 사용)
    InitializeMqttCounters();
    
    // 기본 설정
    client_id_ = GenerateClientId();
    
    LogMessage("INFO", "MqttDriver created with client_id: " + client_id_);
}

MqttDriver::~MqttDriver() {
    // 정리 작업
    Stop();
    Disconnect();
    CleanupMqttClient();
    
    LogMessage("INFO", "MqttDriver destroyed");
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
    driver_statistics_.IncrementProtocolCounter("total_bytes_sent", 0);
    driver_statistics_.IncrementProtocolCounter("total_bytes_received", 0);
    driver_statistics_.IncrementProtocolCounter("subscription_count", 0);
    
    // ✅ MQTT 특화 메트릭 초기화
    driver_statistics_.SetProtocolMetric("avg_message_size_bytes", 0.0);
    driver_statistics_.SetProtocolMetric("message_loss_rate", 0.0);
    driver_statistics_.SetProtocolMetric("avg_latency_ms", 0.0);
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig& config) {
    try {
        if (!ParseDriverConfig(config)) {
            SetError("Failed to parse MQTT driver configuration");
            return false;
        }
        
        if (!InitializeMqttClient()) {
            SetError("Failed to initialize MQTT client");
            return false;
        }
        
        status_ = Structs::DriverStatus::INITIALIZED;
        LogMessage("INFO", "MQTT driver initialized successfully for broker: " + broker_url_);
        return true;
        
    } catch (const std::exception& e) {
        SetError("Exception during MQTT initialization: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Connect() {
    if (IsConnected()) {
        return true;
    }
    
    return EstablishConnection();
}

bool MqttDriver::Disconnect() {
    if (!IsConnected()) {
        return true;
    }
    
    try {
        // 스레드 정리
        Stop();
        
        // MQTT 클라이언트 연결 해제
        if (mqtt_client_ && mqtt_client_->is_connected()) {
            mqtt::disconnect_options disc_opts;
            disc_opts.set_timeout(std::chrono::seconds(5));
            
            auto token = mqtt_client_->disconnect(disc_opts);
            token->wait();
        }
        
        is_connected_ = false;
        status_ = Structs::DriverStatus::STOPPED;
        
        // 구독 정보 정리
        ClearSubscriptions();
        
        // 연결 시간 통계 업데이트
        UpdateConnectionStats(false, "User requested disconnect");
        
        LogMessage("INFO", "Disconnected from MQTT broker");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Exception during MQTT disconnect: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::IsConnected() const {
    return is_connected_.load() && mqtt_client_ && mqtt_client_->is_connected();
}

bool MqttDriver::ReadValues(const std::vector<DataPoint>& points, std::vector<TimestampedValue>& values) {
    // MQTT는 구독 기반이므로 실제 읽기 요청보다는 구독 상태 확인
    values.clear();
    
    if (!IsConnected()) {
        UpdateStats("read", false);
        return false;
    }
    
    // 구독된 토픽들에 대한 정보 반환 (예시)
    for (const auto& point : points) {
        if (IsSubscribed(point.name)) {
            TimestampedValue value;
            value.timestamp = std::chrono::system_clock::now();
            value = "subscribed";  // 실제로는 마지막 수신 값
            values.push_back(value);
        }
    }
    
    UpdateStats("read", true);
    return true;
}

bool MqttDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    // MQTT에서는 발행으로 쓰기 구현
    std::string payload;
    
    // DataValue를 문자열로 변환
    if (std::holds_alternative<std::string>(value)) {
        payload = std::get<std::string>(value);
    } else if (std::holds_alternative<double>(value)) {
        payload = std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<int>(value)) {
        payload = std::to_string(std::get<int>(value));
    } else if (std::holds_alternative<bool>(value)) {
        payload = std::get<bool>(value) ? "true" : "false";
    }
    
    return Publish(point.name, payload, default_qos_, false);
}

// =============================================================================
// ✅ 표준 통계 인터페이스 구현 (ModbusDriver와 동일한 패턴)
// =============================================================================

const DriverStatistics& MqttDriver::GetStatistics() const {
    // ✅ 심플화됨 - DriverStatistics 내부에 뮤텍스 포함되므로 별도 락 불필요
    return driver_statistics_;
}

void MqttDriver::ResetStatistics() {
    // ✅ 표준 리셋
    driver_statistics_.ResetStatistics();
    
    // ✅ MQTT 특화 카운터 재초기화
    InitializeMqttCounters();
    
    LogMessage("INFO", "MQTT statistics reset");
}

void MqttDriver::UpdateStats(const std::string& operation, bool success, double duration_ms) {
    // ✅ 표준 기본 통계 업데이트
    if (operation == "read" || operation == "receive" || operation == "subscribe") {
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
    
    // ✅ MQTT 특화 통계 업데이트
    driver_statistics_.IncrementProtocolCounter("mqtt_messages", 1);
    
    if (operation == "publish") {
        driver_statistics_.IncrementProtocolCounter("published_messages", 1);
    } else if (operation == "receive") {
        driver_statistics_.IncrementProtocolCounter("received_messages", 1);
    }
    
    if (!success) {
        if (operation == "connect") {
            driver_statistics_.IncrementProtocolCounter("connection_failures", 1);
        }
    }
    
    // ✅ 응답 시간 업데이트
    if (duration_ms > 0) {
        driver_statistics_.UpdateResponseTimeStats(duration_ms);
    }
    
    // 마지막 성공 작업 시간 업데이트
    if (success) {
        last_successful_operation_ = system_clock::now();
    }
}

void MqttDriver::UpdateMessageStats(const std::string& operation, size_t payload_size, int qos, bool success) {
    UpdateStats(operation, success);
    
    // ✅ 바이트 통계 업데이트
    if (operation == "publish") {
        driver_statistics_.IncrementProtocolCounter("total_bytes_sent", payload_size);
    } else if (operation == "receive") {
        driver_statistics_.IncrementProtocolCounter("total_bytes_received", payload_size);
    }
    
    // QoS별 통계 업데이트
    switch(qos) {
        case 0: driver_statistics_.IncrementProtocolCounter("qos0_messages", 1); break;
        case 1: driver_statistics_.IncrementProtocolCounter("qos1_messages", 1); break;
        case 2: driver_statistics_.IncrementProtocolCounter("qos2_messages", 1); break;
    }
    
    // 평균 메시지 크기 업데이트
    double current_avg = driver_statistics_.GetProtocolMetric("avg_message_size_bytes");
    uint64_t total_messages = driver_statistics_.GetProtocolCounter("mqtt_messages");
    if (total_messages > 0) {
        double new_avg = ((current_avg * (total_messages - 1)) + payload_size) / total_messages;
        driver_statistics_.SetProtocolMetric("avg_message_size_bytes", new_avg);
    }
}

// =============================================================================
// MQTT 핵심 기능 구현
// =============================================================================

bool MqttDriver::Publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!IsConnected()) {
        SetError("Cannot publish: not connected to broker");
        UpdateMessageStats("publish", payload.size(), qos, false);
        return false;
    }
    
    auto start_time = steady_clock::now();
    
    try {
        // MQTT 메시지 생성
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retain);
        
        // 비동기 발행
        auto token = mqtt_client_->publish(msg);
        token->wait();  // 동기적으로 완료 대기
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        // ✅ 표준화된 통계 업데이트
        UpdateMessageStats("publish", payload.size(), qos, true);
        
        if (retain) {
            driver_statistics_.IncrementProtocolCounter("retained_messages", 1);
        }
        
        // 패킷 로깅
        if (packet_logging_enabled_) {
            LogPacket("SEND", topic, payload, qos);
        }
        
        LogMessage("DEBUG", "Published to topic '" + topic + "' (QoS " + std::to_string(qos) + ")");
        return true;
        
    } catch (const mqtt::exception& e) {
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        UpdateMessageStats("publish", payload.size(), qos, false);
        SetError("Failed to publish message: " + std::string(e.what()));
        
        LogMessage("ERROR", "Publish failed for topic '" + topic + "': " + e.what());
        return false;
    }
}

bool MqttDriver::Subscribe(const std::string& topic, int qos) {
    if (!IsConnected()) {
        SetError("Cannot subscribe: not connected to broker");
        UpdateStats("subscribe", false);
        return false;
    }
    
    // 이미 구독 중인지 확인
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        if (subscriptions_.find(topic) != subscriptions_.end()) {
            return true; // 이미 구독 중
        }
    }
    
    try {
        auto token = mqtt_client_->subscribe(topic, qos);
        token->wait();
        
        // 구독 정보 저장
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_[topic] = qos;
            driver_statistics_.IncrementProtocolCounter("subscription_count", subscriptions_.size());
        }
        
        UpdateStats("subscribe", true);
        LogMessage("INFO", "Subscribed to topic '" + topic + "' (QoS " + std::to_string(qos) + ")");
        return true;
        
    } catch (const mqtt::exception& e) {
        UpdateStats("subscribe", false);
        SetError("Failed to subscribe to topic: " + std::string(e.what()));
        LogMessage("ERROR", "Subscribe failed for topic '" + topic + "': " + e.what());
        return false;
    }
}

bool MqttDriver::Unsubscribe(const std::string& topic) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        auto token = mqtt_client_->unsubscribe(topic);
        token->wait();
        
        // 구독 정보 제거
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_.erase(topic);
            driver_statistics_.IncrementProtocolCounter("subscription_count", subscriptions_.size());
        }
        
        LogMessage("INFO", "Unsubscribed from topic '" + topic + "'");
        return true;
        
    } catch (const mqtt::exception& e) {
        SetError("Failed to unsubscribe from topic: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Eclipse Paho 콜백 구현
// =============================================================================

void MqttDriver::connected(const std::string& cause) {
    is_connected_ = true;
    connection_in_progress_ = false;
    status_ = Structs::DriverStatus::RUNNING;
    connection_start_time_ = system_clock::now();
    
    UpdateConnectionStats(true, cause);
    
    // 구독 복원
    RestoreSubscriptions();
    
    LogMessage("INFO", "Connected to MQTT broker: " + cause);
    
    if (connection_callback_) {
        connection_callback_(true, cause);
    }
}

void MqttDriver::connection_lost(const std::string& cause) {
    is_connected_ = false;
    status_ = Structs::DriverStatus::ERROR;
    
    UpdateConnectionStats(false, cause);
    HandleConnectionLoss(cause);
    
    LogMessage("WARN", "Connection lost: " + cause);
    
    if (connection_callback_) {
        connection_callback_(false, cause);
    }
}

void MqttDriver::message_arrived(mqtt::const_message_ptr msg) {
    if (!msg) return;
    
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();
    int qos = msg->get_qos();
    
    // ✅ 표준화된 통계 업데이트
    UpdateMessageStats("receive", payload.size(), qos, true);
    
    // 패킷 로깅
    if (packet_logging_enabled_) {
        LogPacket("RECV", topic, payload, qos);
    }
    
    LogMessage("DEBUG", "Message arrived from topic '" + topic + "' (QoS " + std::to_string(qos) + ")");
    
    // 메시지 큐에 추가 (비동기 처리)
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        message_queue_.push({topic, payload});
    }
    message_queue_cv_.notify_one();
    
    // 즉시 콜백 호출 (옵션)
    if (message_callback_) {
        message_callback_(topic, payload, qos);
    }
}

void MqttDriver::delivery_complete(mqtt::delivery_token_ptr token) {
    LogMessage("DEBUG", "Message delivery complete: " + std::to_string(token->get_message_id()));
}

void MqttDriver::on_failure(const mqtt::token& token) {
    std::string error_msg = "MQTT operation failed";
    }
    
    LogMessage("ERROR", error_msg);
    SetError(error_msg);
}

void MqttDriver::on_success(const mqtt::token& token) {
    LogMessage("DEBUG", "MQTT operation successful");
}

// =============================================================================
// 연결 관리
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
        
        // 연결 옵션 설정
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(keep_alive_seconds_);
        connOpts.set_clean_session(clean_session_);
        connOpts.set_automatic_reconnect(auto_reconnect_);
        
        // 비동기 연결 시작
        auto token = mqtt_client_->connect(connOpts);
        token->wait();
        
        auto end_time = steady_clock::now();
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        bool success = mqtt_client_->is_connected();
        UpdateStats("connect", success, duration_ms);
        
        if (success) {
            is_connected_ = true;
            status_ = Structs::DriverStatus::RUNNING;
            LogMessage("INFO", "Successfully connected to MQTT broker: " + broker_url_);
        } else {
            SetError("Failed to connect to MQTT broker");
        }
        
        connection_in_progress_ = false;
        return success;
        
    } catch (const mqtt::exception& e) {
        auto end_time = steady_clock::now(); 
        double duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        
        UpdateStats("connect", false, duration_ms);
        SetError("MQTT connection exception: " + std::string(e.what()));
        
        connection_in_progress_ = false;
        return false;
    }
}

// =============================================================================
// 초기화 및 설정
// =============================================================================

bool MqttDriver::InitializeMqttClient() {
    try {
        if (broker_url_.empty()) {
            SetError("Broker URL not specified");
            return false;
        }
        
        // MQTT 클라이언트 생성
        mqtt_client_ = std::make_unique<mqtt::async_client>(broker_url_, client_id_);
        
        // 콜백 생성 및 설정
        mqtt_callback_ = std::make_shared<MqttCallbackImpl>(this);
        mqtt_action_listener_ = std::make_shared<MqttActionListener>(this);
        
        mqtt_client_->set_callback(*mqtt_callback_);
        
        LogMessage("INFO", "MQTT client initialized for broker: " + broker_url_);
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
            // 정리 중 예외 무시
        }
        
        mqtt_client_.reset();
    }
    
    mqtt_callback_.reset();
    mqtt_action_listener_.reset();
}

bool MqttDriver::ParseDriverConfig(const DriverConfig& config) {
    try {
        // 기본 설정
        if (!config.endpoint.empty()) {
            broker_url_ = config.endpoint;
            
            // mqtt:// 또는 mqtts:// 프로토콜 확인
            if (broker_url_.find("://") == std::string::npos) {
                broker_url_ = "tcp://" + broker_url_;
            }
        }
        
        if (!config.name.empty()) {
            client_id_ = config.name + "_" + std::to_string(std::time(nullptr));
        }
        
        // JSON 설정 파싱 (옵션)
#ifdef HAS_NLOHMANN_JSON
        if (!config.connection_params.empty()) {
            try {
                auto json_config = json::parse(config.connection_params);
                
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
                LogMessage("WARN", "Failed to parse JSON config: " + std::string(e.what()));
            }
        }
#endif
        
        LogMessage("INFO", "MQTT configuration parsed successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to parse driver config: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 스레드 관리 및 상태 관리
// =============================================================================

bool MqttDriver::Start() {
    if (message_processor_running_) {
        return true;
    }
    
    try {
        // 메시지 처리 스레드 시작
        message_processor_running_ = true;
        message_processor_thread_ = std::thread(&MqttDriver::MessageProcessorLoop, this);
        
        // 연결 모니터 스레드 시작 (자동 재연결 활성화시)
        if (auto_reconnect_) {
            connection_monitor_running_ = true;
            connection_monitor_thread_ = std::thread(&MqttDriver::ConnectionMonitorLoop, this);
        }
        
        status_ = Structs::DriverStatus::RUNNING;
        LogMessage("INFO", "MQTT driver started");
        return true;
        
    } catch (const std::exception& e) {
        SetError("Failed to start MQTT driver: " + std::string(e.what()));
        return false;
    }
}

bool MqttDriver::Stop() {
    // 스레드 정리
    message_processor_running_ = false;
    connection_monitor_running_ = false;
    
    // 스레드 종료 신호
    message_queue_cv_.notify_all();
    connection_cv_.notify_all();
    
    // 스레드 종료 대기
    if (message_processor_thread_.joinable()) {
        message_processor_thread_.join();
    }
    
    if (connection_monitor_thread_.joinable()) {
        connection_monitor_thread_.join();
    }
    
    status_ = Structs::DriverStatus::STOPPED;
    LogMessage("INFO", "MQTT driver stopped");
    return true;
}

void MqttDriver::SetError(const std::string& error_message) {
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_.code = ErrorCode::DRIVER_ERROR;
        last_error_.message = error_message;
        last_error_.occurred_at = std::chrono::system_clock::now();
    }
    
    status_ = Structs::DriverStatus::ERROR;
    LogMessage("ERROR", error_message);
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string MqttDriver::GenerateClientId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    return "pulseone_mqtt_" + std::to_string(time_t) + "_" + std::to_string(rand() % 1000);
}

std::string MqttDriver::GetBrokerUrl() const {
    return broker_url_;
}

std::string MqttDriver::GetClientId() const {
    return client_id_;
}

std::vector<std::string> MqttDriver::GetSubscribedTopics() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    std::vector<std::string> topics;
    for (const auto& [topic, qos] : subscriptions_) {
        topics.push_back(topic);
    }
    return topics;
}

bool MqttDriver::IsSubscribed(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    return subscriptions_.find(topic) != subscriptions_.end();
}

// =============================================================================
// 진단 및 모니터링
// =============================================================================

std::string MqttDriver::GetDiagnosticsJSON() const {
#ifdef HAS_NLOHMANN_JSON
    try {
        json diagnostics;
        
        // 기본 정보
        diagnostics["broker_url"] = broker_url_;
        diagnostics["client_id"] = client_id_;
        diagnostics["is_connected"] = IsConnected();
        diagnostics["status"] = static_cast<int>(status_.load());
        
        // ✅ 표준 통계 정보
        json stats;
        stats["total_reads"] = driver_statistics_.total_reads.load();
        stats["total_writes"] = driver_statistics_.total_writes.load();
        stats["successful_reads"] = driver_statistics_.successful_reads.load();
        stats["successful_writes"] = driver_statistics_.successful_writes.load();
        stats["failed_reads"] = driver_statistics_.failed_reads.load();
        stats["failed_writes"] = driver_statistics_.failed_writes.load();
        stats["avg_response_time_ms"] = driver_statistics_.avg_response_time_ms.load();
        
        // ✅ MQTT 특화 통계 (표준 방식으로 접근)
        stats["published_messages"] = driver_statistics_.GetProtocolCounter("published_messages");
        stats["received_messages"] = driver_statistics_.GetProtocolCounter("received_messages");
        stats["total_bytes_sent"] = driver_statistics_.GetProtocolCounter("total_bytes_sent");
        stats["total_bytes_received"] = driver_statistics_.GetProtocolCounter("total_bytes_received");
        stats["qos0_messages"] = driver_statistics_.GetProtocolCounter("qos0_messages");
        stats["qos1_messages"] = driver_statistics_.GetProtocolCounter("qos1_messages");
        stats["qos2_messages"] = driver_statistics_.GetProtocolCounter("qos2_messages");
        stats["connection_failures"] = driver_statistics_.GetProtocolCounter("connection_failures");
        
        diagnostics["statistics"] = stats;
        
        // 구독 정보
        json subscriptions;
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            for (const auto& [topic, qos] : subscriptions_) {
                json sub;
                sub["topic"] = topic;
                sub["qos"] = qos;
                subscriptions.push_back(sub);
            }
        }
        diagnostics["subscriptions"] = subscriptions;
        
        // 연결 시간 정보
        if (IsConnected()) {
            auto uptime = duration_cast<seconds>(system_clock::now() - connection_start_time_).count();
            diagnostics["connection_uptime_seconds"] = uptime;
        }
        
        return diagnostics.dump(2);
        
    } catch (const std::exception& e) {
        return "{\"error\":\"Failed to generate diagnostics: " + std::string(e.what()) + "\"}";
    }
#else
    return "{\"error\":\"JSON support not available\"}";
#endif
}

void MqttDriver::LogMessage(const std::string& level, const std::string& message, const std::string& category) const {
    if (logger_) {
        // DriverLogger 사용
        if (level == "DEBUG") {
            logger_->Debug(message, DriverLogCategory::COMMUNICATION);
        } else if (level == "INFO") {
            logger_->Info(message, DriverLogCategory::GENERAL);
        } else if (level == "WARN") {
            logger_->Warn(message, DriverLogCategory::GENERAL);
        } else if (level == "ERROR") {
            logger_->Error(message, DriverLogCategory::ERROR_HANDLING);
        }
    }
    
    // 콘솔 출력 (옵션)
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
// 기타 지원 메서드들
// =============================================================================

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
            
            // 메시지 처리 로직 (필요에 따라 확장)
            ProcessReceivedMessage(topic, payload, 0);
            
            lock.lock();
        }
    }
}

void MqttDriver::ProcessReceivedMessage(const std::string& topic, const std::string& payload, int qos) {
    // 기본 메시지 처리 로직
    // 실제 구현에서는 데이터 파싱, 변환, 저장 등을 수행
    LogMessage("DEBUG", "Processing message from topic: " + topic);
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
            LogMessage("INFO", "Attempting automatic reconnection...");
            EstablishConnection();
        }
    }
}

bool MqttDriver::ShouldReconnect() const {
    // 재연결 조건 확인
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
            token->wait();
            LogMessage("INFO", "Restored subscription to topic: " + topic);
        } catch (const mqtt::exception& e) {
            LogMessage("ERROR", "Failed to restore subscription to topic " + topic + ": " + e.what());
            all_success = false;
        }
    }
    
    return all_success;
}

void MqttDriver::ClearSubscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.clear();
    driver_statistics_.IncrementProtocolCounter("subscription_count", 0);
}

void MqttDriver::UpdateConnectionStats(bool connected, const std::string& reason) {
    if (connected) {
        connection_start_time_ = system_clock::now();
        LogMessage("INFO", "Connection established: " + reason);
    } else {
        if (connection_start_time_ != system_clock::time_point{}) {
            auto uptime = duration_cast<seconds>(system_clock::now() - connection_start_time_).count();
        }
        LogMessage("WARN", "Connection lost: " + reason);
    }
}

void MqttDriver::HandleConnectionLoss(const std::string& cause) {
    // 연결 손실 처리 로직
    need_reconnect_ = auto_reconnect_;
    
    if (need_reconnect_) {
        LogMessage("INFO", "Will attempt to reconnect due to: " + cause);
    }
}


// =============================================================================
// ✅ IProtocolDriver 순수 가상 함수들 구현 - IProtocolDriver와 정확히 동일한 시그니처
// =============================================================================

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

} // namespace Drivers
} // namespace PulseOne

