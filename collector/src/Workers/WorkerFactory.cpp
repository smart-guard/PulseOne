/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 중복 메서드 제거 완료
 */

// 🔧 매크로 충돌 방지 - BACnet 헤더보다 먼저 STL 포함
#include <algorithm>
#include <functional>
#include <memory>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>



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
#include "Database/RepositoryFactory.h"    // RepositoryFactory 해결
#include "Common/Enums.h"                  // ProtocolType, ConnectionStatus 해결
#include "Common/Utils.h"                  // 유틸리티 함수들

// Workers includes
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/MqttWorker.h"
#include "Workers/Protocol/BACnetWorker.h"

// Drivers includes
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Bacnet/BACnetDriver.h"

// Database includes
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/Entities/CurrentValueEntity.h"

// Utils includes
#include "Utils/LogManager.h"
#include "Common/Structs.h"


#include <sstream>
#include <regex>

using std::max;
using std::min;
using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 🔥 네임스페이스 및 타입 별칭 (호환성 해결)
// =============================================================================
using ProtocolType = PulseOne::Enums::ProtocolType;
using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
using DataQuality = PulseOne::Enums::DataQuality;
using LogLevel = PulseOne::Enums::LogLevel;
using DataVariant = PulseOne::BasicTypes::DataVariant;

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
void WorkerFactory::SetRepositoryFactory(std::shared_ptr<Database::RepositoryFactory> repo_factory) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    repo_factory_ = repo_factory;
    logger_->Info("✅ RepositoryFactory injected into WorkerFactory");
}

void WorkerFactory::SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    device_repo_ = device_repo;
    logger_->Info("✅ DeviceRepository injected into WorkerFactory");
}

void WorkerFactory::SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    datapoint_repo_ = datapoint_repo;
    
    // 🔥 핵심: CurrentValueRepository가 이미 있으면 자동 연결
    if (datapoint_repo_ && current_value_repo_) {
        datapoint_repo_->setCurrentValueRepository(current_value_repo_);
        logger_->Info("✅ CurrentValueRepository auto-connected to DataPointRepository");
    }
    
    logger_->Info("✅ DataPointRepository injected into WorkerFactory");
}

