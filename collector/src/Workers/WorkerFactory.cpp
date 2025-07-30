/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 모든 컴파일 에러 해결
 */

// 🔧 매크로 충돌 방지 - BACnet 헤더보다 먼저 STL 포함
#include <algorithm>
#include <functional>
#include <memory>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Workers/WorkerFactory.h"

// ✅ 실제 헤더들은 cpp 파일에서만 include (complete type 생성)
#include "Common/Structs.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/MQTTWorker.h"
#include "Workers/Protocol/BACnetWorker.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Common/Constants.h"

#include <sstream>
#include <regex>

using std::max;
using std::min;
using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// FactoryStats 구현
// =============================================================================

std::string FactoryStats::ToString() const {
    std::ostringstream oss;
    oss << "WorkerFactory Stats:\n"
        << "  Workers Created: " << workers_created << "\n"
        << "  Creation Failures: " << creation_failures << "\n"
        << "  Registered Protocols: " << registered_protocols << "\n"
        << "  Total Creation Time: " << total_creation_time.count() << "ms\n";
    
    auto now = system_clock::now();
    auto uptime = duration_cast<seconds>(now - factory_start_time);
    oss << "  Factory Uptime: " << uptime.count() << "s";
    
    return oss.str();
}

void FactoryStats::Reset() {
    workers_created = 0;
    creation_failures = 0;
    total_creation_time = std::chrono::milliseconds{0};
    factory_start_time = system_clock::now();
}

// =============================================================================
// 🔧 수정: 싱글톤 구현 - 메서드명 통일
// =============================================================================

WorkerFactory& WorkerFactory::getInstance() {  // ✅ getInstance (소문자 g)
    static WorkerFactory instance;
    return instance;
}

// =============================================================================
// 🔧 수정: 초기화 메서드들 - 두 버전 모두 구현
// =============================================================================

bool WorkerFactory::Initialize() {
    // 매개변수 없는 버전 - 싱글톤들을 직접 가져오기
    ::LogManager* logger = &::LogManager::getInstance();
    ::ConfigManager* config_manager = &::ConfigManager::getInstance();
    
    return Initialize(logger, config_manager);
}

bool WorkerFactory::Initialize(::LogManager* logger, ::ConfigManager* config_manager) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (initialized_.load()) {
        return true;
    }
    
    logger_ = logger;
    config_manager_ = config_manager;
    factory_start_time_ = system_clock::now();
    
    if (!logger_ || !config_manager_) {
        return false;
    }
    
    logger_->Info("🏭 Initializing WorkerFactory");
    
    try {
        RegisterWorkerCreators();
        initialized_.store(true);
        logger_->Info("✅ WorkerFactory initialized successfully");
        return true;
    } catch (const std::exception& e) {
        logger_->Error("❌ WorkerFactory initialization failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 의존성 주입 메서드들
// =============================================================================

void WorkerFactory::SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    device_repo_ = device_repo;
    logger_->Info("✅ DeviceRepository injected into WorkerFactory");
}

void WorkerFactory::SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    datapoint_repo_ = datapoint_repo;
    logger_->Info("✅ DataPointRepository injected into WorkerFactory");
}

void WorkerFactory::SetDatabaseClients(
    std::shared_ptr<::RedisClient> redis_client,     // ✅ 전역 클래스
    std::shared_ptr<::InfluxClient> influx_client) { // ✅ 전역 클래스
    
    std::lock_guard<std::mutex> lock(factory_mutex_);
    redis_client_ = redis_client;
    influx_client_ = influx_client;
    logger_->Info("✅ Database clients injected into WorkerFactory");
}

