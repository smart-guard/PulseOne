// collector/src/Engine/DeviceManager.cpp
// 전체 디바이스 관리자 구현
#include "Engine/DeviceManager.h"
//#include "Engine/VirtualPointEngine.h"
//#include "Engine/AlarmEngine.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Engine {

// Forward declarations
class DeviceWorker;
class VirtualPointEngine;
class AlarmEngine;
class DeviceControlHandler;

// Type aliases for consistency
using UUID = PulseOne::Drivers::UUID;
using DeviceInfo = PulseOne::Drivers::DeviceInfo;
using DataPoint = PulseOne::Drivers::DataPoint;
using DataValue = PulseOne::Drivers::DataValue;
using WriteRequest = PulseOne::Drivers::WriteRequest;

// =============================================================================
// DeviceManager 구현
// =============================================================================

DeviceManager::DeviceManager(const DeviceManagerConfig& config)
    : config_(config)
    , logger_(LogManager::GetInstance())
    , start_time_(std::chrono::system_clock::now()) {
    
    logger_.Info("DeviceManager created with max_devices=" + std::to_string(config_.max_concurrent_devices));
}

DeviceManager::~DeviceManager() {
    Stop();
    logger_.Info("DeviceManager destroyed");
}

// =============================================================================
// 매니저 생명주기 관리
// =============================================================================

