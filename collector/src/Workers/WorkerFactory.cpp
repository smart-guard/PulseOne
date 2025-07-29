/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 헤더에서 제거된 include들 여기서 처리
 */

#include "Workers/WorkerFactory.h"

// 🔥 실제 헤더들은 cpp 파일에서만 include (헤더 의존성 차단)
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/MQTTWorker.h"
#include "Workers/Protocol/BACnetWorker.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Common/UnifiedCommonTypes.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Common/Constants.h"

#include <sstream>
#include <algorithm>
#include <regex>

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

// =============================================================================
// 싱글톤 패턴 구현
// =============================================================================

WorkerFactory& WorkerFactory::getInstance() {
    static WorkerFactory instance;
    return instance;
}

bool WorkerFactory::Initialize() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // 기존 싱글톤들 활용
        logger_ = &PulseOne::LogManager::getInstance();
        config_manager_ = &ConfigManager::getInstance();
        
        logger_->Info("🔧 WorkerFactory initialization started");
        
        // Worker 생성 함수들 등록
        RegisterWorkerCreators();
        
        // 팩토리 시작 시간 기록
        factory_start_time_ = system_clock::now();
        
        initialized_ = true;
        
        logger_->Info("✅ WorkerFactory initialized successfully");
        logger_->Info("   Supported protocols: " + std::to_string(worker_creators_.size()));
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ WorkerFactory initialization failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 의존성 주입 메서드들
// =============================================================================

void WorkerFactory::SetDeviceRepository(std::shared_ptr<PulseOne::Database::Repositories::DeviceRepository> device_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    device_repo_ = device_repo;
    logger_->Info("✅ DeviceRepository injected into WorkerFactory");
}

void WorkerFactory::SetDataPointRepository(std::shared_ptr<PulseOne::Database::Repositories::DataPointRepository> datapoint_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    datapoint_repo_ = datapoint_repo;
    logger_->Info("✅ DataPointRepository injected into WorkerFactory");
}

void WorkerFactory::SetDatabaseClients(
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client) {
    
    std::lock_guard<std::mutex> lock(factory_mutex_);
    redis_client_ = redis_client;
    influx_client_ = influx_client;
    logger_->Info("✅ Database clients injected into WorkerFactory");
}

