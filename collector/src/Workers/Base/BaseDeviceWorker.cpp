/**
 * @file BaseDeviceWorker.cpp
 * @brief BaseDeviceWorker 클래스 구현 (GitHub 구조 맞춤 + Write 가상함수 지원)
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#include "Workers/Base/BaseDeviceWorker.h"
#include "Pipeline/PipelineManager.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <sstream>
#include <iomanip>
#include <thread>

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// ReconnectionSettings 구현
// =============================================================================

std::string ReconnectionSettings::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"auto_reconnect_enabled\": " << (auto_reconnect_enabled ? "true" : "false") << ",\n";
    ss << "  \"retry_interval_ms\": " << retry_interval_ms << ",\n";
    ss << "  \"max_retries_per_cycle\": " << max_retries_per_cycle << ",\n";
    ss << "  \"wait_time_after_max_retries_ms\": " << wait_time_after_max_retries_ms << ",\n";
    ss << "  \"keep_alive_enabled\": " << (keep_alive_enabled ? "true" : "false") << ",\n";
    ss << "  \"keep_alive_interval_seconds\": " << keep_alive_interval_seconds << ",\n";
    ss << "  \"connection_timeout_seconds\": " << connection_timeout_seconds << "\n";
    ss << "}";
    return ss.str();
}

bool ReconnectionSettings::FromJson(const std::string& json_str) {
    // 간단한 JSON 파싱 (실제로는 JSON 라이브러리 사용 권장)
    try {
        // auto_reconnect_enabled 파싱
        size_t pos = json_str.find("\"auto_reconnect_enabled\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            auto_reconnect_enabled = (value == "true");
        }
        
        // retry_interval_ms 파싱
        pos = json_str.find("\"retry_interval_ms\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            retry_interval_ms = std::stoi(value);
        }
        
        // max_retries_per_cycle 파싱
        pos = json_str.find("\"max_retries_per_cycle\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            max_retries_per_cycle = std::stoi(value);
        }
        
        // wait_time_after_max_retries_ms 파싱
        pos = json_str.find("\"wait_time_after_max_retries_ms\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            wait_time_after_max_retries_ms = std::stoi(value);
        }
        
        // keep_alive_enabled 파싱
        pos = json_str.find("\"keep_alive_enabled\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            keep_alive_enabled = (value == "true");
        }
        
        // keep_alive_interval_seconds 파싱
        pos = json_str.find("\"keep_alive_interval_seconds\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            keep_alive_interval_seconds = std::stoi(value);
        }
        
        // connection_timeout_seconds 파싱
        pos = json_str.find("\"connection_timeout_seconds\":");
        if (pos != std::string::npos) {
            size_t value_start = json_str.find(":", pos) + 1;
            size_t value_end = json_str.find_first_of(",}", value_start);
            std::string value = json_str.substr(value_start, value_end - value_start);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            connection_timeout_seconds = std::stoi(value);
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// BaseDeviceWorker 생성자 및 소멸자
// =============================================================================

BaseDeviceWorker::BaseDeviceWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : device_info_(device_info) {
    
    // Worker ID 생성 (기존 방식 사용)
    std::stringstream ss;
    ss << device_info_.protocol_type << "_" << device_info_.id;
    worker_id_ = ss.str();
    
    LogMessage(LogLevel::INFO, "BaseDeviceWorker 생성됨 (Worker ID: " + worker_id_ + ")");

    // 통계 초기화
    reconnection_stats_.first_connection_time = system_clock::now();
    wait_start_time_ = system_clock::now();
    last_keep_alive_time_ = system_clock::now();
    
    LogMessage(LogLevel::INFO, "BaseDeviceWorker created for device: " + device_info_.name);
    
    // 재연결 관리 스레드 시작
    thread_running_ = true;
    reconnection_thread_ = std::make_unique<std::thread>(&BaseDeviceWorker::ReconnectionThreadMain, this);
}

BaseDeviceWorker::~BaseDeviceWorker() {
    // 재연결 스레드 정리
    thread_running_ = false;
    if (reconnection_thread_ && reconnection_thread_->joinable()) {
        reconnection_thread_->join();
    }
    
    LogMessage(LogLevel::INFO, "BaseDeviceWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// 공통 인터페이스 구현
// =============================================================================

bool BaseDeviceWorker::AddDataPoint(const PulseOne::Structs::DataPoint& point) {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    
    // 중복 확인 (GitHub 구조: point_id -> id)
    for (const auto& existing_point : data_points_) {
        if (existing_point.id == point.id) {
            LogMessage(LogLevel::WARN, "Data point already exists: " + point.name);
            return false;
        }
    }
    
    data_points_.push_back(point);
    LogMessage(LogLevel::INFO, "Data point added: " + point.name);
    return true;
}

std::vector<PulseOne::Structs::DataPoint> BaseDeviceWorker::GetDataPoints() const {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    return data_points_;
}

// =============================================================================
// 재연결 관리 - UpdateReconnectionSettings 구현 추가
// =============================================================================

bool BaseDeviceWorker::UpdateReconnectionSettings(const ReconnectionSettings& settings) {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    reconnection_settings_ = settings;
    LogMessage(LogLevel::INFO, "Reconnection settings updated");
    return true;
}

ReconnectionSettings BaseDeviceWorker::GetReconnectionSettings() const {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    return reconnection_settings_;
}

std::future<bool> BaseDeviceWorker::ForceReconnect() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            LogMessage(LogLevel::INFO, "Force reconnect requested");
            
            // 현재 연결 강제 종료
            if (is_connected_.load()) {
                CloseConnection();
                SetConnectionState(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            // 재연결 상태 리셋
            ResetReconnectionState();
            
            // 즉시 재연결 시도
            bool success = AttemptReconnection();
            
            LogMessage(LogLevel::INFO, "Force reconnect " + std::string(success ? "successful" : "failed"));
            promise->set_value(success);
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception in force reconnect: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::string BaseDeviceWorker::GetReconnectionStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"connection_stats\": {\n";
    ss << "    \"total_connections\": " << reconnection_stats_.total_connections.load() << ",\n";
    ss << "    \"successful_connections\": " << reconnection_stats_.successful_connections.load() << ",\n";
    ss << "    \"failed_connections\": " << reconnection_stats_.failed_connections.load() << ",\n";
    ss << "    \"reconnection_cycles\": " << reconnection_stats_.reconnection_cycles.load() << ",\n";
    ss << "    \"wait_cycles\": " << reconnection_stats_.wait_cycles.load() << ",\n";
    ss << "    \"keep_alive_sent\": " << reconnection_stats_.keep_alive_sent.load() << ",\n";
    ss << "    \"keep_alive_failed\": " << reconnection_stats_.keep_alive_failed.load() << ",\n";
    ss << "    \"avg_connection_duration_seconds\": " << reconnection_stats_.avg_connection_duration_seconds.load() << "\n";
    ss << "  },\n";
    ss << "  \"current_state\": {\n";
    ss << "    \"connected\": " << (is_connected_.load() ? "true" : "false") << ",\n";
    ss << "    \"current_retry_count\": " << current_retry_count_.load() << ",\n";
    ss << "    \"in_wait_cycle\": " << (in_wait_cycle_.load() ? "true" : "false") << ",\n";
    ss << "    \"worker_state\": \"" << WorkerStateToString(current_state_.load()) << "\"\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

void BaseDeviceWorker::ResetReconnectionState() {
    current_retry_count_ = 0;
    in_wait_cycle_ = false;
    wait_start_time_ = system_clock::now();
    
    LogMessage(LogLevel::INFO, "Reconnection state reset");
}

// =============================================================================
// 상태 관리
// =============================================================================

std::string BaseDeviceWorker::GetStatusJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"device_id\": \"" << device_info_.id << "\",\n";
    ss << "  \"device_name\": \"" << device_info_.name << "\",\n";
    ss << "  \"worker_id\": \"" << worker_id_ << "\",\n";
    ss << "  \"protocol_type\": \"" << device_info_.protocol_type << "\",\n";
    ss << "  \"endpoint\": \"" << device_info_.endpoint << "\",\n";
    ss << "  \"state\": \"" << WorkerStateToString(current_state_.load()) << "\",\n";
    ss << "  \"connected\": " << (is_connected_.load() ? "true" : "false") << ",\n";
    ss << "  \"data_points_count\": " << data_points_.size() << ",\n";
    ss << "  \"write_supported\": " << (GetProtocolType() == "MODBUS_TCP" || GetProtocolType() == "MODBUS_RTU" || 
                                      GetProtocolType() == "BACNET" || GetProtocolType() == "MQTT" ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// 보호된 메서드들
// =============================================================================

void BaseDeviceWorker::ChangeState(WorkerState new_state) {
    WorkerState old_state = current_state_.exchange(new_state);
    
    if (old_state != new_state) {
        LogMessage(LogLevel::INFO, 
                  "상태 변경: " + WorkerStateToString(old_state) + " → " + WorkerStateToString(new_state));
        
        // 상태 변경 콜백
        OnStateChanged(old_state, new_state);
    }
}

void BaseDeviceWorker::SetConnectionState(bool connected) {
    bool old_connected = is_connected_.exchange(connected);
    
    if (old_connected != connected) {
        LogMessage(LogLevel::INFO, connected ? "디바이스 연결됨" : "디바이스 연결 해제됨");
        
        // 상태 변경 로직
        if (connected && current_state_.load() == WorkerState::RECONNECTING) {
            ChangeState(WorkerState::RUNNING);
        } else if (!connected && current_state_.load() == WorkerState::RUNNING) {
            ChangeState(WorkerState::DEVICE_OFFLINE);
        }
    }
}

void BaseDeviceWorker::HandleConnectionError(const std::string& error_message) {
    LogMessage(LogLevel::ERROR, "연결 에러: " + error_message);
    
    SetConnectionState(false);
    
    // 재연결 상태로 변경
    if (current_state_.load() == WorkerState::RUNNING) {
        ChangeState(WorkerState::RECONNECTING);
    }
}

// =============================================================================
// 내부 메서드들
// =============================================================================

void BaseDeviceWorker::ReconnectionThreadMain() {
    LogMessage(LogLevel::INFO, "재연결 관리 스레드 시작됨");
    
    while (thread_running_.load()) {
        try {
            WorkerState current_state = current_state_.load();
            
            // 실행 중이고 연결이 끊어진 경우만 재연결 시도
            if ((current_state == WorkerState::RUNNING || current_state == WorkerState::RECONNECTING) &&
                !is_connected_.load()) {
                
                LogMessage(LogLevel::DEBUG_LEVEL, "재연결 시도");
                
                if (AttemptReconnection()) {
                    LogMessage(LogLevel::INFO, "재연결 성공");
                    if (current_state == WorkerState::RECONNECTING) {
                        ChangeState(WorkerState::RUNNING);
                    }
                } else {
                    LogMessage(LogLevel::DEBUG_LEVEL, "재연결 실패, 5초 후 재시도");
                }
            }
            
            // 5초마다 재연결 시도
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "재연결 스레드 예외: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    LogMessage(LogLevel::INFO, "재연결 관리 스레드 종료됨");
}

bool BaseDeviceWorker::AttemptReconnection() {
    try {
        LogMessage(LogLevel::DEBUG_LEVEL, "연결 수립 시도");
        
        if (EstablishConnection()) {
            SetConnectionState(true);
            return true;
        } else {
            return false;
        }
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "재연결 시도 중 예외: " + std::string(e.what()));
        return false;
    }
}

void BaseDeviceWorker::HandleKeepAlive() {
    if (!reconnection_settings_.keep_alive_enabled) {
        return;
    }
    
    auto now = system_clock::now();
    auto time_since_last_keepalive = duration_cast<seconds>(now - last_keep_alive_time_).count();
    
    if (time_since_last_keepalive >= reconnection_settings_.keep_alive_interval_seconds) {
        try {
            // 프로토콜별 Keep-alive 전송 (기본 구현은 연결 상태만 확인)
            bool keepalive_success = SendKeepAlive();
            
            if (keepalive_success) {
                // 추가로 연결 상태 확인
                if (!CheckConnection()) {
                    LogMessage(LogLevel::WARN, "Keep-alive sent but connection check failed");
                    HandleConnectionError("Keep-alive connection check failed");
                    reconnection_stats_.keep_alive_failed.fetch_add(1);
                } else {
                    reconnection_stats_.keep_alive_sent.fetch_add(1);
                    LogMessage(LogLevel::DEBUG_LEVEL, "Keep-alive successful");
                }
            } else {
                LogMessage(LogLevel::WARN, "Keep-alive failed");
                HandleConnectionError("Keep-alive send failed");
                reconnection_stats_.keep_alive_failed.fetch_add(1);
            }
            
            last_keep_alive_time_ = now;
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Keep-alive exception: " + std::string(e.what()));
            HandleConnectionError("Keep-alive exception: " + std::string(e.what()));
            reconnection_stats_.keep_alive_failed.fetch_add(1);
        }
    }
}

void BaseDeviceWorker::InitializeRedisChannels() {
    status_channel_ = "device_status:" + device_info_.id;
    reconnection_channel_ = "device_reconnection:" + device_info_.id;
}

void BaseDeviceWorker::UpdateReconnectionStats(bool connection_successful) {
    if (connection_successful) {
        reconnection_stats_.successful_connections.fetch_add(1);
        reconnection_stats_.last_successful_connection = system_clock::now();
        
        // 평균 연결 지속 시간 계산 (간단한 구현)
        static auto last_connection_start = system_clock::now();
        last_connection_start = system_clock::now();
        
    } else {
        reconnection_stats_.failed_connections.fetch_add(1);
        reconnection_stats_.last_failure_time = system_clock::now();
        
        // 연결 지속 시간 업데이트 (연결이 끊어질 때)
        static auto last_connection_start = system_clock::now();
        auto duration = duration_cast<seconds>(system_clock::now() - last_connection_start).count();
        
        if (duration > 0) {
            double current_avg = reconnection_stats_.avg_connection_duration_seconds.load();
            double new_avg = (current_avg + duration) / 2.0; // 간단한 이동 평균
            reconnection_stats_.avg_connection_duration_seconds.store(new_avg);
        }
    }
}

/**
 * @brief 활성 상태인지 확인
 */