bool DeviceManager::Initialize() {
    try {
        logger_.Info("Initializing DeviceManager...");
        
        // Redis 클라이언트 초기화
        redis_client_ = std::make_shared<RedisClientImpl>();
        if (!redis_client_->connect("localhost", 6379)) {
            logger_.Error("Failed to connect to Redis");
            return false;
        }
        logger_.Info("✅ Redis connected");
        
        // 데이터베이스 매니저 초기화
        db_manager_ = std::make_shared<DatabaseManager>();
        if (!db_manager_->Initialize()) {
            logger_.Error("Failed to initialize DatabaseManager");
            return false;
        }
        logger_.Info("✅ Database manager initialized");
        
        // 제어 핸들러 초기화
//        control_handler_ = std::make_unique<DeviceControlHandler>(shared_from_this(), redis_client_);
//        if (!control_handler_->Initialize()) {
//            logger_.Error("Failed to initialize DeviceControlHandler");
//            return false;
//        }
//        logger_.Info("✅ Device control handler initialized");
        
        // 가상 포인트 엔진 초기화
//        virtual_point_engine_ = std::make_unique<VirtualPointEngine>();
//        if (!virtual_point_engine_->Initialize()) {
//            logger_.Warn("Failed to initialize VirtualPointEngine - continuing without virtual points");
//        } else {
//            logger_.Info("✅ Virtual point engine initialized");
//        }
        
        // 알람 엔진 초기화
//        alarm_engine_ = std::make_unique<AlarmEngine>();
//        if (!alarm_engine_->Initialize()) {
//            logger_.Warn("Failed to initialize AlarmEngine - continuing without alarms");
//        } else {
//           logger_.Info("✅ Alarm engine initialized");
//        }
        
        // 통계 초기화
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            system_stats_ = SystemStatistics{};
            system_stats_.last_statistics_time = std::chrono::system_clock::now();
        }
        
        logger_.Info("DeviceManager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in DeviceManager::Initialize: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::Start() {
    if (running_.load()) {
        logger_.Warn("DeviceManager already running");
        return true;
    }
    
    logger_.Info("Starting DeviceManager...");
    
    try {
        // 데이터베이스에서 디바이스 로드
        int loaded_devices = LoadAllDevicesFromDatabase();
        if (loaded_devices < 0) {
            logger_.Error("Failed to load devices from database");
            return false;
        }
        
        logger_.Info("Loaded " + std::to_string(loaded_devices) + " devices from database");
        
        // 실행 플래그 설정
        running_.store(true);
        
        // 하위 서비스들 시작
        if (control_handler_) {
            control_handler_->Start();
        }
        
        if (virtual_point_engine_) {
            virtual_point_engine_->LoadVirtualPointsFromDB();
        }
        
        if (alarm_engine_) {
            alarm_engine_->LoadAlarmRulesFromDB();
        }
        
        // 관리 스레드들 시작
        manager_thread_ = std::thread(&DeviceManager::ManagerThreadFunction, this);
        statistics_thread_ = std::thread(&DeviceManager::StatisticsThreadFunction, this);
        health_monitor_thread_ = std::thread(&DeviceManager::HealthMonitorThreadFunction, this);
        cleanup_thread_ = std::thread(&DeviceManager::CleanupThreadFunction, this);
        
        logger_.Info("DeviceManager started successfully with " + 
                    std::to_string(device_workers_.size()) + " devices");
        
        // 초기 상태를 Redis에 발행
        PublishSystemStatus();
        
        return true;
        
    } catch (const std::exception& e) {
        running_.store(false);
        logger_.Error("Exception while starting DeviceManager: " + std::string(e.what()));
        return false;
    }
}

void DeviceManager::Stop() {
    if (!running_.load()) {
        return;
    }
    
    logger_.Info("Stopping DeviceManager...");
    running_.store(false);
    
    // 하위 서비스들 중지
    if (control_handler_) {
        control_handler_->Stop();
    }
    
    // 관리 스레드들 종료
    SafeStopThread(manager_thread_, "ManagerThread");
    SafeStopThread(statistics_thread_, "StatisticsThread");
    SafeStopThread(health_monitor_thread_, "HealthMonitorThread");
    SafeStopThread(cleanup_thread_, "CleanupThread");
    
    // 모든 디바이스 워커 중지
    {
        std::unique_lock<std::shared_mutex> lock(workers_mutex_);
        logger_.Info("Stopping " + std::to_string(device_workers_.size()) + " device workers...");
        
        for (auto& [device_id, worker] : device_workers_) {
            try {
                worker->Stop(true);  // 강제 정지
            } catch (const std::exception& e) {
                logger_.Warn("Exception while stopping worker " + device_id + ": " + e.what());
            }
        }
        
        device_workers_.clear();
    }
    
    // 재시작 추적 정리
    {
        std::lock_guard<std::mutex> lock(restart_tracking_mutex_);
        restart_attempts_.clear();
        last_restart_time_.clear();
    }
    
    logger_.Info("DeviceManager stopped");
}

// =============================================================================
// 디바이스 관리 (CRUD)
// =============================================================================

int DeviceManager::LoadAllDevicesFromDatabase() {
    try {
        std::vector<DeviceInfo> devices = QueryDevicesFromDB("is_enabled = true");
        
        int loaded_count = 0;
        {
            std::unique_lock<std::shared_mutex> lock(workers_mutex_);
            
            for (const auto& device_info : devices) {
                // 이미 존재하는 디바이스는 스킵
                if (device_workers_.find(device_info.id) != device_workers_.end()) {
                    logger_.Info("Device already loaded: " + device_info.name);
                    continue;
                }
                
                // 데이터 포인트 조회
                std::vector<DataPoint> data_points = QueryDataPointsFromDB(device_info.id);
                
                if (data_points.empty()) {
                    logger_.Warn("No data points found for device: " + device_info.name);
                    continue;
                }
                
                // 워커 생성 및 시작
                if (CreateAndStartWorker(device_info, data_points)) {
                    loaded_count++;
                    logger_.Info("Device loaded: " + device_info.name + " (" + device_info.id + ")");
                } else {
                    logger_.Error("Failed to load device: " + device_info.name);
                }
                
                // 최대 디바이스 수 체크
                if (device_workers_.size() >= static_cast<size_t>(config_.max_concurrent_devices)) {
                    logger_.Warn("Reached maximum concurrent devices limit: " + 
                                   std::to_string(config_.max_concurrent_devices));
                    break;
                }
            }
        }
        
        logger_.Info("Loaded " + std::to_string(loaded_count) + " devices successfully");
        return loaded_count;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in LoadAllDevicesFromDatabase: " + std::string(e.what()));
        return -1;
    }
}

int DeviceManager::LoadDevicesFromProject(const UUID& project_id) {
    try {
        std::string where_clause = "is_enabled = true AND project_id = '" + project_id + "'";
        std::vector<DeviceInfo> devices = QueryDevicesFromDB(where_clause);
        
        int loaded_count = 0;
        {
            std::unique_lock<std::shared_mutex> lock(workers_mutex_);
            
            for (const auto& device_info : devices) {
                if (device_workers_.find(device_info.id) != device_workers_.end()) {
                    continue;  // 이미 로드된 디바이스
                }
                
                std::vector<DataPoint> data_points = QueryDataPointsFromDB(device_info.id);
                
                if (!data_points.empty() && CreateAndStartWorker(device_info, data_points)) {
                    loaded_count++;
                }
            }
        }
        
        return loaded_count;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in LoadDevicesFromProject: " + std::string(e.what()));
        return -1;
    }
}

bool DeviceManager::AddDevice(const DeviceInfo& device_info, const std::vector<DataPoint>& data_points) {
    try {
        // 유효성 검사
        if (!DeviceWorkerFactory::ValidateDeviceInfo(device_info) ||
            !DeviceWorkerFactory::ValidateDataPoints(data_points)) {
            logger_.Error("Invalid device info or data points for device: " + device_info.name);
            return false;
        }
        
        std::unique_lock<std::shared_mutex> lock(workers_mutex_);
        
        // 이미 존재하는 디바이스 체크
        if (device_workers_.find(device_info.id) != device_workers_.end()) {
            logger_.Warn("Device already exists: " + device_info.name);
            return false;
        }
        
        // 최대 디바이스 수 체크
        if (device_workers_.size() >= static_cast<size_t>(config_.max_concurrent_devices)) {
            logger_.Error("Cannot add device - maximum limit reached: " + 
                         std::to_string(config_.max_concurrent_devices));
            return false;
        }
        
        // 워커 생성 및 시작
        return CreateAndStartWorker(device_info, data_points);
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in AddDevice: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::RemoveDevice(const UUID& device_id, bool force_remove) {
    try {
        std::unique_lock<std::shared_mutex> lock(workers_mutex_);
        
        auto it = device_workers_.find(device_id);
        if (it == device_workers_.end()) {
            logger_.Warn("Device not found for removal: " + device_id);
            return false;
        }
        
        std::string device_name = it->second->GetStatus().device_name;
        logger_.Info("Removing device: " + device_name + " (force=" + (force_remove ? "true" : "false") + ")");
        
        // 워커 중지
        bool stop_success = it->second->Stop(force_remove);
        
        if (!stop_success && !force_remove) {
            logger_.Error("Failed to stop device worker for: " + device_name);
            return false;
        }
        
        // 워커 제거
        device_workers_.erase(it);
        
        // 재시작 추적 정리
        {
            std::lock_guard<std::mutex> restart_lock(restart_tracking_mutex_);
            restart_attempts_.erase(device_id);
            last_restart_time_.erase(device_id);
        }
        
        logger_.Info("Device removed successfully: " + device_name);
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in RemoveDevice: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::UpdateDevice(const UUID& device_id, const DeviceInfo& new_device_info) {
    try {
        std::unique_lock<std::shared_mutex> lock(workers_mutex_);
        
        auto it = device_workers_.find(device_id);
        if (it == device_workers_.end()) {
            logger_.Warn("Device not found for update: " + device_id);
            return false;
        }
        
        // 현재 데이터 포인트 백업
        auto current_data_points = QueryDataPointsFromDB(device_id);
        
        // 기존 워커 중지 및 제거
        it->second->Stop(false);
        device_workers_.erase(it);
        
        // 새로운 워커 생성 및 시작
        bool success = CreateAndStartWorker(new_device_info, current_data_points);
        
        if (success) {
            logger_.Info("Device updated successfully: " + new_device_info.name);
        } else {
            logger_.Error("Failed to update device: " + new_device_info.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in UpdateDevice: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::UpdateDeviceDataPoints(const UUID& device_id, const std::vector<DataPoint>& new_data_points) {
    try {
        std::shared_lock<std::shared_mutex> lock(workers_mutex_);
        
        auto it = device_workers_.find(device_id);
        if (it == device_workers_.end()) {
            logger_.Warn("Device not found for data points update: " + device_id);
            return false;
        }
        
        // TODO: DeviceWorker에 데이터 포인트 동적 업데이트 기능 추가
        // 현재는 재시작으로 처리
        
        auto device_status = it->second->GetStatus();
        bool was_running = it->second->IsRunning();
        
        lock.unlock();  // 락 해제
        
        // 디바이스 정보 조회
        auto devices = QueryDevicesFromDB("id = '" + device_id + "'");
        if (devices.empty()) {
            logger_.Error("Device info not found in database: " + device_id);
            return false;
        }
        
        // 재시작으로 데이터 포인트 업데이트
        if (RemoveDevice(device_id, false) && 
            AddDevice(devices[0], new_data_points)) {
            
            if (was_running) {
                StartDevice(device_id);
            }
            
            logger_.Info("Device data points updated successfully: " + device_id);
            return true;
        } else {
            logger_.Error("Failed to update device data points: " + device_id);
            return false;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in UpdateDeviceDataPoints: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 디바이스 제어 (웹에서 호출)
// =============================================================================

bool DeviceManager::StartDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for start: " + device_id);
        return false;
    }
    
    bool success = it->second->Start(false);
    
    if (success) {
        // 재시작 기록 초기화
        ResetRestartAttempts(device_id);
        logger_.Info("Device started: " + it->second->GetStatus().device_name);
    }
    
    return success;
}

bool DeviceManager::StopDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for stop: " + device_id);
        return false;
    }
    
    bool success = it->second->Stop(false);
    
    if (success) {
        logger_.Info("Device stopped: " + it->second->GetStatus().device_name);
    }
    
    return success;
}

bool DeviceManager::PauseDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for pause: " + device_id);
        return false;
    }
    
    bool success = it->second->Pause();
    
    if (success) {
        logger_.Info("Device paused: " + it->second->GetStatus().device_name);
    }
    
    return success;
}

bool DeviceManager::ResumeDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for resume: " + device_id);
        return false;
    }
    
    bool success = it->second->Resume();
    
    if (success) {
        logger_.Info("Device resumed: " + it->second->GetStatus().device_name);
    }
    
    return success;
}

bool DeviceManager::RestartDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for restart: " + device_id);
        return false;
    }
    
    bool success = it->second->Restart();
    
    if (success) {
        RecordRestartAttempt(device_id);
        logger_.Info("Device restarted: " + it->second->GetStatus().device_name);
    }
    
    return success;
}

int DeviceManager::StartAllDevices() {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    int started_count = 0;
    for (auto& [device_id, worker] : device_workers_) {
        try {
            if (worker->Start(false)) {
                started_count++;
                ResetRestartAttempts(device_id);
            }
        } catch (const std::exception& e) {
            logger_.Warn("Exception while starting device " + device_id + ": " + e.what());
        }
    }
    
    logger_.Info("Started " + std::to_string(started_count) + " devices");
    return started_count;
}

int DeviceManager::StopAllDevices() {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    int stopped_count = 0;
    for (auto& [device_id, worker] : device_workers_) {
        try {
            if (worker->Stop(false)) {
                stopped_count++;
            }
        } catch (const std::exception& e) {
            logger_.Warn("Exception while stopping device " + device_id + ": " + e.what());
        }
    }
    
    logger_.Info("Stopped " + std::to_string(stopped_count) + " devices");
    return stopped_count;
}

int DeviceManager::ControlProjectDevices(const UUID& project_id, const std::string& command) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    int processed_count = 0;
    
    // 프로젝트에 속한 디바이스들 찾기
    for (auto& [device_id, worker] : device_workers_) {
        try {
            // TODO: DeviceWorker에서 project_id 조회 기능 추가
            // 임시로 데이터베이스에서 직접 조회
            auto devices = QueryDevicesFromDB("id = '" + device_id + "' AND project_id = '" + project_id + "'");
            
            if (!devices.empty()) {
                bool success = false;
                
                if (command == "start") {
                    success = worker->Start(false);
                } else if (command == "stop") {
                    success = worker->Stop(false);
                } else if (command == "pause") {
                    success = worker->Pause();
                } else if (command == "resume") {
                    success = worker->Resume();
                } else if (command == "restart") {
                    success = worker->Restart();
                }
                
                if (success) {
                    processed_count++;
                }
            }
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception while controlling device " + device_id + ": " + e.what());
        }
    }
    
    logger_.Info("Controlled " + std::to_string(processed_count) + " devices in project " + project_id);
    return processed_count;
}

// =============================================================================
// 데이터 처리
// =============================================================================

bool DeviceManager::WriteToDevice(const UUID& device_id, const WriteRequest& request) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for write: " + device_id);
        return false;
    }
    
    it->second->AddWriteRequest(request);
    return true;
}

