// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 (기존 구조 완전 호환 버전)
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Common/DriverFactory.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>

using namespace PulseOne::Drivers;
using namespace std::chrono;

// 시간 타입 변환 헬퍼 함수들 (사용하지 않으므로 주석 처리)
namespace {
    /*
    // steady_clock을 system_clock으로 변환
    Timestamp SteadyToSystemClock(const std::chrono::steady_clock::time_point& steady_time) {
        auto system_now = std::chrono::system_clock::now();
        auto steady_now = std::chrono::steady_clock::now();
        auto duration = steady_time - steady_now;
        return system_now + duration;
    }
    
    // system_clock을 steady_clock으로 변환
    std::chrono::steady_clock::time_point SystemToSteadyClock(const Timestamp& system_time) {
        auto system_now = std::chrono::system_clock::now();
        auto steady_now = std::chrono::steady_clock::now();
        auto duration = system_time - system_now;
        return steady_now + duration;
    }
    */
}

// =============================================================================
// MQTT 콜백 구현 클래스
// =============================================================================

class MqttCallbackImpl : public virtual mqtt::callback {
public:
    MqttCallbackImpl(MqttDriver* driver) : parent_driver_(driver) {}
    
    void connection_lost(const std::string& cause) override {
        if (parent_driver_) {
            parent_driver_->OnConnectionLost(cause);
        }
    }
    
    void message_arrived(mqtt::const_message_ptr msg) override {
        if (parent_driver_) {
            parent_driver_->OnMessageArrived(msg);
        }
    }
    
    void delivery_complete(mqtt::delivery_token_ptr token) override {
        if (parent_driver_) {
            parent_driver_->OnDeliveryComplete(token);
        }
    }
    
private:
    MqttDriver* parent_driver_;
};

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttDriver::MqttDriver()
    : status_(DriverStatus::UNINITIALIZED)
    , is_connected_(false)
    , connection_in_progress_(false)
    , stop_workers_(false)
    , need_reconnect_(false)
    , last_successful_operation_(system_clock::now())
    , total_messages_received_(0)
    , total_messages_sent_(0)
    , total_bytes_received_(0)
    , total_bytes_sent_(0)
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false) 
    , console_output_enabled_(false)
    , log_manager_(nullptr)
    , mqtt_client_(nullptr)
    , mqtt_callback_(nullptr)
{
}