void WorkerFactory::SetCurrentValueRepository(std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    current_value_repo_ = current_value_repo;
    
    // 🔥 핵심: DataPointRepository에 CurrentValueRepository 자동 주입
    if (datapoint_repo_ && current_value_repo_) {
        datapoint_repo_->setCurrentValueRepository(current_value_repo_);
        logger_->Info("✅ CurrentValueRepository auto-injected into DataPointRepository");
    }
    
    logger_->Info("✅ CurrentValueRepository injected into WorkerFactory");
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
    
    // 🔧 추가: using 선언으로 네임스페이스 명확화
    using std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    
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
        
        // 🔧 추가: DataPoint가 없는 경우 경고
        if (data_points.empty()) {
            logger_->Warn("⚠️ No data points found for device: " + device_entity.getName() + 
                         " - Worker will be created but without data points");
        }
        
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
        // 🔥 7. DataPoint들을 Worker에 추가 - 기존 로직 유지하되 컴파일 에러 수정
        // =======================================================================
        int added_points = 0;
        int failed_points = 0;
        
        for (const auto& data_point : data_points) {
            try {
                // ✅ 타입이 일치함: PulseOne::Structs::DataPoint -> PulseOne::DataPoint (별칭)
                if (worker->AddDataPoint(data_point)) {
                    added_points++;
                    
                    // 🔥 수정: 메서드 이름 수정 (isWritable 사용)
                    logger_->Debug("✅ Added DataPoint: " + data_point.name + 
                                  " (writable=" + (data_point.isWritable() ? "true" : "false") + 
                                  ", log_enabled=" + (data_point.log_enabled ? "true" : "false") + 
                                  ", interval=" + std::to_string(data_point.log_interval_ms) + "ms" + 
                                  ", current_value=" + GetCurrentValueAsString(data_point) + 
                                  ", quality=" + DataQualityToString(data_point.quality_code) + ")");
                } else {
                    failed_points++;
                    logger_->Warn("Failed to add data point: " + data_point.name + " to worker");
                }
            } catch (const std::exception& e) {
                failed_points++;
                logger_->Warn("Exception adding data point " + data_point.name + ": " + std::string(e.what()));
            }
        }
        
        // DataPoint 추가 결과 로깅
        if (failed_points > 0) {
            logger_->Warn("⚠️ Failed to add " + std::to_string(failed_points) + 
                         " out of " + std::to_string(data_points.size()) + " data points");
        }
        
        // 8. 통계 업데이트
        workers_created_++;
        auto end_time = steady_clock::now();
        auto creation_time = duration_cast<milliseconds>(end_time - start_time);
        
        // =======================================================================
        // 🔥 9. Worker별 통계 출력 - 기존 로직 유지하되 메서드 수정
        // =======================================================================
        int enabled_points = 0;
        int writable_points = 0;
        int log_enabled_points = 0;
        int good_quality_points = 0;
        
        for (const auto& dp : data_points) {
            try {
                if (dp.is_enabled) enabled_points++;
                if (dp.isWritable()) writable_points++;         // 🔥 수정: IsWritable() → isWritable()
                if (dp.log_enabled) log_enabled_points++;
                if (dp.quality_code == DataQuality::GOOD) good_quality_points++;  // 🔥 수정: IsGoodQuality() → 직접 비교
            } catch (const std::exception& e) {
                logger_->Debug("Exception in statistics calculation: " + std::string(e.what()));
            }
        }
        
        logger_->Info("✅ Worker created successfully for device: " + device_entity.getName() + 
                     " (Protocol: " + protocol_type + 
                     ", DataPoints: " + std::to_string(data_points.size()) + 
                     ", Added: " + std::to_string(added_points) +
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
    if (!repo_factory_) {
        logger_->Error("RepositoryFactory not set");
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
    
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo;
    if (repo_factory_) {
        device_repo = repo_factory_->getDeviceRepository();
    } else if (device_repo_) {
        device_repo = device_repo_;
    } else {
        logger_->Error("No DeviceRepository available (neither RepositoryFactory nor individual repo)");
        return workers;
    }

    try {
        logger_->Info("🏭 Creating workers for all active devices");
        
        // 🔧 수정: device_repo_ → device_repo
        auto devices = device_repo->findAll();
        
        for (const auto& device : devices) {
            if (device.isEnabled()) {
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
    
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo;
    if (repo_factory_) {
        device_repo = repo_factory_->getDeviceRepository();
    } else if (device_repo_) {
        device_repo = device_repo_;
    } else {
        logger_->Error("No DeviceRepository available (neither RepositoryFactory nor individual repo)");
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


/*
 * @brief DeviceEntity를 DeviceInfo로 변환 (DeviceSettings 통합)
 * @param device_entity 변환할 DeviceEntity
 * @return 완전한 DeviceInfo (DeviceSettings 포함)
 */
PulseOne::Structs::DeviceInfo WorkerFactory::ConvertToDeviceInfo(
    const Database::Entities::DeviceEntity& device_entity) const {
    
    PulseOne::Structs::DeviceInfo device_info;
    
    // =========================================================================
    // 🔥 1단계: DeviceEntity 기본 정보 매핑 (기존 로직 유지)
    // =========================================================================
    
    // 기본 식별 정보
    device_info.id = std::to_string(device_entity.getId());
    device_info.tenant_id = device_entity.getTenantId();
    device_info.name = device_entity.getName();
    device_info.description = device_entity.getDescription();
    device_info.endpoint = device_entity.getEndpoint();
    device_info.connection_string = device_entity.getEndpoint();
    device_info.is_enabled = device_entity.isEnabled();
    device_info.protocol_type = device_entity.getProtocolType();
    
    // 디바이스 상세 정보 (🆕 새로 추가된 필드들)
    device_info.device_type = device_entity.getDeviceType();
    device_info.manufacturer = device_entity.getManufacturer();
    device_info.model = device_entity.getModel();
    device_info.serial_number = device_entity.getSerialNumber();
    
    // 시간 정보
    device_info.created_at = device_entity.getCreatedAt();
    device_info.updated_at = device_entity.getUpdatedAt();
    
    // 🔥 수정: last_communication, last_seen 필드 추가 (컴파일 에러 수정)
    device_info.properties["last_communication"] = PulseOne::Utils::TimestampToString(device_entity.getUpdatedAt());
    device_info.properties["last_seen"] = PulseOne::Utils::TimestampToString(device_entity.getUpdatedAt());
    
    // 그룹 정보 - 🔥 수정: device_group_id 타입 변환 수정
    if (device_entity.getDeviceGroupId().has_value()) {
        device_info.properties["device_group_id"] = std::to_string(device_entity.getDeviceGroupId().value());
    }
    
    logger_->Debug("✅ DeviceEntity basic info mapped for device: " + device_entity.getName());
    
    // =========================================================================
    // 🔥 2단계: DeviceSettings 정보 로드 및 매핑 (기존 로직 유지)
    // =========================================================================
    
    try {
        if (!repo_factory_) {
            logger_->Warn("⚠️ RepositoryFactory not available, using default DeviceSettings");
            ApplyProtocolSpecificDefaults(device_info, device_entity.getProtocolType());
            return device_info;
        }
        
        auto device_settings_repo = repo_factory_->getDeviceSettingsRepository();
        if (!device_settings_repo) {
            logger_->Warn("⚠️ DeviceSettingsRepository not available, using default settings");
            ApplyProtocolSpecificDefaults(device_info, device_entity.getProtocolType());
            return device_info;
        }
        
        // DeviceSettings 로드 시도
        auto settings = device_settings_repo->findById(device_entity.getId());
        
        if (settings.has_value()) {
            const auto& s = settings.value();
            
            logger_->Debug("🔍 DeviceSettings found for device " + device_entity.getName() + 
                          " (device_id: " + std::to_string(device_entity.getId()) + ")");
            
            // ✅ 기본 타이밍 설정 매핑
            device_info.polling_interval_ms = s.getPollingIntervalMs();
            device_info.timeout_ms = s.getConnectionTimeoutMs();
            
            // 🔥 수정: 추가 필드들을 properties로 저장 (컴파일 에러 해결)
            device_info.properties["retry_interval_ms"] = std::to_string(s.getRetryIntervalMs());
            device_info.properties["backoff_time_ms"] = std::to_string(s.getBackoffTimeMs());
            device_info.properties["backoff_multiplier"] = std::to_string(s.getBackoffMultiplier());
            device_info.properties["max_backoff_time_ms"] = std::to_string(s.getMaxBackoffTimeMs());
            
            // Keep-Alive 설정
            device_info.properties["keep_alive_interval_s"] = std::to_string(s.getKeepAliveIntervalS());
            device_info.properties["keep_alive_timeout_s"] = std::to_string(s.getKeepAliveTimeoutS());
            
            // 추가 타이밍
            device_info.properties["write_timeout_ms"] = std::to_string(s.getWriteTimeoutMs());
            
            // 기능 플래그들
            device_info.properties["data_validation_enabled"] = s.isDataValidationEnabled() ? "true" : "false";
            device_info.properties["performance_monitoring_enabled"] = s.isPerformanceMonitoringEnabled() ? "true" : "false";
            device_info.properties["diagnostic_mode_enabled"] = s.isDiagnosticModeEnabled() ? "true" : "false";
            
        } else {
            // DeviceSettings가 없는 경우 - 기본 설정 생성 시도
            logger_->Warn("⚠️ DeviceSettings not found for device " + device_entity.getName() + 
                         " (device_id: " + std::to_string(device_entity.getId()) + ")");
            
            // 기본값 적용
            ApplyProtocolSpecificDefaults(device_info, device_entity.getProtocolType());
            logger_->Info("✅ Applied industrial default settings for device " + device_entity.getName());
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Exception while loading DeviceSettings for device " + device_entity.getName() + 
                      ": " + std::string(e.what()));
        
        // 예외 발생 시 기본값 사용
        ApplyProtocolSpecificDefaults(device_info, device_entity.getProtocolType());
        logger_->Info("✅ Applied fallback industrial defaults due to exception");
    }
    
    // =========================================================================
    // 🔥 3단계: 최종 검증 및 로깅 (기존 로직 유지)
    // =========================================================================
    
    // 프로토콜 타입 변환 (문자열 → 열거형) - 🔥 수정: properties 사용
    if (device_entity.getProtocolType() == "MODBUS_TCP") {
        device_info.properties["protocol"] = "MODBUS_TCP";
    } else if (device_entity.getProtocolType() == "MQTT") {
        device_info.properties["protocol"] = "MQTT";
    } else if (device_entity.getProtocolType() == "BACNET_IP") {
        device_info.properties["protocol"] = "BACNET_IP";
    } else {
        device_info.properties["protocol"] = "UNKNOWN";
        logger_->Warn("⚠️ Unknown protocol type: " + device_entity.getProtocolType());
    }
    
    // 연결 상태 초기화
    device_info.properties["connection_status"] = "DISCONNECTED";
    device_info.properties["auto_reconnect"] = "true";  
    device_info.properties["maintenance_allowed"] = "true";
    
    // 최종 검증 - 🔥 수정: ValidateDeviceSettings 제거하고 기본 검증
    if (device_info.timeout_ms <= 0) {
        device_info.timeout_ms = 5000;
    }
    if (device_info.polling_interval_ms <= 0) {
        device_info.polling_interval_ms = 1000;
    }
    
    logger_->Info("🎯 DeviceInfo conversion completed for device: " + device_entity.getName() + 
                 " (protocol: " + device_entity.getProtocolType() + 
                 ", endpoint: " + device_entity.getEndpoint() + 
                 ", enabled: " + (device_entity.isEnabled() ? "true" : "false") + ")");
    
    return device_info;
}


// =========================================================================
// 🆕 추가 헬퍼 메서드 (선택적)
// =========================================================================

/**
 * @brief DeviceSettings 로드 실패 시 프로토콜별 기본값 적용
 * @param device_info 설정할 DeviceInfo
 * @param protocol_type 프로토콜 타입
 */
void WorkerFactory::ApplyProtocolSpecificDefaults(PulseOne::Structs::DeviceInfo& device_info, 
                                                  const std::string& protocol_type) const {
    if (protocol_type == "MODBUS_TCP" || protocol_type == "MODBUS_RTU") {
        // Line 774: write_timeout_ms → properties 사용
        // device_info.write_timeout_ms = 3000;
        device_info.properties["write_timeout_ms"] = "3000";
        
        device_info.timeout_ms = 5000;
        device_info.polling_interval_ms = 1000;
        device_info.retry_count = 3;
        
    } else if (protocol_type == "MQTT") {
        // Line 783: keep_alive_interval_s → properties 사용
        // device_info.keep_alive_interval_s = 60;
        device_info.properties["keep_alive_interval_s"] = "60";
        
        device_info.timeout_ms = 10000;
        device_info.polling_interval_ms = 5000;
        device_info.retry_count = 5;
        
    } else if (protocol_type == "BACNET_IP" || protocol_type == "BACNET_MSTP") {
        device_info.timeout_ms = 15000;
        device_info.polling_interval_ms = 2000;
        device_info.retry_count = 2;
        
        // Line 795: SetStabilityMode() → 직접 구현
        // device_info.SetStabilityMode();
        device_info.properties["stability_mode"] = "true";
    }
    
    // Line 798: SyncCompatibilityFields() → 직접 구현
    // device_info.SyncCompatibilityFields();
    SyncDeviceInfoFields(device_info);
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
        // 🔥 수정: UpdateCurrentValue 메서드 대신 직접 필드 업데이트
        auto now = std::chrono::system_clock::now();
        
        // properties에 값 저장
        data_point.properties["current_value"] = PulseOne::Utils::DataVariantToString(new_value);
        data_point.properties["quality_code"] = std::to_string(static_cast<int>(new_quality));
        data_point.properties["value_timestamp"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
        data_point.properties["quality_timestamp"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
        
        logger_->Debug("📊 Updated DataPoint value: " + data_point.name + 
                      " = " + GetCurrentValueAsString(data_point) +
                      " (Quality: " + DataQualityToString(new_quality) + ")");
                      
    } catch (const std::exception& e) {
        logger_->Error("Failed to update data point value: " + std::string(e.what()));
        
        // 에러 발생 시 BAD 품질로 설정
        data_point.properties["quality_code"] = std::to_string(static_cast<int>(DataQuality::BAD));
        data_point.properties["quality_timestamp"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        // 에러 카운트 증가
        auto error_count_it = data_point.properties.find("error_count");
        int error_count = 0;
        if (error_count_it != data_point.properties.end()) {
            error_count = std::stoi(error_count_it->second);
        }
        data_point.properties["error_count"] = std::to_string(error_count + 1);
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
        // 🔥 수정: ShouldLog 메서드 대신 직접 구현
        if (!data_point.log_enabled) {
            return false;
        }
        
        // 시간 간격 체크
        auto now = std::chrono::system_clock::now();
        
        // 🔥 수정: last_log_time 필드 대신 properties 사용
        auto last_log_it = data_point.properties.find("last_log_time");
        if (last_log_it != data_point.properties.end()) {
            auto last_log_ms = std::stoull(last_log_it->second);
            auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            if (current_ms - last_log_ms < data_point.log_interval_ms) {
                return false;
            }
        }
        
        // Deadband 체크 (숫자 값인 경우)
        if (std::holds_alternative<double>(new_value)) {
            auto current_value_it = data_point.properties.find("current_value");
            if (current_value_it != data_point.properties.end()) {
                try {
                    double new_val = std::get<double>(new_value);
                    double old_val = std::stod(current_value_it->second);
                    double diff = std::abs(new_val - old_val);
                    
                    if (diff < data_point.log_deadband) {
                        return false;
                    }
                } catch (...) {
                    // 타입 변환 실패 시 로깅 허용
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to check logging condition: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief DataPointEntity를 DataPoint로 변환 (DeviceInfo 패턴 적용)
 * @param datapoint_entity 변환할 DataPointEntity
 * @param device_id_string DeviceInfo에서 가져온 device_id (UUID 문자열)
 * @return 완전한 DataPoint 구조체
 */
/**
 * @brief ConvertToDataPoint 메서드 (헤더에서 선언했다고 가정)
 */
PulseOne::Structs::DataPoint WorkerFactory::ConvertToDataPoint(
    const Database::Entities::DataPointEntity& datapoint_entity,
    const std::string& device_id_string) const {
    
    PulseOne::Structs::DataPoint data_point;
    
    // =======================================================================
    // 기본 식별 정보
    // =======================================================================
    data_point.id = std::to_string(datapoint_entity.getId());
    data_point.device_id = device_id_string;
    data_point.name = datapoint_entity.getName();
    data_point.description = datapoint_entity.getDescription();
    
    // =======================================================================
    // 주소 및 타입 정보
    // =======================================================================
    data_point.address = datapoint_entity.getAddress();
    data_point.data_type = StringToDataType(datapoint_entity.getDataType());
    data_point.access_mode = StringToAccessMode(datapoint_entity.getAccessMode());
    
    // =======================================================================
    // 스케일링 및 범위
    // =======================================================================
    data_point.unit = datapoint_entity.getUnit();
    data_point.scaling_factor = datapoint_entity.getScalingFactor();
    data_point.scaling_offset = datapoint_entity.getScalingOffset();
    data_point.min_value = datapoint_entity.getMinValue();
    data_point.max_value = datapoint_entity.getMaxValue();
    
    // =======================================================================
    // 로깅 설정 (수정된 버전)
    // =======================================================================
    data_point.log_enabled = datapoint_entity.isLogEnabled();
    data_point.log_interval_ms = datapoint_entity.getLogIntervalMs();
    data_point.log_deadband = datapoint_entity.getLogDeadband();
    data_point.last_log_time = std::chrono::system_clock::now();
    
    // =======================================================================
    // 메타데이터 (타입 변환 수정)
    // =======================================================================
    // 🔥 수정: vector<string> → string 변환
    auto tags = datapoint_entity.getTags();
    if (!tags.empty()) {
        std::ostringstream tag_stream;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) tag_stream << ",";
            tag_stream << tags[i];
        }
        data_point.tags = tag_stream.str();
    }
    
    // 🔥 수정: map<string,string> → string 변환 (JSON)
    auto metadata = datapoint_entity.getMetadata();
    if (!metadata.empty()) {
        std::ostringstream meta_stream;
        meta_stream << "{";
        bool first = true;
        for (const auto& [key, value] : metadata) {
            if (!first) meta_stream << ",";
            meta_stream << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        meta_stream << "}";
        data_point.metadata = meta_stream.str();
    }
    
    // =======================================================================
    // 통계 정보 (수정된 버전)
    // =======================================================================
    data_point.last_read_time = datapoint_entity.getLastReadTime();
    data_point.last_write_time = datapoint_entity.getLastWriteTime();
    data_point.read_count = datapoint_entity.getReadCount();
    data_point.write_count = datapoint_entity.getWriteCount();
    data_point.error_count = datapoint_entity.getErrorCount();
    
    // =======================================================================
    // 기본값 설정 (수정된 버전)
    // =======================================================================
    data_point.current_value = DataVariant(0.0);
    data_point.quality_code = DataQuality::NOT_CONNECTED;
    data_point.value_timestamp = std::chrono::system_clock::now();
    data_point.quality_timestamp = std::chrono::system_clock::now();
    
    return data_point;
}
/**
 * @brief DataPoint에 현재값 로드 (별도 메서드로 분리)
 * @param data_point 현재값을 로드할 DataPoint (참조)
 */
void WorkerFactory::LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const {
    try {
        auto factory = Database::RepositoryFactory::getInstance();
        auto current_value_repo = factory->getCurrentValueRepository();
        
        int data_point_id = std::stoi(data_point.id);
        auto current_value = current_value_repo->findByDataPointId(data_point_id);
        
        if (current_value.has_value()) {
            // 현재 값 설정 (properties 대신 직접 필드 사용)
            data_point.current_value = DataVariant(current_value->getValue());
            data_point.quality_code = current_value->getQuality();
            data_point.value_timestamp = current_value->getTimestamp();
            data_point.quality_timestamp = current_value->getUpdatedAt();
            
            logger_->Debug("📊 Loaded current value for: " + data_point.name + 
                          " = " + GetCurrentValueAsString(data_point) +
                          " (Quality: " + DataQualityToString(data_point.quality_code) + ")");
        } else {
            // 기본값 설정
            data_point.quality_code = DataQuality::BAD;
            data_point.quality_timestamp = std::chrono::system_clock::now();
            
            logger_->Debug("⚠️ No current value found for: " + data_point.name);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load current value: " + std::string(e.what()));
        
        // 에러 시 기본값
        data_point.quality_code = DataQuality::BAD;
        data_point.quality_timestamp = std::chrono::system_clock::now();
    }
}


/**
 * @brief LoadDataPointsForDevice 메서드 개선 (ConvertToDataPoint 사용)
 */
std::vector<PulseOne::Structs::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::Structs::DataPoint> data_points;
    
    try {
        auto factory = Database::RepositoryFactory::getInstance();
        auto datapoint_repo = factory->getDataPointRepository();
        
        auto datapoint_entities = datapoint_repo->findByDeviceId(device_id);
        
        data_points.reserve(datapoint_entities.size());
        
        int good_quality_count = 0;
        for (const auto& entity : datapoint_entities) {
            try {
                auto dp = ConvertToDataPoint(entity, std::to_string(device_id));
                LoadCurrentValueForDataPoint(dp);
                
                // 품질 통계 (수정된 버전)
                if (dp.quality_code == DataQuality::GOOD) {
                    good_quality_count++;
                }
                
                data_points.push_back(std::move(dp));
                
            } catch (const std::exception& e) {
                logger_->Warn("Failed to convert DataPoint: " + std::string(e.what()));
            }
        }
        
        logger_->Info("📊 Loaded " + std::to_string(data_points.size()) + 
                     " data points for device " + std::to_string(device_id) +
                     " (good quality: " + std::to_string(good_quality_count) + ")");
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load data points: " + std::string(e.what()));
    }
    
    return data_points;
}

} // namespace Workers
} // namespace PulseOne