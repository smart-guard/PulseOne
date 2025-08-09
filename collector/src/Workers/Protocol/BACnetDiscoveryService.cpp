/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 - 🔥 동적 Worker 생성 완전 구현
 * @author PulseOne Development Team
 * @date 2025-08-09
 * @version 6.0.0 - 동적 확장
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Workers/Protocol/BACnetWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"  // 🔥 추가
#include "Database/DatabaseTypes.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <functional>
#include <regex>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono;

// 타입 별칭들
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataValue = PulseOne::Structs::DataValue;
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;
using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;
using DataType = PulseOne::Enums::DataType;
using DataQuality = PulseOne::Enums::DataQuality;
using QueryCondition = PulseOne::Database::QueryCondition;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

    // =======================================================================
    // 🔥 DeviceInfo ↔ DeviceEntity 변환 함수들
    // =======================================================================
    
    void ConvertDeviceInfoToEntity(const DeviceInfo& device_info, Database::Entities::DeviceEntity& entity);
    Database::Entities::DeviceEntity ConvertDeviceInfoToEntity(const DeviceInfo& device_info);
    
    // =============================================================================
// 🔥 DeviceInfo ↔ DeviceEntity 변환 구현
// =============================================================================

void BACnetDiscoveryService::ConvertDeviceInfoToEntity(const DeviceInfo& device_info, Database::Entities::DeviceEntity& entity) {
    try {
        entity.setId(std::stoi(device_info.id));
        entity.setName(device_info.name);
        entity.setDescription(device_info.description);
        entity.setProtocolType(device_info.protocol_type);
        entity.setEndpoint(device_info.endpoint);
        entity.setEnabled(device_info.is_enabled);
        
        // properties를 JSON으로 변환
        std::stringstream properties_json;
        properties_json << "{";
        bool first = true;
        for (const auto& [key, value] : device_info.properties) {
            if (!first) properties_json << ",";
            properties_json << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        properties_json << "}";
        entity.setConfig(properties_json.str());
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Failed to convert DeviceInfo to DeviceEntity: " + std::string(e.what()));
        throw;
    }
}

Database::Entities::DeviceEntity BACnetDiscoveryService::ConvertDeviceInfoToEntity(const DeviceInfo& device_info) {
    Database::Entities::DeviceEntity entity;
    ConvertDeviceInfoToEntity(device_info, entity);
    return entity;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDiscoveryService::BACnetDiscoveryService(
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo,
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo,
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo,
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo,  // 🔥 추가
    std::shared_ptr<WorkerFactory> worker_factory)
    : device_repository_(device_repo)
    , datapoint_repository_(datapoint_repo)
    , current_value_repository_(current_value_repo)
    , device_settings_repository_(device_settings_repo)  // 🔥 추가
    , worker_factory_(worker_factory)
    , is_active_(false)
    , is_discovery_active_(false)
    , is_network_scan_active_(false)
    , network_scan_running_(false) {
    
    if (!device_repository_ || !datapoint_repository_) {
        throw std::invalid_argument("DeviceRepository or DataPointRepository is null");
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService created with complete entity support");
    
    // Repository 상태 로깅
    logger.Info("📊 Repository Status:");
    logger.Info("  - DeviceRepository: ✅");
    logger.Info("  - DataPointRepository: ✅");
    logger.Info(std::string("  - CurrentValueRepository: ") + (current_value_repo ? "✅" : "❌"));
    logger.Info(std::string("  - DeviceSettingsRepository: ") + (device_settings_repo ? "✅" : "❌"));
    logger.Info(std::string("  - WorkerFactory: ") + (worker_factory ? "✅" : "❌"));
}

BACnetDiscoveryService::~BACnetDiscoveryService() {
    // 모든 활성 서비스 중지
    StopDynamicDiscovery();
    StopNetworkScan();
    UnregisterFromWorker();
    
    // 모든 관리 중인 Worker 정리
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    for (auto& [device_id, managed_worker] : managed_workers_) {
        if (managed_worker->is_running && managed_worker->worker) {
            try {
                auto stop_future = managed_worker->worker->Stop();
                stop_future.wait_for(std::chrono::seconds(5));  // 5초 대기
            } catch (const std::exception& e) {
                auto& logger = LogManager::getInstance();
                logger.Error("Exception stopping worker for device " + device_id + ": " + e.what());
            }
        }
    }
    managed_workers_.clear();
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService destroyed");
}

// =============================================================================
// 🔥 동적 Discovery 서비스 제어
// =============================================================================

bool BACnetDiscoveryService::StartDynamicDiscovery() {
    if (is_discovery_active_.load()) {
        auto& logger = LogManager::getInstance();
        logger.Warn("BACnet dynamic discovery already active");
        return false;
    }
    
    if (!worker_factory_) {
        auto& logger = LogManager::getInstance();
        logger.Error("WorkerFactory is null - cannot start dynamic discovery");
        return false;
    }
    
    is_discovery_active_ = true;
    is_active_ = true;
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔥 BACnet dynamic discovery started");
    
    return true;
}

void BACnetDiscoveryService::StopDynamicDiscovery() {
    if (!is_discovery_active_.load()) {
        return;
    }
    
    is_discovery_active_ = false;
    
    // 모든 관리 중인 Worker 중지
    std::vector<std::string> device_ids;
    {
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        for (const auto& [device_id, managed_worker] : managed_workers_) {
            device_ids.push_back(device_id);
        }
    }
    
    for (const auto& device_id : device_ids) {
        StopWorkerForDevice(device_id);
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnet dynamic discovery stopped");
}

bool BACnetDiscoveryService::IsDiscoveryActive() const {
    return is_discovery_active_.load();
}

// =============================================================================
// 🔥 Worker 동적 생성 및 관리
// =============================================================================

std::shared_ptr<BaseDeviceWorker> BACnetDiscoveryService::CreateWorkerForDevice(const DeviceInfo& device_info) {
    if (!worker_factory_) {
        auto& logger = LogManager::getInstance();
        logger.Error("WorkerFactory is null - cannot create worker for device: " + device_info.id);
        return nullptr;
    }
    
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("🔧 Creating worker for device: " + device_info.name + " (ID: " + device_info.id + ")");
        
        // 🔧 DeviceInfo → DeviceEntity 변환 필요
        // WorkerFactory는 DeviceEntity를 받으므로 변환
        Database::Entities::DeviceEntity device_entity;
        ConvertDeviceInfoToEntity(device_info, device_entity);
        
        // WorkerFactory를 통해 프로토콜별 Worker 생성 
        auto unique_worker = worker_factory_->CreateWorker(device_entity);
        
        if (!unique_worker) {
            logger.Error("❌ Failed to create worker for device: " + device_info.name);
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.workers_failed++;
            return nullptr;
        }
        
        // 🔧 unique_ptr → shared_ptr 변환
        std::shared_ptr<BaseDeviceWorker> worker = std::move(unique_worker);
        
        // ManagedWorker로 감싸서 관리
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        auto managed_worker = std::make_unique<ManagedWorker>(worker, device_info);
        managed_workers_[device_info.id] = std::move(managed_worker);
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.workers_created++;
        }
        
        logger.Info("✅ Worker created successfully for device: " + device_info.name);
        return worker;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception creating worker for device " + device_info.id + ": " + std::string(e.what()));
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.workers_failed++;
        return nullptr;
    }
}

bool BACnetDiscoveryService::StartWorkerForDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    
    auto it = managed_workers_.find(device_id);
    if (it == managed_workers_.end()) {
        auto& logger = LogManager::getInstance();
        logger.Error("Worker not found for device: " + device_id);
        return false;
    }
    
    auto& managed_worker = it->second;
    
    if (managed_worker->is_running) {
        auto& logger = LogManager::getInstance();
        logger.Warn("Worker already running for device: " + device_id);
        return true;
    }
    
    if (!managed_worker->worker) {
        auto& logger = LogManager::getInstance();
        logger.Error("Worker is null for device: " + device_id);
        managed_worker->is_failed = true;
        return false;
    }
    
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("🚀 Starting worker for device: " + managed_worker->device_info.name);
        
        // 비동기 시작
        auto start_future = managed_worker->worker->Start();
        bool start_success = start_future.get();  // 시작 완료까지 대기
        
        if (start_success) {
            managed_worker->is_running = true;
            managed_worker->is_failed = false;
            managed_worker->last_activity = std::chrono::system_clock::now();
            
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.workers_started++;
            }
            
            logger.Info("✅ Worker started successfully for device: " + managed_worker->device_info.name);
            return true;
        } else {
            managed_worker->is_failed = true;
            managed_worker->last_error = "Failed to start worker";
            
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.workers_failed++;
            }
            
            logger.Error("❌ Failed to start worker for device: " + managed_worker->device_info.name);
            return false;
        }
        
    } catch (const std::exception& e) {
        managed_worker->is_failed = true;
        managed_worker->last_error = e.what();
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception starting worker for device " + device_id + ": " + std::string(e.what()));
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        statistics_.workers_failed++;
        return false;
    }
}

