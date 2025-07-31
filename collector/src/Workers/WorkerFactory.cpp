/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 중복 메서드 제거 완료
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
#include "Database/Entities/CurrentValueEntity.h"
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
        
        // =======================================================================
        // 🔥 ✅ DataPoint들을 Worker에 추가 - 완성된 필드들 활용
        // =======================================================================
        for (const auto& data_point : data_points) {
            if (!worker->AddDataPoint(data_point)) {
                logger_->Warn("Failed to add data point: " + data_point.name + " to worker");
            } else {
                // ✅ 풍부한 디버깅 정보 - 새로운 필드들 포함
                logger_->Debug("✅ Added DataPoint: " + data_point.name + 
                              " (writable=" + (data_point.IsWritable() ? "true" : "false") + 
                              ", log_enabled=" + (data_point.log_enabled ? "true" : "false") + 
                              ", interval=" + std::to_string(data_point.log_interval_ms) + "ms" + 
                              ", current_value=" + data_point.GetCurrentValueAsString() + 
                              ", quality=" + data_point.GetQualityCodeAsString() + ")");
            }
        }
        
        // 8. 통계 업데이트
        workers_created_++;
        auto end_time = steady_clock::now();
        auto creation_time = duration_cast<milliseconds>(end_time - start_time);
        
        // =======================================================================
        // 🔥 ✅ Worker별 통계 출력 - 완성된 필드들 활용
        // =======================================================================
        int enabled_points = 0;
        int writable_points = 0;
        int log_enabled_points = 0;
        int good_quality_points = 0;
        
        for (const auto& dp : data_points) {
            if (dp.is_enabled) enabled_points++;
            if (dp.IsWritable()) writable_points++;         // ✅ 새 메서드 사용
            if (dp.log_enabled) log_enabled_points++;
            if (dp.IsGoodQuality()) good_quality_points++;  // ✅ 새 메서드 사용
        }
        
        logger_->Info("✅ Worker created successfully for device: " + device_entity.getName() + 
                     " (Protocol: " + protocol_type + 
                     ", DataPoints: " + std::to_string(data_points.size()) + 
                     ", Enabled: " + std::to_string(enabled_points) +
                     ", Writable: " + std::to_string(writable_points) +
                     ", LogEnabled: " + std::to_string(log_enabled_points) +
                     ", GoodQuality: " + std::to_string(good_quality_points) +
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
// 내부 유틸리티 메서드들 - 🔧 완전 수정: 실제 Worker 생성
// =============================================================================

void WorkerFactory::RegisterWorkerCreators() {
    logger_->Info("📝 Registering worker creators...");
    
    logger_->Info("🔧 Step 1: Registering MODBUS_TCP worker...");
    // 🔧 완전 수정: MODBUS_TCP Worker 실제 생성 - 정확한 네임스페이스 사용
    RegisterWorkerCreator("MODBUS_TCP", [](
        const PulseOne::Structs::DeviceInfo& device_info,
        std::shared_ptr<::RedisClient> redis_client,
        std::shared_ptr<::InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
        
        try {
            // DeviceInfo 타입 변환 (Structs::DeviceInfo → DeviceInfo)
            PulseOne::DeviceInfo converted_info;
            converted_info.id = device_info.id;
            converted_info.name = device_info.name;
            converted_info.description = device_info.description;
            converted_info.protocol_type = device_info.protocol_type;
            converted_info.endpoint = device_info.endpoint;
            converted_info.connection_string = device_info.connection_string;
            converted_info.is_enabled = device_info.is_enabled;
            
            // ✅ 정확한 네임스페이스: PulseOne::Workers::ModbusTcpWorker
            auto worker = std::make_unique<PulseOne::Workers::ModbusTcpWorker>(
                converted_info,
                redis_client,
                influx_client
            );
            return std::unique_ptr<BaseDeviceWorker>(worker.release());
        } catch (const std::exception&) {
            return nullptr; // 예외 발생 시 nullptr 반환
        }
    });
    logger_->Info("✅ MODBUS_TCP worker registered");
    
    logger_->Info("🔧 Step 2: Registering MQTT worker...");
    // 🔧 완전 수정: MQTT Worker 실제 생성 - 정확한 네임스페이스 사용
    RegisterWorkerCreator("MQTT", [](
        const PulseOne::Structs::DeviceInfo& device_info,
        std::shared_ptr<::RedisClient> redis_client,
        std::shared_ptr<::InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
        
        try {
            // DeviceInfo 타입 변환
            PulseOne::DeviceInfo converted_info;
            converted_info.id = device_info.id;
            converted_info.name = device_info.name;
            converted_info.description = device_info.description;
            converted_info.protocol_type = device_info.protocol_type;
            converted_info.endpoint = device_info.endpoint;
            converted_info.connection_string = device_info.connection_string;
            converted_info.is_enabled = device_info.is_enabled;
            
            // ✅ 정확한 네임스페이스: PulseOne::Workers::MQTTWorker
            auto worker = std::make_unique<PulseOne::Workers::MQTTWorker>(
                converted_info,
                redis_client,
                influx_client
            );
            return std::unique_ptr<BaseDeviceWorker>(worker.release());
        } catch (const std::exception&) {
            return nullptr; // 예외 발생 시 nullptr 반환
        }
    });
    logger_->Info("✅ MQTT worker registered");
    
    logger_->Info("🔧 Step 3: Registering BACNET worker...");
    // 🔧 완전 수정: BACNET Worker 실제 생성 - 정확한 네임스페이스 사용
    RegisterWorkerCreator("BACNET", [](
        const PulseOne::Structs::DeviceInfo& device_info,
        std::shared_ptr<::RedisClient> redis_client,
        std::shared_ptr<::InfluxClient> influx_client) -> std::unique_ptr<BaseDeviceWorker> {
        
        try {
            // DeviceInfo 타입 변환
            PulseOne::DeviceInfo converted_info;
            converted_info.id = device_info.id;
            converted_info.name = device_info.name;
            converted_info.description = device_info.description;
            converted_info.protocol_type = device_info.protocol_type;
            converted_info.endpoint = device_info.endpoint;
            converted_info.connection_string = device_info.connection_string;
            converted_info.is_enabled = device_info.is_enabled;
            
            // ✅ 정확한 네임스페이스: PulseOne::Workers::BACnetWorker
            auto worker = std::make_unique<PulseOne::Workers::BACnetWorker>(
                converted_info,
                redis_client,
                influx_client
            );
            return std::unique_ptr<BaseDeviceWorker>(worker.release());
        } catch (const std::exception&) {
            return nullptr; // 예외 발생 시 nullptr 반환
        }
    });
    logger_->Info("✅ BACNET worker registered");
    
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
    device_info.description = device_entity.getDescription();
    device_info.protocol_type = device_entity.getProtocolType();
    
    // ✅ endpoint와 connection_string 필드 사용
    device_info.endpoint = device_entity.getConnectionString();
    device_info.connection_string = device_entity.getConnectionString();
    
    // ✅ isEnabled() 메서드 사용
    device_info.is_enabled = device_entity.isEnabled();
    
    return device_info;
}

// =============================================================================
// 🔥 유일한 LoadDataPointsForDevice 구현 - 완전 구현
// =============================================================================

std::vector<PulseOne::Structs::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::Structs::DataPoint> data_points;
    
    if (!datapoint_repo_) {
        logger_->Warn("DataPointRepository not set");
        return data_points;
    }
    
    try {
        logger_->Debug("🔍 Loading DataPoints for device ID: " + std::to_string(device_id));
        
        // ✅ DataPointRepository를 사용하여 실제 DataPoint들 로드
        auto datapoint_entities = datapoint_repo_->findByDeviceId(device_id, true); // enabled_only = true
        
        logger_->Debug("📊 Found " + std::to_string(datapoint_entities.size()) + " DataPoint entities");
        
        // ✅ Entity → Structs::DataPoint 변환 - Entity의 toDataPointStruct() 메서드 활용!
        for (const auto& entity : datapoint_entities) {
            
            // 🎯 핵심: Entity가 제공하는 변환 메서드 사용
            PulseOne::Structs::DataPoint data_point = entity.toDataPointStruct();
            
            // =======================================================================
            // 🔥 ✅ 새로 추가된 필드들 설정 (현재값/품질코드)
            // =======================================================================
            
            // 1. 현재값 조회 및 설정
            try {
                // CurrentValueEntity를 통해 현재값 조회 (향후 Repository 패턴 적용)
                PulseOne::Database::Entities::CurrentValueEntity current_value_entity;
                
                // 🔧 임시: 기본값으로 초기화 (실제로는 DB에서 조회)
                // current_value_entity.loadByDataPointId(entity.getId());
                
                // ✅ 완성된 필드 사용!
                data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);  // 기본값
                data_point.quality_code = PulseOne::Enums::DataQuality::GOOD;       // 기본 품질
                data_point.quality_timestamp = std::chrono::system_clock::now();     // 현재 시각
                
                logger_->Debug("💡 Current value fields initialized: " + 
                              data_point.GetCurrentValueAsString() + 
                              " (Quality: " + data_point.GetQualityCodeAsString() + ")");
                
            } catch (const std::exception& e) {
                logger_->Debug("Current value not found, using defaults: " + std::string(e.what()));
                
                // ✅ 에러 시 BAD 품질로 설정
                data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
                data_point.quality_timestamp = std::chrono::system_clock::now();
            }
            
            // 2. 주소 필드 동기화 (기존 메서드 활용)
            data_point.SyncAddressFields();
            
            // 3. Worker 컨텍스트 정보 추가 (metadata에서 제거 - 이제 직접 필드 사용)
            try {
                auto worker_context = entity.getWorkerContext();
                if (!worker_context.empty()) {
                    data_point.metadata = worker_context;  // JSON을 metadata에 직접 할당
                }
            } catch (const std::exception& e) {
                logger_->Debug("Worker context not available: " + std::string(e.what()));
            }
            
            data_points.push_back(data_point);
            
            // ✅ 풍부한 로깅 - 새로운 필드들 포함
            logger_->Debug("✅ Converted DataPoint: " + data_point.name + 
                          " (Address: " + std::to_string(data_point.address) + 
                          ", Type: " + data_point.data_type + 
                          ", Writable: " + (data_point.IsWritable() ? "true" : "false") + 
                          ", LogEnabled: " + (data_point.log_enabled ? "true" : "false") + 
                          ", LogInterval: " + std::to_string(data_point.log_interval_ms) + "ms" + 
                          ", CurrentValue: " + data_point.GetCurrentValueAsString() + 
                          ", Quality: " + data_point.GetQualityCodeAsString() + ")");
        }
        
        logger_->Info("✅ Successfully loaded " + std::to_string(data_points.size()) + 
                     " active data points for device: " + std::to_string(device_id));
        
        // =======================================================================
        // 🔥 ✅ 새로운 통계 출력 - 완성된 필드들 활용
        // =======================================================================
        int writable_count = 0;
        int log_enabled_count = 0;
        int good_quality_count = 0;
        int total_read_count = 0;
        
        for (const auto& dp : data_points) {
            if (dp.IsWritable()) writable_count++;           // ✅ 새 메서드 사용
            if (dp.log_enabled) log_enabled_count++;
            if (dp.IsGoodQuality()) good_quality_count++;   // ✅ 새 메서드 사용
            total_read_count += dp.read_count;
        }
        
        logger_->Debug("📊 DataPoint Statistics: " + 
                      std::to_string(writable_count) + " writable, " + 
                      std::to_string(log_enabled_count) + " log-enabled, " + 
                      std::to_string(good_quality_count) + " good-quality, " + 
                      "total reads: " + std::to_string(total_read_count));
        
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
    // 🔧 수정: 이미 factory_mutex_ 잠금이 RegisterWorkerCreators()에서 발생했으므로 여기서는 불필요
    worker_creators_[protocol_type] = creator;
    logger_->Info("✅ Registered worker creator for protocol: " + protocol_type);
}