// =============================================================================
// Worker 생성 메서드들
// =============================================================================

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorker(const PulseOne::Database::Entities::DeviceEntity& device_entity) {
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
        
        // 4. DeviceEntity → DeviceInfo 변환 (기존 구조 활용)
        PulseOne::DeviceInfo device_info = ConvertToDeviceInfo(device_entity);
        
        // 5. 해당 디바이스의 DataPoint들 로드
        std::vector<PulseOne::DataPoint> data_points = LoadDataPointsForDevice(device_entity.getId());
        logger_->Debug("   Loaded " + std::to_string(data_points.size()) + " data points");
        
        // 6. Worker 생성 (프로토콜별)
        std::lock_guard<std::mutex> lock(factory_mutex_);
        auto creator_it = worker_creators_.find(protocol_type);
        if (creator_it == worker_creators_.end()) {
            logger_->Error("No creator found for protocol: " + protocol_type);
            creation_failures_++;
            return nullptr;
        }
        
        auto worker = creator_it->second(device_info, redis_client_, influx_client_);
        if (!worker) {
            logger_->Error("Worker creation failed for protocol: " + protocol_type);
            creation_failures_++;
            return nullptr;
        }
        
        // 7. DataPoint들을 Worker에 추가 (기존 BaseDeviceWorker API 활용)
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
        // DeviceRepository 활용 (기존 구조)
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

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateAllActiveWorkers(int tenant_id) {
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("DeviceRepository not set");
        return workers;
    }
    
    try {
        logger_->Info("🏭 Creating workers for all active devices (tenant: " + std::to_string(tenant_id) + ")");
        
        // 모든 활성 디바이스 조회 (기존 Repository API 활용)
        auto devices = device_repo_->findAllActive(tenant_id);
        
        logger_->Info("   Found " + std::to_string(devices.size()) + " active devices");
        
        // 각 디바이스별 Worker 생성
        for (const auto& device : devices) {
            auto worker = CreateWorker(device);
            if (worker) {
                workers.push_back(std::move(worker));
            }
        }
        
        logger_->Info("✅ Created " + std::to_string(workers.size()) + "/" + 
                     std::to_string(devices.size()) + " workers successfully");
        
        return workers;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in CreateAllActiveWorkers: " + std::string(e.what()));
        return workers;
    }
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateWorkersByProtocol(
    const std::string& protocol_type, 
    int tenant_id) {
    
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("DeviceRepository not set");
        return workers;
    }
    
    try {
        logger_->Info("🏭 Creating workers for protocol: " + protocol_type + 
                     " (tenant: " + std::to_string(tenant_id) + ")");
        
        // 프로토콜별 활성 디바이스 조회 (기존 Repository API 활용)
        auto devices = device_repo_->findByProtocol(protocol_type, tenant_id);
        
        logger_->Info("   Found " + std::to_string(devices.size()) + " devices for protocol: " + protocol_type);
        
        // 각 디바이스별 Worker 생성
        for (const auto& device : devices) {
            if (device.getIsEnabled()) {  // 활성 디바이스만
                auto worker = CreateWorker(device);
                if (worker) {
                    workers.push_back(std::move(worker));
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
// 내부 초기화 메서드들
// =============================================================================

void WorkerFactory::RegisterWorkerCreators() {
    logger_->Debug("Registering worker creators...");
    
    // ModbusTCP Worker 등록 (기존 클래스 활용)
    RegisterWorkerCreator("MODBUS_TCP", 
        [](const PulseOne::DeviceInfo& device_info,
           std::shared_ptr<RedisClient> redis_client,
           std::shared_ptr<InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<ModbusTcpWorker>(device_info, redis_client, influx_client);
        });
    
    // MQTT Worker 등록 (기존 클래스 활용)
    RegisterWorkerCreator("MQTT", 
        [](const PulseOne::DeviceInfo& device_info,
           std::shared_ptr<RedisClient> redis_client,
           std::shared_ptr<InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<MQTTWorker>(device_info, redis_client, influx_client);
        });
    
    // BACnet Worker 등록 (기존 클래스 활용)
    RegisterWorkerCreator("BACNET", 
        [](const PulseOne::DeviceInfo& device_info,
           std::shared_ptr<RedisClient> redis_client,
           std::shared_ptr<InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<BACnetWorker>(device_info, redis_client, influx_client);
        });
    
    // Modbus RTU Worker 등록 (향후 확장용)
    RegisterWorkerCreator("MODBUS_RTU", 
        [](const PulseOne::DeviceInfo& device_info,
           std::shared_ptr<RedisClient> redis_client,
           std::shared_ptr<InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
            // 현재는 ModbusTcp를 재사용 (향후 별도 구현 가능)
            return std::make_unique<ModbusTcpWorker>(device_info, redis_client, influx_client);
        });
    
    logger_->Info("✅ Registered " + std::to_string(worker_creators_.size()) + " worker creators");
}

void WorkerFactory::RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    worker_creators_[protocol_type] = creator;
    logger_->Debug("Worker creator registered for protocol: " + protocol_type);
}

std::string WorkerFactory::ValidateWorkerConfig(const PulseOne::Database::Entities::DeviceEntity& device_entity) const {
    // 기본 유효성 검사
    if (device_entity.getName().empty()) {
        return "Device name is empty";
    }
    
    if (device_entity.getEndpoint().empty()) {
        return "Device endpoint is empty";
    }
    
    if (device_entity.getProtocolType().empty()) {
        return "Protocol type is empty";
    }
    
    if (device_entity.getPollingInterval() <= 0) {
        return "Invalid polling interval: " + std::to_string(device_entity.getPollingInterval());
    }
    
    if (device_entity.getTimeout() <= 0) {
        return "Invalid timeout: " + std::to_string(device_entity.getTimeout());
    }
    
    // 프로토콜별 특수 검증 (간단히)
    std::string protocol = device_entity.getProtocolType();
    if (protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU") {
        // Modbus 엔드포인트 검증 (IP:Port 형식)
        std::regex modbus_regex(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{1,5})");
        if (!std::regex_match(device_entity.getEndpoint(), modbus_regex)) {
            return "Invalid Modbus endpoint format. Expected IP:Port";
        }
    } else if (protocol == "MQTT") {
        // MQTT 브로커 URL 검증
        if (device_entity.getEndpoint().find("mqtt://") != 0 && 
            device_entity.getEndpoint().find("mqtts://") != 0) {
            return "Invalid MQTT endpoint. Expected mqtt:// or mqtts:// URL";
        }
    }
    
    return "";  // 검증 통과
}

// =============================================================================
// 변환 메서드들 (Entity → 기존 구조체)
// =============================================================================

PulseOne::DeviceInfo WorkerFactory::ConvertToDeviceInfo(const PulseOne::Database::Entities::DeviceEntity& device_entity) const {
    PulseOne::DeviceInfo device_info;
    
    // 기본 정보 매핑 (DeviceEntity → DeviceInfo)
    device_info.id = std::to_string(device_entity.getId());
    device_info.name = device_entity.getName();
    device_info.description = device_entity.getDescription();
    device_info.protocol_type = device_entity.getProtocolType();
    device_info.endpoint = device_entity.getEndpoint();
    device_info.is_enabled = device_entity.getIsEnabled();
    
    // 통신 설정
    device_info.polling_interval_ms = device_entity.getPollingInterval();
    device_info.timeout_ms = device_entity.getTimeout();
    device_info.retry_count = device_entity.getRetryCount();
    
    // 메타데이터
    device_info.tags = device_entity.getTags();
    device_info.metadata = device_entity.getMetadata();
    
    // 시간 정보
    device_info.created_at = device_entity.getCreatedAt();
    device_info.updated_at = device_entity.getUpdatedAt();
    
    return device_info;
}

std::vector<PulseOne::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::DataPoint> data_points;
    
    if (!datapoint_repo_) {
        logger_->Error("DataPointRepository not set");
        return data_points;
    }
    
    try {
        // DataPointRepository 활용 (기존 구조)
        auto datapoint_entities = datapoint_repo_->findByDeviceId(device_id);
        
        // DataPointEntity → DataPoint 변환 (기존 메서드 활용)
        for (const auto& entity : datapoint_entities) {
            if (entity.getIsEnabled()) {  // 활성 데이터포인트만
                PulseOne::DataPoint dp = entity.toDataPointStruct();  // 기존 메서드 활용!
                data_points.push_back(dp);
            }
        }
        
        logger_->Debug("Loaded " + std::to_string(data_points.size()) + "/" + 
                       std::to_string(datapoint_entities.size()) + " active data points for device: " + 
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

} // namespace Workers
} // namespace PulseOne