MqttDriver::~MqttDriver() {
    Disconnect();
    StopBackgroundTasks();
    
    if (mqtt_client_) {
        delete mqtt_client_;
        mqtt_client_ = nullptr;
    }
    
    if (mqtt_callback_) {
        delete mqtt_callback_;
        mqtt_callback_ = nullptr;
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // 로거 초기화
    std::string device_id_str = std::to_string(config_.device_id);
    logger_ = std::make_unique<DriverLogger>(
        device_id_str,
        ProtocolType::MQTT,
        config_.endpoint
    );
    
    logger_->Info("MQTT driver initialization started", DriverLogCategory::GENERAL);
    
    try {
        // 설정 파싱
        if (!ParseConfig(config)) {
            return false;
        }
        
        // 실제 MQTT 클라이언트 생성
        if (!CreateMqttClient()) {
            return false;
        }
        
        // 콜백 설정
        if (!SetupCallbacks()) {
            return false;
        }
        
        // SSL 설정 (필요한 경우)
        if (mqtt_config_.use_ssl) {
            if (!SetupSslOptions()) {
                return false;
            }
        }
        
        // 백그라운드 작업 시작
        StartBackgroundTasks();
        
        status_ = DriverStatus::INITIALIZED;
        
        std::string broker_info = "broker=" + mqtt_config_.broker_address + 
                                ":" + std::to_string(mqtt_config_.broker_port) +
                                ", client=" + mqtt_config_.client_id;
        logger_->Info("MQTT configuration parsed: " + broker_info, DriverLogCategory::GENERAL);
        logger_->Info("MQTT driver initialized successfully", DriverLogCategory::GENERAL);
        
        // 통계 초기화
        ResetStatistics();
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::UNKNOWN_ERROR, 
                               "MQTT initialization failed: " + std::string(e.what()));
        logger_->Error("MQTT initialization error: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}


bool MqttDriver::ParseConfig(const DriverConfig& config) {
    // 브로커 URL 파싱
    mqtt_config_.broker_url = config.endpoint;
    
    // 클라이언트 ID 생성 (device_id는 int)
    mqtt_config_.client_id = "pulseone_" + std::to_string(config.device_id);
    
    // 기본값 설정 (필드가 없는 경우)
    mqtt_config_.username = "";
    mqtt_config_.password = "";
    mqtt_config_.use_ssl = false;
    
    logger_->Info("MQTT configuration parsed: broker=" + mqtt_config_.broker_url + 
                  ", client=" + mqtt_config_.client_id,
                  DriverLogCategory::GENERAL);
    
    return true;
}

bool MqttDriver::Connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (status_ != DriverStatus::INITIALIZED && status_ != DriverStatus::STOPPED) {
        last_error_ = ErrorInfo(ErrorCode::INVALID_PARAMETER, "Driver not initialized");
        return false;
    }
    
    if (is_connected_) {
        return true;  // 이미 연결됨
    }
    
    logger_->Info("Attempting to connect to MQTT broker", DriverLogCategory::CONNECTION);
    
    try {
        // 연결 옵션 설정
        mqtt::connect_options conn_opts;
        conn_opts.set_keep_alive_interval(mqtt_config_.keep_alive_interval);
        conn_opts.set_clean_session(mqtt_config_.clean_session);
        conn_opts.set_automatic_reconnect(mqtt_config_.auto_reconnect);
        
        if (!mqtt_config_.username.empty()) {
            conn_opts.set_user_name(mqtt_config_.username);
            if (!mqtt_config_.password.empty()) {
                conn_opts.set_password(mqtt_config_.password);
            }
        }
        
        // 비동기 연결 시도
        connection_in_progress_ = true;
        auto token = mqtt_client_->connect(conn_opts);
        
        // 연결 완료까지 대기 (타임아웃 적용)
        bool connected = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        connection_in_progress_ = false;
        
        if (connected && mqtt_client_->is_connected()) {
            is_connected_ = true;
            status_ = DriverStatus::RUNNING;
            last_successful_operation_ = system_clock::now();
            
            logger_->Info("MQTT connection successful", DriverLogCategory::CONNECTION);
            
            // 구독 복원
            RestoreSubscriptions();
            
            return true;
        } else {
            last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, "MQTT connection timeout or failed");
            logger_->Error("MQTT connection failed or timeout", DriverLogCategory::CONNECTION);
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        connection_in_progress_ = false;
        last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, 
                               "MQTT connection error: " + std::string(e.what()));
        logger_->Error("MQTT connection error: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        return false;
    } catch (const std::exception& e) {
        connection_in_progress_ = false;
        last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, 
                               "Unexpected error: " + std::string(e.what()));
        logger_->Error("Unexpected connection error: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        return false;
    }
}

bool MqttDriver::Disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!is_connected_) {
        return true;  // 이미 연결 해제됨
    }
    
    logger_->Info("Disconnecting from MQTT broker", DriverLogCategory::CONNECTION);
    
    try {
        if (mqtt_client_ && mqtt_client_->is_connected()) {
            auto token = mqtt_client_->disconnect();
            token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        }
        
        is_connected_ = false;
        status_ = DriverStatus::STOPPED;
        
        logger_->Info("MQTT disconnection successful", DriverLogCategory::CONNECTION);
        return true;
        
    } catch (const mqtt::exception& e) {
        logger_->Error("MQTT disconnection error: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        is_connected_ = false;
        status_ = DriverStatus::STOPPED;
        return false;
    } catch (const std::exception& e) {
        logger_->Error("Unexpected disconnection error: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        is_connected_ = false;
        status_ = DriverStatus::STOPPED;
        return false;
    }
}

bool MqttDriver::IsConnected() const {
    return is_connected_ && mqtt_client_ && mqtt_client_->is_connected();
}

ProtocolType MqttDriver::GetProtocolType() const {
    return ProtocolType::MQTT;
}

DriverStatus MqttDriver::GetStatus() const {
    return status_;
}

ErrorInfo MqttDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

// =============================================================================
// 데이터 읽기/쓰기 구현 (스텁)
// =============================================================================

bool MqttDriver::ReadValues(const std::vector<DataPoint>& points,
                           std::vector<TimestampedValue>& values) {
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    values.clear();
    values.reserve(points.size());
    
    bool overall_success = true;
    
    for (const auto& point : points) {
        (void)point; // 경고 제거
        
        // 스텁 데이터 생성
        TimestampedValue tvalue;
        tvalue.value = DataValue(42); // 더미 값
        tvalue.quality = DataQuality::GOOD;
        tvalue.timestamp = system_clock::now();
        values.push_back(tvalue);
    }
    
    UpdateStatistics("read", overall_success);
    
    if (overall_success) {
        last_successful_operation_ = system_clock::now();
    }
    
    return overall_success;
}

bool MqttDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    (void)point; (void)value; // 경고 제거
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    // 스텁 구현
    UpdateStatistics("write", true);
    last_successful_operation_ = system_clock::now();
    
    return true;
}

// =============================================================================
// MQTT 특화 메소드들 (실제 구현)
// =============================================================================

bool MqttDriver::Subscribe(const std::string& topic, int qos) {
    if (!IsConnected()) {
        last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    try {
        auto token = mqtt_client_->subscribe(topic, qos);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        if (success) {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_[topic] = qos;
            
            logger_->Info("Subscribed to topic: " + topic + " (QoS " + std::to_string(qos) + ")", 
                         DriverLogCategory::PROTOCOL_SPECIFIC);
            
            // 통계 업데이트
            statistics_.total_operations++;
            statistics_.successful_operations++;
            
            return true;
        } else {
            last_error_ = ErrorInfo(ErrorCode::CONNECTION_TIMEOUT, "Subscribe timeout for topic: " + topic);
            logger_->Error("Subscribe timeout for topic: " + topic, DriverLogCategory::PROTOCOL_SPECIFIC);
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::PROTOCOL_ERROR, 
                               "Subscribe failed: " + std::string(e.what()));
        logger_->Error("Subscribe error for topic " + topic + ": " + std::string(e.what()), 
                      DriverLogCategory::PROTOCOL_SPECIFIC);
        return false;
    }
}

bool MqttDriver::Unsubscribe(const std::string& topic) {
    if (!IsConnected()) {
        last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    try {
        auto token = mqtt_client_->unsubscribe(topic);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        if (success) {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_.erase(topic);
            
            logger_->Info("Unsubscribed from topic: " + topic, DriverLogCategory::PROTOCOL_SPECIFIC);
            
            // 통계 업데이트
            statistics_.total_operations++;
            statistics_.successful_operations++;
            
            return true;
        } else {
            last_error_ = ErrorInfo(ErrorCode::CONNECTION_TIMEOUT, "Unsubscribe timeout for topic: " + topic);
            logger_->Error("Unsubscribe timeout for topic: " + topic, DriverLogCategory::PROTOCOL_SPECIFIC);
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::PROTOCOL_ERROR, 
                               "Unsubscribe failed: " + std::string(e.what()));
        logger_->Error("Unsubscribe error for topic " + topic + ": " + std::string(e.what()), 
                      DriverLogCategory::PROTOCOL_SPECIFIC);
        return false;
    }
}

bool MqttDriver::Publish(const std::string& topic, const std::string& payload, 
                        int qos, bool retained) {
    if (!IsConnected()) {
        last_error_ = ErrorInfo(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    try {
        mqtt::message_ptr msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retained);
        
        auto token = mqtt_client_->publish(msg);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        if (success) {
            total_messages_sent_++;
            total_bytes_sent_ += payload.size();
            
            logger_->Info("Published to topic: " + topic + " (" + std::to_string(payload.size()) + " bytes)", 
                         DriverLogCategory::DATA_PROCESSING);
            
            // 통계 업데이트
            statistics_.total_operations++;
            statistics_.successful_operations++;
            last_successful_operation_ = system_clock::now();
            
            // 패킷 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), true);
            }
            
            return true;
        } else {
            last_error_ = ErrorInfo(ErrorCode::CONNECTION_TIMEOUT, "Publish timeout for topic: " + topic);
            logger_->Error("Publish timeout for topic: " + topic, DriverLogCategory::DATA_PROCESSING);
            
            // 패킷 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), false, "Timeout");
            }
            
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::PROTOCOL_ERROR, 
                               "Publish failed: " + std::string(e.what()));
        logger_->Error("Publish error for topic " + topic + ": " + std::string(e.what()), 
                      DriverLogCategory::DATA_PROCESSING);
        
        // 패킷 로깅
        if (packet_logging_enabled_) {
            LogMqttPacket("PUBLISH", topic, qos, payload.size(), false, e.what());
        }
        
        return false;
    }
}

// JSON 관련 메소드들은 일단 주석 처리 또는 스텁으로 구현
bool MqttDriver::PublishJson(const std::string& topic, const nlohmann::json& json_data,
                            int qos, bool retained) {
    std::string payload = json_data.dump();
    return Publish(topic, payload, qos, retained);
}

bool MqttDriver::PublishDataPoints(
    const std::vector<std::pair<DataPoint, TimestampedValue>>& data_points,
    const std::string& base_topic) {
    
    if (!IsConnected()) {
        return false;
    }
    
    bool overall_success = true;
    
    for (const auto& pair : data_points) {
        const DataPoint& point = pair.first;
        // const TimestampedValue& value = pair.second; // 사용하지 않으므로 주석 처리
        
        // 토픽 생성 (device_id가 int이므로 string으로 변환)
        std::string topic = base_topic + "/" + std::to_string(config_.device_id) + "/" + point.name;
        
        // 스텁으로 발행
        bool success = Publish(topic, "dummy_value", 1, false);
        if (!success) {
            overall_success = false;
        }
    }
    
    return overall_success;
}

// =============================================================================
// MQTT 콜백 구현 (스텁)
// =============================================================================

void MqttDriver::connected(const std::string& cause) {
    logger_->Info("MQTT connected: " + cause, DriverLogCategory::CONNECTION);
    
    is_connected_ = true;
    status_ = DriverStatus::INITIALIZED;
    last_successful_operation_ = system_clock::now();
}

void MqttDriver::connection_lost(const std::string& cause) {
    logger_->Warn("MQTT connection lost: " + cause, DriverLogCategory::CONNECTION);
    
    is_connected_ = false;
    status_ = DriverStatus::ERROR;
}

void MqttDriver::message_arrived(mqtt::const_message_ptr msg) {
    if (!msg) return; // null 체크
    
    try {
        // 실제 구현에서는 msg->get_payload().length() 등 사용
        // 현재는 스텁이므로 더미 값
        total_messages_received_++;
        total_bytes_received_ += 100; // 더미 크기
        
        logger_->Info("Message received (stub)", 
                     DriverLogCategory::COMMUNICATION);
        
        last_successful_operation_ = system_clock::now();
        
    } catch (const std::exception& e) {
        logger_->Error("Message processing error: " + std::string(e.what()),
                      DriverLogCategory::ERROR_HANDLING);
    }
}

void MqttDriver::delivery_complete(mqtt::delivery_token_ptr tok) {
    (void)tok; // 경고 제거
    logger_->Debug("Message delivery complete", DriverLogCategory::COMMUNICATION);
    UpdateStatistics("delivery", true);
}

// =============================================================================
// MQTT 액션 리스너 구현 (스텁)
// =============================================================================

void MqttDriver::on_failure(const mqtt::token& tok) {
    (void)tok; // 경고 제거
    logger_->Error("MQTT operation failed", DriverLogCategory::ERROR_HANDLING);
    UpdateStatistics("operation", false);
}

void MqttDriver::on_success(const mqtt::token& tok) {
    (void)tok; // 경고 제거
    logger_->Debug("MQTT operation succeeded", DriverLogCategory::COMMUNICATION);
    UpdateStatistics("operation", true);
}

// =============================================================================
// 내부 헬퍼 메소드들 구현
// =============================================================================

void MqttDriver::ProcessIncomingMessage(mqtt::const_message_ptr msg) {
    if (!msg) return; // null 체크
    (void)msg; // 스텁이므로 사용하지 않음 (경고 제거)
    // 나중에 구현
}

DataValue MqttDriver::ParseMessagePayload(const std::string& payload, DataType expected_type) {
    switch (expected_type) {
        case DataType::BOOL: {
            std::string lower_payload = payload;
            std::transform(lower_payload.begin(), lower_payload.end(), lower_payload.begin(), ::tolower);
            return DataValue(lower_payload == "true" || lower_payload == "1" || lower_payload == "on");
        }
        case DataType::INT16:
            return DataValue(static_cast<int16_t>(std::stoi(payload)));
        case DataType::UINT16:
            return DataValue(static_cast<uint16_t>(std::stoul(payload)));
        case DataType::INT32:
            return DataValue(static_cast<int32_t>(std::stoi(payload)));
        case DataType::UINT32:
            return DataValue(static_cast<uint32_t>(std::stoul(payload)));
        case DataType::FLOAT32:  // FLOAT 대신 FLOAT32 사용
            return DataValue(std::stof(payload));
        case DataType::FLOAT64:  // DOUBLE 대신 FLOAT64 사용
            return DataValue(std::stod(payload));
        case DataType::STRING:
            return DataValue(payload);
        default:
            throw std::invalid_argument("Unsupported data type for MQTT message parsing");
    }
}

nlohmann::json MqttDriver::CreateDataPointJson(const DataPoint& point, const TimestampedValue& value) {
    nlohmann::json json_obj;
    
    json_obj["id"] = point.id;
    json_obj["name"] = point.name;
    json_obj["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        value.timestamp.time_since_epoch()).count();
    json_obj["quality"] = static_cast<int>(value.quality);
    
    // 값 설정 (variant 방문자 패턴 사용, monostate 처리 포함)
    std::visit([&json_obj](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            json_obj["value"] = nullptr;  // monostate는 null로 변환
        } else {
            json_obj["value"] = val;
        }
    }, value.value);
    
    // 메타데이터
    if (!point.unit.empty()) {
        json_obj["unit"] = point.unit;
    }
    
    json_obj["device"] = std::to_string(config_.device_id);
    
    return json_obj;
}


bool MqttDriver::CreateMqttClient() {
    try {
        std::string server_uri = mqtt_config_.broker_address + ":" + std::to_string(mqtt_config_.broker_port);
        
        // 기존 클라이언트 정리
        if (mqtt_client_) {
            delete mqtt_client_;
        }
        
        // 새 클라이언트 생성
        mqtt_client_ = new mqtt::async_client(server_uri, mqtt_config_.client_id);
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::UNKNOWN_ERROR, 
                               "Failed to create MQTT client: " + std::string(e.what()));
        logger_->Error("Failed to create MQTT client: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}


bool MqttDriver::SetupCallbacks() {
    try {
        // 기존 콜백 정리
        if (mqtt_callback_) {
            delete mqtt_callback_;
        }
        
        // 새 콜백 생성 및 설정
        mqtt_callback_ = new MqttCallbackImpl(this);
        mqtt_client_->set_callback(*mqtt_callback_);
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::UNKNOWN_ERROR, 
                               "Failed to setup MQTT callbacks: " + std::string(e.what()));
        logger_->Error("Failed to setup MQTT callbacks: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}


void MqttDriver::RestoreSubscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    for (const auto& subscription : subscriptions_) {
        const std::string& topic = subscription.first;
        const SubscriptionInfo& info = subscription.second;
        try {
            mqtt_client_->subscribe(topic, info.qos);
            logger_->Info("Restored subscription to topic: " + topic, DriverLogCategory::PROTOCOL_SPECIFIC);
        } catch (const std::exception& e) {
            logger_->Error("Failed to restore subscription to topic " + topic + ": " + std::string(e.what()), 
                          DriverLogCategory::PROTOCOL_SPECIFIC);
        }
    }
}

void MqttDriver::UpdateSubscriptionStatus(const std::string& topic, bool subscribed) {
    (void)topic; (void)subscribed; // 경고 제거
    // 스텁 구현
}

// =============================================================================
// 통계 및 진단
// =============================================================================

void MqttDriver::SetError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = code;
    last_error_.message = message;
    last_error_.occurred_at = system_clock::now(); // timestamp 대신 occurred_at 사용
    
    if (logger_) {
        logger_->Error(message, DriverLogCategory::ERROR_HANDLING);
    }
}

void MqttDriver::UpdateStatistics(const std::string& operation, bool success, double duration_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 기존 DriverStatistics 구조에 맞춰 수정
    if (operation == "read" || operation == "subscribe" || operation == "receive") {
        statistics_.total_operations++;
        if (success) {
            statistics_.successful_operations++;
            statistics_.last_success_time = system_clock::now();
        } else {
            statistics_.failed_operations++;
            statistics_.last_error_time = system_clock::now();
        }
    } else if (operation == "write" || operation == "publish") {
        statistics_.total_operations++;
        if (success) {
            statistics_.successful_operations++;
        } else {
            statistics_.failed_operations++;
            statistics_.last_error_time = system_clock::now();
        }
    }
    
    // 응답 시간 통계 (제공된 경우)
    if (duration_ms > 0) {
        if (statistics_.total_operations == 1) {
            statistics_.avg_response_time_ms = duration_ms;
            statistics_.max_response_time_ms = duration_ms;
        } else {
            statistics_.avg_response_time_ms = 
                (statistics_.avg_response_time_ms * (statistics_.total_operations - 1) + duration_ms) / statistics_.total_operations;
            
            if (duration_ms > statistics_.max_response_time_ms) {
                statistics_.max_response_time_ms = duration_ms;
            }
        }
    }
    
    // 성공률 계산
    if (statistics_.total_operations > 0) {
        statistics_.success_rate = 
            (static_cast<double>(statistics_.successful_operations) / statistics_.total_operations) * 100.0;
    }
}

const DriverStatistics& MqttDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void MqttDriver::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // DriverStatistics 구조체를 기본값으로 초기화
    statistics_ = DriverStatistics{};
    
    // MQTT 특화 통계 초기화
    total_messages_received_ = 0;
    total_messages_sent_ = 0;
    total_bytes_received_ = 0;
    total_bytes_sent_ = 0;
}

void MqttDriver::UpdateDiagnostics() {
    // GetDiagnostics()에서 실시간으로 계산하므로 별도 저장 불필요
}

// =============================================================================
// 백그라운드 작업 관리 (스텁)
// =============================================================================

void MqttDriver::StartBackgroundTasks() {
    stop_workers_ = false;
    if (logger_) {  // 🔧 이 줄 추가
        logger_->Info("MQTT background tasks started (stub)", DriverLogCategory::GENERAL);
    }
}

void MqttDriver::StopBackgroundTasks() {
    stop_workers_ = true;
    if (logger_) {  // 🔧 이 줄 추가
        logger_->Info("MQTT background tasks stopped (stub)", DriverLogCategory::GENERAL);
    }
}

void MqttDriver::PublishWorkerLoop() {
    // 스텁 구현
}

void MqttDriver::ReconnectWorkerLoop() {
    // 스텁 구현
}

void MqttDriver::ProcessReconnection() {
    // 스텁 구현
}

void MqttDriver::CleanupResources() {
    // 스텁 구현
}

// =============================================================================
// 진단 및 유틸리티 메소드들 (스텁)
// =============================================================================

bool MqttDriver::EnableDiagnostics(LogManager& log_manager, 
                                  bool packet_log, bool console) {
    (void)log_manager; (void)packet_log; (void)console; // 경고 제거
    
    diagnostics_enabled_ = true;
    packet_logging_enabled_ = packet_log;
    console_output_enabled_ = console;
    
    if (logger_) {  
        logger_->Info("MQTT diagnostics enabled (stub)", DriverLogCategory::GENERAL);
    }
    return true;
}

void MqttDriver::DisableDiagnostics() {
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    if (logger_) {  
        logger_->Info("MQTT diagnostics disabled", DriverLogCategory::GENERAL);
    }
}

std::string MqttDriver::GetDiagnosticsJSON() const {
    nlohmann::json diag_json;
    
    // 기본 진단 정보를 직접 생성
    diag_json["basic"]["protocol"] = "MQTT";
    diag_json["basic"]["broker_url"] = mqtt_config_.broker_url;
    diag_json["basic"]["client_id"] = mqtt_config_.client_id;
    diag_json["basic"]["status"] = std::to_string(static_cast<int>(status_.load()));
    diag_json["basic"]["connected"] = is_connected_ ? "true" : "false";
    
    // 메시지 통계
    diag_json["basic"]["messages_received"] = std::to_string(total_messages_received_.load());
    diag_json["basic"]["messages_sent"] = std::to_string(total_messages_sent_.load());
    diag_json["basic"]["bytes_received"] = std::to_string(total_bytes_received_.load());
    diag_json["basic"]["bytes_sent"] = std::to_string(total_bytes_sent_.load());
    
    // 기본 통계 정보
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        diag_json["basic"]["total_operations"] = std::to_string(statistics_.total_operations);
        diag_json["basic"]["success_rate"] = std::to_string(statistics_.success_rate) + "%";
        diag_json["basic"]["avg_response_time"] = std::to_string(statistics_.avg_response_time_ms) + "ms";
        diag_json["basic"]["max_response_time"] = std::to_string(statistics_.max_response_time_ms) + "ms";
    }
    
    return diag_json.dump(2);
}

std::string MqttDriver::GetTopicPointName(const std::string& topic) const {
    return topic; // 스텁
}

void MqttDriver::LogMqttPacket(const std::string& direction, const std::string& topic,
                              int qos, size_t payload_size, bool success,
                              const std::string& error, double response_time_ms) {
    (void)direction; (void)topic; (void)qos; (void)payload_size; 
    (void)success; (void)error; (void)response_time_ms; // 경고 제거
    
    // 스텁 구현
}

std::string MqttDriver::FormatMqttValue(const std::string& topic, 
                                       const std::string& payload) const {
    return topic + ": " + payload; // 스텁
}

std::string MqttDriver::FormatMqttPacketForConsole(const MqttPacketLog& log) const {
    (void)log; // 경고 제거
    return "MQTT packet log (stub)"; // 스텁
}

bool MqttDriver::LoadMqttPointsFromDB() {
    // 스텁 구현
    logger_->Info("MQTT points loading skipped (stub)", DriverLogCategory::GENERAL);
    return true;
}

// =============================================================================
// MQTT 콜백 핸들러들
// =============================================================================

void MqttDriver::OnConnectionLost(const std::string& cause) {
    is_connected_ = false;
    status_ = DriverStatus::STOPPED;
    
    logger_->Warn("MQTT connection lost: " + cause, DriverLogCategory::CONNECTION);
    
    if (mqtt_config_.auto_reconnect && !stop_workers_) {
        need_reconnect_ = true;
        logger_->Info("Auto-reconnect enabled, will attempt to reconnect", DriverLogCategory::CONNECTION);
    }
}

void MqttDriver::OnMessageArrived(mqtt::const_message_ptr msg) {
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        
        total_messages_received_++;
        total_bytes_received_ += payload.size();
        
        logger_->Info("Message received from topic: " + topic + " (" + std::to_string(payload.size()) + " bytes)", 
                     DriverLogCategory::DATA_PROCESSING);
        
        // 패킷 로깅
        if (packet_logging_enabled_) {
            LogMqttPacket("RECEIVE", topic, msg->get_qos(), payload.size(), true);
        }
        
        // 메시지 처리 (상위 시스템으로 전달)
        ProcessReceivedMessage(topic, payload, msg->get_qos());
        
    } catch (const std::exception& e) {
        logger_->Error("Error processing received message: " + std::string(e.what()), DriverLogCategory::DATA_PROCESSING);
    }
}

void MqttDriver::OnDeliveryComplete(mqtt::delivery_token_ptr token) {
    try {
        auto msg = token->get_message();
        if (msg) {
            logger_->Info("Message delivery confirmed for topic: " + msg->get_topic(), DriverLogCategory::DATA_PROCESSING);
        }
    } catch (const std::exception& e) {
        logger_->Error("Error in delivery confirmation: " + std::string(e.what()), DriverLogCategory::DATA_PROCESSING);
    }
}


// 1. SetupSslOptions 함수 구현
bool MqttDriver::SetupSslOptions() {
    try {
        // SSL 설정 구현 (현재는 스텁)
        logger_->Info("SSL configuration skipped (not implemented)", DriverLogCategory::SECURITY);
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = ErrorInfo(ErrorCode::UNKNOWN_ERROR, 
                               "SSL setup failed: " + std::string(e.what()));
        logger_->Error("SSL setup error: " + std::string(e.what()), DriverLogCategory::SECURITY);
        return false;
    }
}

// 2. ProcessReceivedMessage 함수 구현 (스텁)
void MqttDriver::ProcessReceivedMessage(const std::string& topic, 
                                       const std::string& payload, 
                                       int qos) {
    try {
        logger_->Debug("Processing MQTT message from topic: " + topic + 
                      " (QoS " + std::to_string(qos) + ")", 
                      DriverLogCategory::DATA_PROCESSING);
        
        // 현재는 스텁 구현 - 실제로는 데이터 포인트 매핑 및 변환 로직
        logger_->Info("Message payload: " + payload.substr(0, 100) + 
                     (payload.length() > 100 ? "..." : ""), 
                     DriverLogCategory::DATA_PROCESSING);
        
        // 향후 구현할 내용:
        // 1. topic -> data points 매핑
        // 2. payload 파싱 (JSON, raw value 등)
        // 3. 데이터 타입 변환
        // 4. 스케일링 적용
        // 5. 상위 시스템으로 데이터 전달
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to process MQTT message from topic " + topic + 
                      ": " + std::string(e.what()),
                      DriverLogCategory::ERROR_HANDLING);
    }
}


// =============================================================================
// 드라이버 자동 등록
// =============================================================================

// MQTT 드라이버 등록
REGISTER_DRIVER(ProtocolType::MQTT, MqttDriver)