std::vector<DataValue> DeviceManager::ReadFromDevice(const UUID& device_id) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for read: " + device_id);
        return {};
    }
    
    return it->second->ReadDataNow();
}

bool DeviceManager::WriteToPoint(const UUID& device_id, const UUID& point_id, double value) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        logger_.Warn("Device not found for point write: " + device_id);
        return false;
    }
    
    return it->second->WriteDataNow(point_id, value);
}

// =============================================================================
// 상태 조회 및 모니터링
// =============================================================================

std::vector<DeviceStatus> DeviceManager::GetAllDeviceStatus() const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    std::vector<DeviceStatus> all_status;
    all_status.reserve(device_workers_.size());
    
    for (const auto& [device_id, worker] : device_workers_) {
        try {
            all_status.push_back(worker->GetStatus());
        } catch (const std::exception& e) {
            logger_.Warn("Exception while getting status for device " + device_id + ": " + e.what());
        }
    }
    
    return all_status;
}

std::optional<DeviceStatus> DeviceManager::GetDeviceStatus(const UUID& device_id) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        return std::nullopt;
    }
    
    try {
        return it->second->GetStatus();
    } catch (const std::exception& e) {
        logger_.Warn("Exception while getting status for device " + device_id + ": " + e.what());
        return std::nullopt;
    }
}

SystemStatistics DeviceManager::GetSystemStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return system_stats_;
}

std::optional<DeviceStatistics> DeviceManager::GetDeviceStatistics(const UUID& device_id) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    if (it == device_workers_.end()) {
        return std::nullopt;
    }
    
    try {
        return it->second->GetStatistics();
    } catch (const std::exception& e) {
        logger_.Warn("Exception while getting statistics for device " + device_id + ": " + e.what());
        return std::nullopt;
    }
}

