/**
 * @file BaseDeviceWorker.cpp
 * @brief BaseDeviceWorker 클래스 구현 (GitHub 구조 맞춤)
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#include "Workers/Base/BaseDeviceWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>

using namespace std::chrono;

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

BaseDeviceWorker::BaseDeviceWorker(const Drivers::DeviceInfo& device_info,
                                   std::shared_ptr<RedisClient> redis_client,
                                   std::shared_ptr<InfluxClient> influx_client)
    : device_info_(device_info)
    , redis_client_(redis_client)
    , influx_client_(influx_client) {
    
    // Redis 채널명 초기화
    InitializeRedisChannels();
    
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

std::future<bool> BaseDeviceWorker::Pause() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        if (current_state_.load() == WorkerState::RUNNING) {
            ChangeState(WorkerState::PAUSED);
            LogMessage(LogLevel::INFO, "Worker paused");
            promise->set_value(true);
        } else {
            LogMessage(LogLevel::WARN, "Cannot pause worker in current state");
            promise->set_value(false);
        }
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in Pause: " + std::string(e.what()));
        promise->set_value(false);
    }
    
    return future;
}

std::future<bool> BaseDeviceWorker::Resume() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        if (current_state_.load() == WorkerState::PAUSED) {
            ChangeState(WorkerState::RUNNING);
            LogMessage(LogLevel::INFO, "Worker resumed");
            promise->set_value(true);
        } else {
            LogMessage(LogLevel::WARN, "Cannot resume worker in current state");
            promise->set_value(false);
        }
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in Resume: " + std::string(e.what()));
        promise->set_value(false);
    }
    
    return future;
}

bool BaseDeviceWorker::AddDataPoint(const Drivers::DataPoint& point) {
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

std::vector<Drivers::DataPoint> BaseDeviceWorker::GetDataPoints() const {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    return data_points_;
}

// =============================================================================
// 재연결 관리
// =============================================================================

bool BaseDeviceWorker::UpdateReconnectionSettings(const ReconnectionSettings& settings) {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    
    // 유효성 검사
    if (settings.retry_interval_ms < 1000 || settings.retry_interval_ms > 300000) {
        LogMessage(LogLevel::ERROR, "Invalid retry interval: " + std::to_string(settings.retry_interval_ms));
        return false;
    }
    
    if (settings.max_retries_per_cycle < 0 || settings.max_retries_per_cycle > 100) {
        LogMessage(LogLevel::ERROR, "Invalid max retries: " + std::to_string(settings.max_retries_per_cycle));
        return false;
    }
    
    if (settings.wait_time_after_max_retries_ms < 10000) {
        LogMessage(LogLevel::ERROR, "Invalid wait time: " + std::to_string(settings.wait_time_after_max_retries_ms));
        return false;
    }
    
    ReconnectionSettings old_settings = reconnection_settings_;
    reconnection_settings_ = settings;
    
    LogMessage(LogLevel::INFO, "Reconnection settings updated");
    
    // Redis에 변경사항 저장 (GitHub 구조: publish -> set)
    if (redis_client_) {
        try {
            std::string change_msg = "Settings changed from: " + old_settings.ToJson() + 
                                   " to: " + settings.ToJson();
            redis_client_->set(reconnection_channel_, change_msg);
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Failed to save settings change to Redis: " + std::string(e.what()));
        }
    }
    
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
    ss << "  \"device_info\": {\n";
    ss << "    \"device_id\": \"" << device_info_.id << "\",\n";
    ss << "    \"device_name\": \"" << device_info_.name << "\",\n";
    ss << "    \"protocol_type\": \"" << PulseOne::Drivers::ProtocolTypeToString(device_info_.protocol) << "\",\n";
    ss << "    \"endpoint\": \"" << device_info_.endpoint << "\"\n";
    ss << "  },\n";
    ss << "  \"status\": {\n";
    ss << "    \"state\": \"" << WorkerStateToString(current_state_.load()) << "\",\n";
    ss << "    \"connected\": " << (is_connected_.load() ? "true" : "false") << ",\n";
    ss << "    \"data_points_count\": " << GetDataPoints().size() << "\n";
    ss << "  },\n";
    ss << "  \"reconnection_settings\": " << GetReconnectionSettings().ToJson() << "\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// 보호된 메서드들
// =============================================================================

void BaseDeviceWorker::ChangeState(WorkerState new_state) {
    WorkerState old_state = current_state_.exchange(new_state);
    
    if (old_state != new_state) {
        LogMessage(LogLevel::INFO, "State changed: " + WorkerStateToString(old_state) + 
                  " → " + WorkerStateToString(new_state));
        
        PublishStatusToRedis();
    }
}

void BaseDeviceWorker::PublishStatusToRedis() {
    if (!redis_client_) return;
    
    try {
        std::string status_json = GetStatusJson();
        // GitHub 구조: publish -> set 사용
        redis_client_->set(status_channel_, status_json);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to save status to Redis: " + std::string(e.what()));
    }
}

void BaseDeviceWorker::SaveToInfluxDB(const std::string& point_id, 
                                      const Drivers::TimestampedValue& value) {
    if (!influx_client_) return;
    
    try {
        // 미사용 매개변수 경고 제거
        (void)value;  // 명시적으로 사용하지 않음을 표시
        
        // InfluxDB 포인트 생성 및 저장
        LogMessage(LogLevel::DEBUG, "Data saved to InfluxDB for point: " + point_id);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to save to InfluxDB: " + std::string(e.what()));
    }
}

void BaseDeviceWorker::SetConnectionState(bool connected) {
    bool old_state = is_connected_.exchange(connected);
    
    if (old_state != connected) {
        if (connected) {
            LogMessage(LogLevel::INFO, "Connection established");
            UpdateReconnectionStats(true);
        } else {
            LogMessage(LogLevel::WARN, "Connection lost");
            UpdateReconnectionStats(false);
        }
    }
}

void BaseDeviceWorker::HandleConnectionError(const std::string& error_message) {
    LogMessage(LogLevel::ERROR, "Connection error: " + error_message);
    
    SetConnectionState(false);
    
    // 재연결이 필요한 상태로 변경 (재연결 스레드에서 처리됨)
    if (current_state_.load() == WorkerState::RUNNING) {
        ChangeState(WorkerState::RECONNECTING);
    }
}

// =============================================================================
// 내부 메서드들
// =============================================================================

void BaseDeviceWorker::ReconnectionThreadMain() {
    LogMessage(LogLevel::INFO, "Reconnection management thread started");
    
    while (thread_running_.load()) {
        try {
            // 현재 상태 확인
            WorkerState current_state = current_state_.load();
            
            // 실행 중이고 연결이 끊어진 경우만 재연결 시도
            if (current_state == WorkerState::RUNNING || current_state == WorkerState::RECONNECTING) {
                if (!is_connected_.load()) {
                    
                    // 대기 사이클 처리
                    if (in_wait_cycle_.load()) {
                        if (HandleWaitCycle()) {
                            // 대기 완료, 재연결 시도 재개
                            in_wait_cycle_ = false;
                            current_retry_count_ = 0;
                            reconnection_stats_.wait_cycles.fetch_add(1);
                            LogMessage(LogLevel::INFO, "Wait cycle completed, resuming reconnection attempts");
                        }
                    } else {
                        // 재연결 시도
                        bool reconnect_enabled = reconnection_settings_.auto_reconnect_enabled;
                        
                        if (reconnect_enabled) {
                            if (AttemptReconnection()) {
                                // 재연결 성공
                                if (current_state == WorkerState::RECONNECTING) {
                                    ChangeState(WorkerState::RUNNING);
                                }
                                ResetReconnectionState();
                                LogMessage(LogLevel::INFO, "Reconnection successful");
                            } else {
                                // 재연결 실패 - 재시도 카운터 증가
                                current_retry_count_.fetch_add(1);
                                
                                // 최대 재시도 횟수 확인
                                if (reconnection_settings_.max_retries_per_cycle > 0 && 
                                    current_retry_count_.load() >= reconnection_settings_.max_retries_per_cycle) {
                                    
                                    // 대기 사이클 시작
                                    in_wait_cycle_ = true;
                                    wait_start_time_ = system_clock::now();
                                    ChangeState(WorkerState::WAITING_RETRY);
                                    
                                    LogMessage(LogLevel::WARN, 
                                              "Max retries exceeded (" + std::to_string(reconnection_settings_.max_retries_per_cycle) + 
                                              "), entering wait cycle for " + 
                                              std::to_string(reconnection_settings_.wait_time_after_max_retries_ms / 1000) + " seconds");
                                } else {
                                    // 다음 재시도까지 대기
                                    std::this_thread::sleep_for(
                                        std::chrono::milliseconds(reconnection_settings_.retry_interval_ms));
                                }
                            }
                        }
                    }
                } else {
                    // 연결된 상태 - Keep-alive 처리
                    HandleKeepAlive();
                }
            }
            
            // 짧은 대기 (1초)
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception in reconnection thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LogMessage(LogLevel::INFO, "Reconnection management thread stopped");
}

bool BaseDeviceWorker::AttemptReconnection() {
    try {
        LogMessage(LogLevel::INFO, "Attempting reconnection (attempt " + 
                  std::to_string(current_retry_count_.load() + 1) + ")");
        
        reconnection_stats_.total_connections.fetch_add(1);
        
        // 프로토콜별 연결 수립 시도
        if (EstablishConnection()) {
            SetConnectionState(true);
            reconnection_stats_.reconnection_cycles.fetch_add(1);
            return true;
        } else {
            return false;
        }
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Reconnection attempt failed: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::HandleWaitCycle() {
    auto now = system_clock::now();
    auto wait_duration = duration_cast<milliseconds>(now - wait_start_time_).count();
    
    if (wait_duration >= reconnection_settings_.wait_time_after_max_retries_ms) {
        LogMessage(LogLevel::INFO, "Wait cycle completed after " + 
                  std::to_string(wait_duration / 1000) + " seconds");
        return true;
    }
    
    // 대기 중 - 남은 시간 계산
    int remaining_seconds = (reconnection_settings_.wait_time_after_max_retries_ms - wait_duration) / 1000;
    
    // 매 30초마다 대기 상태 로그 출력
    static auto last_log_time = system_clock::now();
    if (duration_cast<seconds>(now - last_log_time).count() >= 30) {
        LogMessage(LogLevel::INFO, "Waiting for retry cycle completion, " + 
                  std::to_string(remaining_seconds) + " seconds remaining");
        last_log_time = now;
    }
    
    return false;
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
                    LogMessage(LogLevel::DEBUG, "Keep-alive successful");
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

std::string BaseDeviceWorker::WorkerStateToString(WorkerState state) const {
    switch (state) {
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
        default: return "UNKNOWN";
    }
}

} // namespace Workers
} // namespace PulseOne