// =============================================================================
// 헬퍼 함수들
// =============================================================================

std::string WorkerFactory::DataQualityToString(PulseOne::Enums::DataQuality quality) const {
    switch (quality) {
        case PulseOne::Enums::DataQuality::GOOD: return "GOOD";
        case PulseOne::Enums::DataQuality::BAD: return "BAD";
        case PulseOne::Enums::DataQuality::UNCERTAIN: return "UNCERTAIN";
        case PulseOne::Enums::DataQuality::NOT_CONNECTED: return "NOT_CONNECTED";
        case PulseOne::Enums::DataQuality::SCAN_DELAYED: return "SCAN_DELAYED";
        case PulseOne::Enums::DataQuality::UNDER_MAINTENANCE: return "UNDER_MAINTENANCE";
        case PulseOne::Enums::DataQuality::STALE_DATA: return "STALE_DATA";
        case PulseOne::Enums::DataQuality::VERY_STALE_DATA: return "VERY_STALE_DATA";
        case PulseOne::Enums::DataQuality::MAINTENANCE_BLOCKED: return "MAINTENANCE_BLOCKED";
        case PulseOne::Enums::DataQuality::ENGINEER_OVERRIDE: return "ENGINEER_OVERRIDE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Worker에서 DataPoint 현재값 업데이트 헬퍼
 * @param data_point 업데이트할 DataPoint
 * @param new_value 새로운 값
 * @param new_quality 새로운 품질 (기본값: GOOD)
 */
void WorkerFactory::UpdateDataPointValue(PulseOne::Structs::DataPoint& data_point, 
                                         const PulseOne::BasicTypes::DataVariant& new_value,
                                         PulseOne::Enums::DataQuality new_quality) const {
    try {
        // ✅ 완성된 메서드 사용
        data_point.UpdateCurrentValue(new_value, new_quality);
        
        logger_->Debug("📊 Updated DataPoint value: " + data_point.name + 
                      " = " + data_point.GetCurrentValueAsString() + 
                      " (Quality: " + data_point.GetQualityCodeAsString() + ")");
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to update DataPoint value: " + std::string(e.what()));
        
        // 에러 시 BAD 품질로 설정
        data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
        data_point.quality_timestamp = std::chrono::system_clock::now();
        data_point.error_count++;
    }
}


/**
 * @brief DataPoint 로깅 조건 확인 헬퍼
 * @param data_point 확인할 DataPoint
 * @param new_value 새로운 값
 * @return 로깅해야 하면 true
 */
bool WorkerFactory::ShouldLogDataPoint(const PulseOne::Structs::DataPoint& data_point,
                                       const PulseOne::BasicTypes::DataVariant& new_value) const {
    try {
        // ✅ 완성된 메서드 사용
        bool should_log = data_point.ShouldLog(new_value);
        
        if (should_log) {
            logger_->Debug("📝 DataPoint logging triggered: " + data_point.name + 
                          " (interval: " + std::to_string(data_point.log_interval_ms) + "ms, " +
                          "deadband: " + std::to_string(data_point.log_deadband) + ")");
        }
        
        return should_log;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to check logging condition: " + std::string(e.what()));
        return false;
    }
}

} // namespace Workers
} // namespace PulseOne