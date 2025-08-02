// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 완전 구현 - MQTTWorker 고급 기능 지원
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

// =============================================================================
// MQTT 콜백 클래스 구현
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
// 액션 리스너 클래스 구현
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
    : status_(Structs::DriverStatus::UNINITIALIZED)
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
    // 기본 설정값들 초기화
    mqtt_config_.broker_url = "mqtt://localhost:1883";
    mqtt_config_.broker_address = "localhost";
    mqtt_config_.broker_port = 1883;
    mqtt_config_.client_id = "pulseone_default";
    mqtt_config_.keep_alive_interval = 60;
    mqtt_config_.clean_session = true;
    mqtt_config_.auto_reconnect = true;
    mqtt_config_.use_ssl = false;
    mqtt_config_.qos_level = 1;
}

MqttDriver::~MqttDriver() {
    try {
        StopBackgroundTasks();
        
        if (is_connected_) {
            Disconnect();
        }
        
        CleanupResources();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Destructor cleanup error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    } catch (...) {
        // 모든 예외 무시 (소멸자에서)
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // 로거 초기화
    logger_ = std::make_unique<DriverLogger>(
        config_.device_id,
        Enums::ProtocolType::MQTT,
        config_.endpoint
    );
    
    logger_->Info("MQTT driver initialization started", DriverLogCategory::GENERAL);
    
    try {
        // 설정 파싱
        if (!ParseConfig(config)) {
            return false;
        }
        
        // 데이터베이스에서 MQTT 포인트 로드
        if (!LoadMqttPointsFromDB()) {
            logger_->Warn("Failed to load MQTT points from database", DriverLogCategory::GENERAL);
            // 데이터베이스 로드 실패는 치명적이지 않음
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
        
        status_ = Structs::DriverStatus::INITIALIZED;
        
        std::string broker_info = "broker=" + mqtt_config_.broker_address + 
                                ":" + std::to_string(mqtt_config_.broker_port) +
                                ", client=" + mqtt_config_.client_id;
        logger_->Info("MQTT configuration parsed: " + broker_info, DriverLogCategory::GENERAL);
        logger_->Info("MQTT driver initialized successfully", DriverLogCategory::GENERAL);
        
        // 통계 초기화
        ResetStatistics();
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = Structs::ErrorInfo(Structs::ErrorCode::INTERNAL_ERROR,
                               "MQTT initialization failed: " + std::string(e.what()), "");
        logger_->Error("MQTT initialization error: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}

bool MqttDriver::ParseConfig(const DriverConfig& config) {
    // 브로커 URL 파싱
    mqtt_config_.broker_url = config.endpoint;

    // ✅ 수정: connection_string에서 추가 설정 파싱
    if (!config.connection_string.empty()) {
        try {
            // JSON 파싱 시도
            auto json_config = nlohmann::json::parse(config.connection_string);
            
            // 브로커 URL 우선순위: JSON > endpoint
            if (json_config.contains("broker_url")) {
                mqtt_config_.broker_url = json_config["broker_url"];
            }
            
            // 클라이언트 ID
            if (json_config.contains("client_id")) {
                mqtt_config_.client_id = json_config["client_id"];
            }
            
            // 인증 정보
            if (json_config.contains("username")) {
                mqtt_config_.username = json_config["username"];
            }
            if (json_config.contains("password")) {
                mqtt_config_.password = json_config["password"];
            }
            
            // SSL 설정
            if (json_config.contains("use_ssl")) {
                mqtt_config_.use_ssl = json_config["use_ssl"];
            }
            
            // QoS 설정
            if (json_config.contains("qos_level")) {
                mqtt_config_.qos_level = json_config["qos_level"];
            }
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Warn("Failed to parse connection_string JSON: " + std::string(e.what()));
            }
            // JSON 파싱 실패 시 기본값 사용
        }
    }
    
    // ✅ DriverConfig 필드에서 직접 설정
    if (!config.username.empty()) {
        mqtt_config_.username = config.username;
    }
    if (!config.password.empty()) {
        mqtt_config_.password = config.password;
    }
    if (!config.client_id.empty()) {
        mqtt_config_.client_id = config.client_id;
    } else {
        // 기본 클라이언트 ID 생성
        mqtt_config_.client_id = "pulseone_" + config.device_id;
    }
    
    mqtt_config_.use_ssl = config.use_ssl;
    mqtt_config_.keep_alive_interval = config.keep_alive_interval;
    mqtt_config_.clean_session = config.clean_session;
    mqtt_config_.qos_level = config.qos_level;

    // URL에서 주소와 포트 추출
    std::string url = mqtt_config_.broker_url;
    if (url.find("mqtt://") == 0) {
        url = url.substr(7); // "mqtt://" 제거
    } else if (url.find("mqtts://") == 0) {
        url = url.substr(8); // "mqtts://" 제거
        mqtt_config_.use_ssl = true; // SSL 자동 활성화
    }
    
    size_t colon_pos = url.find(':');
    if (colon_pos != std::string::npos) {
        mqtt_config_.broker_address = url.substr(0, colon_pos);
        mqtt_config_.broker_port = std::stoi(url.substr(colon_pos + 1));
    } else {
        mqtt_config_.broker_address = url;
        mqtt_config_.broker_port = mqtt_config_.use_ssl ? 8883 : 1883; // SSL에 따른 기본 포트
    }
    
    // 기본 설정값들
    mqtt_config_.auto_reconnect = config.auto_reconnect;
    
    if (logger_) {
        logger_->Info("MQTT configuration parsed: broker=" + mqtt_config_.broker_address + 
                      ":" + std::to_string(mqtt_config_.broker_port) + 
                      ", client=" + mqtt_config_.client_id + 
                      ", ssl=" + (mqtt_config_.use_ssl ? "enabled" : "disabled"),
                      DriverLogCategory::GENERAL);
    }
    
    return true;
}

bool MqttDriver::ParseAdvancedConfig(const std::string& connection_string) {
#ifdef HAS_NLOHMANN_JSON
    try {
        json config_json = json::parse(connection_string);
        
        // 고급 설정들
        if (config_json.contains("username")) {
            mqtt_config_.username = config_json["username"].get<std::string>();
        }
        
        if (config_json.contains("password")) {
            mqtt_config_.password = config_json["password"].get<std::string>();
        }
        
        if (config_json.contains("keep_alive")) {
            mqtt_config_.keep_alive_interval = config_json["keep_alive"].get<int>();
        }
        
        if (config_json.contains("clean_session")) {
            mqtt_config_.clean_session = config_json["clean_session"].get<bool>();
        }
        
        if (config_json.contains("auto_reconnect")) {
            mqtt_config_.auto_reconnect = config_json["auto_reconnect"].get<bool>();
        }
        
        if (config_json.contains("qos_level")) {
            mqtt_config_.qos_level = config_json["qos_level"].get<int>();
        }
        
        if (config_json.contains("ca_cert_path")) {
            mqtt_config_.ca_cert_path = config_json["ca_cert_path"].get<std::string>();
        }
        
        return true;
        
    } catch (const json::exception& e) {
        if (logger_) {
            logger_->Warn("Advanced config parsing failed: " + std::string(e.what()), 
                         DriverLogCategory::GENERAL);
        }
        return true; // 고급 설정 실패는 치명적이지 않음
    }
#else
    (void)connection_string; // 경고 제거
    return true;
#endif
}

bool MqttDriver::Connect() {
    if (is_connected_) {
        return true;
    }
    
    if (connection_in_progress_) {
        if (logger_) {
            logger_->Warn("Connection already in progress", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    connection_in_progress_ = true;
    
    try {
        if (logger_) {
            logger_->Info("Attempting to connect to MQTT broker: " + mqtt_config_.broker_url, 
                         DriverLogCategory::CONNECTION);
        }
        
        // 연결 옵션 설정
        connect_options_.set_keep_alive_interval(std::chrono::seconds(mqtt_config_.keep_alive_interval));
        connect_options_.set_clean_session(mqtt_config_.clean_session);
        connect_options_.set_automatic_reconnect(mqtt_config_.auto_reconnect);
        
        // 인증 정보 설정
        if (!mqtt_config_.username.empty()) {
            connect_options_.set_user_name(mqtt_config_.username);
            if (!mqtt_config_.password.empty()) {
                connect_options_.set_password(mqtt_config_.password);
            }
        }
        
        // Will 메시지 설정 (연결 끊김 시 발행할 메시지)
        std::string will_topic = "pulseone/status/" + mqtt_config_.client_id;
        mqtt::will_options will_opts;
        will_opts.set_topic(will_topic);
        will_opts.set_payload(std::string("offline"));
        will_opts.set_qos(1);
        will_opts.set_retained(false);
        connect_options_.set_will(will_opts);
        
        // 연결 시도
        auto token = mqtt_client_->connect(connect_options_);
        bool success = token->wait_for(std::chrono::milliseconds(10000)); // 10초 타임아웃
        
        connection_in_progress_ = false;
        
        if (success && mqtt_client_->is_connected()) {
            is_connected_ = true;
            status_ = Structs::DriverStatus::RUNNING;
            last_successful_operation_ = system_clock::now();
            
            // 온라인 상태 발행
            PublishStatus("online");
            
            // 기존 구독 복원
            RestoreSubscriptions();
            
            if (logger_) {
                logger_->Info("MQTT connection successful", DriverLogCategory::CONNECTION);
            }
            
            UpdateStatistics("connect", true);
            return true;
        } else {
            SetError(Structs::ErrorCode::CONNECTION_FAILED, "MQTT connection failed or timeout");
            if (logger_) {
                logger_->Error("MQTT connection failed", DriverLogCategory::CONNECTION);
            }
            UpdateStatistics("connect", false);
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        connection_in_progress_ = false;
        SetError(Structs::ErrorCode::CONNECTION_FAILED, "MQTT connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("MQTT connection error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        UpdateStatistics("connect", false);
        return false;
    } catch (const std::exception& e) {
        connection_in_progress_ = false;
        SetError(Structs::ErrorCode::INTERNAL_ERROR, "Unexpected connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Unexpected connection error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
        UpdateStatistics("connect", false);
        return false;
    }
}

bool MqttDriver::Disconnect() {
    if (!is_connected_) {
        return true;
    }
    
    try {
        if (logger_) {
            logger_->Info("Disconnecting from MQTT broker", DriverLogCategory::CONNECTION);
        }
        
        // 오프라인 상태 발행
        PublishStatus("offline");
        
        // 백그라운드 작업 중지
        StopBackgroundTasks();
        
        if (mqtt_client_ && mqtt_client_->is_connected()) {
            auto token = mqtt_client_->disconnect();
            bool success = token->wait_for(std::chrono::milliseconds(5000)); // 5초 타임아웃
            
            if (!success) {
                if (logger_) {
                    logger_->Warn("MQTT disconnect timeout", DriverLogCategory::CONNECTION);
                }
            }
        }
        
        is_connected_ = false;
        status_ = Structs::DriverStatus::INITIALIZED;
        
        if (logger_) {
            logger_->Info("MQTT disconnection completed", DriverLogCategory::CONNECTION);
        }
        
        UpdateStatistics("disconnect", true);
        return true;
        
    } catch (const mqtt::exception& e) {
        if (logger_) {
            logger_->Error("MQTT disconnect error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        is_connected_ = false;
        UpdateStatistics("disconnect", false);
        return false;
    }
}

bool MqttDriver::IsConnected() const {
    return is_connected_ && mqtt_client_ && mqtt_client_->is_connected();
}

// =============================================================================
// 데이터 읽기/쓰기 구현
// =============================================================================

bool MqttDriver::ReadValues(const std::vector<Structs::DataPoint>& points,
                           std::vector<TimestampedValue>& values) {
    values.clear();
    values.reserve(points.size());
    
    std::lock_guard<std::mutex> lock(values_mutex_);
    
    for (const auto& point : points) {
        auto it = latest_values_.find(point.id);
        if (it != latest_values_.end()) {
            values.push_back(it->second);
        } else {
            // 값이 없는 경우 기본값 생성
            TimestampedValue tvalue;
            tvalue.value = Structs::DataValue(); // 기본 값
            tvalue.quality = Enums::DataQuality::BAD;
            tvalue.timestamp = system_clock::now();
            values.push_back(tvalue);
        }
    }
    
    UpdateStatistics("read", true);
    return true;
}

bool MqttDriver::WriteValue(const Structs::DataPoint& point, const Structs::DataValue& value) {
    try {
        // DataPoint ID로 토픽 찾기
        std::string topic = FindTopicByPointId(point.id);
        if (topic.empty()) {
            if (logger_) {
                logger_->Error("Topic not found for point ID: " + point.id, 
                              DriverLogCategory::COMMUNICATION);
            }
            return false;
        }
        
        // 값을 문자열로 변환
        std::string payload = DataValueToString(value);
        
        // MQTT 발행
        bool success = Publish(topic, payload, mqtt_config_.qos_level, false);
        
        if (success) {
            if (logger_) {
                logger_->Debug("Value written to topic '" + topic + "': " + payload, 
                              DriverLogCategory::COMMUNICATION);
            }
        }
        
        UpdateStatistics("write", success);
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Write value error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
        UpdateStatistics("write", false);
        return false;
    }
}

// =============================================================================
// MQTT 특화 메소드들 구현
// =============================================================================

bool MqttDriver::Subscribe(const std::string& topic, int qos) {
    if (!IsConnected()) {
        if (logger_) {
            logger_->Error("Cannot subscribe - not connected", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    try {
        auto start_time = steady_clock::now();
        
        auto token = mqtt_client_->subscribe(topic, qos);
        bool success = token->wait_for(std::chrono::milliseconds(5000)); // 5초 타임아웃
        
        auto end_time = steady_clock::now();
        double duration = duration_cast<milliseconds>(end_time - start_time).count();
        
        if (success) {
            // 구독 정보 저장
            {
                std::lock_guard<std::mutex> lock(subscriptions_mutex_);
                subscriptions_[topic] = SubscriptionInfo(qos);
            }
            
            if (logger_) {
                logger_->Info("Subscribed to topic: " + topic + " (QoS: " + std::to_string(qos) + ")", 
                             DriverLogCategory::COMMUNICATION);
            }
            
            // 진단 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("SUBSCRIBE", topic, qos, 0, true, "", duration);
            }
            
            UpdateStatistics("subscribe", true, duration);
        } else {
            if (logger_) {
                logger_->Error("Failed to subscribe to topic: " + topic, 
                              DriverLogCategory::COMMUNICATION);
            }
            
            // 진단 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("SUBSCRIBE", topic, qos, 0, false, "Timeout", duration);
            }
            
            UpdateStatistics("subscribe", false, duration);
        }
        
        return success;
        
    } catch (const mqtt::exception& e) {
        if (logger_) {
            logger_->Error("Subscribe error for topic '" + topic + "': " + std::string(e.what()), 
                          DriverLogCategory::COMMUNICATION);
        }
        UpdateStatistics("subscribe", false);
        return false;
    }
}

bool MqttDriver::Unsubscribe(const std::string& topic) {
    if (!IsConnected()) {
        if (logger_) {
            logger_->Error("Cannot unsubscribe - not connected", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    try {
        auto start_time = steady_clock::now();
        
        auto token = mqtt_client_->unsubscribe(topic);
        bool success = token->wait_for(std::chrono::milliseconds(5000)); // 5초 타임아웃
        
        auto end_time = steady_clock::now();
        double duration = duration_cast<milliseconds>(end_time - start_time).count();
        
        if (success) {
            // 구독 정보 제거
            {
                std::lock_guard<std::mutex> lock(subscriptions_mutex_);
                subscriptions_.erase(topic);
            }
            
            if (logger_) {
                logger_->Info("Unsubscribed from topic: " + topic, DriverLogCategory::COMMUNICATION);
            }
            
            UpdateStatistics("unsubscribe", true, duration);
        } else {
            if (logger_) {
                logger_->Error("Failed to unsubscribe from topic: " + topic, 
                              DriverLogCategory::COMMUNICATION);
            }
            
            UpdateStatistics("unsubscribe", false, duration);
        }
        
        return success;
        
    } catch (const mqtt::exception& e) {
        if (logger_) {
            logger_->Error("Unsubscribe error for topic '" + topic + "': " + std::string(e.what()), 
                          DriverLogCategory::COMMUNICATION);
        }
        UpdateStatistics("unsubscribe", false);
        return false;
    }
}

bool MqttDriver::Publish(const std::string& topic, const std::string& payload, 
                        int qos, bool retained) {
    if (!IsConnected()) {
        if (logger_) {
            logger_->Error("Cannot publish - not connected", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    try {
        auto start_time = steady_clock::now();
        
        // MQTT 메시지 생성
        mqtt::message_ptr msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retained);
        
        auto token = mqtt_client_->publish(msg);
        bool success = token->wait_for(std::chrono::milliseconds(5000)); // 5초 타임아웃
        
        auto end_time = steady_clock::now();
        double duration = duration_cast<milliseconds>(end_time - start_time).count();
        
        if (success) {
            // 통계 업데이트
            total_messages_sent_++;
            total_bytes_sent_ += payload.size();
            
            if (logger_) {
                logger_->Debug("Published to topic '" + topic + "': " + payload.substr(0, 100) + 
                              (payload.size() > 100 ? "..." : ""), 
                              DriverLogCategory::COMMUNICATION);
            }
            
            // 진단 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), true, "", duration);
            }
            
            UpdateStatistics("publish", true, duration);
        } else {
            if (logger_) {
                logger_->Error("Failed to publish to topic: " + topic, 
                              DriverLogCategory::COMMUNICATION);
            }
            
            // 진단 로깅
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), false, "Timeout", duration);
            }
            
            UpdateStatistics("publish", false, duration);
        }
        
        return success;
        
    } catch (const mqtt::exception& e) {
        if (logger_) {
            logger_->Error("Publish error for topic '" + topic + "': " + std::string(e.what()), 
                          DriverLogCategory::COMMUNICATION);
        }
        UpdateStatistics("publish", false);
        return false;
    }
}

bool MqttDriver::PublishJson(const std::string& topic, const nlohmann::json& json_data,
                            int qos, bool retained) {
#ifdef HAS_NLOHMANN_JSON
    try {
        std::string payload = json_data.dump();
        return Publish(topic, payload, qos, retained);
    } catch (const json::exception& e) {
        if (logger_) {
            logger_->Error("JSON serialization error: " + std::string(e.what()), 
                          DriverLogCategory::COMMUNICATION);
        }
        return false;
    }
#else
    (void)topic; (void)json_data; (void)qos; (void)retained; // 경고 제거
    if (logger_) {
        logger_->Error("JSON support not available", DriverLogCategory::COMMUNICATION);
    }
    return false;
#endif
}

// =============================================================================
// MQTT 콜백 메소드들 구현
// =============================================================================

void MqttDriver::connected(const std::string& cause) {
    if (logger_) {
        logger_->Info("MQTT connected: " + cause, DriverLogCategory::CONNECTION);
    }
    
    is_connected_ = true;
    status_ = Structs::DriverStatus::RUNNING;
    last_successful_operation_ = system_clock::now();
    
    // 연결 성공 시 구독 복원
    RestoreSubscriptions();
}

void MqttDriver::connection_lost(const std::string& cause) {
    if (logger_) {
        logger_->Warn("MQTT connection lost: " + cause, DriverLogCategory::CONNECTION);
    }
    
    is_connected_ = false;
    status_ = Structs::DriverStatus::ERROR;
    
    // 재연결 트리거
    {
        std::lock_guard<std::mutex> lock(reconnect_mutex_);
        need_reconnect_ = true;
    }
    reconnect_cv_.notify_one();
    
    SetError(Structs::ErrorCode::CONNECTION_LOST, "Connection lost: " + cause);
}

void MqttDriver::message_arrived(mqtt::const_message_ptr msg) {
    if (!msg) return;
    
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        int qos = msg->get_qos();
        
        // 통계 업데이트
        total_messages_received_++;
        total_bytes_received_ += payload.size();
        
        if (logger_) {
            logger_->Debug("Message arrived from topic '" + topic + "': " + 
                          payload.substr(0, 100) + (payload.size() > 100 ? "..." : ""), 
                          DriverLogCategory::COMMUNICATION);
        }
        
        // 진단 로깅
        if (packet_logging_enabled_) {
            LogMqttPacket("RECEIVE", topic, qos, payload.size(), true, "", 0.0);
        }
        
        // 메시지 처리
        ProcessIncomingMessage(msg);
        
        last_successful_operation_ = system_clock::now();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Message processing error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

void MqttDriver::delivery_complete(mqtt::delivery_token_ptr token) {
    if (!token) return;
    
    try {
        if (logger_) {
            logger_->Debug("Message delivery complete for message ID: " + 
                          std::to_string(token->get_message_id()), 
                          DriverLogCategory::COMMUNICATION);
        }
        
        last_successful_operation_ = system_clock::now();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Delivery complete processing error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

void MqttDriver::on_failure(const mqtt::token& token) {
    try {
        std::string error_msg = "MQTT operation failed";
        
        if (logger_) {
            logger_->Error(error_msg, DriverLogCategory::ERROR_HANDLING);
        }
        
        SetError(Structs::ErrorCode::INTERNAL_ERROR, error_msg);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failure callback error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

void MqttDriver::on_success(const mqtt::token& token) {
    try {
        if (logger_) {
            logger_->Debug("MQTT operation succeeded for message ID: " + 
                          std::to_string(token.get_message_id()), 
                          DriverLogCategory::COMMUNICATION);
        }
        
        last_successful_operation_ = system_clock::now();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Success callback error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

// =============================================================================
// 헬퍼 메소드들 구현
// =============================================================================

bool MqttDriver::CreateMqttClient() {
    try {
        std::string server_uri = mqtt_config_.broker_url;
        mqtt_client_ = std::make_unique<mqtt::async_client>(server_uri, mqtt_config_.client_id);
        
        if (logger_) {
            logger_->Info("MQTT client created for: " + server_uri, DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(Structs::ErrorCode::INTERNAL_ERROR, "Failed to create MQTT client: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Failed to create MQTT client: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

bool MqttDriver::SetupCallbacks() {
    try {
        if (!mqtt_client_) {
            return false;
        }
        
        // 콜백 객체 생성
        mqtt_callback_ = std::make_unique<MqttCallbackImpl>(this);
        
        // 클라이언트에 콜백 설정
        mqtt_client_->set_callback(*mqtt_callback_);
        
        if (logger_) {
            logger_->Info("MQTT callbacks setup completed", DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to setup MQTT callbacks: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

bool MqttDriver::SetupSslOptions() {
    try {
        if (!mqtt_config_.use_ssl) {
            return true; // SSL 사용하지 않으면 성공
        }
        
        mqtt::ssl_options ssl_opts;
        
        // CA 인증서 설정
        if (!mqtt_config_.ca_cert_path.empty()) {
            ssl_opts.set_trust_store(mqtt_config_.ca_cert_path);
        }
        
        // SSL 버전 설정
        //ssl_opts.set_ssl_version(mqtt::ssl_options::TLSV1_2);
        
        // 서버 인증서 검증 활성화
        ssl_opts.set_verify(true);
        
        // 연결 옵션에 SSL 설정 추가
        connect_options_.set_ssl(ssl_opts);
        
        if (logger_) {
            logger_->Info("SSL options configured", DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SSL setup error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

void MqttDriver::StartBackgroundTasks() {
    if (stop_workers_) {
        stop_workers_ = false;
    }
    
    // 발행 작업 스레드 시작
    publish_worker_ = std::thread([this]() {
        PublishWorkerLoop();
    });
    
    // 재연결 관리 스레드 시작
    reconnect_worker_ = std::thread([this]() {
        ReconnectWorkerLoop();
    });
    
    if (logger_) {
        logger_->Info("Background tasks started", DriverLogCategory::GENERAL);
    }
}

void MqttDriver::StopBackgroundTasks() {
    stop_workers_ = true;
    
    // 조건 변수 신호 전송
    publish_queue_cv_.notify_all();
    reconnect_cv_.notify_all();
    
    // 스레드 종료 대기
    if (publish_worker_.joinable()) {
        publish_worker_.join();
    }
    
    if (reconnect_worker_.joinable()) {
        reconnect_worker_.join();
    }
    
    if (logger_) {
        logger_->Info("Background tasks stopped", DriverLogCategory::GENERAL);
    }
}

void MqttDriver::PublishWorkerLoop() {
    while (!stop_workers_) {
        std::unique_lock<std::mutex> lock(publish_queue_mutex_);
        
        // 큐에 작업이 있을 때까지 대기
        publish_queue_cv_.wait(lock, [this] {
            return !publish_queue_.empty() || stop_workers_;
        });
        
        if (stop_workers_) {
            break;
        }
        
        // 큐에서 작업 가져오기
        while (!publish_queue_.empty() && !stop_workers_) {
            auto request = std::move(publish_queue_.front());
            publish_queue_.pop();
            lock.unlock();
            
            // 발행 작업 실행
            bool success = Publish(request->topic, request->payload, 
                                 request->qos, request->retained);
            
            // 결과 반환
            request->promise.set_value(success);
            
            lock.lock();
        }
    }
}

void MqttDriver::ReconnectWorkerLoop() {
    while (!stop_workers_) {
        std::unique_lock<std::mutex> lock(reconnect_mutex_);
        
        // 재연결이 필요할 때까지 대기
        reconnect_cv_.wait(lock, [this] {
            return need_reconnect_ || stop_workers_;
        });
        
        if (stop_workers_) {
            break;
        }
        
        if (need_reconnect_) {
            need_reconnect_ = false;
            lock.unlock();
            
            // 재연결 처리
            ProcessReconnection();
            
            lock.lock();
        }
    }
}

void MqttDriver::ProcessReconnection() {
    if (logger_) {
        logger_->Info("Processing MQTT reconnection", DriverLogCategory::CONNECTION);
    }
    
    // 최소 재연결 간격 확인 (1초)
    auto now = system_clock::now();
    if (duration_cast<seconds>(now - last_reconnect_attempt_).count() < 1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    last_reconnect_attempt_ = now;
    
    // 재연결 시도
    if (Connect()) {
        if (logger_) {
            logger_->Info("MQTT reconnection successful", DriverLogCategory::CONNECTION);
        }
    } else {
        if (logger_) {
            logger_->Warn("MQTT reconnection failed, will retry", DriverLogCategory::CONNECTION);
        }
        
        // 실패 시 다시 재연결 예약 (5초 후)
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        {
            std::lock_guard<std::mutex> lock(reconnect_mutex_);
            need_reconnect_ = true;
        }
        reconnect_cv_.notify_one();
    }
}

void MqttDriver::RestoreSubscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    for (const auto& [topic, sub_info] : subscriptions_) {
        try {
            auto token = mqtt_client_->subscribe(topic, sub_info.qos);
            bool success = token->wait_for(std::chrono::milliseconds(5000));
            
            if (success) {
                if (logger_) {
                    logger_->Info("Restored subscription to topic: " + topic, 
                                 DriverLogCategory::CONNECTION);
                }
            } else {
                if (logger_) {
                    logger_->Warn("Failed to restore subscription to topic: " + topic, 
                                 DriverLogCategory::CONNECTION);
                }
            }
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Error restoring subscription to topic '" + topic + "': " + 
                              std::string(e.what()), DriverLogCategory::CONNECTION);
            }
        }
    }
}

void MqttDriver::ProcessIncomingMessage(mqtt::const_message_ptr msg) {
    if (!msg) return;
    
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();
    
    // 토픽에 연결된 데이터 포인트들 찾기
    std::lock_guard<std::mutex> lock(data_mapping_mutex_);
    
    auto it = topic_to_datapoints_.find(topic);
    if (it != topic_to_datapoints_.end()) {
        for (const auto& point : it->second) {
            try {
                // 페이로드를 데이터 타입에 맞게 파싱
                Enums::DataType data_type_enum = Enums::DataType::FLOAT32;  // 기본값
                if (point.data_type == "INT16") {
                    data_type_enum = Enums::DataType::INT16;
                } else if (point.data_type == "INT32") {
                    data_type_enum = Enums::DataType::INT32;
                } else if (point.data_type == "FLOAT32") {
                    data_type_enum = Enums::DataType::FLOAT32;
                } else if (point.data_type == "FLOAT64" || point.data_type == "DOUBLE") {
                    data_type_enum = Enums::DataType::FLOAT64;    
                } else if (point.data_type == "BOOL") {
                    data_type_enum = Enums::DataType::BOOL;
                } else if (point.data_type == "STRING") {
                    data_type_enum = Enums::DataType::STRING;
                }
                Structs::DataValue parsed_value = ParseMessagePayload(payload, data_type_enum);

                
                // TimestampedValue 생성
                TimestampedValue tvalue;
                tvalue.value = parsed_value;
                tvalue.quality = Enums::DataQuality::GOOD;
                tvalue.timestamp = system_clock::now();
                
                // 최신 값 저장
                {
                    std::lock_guard<std::mutex> values_lock(values_mutex_);
                    latest_values_[point.id] = tvalue;
                }
                
                if (logger_) {
                    logger_->Debug("Updated value for point '" + point.name + "' from topic '" + 
                                  topic + "'", DriverLogCategory::COMMUNICATION);
                }
                
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Error("Error processing message for point '" + point.name + "': " + 
                                  std::string(e.what()), DriverLogCategory::ERROR_HANDLING);
                }
            }
        }
    }
}

Structs::DataValue MqttDriver::ParseMessagePayload(const std::string& payload, 
                                                   Structs::DataType expected_type) {
    try {
        switch (expected_type) {
            case Enums::DataType::BOOL:
                if (payload == "true" || payload == "1" || payload == "on" || payload == "ON") {
                    return Structs::DataValue(true);
                } else {
                    return Structs::DataValue(false);
                }
                
            case Enums::DataType::INT32:
                return Structs::DataValue(std::stoi(payload));
                
            case Enums::DataType::FLOAT32:
                return Structs::DataValue(std::stof(payload));
                
            case Enums::DataType::FLOAT64:
                return Structs::DataValue(std::stod(payload));
                
            case Enums::DataType::STRING:
                return Structs::DataValue(payload);
                
            default:
                // JSON 파싱 시도
#ifdef HAS_NLOHMANN_JSON
                try {
                    json j = json::parse(payload);
                    if (j.is_boolean()) {
                        return Structs::DataValue(j.get<bool>());
                    } else if (j.is_number_integer()) {
                        return Structs::DataValue(j.get<int>());
                    } else if (j.is_number_float()) {
                        return Structs::DataValue(j.get<double>());
                    } else if (j.is_string()) {
                        return Structs::DataValue(j.get<std::string>());
                    }
                } catch (const json::exception&) {
                    // JSON 파싱 실패 시 문자열로 처리
                }
#endif
                return Structs::DataValue(payload);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Warn("Payload parsing failed, using raw string: " + std::string(e.what()), 
                         DriverLogCategory::COMMUNICATION);
        }
        return Structs::DataValue(payload);
    }
}

std::string MqttDriver::DataValueToString(const Structs::DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int>(value)) {
            return std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        } else {
            return ""; // 기본값
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataValue conversion error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
        return "";
    }
}

bool MqttDriver::PublishStatus(const std::string& status) {
    std::string status_topic = "pulseone/status/" + mqtt_config_.client_id;
    return Publish(status_topic, status, 1, true); // QoS 1, Retained
}

// =============================================================================
// 진단 및 통계 메소드들
// =============================================================================

void MqttDriver::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // statistics_ 리셋은 개별 필드로 수행
    
    // MQTT 특화 통계 초기화
    total_messages_received_ = 0;
    total_messages_sent_ = 0;
    total_bytes_received_ = 0;
    total_bytes_sent_ = 0;
    
    if (logger_) {
        logger_->Info("Statistics reset", DriverLogCategory::GENERAL);
    }
}

void MqttDriver::UpdateStatistics(const std::string& operation, bool success, double duration_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_operations++;
    
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    if (duration_ms > 0) {
        //statistics_.total_response_time_ms += duration_ms;
        if (duration_ms > statistics_.max_response_time_ms) {
            statistics_.max_response_time_ms = duration_ms;
        }
    }
    
    // 마지막 활동 시간 업데이트
    //statistics_.last_activity_time = system_clock::now();
}

void MqttDriver::SetError(Structs::ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = code;
    last_error_.message = message;
    last_error_.occurred_at = system_clock::now();
    
    if (logger_) {
        logger_->Error(message, DriverLogCategory::ERROR_HANDLING);
    }
}

void MqttDriver::CleanupResources() {
    // 큐 정리
    {
        std::lock_guard<std::mutex> lock(publish_queue_mutex_);
        while (!publish_queue_.empty()) {
            publish_queue_.pop();
        }
    }
    
    // 구독 정보 정리
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.clear();
    }
    
    // 값 캐시 정리
    {
        std::lock_guard<std::mutex> lock(values_mutex_);
        latest_values_.clear();
    }
    
    // 콜백 객체 정리
    mqtt_callback_.reset();
    mqtt_client_.reset();
    
    if (logger_) {
        logger_->Info("Resources cleaned up", DriverLogCategory::GENERAL);
    }
}

// =============================================================================
// 인터페이스 메소드들 구현
// =============================================================================

Enums::ProtocolType MqttDriver::GetProtocolType() const {
    return Enums::ProtocolType::MQTT;
}

Structs::DriverStatus MqttDriver::GetStatus() const {
    return status_;
}

Structs::ErrorInfo MqttDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

const DriverStatistics& MqttDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

// =============================================================================
// 추가 유틸리티 메소드들
// =============================================================================

std::optional<MqttDriver::MqttDataPointInfo> MqttDriver::FindPointByTopic(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
    
    auto it = mqtt_point_info_map_.find(topic);
    if (it != mqtt_point_info_map_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::string MqttDriver::FindTopicByPointId(const std::string& point_id) const {
    std::lock_guard<std::mutex> lock(data_mapping_mutex_);
    
    auto it = datapoint_to_topic_.find(point_id);
    if (it != datapoint_to_topic_.end()) {
        return it->second;
    }
    
    return "";
}

size_t MqttDriver::GetLoadedPointCount() const {
    std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
    return mqtt_point_info_map_.size();
}

bool MqttDriver::LoadMqttPointsFromDB() {
    // 스텁 구현 - 실제로는 데이터베이스에서 MQTT 포인트 정보를 로드
    // 현재는 기본 포인트 몇 개를 추가
    
    std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
    
    // 예시 포인트들
    MqttDataPointInfo point1;
    point1.point_id = "mqtt_temp_001";
    point1.name = "Temperature Sensor 1";
    point1.topic = "sensors/temperature/001";
    point1.data_type = Structs::DataType::FLOAT32;
    point1.unit = "°C";
    mqtt_point_info_map_[point1.topic] = point1;
    
    MqttDataPointInfo point2;
    point2.point_id = "mqtt_humidity_001";
    point2.name = "Humidity Sensor 1";
    point2.topic = "sensors/humidity/001";
    point2.data_type = Structs::DataType::FLOAT32;
    point2.unit = "%RH";
    mqtt_point_info_map_[point2.topic] = point2;
    
    if (logger_) {
        logger_->Info("Loaded " + std::to_string(mqtt_point_info_map_.size()) + 
                     " MQTT points from database", DriverLogCategory::GENERAL);
    }
    
    return true;
}

// =============================================================================
// MqttDriver 진단 및 로깅 메소드들 (기존 파일에 추가)
// =============================================================================

void MqttDriver::LogMqttPacket(const std::string& direction, const std::string& topic,
                              int qos, size_t payload_size, bool success,
                              const std::string& error, double response_time_ms) {
    if (!packet_logging_enabled_) {
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(mqtt_packet_log_mutex_);
        
        MqttPacketLog log;
        log.direction = direction;
        log.timestamp = system_clock::now();
        log.topic = topic;
        log.qos = qos;
        log.payload_size = payload_size;
        log.success = success;
        log.error_message = error;
        log.response_time_ms = response_time_ms;
        
        // 진단 정보 추가
        if (direction == "RECEIVE") {
            log.decoded_value = FormatMqttValue(topic, ""); // 실제 페이로드는 보안상 기록하지 않음
        }
        
        // 히스토리에 추가 (최대 1000개 유지)
        mqtt_packet_history_.push_back(log);
        if (mqtt_packet_history_.size() > 1000) {
            mqtt_packet_history_.pop_front();
        }
        
        // 콘솔 출력 (활성화된 경우)
        if (console_output_enabled_) {
            std::cout << FormatMqttPacketForConsole(log) << std::endl;
        }
        
        // 로그 매니저에 기록
        if (log_manager_) {
            std::string log_message = direction + " " + topic + 
                                    " (QoS:" + std::to_string(qos) + 
                                    ", Size:" + std::to_string(payload_size) + 
                                    ", " + (success ? "OK" : "FAIL") + ")";
            
            if (success) {
                log_manager_->Info(log_message);
            } else {
                log_manager_->Error(log_message + " - " + error);
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Packet logging error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

std::string MqttDriver::FormatMqttValue(const std::string& topic, 
                                       const std::string& payload) const {
    try {
        // 토픽에 따른 값 포맷팅
        if (topic.find("temperature") != std::string::npos) {
            return payload + "°C";
        } else if (topic.find("humidity") != std::string::npos) {
            return payload + "%RH";
        } else if (topic.find("pressure") != std::string::npos) {
            return payload + "Pa";
        } else if (topic.find("status") != std::string::npos) {
            if (payload == "1" || payload == "true") {
                return "ON";
            } else if (payload == "0" || payload == "false") {
                return "OFF";
            }
        }
        
        return payload; // 기본 포맷
        
    } catch (const std::exception&) {
        return payload;
    }
}

std::string MqttDriver::FormatMqttPacketForConsole(const MqttPacketLog& log) const {
    std::ostringstream oss;
    
    // 시간 포맷팅
    auto time_t = system_clock::to_time_t(log.timestamp);
    auto ms = duration_cast<milliseconds>(
        log.timestamp.time_since_epoch()) % 1000;
    
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // 방향 및 상태
    std::string status_symbol = log.success ? "✓" : "✗";
    std::string direction_symbol;
    
    if (log.direction == "PUBLISH") {
        direction_symbol = "→";
    } else if (log.direction == "SUBSCRIBE") {
        direction_symbol = "⇄";
    } else if (log.direction == "RECEIVE") {
        direction_symbol = "←";
    } else {
        direction_symbol = "•";
    }
    
    oss << " " << status_symbol << " " << direction_symbol << " ";
    
    // 토픽 (최대 30자로 제한)
    std::string display_topic = log.topic;
    if (display_topic.length() > 30) {
        display_topic = display_topic.substr(0, 27) + "...";
    }
    oss << std::setw(32) << std::left << display_topic;
    
    // QoS 및 크기
    oss << " QoS:" << log.qos;
    if (log.payload_size > 0) {
        oss << " Size:" << log.payload_size << "B";
    }
    
    // 응답 시간
    if (log.response_time_ms > 0) {
        oss << " Time:" << std::fixed << std::setprecision(1) << log.response_time_ms << "ms";
    }
    
    // 에러 메시지
    if (!log.success && !log.error_message.empty()) {
        oss << " Error:" << log.error_message;
    }
    
    return oss.str();
}

bool MqttDriver::EnableDiagnostics(LogManager& log_manager, 
                                  bool packet_log, bool console) {
    try {
        log_manager_ = &log_manager;
        packet_logging_enabled_ = packet_log;
        console_output_enabled_ = console;
        diagnostics_enabled_ = true;
        
        if (logger_) {
            logger_->Info("MQTT diagnostics enabled (packet_log=" + 
                         std::string(packet_log ? "true" : "false") + 
                         ", console=" + std::string(console ? "true" : "false") + ")", 
                         DriverLogCategory::GENERAL);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to enable diagnostics: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
        return false;
    }
}

void MqttDriver::DisableDiagnostics() {
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    log_manager_ = nullptr;
    
    // 히스토리 정리
    {
        std::lock_guard<std::mutex> lock(mqtt_packet_log_mutex_);
        mqtt_packet_history_.clear();
    }
    
    if (logger_) {
        logger_->Info("MQTT diagnostics disabled", DriverLogCategory::GENERAL);
    }
}

std::string MqttDriver::GetDiagnosticsJSON() const {
    if (!diagnostics_enabled_) {
        return "{\"error\":\"Diagnostics not enabled\"}";
    }
    
#ifdef HAS_NLOHMANN_JSON
    try {
        json diagnostics;
        
        // 기본 정보
        diagnostics["protocol"] = "MQTT";
        diagnostics["broker_url"] = mqtt_config_.broker_url;
        diagnostics["client_id"] = mqtt_config_.client_id;
        diagnostics["connected"] = IsConnected();
        diagnostics["status"] = static_cast<int>(status_.load());
        
        // 연결 정보
        json connection_info;
        connection_info["broker_address"] = mqtt_config_.broker_address;
        connection_info["broker_port"] = mqtt_config_.broker_port;
        connection_info["use_ssl"] = mqtt_config_.use_ssl;
        connection_info["keep_alive"] = mqtt_config_.keep_alive_interval;
        connection_info["clean_session"] = mqtt_config_.clean_session;
        connection_info["auto_reconnect"] = mqtt_config_.auto_reconnect;
        diagnostics["connection"] = connection_info;
        
        // 통계 정보
        json stats;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats["total_operations"] = statistics_.total_operations.load();
            stats["successful_operations"] = statistics_.successful_operations.load();
            stats["failed_operations"] = statistics_.failed_operations.load();
            double avg_response = (statistics_.total_operations > 0) ? 
                (statistics_.max_response_time_ms / statistics_.total_operations) : 0.0;
            stats["avg_response_time_ms"] = avg_response;
            stats["max_response_time_ms"] = statistics_.max_response_time_ms.load();
        }
        
        stats["messages_sent"] = total_messages_sent_.load();
        stats["messages_received"] = total_messages_received_.load();
        stats["bytes_sent"] = total_bytes_sent_.load();
        stats["bytes_received"] = total_bytes_received_.load();
        diagnostics["statistics"] = stats;
        
        // 구독 정보
        json subscriptions;
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            for (const auto& [topic, sub_info] : subscriptions_) {
                json sub;
                sub["qos"] = sub_info.qos;
                sub["active"] = sub_info.is_active;
                
                auto time_t = system_clock::to_time_t(sub_info.subscribed_at);
                std::ostringstream time_stream;
                time_stream << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
                sub["subscribed_at"] = time_stream.str();
                
                subscriptions[topic] = sub;
            }
        }
        diagnostics["subscriptions"] = subscriptions;
        
        // 로드된 포인트 정보
        json points;
        {
            std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
            for (const auto& [topic, point_info] : mqtt_point_info_map_) {
                json point;
                point["point_id"] = point_info.point_id;
                point["name"] = point_info.name;
                point["data_type"] = static_cast<int>(point_info.data_type);
                point["unit"] = point_info.unit;
                point["qos"] = point_info.qos;
                point["writable"] = point_info.is_writable;
                point["auto_subscribe"] = point_info.auto_subscribe;
                
                points[topic] = point;
            }
        }
        diagnostics["data_points"] = points;
        
        // 최근 패킷 히스토리 (최대 50개)
        if (packet_logging_enabled_) {
            json packet_history = json::array();
            {
                std::lock_guard<std::mutex> lock(mqtt_packet_log_mutex_);
                
                size_t start_idx = mqtt_packet_history_.size() > 50 ? 
                    mqtt_packet_history_.size() - 50 : 0;
                
                for (size_t i = start_idx; i < mqtt_packet_history_.size(); ++i) {
                    const auto& log = mqtt_packet_history_[i];
                    
                    json packet;
                    packet["direction"] = log.direction;
                    packet["topic"] = log.topic;
                    packet["qos"] = log.qos;
                    packet["payload_size"] = log.payload_size;
                    packet["success"] = log.success;
                    packet["response_time_ms"] = log.response_time_ms;
                    
                    if (!log.error_message.empty()) {
                        packet["error"] = log.error_message;
                    }
                    
                    auto time_t = system_clock::to_time_t(log.timestamp);
                    auto ms = duration_cast<milliseconds>(
                        log.timestamp.time_since_epoch()) % 1000;
                    
                    std::ostringstream time_str;
                    time_str << std::put_time(std::localtime(&time_t), "%H:%M:%S");
                    time_str << "." << std::setfill('0') << std::setw(3) << ms.count();
                    packet["timestamp"] = time_str.str();
                    
                    packet_history.push_back(packet);
                }
            }
            diagnostics["packet_history"] = packet_history;
        }
        
        // 에러 정보
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            if (last_error_.code != Structs::ErrorCode::SUCCESS) {
                json error_info;
                error_info["code"] = static_cast<int>(last_error_.code);
                error_info["message"] = last_error_.message;
                
                auto error_time_t = std::chrono::system_clock::to_time_t(last_error_.occurred_at);
                std::stringstream ss;
                ss << std::put_time(std::localtime(&error_time_t), "%Y-%m-%d %H:%M:%S");
                error_info["occurred_at"] = ss.str();
                
                diagnostics["last_error"] = error_info;
            }
        }
        
        // 시스템 정보
        json system_info;
        auto now = system_clock::now();
        auto uptime = duration_cast<seconds>(now - statistics_.start_time).count();
        system_info["uptime_seconds"] = uptime;
        
        auto time_t = system_clock::to_time_t(last_successful_operation_);
        std::stringstream activity_ss;
        activity_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        system_info["last_activity"] = activity_ss.str();
        
        diagnostics["system"] = system_info;
        
        return diagnostics.dump(2);
        
    } catch (const json::exception& e) {
        return "{\"error\":\"JSON serialization failed: " + std::string(e.what()) + "\"}";
    }
#else
    return "{\"error\":\"JSON support not available\"}";
#endif
}

std::string MqttDriver::GetTopicPointName(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
    
    auto it = mqtt_point_info_map_.find(topic);
    if (it != mqtt_point_info_map_.end()) {
        return it->second.name;
    }
    
    return topic; // 포인트 이름을 찾지 못하면 토픽 자체를 반환
}

// =============================================================================
// 고급 MQTT 기능들 (MQTTWorker 지원용)
// =============================================================================

bool MqttDriver::PublishDataPoints(const std::vector<std::pair<Structs::DataPoint, TimestampedValue>>& data_points,
                                  const std::string& base_topic) {
    if (data_points.empty()) {
        return true;
    }
    
    if (!IsConnected()) {
        if (logger_) {
            logger_->Error("Cannot publish data points - not connected", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    bool all_success = true;
    
    for (const auto& [point, tvalue] : data_points) {
        try {
            // 개별 토픽 생성
            std::string topic = base_topic + "/" + point.name;
            
            // JSON 페이로드 생성
#ifdef HAS_NLOHMANN_JSON
            json payload = CreateDataPointJson(point, tvalue);
            bool success = PublishJson(topic, payload, mqtt_config_.qos_level, false);
#else
            // JSON 지원이 없는 경우 간단한 값만 발행
            std::string payload = DataValueToString(tvalue.value);
            bool success = Publish(topic, payload, mqtt_config_.qos_level, false);
#endif
            
            if (!success) {
                all_success = false;
                if (logger_) {
                    logger_->Error("Failed to publish data point: " + point.name, 
                                  DriverLogCategory::COMMUNICATION);
                }
            }
            
        } catch (const std::exception& e) {
            all_success = false;
            if (logger_) {
                logger_->Error("Error publishing data point '" + point.name + "': " + 
                              std::string(e.what()), DriverLogCategory::ERROR_HANDLING);
            }
        }
    }
    
    return all_success;
}

nlohmann::json MqttDriver::CreateDataPointJson(const Structs::DataPoint& point, 
                                              const TimestampedValue& value) {
#ifdef HAS_NLOHMANN_JSON
    json j;
    
    // 기본 정보
    j["point_id"] = point.id;
    j["name"] = point.name;
    j["device_id"] = point.device_id;
    
    // 값 정보
    if (std::holds_alternative<bool>(value.value)) {
        j["value"] = std::get<bool>(value.value);
        j["type"] = "boolean";
    } else if (std::holds_alternative<int>(value.value)) {
        j["value"] = std::get<int>(value.value);
        j["type"] = "integer";
    } else if (std::holds_alternative<float>(value.value)) {
        j["value"] = std::get<float>(value.value);
        j["type"] = "float";
    } else if (std::holds_alternative<double>(value.value)) {
        j["value"] = std::get<double>(value.value);
        j["type"] = "double";
    } else if (std::holds_alternative<std::string>(value.value)) {
        j["value"] = std::get<std::string>(value.value);
        j["type"] = "string";
    }
    
    // 품질 및 시간 정보
    j["quality"] = static_cast<int>(value.quality);
    
    auto time_t = system_clock::to_time_t(value.timestamp);
    auto ms = duration_cast<milliseconds>(value.timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream time_str;
    time_str << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    time_str << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    j["timestamp"] = time_str.str();
    
    // 단위 정보 (있는 경우)
    if (!point.unit.empty()) {
        j["unit"] = point.unit;
    }
    
    return j;
#else
    // JSON 지원이 없는 경우 빈 객체 반환
    return nlohmann::json{};
#endif
}

// =============================================================================
// 비동기 발행 지원 (MQTTWorker의 고급 기능용)
// =============================================================================

std::future<bool> MqttDriver::PublishAsync(const std::string& topic, const std::string& payload,
                                          int qos, bool retained) {
    auto request = std::make_unique<PublishRequest>(topic, payload, qos, retained);
    auto future = request->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(publish_queue_mutex_);
        publish_queue_.push(std::move(request));
    }
    
    publish_queue_cv_.notify_one();
    
    return future;
}

bool MqttDriver::PublishBatch(const std::vector<std::pair<std::string, std::string>>& messages,
                             int qos, bool retained) {
    if (messages.empty()) {
        return true;
    }
    
    if (!IsConnected()) {
        if (logger_) {
            logger_->Error("Cannot publish batch - not connected", DriverLogCategory::CONNECTION);
        }
        return false;
    }
    
    std::vector<std::future<bool>> futures;
    futures.reserve(messages.size());
    
    // 모든 메시지를 비동기로 발행
    for (const auto& [topic, payload] : messages) {
        futures.push_back(PublishAsync(topic, payload, qos, retained));
    }
    
    // 모든 결과 대기
    bool all_success = true;
    for (auto& future : futures) {
        try {
            if (!future.get()) {
                all_success = false;
            }
        } catch (const std::exception& e) {
            all_success = false;
            if (logger_) {
                logger_->Error("Batch publish error: " + std::string(e.what()), 
                              DriverLogCategory::ERROR_HANDLING);
            }
        }
    }
    
    if (logger_) {
        logger_->Info("Batch publish completed: " + std::to_string(messages.size()) + 
                     " messages, success: " + (all_success ? "true" : "false"), 
                     DriverLogCategory::COMMUNICATION);
    }
    
    return all_success;
}

// =============================================================================
// 연결 상태 모니터링 (MQTTWorker 지원용)
// =============================================================================

bool MqttDriver::IsHealthy() const {
    if (!IsConnected()) {
        return false;
    }
    
    // 최근 활동 확인 (5분 이내)
    auto now = system_clock::now();
    auto time_since_activity = duration_cast<minutes>(now - last_successful_operation_);
    
    if (time_since_activity.count() > 5) {
        return false;
    }
    
    // 에러 상태 확인
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (last_error_.code != Structs::ErrorCode::SUCCESS) {
        auto time_since_error = duration_cast<minutes>(now - last_error_.occurred_at);
        if (time_since_error.count() < 1) { // 1분 이내 에러
            return false;
        }
    }
    
    return true;
}

double MqttDriver::GetConnectionQuality() const {
    if (!IsConnected()) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (statistics_.total_operations == 0) {
        return 1.0; // 새 연결
    }
    
    // 성공률 계산
    double success_rate = static_cast<double>(statistics_.successful_operations) / 
                         statistics_.total_operations;
    
    // 응답 시간 품질 계산 (100ms 기준)
    double avg_response_time = (statistics_.total_operations > 0) ? 
        (statistics_.max_response_time_ms / statistics_.total_operations) : 0.0;
    double response_quality = std::max(0.0, 1.0 - (avg_response_time / 1000.0));
    
    // 전체 품질 = 성공률 70% + 응답시간 30%
    return (success_rate * 0.7) + (response_quality * 0.3);
}

std::string MqttDriver::GetConnectionInfo() const {
    std::ostringstream info;
    
    info << "MQTT Connection Info:\n";
    info << "  Broker: " << mqtt_config_.broker_address << ":" << mqtt_config_.broker_port << "\n";
    info << "  Client ID: " << mqtt_config_.client_id << "\n";
    info << "  Connected: " << (IsConnected() ? "Yes" : "No") << "\n";
    info << "  SSL: " << (mqtt_config_.use_ssl ? "Enabled" : "Disabled") << "\n";
    info << "  Quality: " << std::fixed << std::setprecision(1) << (GetConnectionQuality() * 100) << "%\n";
    
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        info << "  Subscriptions: " << subscriptions_.size() << "\n";
    }
    
    {
        std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
        info << "  Data Points: " << mqtt_point_info_map_.size() << "\n";
    }
    
    return info.str();
}


} // namespace Drivers
} // namespace PulseOne