bool BACnetDiscoveryService::StopWorkerForDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    
    auto it = managed_workers_.find(device_id);
    if (it == managed_workers_.end()) {
        return false;  // Worker가 없으면 이미 중지된 것으로 간주
    }
    
    auto& managed_worker = it->second;
    
    if (!managed_worker->is_running) {
        return true;  // 이미 중지됨
    }
    
    if (!managed_worker->worker) {
        managed_worker->is_running = false;
        return true;
    }
    
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("🛑 Stopping worker for device: " + managed_worker->device_info.name);
        
        // 비동기 중지
        auto stop_future = managed_worker->worker->Stop();
        bool stop_success = stop_future.get();  // 중지 완료까지 대기
        
        managed_worker->is_running = false;
        
        if (stop_success) {
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.workers_stopped++;
            }
            
            logger.Info("✅ Worker stopped successfully for device: " + managed_worker->device_info.name);
        } else {
            logger.Warn("⚠️ Worker stop returned false for device: " + managed_worker->device_info.name);
        }
        
        return stop_success;
        
    } catch (const std::exception& e) {
        managed_worker->is_running = false;
        managed_worker->is_failed = true;
        managed_worker->last_error = e.what();
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception stopping worker for device " + device_id + ": " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::RemoveWorkerForDevice(const std::string& device_id) {
    // 먼저 중지
    StopWorkerForDevice(device_id);
    
    // 관리 목록에서 제거
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    auto it = managed_workers_.find(device_id);
    if (it != managed_workers_.end()) {
        auto& logger = LogManager::getInstance();
        logger.Info("🗑️ Removing worker for device: " + it->second->device_info.name);
        managed_workers_.erase(it);
        return true;
    }
    
    return false;
}

