// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 (기존 구조 완전 호환 버전)
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Common/DriverFactory.h"
#include <optional>
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
    
    // URL에서 주소와 포트 추출 (간단한 파싱)
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
    mqtt_config_.client_id = "pulseone_" + std::to_string(config.device_id);
    
    // 🔧 수정: 기존 변수명 사용
    mqtt_config_.keep_alive_interval = 60;    // keep_alive_sec가 아니라 keep_alive_interval
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
        
        // 사용자명/비밀번호 설정 (있는 경우)
        if (!mqtt_config_.username.empty()) {
            conn_opts.set_user_name(mqtt_config_.username);
            if (!mqtt_config_.password.empty()) {
                conn_opts.set_password(mqtt_config_.password);
            }
        }
        
        // SSL/TLS 설정 (필요한 경우)
        if (mqtt_config_.use_ssl) {
            mqtt::ssl_options ssl_opts;
            if (!mqtt_config_.ca_cert_path.empty()) {
                ssl_opts.set_trust_store(mqtt_config_.ca_cert_path);
            }
            conn_opts.set_ssl(ssl_opts);
        }
        
        // 실제 연결 시도
        auto token = mqtt_client_->connect(conn_opts);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        connection_in_progress_ = false;
        
        if (success && mqtt_client_->is_connected()) {
            is_connected_ = true;
            status_ = DriverStatus::INITIALIZED;
            last_successful_operation_ = system_clock::now();
            
            if (logger_) {
                logger_->Info("MQTT connection successful", DriverLogCategory::CONNECTION);
            }
            
            return true;
        } else {
            SetError(ErrorCode::CONNECTION_FAILED, "MQTT connection failed or timeout");
            if (logger_) {
                logger_->Error("MQTT connection failed", DriverLogCategory::CONNECTION);
            }
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        connection_in_progress_ = false;
        SetError(ErrorCode::CONNECTION_FAILED, "MQTT connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("MQTT connection error: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    } catch (const std::exception& e) {
        connection_in_progress_ = false;
        SetError(ErrorCode::UNKNOWN_ERROR, "Unexpected connection error: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Unexpected connection error: " + std::string(e.what()), 
                          DriverLogCategory::ERROR_HANDLING);
        }
        return false;
    }
}