SystemStatistics DeviceManager::GetProjectStatistics(const UUID& project_id) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    SystemStatistics project_stats;
    project_stats.last_statistics_time = std::chrono::system_clock::now();
    
    // 프로젝트에 속한 디바이스들의 통계 집계
    for (const auto& [device_id, worker] : device_workers_) {
        try {
            // TODO: 프로젝트 ID 조회 최적화
            auto devices = QueryDevicesFromDB("id = '" + device_id + "' AND project_id = '" + project_id + "'");
            
            if (!devices.empty()) {
                auto status = worker->GetStatus();
                auto stats = worker->GetStatistics();
                
                project_stats.total_devices++;
                
                switch (status.worker_state) {
                    case DeviceWorkerState::RUNNING:
                        project_stats.running_devices++;
                        if (status.connection_status == ConnectionStatus::CONNECTED) {
                            project_stats.connected_devices++;
                        }
                        break;
                    case DeviceWorkerState::PAUSED:
                        project_stats.paused_devices++;
                        break;
                    case DeviceWorkerState::ERROR:
                        project_stats.error_devices++;
                        break;
                    default:
                        break;
                }
                
                project_stats.total_read_count += stats.total_reads;
                project_stats.total_write_count += stats.total_writes;
                project_stats.total_error_count += stats.total_errors;
            }
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception while calculating project stats for device " + device_id + ": " + e.what());
        }
    }
    
    // 성공률 계산
    if (project_stats.total_read_count > 0) {
        project_stats.overall_success_rate = 1.0 - 
            (static_cast<double>(project_stats.total_error_count) / project_stats.total_read_count);
    }
    
    return project_stats;
}

std::vector<UUID> DeviceManager::GetActiveDeviceIds() const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    std::vector<UUID> active_devices;
    
    for (const auto& [device_id, worker] : device_workers_) {
        if (worker->IsRunning() && !worker->IsPaused()) {
            active_devices.push_back(device_id);
        }
    }
    
    return active_devices;
}

std::vector<UUID> DeviceManager::GetErrorDeviceIds() const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    std::vector<UUID> error_devices;
    
    for (const auto& [device_id, worker] : device_workers_) {
        if (worker->GetWorkerState() == DeviceWorkerState::ERROR) {
            error_devices.push_back(device_id);
        }
    }
    
    return error_devices;
}

nlohmann::json DeviceManager::GetPerformanceMetrics() const {
    auto system_stats = GetSystemStatistics();
    auto all_status = GetAllDeviceStatus();
    
    nlohmann::json metrics = {
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        
        {"system", {
            {"total_devices", system_stats.total_devices},
            {"connected_devices", system_stats.connected_devices},
            {"running_devices", system_stats.running_devices},
            {"paused_devices", system_stats.paused_devices},
            {"error_devices", system_stats.error_devices},
            {"uptime_seconds", system_stats.uptime_seconds},
            {"memory_usage_mb", system_stats.memory_usage_mb},
            {"cpu_usage_percent", system_stats.cpu_usage_percent}
        }},
        
        {"performance", {
            {"total_reads", system_stats.total_read_count},
            {"total_writes", system_stats.total_write_count},
            {"total_errors", system_stats.total_error_count},
            {"success_rate", system_stats.overall_success_rate},
            {"avg_response_time_ms", system_stats.avg_response_time_ms}
        }},
        
        {"devices", nlohmann::json::array()}
    };
    
    // 디바이스별 상세 메트릭
    for (const auto& status : all_status) {
        metrics["devices"].push_back({
            {"device_id", status.device_id},
            {"device_name", status.device_name},
            {"state", static_cast<int>(status.worker_state)},
            {"connection_status", static_cast<int>(status.connection_status)},
            {"read_count", status.read_count},
            {"error_count", status.error_count},
            {"response_time_ms", status.avg_response_time_ms}
        });
    }
    
    return metrics;
}

// =============================================================================
// 설정 및 구성
// =============================================================================