bool BaseDeviceWorker::IsActiveState(WorkerState state) {
    return state == WorkerState::RUNNING ||
           state == WorkerState::SIMULATION ||
           state == WorkerState::CALIBRATION ||
           state == WorkerState::COMMISSIONING ||
           state == WorkerState::MAINTENANCE ||
           state == WorkerState::DIAGNOSTIC_MODE;
}

/**
 * @brief 에러 상태인지 확인
 */
bool BaseDeviceWorker::IsErrorState(WorkerState state) {
    return state == WorkerState::ERROR ||
           state == WorkerState::DEVICE_OFFLINE ||
           state == WorkerState::COMMUNICATION_ERROR ||
           state == WorkerState::DATA_INVALID ||
           state == WorkerState::SENSOR_FAULT ||
           state == WorkerState::EMERGENCY_STOP ||
           state == WorkerState::MAX_RETRIES_EXCEEDED;
}

// =============================================================================
// 파이프라인 전송 메서드
// =============================================================================
bool BaseDeviceWorker::SendDataToPipeline(const std::vector<PulseOne::Structs::TimestampedValue>& values, 
                                         uint32_t priority) {
    if (values.empty()) {
        LogMessage(LogLevel::DEBUG_LEVEL, "전송할 데이터가 없음");
        return false;
    }

    try {
        auto& pipeline_manager = Pipeline::PipelineManager::GetInstance();
        
        if (!pipeline_manager.IsRunning()) {
            LogMessage(LogLevel::WARN, "파이프라인이 실행되지 않음");
            return false;
        }

        // DeviceDataMessage 구조체 생성 후 전송
        PulseOne::Structs::DeviceDataMessage message;
        
        // 기본 정보
        message.type = "device_data";
        message.device_id = device_info_.id;
        message.protocol = device_info_.GetProtocolName();
        message.timestamp = std::chrono::system_clock::now();
        message.points = values;
        message.priority = priority;
        
        // 테넌트 정보
        message.tenant_id = device_info_.tenant_id;
        message.site_id = device_info_.site_id;
        
        // 처리 제어 (프로토콜별 최적화)
        message.trigger_alarms = true;
        message.trigger_virtual_points = false;
        
        if (device_info_.GetProtocolName() == "MODBUS_TCP") {
            message.trigger_virtual_points = true;
            message.high_priority = (priority > 3);
        } else if (device_info_.GetProtocolName() == "MQTT") {
            message.trigger_virtual_points = false;
            message.high_priority = (priority > 7);
        } else if (device_info_.GetProtocolName() == "BACNET") {
            message.trigger_virtual_points = false;
            message.high_priority = (priority > 5);
        } else {
            message.high_priority = (priority > 5);
        }
        
        // 추적 정보
        message.correlation_id = GenerateCorrelationId();
        message.source_worker = GetWorkerIdString();
        message.batch_sequence = ++batch_sequence_counter_;
        
        // 디바이스 상태 정보
        message.device_status = ConvertWorkerStateToDeviceStatus(current_state_.load());
        message.previous_status = ConvertWorkerStateToDeviceStatus(previous_state_);
        message.status_changed = (message.device_status != message.previous_status);
        
        // 상태 관리 정보
        message.manual_status = false;
        message.status_message = GetStatusMessage();
        message.status_changed_time = state_change_time_;
        message.status_changed_by = "system";
        
        // 통신 상태 정보
        message.is_connected = IsConnected();
        message.consecutive_failures = consecutive_failures_;
        message.total_failures = total_failures_;
        message.total_attempts = total_attempts_;
        
        message.response_time = last_response_time_;
        message.last_success_time = last_success_time_;
        message.last_attempt_time = message.timestamp;
        
        message.last_error_message = last_error_message_;
        message.last_error_code = last_error_code_;
        
        // 포인트 상태 정보
        message.total_points_configured = GetDataPoints().size();
        message.successful_points = 0;
        message.failed_points = 0;
        
        for (const auto& point : values) {
            if (point.quality == PulseOne::Structs::DataQuality::GOOD) {
                message.successful_points++;
            } else {
                message.failed_points++;
            }
        }
        
        // 디바이스 상태 자동 판단을 위한 임계값 설정
        PulseOne::Structs::StatusThresholds thresholds;
        
        if (device_info_.GetProtocolName() == "MODBUS_TCP" || "MODBUS_RTU") {
            thresholds.offline_failure_count = 3;
            thresholds.timeout_threshold = std::chrono::seconds(3);
            thresholds.partial_failure_ratio = 0.2;
            thresholds.error_failure_ratio = 0.5;
            thresholds.offline_timeout = std::chrono::seconds(10);
        } else if (device_info_.GetProtocolName() == "MQTT") {
            thresholds.offline_failure_count = 10;
            thresholds.timeout_threshold = std::chrono::seconds(10);
            thresholds.partial_failure_ratio = 0.5;
            thresholds.error_failure_ratio = 0.8;
            thresholds.offline_timeout = std::chrono::seconds(60);
        } else if (device_info_.GetProtocolName() == "BACNET") {
            thresholds.offline_failure_count = 5;
            thresholds.timeout_threshold = std::chrono::seconds(5);
            thresholds.partial_failure_ratio = 0.3;
            thresholds.error_failure_ratio = 0.7;
            thresholds.offline_timeout = std::chrono::seconds(30);
        } else {
            // 기본값
            thresholds.offline_failure_count = 3;
            thresholds.timeout_threshold = std::chrono::seconds(5);
            thresholds.partial_failure_ratio = 0.3;
            thresholds.error_failure_ratio = 0.7;
            thresholds.offline_timeout = std::chrono::seconds(30);
        }
        
        message.UpdateDeviceStatus(thresholds);
        
        // PipelineManager로 전송
        bool success = pipeline_manager.SendDeviceData(message);

        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "파이프라인 전송 성공: " + std::to_string(values.size()) + "개 포인트 " +
                      "(상태: " + PulseOne::Enums::DeviceStatusToString(message.device_status) + ", " +
                      "알람=" + (message.trigger_alarms ? "ON" : "OFF") + ", " +
                      "가상포인트=" + (message.trigger_virtual_points ? "ON" : "OFF") + ")");
            
            last_success_time_ = message.timestamp;
            consecutive_failures_ = 0;
        } else {
            LogMessage(LogLevel::WARN, "파이프라인 전송 실패 (큐 오버플로우?)");
            consecutive_failures_++;
            total_failures_++;
            last_error_message_ = "Pipeline queue overflow";
            last_error_code_ = -1;
        }
        
        total_attempts_++;
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "파이프라인 전송 중 예외: " + std::string(e.what()));
        
        consecutive_failures_++;
        total_failures_++;
        total_attempts_++;
        last_error_message_ = e.what();
        last_error_code_ = -2;
        
        return false;
    }
}