// =============================================================================
// 🔥 Worker 상태 조회
// =============================================================================

std::vector<std::string> BACnetDiscoveryService::GetManagedWorkerIds() const {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    std::vector<std::string> device_ids;
    device_ids.reserve(managed_workers_.size());
    
    for (const auto& [device_id, managed_worker] : managed_workers_) {
        device_ids.push_back(device_id);
    }
    
    return device_ids;
}

std::shared_ptr<BaseDeviceWorker> BACnetDiscoveryService::GetWorkerForDevice(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    auto it = managed_workers_.find(device_id);
    if (it != managed_workers_.end()) {
        return it->second->worker;
    }
    return nullptr;
}

BACnetDiscoveryService::ManagedWorker* BACnetDiscoveryService::GetManagedWorkerInfo(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    auto it = managed_workers_.find(device_id);
    if (it != managed_workers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::map<std::string, bool> BACnetDiscoveryService::GetAllWorkerStates() const {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    std::map<std::string, bool> states;
    
    for (const auto& [device_id, managed_worker] : managed_workers_) {
        states[device_id] = managed_worker->is_running;
    }
    
    return states;
}

// =============================================================================
// 🔥 콜백 핸들러들 - 동적 Worker 생성 로직 추가
// =============================================================================

void BACnetDiscoveryService::OnDeviceDiscovered(const DeviceInfo& device) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.devices_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet device discovered: " + device.name + 
                     " (ID: " + device.id + ")");
        
        // 데이터베이스에 저장
        if (SaveDiscoveredDeviceToDatabase(device)) {
            statistics_.devices_saved++;
            logger.Info("✅ Device saved to database: " + device.name);
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save device to database: " + device.name);
            return;  // DB 저장 실패 시 Worker 생성하지 않음
        }
        
        // 🔥 핵심: 동적 Discovery가 활성화된 경우 자동으로 Worker 생성
        if (is_discovery_active_.load()) {
            CreateAndStartWorkerForNewDevice(device);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnDeviceDiscovered: " + std::string(e.what()));
    }
}

