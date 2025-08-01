// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 (타입 에러 완전 수정 버전)
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Common/UnifiedCommonTypes.h"
#include "Drivers/Common/DriverFactory.h"
#include <optional>
#include <sstream>
#include <iomanip>
#include <algorithm>
// #include <nlohmann/json.hpp>  // 일시적으로 주석 처리

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>

namespace PulseOne {
namespace Drivers {

using namespace std::chrono;

// =============================================================================
// 전방 선언 (콜백 클래스용)
// =============================================================================
class MqttCallbackImpl;

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttDriver::MqttDriver()
    : status_(Structs::DriverStatus::UNINITIALIZED)  // ✅ 수정: Structs:: 사용
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
    try {
        StopBackgroundTasks();
        
        if (is_connected_) {
            Disconnect();
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Destructor cleanup error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
    } catch (...) {
        // 모든 예외 무시
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
        Enums::ProtocolType::MQTT,  // ✅ 수정: Enums:: 추가
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
        
        status_ = Structs::DriverStatus::INITIALIZED;  // ✅ 수정: Structs:: 사용
        
        std::string broker_info = "broker=" + mqtt_config_.broker_address + 
                                ":" + std::to_string(mqtt_config_.broker_port) +
                                ", client=" + mqtt_config_.client_id;
        logger_->Info("MQTT configuration parsed: " + broker_info, DriverLogCategory::GENERAL);
        logger_->Info("MQTT driver initialized successfully", DriverLogCategory::GENERAL);
        
        // 통계 초기화
        ResetStatistics();
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = Structs::ErrorInfo(Enums::ErrorCode::INTERNAL_ERROR,  // ✅ 수정: Structs:: 및 Enums:: 추가
                               "MQTT initialization failed: " + std::string(e.what()));
        logger_->Error("MQTT initialization error: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}

bool MqttDriver::ParseConfig(const DriverConfig& config) {
    // 브로커 URL 파싱
    mqtt_config_.broker_url = config.endpoint;

    // URL에서 주소와 포트 추출
    std::string url = config.endpoint;
    if (url.find("mqtt://") == 0) {
        url = url.substr(7); // "mqtt://" 제거
    }
    
    size_t colon_pos = url.find(':');
    if (colon_pos != std::string::npos) {
        mqtt_config_.broker_address = url.substr(0, colon_pos);
        mqtt_config_.broker_port = std::stoi(url.substr(colon_pos + 1));
    } else {
        mqtt_config_.broker_address = url;
        mqtt_config_.broker_port = 1883; // 기본 포트
    }
    
    // 클라이언트 ID 생성
    mqtt_config_.client_id = "pulseone_" + config.device_id;
    
    // 기본 설정값들
    mqtt_config_.keep_alive_interval = 60;
    mqtt_config_.clean_session = true;
    mqtt_config_.auto_reconnect = true;
    mqtt_config_.use_ssl = false;
    mqtt_config_.qos_level = 1;
    
    if (logger_) {
        logger_->Info("MQTT configuration parsed: broker=" + mqtt_config_.broker_address + 
                      ":" + std::to_string(mqtt_config_.broker_port) + 
                      ", client=" + mqtt_config_.client_id,
                      DriverLogCategory::GENERAL);
    }
    
    return true;
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
        mqtt::connect_options conn_opts;
        conn_opts.set_keep_alive_interval(std::chrono::seconds(mqtt_config_.keep_alive_interval));
        conn_opts.set_clean_session(mqtt_config_.clean_session);
        conn_opts.set_automatic_reconnect(mqtt_config_.auto_reconnect);
        
        // 실제 연결 시도
        auto token = mqtt_client_->connect(conn_opts);
        bool success = token->wait_for(std::chrono::milliseconds(5000)); // 5초 타임아웃
        
        connection_in_progress_ = false;
        
        if (success && mqtt_client_->is_connected()) {
            is_connected_ = true;
            status_ = Structs::DriverStatus::INITIALIZED;  // ✅ 수정: Structs:: 사용
            last_successful_operation_ = system_clock::now();
            
            if (logger_) {
                logger_->Info("MQTT connection successful", DriverLogCategory::CONNECTION);
            }
            
            return true;
        } else {
            SetError(Enums::ErrorCode::CONNECTION_FAILED, "MQTT connection failed or timeout");  // ✅ 수정: Enums:: 추가
            if (logger_) {
                logger_->Error("MQTT connection failed", DriverLogCategory::CONNECTION);
            }
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        connection_in_progress_ = false;
        SetError(Enums::ErrorCode::CONNECTION_FAILED, "MQTT connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("MQTT connection error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    } catch (const std::exception& e) {
        connection_in_progress_ = false;
        SetError(Enums::ErrorCode::INTERNAL_ERROR, "Unexpected connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Unexpected connection error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
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
        status_ = Structs::DriverStatus::UNINITIALIZED;  // ✅ 수정: Structs:: 사용
        
        if (logger_) {
            logger_->Info("MQTT disconnection completed", DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const mqtt::exception& e) {
        if (logger_) {
            logger_->Error("MQTT disconnect error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        is_connected_ = false;
        return false;
    }
}

bool MqttDriver::IsConnected() const {
    return is_connected_ && mqtt_client_ && mqtt_client_->is_connected();
}

Enums::ProtocolType MqttDriver::GetProtocolType() const {
    return Enums::ProtocolType::MQTT;
}

Structs::DriverStatus MqttDriver::GetStatus() const {  // ✅ 수정: Structs:: 사용
    return status_;
}

Structs::ErrorInfo MqttDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

// =============================================================================
// 스텁 구현들 (나머지 메소드들)
// =============================================================================

bool MqttDriver::ReadValues(const std::vector<Structs::DataPoint>& points,
                           std::vector<TimestampedValue>& values) {
    values.clear();
    values.reserve(points.size());
    
    // 스텁 구현
    for (const auto& point : points) {
        (void)point; // 경고 제거
        
        TimestampedValue tvalue;
        tvalue.value = Structs::DataValue(42); // 더미 값
        tvalue.quality = Enums::DataQuality::GOOD;
        tvalue.timestamp = system_clock::now();
        values.push_back(tvalue);
    }
    
    return true;
}

bool MqttDriver::WriteValue(const Structs::DataPoint& point, const Structs::DataValue& value) {
    (void)point; (void)value; // 경고 제거
    return true; // 스텁
}

// =============================================================================
// 헬퍼 메소드들 (스텁)
// =============================================================================

void MqttDriver::SetError(Enums::ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = code;
    last_error_.message = message;
    last_error_.occurred_at = system_clock::now();
    
    if (logger_) {
        logger_->Error(message, DriverLogCategory::ERROR_HANDLING);
    }
}

bool MqttDriver::CreateMqttClient() {
    try {
        std::string server_uri = mqtt_config_.broker_url;
        mqtt_client_ = std::make_unique<mqtt::async_client>(server_uri, mqtt_config_.client_id);
        
        if (logger_) {
            logger_->Info("MQTT client created for: " + server_uri, DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, "Failed to create MQTT client: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Failed to create MQTT client: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

bool MqttDriver::SetupCallbacks() {
    // ✅ 콜백 설정을 일단 스킵 (나중에 구현)
    if (logger_) {
        logger_->Info("MQTT callbacks setup (stub)", DriverLogCategory::CONNECTION);
    }
    return true;
}

// 나머지 메소드들은 스텁으로 구현
bool MqttDriver::SetupSslOptions() { return true; }
void MqttDriver::StartBackgroundTasks() {}
void MqttDriver::StopBackgroundTasks() {}
void MqttDriver::ResetStatistics() {}
void MqttDriver::OnConnectionLost(const std::string& cause) { (void)cause; }
void MqttDriver::OnMessageArrived(mqtt::const_message_ptr msg) { (void)msg; }
void MqttDriver::OnDeliveryComplete(mqtt::delivery_token_ptr token) { (void)token; }

// ✅ 헤더에 선언된 virtual 메소드들 구현 추가
void MqttDriver::connected(const std::string& cause) { 
    (void)cause; 
    if (logger_) {
        logger_->Info("MQTT connected: " + cause, DriverLogCategory::CONNECTION);
    }
}

void MqttDriver::connection_lost(const std::string& cause) { 
    (void)cause;
    if (logger_) {
        logger_->Warn("MQTT connection lost: " + cause, DriverLogCategory::CONNECTION);
    }
}

void MqttDriver::message_arrived(mqtt::const_message_ptr msg) { 
    (void)msg;
    if (logger_) {
        logger_->Debug("MQTT message arrived (stub)", DriverLogCategory::COMMUNICATION);
    }
}

void MqttDriver::delivery_complete(mqtt::delivery_token_ptr token) { 
    (void)token;
    if (logger_) {
        logger_->Debug("MQTT delivery complete (stub)", DriverLogCategory::COMMUNICATION);
    }
}

void MqttDriver::on_failure(const mqtt::token& token) { 
    (void)token;
    if (logger_) {
        logger_->Error("MQTT operation failed (stub)", DriverLogCategory::ERROR_HANDLING);
    }
}

void MqttDriver::on_success(const mqtt::token& token) { 
    (void)token;
    if (logger_) {
        logger_->Debug("MQTT operation succeeded (stub)", DriverLogCategory::COMMUNICATION);
    }
}

// IProtocolDriver 상속 메소드들 추가 구현
const DriverStatistics& MqttDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

} // namespace Drivers
} // namespace PulseOne