void DeviceManager::UpdateConfig(const DeviceManagerConfig& new_config) {
    config_ = new_config;
    logger_.Info("DeviceManager configuration updated");
    
    // 설정 변경사항을 Redis에 알림
    if (redis_client_) {
        nlohmann::json config_json = {
            {"max_concurrent_devices", config_.max_concurrent_devices},
            {"statistics_interval_seconds", config_.statistics_interval_seconds},
            {"auto_restart_failed_devices", config_.auto_restart_failed_devices},
            {"updated_at", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        
        redis_client_->publish("device_manager_config_updated", config_json.dump());
    }
}

bool DeviceManager::UpdateVirtualPointConfiguration() {
    if (!virtual_point_engine_) {
        logger_.Warn("Virtual point engine not available");
        return false;
    }
    
    try {
        return virtual_point_engine_->LoadVirtualPointsFromDB();
    } catch (const std::exception& e) {
        logger_.Error("Exception while updating virtual point configuration: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::UpdateAlarmConfiguration() {
    if (!alarm_engine_) {
        logger_.Warn("Alarm engine not available");
        return false;
    }
    
    try {
        return alarm_engine_->LoadAlarmRulesFromDB();
    } catch (const std::exception& e) {
        logger_.Error("Exception while updating alarm configuration: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 고급 기능
// =============================================================================

int DeviceManager::ControlDeviceGroup(const UUID& group_id, const std::string& command) {
    try {
        // 그룹에 속한 디바이스들 조회
        std::string where_clause = "device_group_id = '" + group_id + "'";
        std::vector<DeviceInfo> group_devices = QueryDevicesFromDB(where_clause);
        
        int processed_count = 0;
        
        std::shared_lock<std::shared_mutex> lock(workers_mutex_);
        
        for (const auto& device_info : group_devices) {
            auto it = device_workers_.find(device_info.id);
            if (it != device_workers_.end()) {
                bool success = false;
                
                if (command == "start") {
                    success = it->second->Start(false);
                } else if (command == "stop") {
                    success = it->second->Stop(false);
                } else if (command == "pause") {
                    success = it->second->Pause();
                } else if (command == "resume") {
                    success = it->second->Resume();
                } else if (command == "restart") {
                    success = it->second->Restart();
                }
                
                if (success) {
                    processed_count++;
                }
            }
        }
        
        logger_.Info("Controlled " + std::to_string(processed_count) + " devices in group " + group_id);
        return processed_count;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in ControlDeviceGroup: " + std::string(e.what()));
        return 0;
    }
}

int DeviceManager::ControlDevicesWhere(std::function<bool(const DeviceStatus&)> condition, 
                                      const std::string& command) {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    int processed_count = 0;
    
    for (auto& [device_id, worker] : device_workers_) {
        try {
            auto status = worker->GetStatus();
            
            if (condition(status)) {
                bool success = false;
                
                if (command == "start") {
                    success = worker->Start(false);
                } else if (command == "stop") {
                    success = worker->Stop(false);
                } else if (command == "pause") {
                    success = worker->Pause();
                } else if (command == "resume") {
                    success = worker->Resume();
                } else if (command == "restart") {
                    success = worker->Restart();
                }
                
                if (success) {
                    processed_count++;
                }
            }
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception while controlling device " + device_id + ": " + e.what());
        }
    }
    
    return processed_count;
}

std::string DeviceManager::OptimizeSystemResources() {
    std::ostringstream result;
    result << "System Resource Optimization Report:\n";
    
    try {
        // 메모리 정리
        int cleanup_count = 0;
        {
            std::unique_lock<std::shared_mutex> lock(workers_mutex_);
            
            auto it = device_workers_.begin();
            while (it != device_workers_.end()) {
                if (it->second->GetWorkerState() == DeviceWorkerState::ERROR) {
                    auto error_devices = GetErrorDeviceIds();
                    if (error_devices.size() > 5) {  // 에러 디바이스가 많으면 정리
                        result << "- Cleaned up error device: " << it->first << "\n";
                        it->second->Stop(true);
                        it = device_workers_.erase(it);
                        cleanup_count++;
                        continue;
                    }
                }
                ++it;
            }
        }
        
        // 재시작 기록 정리
        {
            std::lock_guard<std::mutex> lock(restart_tracking_mutex_);
            auto now = std::chrono::system_clock::now();
            
            auto it = last_restart_time_.begin();
            while (it != last_restart_time_.end()) {
                auto time_diff = std::chrono::duration_cast<std::chrono::hours>(now - it->second);
                if (time_diff.count() > 24) {  // 24시간 이상 된 기록 삭제
                    restart_attempts_.erase(it->first);
                    it = last_restart_time_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        result << "- Cleaned up " << cleanup_count << " inactive device workers\n";
        result << "- Reset old restart attempt records\n";
        result << "- Current memory usage: " << system_stats_.memory_usage_mb << " MB\n";
        result << "- Active devices: " << GetActiveDeviceIds().size() << "\n";
        
    } catch (const std::exception& e) {
        result << "- Optimization failed: " << e.what() << "\n";
    }
    
    std::string result_str = result.str();
    logger_.Info("Resource optimization completed: " + result_str);
    
    return result_str;
}

bool DeviceManager::BackupConfiguration(const std::string& backup_path) {
    try {
        nlohmann::json backup_data;
        
        // 시스템 정보
        backup_data["backup_info"] = {
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"version", "1.0.0"},
            {"device_count", device_workers_.size()}
        };
        
        // 매니저 설정
        backup_data["manager_config"] = {
            {"max_concurrent_devices", config_.max_concurrent_devices},
            {"statistics_interval_seconds", config_.statistics_interval_seconds},
            {"auto_restart_failed_devices", config_.auto_restart_failed_devices}
        };
        
        // 현재 실행 중인 디바이스 상태
        backup_data["device_states"] = nlohmann::json::array();
        {
            std::shared_lock<std::shared_mutex> lock(workers_mutex_);
            for (const auto& [device_id, worker] : device_workers_) {
                auto status = worker->GetStatus();
                backup_data["device_states"].push_back({
                    {"device_id", device_id},
                    {"device_name", status.device_name},
                    {"worker_state", static_cast<int>(status.worker_state)},
                    {"is_running", status.is_running},
                    {"is_paused", status.is_paused}
                });
            }
        }
        
        // 파일에 저장
        std::ofstream backup_file(backup_path);
        if (!backup_file.is_open()) {
            logger_.Error("Failed to create backup file: " + backup_path);
            return false;
        }
        
        backup_file << backup_data.dump(4);
        backup_file.close();
        
        logger_.Info("Configuration backup created: " + backup_path);
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in BackupConfiguration: " + std::string(e.what()));
        return false;
    }
}

bool DeviceManager::RestoreConfiguration(const std::string& backup_path) {
    try {
        std::ifstream backup_file(backup_path);
        if (!backup_file.is_open()) {
            logger_.Error("Failed to open backup file: " + backup_path);
            return false;
        }
        
        nlohmann::json backup_data;
        backup_file >> backup_data;
        backup_file.close();
        
        // 설정 복원
        if (backup_data.contains("manager_config")) {
            DeviceManagerConfig restored_config;
            auto config_json = backup_data["manager_config"];
            
            if (config_json.contains("max_concurrent_devices")) {
                restored_config.max_concurrent_devices = config_json["max_concurrent_devices"];
            }
            if (config_json.contains("statistics_interval_seconds")) {
                restored_config.statistics_interval_seconds = config_json["statistics_interval_seconds"];
            }
            if (config_json.contains("auto_restart_failed_devices")) {
                restored_config.auto_restart_failed_devices = config_json["auto_restart_failed_devices"];
            }
            
            UpdateConfig(restored_config);
        }
        
        // 디바이스 상태 복원
        if (backup_data.contains("device_states")) {
            for (const auto& device_state : backup_data["device_states"]) {
                std::string device_id = device_state["device_id"];
                bool was_running = device_state["is_running"];
                bool was_paused = device_state["is_paused"];
                
                if (DeviceExists(device_id)) {
                    if (was_running) {
                        StartDevice(device_id);
                        if (was_paused) {
                            PauseDevice(device_id);
                        }
                    }
                }
            }
        }
        
        logger_.Info("Configuration restored from: " + backup_path);
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in RestoreConfiguration: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 이벤트 및 콜백
// =============================================================================

void DeviceManager::RegisterEventListener(std::shared_ptr<IDeviceWorkerEventListener> listener) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    event_listeners_.push_back(listener);
    logger_.Info("Event listener registered");
}

void DeviceManager::UnregisterEventListener(std::shared_ptr<IDeviceWorkerEventListener> listener) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    event_listeners_.erase(
        std::remove_if(event_listeners_.begin(), event_listeners_.end(),
                      [&listener](const std::weak_ptr<IDeviceWorkerEventListener>& weak_listener) {
                          return weak_listener.expired() || weak_listener.lock() == listener;
                      }),
        event_listeners_.end()
    );
    
    logger_.Info("Event listener unregistered");
}

// =============================================================================
// 스레드 함수들
// =============================================================================

void DeviceManager::ManagerThreadFunction() {
    logger_.Info("DeviceManager main thread started");
    
    while (running_.load()) {
        try {
            // 디바이스 상태 모니터링
            MonitorDeviceHealth();
            
            // 자동 재시작 처리
            if (config_.auto_restart_failed_devices) {
                AutoRestartFailedDevices();
            }
            
            // 시스템 상태 발행
            PublishSystemStatus();
            
            // 10초마다 실행
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in manager thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    logger_.Info("DeviceManager main thread stopped");
}

void DeviceManager::StatisticsThreadFunction() {
    logger_.Info("DeviceManager statistics thread started");
    
    while (running_.load()) {
        try {
            UpdateStatistics();
            MonitorSystemResources();
            
            std::this_thread::sleep_for(std::chrono::seconds(config_.statistics_interval_seconds));
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in statistics thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    logger_.Info("DeviceManager statistics thread stopped");
}

void DeviceManager::HealthMonitorThreadFunction() {
    logger_.Info("DeviceManager health monitor thread started");
    
    while (running_.load()) {
        try {
            MonitorDeviceHealth();
            
            std::this_thread::sleep_for(std::chrono::seconds(config_.health_check_interval_seconds));
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in health monitor thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    
    logger_.Info("DeviceManager health monitor thread stopped");
}

void DeviceManager::CleanupThreadFunction() {
    logger_.Info("DeviceManager cleanup thread started");
    
    while (running_.load()) {
        try {
            CleanupInactiveDevices();
            
            std::this_thread::sleep_for(std::chrono::seconds(config_.cleanup_interval_seconds));
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in cleanup thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }
    
    logger_.Info("DeviceManager cleanup thread stopped");
}

// =============================================================================
// 내부 로직 메서드들
// =============================================================================

std::vector<DeviceInfo> DeviceManager::QueryDevicesFromDB(const std::string& where_clause) {
    std::vector<DeviceInfo> devices;
    
    try {
        // TODO: 실제 데이터베이스 쿼리 구현
        // 지금은 샘플 데이터 생성
        
        std::string query = "SELECT * FROM devices";
        if (!where_clause.empty()) {
            query += " WHERE " + where_clause;
        }
        
        // 샘플 데이터 (실제로는 데이터베이스에서 조회)
        DeviceInfo device1;
        device1.id = "device-001";
        device1.name = "PLC-001";
        device1.description = "Main Production Line PLC";
        device1.protocol = ProtocolType::MODBUS_TCP;
        device1.endpoint = "192.168.1.100:502";
        device1.config_json = R"({
            "host": "192.168.1.100",
            "port": 502,
            "unit_id": 1,
            "connection_timeout": 3000
        })";
        device1.polling_interval_ms = 1000;
        device1.is_enabled = true;
        
        devices.push_back(device1);
        
        DeviceInfo device2;
        device2.id = "device-002";
        device2.name = "MQTT-Sensor-001";
        device2.description = "Temperature Sensors";
        device2.protocol = ProtocolType::MQTT;
        device2.endpoint = "mqtt://192.168.1.200:1883";
        device2.config_json = R"({
            "broker": "192.168.1.200",
            "port": 1883,
            "topics": ["sensors/temperature", "sensors/humidity"]
        })";
        device2.polling_interval_ms = 5000;
        device2.is_enabled = true;
        
        devices.push_back(device2);
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in QueryDevicesFromDB: " + std::string(e.what()));
    }
    
    return devices;
}

std::vector<DataPoint> DeviceManager::QueryDataPointsFromDB(const UUID& device_id) {
    std::vector<DataPoint> data_points;
    
    try {
        // TODO: 실제 데이터베이스 쿼리 구현
        // 지금은 샘플 데이터 생성
        
        if (device_id == "device-001") {  // Modbus PLC
            DataPoint point1;
            point1.id = "point-001";
            point1.device_id = device_id;
            point1.name = "Temperature_1";
            point1.address = 40001;
            point1.data_type = DataType::FLOAT32;
            point1.unit = "°C";
            point1.min_value = -50.0;
            point1.max_value = 150.0;
            point1.is_enabled = true;
            
            DataPoint point2;
            point2.id = "point-002";
            point2.device_id = device_id;
            point2.name = "Pressure_1";
            point2.address = 40002;
            point2.data_type = DataType::FLOAT32;
            point2.unit = "bar";
            point2.min_value = 0.0;
            point2.max_value = 10.0;
            point2.is_enabled = true;
            
            data_points.push_back(point1);
            data_points.push_back(point2);
        }
        else if (device_id == "device-002") {  // MQTT Sensors
            DataPoint point1;
            point1.id = "point-003";
            point1.device_id = device_id;
            point1.name = "Room_Temperature";
            point1.address_string = "sensors/temperature";
            point1.data_type = DataType::FLOAT32;
            point1.unit = "°C";
            point1.min_value = 0.0;
            point1.max_value = 50.0;
            point1.is_enabled = true;
            
            DataPoint point2;
            point2.id = "point-004";
            point2.device_id = device_id;
            point2.name = "Room_Humidity";
            point2.address_string = "sensors/humidity";
            point2.data_type = DataType::FLOAT32;
            point2.unit = "%";
            point2.min_value = 0.0;
            point2.max_value = 100.0;
            point2.is_enabled = true;
            
            data_points.push_back(point1);
            data_points.push_back(point2);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in QueryDataPointsFromDB: " + std::string(e.what()));
    }
    
    return data_points;
}

bool DeviceManager::CreateAndStartWorker(const DeviceInfo& device_info, 
                                        const std::vector<DataPoint>& data_points) {
    try {
        // DeviceWorker 생성
        auto worker = DeviceWorkerFactory::CreateWorker(
            device_info, data_points, redis_client_, db_manager_
        );
        
        if (!worker) {
            logger_.Error("Failed to create worker for device: " + device_info.name);
            return false;
        }
        
        // 워커 시작
        if (!worker->Start(false)) {
            logger_.Error("Failed to start worker for device: " + device_info.name);
            return false;
        }
        
        // 맵에 추가
        device_workers_[device_info.id] = std::move(worker);
        
        logger_.Info("Worker created and started for device: " + device_info.name);
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Exception in CreateAndStartWorker: " + std::string(e.what()));
        return false;
    }
}

void DeviceManager::CleanupInactiveDevices() {
    std::unique_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.begin();
    while (it != device_workers_.end()) {
        try {
            auto status = it->second->GetStatus();
            
            // 장시간 에러 상태인 디바이스 제거 조건
            if (status.worker_state == DeviceWorkerState::ERROR) {
                auto now = std::chrono::system_clock::now();
                auto error_duration = std::chrono::duration_cast<std::chrono::minutes>(
                    now - status.last_read_time
                );
                
                // 30분 이상 에러 상태이고 자동 재시작도 실패한 경우
                if (error_duration.count() > 30 && 
                    !CanAttemptRestart(it->first)) {
                    
                    logger_.Warn("Cleaning up long-term error device: " + status.device_name);
                    it->second->Stop(true);
                    it = device_workers_.erase(it);
                    continue;
                }
            }
            
            ++it;
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception while cleaning up device " + it->first + ": " + e.what());
            ++it;
        }
    }
}

void DeviceManager::MonitorDeviceHealth() {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    for (const auto& [device_id, worker] : device_workers_) {
        try {
            auto status = worker->GetStatus();
            
            // 연결 상태 체크
            if (status.connection_status == ConnectionStatus::ERROR) {
                logger_.Warn("Device in error state: " + status.device_name);
                NotifyDeviceError(device_id, "Device connection error", ErrorSeverity::HIGH);
            }
            
            // 응답 시간 체크
            auto now = std::chrono::system_clock::now();
            auto last_read_duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - status.last_read_time
            );
            
            // 폴링 간격의 5배 이상 응답이 없으면 경고
            int expected_interval = 1000;  // TODO: 실제 폴링 간격 사용
            if (last_read_duration.count() > (expected_interval * 5 / 1000)) {
                logger_.Warn("Device not responding: " + status.device_name + 
                               ", Last read: " + std::to_string(last_read_duration.count()) + "s ago");
            }
            
            // 에러율 체크
            if (status.read_count > 100) {  // 충분한 샘플이 있을 때만
                double error_rate = static_cast<double>(status.error_count) / status.read_count;
                if (error_rate > 0.1) {  // 10% 이상 에러율
                    logger_.Warn("High error rate for device: " + status.device_name + 
                                   " (" + std::to_string(error_rate * 100) + "%)");
                }
            }
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception while monitoring device " + device_id + ": " + e.what());
        }
    }
}

void DeviceManager::UpdateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    system_stats_.total_devices = 0;
    system_stats_.connected_devices = 0;
    system_stats_.running_devices = 0;
    system_stats_.paused_devices = 0;
    system_stats_.error_devices = 0;
    
    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_errors = 0;
    uint64_t total_response_time = 0;
    uint32_t response_time_count = 0;
    
    {
        std::shared_lock<std::shared_mutex> workers_lock(workers_mutex_);
        
        for (const auto& [device_id, worker] : device_workers_) {
            try {
                auto status = worker->GetStatus();
                auto stats = worker->GetStatistics();
                
                system_stats_.total_devices++;
                
                // 상태별 집계
                switch (status.worker_state) {
                    case DeviceWorkerState::RUNNING:
                        system_stats_.running_devices++;
                        if (status.connection_status == ConnectionStatus::CONNECTED) {
                            system_stats_.connected_devices++;
                        }
                        break;
                    case DeviceWorkerState::PAUSED:
                        system_stats_.paused_devices++;
                        break;
                    case DeviceWorkerState::ERROR:
                        system_stats_.error_devices++;
                        break;
                    default:
                        break;
                }
                
                // 통계 집계
                total_reads += stats.total_reads;
                total_writes += stats.total_writes;
                total_errors += stats.total_errors;
                
                if (stats.avg_response_time_ms > 0) {
                    total_response_time += stats.avg_response_time_ms;
                    response_time_count++;
                }
                
            } catch (const std::exception& e) {
                logger_.Warn("Exception while updating statistics for device " + device_id + ": " + e.what());
            }
        }
    }
    
    system_stats_.total_read_count = total_reads;
    system_stats_.total_write_count = total_writes;
    system_stats_.total_error_count = total_errors;
    
    // 평균 응답 시간 계산
    if (response_time_count > 0) {
        system_stats_.avg_response_time_ms = static_cast<uint32_t>(total_response_time / response_time_count);
    }
    
    // 전체 성공률 계산
    if (total_reads > 0) {
        system_stats_.overall_success_rate = 1.0 - (static_cast<double>(total_errors) / total_reads);
    }
    
    // 가동 시간 계산
    auto now = std::chrono::system_clock::now();
    auto uptime_duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    system_stats_.uptime_seconds = uptime_duration.count();
    
    system_stats_.last_statistics_time = now;
    
    // 전역 카운터 업데이트
    total_read_count_.store(total_reads);
    total_write_count_.store(total_writes);
    total_error_count_.store(total_errors);
}

void DeviceManager::MonitorSystemResources() {
    try {
        // 메모리 사용량 모니터링 (Linux 기준)
        if (config_.enable_memory_monitoring) {
            std::ifstream status_file("/proc/self/status");
            std::string line;
            
            while (std::getline(status_file, line)) {
                if (line.substr(0, 6) == "VmRSS:") {
                    std::istringstream iss(line.substr(6));
                    uint32_t memory_kb;
                    iss >> memory_kb;
                    
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    system_stats_.memory_usage_mb = memory_kb / 1024;
                    break;
                }
            }
        }
        
        // CPU 사용률 모니터링 (간단한 구현)
        if (config_.enable_performance_monitoring) {
            // TODO: 더 정확한 CPU 사용률 측정 구현
            std::lock_guard<std::mutex> lock(stats_mutex_);
            system_stats_.cpu_usage_percent = 0.0;  // 일시적으로 0으로 설정
        }
        
    } catch (const std::exception& e) {
        logger_.Warn("Exception while monitoring system resources: " + std::string(e.what()));
    }
}

void DeviceManager::AutoRestartFailedDevices() {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    for (auto& [device_id, worker] : device_workers_) {
        try {
            if (worker->GetWorkerState() == DeviceWorkerState::ERROR) {
                if (CanAttemptRestart(device_id)) {
                    logger_.Info("Attempting auto-restart for failed device: " + device_id);
                    
                    if (worker->Restart()) {
                        RecordRestartAttempt(device_id);
                        logger_.Info("Auto-restart successful for device: " + device_id);
                    } else {
                        logger_.Warn("Auto-restart failed for device: " + device_id);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            logger_.Warn("Exception during auto-restart for device " + device_id + ": " + e.what());
        }
    }
}

void DeviceManager::PublishSystemStatus() {
    if (!redis_client_) return;
    
    try {
        auto system_stats = GetSystemStatistics();
        
        nlohmann::json status_json = {
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"system_statistics", {
                {"total_devices", system_stats.total_devices},
                {"connected_devices", system_stats.connected_devices},
                {"running_devices", system_stats.running_devices},
                {"paused_devices", system_stats.paused_devices},
                {"error_devices", system_stats.error_devices},
                {"total_read_count", system_stats.total_read_count},
                {"total_write_count", system_stats.total_write_count},
                {"total_error_count", system_stats.total_error_count},
                {"overall_success_rate", system_stats.overall_success_rate},
                {"avg_response_time_ms", system_stats.avg_response_time_ms},
                {"uptime_seconds", system_stats.uptime_seconds},
                {"memory_usage_mb", system_stats.memory_usage_mb},
                {"cpu_usage_percent", system_stats.cpu_usage_percent}
            }},
            {"manager_info", {
                {"is_running", running_.load()},
                {"config", {
                    {"max_concurrent_devices", config_.max_concurrent_devices},
                    {"auto_restart_failed_devices", config_.auto_restart_failed_devices}
                }}
            }}
        };
        
        redis_client_->set("system_status", status_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to publish system status: " + std::string(e.what()));
    }
}

bool DeviceManager::CanAttemptRestart(const UUID& device_id) {
    std::lock_guard<std::mutex> lock(restart_tracking_mutex_);
    
    auto attempts_it = restart_attempts_.find(device_id);
    if (attempts_it == restart_attempts_.end()) {
        return true;  // 처음 시도
    }
    
    if (attempts_it->second >= config_.max_restart_attempts) {
        return false;  // 최대 시도 횟수 초과
    }
    
    auto last_restart_it = last_restart_time_.find(device_id);
    if (last_restart_it != last_restart_time_.end()) {
        auto now = std::chrono::system_clock::now();
        auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_restart_it->second
        );
        
        // 최소 지연 시간 체크
        if (time_since_last.count() < config_.restart_delay_seconds) {
            return false;
        }
    }
    
    return true;
}

void DeviceManager::RecordRestartAttempt(const UUID& device_id) {
    std::lock_guard<std::mutex> lock(restart_tracking_mutex_);
    
    restart_attempts_[device_id]++;
    last_restart_time_[device_id] = std::chrono::system_clock::now();
}

void DeviceManager::ResetRestartAttempts(const UUID& device_id) {
    std::lock_guard<std::mutex> lock(restart_tracking_mutex_);
    
    restart_attempts_.erase(device_id);
    last_restart_time_.erase(device_id);
}

bool DeviceManager::DeviceExists(const UUID& device_id) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    return device_workers_.find(device_id) != device_workers_.end();
}

DeviceWorker* DeviceManager::FindWorker(const UUID& device_id) const {
    std::shared_lock<std::shared_mutex> lock(workers_mutex_);
    
    auto it = device_workers_.find(device_id);
    return (it != device_workers_.end()) ? it->second.get() : nullptr;
}

void DeviceManager::SafeStopThread(std::thread& thread, const std::string& thread_name) {
    if (thread.joinable()) {
        try {
            auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            
            while (thread.joinable() && std::chrono::steady_clock::now() < timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            if (thread.joinable()) {
                logger_.Warn("Force detaching " + thread_name + " thread");
                thread.detach();
            }
            
        } catch (const std::exception& e) {
            logger_.Error("Exception while stopping " + thread_name + " thread: " + e.what());
        }
    }
}

void DeviceManager::NotifyDeviceStateChanged(const UUID& device_id, 
                                            DeviceWorkerState old_state, 
                                            DeviceWorkerState new_state) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    // 만료된 리스너들 정리
    event_listeners_.erase(
        std::remove_if(event_listeners_.begin(), event_listeners_.end(),
                      [](const std::weak_ptr<IDeviceWorkerEventListener>& weak_listener) {
                          return weak_listener.expired();
                      }),
        event_listeners_.end()
    );
    
    // 활성 리스너들에게 알림
    for (auto& weak_listener : event_listeners_) {
        if (auto listener = weak_listener.lock()) {
            try {
                listener->OnWorkerStateChanged(device_id, old_state, new_state);
            } catch (const std::exception& e) {
                logger_.Warn("Exception in event listener: " + std::string(e.what()));
            }
        }
    }
}

void DeviceManager::NotifyDeviceError(const UUID& device_id, 
                                     const std::string& error_message, 
                                     ErrorSeverity severity) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    for (auto& weak_listener : event_listeners_) {
        if (auto listener = weak_listener.lock()) {
            try {
                listener->OnError(device_id, error_message, severity);
            } catch (const std::exception& e) {
                logger_.Warn("Exception in error event listener: " + std::string(e.what()));
            }
        }
    }
}

// =============================================================================
// DeviceManagerSingleton 구현
// =============================================================================

std::unique_ptr<DeviceManager> DeviceManagerSingleton::instance_ = nullptr;
std::mutex DeviceManagerSingleton::instance_mutex_;

DeviceManager& DeviceManagerSingleton::CreateInstance(const DeviceManagerConfig& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (instance_) {
        throw std::runtime_error("DeviceManager instance already exists");
    }
    
    instance_ = std::make_unique<DeviceManager>(config);
    return *instance_;
}

DeviceManager& DeviceManagerSingleton::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (!instance_) {
        throw std::runtime_error("DeviceManager instance not created");
    }
    
    return *instance_;
}

void DeviceManagerSingleton::DestroyInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (instance_) {
        instance_->Stop();
        instance_.reset();
    }
}

bool DeviceManagerSingleton::HasInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    return instance_ != nullptr;
}

} // namespace Engine
} // namespace PulseOne