bool MqttDriver::EstablishConnection() {
    if (!mqtt_client_) {
        SetError(ErrorCode::UNKNOWN_ERROR, "MQTT client not created");
        return false;
    }
    
    try {
        // 연결 옵션 설정
        mqtt::connect_options conn_opts;
        conn_opts.set_keep_alive_interval(std::chrono::seconds(mqtt_config_.keep_alive_interval));
        conn_opts.set_clean_session(mqtt_config_.clean_session);
        conn_opts.set_automatic_reconnect(mqtt_config_.auto_reconnect);
        
        // 사용자명/비밀번호 설정
        if (!mqtt_config_.username.empty()) {
            conn_opts.set_user_name(mqtt_config_.username);
            if (!mqtt_config_.password.empty()) {
                conn_opts.set_password(mqtt_config_.password);
            }
        }
        
        // SSL/TLS 설정
        if (mqtt_config_.use_ssl) {
            mqtt::ssl_options ssl_opts;
            if (!mqtt_config_.ca_cert_path.empty()) {
                ssl_opts.set_trust_store(mqtt_config_.ca_cert_path);
            }
            conn_opts.set_ssl(ssl_opts);
        }
        
        // 연결 시도
        auto token = mqtt_client_->connect(conn_opts);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        if (success && mqtt_client_->is_connected()) {
            if (logger_) {
                logger_->Info("MQTT connection established successfully", DriverLogCategory::CONNECTION);
            }
            return true;
        } else {
            SetError(ErrorCode::CONNECTION_FAILED, "Connection failed or timeout");
            if (logger_) {
                logger_->Error("Connection failed or timeout", DriverLogCategory::CONNECTION);
            }
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        SetError(ErrorCode::CONNECTION_FAILED, "Connection failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Connection failed: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
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
        status_ = DriverStatus::UNINITIALIZED;
        
        // 구독 정보 정리
        {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            subscriptions_.clear();
        }
        
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
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    if (!mqtt_client_) {
        SetError(ErrorCode::UNKNOWN_ERROR, "MQTT client not initialized");
        return false;
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        // 실제 구독 시도
        auto token = mqtt_client_->subscribe(topic, qos);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (success) {
            // ✅ 구독 정보 저장 - SubscriptionInfo 객체 사용
            {
                std::lock_guard<std::mutex> lock(subscriptions_mutex_);
                subscriptions_[topic] = SubscriptionInfo(qos);  // int 대신 SubscriptionInfo 사용
            }
            
            // 통계 업데이트
            UpdateStatistics("subscribe", true, duration.count());
            
            if (logger_) {
                logger_->Info("Subscribed to topic: " + topic + " (QoS " + std::to_string(qos) + ")", 
                             DriverLogCategory::PROTOCOL_SPECIFIC);
            }
            
            // 패킷 로깅 (진단 모드인 경우)
            if (packet_logging_enabled_) {
                LogMqttPacket("SUBSCRIBE", topic, qos, 0, true, "", duration.count());
            }
            
            return true;
        } else {
            SetError(ErrorCode::CONNECTION_TIMEOUT, "Subscribe timeout for topic: " + topic);
            if (logger_) {
                logger_->Error("Subscribe timeout for topic: " + topic, 
                              DriverLogCategory::PROTOCOL_SPECIFIC);
            }
            
            if (packet_logging_enabled_) {
                LogMqttPacket("SUBSCRIBE", topic, qos, 0, false, "Timeout", duration.count());
            }
            
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        SetError(ErrorCode::PROTOCOL_ERROR, "Subscribe failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Subscribe error for topic " + topic + ": " + std::string(e.what()), 
                          DriverLogCategory::PROTOCOL_SPECIFIC);
        }
        
        if (packet_logging_enabled_) {
            LogMqttPacket("SUBSCRIBE", topic, qos, 0, false, e.what());
        }
        
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
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    if (!mqtt_client_) {
        SetError(ErrorCode::UNKNOWN_ERROR, "MQTT client not initialized");
        return false;
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        // MQTT 메시지 생성
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        msg->set_retained(retained);
        
        // 실제 발행
        auto token = mqtt_client_->publish(msg);
        bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (success) {
            // 통계 업데이트
            total_messages_sent_++;
            total_bytes_sent_ += payload.length();
            UpdateStatistics("publish", true, duration.count());
            last_successful_operation_ = system_clock::now();
            
            if (logger_) {
                logger_->Debug("Published to topic: " + topic + " (" + 
                              std::to_string(payload.length()) + " bytes)", 
                              DriverLogCategory::DATA_PROCESSING);
            }
            
            // 패킷 로깅 (진단 모드인 경우)
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), true, "", duration.count());
            }
            
            return true;
        } else {
            SetError(ErrorCode::CONNECTION_TIMEOUT, "Publish timeout for topic: " + topic);
            if (logger_) {
                logger_->Error("Publish timeout for topic: " + topic, 
                              DriverLogCategory::DATA_PROCESSING);
            }
            
            if (packet_logging_enabled_) {
                LogMqttPacket("PUBLISH", topic, qos, payload.size(), false, "Timeout", duration.count());
            }
            
            return false;
        }
        
    } catch (const mqtt::exception& e) {
        SetError(ErrorCode::PROTOCOL_ERROR, "Publish failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Publish error for topic " + topic + ": " + std::string(e.what()), 
                          DriverLogCategory::DATA_PROCESSING);
        }
        
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
    if (!msg) return;
    
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        
        if (logger_) {
            // 페이로드가 길면 잘라서 로그
            std::string log_payload = payload.length() > 100 ? 
                                     payload.substr(0, 100) + "..." : payload;
            
            logger_->Debug("Processing message from topic: " + topic + 
                          ", payload: " + log_payload, 
                          DriverLogCategory::DATA_PROCESSING);
        }
        
        // 여기서 실제 데이터 처리 로직 구현
        // 예: topic -> data point 매핑, JSON 파싱, 데이터 변환 등
        
        // 현재는 기본적인 처리만 구현
        // TODO: 향후 확장할 기능들:
        // 1. topic -> data point 매핑 조회
        // 2. payload 파싱 (JSON, raw value 등)
        // 3. 데이터 타입 변환 및 검증
        // 4. 스케일링/단위 변환 적용
        // 5. 상위 시스템으로 데이터 전달 (메시지 큐 등)
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to process MQTT message: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
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
        // 🔧 수정: 전체 URL 사용하도록 수정
        std::string server_uri = mqtt_config_.broker_url;
        
        // 🔧 수정: raw pointer 할당 (이미 맞음)
        mqtt_client_ = std::make_unique<mqtt::async_client>(server_uri, mqtt_config_.client_id);
        
        if (logger_) {
            logger_->Info("MQTT client created for: " + server_uri, DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::UNKNOWN_ERROR, "Failed to create MQTT client: " + std::string(e.what()));  // 🔧 수정
        if (logger_) {
            logger_->Error("Failed to create MQTT client: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}



bool MqttDriver::SetupCallbacks() {
    try {
        
        // 🔧 수정: raw pointer 할당 (이미 맞음)
        mqtt_callback_ = std::make_unique<MqttCallbackImpl>(this);
        
        // 클라이언트에 콜백 설정
        if (mqtt_client_) {
            mqtt_client_->set_callback(*mqtt_callback_);
        }
        
        if (logger_) {
            logger_->Info("MQTT callbacks configured", DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::UNKNOWN_ERROR, "Failed to setup callbacks: " + std::string(e.what()));  // 🔧 수정
        if (logger_) {
            logger_->Error("Failed to setup callbacks: " + std::string(e.what()), 
                          DriverLogCategory::CONNECTION);
        }
        return false;
    }
}



void MqttDriver::RestoreSubscriptions() {
    if (!mqtt_client_ || !mqtt_client_->is_connected()) {
        if (logger_) {
            logger_->Warn("Cannot restore subscriptions: client not connected", 
                         DriverLogCategory::CONNECTION);
        }
        return;
    }
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    if (subscriptions_.empty()) {
        if (logger_) {
            logger_->Info("No subscriptions to restore", DriverLogCategory::CONNECTION);
        }
        return;
    }
    
    if (logger_) {
        logger_->Info("Restoring " + std::to_string(subscriptions_.size()) + " subscriptions...", 
                     DriverLogCategory::CONNECTION);
    }
    
    int successful = 0;
    int failed = 0;
    
    for (const auto& subscription : subscriptions_) {
        const std::string& topic = subscription.first;
        const SubscriptionInfo& sub_info = subscription.second;  // ✅ SubscriptionInfo 사용
        int qos = sub_info.qos;  // ✅ QoS 값 추출
        
        try {
            auto token = mqtt_client_->subscribe(topic, qos);
            bool success = token->wait_for(std::chrono::milliseconds(config_.timeout_ms));
            
            if (success) {
                successful++;
                if (logger_) {
                    logger_->Info("Restored subscription: " + topic + " (QoS " + std::to_string(qos) + ")", 
                                 DriverLogCategory::CONNECTION);
                }
            } else {
                failed++;
                if (logger_) {
                    logger_->Error("Failed to restore subscription: " + topic + " (timeout)", 
                                  DriverLogCategory::CONNECTION);
                }
            }
            
        } catch (const mqtt::exception& e) {
            failed++;
            if (logger_) {
                logger_->Error("Error restoring subscription " + topic + ": " + std::string(e.what()),
                              DriverLogCategory::CONNECTION);
            }
        }
    }
    
    if (logger_) {
        logger_->Info("Subscription restoration completed: " + 
                     std::to_string(successful) + " successful, " + 
                     std::to_string(failed) + " failed",
                     DriverLogCategory::CONNECTION);
    }
}


void MqttDriver::UpdateSubscriptionStatus(const std::string& topic, bool subscribed) {
    try {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        if (subscribed) {
            // ✅ 구독 성공 - SubscriptionInfo 객체로 저장
            if (subscriptions_.find(topic) == subscriptions_.end()) {
                subscriptions_[topic] = SubscriptionInfo(mqtt_config_.qos_level);
            }
            
            if (logger_) {
                logger_->Debug("Subscription status updated: " + topic + " -> ACTIVE", 
                              DriverLogCategory::PROTOCOL_SPECIFIC);
            }
        } else {
            // 구독 취소 또는 실패 - 목록에서 제거
            auto it = subscriptions_.find(topic);
            if (it != subscriptions_.end()) {
                subscriptions_.erase(it);
                
                if (logger_) {
                    logger_->Debug("Subscription status updated: " + topic + " -> REMOVED", 
                                  DriverLogCategory::PROTOCOL_SPECIFIC);
                }
            }
        }
        
        // 구독 상태 로그 (디버깅용)
        if (logger_) {
            logger_->Debug("Total active subscriptions: " + std::to_string(subscriptions_.size()),
                          DriverLogCategory::PROTOCOL_SPECIFIC);
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to update subscription status for topic " + topic + 
                          ": " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
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
    if (!diagnostics_enabled_) {
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(mqtt_diagnostics_mutex_);
        
        // 현재 시간
        auto now = system_clock::now();
        
        // 연결 상태 진단
        std::string connection_status = "Unknown";
        if (mqtt_client_) {
            if (mqtt_client_->is_connected()) {
                connection_status = "Connected";
            } else if (connection_in_progress_) {
                connection_status = "Connecting";
            } else if (need_reconnect_) {
                connection_status = "Reconnecting";
            } else {
                connection_status = "Disconnected";
            }
        } else {
            connection_status = "Not Initialized";
        }
        
        // 성능 메트릭 계산
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - last_successful_operation_);
        
        // 구독 상태 확인
        size_t active_subscriptions = 0;
        {
            std::lock_guard<std::mutex> sub_lock(subscriptions_mutex_);
            active_subscriptions = subscriptions_.size();
        }
        
        // 메시지 처리율 계산 (간단한 버전)
        uint64_t total_messages = total_messages_received_ + total_messages_sent_;
        double message_rate = 0.0;
        if (uptime.count() > 0) {
            message_rate = static_cast<double>(total_messages) / uptime.count();
        }
        
        // 진단 정보 로깅 (주기적으로)
        static auto last_diagnostic_log = now;
        if (std::chrono::duration_cast<std::chrono::minutes>(now - last_diagnostic_log).count() >= 5) {
            if (logger_) {
                std::ostringstream oss;
                oss << "MQTT Diagnostics - Status: " << connection_status
                    << ", Subscriptions: " << active_subscriptions
                    << ", Messages RX/TX: " << total_messages_received_ << "/" << total_messages_sent_
                    << ", Bytes RX/TX: " << total_bytes_received_ << "/" << total_bytes_sent_
                    << ", Rate: " << std::fixed << std::setprecision(2) << message_rate << " msg/s";
                
                logger_->Info(oss.str(), DriverLogCategory::GENERAL);
            }
            last_diagnostic_log = now;
        }
        
        // 메모리나 성능 이슈 감지
        if (total_messages > 1000000) { // 100만 메시지 이상 처리했을 때 경고
            if (logger_) {
                logger_->Warn("High message volume processed: " + std::to_string(total_messages) + 
                             " messages", DriverLogCategory::GENERAL);
            }
        }
        
        // 연결 끊김이 지속되는 경우 경고
        if (!is_connected_ && uptime.count() > 300) { // 5분 이상 연결 끊김
            if (logger_) {
                logger_->Warn("Extended disconnection detected: " + std::to_string(uptime.count()) + 
                             " seconds", DriverLogCategory::CONNECTION);
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Diagnostics update error: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
}

// =============================================================================
// 백그라운드 작업 관리 (스텁)
// =============================================================================

void MqttDriver::StartBackgroundTasks() {
    if (stop_workers_) {
        stop_workers_ = false;
        
        // 🔧 수정: 기존 헤더의 멤버 변수명에 맞춤
        // reconnect_thread_ 대신 reconnect_worker_ 사용
        if (!reconnect_worker_.joinable()) {
            reconnect_worker_ = std::thread(&MqttDriver::ReconnectWorkerLoop, this);
        }
        
        if (logger_) {
            logger_->Info("MQTT background tasks started", DriverLogCategory::GENERAL);
        }
    }
}

void MqttDriver::StopBackgroundTasks() {
    if (!stop_workers_) {
        stop_workers_ = true;
        
        // 🔧 수정: 기존 헤더의 멤버 변수명에 맞춤
        if (reconnect_worker_.joinable()) {
            reconnect_worker_.join();
        }
        
        if (logger_) {
            logger_->Info("MQTT background tasks stopped", DriverLogCategory::GENERAL);
        }
    }
}


void MqttDriver::PublishWorkerLoop() {
    // 스텁 구현
}

void MqttDriver::ReconnectWorkerLoop() {
    if (logger_) {
        logger_->Info("MQTT reconnect worker started", DriverLogCategory::CONNECTION);
    }
    
    while (!stop_workers_) {
        try {
            // 재연결이 필요하고, 현재 연결되지 않은 상태이며, 연결 시도 중이 아닌 경우
            if (need_reconnect_ && !is_connected_ && !connection_in_progress_) {
                if (logger_) {
                    logger_->Info("Attempting automatic reconnection...", DriverLogCategory::CONNECTION);
                }
                
                // 재연결 시도
                if (EstablishConnection()) {
                    // 재연결 성공
                    is_connected_ = true;
                    status_ = DriverStatus::INITIALIZED;
                    need_reconnect_ = false;
                    last_successful_operation_ = system_clock::now();
                    
                    if (logger_) {
                        logger_->Info("Automatic reconnection successful", DriverLogCategory::CONNECTION);
                    }
                    
                    // 기존 구독 복원
                    RestoreSubscriptions();
                    
                } else {
                    // 재연결 실패
                    if (logger_) {
                        logger_->Warn("Automatic reconnection failed, will retry in 5 seconds", 
                                     DriverLogCategory::CONNECTION);
                    }
                }
            }
            
            // 5초 대기 (또는 중지 신호까지)
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Reconnect worker error: " + std::string(e.what()),
                              DriverLogCategory::ERROR_HANDLING);
            }
            // 에러 발생 시에도 5초 대기
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    if (logger_) {
        logger_->Info("MQTT reconnect worker stopped", DriverLogCategory::CONNECTION);
    }
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
    try {
        // MQTT 토픽을 데이터 포인트명으로 변환
        // 일반적인 토픽 구조: "device/sensor/temperature" -> "sensor_temperature"
        // 또는 "factory1/line2/motor/speed" -> "line2_motor_speed"
        
        std::string point_name = topic;
        
        // 1. 슬래시를 언더스코어로 변환
        std::replace(point_name.begin(), point_name.end(), '/', '_');
        
        // 2. 하이픈을 언더스코어로 변환
        std::replace(point_name.begin(), point_name.end(), '-', '_');
        
        // 3. 공백을 언더스코어로 변환
        std::replace(point_name.begin(), point_name.end(), ' ', '_');
        
        // 4. 점을 언더스코어로 변환
        std::replace(point_name.begin(), point_name.end(), '.', '_');
        
        // 5. 연속된 언더스코어 제거
        std::string result;
        bool prev_underscore = false;
        for (char c : point_name) {
            if (c == '_') {
                if (!prev_underscore && !result.empty()) {
                    result += c;
                    prev_underscore = true;
                }
            } else {
                result += c;
                prev_underscore = false;
            }
        }
        
        // 6. 앞뒤 언더스코어 제거
        if (!result.empty() && result.front() == '_') {
            result.erase(0, 1);
        }
        if (!result.empty() && result.back() == '_') {
            result.pop_back();
        }
        
        // 7. 디바이스 ID 접두사 제거 (pulseone_12345 같은 패턴)
        std::string device_prefix = "pulseone_" + std::to_string(config_.device_id) + "_";
        if (result.find(device_prefix) == 0) {
            result = result.substr(device_prefix.length());
        }
        
        // 8. 빈 문자열이면 원본 토픽 반환
        if (result.empty()) {
            return topic;
        }
        
        // 9. 너무 길면 잘라내기 (최대 50자)
        if (result.length() > 50) {
            result = result.substr(0, 47) + "...";
        }
        
        // 10. 소문자로 변환 (옵션)
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        
        return result;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Error converting topic to point name: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
        // 에러 발생 시 원본 토픽 반환
        return topic;
    }
}

void MqttDriver::LogMqttPacket(const std::string& direction, const std::string& topic,
                              int qos, size_t payload_size, bool success,
                              const std::string& error, double response_time_ms) {
    if (!packet_logging_enabled_) {
        return;
    }
    
    try {
        std::lock_guard<std::mutex> lock(mqtt_packet_log_mutex_);
        
        // 패킷 로그 생성
        MqttPacketLog log;
        log.timestamp = system_clock::now();
        log.direction = direction;
        log.topic = topic;
        log.qos = qos;
        log.payload_size = payload_size;
        log.success = success;
        log.error_message = error;
        log.response_time_ms = response_time_ms;
        
        // 히스토리에 추가 (최대 1000개 유지)
        mqtt_packet_history_.push_back(log);
        if (mqtt_packet_history_.size() > 1000) {
            mqtt_packet_history_.pop_front();
        }
        
        // 콘솔 출력 (활성화된 경우)
        if (console_output_enabled_) {
            std::cout << FormatMqttPacketForConsole(log) << std::endl;
        }
        
        // 상세 로깅
        if (logger_) {
            std::ostringstream oss;
            oss << "[" << direction << "] " << topic 
                << " (QoS:" << qos << ", Size:" << payload_size << "B";
            
            if (response_time_ms > 0) {
                oss << ", Time:" << std::fixed << std::setprecision(1) << response_time_ms << "ms";
            }
            
            oss << ") - " << (success ? "SUCCESS" : "FAILED");
            
            if (!error.empty()) {
                oss << " (" << error << ")";
            }
            
            logger_->Debug(oss.str(), DriverLogCategory::PROTOCOL_SPECIFIC);
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
        std::ostringstream oss;
        
        // 포인트명 생성
        std::string point_name = GetTopicPointName(topic);
        
        // 값 포맷팅
        std::string formatted_value = payload;
        
        // JSON 데이터인지 확인
        if (payload.front() == '{' && payload.back() == '}') {
            try {
                auto json_data = nlohmann::json::parse(payload);
                
                // JSON에서 value 필드 추출
                if (json_data.contains("value")) {
                    formatted_value = json_data["value"].dump();
                } else if (json_data.contains("data")) {
                    formatted_value = json_data["data"].dump();
                } else {
                    // 전체 JSON을 간단히 표시
                    formatted_value = json_data.dump();
                }
                
                // 단위 정보 추가 (있는 경우)
                if (json_data.contains("unit")) {
                    formatted_value += " " + json_data["unit"].get<std::string>();
                }
                
            } catch (const nlohmann::json::exception&) {
                // JSON 파싱 실패 시 원본 사용
                formatted_value = payload;
            }
        }
        
        // 너무 긴 값은 잘라내기
        if (formatted_value.length() > 100) {
            formatted_value = formatted_value.substr(0, 97) + "...";
        }
        
        // 최종 포맷: "point_name: value"
        oss << point_name << ": " << formatted_value;
        
        return oss.str();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Error formatting MQTT value: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
        return topic + ": " + payload;
    }
}

std::string MqttDriver::FormatMqttPacketForConsole(const MqttPacketLog& log) const {
    try {
        std::ostringstream oss;
        
        // 타임스탬프 포맷팅
        auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
        auto tm = *std::localtime(&time_t);
        
        oss << "[" << std::put_time(&tm, "%H:%M:%S") << "]";
        
        // 방향 표시
        if (log.direction == "PUBLISH") {
            oss << " [TX]";
        } else if (log.direction == "RECEIVE") {
            oss << " [RX]";
        } else if (log.direction == "SUBSCRIBE") {
            oss << " [SUB]";
        } else {
            oss << " [" << log.direction << "]";
        }
        
        // 성공/실패 표시
        oss << (log.success ? " ✅" : " ❌");
        
        // 토픽과 QoS
        oss << " " << log.topic << " (QoS:" << log.qos << ")";
        
        // 페이로드 크기 (PUBLISH/RECEIVE인 경우)
        if (log.payload_size > 0) {
            oss << " [" << log.payload_size << "B]";
        }
        
        // 응답 시간 (있는 경우)
        if (log.response_time_ms > 0) {
            oss << " (" << std::fixed << std::setprecision(1) << log.response_time_ms << "ms)";
        }
        
        // 에러 메시지 (실패한 경우)
        if (!log.success && !log.error_message.empty()) {
            oss << " - " << log.error_message;
        }
        
        return oss.str();
        
    } catch (const std::exception& e) {
        return "[ERROR] Failed to format packet log: " + std::string(e.what());
    }
}


bool MqttDriver::LoadMqttPointsFromDB() {
    try {
        if (logger_) {
            logger_->Info("Loading MQTT data points from database...", DriverLogCategory::GENERAL);
        }
        
        std::lock_guard<std::mutex> lock(mqtt_points_mutex_);
        mqtt_point_info_map_.clear();
        
        int loaded_points = 0;
        int auto_subscribed = 0;
        
        // 새로운 구조체를 사용한 데이터 포인트들
        std::vector<MqttDataPointInfo> default_points = {
            // 온도 센서 - 전체 설정 생성자 사용
            MqttDataPointInfo(
                "temp_sensor_01",                    // point_id
                "Temperature Sensor 1",              // name
                "Room temperature monitoring",       // description
                "sensors/temperature/room1",         // topic
                1,                                   // qos
                DataType::FLOAT32,                   // data_type
                "°C",                               // unit
                1.0,                                // scaling_factor
                0.0,                                // scaling_offset
                false,                              // is_writable
                true                                // auto_subscribe
            ),
            
            // 습도 센서
            MqttDataPointInfo(
                "humid_sensor_01",                   // point_id
                "Humidity Sensor 1",                 // name
                "Room humidity monitoring",          // description
                "sensors/humidity/room1",            // topic
                1,                                   // qos
                DataType::FLOAT32,                   // data_type
                "%RH",                              // unit
                1.0,                                // scaling_factor
                0.0,                                // scaling_offset
                false,                              // is_writable
                true                                // auto_subscribe
            ),
            
            // 제어 밸브
            MqttDataPointInfo(
                "valve_ctrl_01",                     // point_id
                "Control Valve 1",                   // name
                "HVAC control valve",                // description
                "actuators/valve/room1/setpoint",    // topic
                2,                                   // qos
                DataType::FLOAT32,                   // data_type
                "%",                                // unit
                1.0,                                // scaling_factor
                0.0,                                // scaling_offset
                true,                               // is_writable
                false                               // auto_subscribe
            ),
            
            // 상태 포인트
            MqttDataPointInfo(
                "system_status",                     // point_id
                "System Status",                     // name
                "Overall system status",             // description
                "status/system/overall",             // topic
                1,                                   // qos
                DataType::STRING,                    // data_type
                "",                                 // unit
                1.0,                                // scaling_factor
                0.0,                                // scaling_offset
                false,                              // is_writable
                true                                // auto_subscribe
            )
        };
        
        // 포인트들을 맵에 저장하고 자동 구독 처리
        for (const auto& point : default_points) {
            mqtt_point_info_map_[point.topic] = point;
            loaded_points++;
            
            // 자동 구독이 설정된 포인트들은 구독 목록에 추가
            if (point.auto_subscribe && is_connected_) {
                try {
                    if (Subscribe(point.topic, point.qos)) {
                        auto_subscribed++;
                        if (logger_) {
                            logger_->Debug("Auto-subscribed to topic: " + point.topic, 
                                         DriverLogCategory::PROTOCOL_SPECIFIC);
                        }
                    }
                } catch (const std::exception& e) {
                    if (logger_) {
                        logger_->Warn("Failed to auto-subscribe to topic " + point.topic + 
                                     ": " + std::string(e.what()),
                                     DriverLogCategory::PROTOCOL_SPECIFIC);
                    }
                }
            }
        }
        
        // 토픽 매핑 테이블 업데이트
        {
            std::lock_guard<std::mutex> mapping_lock(data_mapping_mutex_);
            topic_to_datapoints_.clear();
            datapoint_to_topic_.clear();
            
            for (const auto& pair : mqtt_point_info_map_) {
                const auto& info = pair.second;
                
                // DataPoint 객체 생성
                DataPoint dp;
                dp.id = info.point_id;
                dp.name = info.name;
                dp.description = info.description;
                dp.address = 0; // MQTT는 주소 개념 없음
                dp.data_type = info.data_type;
                dp.unit = info.unit;
                dp.scaling_factor = info.scaling_factor;
                dp.scaling_offset = info.scaling_offset;
                dp.is_writable = info.is_writable;
                
                // 매핑 테이블 업데이트
                topic_to_datapoints_[info.topic].push_back(dp);
                datapoint_to_topic_[info.point_id] = info.topic;
            }
        }
        
        if (logger_) {
            std::ostringstream oss;
            oss << "MQTT points loaded successfully: " << loaded_points << " points, "
                << auto_subscribed << " auto-subscribed";
            logger_->Info(oss.str(), DriverLogCategory::GENERAL);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to load MQTT points: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
        return false;
    }
}

// =============================================================================
// MQTT 콜백 핸들러들
// =============================================================================

void MqttDriver::OnConnectionLost(const std::string& cause) {
    if (logger_) {
        logger_->Warn("MQTT connection lost: " + cause, DriverLogCategory::CONNECTION);
    }
    
    is_connected_ = false;
    status_ = DriverStatus::ERROR;
    need_reconnect_ = true;
    
    SetError(ErrorCode::CONNECTION_LOST, "Connection lost: " + cause);
}

void MqttDriver::OnMessageArrived(mqtt::const_message_ptr msg) {
    if (!msg) return;
    
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        int qos = msg->get_qos();
        
        // 통계 업데이트
        total_messages_received_++;
        total_bytes_received_ += payload.length();
        last_successful_operation_ = system_clock::now();
        
        if (logger_) {
            logger_->Debug("Message received from topic: " + topic + 
                          " (QoS " + std::to_string(qos) + ", " + 
                          std::to_string(payload.length()) + " bytes)", 
                          DriverLogCategory::DATA_PROCESSING);
        }
        
        // 패킷 로깅 (진단 모드인 경우)
        if (packet_logging_enabled_) {
            LogMqttPacket("RECEIVE", topic, qos, payload.size(), true, "", 0.0);
        }
        
        // 콘솔 출력 (디버깅용)
        if (console_output_enabled_) {
            std::cout << "[MQTT RX] " << topic << " (QoS " << qos << "): " << payload << std::endl;
        }
        
        // 실제 메시지 처리
        ProcessIncomingMessage(msg);
        
        // 통계 업데이트
        UpdateStatistics("receive", true);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Message processing error: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
        UpdateStatistics("receive", false);
    }
}

void MqttDriver::OnDeliveryComplete(mqtt::delivery_token_ptr token) {
    (void)token; // 경고 제거
    
    if (logger_) {
        logger_->Debug("Message delivery complete", DriverLogCategory::COMMUNICATION);
    }
    UpdateStatistics("delivery", true);
}

// 1. SetupSslOptions 함수 구현
bool MqttDriver::SetupSslOptions() {
    // SSL 설정이 필요한 경우 여기서 구현
    // 현재는 스텁
    if (logger_) {
        logger_->Info("SSL options setup (stub)", DriverLogCategory::CONNECTION);
    }
    return true;
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