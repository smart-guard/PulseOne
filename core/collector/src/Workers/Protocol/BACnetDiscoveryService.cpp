/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 - setProtocolType deprecated 해결 완성본
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 6.2.0 - setProtocolType deprecated 해결
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Workers/Protocol/BACnetWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/DatabaseTypes.h"
#include "Database/RepositoryFactory.h"          // 추가: ProtocolRepository 접근용
#include "Database/Repositories/ProtocolRepository.h"  // 추가
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
    // 헬퍼 함수: 프로토콜 타입을 ID로 안전하게 변환
    // =======================================================================
    
    int ResolveProtocolTypeToId(const std::string& protocol_type) {
        try {
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            auto protocol_repo = repo_factory.getProtocolRepository();
            
            if (!protocol_repo) {
                LogManager::getInstance().Error("ProtocolRepository not available");
                return 0;  // 실패 시 0 반환 (유효하지 않은 ID)
            }
            
            std::string actual_protocol_type = protocol_type.empty() ? "BACNET_IP" : protocol_type;
            auto protocol_opt = protocol_repo->findByType(actual_protocol_type);
            
            if (protocol_opt.has_value()) {
                LogManager::getInstance().Debug("Resolved protocol '" + actual_protocol_type + 
                                              "' to ID " + std::to_string(protocol_opt->getId()));
                return protocol_opt->getId();
            } else {
                LogManager::getInstance().Error("Protocol type not found in database: " + actual_protocol_type);
                return 0;  // 프로토콜이 DB에 없으면 0 반환
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Failed to resolve protocol type '" + protocol_type + 
                                          "': " + std::string(e.what()));
            return 0;
        }
    }

    // =======================================================================
    // 🔥 DeviceInfo ↔ DeviceEntity 변환 함수들
    // =======================================================================
    
    void ConvertDeviceInfoToEntity(const DeviceInfo& device_info, Database::Entities::DeviceEntity& entity);
    Database::Entities::DeviceEntity ConvertDeviceInfoToEntity(const DeviceInfo& device_info);
    
    // =============================================================================
// 🔥 DeviceInfo ↔ DeviceEntity 변환 구현
// =============================================================================

