/**
 * @file BaseDeviceWorker.cpp
 * @brief BaseDeviceWorker 클래스 구현
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "Workers/Base/BaseDeviceWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BaseDeviceWorker::BaseDeviceWorker(const Drivers::DeviceInfo& device_info,
                                   std::shared_ptr<RedisClient> redis_client,
                                   std::shared_ptr<InfluxClient> influx_client,
                                   std::shared_ptr<LogManager> logger)
    : device_info_(device_info)
    , redis_client_(redis_client)
    , influx_client_(influx_client)
    , logger_(logger) {
    
    // Redis 채널명 초기화
    InitializeRedisChannels();
    
    // 통계 초기화
    statistics_.start_time = std::chrono::system_clock::now();
    
    if (logger_) {
        logger_->Info("BaseDeviceWorker created for device: " + device_info_.device_name);
    }
}

BaseDeviceWorker::~BaseDeviceWorker() {
    if (logger_) {
        logger_->Info("BaseDeviceWorker destroyed for device: " + device_info_.device_name);
    }
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
            LogMessage(LogLevel::WARN, "Cannot pause worker - not in running state");
            promise->set_value(false);
        }
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to pause worker: " + std::string(e.what()));
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
            LogMessage(LogLevel::WARN, "Cannot resume worker - not in paused state");
            promise->set_value(false);
        }
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to resume worker: " + std::string(e.what()));
        promise->set_value(false);
    }
    
    return future;
}

std::future<bool> BaseDeviceWorker::Restart() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            LogMessage(LogLevel::INFO, "Restarting worker...");
            
            // 먼저 정지
            auto stop_future = Stop();
            if (!stop_future.get()) {
                LogMessage(LogLevel::ERROR, "Failed to stop worker during restart");
                promise->set_value(false);
                return;
            }
            
            // 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // 다시 시작
            auto start_future = Start();
            bool result = start_future.get();
            
            if (result) {
                LogMessage(LogLevel::INFO, "Worker restarted successfully");
            } else {
                LogMessage(LogLevel::ERROR, "Failed to start worker during restart");
            }
            
            promise->set_value(result);
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception during restart: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

void BaseDeviceWorker::Shutdown() {
    LogMessage(LogLevel::INFO, "Shutting down worker...");
    
    try {
        // 정지 시도
        auto stop_future = Stop();
        
        // 최대 5초 대기
        auto status = stop_future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            LogMessage(LogLevel::WARN, "Worker shutdown timed out");
        }
        
        ChangeState(WorkerState::STOPPED);
        LogMessage(LogLevel::INFO, "Worker shutdown completed");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception during shutdown: " + std::string(e.what()));
    }
}

// =============================================================================
// 현장 운영 모드 제어
// =============================================================================

bool BaseDeviceWorker::EnableMaintenanceMode(const std::string& reason, 
                                            std::chrono::minutes estimated_duration) {
    try {
        maintenance_reason_ = reason;
        mode_change_time_ = std::chrono::system_clock::now();
        alarm_suppression_enabled_ = true;
        
        ChangeState(WorkerState::MAINTENANCE);
        
        statistics_.maintenance_time_seconds += 
            std::chrono::duration_cast<std::chrono::seconds>(estimated_duration).count();
        
        LogMessage(LogLevel::INFO, "Maintenance mode enabled: " + reason + 
                  " (estimated duration: " + std::to_string(estimated_duration.count()) + " minutes)");
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to enable maintenance mode: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::DisableMaintenanceMode() {
    try {
        if (current_state_.load() != WorkerState::MAINTENANCE) {
            LogMessage(LogLevel::WARN, "Not in maintenance mode");
            return false;
        }
        
        // 실제 점검 시간 계산
        auto now = std::chrono::system_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - mode_change_time_).count();
        
        alarm_suppression_enabled_ = false;
        ChangeState(WorkerState::RUNNING);
        
        LogMessage(LogLevel::INFO, "Maintenance mode disabled (actual duration: " + 
                  std::to_string(actual_duration) + " seconds)");
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to disable maintenance mode: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::EnableSimulationMode(const std::string& test_data_file) {
    try {
        ChangeState(WorkerState::SIMULATION);
        mode_change_time_ = std::chrono::system_clock::now();
        
        LogMessage(LogLevel::INFO, "Simulation mode enabled" + 
                  (test_data_file.empty() ? "" : " with data file: " + test_data_file));
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to enable simulation mode: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::EnableDiagnosticMode(int diagnostic_level) {
    try {
        ChangeState(WorkerState::DIAGNOSTIC_MODE);
        mode_change_time_ = std::chrono::system_clock::now();
        
        LogMessage(LogLevel::INFO, "Diagnostic mode enabled (level: " + 
                  std::to_string(diagnostic_level) + ")");
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to enable diagnostic mode: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::EnableManualOverride(const std::string& operator_id, 
                                           const std::string& reason) {
    try {
        current_operator_id_ = operator_id;
        maintenance_reason_ = reason;
        mode_change_time_ = std::chrono::system_clock::now();
        
        ChangeState(WorkerState::MANUAL_OVERRIDE);
        statistics_.manual_overrides++;
        
        LogMessage(LogLevel::WARN, "Manual override enabled by " + operator_id + ": " + reason);
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to enable manual override: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::TriggerEmergencyStop(const std::string& emergency_reason) {
    try {
        emergency_reason_ = emergency_reason;
        mode_change_time_ = std::chrono::system_clock::now();
        
        ChangeState(WorkerState::EMERGENCY_STOP);
        statistics_.emergency_stops++;
        
        LogMessage(LogLevel::CRITICAL, "EMERGENCY STOP triggered: " + emergency_reason);
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to trigger emergency stop: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::ReturnToNormalOperation(const std::string& operator_id) {
    try {
        current_operator_id_ = operator_id;
        ChangeState(WorkerState::RUNNING);
        
        LogMessage(LogLevel::INFO, "Returned to normal operation by " + operator_id);
        
        PublishStatusToRedis();
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to return to normal operation: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 데이터 포인트 관리
// =============================================================================

bool BaseDeviceWorker::AddDataPoint(const Drivers::DataPoint& point) {
    try {
        std::lock_guard<std::mutex> lock(data_points_mutex_);
        data_points_[point.id] = point;
        
        LogMessage(LogLevel::INFO, "Data point added: " + point.name);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to add data point: " + std::string(e.what()));
        return false;
    }
}

bool BaseDeviceWorker::RemoveDataPoint(const UUID& point_id) {
    try {
        std::lock_guard<std::mutex> lock(data_points_mutex_);
        auto it = data_points_.find(point_id);
        if (it != data_points_.end()) {
            LogMessage(LogLevel::INFO, "Data point removed: " + it->second.name);
            data_points_.erase(it);
            return true;
        }
        return false;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to remove data point: " + std::string(e.what()));
        return false;
    }
}

void BaseDeviceWorker::SetPollingInterval(int interval_ms) {
    polling_interval_ms_ = std::max(100, interval_ms); // 최소 100ms
    LogMessage(LogLevel::INFO, "Polling interval changed to " + std::to_string(interval_ms) + "ms");
}

std::vector<Drivers::DataPoint> BaseDeviceWorker::GetDataPoints() const {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    std::vector<Drivers::DataPoint> points;
    points.reserve(data_points_.size());
    
    for (const auto& pair : data_points_) {
        points.push_back(pair.second);
    }
    return points;
}

// =============================================================================
// 상태 조회
// =============================================================================

std::string BaseDeviceWorker::GetStatusJson() const {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"device_id\": \"" << device_info_.device_id.to_string() << "\",\n";
    json << "  \"device_name\": \"" << device_info_.device_name << "\",\n";
    json << "  \"state\": " << static_cast<int>(current_state_.load()) << ",\n";
    json << "  \"state_name\": \"" << WorkerStateToString(current_state_.load()) << "\",\n";
    json << "  \"polling_interval_ms\": " << polling_interval_ms_.load() << ",\n";
    json << "  \"data_points_count\": " << data_points_.size() << ",\n";
    json << "  \"maintenance_mode\": " << (current_state_.load() == WorkerState::MAINTENANCE ? "true" : "false") << ",\n";
    json << "  \"simulation_mode\": " << (current_state_.load() == WorkerState::SIMULATION ? "true" : "false") << ",\n";
    json << "  \"emergency_state\": " << (IsInEmergencyState() ? "true" : "false") << ",\n";
    json << "  \"data_valid\": " << (IsDataValid() ? "true" : "false") << ",\n";
    json << "  \"statistics\": {\n";
    json << "    \"total_reads\": " << statistics_.total_reads.load() << ",\n";
    json << "    \"successful_reads\": " << statistics_.successful_reads.load() << ",\n";
    json << "    \"communication_errors\": " << statistics_.communication_errors.load() << ",\n";
    json << "    \"emergency_stops\": " << statistics_.emergency_stops.load() << ",\n";
    json << "    \"manual_overrides\": " << statistics_.manual_overrides.load() << "\n";
    json << "  }\n";
    json << "}";
    
    return json.str();
}

// =============================================================================
// 공통 유틸리티 함수들
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
        redis_client_->publish(status_channel_, status_json);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to publish status to Redis: " + std::string(e.what()));
    }
}

void BaseDeviceWorker::SaveToInfluxDB(const UUID& point_id, const Drivers::TimestampedValue& value) {
    if (!influx_client_) return;
    
    try {
        // InfluxDB 포인트 생성 및 저장 로직
        // 실제 구현은 InfluxClient API에 따라 달라짐
        
        // 예시 구조:
        // influx_client_->writePoint(measurement, tags, fields, timestamp);
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to save to InfluxDB: " + std::string(e.what()));
    }
}

void BaseDeviceWorker::UpdateStatistics(bool read_success, bool write_success, uint32_t response_time_ms) {
    if (read_success) {
        statistics_.total_reads++;
        statistics_.successful_reads++;
        statistics_.last_read_time = std::chrono::system_clock::now();
    } else if (statistics_.total_reads > 0) {
        statistics_.total_reads++;
    }
    
    if (write_success) {
        statistics_.total_writes++;
        statistics_.successful_writes++;
    } else if (statistics_.total_writes > 0) {
        statistics_.total_writes++;
    }
    
    if (response_time_ms > 0) {
        // 이동 평균 계산
        uint32_t current_avg = statistics_.avg_response_time_ms.load();
        uint32_t new_avg = (current_avg + response_time_ms) / 2;
        statistics_.avg_response_time_ms = new_avg;
    }
}

void BaseDeviceWorker::LogMessage(LogLevel level, const std::string& message) {
    if (logger_) {
        std::string full_message = "[" + device_info_.device_name + "] " + message;
        
        switch (level) {
            case LogLevel::DEBUG:
                logger_->Debug(full_message);
                break;
            case LogLevel::INFO:
                logger_->Info(full_message);
                break;
            case LogLevel::WARN:
                logger_->Warn(full_message);
                break;
            case LogLevel::ERROR:
                logger_->Error(full_message);
                break;
            case LogLevel::CRITICAL:
                logger_->Critical(full_message);
                break;
        }
    }
}

void BaseDeviceWorker::InitializeRedisChannels() {
    status_channel_ = "device_status:" + device_info_.device_id.to_string();
    command_channel_ = "device_command:" + device_info_.device_id.to_string();
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
        default: return "UNKNOWN";
    }
}

} // namespace Workers
} // namespace PulseOne