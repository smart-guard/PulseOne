// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 (기존 구조 완전 호환 버전)
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Common/DriverFactory.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

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
{
}

MqttDriver::~MqttDriver() {
    // 리소스 정리는 나중에 구현
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // 로거 초기화 (device_id가 int이므로 string으로 변환)
    std::string device_id_str = std::to_string(config_.device_id);
    logger_ = std::make_unique<DriverLogger>(
        device_id_str,
        ProtocolType::MQTT,
        config_.endpoint
    );
    
    logger_->Info("MQTT driver initialization started", DriverLogCategory::GENERAL);
    
    try {
        // 기본 설정 파싱
        if (!ParseConfig(config)) {
            return false;
        }
        
        status_ = DriverStatus::INITIALIZED;
        
        logger_->Info("MQTT driver initialized successfully", DriverLogCategory::GENERAL);
        
        // 통계 초기화
        ResetStatistics();
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONFIGURATION_ERROR, 
                 "Initialization error: " + std::string(e.what()));
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
    mqtt_config_.ca_cert_path = "";
    
    logger_->Info("MQTT configuration parsed: broker=" + mqtt_config_.broker_url + 
                  ", client=" + mqtt_config_.client_id,
                  DriverLogCategory::GENERAL);
    
    return true;
}

bool MqttDriver::Connect() {
    if (is_connected_) {
        return true;
    }
    
    logger_->Info("Attempting to connect to MQTT broker (stub)", DriverLogCategory::CONNECTION);
    
    // 현재는 스텁으로 구현
    is_connected_ = true;
    status_ = DriverStatus::INITIALIZED; // CONNECTED가 없으므로 INITIALIZED 사용
    last_successful_operation_ = system_clock::now();
    
    logger_->Info("MQTT connection successful (stub)", DriverLogCategory::CONNECTION);
    
    return true;
}

bool MqttDriver::Disconnect() {
    if (!is_connected_) {
        return true;
    }
    
    logger_->Info("Disconnecting from MQTT broker (stub)", DriverLogCategory::CONNECTION);
    
    is_connected_ = false;
    status_ = DriverStatus::UNINITIALIZED; // DISCONNECTED가 없으므로 UNINITIALIZED 사용
    
    logger_->Info("MQTT disconnection successful (stub)", DriverLogCategory::CONNECTION);
    
    return true;
}

bool MqttDriver::IsConnected() const {
    return is_connected_;
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
// MQTT 특화 메소드 구현 (스텁)
// =============================================================================

bool MqttDriver::Subscribe(const std::string& topic, int qos) {
    (void)qos; // 경고 제거
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    logger_->Info("Subscribed to topic: " + topic + " (stub)", DriverLogCategory::COMMUNICATION);
    UpdateStatistics("subscribe", true);
    return true;
}

bool MqttDriver::Unsubscribe(const std::string& topic) {
    if (!IsConnected()) {
        return false;
    }
    
    logger_->Info("Unsubscribed from topic: " + topic + " (stub)", DriverLogCategory::COMMUNICATION);
    UpdateStatistics("unsubscribe", true);
    return true;
}

bool MqttDriver::Publish(const std::string& topic, const std::string& payload,
                        int qos, bool retained) {
    (void)qos; (void)retained; // 경고 제거
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to MQTT broker");
        return false;
    }
    
    total_messages_sent_++;
    total_bytes_sent_ += payload.length();
    
    logger_->Info("Published to topic: " + topic + " (stub)", DriverLogCategory::COMMUNICATION);
    UpdateStatistics("publish", true);
    return true;
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

void MqttDriver::RestoreSubscriptions() {
    // 스텁 구현
    logger_->Info("Subscription restoration skipped (stub)", DriverLogCategory::CONNECTION);
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
    logger_->Info("MQTT background tasks started (stub)", DriverLogCategory::GENERAL);
}

void MqttDriver::StopBackgroundTasks() {
    stop_workers_ = true;
    logger_->Info("MQTT background tasks stopped (stub)", DriverLogCategory::GENERAL);
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
    
    logger_->Info("MQTT diagnostics enabled (stub)", DriverLogCategory::GENERAL);
    return true;
}

void MqttDriver::DisableDiagnostics() {
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    logger_->Info("MQTT diagnostics disabled", DriverLogCategory::GENERAL);
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
// 드라이버 자동 등록
// =============================================================================

// MQTT 드라이버 등록
REGISTER_DRIVER(ProtocolType::MQTT, MqttDriver)