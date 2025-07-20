/**
 * @file DeviceWorker.cpp
 * @brief 개별 디바이스 워커 클래스 구현
 * @details 멀티스레드 기반 디바이스 제어 및 모니터링 구현
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#include "Engine/DeviceWorker.h"
#include "Engine/MaintenanceManager.h"
#include "Engine/AlarmContextEngine.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <regex>
#include <cmath>

namespace PulseOne {
namespace Engine {

using json = nlohmann::json;
using namespace std::chrono;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DeviceWorker::DeviceWorker(const Drivers::DeviceInfo& device_info,
                           std::unique_ptr<Drivers::IProtocolDriver> driver,
                           std::shared_ptr<RedisClient> redis_client,
                           std::shared_ptr<InfluxClient> influx_client,
                           std::shared_ptr<LogManager> logger)
    : device_info_(device_info)
    , driver_(std::move(driver))
    , redis_client_(redis_client)
    , influx_client_(influx_client)
    , logger_(logger)
    , current_state_(WorkerState::STOPPED)
    , shutdown_requested_(false)
    , polling_interval_ms_(device_info.polling_interval_ms)
    , auto_reconnect_enabled_(device_info.auto_reconnect)
{
    // Redis 채널명 생성
    std::string device_id_str = device_info_.device_id.to_string();
    status_channel_ = "device_status:" + device_id_str;
    command_channel_ = "device_commands:" + device_id_str;
    alarm_channel_ = "device_alarms:" + device_id_str;
    
    // 점검 모드 관리자 초기화
    maintenance_manager_ = std::make_unique<MaintenanceManager>(device_info_.device_id, redis_client_, logger_);
    
    // 알람 엔진 초기화
    alarm_engine_ = std::make_unique<AlarmContextEngine>(device_info_.device_id, redis_client_, logger_);
    
    // 통계 초기화
    statistics_.start_time = system_clock::now();
    
    logger_->Info("DeviceWorker created for device: " + device_info_.device_name);
}

DeviceWorker::~DeviceWorker() {
    Shutdown();
}

// =============================================================================
// 기본 제어 인터페이스
// =============================================================================

std::future<bool> DeviceWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            std::lock_guard<std::mutex> lock(state_mutex_);
            
            if (current_state_ == WorkerState::RUNNING) {
                logger_->Warn("Device already running: " + device_info_.device_name);
                promise->set_value(true);
                return;
            }
            
            ChangeState(WorkerState::STARTING);
            
            // 드라이버 초기화
            if (!driver_->Initialize()) {
                logger_->Error("Failed to initialize driver for: " + device_info_.device_name);
                ChangeState(WorkerState::ERROR);
                promise->set_value(false);
                return;
            }
            
            // 드라이버 시작
            if (!driver_->Start()) {
                logger_->Error("Failed to start driver for: " + device_info_.device_name);
                ChangeState(WorkerState::ERROR);
                promise->set_value(false);
                return;
            }
            
            // 워커 스레드들 시작
            shutdown_requested_ = false;
            read_thread_ = std::make_unique<std::thread>(&DeviceWorker::ReadThreadMain, this);
            write_thread_ = std::make_unique<std::thread>(&DeviceWorker::WriteThreadMain, this);
            control_thread_ = std::make_unique<std::thread>(&DeviceWorker::ControlThreadMain, this);
            
            ChangeState(WorkerState::RUNNING);
            logger_->Info("DeviceWorker started successfully: " + device_info_.device_name);
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in Start(): " + std::string(e.what()));
            ChangeState(WorkerState::ERROR);
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> DeviceWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            std::lock_guard<std::mutex> lock(state_mutex_);
            
            if (current_state_ == WorkerState::STOPPED) {
                promise->set_value(true);
                return;
            }
            
            ChangeState(WorkerState::STOPPING);
            
            // 스레드 종료 요청
            shutdown_requested_ = true;
            control_queue_cv_.notify_all();
            
            // 스레드 조인
            if (read_thread_ && read_thread_->joinable()) {
                read_thread_->join();
            }
            if (write_thread_ && write_thread_->joinable()) {
                write_thread_->join();
            }
            if (control_thread_ && control_thread_->joinable()) {
                control_thread_->join();
            }
            
            // 드라이버 정지
            driver_->Stop();
            
            ChangeState(WorkerState::STOPPED);
            logger_->Info("DeviceWorker stopped: " + device_info_.device_name);
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in Stop(): " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> DeviceWorker::Pause() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            std::lock_guard<std::mutex> lock(state_mutex_);
            
            if (current_state_ != WorkerState::RUNNING) {
                promise->set_value(false);
                return;
            }
            
            ChangeState(WorkerState::PAUSED);
            logger_->Info("DeviceWorker paused: " + device_info_.device_name);
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in Pause(): " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> DeviceWorker::Resume() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            std::lock_guard<std::mutex> lock(state_mutex_);
            
            if (current_state_ != WorkerState::PAUSED) {
                promise->set_value(false);
                return;
            }
            
            ChangeState(WorkerState::RUNNING);
            logger_->Info("DeviceWorker resumed: " + device_info_.device_name);
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in Resume(): " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> DeviceWorker::Restart() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            // 먼저 정지
            auto stop_future = Stop();
            if (!stop_future.get()) {
                promise->set_value(false);
                return;
            }
            
            // 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            // 다시 시작
            auto start_future = Start();
            promise->set_value(start_future.get());
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in Restart(): " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

void DeviceWorker::Shutdown() {
    if (current_state_ != WorkerState::STOPPED) {
        Stop().wait();
    }
    
    // 리소스 정리
    read_thread_.reset();
    write_thread_.reset();
    control_thread_.reset();
    
    logger_->Info("DeviceWorker shutdown completed: " + device_info_.device_name);
}

// =============================================================================
// 하드웨어 제어 인터페이스
// =============================================================================

std::future<bool> DeviceWorker::ExecuteHardwareControl(
    const UUID& point_id,
    const Drivers::DataValue& value,
    const std::string& command_type,
    const std::map<std::string, std::string>& params,
    int timeout_ms) {
    
    HardwareControlRequest request;
    request.point_id = point_id;
    request.value = value;
    request.command_type = command_type;
    request.params = params;
    request.timeout = system_clock::now() + milliseconds(timeout_ms);
    
    auto future = request.result_promise.get_future();
    
    // 제어 큐에 추가
    {
        std::lock_guard<std::mutex> lock(control_queue_mutex_);
        control_queue_.push(std::move(request));
    }
    control_queue_cv_.notify_one();
    
    return future;
}

std::future<bool> DeviceWorker::ControlPump(const UUID& pump_id, bool enable) {
    std::map<std::string, std::string> params;
    params["pump_action"] = enable ? "start" : "stop";
    
    Drivers::DataValue control_value(enable ? 1 : 0);
    return ExecuteHardwareControl(pump_id, control_value, "pump_control", params);
}

std::future<bool> DeviceWorker::ControlValve(const UUID& valve_id, double position) {
    std::map<std::string, std::string> params;
    params["valve_position"] = std::to_string(position);
    
    // 위치를 0-100 범위로 제한
    position = std::max(0.0, std::min(100.0, position));
    
    Drivers::DataValue control_value(position);
    return ExecuteHardwareControl(valve_id, control_value, "valve_control", params);
}

std::future<bool> DeviceWorker::ChangeSetpoint(const UUID& setpoint_id, double value) {
    std::map<std::string, std::string> params;
    params["setpoint_value"] = std::to_string(value);
    
    Drivers::DataValue control_value(value);
    return ExecuteHardwareControl(setpoint_id, control_value, "setpoint_change", params);
}

// =============================================================================
// 점검 모드 제어
// =============================================================================

bool DeviceWorker::EnableMaintenanceMode(const std::string& reason, 
                                         std::chrono::minutes estimated_duration) {
    if (!maintenance_manager_) return false;
    
    bool result = maintenance_manager_->EnableMaintenanceMode(reason, estimated_duration);
    if (result) {
        logger_->Info("Maintenance mode enabled for device: " + device_info_.device_name + ", reason: " + reason);
        PublishStatusToRedis();
    }
    return result;
}

bool DeviceWorker::DisableMaintenanceMode() {
    if (!maintenance_manager_) return false;
    
    bool result = maintenance_manager_->DisableMaintenanceMode();
    if (result) {
        logger_->Info("Maintenance mode disabled for device: " + device_info_.device_name);
        PublishStatusToRedis();
    }
    return result;
}

bool DeviceWorker::IsInMaintenanceMode() const {
    return maintenance_manager_ && maintenance_manager_->IsInMaintenanceMode();
}

// =============================================================================
// 설정 관리
// =============================================================================

bool DeviceWorker::AddDataPoint(const Drivers::DataPoint& point) {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    data_points_[point.id] = point;
    logger_->Info("Data point added: " + point.name + " for device: " + device_info_.device_name);
    return true;
}

bool DeviceWorker::RemoveDataPoint(const UUID& point_id) {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    auto it = data_points_.find(point_id);
    if (it != data_points_.end()) {
        logger_->Info("Data point removed: " + it->second.name + " from device: " + device_info_.device_name);
        data_points_.erase(it);
        latest_values_.erase(point_id);
        return true;
    }
    return false;
}

bool DeviceWorker::AddVirtualPointRule(const VirtualPointRule& rule) {
    std::lock_guard<std::mutex> lock(virtual_points_mutex_);
    virtual_point_rules_[rule.virtual_point_id] = rule;
    logger_->Info("Virtual point rule added for device: " + device_info_.device_name);
    return true;
}

bool DeviceWorker::AddAlarmRule(const AlarmRule& rule) {
    std::lock_guard<std::mutex> lock(alarm_rules_mutex_);
    alarm_rules_[rule.alarm_id] = rule;
    logger_->Info("Alarm rule added for device: " + device_info_.device_name);
    return true;
}

void DeviceWorker::SetPollingInterval(int interval_ms) {
    polling_interval_ms_ = std::max(100, interval_ms); // 최소 100ms
    logger_->Info("Polling interval changed to " + std::to_string(interval_ms) + "ms for device: " + device_info_.device_name);
}

// =============================================================================
// 상태 조회
// =============================================================================

Drivers::ConnectionStatus DeviceWorker::GetConnectionStatus() const {
    if (!driver_) return Drivers::ConnectionStatus::DISCONNECTED;
    return driver_->GetConnectionStatus();
}

std::vector<Drivers::DataPoint> DeviceWorker::GetDataPoints() const {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    std::vector<Drivers::DataPoint> points;
    points.reserve(data_points_.size());
    
    for (const auto& pair : data_points_) {
        points.push_back(pair.second);
    }
    return points;
}

std::string DeviceWorker::GetStatusJson() const {
    json status;
    
    try {
        status["device_id"] = device_info_.device_id.to_string();
        status["device_name"] = device_info_.device_name;
        status["protocol"] = Drivers::ProtocolTypeToString(device_info_.protocol);
        status["worker_state"] = static_cast<int>(current_state_.load());
        status["connection_status"] = static_cast<int>(GetConnectionStatus());
        status["polling_interval_ms"] = polling_interval_ms_.load();
        status["auto_reconnect"] = auto_reconnect_enabled_.load();
        status["maintenance_mode"] = IsInMaintenanceMode();
        
        // 통계 정보
        status["statistics"]["total_reads"] = statistics_.total_reads.load();
        status["statistics"]["successful_reads"] = statistics_.successful_reads.load();
        status["statistics"]["total_writes"] = statistics_.total_writes.load();
        status["statistics"]["successful_writes"] = statistics_.successful_writes.load();
        status["statistics"]["hardware_commands"] = statistics_.hardware_commands.load();
        status["statistics"]["alarms_triggered"] = statistics_.alarms_triggered.load();
        status["statistics"]["virtual_calculations"] = statistics_.virtual_calculations.load();
        status["statistics"]["avg_response_time_ms"] = statistics_.avg_response_time_ms.load();
        
        // 시간 정보
        auto now = system_clock::now();
        auto uptime = duration_cast<seconds>(now - statistics_.start_time).count();
        status["uptime_seconds"] = uptime;
        
        auto last_read_duration = duration_cast<seconds>(now - statistics_.last_read_time).count();
        status["last_read_seconds_ago"] = last_read_duration;
        
        // 데이터 포인트 수
        {
            std::lock_guard<std::mutex> lock(data_points_mutex_);
            status["data_points_count"] = data_points_.size();
        }
        
        // 가상 포인트 및 알람 규칙 수
        {
            std::lock_guard<std::mutex> lock(virtual_points_mutex_);
            status["virtual_points_count"] = virtual_point_rules_.size();
        }
        {
            std::lock_guard<std::mutex> lock(alarm_rules_mutex_);
            status["alarm_rules_count"] = alarm_rules_.size();
        }
        
        status["timestamp"] = duration_cast<milliseconds>(now.time_since_epoch()).count();
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to generate status JSON: " + std::string(e.what()));
        status["error"] = "Failed to generate status";
    }
    
    return status.dump();
}

// =============================================================================
// 내부 스레드 함수들
// =============================================================================

void DeviceWorker::ReadThreadMain() {
    logger_->Info("Read thread started for device: " + device_info_.device_name);
    
    while (!shutdown_requested_) {
        try {
            // 일시정지 상태면 대기
            if (current_state_ == WorkerState::PAUSED) {
                std::this_thread::sleep_for(milliseconds(100));
                continue;
            }
            
            // 실행 상태가 아니면 대기
            if (current_state_ != WorkerState::RUNNING) {
                std::this_thread::sleep_for(milliseconds(100));
                continue;
            }
            
            auto start_time = steady_clock::now();
            
            // 데이터 포인트들 읽기
            std::vector<Drivers::DataPoint> points_to_read;
            {
                std::lock_guard<std::mutex> lock(data_points_mutex_);
                for (const auto& pair : data_points_) {
                    if (pair.second.is_enabled) {
                        points_to_read.push_back(pair.second);
                    }
                }
            }
            
            if (!points_to_read.empty()) {
                std::vector<Drivers::TimestampedValue> values;
                statistics_.total_reads++;
                
                if (driver_->ReadValues(points_to_read, values)) {
                    statistics_.successful_reads++;
                    statistics_.last_read_time = system_clock::now();
                    
                    // 읽은 값들 처리
                    for (size_t i = 0; i < points_to_read.size() && i < values.size(); ++i) {
                        const auto& point = points_to_read[i];
                        const auto& value = values[i];
                        
                        // 최신 값 저장
                        {
                            std::lock_guard<std::mutex> lock(data_points_mutex_);
                            latest_values_[point.id] = value;
                        }
                        
                        // InfluxDB에 저장
                        SaveToInfluxDB(point.id, value);
                        
                        // 알람 검사 (점검 모드가 아닐 때만)
                        if (!IsInMaintenanceMode()) {
                            CheckAlarms(point.id, value);
                        }
                    }
                    
                    // 가상 포인트 계산
                    CalculateVirtualPoints();
                    
                } else {
                    logger_->Warn("Failed to read data from device: " + device_info_.device_name);
                    
                    // 자동 재연결 시도
                    if (auto_reconnect_enabled_) {
                        AttemptReconnection();
                    }
                }
            }
            
            // 응답 시간 업데이트
            auto end_time = steady_clock::now();
            auto response_time = duration_cast<milliseconds>(end_time - start_time);
            if (driver_) {
                driver_->UpdateResponseTime(response_time);
            }
            
            // 폴링 간격만큼 대기
            std::this_thread::sleep_for(milliseconds(polling_interval_ms_));
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in read thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(1000));
        }
    }
    
    logger_->Info("Read thread stopped for device: " + device_info_.device_name);
}

void DeviceWorker::WriteThreadMain() {
    logger_->Info("Write thread started for device: " + device_info_.device_name);
    
    // 현재는 읽기 전용이지만 향후 확장을 위해 스레드 유지
    while (!shutdown_requested_) {
        std::this_thread::sleep_for(milliseconds(1000));
    }
    
    logger_->Info("Write thread stopped for device: " + device_info_.device_name);
}

void DeviceWorker::ControlThreadMain() {
    logger_->Info("Control thread started for device: " + device_info_.device_name);
    
    while (!shutdown_requested_) {
        try {
            HardwareControlRequest request;
            bool has_request = false;
            
            // 제어 요청 확인
            {
                std::unique_lock<std::mutex> lock(control_queue_mutex_);
                if (control_queue_cv_.wait_for(lock, milliseconds(1000), 
                    [this] { return !control_queue_.empty() || shutdown_requested_; })) {
                    
                    if (!control_queue_.empty() && !shutdown_requested_) {
                        request = std::move(control_queue_.front());
                        control_queue_.pop();
                        has_request = true;
                    }
                }
            }
            
            if (has_request) {
                // 타임아웃 확인
                auto now = system_clock::now();
                if (now > request.timeout) {
                    request.result_promise.set_value(false);
                    logger_->Warn("Hardware control request timed out");
                    continue;
                }
                
                try {
                    statistics_.hardware_commands++;
                    
                    // 제어 명령 실행
                    bool success = false;
                    
                    if (request.command_type == "pump_control") {
                        // 펌프 제어
                        success = driver_->WriteValue(request.point_id, request.value);
                        if (success) {
                            logger_->Info("Pump control executed successfully");
                        }
                    } else if (request.command_type == "valve_control") {
                        // 밸브 제어
                        success = driver_->WriteValue(request.point_id, request.value);
                        if (success) {
                            logger_->Info("Valve control executed successfully");
                        }
                    } else if (request.command_type == "setpoint_change") {
                        // 설정값 변경
                        success = driver_->WriteValue(request.point_id, request.value);
                        if (success) {
                            logger_->Info("Setpoint changed successfully");
                        }
                    } else {
                        // 일반 쓰기
                        success = driver_->WriteValue(request.point_id, request.value);
                    }
                    
                    request.result_promise.set_value(success);
                    
                    // Redis에 결과 발행
                    PublishCommandResult(request.command_type, success, 
                                       success ? "Command executed successfully" : "Command failed");
                    
                } catch (const std::exception& e) {
                    logger_->Error("Exception in hardware control: " + std::string(e.what()));
                    request.result_promise.set_value(false);
                }
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in control thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(1000));
        }
    }
    
    logger_->Info("Control thread stopped for device: " + device_info_.device_name);
}

// =============================================================================
// 내부 유틸리티 함수들
// =============================================================================

void DeviceWorker::ChangeState(WorkerState new_state) {
    WorkerState old_state = current_state_.exchange(new_state);
    if (old_state != new_state) {
        logger_->Info("Worker state changed from " + std::to_string(static_cast<int>(old_state)) + 
                     " to " + std::to_string(static_cast<int>(new_state)) + 
                     " for device: " + device_info_.device_name);
        PublishStatusToRedis();
    }
}

void DeviceWorker::PublishStatusToRedis() {
    if (!redis_client_) return;
    
    try {
        std::string status_json = GetStatusJson();
        redis_client_->Publish(status_channel_, status_json);
    } catch (const std::exception& e) {
        logger_->Error("Failed to publish status to Redis: " + std::string(e.what()));
    }
}

void DeviceWorker::SaveToInfluxDB(const UUID& point_id, const Drivers::TimestampedValue& value) {
    if (!influx_client_) return;
    
    try {
        // InfluxDB 포인트 생성 (간단한 구현)
        std::string measurement = "device_data";
        std::map<std::string, std::string> tags;
        tags["device_id"] = device_info_.device_id.to_string();
        tags["point_id"] = point_id.to_string();
        tags["device_name"] = device_info_.device_name;
        
        std::map<std::string, double> fields;
        fields["value"] = value.value.to_double();
        fields["quality"] = static_cast<double>(value.quality);
        
        auto timestamp = duration_cast<nanoseconds>(value.timestamp.time_since_epoch()).count();
        
        // InfluxDB에 쓰기 (실제 구현은 InfluxClient 클래스에서)
        // influx_client_->WritePoint(measurement, tags, fields, timestamp);
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to save to InfluxDB: " + std::string(e.what()));
    }
}

void DeviceWorker::CalculateVirtualPoints() {
    std::lock_guard<std::mutex> lock(virtual_points_mutex_);
    
    for (const auto& pair : virtual_point_rules_) {
        const auto& rule = pair.second;
        
        if (!rule.is_enabled) continue;
        
        try {
            // 입력 값들 수집
            std::map<std::string, double> input_values;
            bool all_inputs_available = true;
            
            {
                std::lock_guard<std::mutex> data_lock(data_points_mutex_);
                for (const auto& input_id : rule.input_point_ids) {
                    auto it = latest_values_.find(input_id);
                    if (it != latest_values_.end()) {
                        input_values["point_" + input_id.to_string()] = it->second.value.to_double();
                    } else {
                        all_inputs_available = false;
                        break;
                    }
                }
            }
            
            if (!all_inputs_available) continue;
            
            // 간단한 수식 계산 (실제로는 더 복잡한 파서 필요)
            double result = 0.0;
            if (rule.formula == "temp + pressure * 0.1" && input_values.size() >= 2) {
                auto it = input_values.begin();
                double temp = it->second;
                ++it;
                double pressure = it->second;
                result = temp + pressure * 0.1;
            }
            
            // 계산된 값 저장
            Drivers::TimestampedValue virtual_value;
            virtual_value.value = Drivers::DataValue(result);
            virtual_value.quality = Drivers::DataQuality::GOOD;
            virtual_value.timestamp = system_clock::now();
            
            {
                std::lock_guard<std::mutex> data_lock(data_points_mutex_);
                latest_values_[rule.virtual_point_id] = virtual_value;
            }
            
            statistics_.virtual_calculations++;
            
            // InfluxDB에 저장
            SaveToInfluxDB(rule.virtual_point_id, virtual_value);
            
        } catch (const std::exception& e) {
            logger_->Error("Failed to calculate virtual point: " + std::string(e.what()));
        }
    }
}

void DeviceWorker::CheckAlarms(const UUID& point_id, const Drivers::TimestampedValue& value) {
    if (!alarm_engine_) return;
    
    try {
        // 알람 엔진에서 처리
        alarm_engine_->CheckAlarms(point_id, value);
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to check alarms: " + std::string(e.what()));
    }
}

bool DeviceWorker::AttemptReconnection() {
    try {
        logger_->Info("Attempting reconnection for device: " + device_info_.device_name);
        
        if (driver_->Disconnect()) {
            std::this_thread::sleep_for(milliseconds(device_info_.reconnect_delay_ms));
            
            if (driver_->Connect()) {
                logger_->Info("Reconnection successful for device: " + device_info_.device_name);
                return true;
            }
        }
        
        logger_->Warn("Reconnection failed for device: " + device_info_.device_name);
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception during reconnection: " + std::string(e.what()));
        return false;
    }
}

void DeviceWorker::PublishCommandResult(const std::string& command, bool success, const std::string& message) {
    if (!redis_client_) return;
    
    try {
        json result;
        result["device_id"] = device_info_.device_id.to_string();
        result["command"] = command;
        result["success"] = success;
        result["message"] = message;
        result["timestamp"] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        
        std::string result_channel = "device_command_results:" + device_info_.device_id.to_string();
        redis_client_->Publish(result_channel, result.dump());
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to publish command result: " + std::string(e.what()));
    }
}

} // namespace Engine
} // namespace PulseOne