void BACnetDiscoveryService::ConvertDeviceInfoToEntity(const DeviceInfo& device_info, 
                                                       Database::Entities::DeviceEntity& entity) {
    try {
        // ID 변환 - 안전성 강화
        int device_id = 0;
        try {
            device_id = std::stoi(device_info.id);
        } catch (const std::exception& e) {
            auto& logger = LogManager::getInstance();
            logger.Warn("Invalid device ID format, using hash: " + device_info.id);
            device_id = static_cast<int>(std::hash<std::string>{}(device_info.id) % 1000000);
        }
        
        entity.setId(device_id);
        entity.setName(device_info.name.empty() ? "Unknown Device" : device_info.name);
        entity.setDescription(device_info.description);
        
        // 수정됨: setProtocolType → setProtocolId + ProtocolRepository 조회
        int protocol_id = ResolveProtocolTypeToId(device_info.protocol_type);
        if (protocol_id > 0) {
            entity.setProtocolId(protocol_id);
        } else {
            auto& logger = LogManager::getInstance();
            logger.Error("Failed to resolve protocol for device " + device_info.id + 
                        ", protocol_type: " + device_info.protocol_type);
            // 프로토콜 해결 실패 시 예외 발생 (데이터 무결성 보장)
            throw std::runtime_error("Cannot resolve protocol type: " + device_info.protocol_type);
        }
        
        entity.setEndpoint(device_info.endpoint);
        entity.setEnabled(device_info.is_enabled);
        
        // properties를 JSON으로 안전하게 변환
        json properties_json;
        try {
            for (const auto& [key, value] : device_info.properties) {
                if (!key.empty()) {
                    properties_json[key] = value;
                }
            }
            entity.setConfig(properties_json.dump());
        } catch (const std::exception& e) {
            auto& logger = LogManager::getInstance();
            logger.Warn("Failed to serialize properties for device " + device_info.id + 
                       ", using empty config: " + std::string(e.what()));
            entity.setConfig("{}");
        }
        
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
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo,
    std::shared_ptr<WorkerFactory> worker_factory)
    : device_repository_(device_repo)
    , datapoint_repository_(datapoint_repo)
    , current_value_repository_(current_value_repo)
    , device_settings_repository_(device_settings_repo)
    , worker_factory_(worker_factory)
    , is_active_(false)
    , is_discovery_active_(false)
    , is_network_scan_active_(false)
    , network_scan_running_(false) {
    
    // 필수 repository 검증
    if (!device_repository_ || !datapoint_repository_) {
        throw std::invalid_argument("DeviceRepository or DataPointRepository is null");
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService created with complete entity support");
    
    // Repository 상태 로깅
    logger.Info("Repository Status:");
    logger.Info("  - DeviceRepository: " + std::string(device_repository_ ? "OK" : "FAIL"));
    logger.Info("  - DataPointRepository: " + std::string(datapoint_repository_ ? "OK" : "FAIL"));
    logger.Info("  - CurrentValueRepository: " + std::string(current_value_repository_ ? "OK" : "OPTIONAL"));
    logger.Info("  - DeviceSettingsRepository: " + std::string(device_settings_repository_ ? "OK" : "OPTIONAL"));
    logger.Info("  - WorkerFactory: " + std::string(worker_factory_ ? "OK" : "OPTIONAL"));
    
    // 통계 초기화
    statistics_ = Statistics{};
}


BACnetDiscoveryService::~BACnetDiscoveryService() {
    try {
        // 모든 활성 서비스 중지
        StopDynamicDiscovery();
        StopNetworkScan();
        UnregisterFromWorker();
        
        // 모든 관리 중인 Worker 안전하게 정리
        SafeCleanupAllWorkers();
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnetDiscoveryService destroyed safely");
        
    } catch (const std::exception& e) {
        // 소멸자에서는 예외를 던지면 안 되므로 로그만 출력
        try {
            auto& logger = LogManager::getInstance();
            logger.Error("Exception in BACnetDiscoveryService destructor: " + std::string(e.what()));
        } catch (...) {
            // 로깅조차 실패하면 무시
        }
    }
}

// =============================================================================
// 안전한 Worker 정리 메서드 추가
// =============================================================================

void BACnetDiscoveryService::SafeCleanupAllWorkers() {
    std::vector<std::string> device_ids;
    
    // 먼저 모든 device_id를 수집 (락 시간 최소화)
    {
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        device_ids.reserve(managed_workers_.size());
        for (const auto& [device_id, managed_worker] : managed_workers_) {
            device_ids.push_back(device_id);
        }
    }
    
    // 각 Worker를 개별적으로 정리 (데드락 방지)
    for (const auto& device_id : device_ids) {
        try {
            StopWorkerForDeviceSafe(device_id);
        } catch (const std::exception& e) {
            try {
                auto& logger = LogManager::getInstance();
                logger.Error("Failed to cleanup worker " + device_id + ": " + std::string(e.what()));
            } catch (...) {
                // 로깅 실패 시 무시
            }
        }
    }
    
    // 최종 정리
    {
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        managed_workers_.clear();
    }
}

bool BACnetDiscoveryService::StopWorkerForDeviceSafe(const std::string& device_id) {
    std::unique_ptr<ManagedWorker> managed_worker_to_stop;
    
    // 먼저 Worker를 맵에서 제거 (락 시간 최소화)
    {
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        auto it = managed_workers_.find(device_id);
        if (it == managed_workers_.end()) {
            return true;  // 이미 제거됨
        }
        
        managed_worker_to_stop = std::move(it->second);
        managed_workers_.erase(it);
    }
    
    // 락 없이 Worker 중지 (데드락 방지)
    if (managed_worker_to_stop && managed_worker_to_stop->worker && managed_worker_to_stop->is_running) {
        try {
            auto& logger = LogManager::getInstance();
            logger.Info("Safely stopping worker for device: " + device_id);
            
            auto stop_future = managed_worker_to_stop->worker->Stop();
            
            // 타임아웃을 짧게 설정하여 응답성 향상
            auto status = stop_future.wait_for(std::chrono::seconds(3));
            
            if (status == std::future_status::ready) {
                bool success = stop_future.get();
                logger.Info("Worker stopped " + std::string(success ? "successfully" : "with errors") + 
                           " for device: " + device_id);
                return success;
            } else {
                logger.Warn("Worker stop timeout for device: " + device_id);
                return false;
            }
            
        } catch (const std::exception& e) {
            auto& logger = LogManager::getInstance();
            logger.Error("Exception stopping worker for device " + device_id + ": " + std::string(e.what()));
            return false;
        }
    }
    
    return true;
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
    
    try {
        is_discovery_active_ = true;
        is_active_ = true;
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnet dynamic discovery started successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        is_discovery_active_ = false;
        is_active_ = false;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Failed to start dynamic discovery: " + std::string(e.what()));
        return false;
    }
}

void BACnetDiscoveryService::StopDynamicDiscovery() {
    if (!is_discovery_active_.load()) {
        return;
    }
    
    try {
        is_discovery_active_ = false;
        
        // 모든 관리 중인 Worker 안전하게 중지
        SafeCleanupAllWorkers();
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnet dynamic discovery stopped successfully");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception during dynamic discovery stop: " + std::string(e.what()));
    }
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
    
    if (device_info.id.empty() || device_info.name.empty()) {
        auto& logger = LogManager::getInstance();
        logger.Error("Invalid device info - ID or name is empty");
        return nullptr;
    }
    
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("Creating worker for device: " + device_info.name + " (ID: " + device_info.id + ")");
        
        // DeviceInfo → DeviceEntity 안전한 변환
        Database::Entities::DeviceEntity device_entity;
        try {
            ConvertDeviceInfoToEntity(device_info, device_entity);
        } catch (const std::exception& e) {
            logger.Error("Failed to convert DeviceInfo to DeviceEntity for device " + 
                        device_info.id + ": " + std::string(e.what()));
            UpdateStatistics("workers_failed", false);
            return nullptr;
        }
        
        // WorkerFactory를 통해 Worker 생성
        auto unique_worker = worker_factory_->CreateWorker(device_entity);
        
        if (!unique_worker) {
            logger.Error("Failed to create worker for device: " + device_info.name);
            UpdateStatistics("workers_failed", false);
            return nullptr;
        }
        
        // unique_ptr → shared_ptr 안전한 변환
        std::shared_ptr<BaseDeviceWorker> shared_worker;
        try {
            shared_worker = std::shared_ptr<BaseDeviceWorker>(unique_worker.release());
        } catch (const std::exception& e) {
            logger.Error("Failed to convert unique_ptr to shared_ptr for device " + 
                        device_info.id + ": " + std::string(e.what()));
            UpdateStatistics("workers_failed", false);
            return nullptr;
        }
        
        // ManagedWorker로 감싸서 안전하게 관리
        {
            std::lock_guard<std::mutex> lock(managed_workers_mutex_);
            auto managed_worker = std::make_unique<ManagedWorker>(shared_worker, device_info);
            managed_workers_[device_info.id] = std::move(managed_worker);
        }
        
        UpdateStatistics("workers_created", true);
        
        logger.Info("Worker created successfully for device: " + device_info.name);
        return shared_worker;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception creating worker for device " + device_info.id + ": " + std::string(e.what()));
        UpdateStatistics("workers_failed", false);
        return nullptr;
    }
}