// 기존 OnObjectDiscovered, OnValueChanged 메서드들은 동일하게 유지...
void BACnetDiscoveryService::OnObjectDiscovered(const DataPoint& object) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet object discovered: " + object.name + " (ID: " + object.id + ")");
        
        std::vector<DataPoint> objects = {object};
        uint32_t device_id = 0;
        try {
            device_id = std::stoul(object.device_id);
        } catch (...) {
            device_id = 260001;
        }
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved++;
            logger.Info("✅ Object saved to database: " + object.name);
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save object to database: " + object.name);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (single): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed += objects.size();
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet objects discovered for device " + std::to_string(device_id) + 
                     ": " + std::to_string(objects.size()) + " objects");
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved += objects.size();
            logger.Info("✅ Objects saved to database for device " + std::to_string(device_id));
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save objects to database for device " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (multiple): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, const TimestampedValue& value) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.values_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet value changed for object: " + object_id);
        
        if (UpdateCurrentValueInDatabase(object_id, value)) {
            statistics_.values_saved++;
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to update current value for object: " + object_id);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnValueChanged: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 핵심: 새 디바이스 발견 시 Worker 생성 및 시작
// =============================================================================

bool BACnetDiscoveryService::CreateAndStartWorkerForNewDevice(const DeviceInfo& device_info) {
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("🎯 Creating and starting worker for new device: " + device_info.name);
        
        // 1. 이미 관리 중인 디바이스인지 확인
        if (IsDeviceAlreadyManaged(device_info.id)) {
            logger.Info("Device already managed: " + device_info.id);
            return true;
        }
        
        // 2. Worker 생성
        auto worker = CreateWorkerForDevice(device_info);
        if (!worker) {
            logger.Error("Failed to create worker for device: " + device_info.name);
            return false;
        }
        
        // 3. Worker 시작
        bool started = StartWorkerForDevice(device_info.id);
        if (!started) {
            logger.Error("Failed to start worker for device: " + device_info.name);
            // 실패한 Worker는 제거
            RemoveWorkerForDevice(device_info.id);
            return false;
        }
        
        logger.Info("🎉 Successfully created and started worker for device: " + device_info.name);
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in CreateAndStartWorkerForNewDevice: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::IsDeviceAlreadyManaged(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    return managed_workers_.find(device_id) != managed_workers_.end();
}

void BACnetDiscoveryService::CleanupFailedWorkers() {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    
    auto it = managed_workers_.begin();
    while (it != managed_workers_.end()) {
        if (it->second->is_failed) {
            auto& logger = LogManager::getInstance();
            logger.Info("🧹 Cleaning up failed worker for device: " + it->second->device_info.name);
            it = managed_workers_.erase(it);
        } else {
            ++it;
        }
    }
}

void BACnetDiscoveryService::UpdateWorkerActivity(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(managed_workers_mutex_);
    auto it = managed_workers_.find(device_id);
    if (it != managed_workers_.end()) {
        it->second->last_activity = std::chrono::system_clock::now();
    }
}

// =============================================================================
// 기존 레거시 호환 메서드들
// =============================================================================

bool BACnetDiscoveryService::RegisterToWorker(std::shared_ptr<BACnetWorker> worker) {
    if (!worker) {
        auto& logger = LogManager::getInstance();
        logger.Error("BACnetDiscoveryService::RegisterToWorker - Worker is null");
        return false;
    }
    
    registered_worker_ = worker;
    is_active_ = true;
    
    // BACnet Worker 콜백 설정 (레거시 방식)
    // worker->SetObjectDiscoveredCallback(...);
    // worker->SetValueChangedCallback(...);
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService registered to worker successfully (legacy mode)");
    return true;
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    if (auto worker = registered_worker_.lock()) {
        // 콜백 제거
        // worker->SetObjectDiscoveredCallback(nullptr);
        // worker->SetValueChangedCallback(nullptr);
    }
    
    registered_worker_.reset();
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService unregistered from worker");
}

BACnetDiscoveryService::Statistics BACnetDiscoveryService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

bool BACnetDiscoveryService::IsActive() const {
    return is_active_.load();
}

void BACnetDiscoveryService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = Statistics{};
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService statistics reset");
}

// =============================================================================
// 🔥 Repository 의존성 주입
// =============================================================================

void BACnetDiscoveryService::SetDeviceSettingsRepository(
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo) {
    device_settings_repository_ = device_settings_repo;
    
    auto& logger = LogManager::getInstance();
    logger.Info("DeviceSettingsRepository injected into BACnetDiscoveryService");
}

// =============================================================================
// 🔥 네트워크 스캔 기능 (선택사항)
// =============================================================================

bool BACnetDiscoveryService::StartNetworkScan(const std::string& network_range) {
    if (is_network_scan_active_.load()) {
        return false;
    }
    
    is_network_scan_active_ = true;
    network_scan_running_ = true;
    
    // 네트워크 스캔 스레드 시작 (실제 구현은 BACnet 라이브러리 기반)
    network_scan_thread_ = std::make_unique<std::thread>([this, network_range]() {
        auto& logger = LogManager::getInstance();
        logger.Info("🔍 Network scan started for range: " + 
                   (network_range.empty() ? "auto-detect" : network_range));
        
        // TODO: 실제 BACnet 네트워크 스캔 구현
        // 1. BACnet Who-Is 브로드캐스트 전송
        // 2. I-Am 응답 수집
        // 3. 새 디바이스 발견 시 OnDeviceDiscovered 호출
        
        while (network_scan_running_.load()) {
            // 네트워크 스캔 로직
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
        logger.Info("🔍 Network scan stopped");
    });
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔍 BACnet network scan started");
    return true;
}

void BACnetDiscoveryService::StopNetworkScan() {
    if (!is_network_scan_active_.load()) {
        return;
    }
    
    network_scan_running_ = false;
    is_network_scan_active_ = false;
    
    if (network_scan_thread_ && network_scan_thread_->joinable()) {
        network_scan_thread_->join();
        network_scan_thread_.reset();
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnet network scan stopped");
}

bool BACnetDiscoveryService::IsNetworkScanActive() const {
    return is_network_scan_active_.load();
}

// =============================================================================
// 기존 데이터베이스 저장 메서드들 (동일하게 유지)
// =============================================================================

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(const DeviceInfo& device) {
    try {
        auto existing_devices = device_repository_->findAll();
        for (const auto& existing : existing_devices) {
            if (existing.getId() == std::stoi(device.id)) {
                auto& logger = LogManager::getInstance();
                logger.Debug("Device already exists in database: " + device.id);
                return true;
            }
        }
        
        DeviceEntity entity;
        entity.setId(std::stoi(device.id));
        entity.setName(device.name);
        entity.setDescription(device.description);
        entity.setProtocolType(device.protocol_type);
        entity.setEndpoint(device.endpoint);
        entity.setEnabled(device.is_enabled);
        
        std::stringstream properties_json;
        properties_json << "{";
        bool first = true;
        for (const auto& [key, value] : device.properties) {
            if (!first) properties_json << ",";
            properties_json << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        properties_json << "}";
        entity.setConfig(properties_json.str());
        
        return device_repository_->save(entity);
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredDeviceToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        (void)device_id;
        bool all_success = true;
        
        for (const auto& object : objects) {
            auto existing_points = datapoint_repository_->findAll();
            bool exists = false;
            for (const auto& existing : existing_points) {
                if (existing.getId() == std::stoi(object.id)) {
                    exists = true;
                    break;
                }
            }
            
            if (exists) {
                continue;
            }
            
            DataPointEntity entity;
            entity.setId(std::stoi(object.id));
            entity.setDeviceId(std::stoi(object.device_id));
            entity.setName(object.name);
            entity.setDescription(object.description);
            entity.setAddress(object.address);
            entity.setDataType(object.data_type);
            entity.setUnit(object.unit);
            entity.setEnabled(object.is_enabled);
            entity.setProtocolParams(object.protocol_params);
            
            if (!datapoint_repository_->save(entity)) {
                all_success = false;
            }
        }
        
        return all_success;
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredObjectsToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value) {
    try {
        if (!current_value_repository_) {
            return false;
        }
        
        CurrentValueEntity entity;
        entity.setId(std::stoi(object_id));
        entity.setPointId(std::stoi(object_id));
        
        double value_double = ConvertDataValueToDouble(value.value);
        json value_json;
        value_json["value"] = value_double;
        entity.setCurrentValue(value_json.dump());
        
        entity.setQuality(value.quality);
        entity.setValueTimestamp(value.timestamp);
        
        std::vector<QueryCondition> conditions = {
            QueryCondition("point_id", "=", object_id)
        };
        
        auto existing_values = current_value_repository_->findByConditions(conditions);
        
        if (existing_values.empty()) {
            return current_value_repository_->save(entity);
        } else {
            entity.setId(existing_values[0].getId());
            return current_value_repository_->update(entity);
        }
        
    } catch (const std::exception& e) {
        HandleError("UpdateCurrentValueInDatabase", e.what());
        return false;
    }
}

// =============================================================================
// 기존 유틸리티 함수들 (동일하게 유지)
// =============================================================================

int BACnetDiscoveryService::FindDeviceIdInDatabase(uint32_t bacnet_device_id) {
    try {
        std::vector<QueryCondition> conditions = {
            QueryCondition("config", "LIKE", "%\"bacnet_device_id\":\"" + std::to_string(bacnet_device_id) + "\"%")
        };
        
        auto devices = device_repository_->findByConditions(conditions);
        
        if (!devices.empty()) {
            return devices[0].getId();
        }
        
        return -1;
        
    } catch (const std::exception& e) {
        HandleError("FindDeviceIdInDatabase", e.what());
        return -1;
    }
}

std::string BACnetDiscoveryService::GenerateDataPointId(uint32_t device_id, const DataPoint& object) {
    auto obj_type_it = object.protocol_params.find("bacnet_object_type");
    auto obj_inst_it = object.protocol_params.find("bacnet_instance");
    
    if (obj_type_it != object.protocol_params.end() && 
        obj_inst_it != object.protocol_params.end()) {
        return std::to_string(device_id) + ":" + 
               obj_type_it->second + ":" + 
               obj_inst_it->second;
    }
    
    return std::to_string(device_id) + ":" + std::to_string(object.address);
}

std::string BACnetDiscoveryService::ObjectTypeToString(int type) {
    switch (type) {
        case 0: return "ANALOG_INPUT";
        case 1: return "ANALOG_OUTPUT";
        case 2: return "ANALOG_VALUE";
        case 3: return "BINARY_INPUT";
        case 4: return "BINARY_OUTPUT";
        case 5: return "BINARY_VALUE";
        case 8: return "DEVICE";
        case 13: return "MULTI_STATE_INPUT";
        case 14: return "MULTI_STATE_OUTPUT";
        case 19: return "MULTI_STATE_VALUE";
        default: return "UNKNOWN_" + std::to_string(type);
    }
}

PulseOne::Enums::DataType BACnetDiscoveryService::DetermineDataType(int type) {
    switch (type) {
        case 0: case 1: case 2:
            return DataType::FLOAT32;
        case 3: case 4: case 5:
            return DataType::BOOL;
        case 13: case 14: case 19:
            return DataType::INT32;
        default:
            return DataType::STRING;
    }
}

std::string BACnetDiscoveryService::DataTypeToString(PulseOne::Enums::DataType type) {
    switch (type) {
        case DataType::BOOL: return "bool";
        case DataType::INT32: return "int";
        case DataType::FLOAT32: return "float";
        case DataType::STRING: return "string";
        default: return "unknown";
    }
}

std::string BACnetDiscoveryService::ConvertDataValueToString(const DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int16_t>(value)) {
            return std::to_string(std::get<int16_t>(value));
        } else if (std::holds_alternative<uint16_t>(value)) {
            return std::to_string(std::get<uint16_t>(value));
        } else if (std::holds_alternative<int32_t>(value)) {
            return std::to_string(std::get<int32_t>(value));
        } else if (std::holds_alternative<uint32_t>(value)) {
            return std::to_string(std::get<uint32_t>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return "unknown";
    } catch (const std::exception&) {
        return "unknown";
    }
}

double BACnetDiscoveryService::ConvertDataValueToDouble(const DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? 1.0 : 0.0;
        } else if (std::holds_alternative<int16_t>(value)) {
            return static_cast<double>(std::get<int16_t>(value));
        } else if (std::holds_alternative<uint16_t>(value)) {
            return static_cast<double>(std::get<uint16_t>(value));
        } else if (std::holds_alternative<int32_t>(value)) {
            return static_cast<double>(std::get<int32_t>(value));
        } else if (std::holds_alternative<uint32_t>(value)) {
            return static_cast<double>(std::get<uint32_t>(value));
        } else if (std::holds_alternative<float>(value)) {
            return static_cast<double>(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            try {
                return std::stod(std::get<std::string>(value));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

void BACnetDiscoveryService::HandleError(const std::string& context, const std::string& error) {
    auto& logger = LogManager::getInstance();
    logger.Error("BACnetDiscoveryService::" + context + " - " + error);
}

int BACnetDiscoveryService::GuessObjectTypeFromName(const std::string& object_name) {
    std::regex ai_pattern("^AI\\d+", std::regex_constants::icase);
    std::regex ao_pattern("^AO\\d+", std::regex_constants::icase);
    std::regex av_pattern("^AV\\d+", std::regex_constants::icase);
    std::regex bi_pattern("^BI\\d+", std::regex_constants::icase);
    std::regex bo_pattern("^BO\\d+", std::regex_constants::icase);
    std::regex bv_pattern("^BV\\d+", std::regex_constants::icase);
    std::regex mi_pattern("^MI\\d+", std::regex_constants::icase);
    std::regex mo_pattern("^MO\\d+", std::regex_constants::icase);
    std::regex mv_pattern("^MV\\d+", std::regex_constants::icase);
    
    if (std::regex_search(object_name, ai_pattern)) return 0;
    if (std::regex_search(object_name, ao_pattern)) return 1;
    if (std::regex_search(object_name, av_pattern)) return 2;
    if (std::regex_search(object_name, bi_pattern)) return 3;
    if (std::regex_search(object_name, bo_pattern)) return 4;
    if (std::regex_search(object_name, bv_pattern)) return 5;
    if (std::regex_search(object_name, mi_pattern)) return 13;
    if (std::regex_search(object_name, mo_pattern)) return 14;
    if (std::regex_search(object_name, mv_pattern)) return 19;
    
    return 2;
}

std::string BACnetDiscoveryService::BACnetAddressToString(const BACNET_ADDRESS& address) {
    std::stringstream ss;
    
    if (address.net == 0) {
        for (int i = 0; i < address.len && i < 6; i++) {
            if (i > 0) ss << ".";
            ss << static_cast<int>(address.adr[i]);
        }
    } else {
        ss << "Network:" << address.net << ",Address:";
        for (int i = 0; i < address.len && i < 6; i++) {
            if (i > 0) ss << ".";
            ss << static_cast<int>(address.adr[i]);
        }
    }
    
    return ss.str();
}

} // namespace Workers
} // namespace PulseOne