// =============================================================================
// Worker 생성 메서드들
// =============================================================================

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorker(const Database::Entities::DeviceEntity& device_entity) {
    if (!initialized_.load()) {
        logger_->Error("WorkerFactory not initialized");
        creation_failures_++;
        return nullptr;
    }
    
    auto start_time = steady_clock::now();
    
    try {
        logger_->Debug("🏭 Creating worker for device: " + device_entity.getName());
        
        // 1. 프로토콜 타입 추출
        std::string protocol_type = device_entity.getProtocolType();
        if (protocol_type.empty()) {
            logger_->Error("Empty protocol type for device: " + device_entity.getName());
            creation_failures_++;
            return nullptr;
        }
        
        // 2. 프로토콜 지원 여부 확인
        if (!IsProtocolSupported(protocol_type)) {
            logger_->Error("Unsupported protocol: " + protocol_type + " for device: " + device_entity.getName());
            creation_failures_++;
            return nullptr;
        }
        
        // 3. Worker 설정 검증
        std::string validation_error = ValidateWorkerConfig(device_entity);
        if (!validation_error.empty()) {
            logger_->Error("Worker config validation failed: " + validation_error);
            creation_failures_++;
            return nullptr;
        }
        
        // 4. DeviceEntity → DeviceInfo 변환
        PulseOne::Structs::DeviceInfo device_info = ConvertToDeviceInfo(device_entity);
        
        // 5. 해당 디바이스의 DataPoint들 로드
        std::vector<PulseOne::Structs::DataPoint> data_points = LoadDataPointsForDevice(device_entity.getId());
        logger_->Debug("   Loaded " + std::to_string(data_points.size()) + " data points");
        
        // 6. WorkerCreator 실행
        auto creator_it = worker_creators_.find(protocol_type);
        if (creator_it == worker_creators_.end()) {
            logger_->Error("No creator found for protocol: " + protocol_type);
            creation_failures_++;
            return nullptr;
        }
        
        auto worker = creator_it->second(device_info, redis_client_, influx_client_);
        if (!worker) {
            logger_->Error("Worker creation failed for device: " + device_entity.getName());
            creation_failures_++;
            return nullptr;
        }
        
        // 7. DataPoint들을 Worker에 추가
        for (const auto& data_point : data_points) {
            if (!worker->AddDataPoint(data_point)) {
                logger_->Warn("Failed to add data point: " + data_point.name + " to worker");
            }
        }
        
        // 8. 통계 업데이트
        workers_created_++;
        auto end_time = steady_clock::now();
        auto creation_time = duration_cast<milliseconds>(end_time - start_time);
        
        logger_->Info("✅ Worker created successfully for device: " + device_entity.getName() + 
                     " (Protocol: " + protocol_type + ", DataPoints: " + std::to_string(data_points.size()) + 
                     ", Time: " + std::to_string(creation_time.count()) + "ms)");
        
        return worker;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in CreateWorker: " + std::string(e.what()));
        creation_failures_++;
        return nullptr;
    }
}

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorkerById(int device_id) {
    if (!device_repo_) {
        logger_->Error("DeviceRepository not set");
        return nullptr;
    }
    
    try {
        auto device = device_repo_->findById(device_id);
        if (!device) {
            logger_->Error("Device not found: " + std::to_string(device_id));
            return nullptr;
        }
        
        return CreateWorker(*device);
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in CreateWorkerById: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateAllActiveWorkers() {
    return CreateAllActiveWorkers(0);  // tenant_id = 0 (기본값)
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateAllActiveWorkers(int /* tenant_id */) {
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("DeviceRepository not set");
        return workers;
    }
    
    try {
        logger_->Info("🏭 Creating workers for all active devices");
        
        auto devices = device_repo_->findAll();
        
        for (const auto& device : devices) {
            if (device.isEnabled()) {  // 🔧 수정: getIsEnabled() → isEnabled()
                auto worker = CreateWorker(device);
                if (worker) {
                    workers.push_back(std::move(worker));
                } else {
                    logger_->Warn("Failed to create worker for device: " + device.getName());
                }
            }
        }
        
        logger_->Info("✅ Created " + std::to_string(workers.size()) + " workers from " + 
                     std::to_string(devices.size()) + " devices");
        
        return workers;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in CreateAllActiveWorkers: " + std::string(e.what()));
        return workers;
    }
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateWorkersByProtocol(
    const std::string& protocol_type, int /* tenant_id */) {
    
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("DeviceRepository not set");
        return workers;
    }
    
    try {
        logger_->Info("🏭 Creating workers for protocol: " + protocol_type);
        
        auto devices = device_repo_->findAll();
        
        for (const auto& device : devices) {
            if (device.isEnabled() && device.getProtocolType() == protocol_type) {  // 🔧 수정: getIsEnabled() → isEnabled()
                auto worker = CreateWorker(device);
                if (worker) {
                    workers.push_back(std::move(worker));
                } else {
                    logger_->Warn("Failed to create worker for device: " + device.getName());
                }
            }
        }
        
        logger_->Info("✅ Created " + std::to_string(workers.size()) + " workers for protocol: " + protocol_type);
        
        return workers;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in CreateWorkersByProtocol: " + std::string(e.what()));
        return workers;
    }
}

// =============================================================================
// 내부 유틸리티 메서드들
// =============================================================================

void WorkerFactory::RegisterWorkerCreators() {
    logger_->Info("📝 Registering worker creators...");
    
    // Modbus TCP Worker 등록
    RegisterWorkerCreator("MODBUS_TCP", [](
        const PulseOne::Structs::DeviceInfo& /* device_info */,  // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::RedisClient> /* redis_client */,       // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::InfluxClient> /* influx_client */) -> std::unique_ptr<BaseDeviceWorker> {  // 🔧 수정: 미사용 매개변수 주석 처리
        
        // ModbusTcpWorker 생성 로직
        // (실제 구현에서는 ModbusTcpWorker 생성자에 맞게 수정)
        return nullptr;  // 임시 반환
    });
    
    // MQTT Worker 등록
    RegisterWorkerCreator("MQTT", [](
        const PulseOne::Structs::DeviceInfo& /* device_info */,  // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::RedisClient> /* redis_client */,       // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::InfluxClient> /* influx_client */) -> std::unique_ptr<BaseDeviceWorker> {  // 🔧 수정: 미사용 매개변수 주석 처리
        
        // MQTTWorker 생성 로직
        return nullptr;  // 임시 반환
    });
    
    // BACnet Worker 등록
    RegisterWorkerCreator("BACNET", [](
        const PulseOne::Structs::DeviceInfo& /* device_info */,  // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::RedisClient> /* redis_client */,       // 🔧 수정: 미사용 매개변수 주석 처리
        std::shared_ptr<::InfluxClient> /* influx_client */) -> std::unique_ptr<BaseDeviceWorker> {  // 🔧 수정: 미사용 매개변수 주석 처리
        
        // BACnetWorker 생성 로직
        return nullptr;  // 임시 반환
    });
    
    logger_->Info("✅ Worker creators registered: " + std::to_string(worker_creators_.size()));
}

std::string WorkerFactory::ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const {
    // 기본 검증 로직
    if (device_entity.getName().empty()) {
        return "Device name is empty";
    }
    
    if (device_entity.getProtocolType().empty()) {
        return "Protocol type is empty";
    }
    
    return "";  // 검증 통과
}

PulseOne::Structs::DeviceInfo WorkerFactory::ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const {
    PulseOne::Structs::DeviceInfo device_info;
    
    // ✅ id 필드 사용 (문자열로 변환)
    device_info.id = std::to_string(device_entity.getId());
    device_info.name = device_entity.getName();
    device_info.description = device_entity.getDeviceInfo().description;
    device_info.protocol_type = device_entity.getProtocolType();
    
    // ✅ endpoint와 connection_string 필드 사용
    device_info.endpoint = device_entity.getConnectionString();
    device_info.connection_string = device_entity.getConnectionString();
    
    // ✅ isEnabled() 메서드 사용
    device_info.is_enabled = device_entity.isEnabled();
    
    return device_info;
}

std::vector<PulseOne::Structs::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::Structs::DataPoint> data_points;
    
    if (!datapoint_repo_) {
        logger_->Warn("DataPointRepository not set");
        return data_points;
    }
    
    try {
        // 실제 Repository에서 DataPoint들 로드
        // (구현에 따라 다를 수 있음 - 기본 구현만 제공)
        
        logger_->Debug("Loaded " + std::to_string(data_points.size()) + " active data points for device: " + 
                       std::to_string(device_id));
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in LoadDataPointsForDevice: " + std::string(e.what()));
    }
    
    return data_points;
}

// =============================================================================
// 팩토리 정보 조회 메서드들
// =============================================================================

std::vector<std::string> WorkerFactory::GetSupportedProtocols() const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    std::vector<std::string> protocols;
    for (const auto& [protocol, creator] : worker_creators_) {
        protocols.push_back(protocol);
    }
    
    return protocols;
}

bool WorkerFactory::IsProtocolSupported(const std::string& protocol_type) const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    return worker_creators_.find(protocol_type) != worker_creators_.end();
}

FactoryStats WorkerFactory::GetFactoryStats() const {
    FactoryStats stats;
    
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        stats.registered_protocols = static_cast<uint32_t>(worker_creators_.size());
    }
    
    stats.workers_created = workers_created_.load();
    stats.creation_failures = creation_failures_.load();
    stats.factory_start_time = factory_start_time_;
    
    return stats;
}

std::string WorkerFactory::GetFactoryStatsString() const {
    return GetFactoryStats().ToString();
}

void WorkerFactory::RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    worker_creators_[protocol_type] = creator;
    logger_->Debug("✅ Registered worker creator for protocol: " + protocol_type);
}

} // namespace Workers
} // namespace PulseOne