bool BaseDeviceWorker::SendValuesToPipelineWithLogging(
    const std::vector<PulseOne::Structs::TimestampedValue>& values,
    const std::string& data_type,
    uint32_t priority) {
    
    if (values.empty()) {
        LogMessage(LogLevel::DEBUG_LEVEL, "전송할 데이터가 없음: " + data_type);
        return true;
    }
    
    try {
        // 파이프라인으로 실제 전송
        bool success = SendDataToPipeline(values, priority);
        
        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "파이프라인 전송 성공 (" + data_type + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        } else {
            LogMessage(LogLevel::ERROR, 
                      "파이프라인 전송 실패 (" + data_type + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "파이프라인 전송 예외 (" + data_type + "): " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

std::future<bool> BaseDeviceWorker::Pause() {
    return std::async(std::launch::async, [this]() -> bool {
        if (current_state_.load() == WorkerState::RUNNING) {
            ChangeState(WorkerState::PAUSED);
            LogMessage(LogLevel::INFO, "Worker 일시정지됨");
            return true;
        } else {
            LogMessage(LogLevel::WARN, "현재 상태에서 일시정지할 수 없음");
            return false;
        }
    });
}

std::future<bool> BaseDeviceWorker::Resume() {
    return std::async(std::launch::async, [this]() -> bool {
        if (current_state_.load() == WorkerState::PAUSED) {
            ChangeState(WorkerState::RUNNING);
            LogMessage(LogLevel::INFO, "Worker 재개됨");
            return true;
        } else {
            LogMessage(LogLevel::WARN, "현재 상태에서 재개할 수 없음");
            return false;
        }
    });
}

void BaseDeviceWorker::LogMessage(LogLevel level, const std::string& message) const {
    std::string prefix = "[Worker:" + device_info_.name + "] ";
    LogManager::getInstance().log("worker", level, prefix + message);
}

std::string BaseDeviceWorker::GetWorkerIdString() const {
    std::stringstream ss;
    ss << device_info_.GetProtocolName() << "_" << device_info_.name << "_" << device_info_.id;
    return ss.str();
}

void BaseDeviceWorker::UpdateCommunicationResult(bool success, 
                             const std::string& error_msg,
                             int error_code,
                             std::chrono::milliseconds response_time) {
    total_attempts_++;
    last_response_time_ = response_time;
    
    if (success) {
        consecutive_failures_ = 0;
        last_success_time_ = std::chrono::system_clock::now();
        last_error_message_ = "";
        last_error_code_ = 0;
    } else {
        consecutive_failures_++;
        total_failures_++;
        last_error_message_ = error_msg;
        last_error_code_ = error_code;
    }
}

void BaseDeviceWorker::OnStateChanged(WorkerState old_state, WorkerState new_state) {
    previous_state_ = old_state;
    state_change_time_ = std::chrono::system_clock::now();
    
    LogMessage(LogLevel::INFO, "상태 변경: " + 
              WorkerStateToString(old_state) + " → " + 
              WorkerStateToString(new_state));
}

PulseOne::Enums::DeviceStatus BaseDeviceWorker::ConvertWorkerStateToDeviceStatus(WorkerState state) const {
    switch (state) {
        case WorkerState::RUNNING:
        case WorkerState::SIMULATION:
        case WorkerState::CALIBRATION:
            return PulseOne::Enums::DeviceStatus::ONLINE;
            
        case WorkerState::ERROR:
        case WorkerState::DEVICE_OFFLINE:
        case WorkerState::COMMUNICATION_ERROR:
        case WorkerState::MAX_RETRIES_EXCEEDED:
            return PulseOne::Enums::DeviceStatus::ERROR;
            
        case WorkerState::MAINTENANCE:
        case WorkerState::COMMISSIONING:
            return PulseOne::Enums::DeviceStatus::MAINTENANCE;
            
        case WorkerState::STOPPED:
        case WorkerState::UNKNOWN:
        default:
            return PulseOne::Enums::DeviceStatus::OFFLINE;
    }
}

std::string BaseDeviceWorker::GetStatusMessage() const {
    switch (current_state_.load()) {
        case WorkerState::RUNNING:
            return "정상 동작 중";
        case WorkerState::ERROR:
            return "오류 발생: " + last_error_message_;
        case WorkerState::DEVICE_OFFLINE:
            return "디바이스 오프라인";
        case WorkerState::COMMUNICATION_ERROR:
            return "통신 오류";
        case WorkerState::MAINTENANCE:
            return "점검 중";
        default:
            return "상태 확인 중";
    }
}

std::string BaseDeviceWorker::GenerateCorrelationId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return device_info_.id + "_" + GetWorkerIdString() + "_" + std::to_string(timestamp);
}

std::string BaseDeviceWorker::WorkerStateToString(WorkerState state) const {
    switch (state) {
        case WorkerState::UNKNOWN: return "UNKNOWN";
        case WorkerState::STOPPED: return "STOPPED";
        case WorkerState::STARTING: return "STARTING";
        case WorkerState::RUNNING: return "RUNNING";
        case WorkerState::PAUSED: return "PAUSED";
        case WorkerState::STOPPING: return "STOPPING";
        case WorkerState::ERROR: return "ERROR";
        case WorkerState::MAINTENANCE: return "MAINTENANCE";
        case WorkerState::SIMULATION: return "SIMULATION";
        case WorkerState::CALIBRATION: return "CALIBRATION";
        case WorkerState::COMMISSIONING: return "COMMISSIONING";
        case WorkerState::DEVICE_OFFLINE: return "DEVICE_OFFLINE";
        case WorkerState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
        case WorkerState::DATA_INVALID: return "DATA_INVALID";
        case WorkerState::SENSOR_FAULT: return "SENSOR_FAULT";
        case WorkerState::MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        case WorkerState::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case WorkerState::BYPASS_MODE: return "BYPASS_MODE";
        case WorkerState::DIAGNOSTIC_MODE: return "DIAGNOSTIC_MODE";
        case WorkerState::RECONNECTING: return "RECONNECTING";
        case WorkerState::WAITING_RETRY: return "WAITING_RETRY";
        case WorkerState::MAX_RETRIES_EXCEEDED: return "MAX_RETRIES_EXCEEDED";
        default: return "UNDEFINED";
    }
}

uint32_t BaseDeviceWorker::GetNextSequenceNumber() {
    return ++sequence_counter_;
}

double BaseDeviceWorker::GetRawDoubleValue(const DataValue& value) const {
    return std::visit([](const auto& v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            try {
                return std::stod(v);
            } catch (...) {
                return 0.0;
            }
        } else {
            return 0.0;
        }
    }, value);
}

} // namespace Workers
} // namespace PulseOne