bool BACnetDiscoveryService::StartWorkerForDevice(const std::string& device_id) {
    if (device_id.empty()) {
        auto& logger = LogManager::getInstance();
        logger.Error("Empty device_id provided to StartWorkerForDevice");
        return false;
    }
    
    std::shared_ptr<BaseDeviceWorker> worker_to_start;
    std::string device_name;
    
    // Worker 찾기 (락 시간 최소화)
    {
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
        
        worker_to_start = managed_worker->worker;
        device_name = managed_worker->device_info.name;
    }
    
    // 락 없이 Worker 시작 (데드락 방지)
    try {
        auto& logger = LogManager::getInstance();
        logger.Info("Starting worker for device: " + device_name);
        
        // 비동기 시작 with timeout
        auto start_future = worker_to_start->Start();
        auto status = start_future.wait_for(std::chrono::seconds(10));
        
        bool start_success = false;
        if (status == std::future_status::ready) {
            start_success = start_future.get();
        } else {
            logger.Error("Worker start timeout for device: " + device_id);
            UpdateStatistics("workers_failed", false);
            return false;
        }
        
        // 결과 업데이트 (다시 락 사용)
        {
            std::lock_guard<std::mutex> lock(managed_workers_mutex_);
            auto it = managed_workers_.find(device_id);
            if (it != managed_workers_.end()) {
                auto& managed_worker = it->second;
                
                if (start_success) {
                    managed_worker->is_running = true;
                    managed_worker->is_failed = false;
                    managed_worker->last_activity = std::chrono::system_clock::now();
                    
                    UpdateStatistics("workers_started", true);
                    logger.Info("Worker started successfully for device: " + device_name);
                } else {
                    managed_worker->is_failed = true;
                    managed_worker->last_error = "Failed to start worker";
                    
                    UpdateStatistics("workers_failed", false);
                    logger.Error("Failed to start worker for device: " + device_name);
                }
            }
        }
        
        return start_success;
        
    } catch (const std::exception& e) {
        // 예외 발생 시 상태 업데이트
        {
            std::lock_guard<std::mutex> lock(managed_workers_mutex_);
            auto it = managed_workers_.find(device_id);
            if (it != managed_workers_.end()) {
                auto& managed_worker = it->second;
                managed_worker->is_failed = true;
                managed_worker->last_error = e.what();
            }
        }
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception starting worker for device " + device_id + ": " + std::string(e.what()));
        UpdateStatistics("workers_failed", false);
        return false;
    }
}

bool BACnetDiscoveryService::StopWorkerForDevice(const std::string& device_id) {
    return StopWorkerForDeviceSafe(device_id);
}

bool BACnetDiscoveryService::RemoveWorkerForDevice(const std::string& device_id) {
    // 먼저 중지
    bool stopped = StopWorkerForDeviceSafe(device_id);
    
    if (stopped) {
        auto& logger = LogManager::getInstance();
        logger.Info("Removed worker for device: " + device_id);
    }
    
    return stopped;
}

// =============================================================================
// 통계 관리 - 스레드 안전성 강화
// =============================================================================

void BACnetDiscoveryService::UpdateStatistics(const std::string& operation, bool success) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (operation == "workers_created" && success) {
            statistics_.workers_created++;
        } else if (operation == "workers_started" && success) {
            statistics_.workers_started++;
        } else if (operation == "workers_stopped") {
            statistics_.workers_stopped++;
        } else if (operation == "workers_failed") {
            statistics_.workers_failed++;
        }
        
        statistics_.last_activity = std::chrono::system_clock::now();
        
    } catch (const std::exception& e) {
        // 통계 업데이트 실패는 로그만 출력
        try {
            auto& logger = LogManager::getInstance();
            logger.Warn("Failed to update statistics: " + std::string(e.what()));
        } catch (...) {
            // 로깅 실패 시 무시
        }
    }
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
        UpdateStatistics("devices_processed", true);
        
        auto& logger = LogManager::getInstance();
        logger.Debug("BACnet device discovered: " + device.name + " (ID: " + device.id + ")");
        
        // 데이터베이스에 저장
        bool saved = false;
        try {
            saved = SaveDiscoveredDeviceToDatabase(device);
        } catch (const std::exception& e) {
            logger.Error("Exception saving device to database: " + std::string(e.what()));
            UpdateStatistics("database_errors", false);
            return;
        }
        
        if (saved) {
            UpdateStatistics("devices_saved", true);
            logger.Info("Device saved to database: " + device.name);
        } else {
            UpdateStatistics("database_errors", false);
            logger.Error("Failed to save device to database: " + device.name);
            return;
        }
        
        // 동적 Discovery가 활성화된 경우 자동으로 Worker 생성
        if (is_discovery_active_.load()) {
            CreateAndStartWorkerForNewDevice(device);
        }
        
    } catch (const std::exception& e) {
        UpdateStatistics("database_errors", false);
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnDeviceDiscovered: " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(const DataPoint& object) {
    try {
        UpdateStatistics("objects_processed", true);
        
        auto& logger = LogManager::getInstance();
        logger.Debug("BACnet object discovered: " + object.name + " (ID: " + object.id + ")");
        
        std::vector<DataPoint> objects = {object};
        uint32_t device_id = 260001;  // 기본값
        
        try {
            if (!object.device_id.empty()) {
                device_id = std::stoul(object.device_id);
            }
        } catch (const std::exception& e) {
            logger.Warn("Invalid device_id in DataPoint, using default: " + object.device_id);
        }
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            UpdateStatistics("objects_saved", true);
            logger.Info("Object saved to database: " + object.name);
        } else {
            UpdateStatistics("database_errors", false);
            logger.Error("Failed to save object to database: " + object.name);
        }
        
    } catch (const std::exception& e) {
        UpdateStatistics("database_errors", false);
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (single): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        UpdateStatistics("objects_processed", objects.size());
        
        auto& logger = LogManager::getInstance();
        logger.Debug("BACnet objects discovered for device " + std::to_string(device_id) + 
                     ": " + std::to_string(objects.size()) + " objects");
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            UpdateStatistics("objects_saved", objects.size());
            logger.Info("Objects saved to database for device " + std::to_string(device_id));
        } else {
            UpdateStatistics("database_errors", false);
            logger.Error("Failed to save objects to database for device " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        UpdateStatistics("database_errors", false);
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (multiple): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, const TimestampedValue& value) {
    try {
        UpdateStatistics("values_processed", true);
        
        auto& logger = LogManager::getInstance();
        logger.Debug("BACnet value changed for object: " + object_id);
        
        if (UpdateCurrentValueInDatabase(object_id, value)) {
            UpdateStatistics("values_saved", true);
        } else {
            UpdateStatistics("database_errors", false);
            logger.Warn("Failed to update current value for object: " + object_id);
        }
        
    } catch (const std::exception& e) {
        UpdateStatistics("database_errors", false);
        
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
        logger.Info("Creating and starting worker for new device: " + device_info.name);
        
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
        
        logger.Info("Successfully created and started worker for device: " + device_info.name);
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
    std::vector<std::string> failed_device_ids;
    
    // 실패한 Worker ID 수집
    {
        std::lock_guard<std::mutex> lock(managed_workers_mutex_);
        for (const auto& [device_id, managed_worker] : managed_workers_) {
            if (managed_worker->is_failed) {
                failed_device_ids.push_back(device_id);
            }
        }
    }
    
    // 개별적으로 정리 (데드락 방지)
    for (const auto& device_id : failed_device_ids) {
        try {
            auto& logger = LogManager::getInstance();
            logger.Info("Cleaning up failed worker for device: " + device_id);
            RemoveWorkerForDevice(device_id);
        } catch (const std::exception& e) {
            auto& logger = LogManager::getInstance();
            logger.Error("Failed to cleanup worker " + device_id + ": " + std::string(e.what()));
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
// UpdateStatistics 오버로드 추가
// =============================================================================

void BACnetDiscoveryService::UpdateStatistics(const std::string& operation, size_t count) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (operation == "objects_processed") {
            statistics_.objects_processed += count;
        } else if (operation == "objects_saved") {
            statistics_.objects_saved += count;
        } else if (operation == "values_processed") {
            statistics_.values_processed += count;
        } else if (operation == "values_saved") {
            statistics_.values_saved += count;
        }
        
        statistics_.last_activity = std::chrono::system_clock::now();
        
    } catch (const std::exception& e) {
        try {
            auto& logger = LogManager::getInstance();
            logger.Warn("Failed to update statistics: " + std::string(e.what()));
        } catch (...) {
            // 로깅 실패 시 무시
        }
    }
}


// =============================================================================
// 기존 레거시 호환 메서드들
// =============================================================================

bool BACnetDiscoveryService::RegisterToWorker(std::shared_ptr<BACnetWorker> worker) {
    try {
        if (!worker) {
            auto& logger = LogManager::getInstance();
            logger.Error("BACnetDiscoveryService::RegisterToWorker - Worker is null");
            return false;
        }
        
        registered_worker_ = worker;
        is_active_ = true;
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnetDiscoveryService registered to worker successfully (legacy mode)");
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in RegisterToWorker: " + std::string(e.what()));
        return false;
    }
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    try {
        if (auto worker = registered_worker_.lock()) {
            // 콜백 제거 로직이 있다면 여기에 추가
        }
        
        registered_worker_.reset();
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnetDiscoveryService unregistered from worker");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in UnregisterFromWorker: " + std::string(e.what()));
    }
}

BACnetDiscoveryService::Statistics BACnetDiscoveryService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

bool BACnetDiscoveryService::IsActive() const {
    return is_active_.load();
}

void BACnetDiscoveryService::ResetStatistics() {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_ = Statistics{};
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnetDiscoveryService statistics reset");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in ResetStatistics: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 Repository 의존성 주입
// =============================================================================


void BACnetDiscoveryService::SetDeviceSettingsRepository(
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo) {
    try {
        device_settings_repository_ = device_settings_repo;
        
        auto& logger = LogManager::getInstance();
        logger.Info("DeviceSettingsRepository injected into BACnetDiscoveryService");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in SetDeviceSettingsRepository: " + std::string(e.what()));
    }
}




// =============================================================================
// 🔥 네트워크 스캔 기능 (선택사항)
// =============================================================================

bool BACnetDiscoveryService::StartNetworkScan(const std::string& network_range) {
    try {
        if (is_network_scan_active_.load()) {
            return false;
        }
        
        is_network_scan_active_ = true;
        network_scan_running_ = true;
        
        // 네트워크 스캔 스레드 시작
        network_scan_thread_ = std::make_unique<std::thread>([this, network_range]() {
            try {
                auto& logger = LogManager::getInstance();
                logger.Info("Network scan started for range: " + 
                           (network_range.empty() ? "auto-detect" : network_range));
                
                while (network_scan_running_.load()) {
                    try {
                        // TODO: 실제 BACnet 네트워크 스캔 구현
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                    } catch (const std::exception& e) {
                        logger.Error("Exception in network scan loop: " + std::string(e.what()));
                        break;
                    }
                }
                
                logger.Info("Network scan stopped");
                
            } catch (const std::exception& e) {
                try {
                    auto& logger = LogManager::getInstance();
                    logger.Error("Exception in network scan thread: " + std::string(e.what()));
                } catch (...) {
                    // 로깅 실패 시 무시
                }
            }
        });
        
        auto& logger = LogManager::getInstance();
        logger.Info("BACnet network scan started");
        return true;
        
    } catch (const std::exception& e) {
        is_network_scan_active_ = false;
        network_scan_running_ = false;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception starting network scan: " + std::string(e.what()));
        return false;
    }
}

void BACnetDiscoveryService::StopNetworkScan() {
    try {
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
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Exception stopping network scan: " + std::string(e.what()));
    }
}

bool BACnetDiscoveryService::IsNetworkScanActive() const {
    return is_network_scan_active_.load();
}

// =============================================================================
// 기존 데이터베이스 저장 메서드들 (동일하게 유지)
// =============================================================================

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(const DeviceInfo& device) {
    try {
        if (!device_repository_) {
            return false;
        }
        
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
        
        // 수정됨: setProtocolType → setProtocolId + ProtocolRepository 조회
        int protocol_id = ResolveProtocolTypeToId(device.protocol_type);
        if (protocol_id > 0) {
            entity.setProtocolId(protocol_id);
        } else {
            auto& logger = LogManager::getInstance();
            logger.Error("Failed to resolve protocol for device " + device.id + 
                        ", protocol_type: " + device.protocol_type);
            return false;  // 프로토콜 해결 실패 시 저장 실패
        }
        
        entity.setEndpoint(device.endpoint);
        entity.setEnabled(device.is_enabled);
        
        // properties를 JSON으로 변환
        json properties_json;
        for (const auto& [key, value] : device.properties) {
            if (!key.empty()) {
                properties_json[key] = value;
            }
        }
        entity.setConfig(properties_json.dump());
        
        return device_repository_->save(entity);
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredDeviceToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        if (!datapoint_repository_) {
            return false;
        }
        
        (void)device_id;
        bool all_success = true;
        
        for (const auto& object : objects) {
            try {
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
                
            } catch (const std::exception& e) {
                auto& logger = LogManager::getInstance();
                logger.Error("Failed to save individual object " + object.id + ": " + std::string(e.what()));
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
        if (!device_repository_) {
            return -1;
        }
        
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
    try {
        auto obj_type_it = object.protocol_params.find("bacnet_object_type");
        auto obj_inst_it = object.protocol_params.find("bacnet_instance");
        
        if (obj_type_it != object.protocol_params.end() && 
            obj_inst_it != object.protocol_params.end()) {
            return std::to_string(device_id) + ":" + 
                   obj_type_it->second + ":" + 
                   obj_inst_it->second;
        }
        
        return std::to_string(device_id) + ":" + std::to_string(object.address);
        
    } catch (const std::exception& e) {
        HandleError("GenerateDataPointId", e.what());
        return std::to_string(device_id) + ":unknown";
    }
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
    try {
        auto& logger = LogManager::getInstance();
        logger.Error("BACnetDiscoveryService::" + context + " - " + error);
    } catch (...) {
        // 로깅 실패 시 무시
    }
}


int BACnetDiscoveryService::GuessObjectTypeFromName(const std::string& object_name) {
    try {
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
        
        return 2;  // 기본값
        
    } catch (const std::exception&) {
        return 2;  // 예외 시 기본값
    }
}

std::string BACnetDiscoveryService::BACnetAddressToString(const BACNET_ADDRESS& address) {
    try {
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
        
    } catch (const std::exception&) {
        return "unknown_address";
    }
}

} // namespace Workers
} // namespace PulseOne