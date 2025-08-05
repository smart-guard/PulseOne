/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 중복 메서드 제거 완료
 */

// 🔧 매크로 충돌 방지 - BACnet 헤더보다 먼저 STL 포함
#include <algorithm>
#include <functional>
#include <memory>

#include "Database/RepositoryFactory.h"

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
#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Utils/ConfigManager.h"
#include "Common/Constants.h"



#include <sstream>
#include <regex>

using std::max;
using std::min;
using namespace std::chrono;

using LogLevel = PulseOne::Enums::LogLevel;
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
        // 🔥 7. DataPoint들을 Worker에 추가 - 완성된 필드들 활용
        // =======================================================================
        int added_points = 0;
        int failed_points = 0;
        
        for (const auto& data_point : data_points) {
            try {
                // ✅ 타입이 일치함: PulseOne::Structs::DataPoint -> PulseOne::DataPoint (별칭)
                if (worker->AddDataPoint(data_point)) {
                    added_points++;
                    
                    // ✅ 풍부한 디버깅 정보 - 새로운 필드들 포함
                    logger_->Debug("✅ Added DataPoint: " + data_point.name + 
                                  " (writable=" + (data_point.IsWritable() ? "true" : "false") + 
                                  ", log_enabled=" + (data_point.log_enabled ? "true" : "false") + 
                                  ", interval=" + std::to_string(data_point.log_interval_ms) + "ms" + 
                                  ", current_value=" + data_point.GetCurrentValueAsString() + 
                                  ", quality=" + data_point.GetQualityCodeAsString() + ")");
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
        // 🔥 9. Worker별 통계 출력 - 완성된 필드들 활용
        // =======================================================================
        int enabled_points = 0;
        int writable_points = 0;
        int log_enabled_points = 0;
        int good_quality_points = 0;
        
        for (const auto& dp : data_points) {
            try {
                if (dp.is_enabled) enabled_points++;
                if (dp.IsWritable()) writable_points++;         // ✅ 새 메서드 사용
                if (dp.log_enabled) log_enabled_points++;
                if (dp.IsGoodQuality()) good_quality_points++;  // ✅ 새 메서드 사용
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
    // 🔥 1단계: DeviceEntity 기본 정보 매핑
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
    device_info.last_communication = device_entity.getUpdatedAt(); // 임시
    device_info.last_seen = device_entity.getUpdatedAt(); // 임시
    
    // 그룹 정보
    if (device_entity.getDeviceGroupId().has_value()) {
        device_info.device_group_id = std::to_string(device_entity.getDeviceGroupId().value());
    }
    
    logger_->Debug("✅ DeviceEntity basic info mapped for device: " + device_entity.getName());
    
    // =========================================================================
    // 🔥 2단계: DeviceSettings 정보 로드 및 매핑
    // =========================================================================
    
    try {
        if (!repo_factory_) {
            logger_->Warn("⚠️ RepositoryFactory not available, using default DeviceSettings");
            device_info.SetIndustrialDefaults();
            return device_info;
        }
        
        auto device_settings_repo = repo_factory_->getDeviceSettingsRepository();
        if (!device_settings_repo) {
            logger_->Warn("⚠️ DeviceSettingsRepository not available, using default settings");
            device_info.SetIndustrialDefaults();
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
            device_info.connection_timeout_ms = s.getConnectionTimeoutMs();
            device_info.timeout_ms = s.getConnectionTimeoutMs(); // 호환성
            
            // ✅ 재시도 설정 매핑
            device_info.retry_count = s.getMaxRetryCount();
            device_info.retry_count = s.getMaxRetryCount(); // 호환성
            device_info.retry_interval_ms = s.getRetryIntervalMs();
            device_info.backoff_time_ms = s.getBackoffTimeMs();
            device_info.backoff_multiplier = s.getBackoffMultiplier();
            device_info.max_backoff_time_ms = s.getMaxBackoffTimeMs();
            
            // ✅ Keep-Alive 설정 매핑
            device_info.keep_alive_enabled = s.isKeepAliveEnabled();
            device_info.keep_alive_interval_s = s.getKeepAliveIntervalS();
            device_info.keep_alive_timeout_s = s.getKeepAliveTimeoutS();
            
            // ✅ 세부 타임아웃 설정 매핑
            device_info.read_timeout_ms = s.getReadTimeoutMs();
            device_info.write_timeout_ms = s.getWriteTimeoutMs();
            
            // ✅ 기능 플래그들 매핑
            device_info.data_validation_enabled = s.isDataValidationEnabled();
            device_info.performance_monitoring_enabled = s.isPerformanceMonitoringEnabled();
            device_info.diagnostic_mode_enabled = s.isDiagnosticModeEnabled();
            
            // ✅ 선택적 설정들 매핑
            device_info.scan_rate_override = s.getScanRateOverride();
            
            // Duration 필드들 동기화
            device_info.timeout_ms = std::chrono::milliseconds(s.getConnectionTimeoutMs());
            device_info.polling_interval_ms = std::chrono::milliseconds(s.getPollingIntervalMs());
            
            // 호환성 필드들 동기화
            device_info.SyncCompatibilityFields();
            
            // 설정 검증
            if (!device_info.ValidateDeviceSettings()) {
                logger_->Warn("⚠️ Invalid DeviceSettings detected for device " + device_entity.getName() + 
                             ", applying industrial defaults");
                device_info.SetIndustrialDefaults();
            }
            
            logger_->Info("✅ DeviceSettings successfully mapped for device " + device_entity.getName() + 
                         " (polling: " + std::to_string(s.getPollingIntervalMs()) + "ms, " +
                         "timeout: " + std::to_string(s.getConnectionTimeoutMs()) + "ms, " +
                         "retry: " + std::to_string(s.getMaxRetryCount()) + ", " +
                         "keep_alive: " + (s.isKeepAliveEnabled() ? "enabled" : "disabled") + ")");
                         
        } else {
            // DeviceSettings가 없는 경우 - 기본 설정 생성 시도
            logger_->Warn("⚠️ DeviceSettings not found for device " + device_entity.getName() + 
                         " (device_id: " + std::to_string(device_entity.getId()) + ")");
            
            // 기본 설정 생성 시도
            try {
                bool created = device_settings_repo->createDefaultSettings(device_entity.getId());
                if (created) {
                    logger_->Info("✅ Created default DeviceSettings for device " + device_entity.getName());
                    
                    // 다시 로드 시도
                    auto new_settings = device_settings_repo->findById(device_entity.getId());
                    if (new_settings.has_value()) {
                        logger_->Info("✅ Successfully loaded newly created DeviceSettings");
                        // 위의 매핑 로직을 재귀 호출하거나 복사
                        // 간단히 기본값 사용
                    }
                }
            } catch (const std::exception& e) {
                logger_->Error("Failed to create default DeviceSettings: " + std::string(e.what()));
            }
            
            // 기본값 적용
            device_info.SetIndustrialDefaults();
            logger_->Info("✅ Applied industrial default settings for device " + device_entity.getName());
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Exception while loading DeviceSettings for device " + device_entity.getName() + 
                      ": " + std::string(e.what()));
        
        // 예외 발생 시 기본값 사용
        device_info.SetIndustrialDefaults();
        logger_->Info("✅ Applied fallback industrial defaults due to exception");
    }
    
    // =========================================================================
    // 🔥 3단계: 최종 검증 및 로깅
    // =========================================================================
    
    // 프로토콜 타입 변환 (문자열 → 열거형)
    if (device_entity.getProtocolType() == "MODBUS_TCP") {
        device_info.protocol = PulseOne::ProtocolType::MODBUS_TCP;
    } else if (device_entity.getProtocolType() == "MQTT") {
        device_info.protocol = PulseOne::ProtocolType::MQTT;
    } else if (device_entity.getProtocolType() == "BACNET") {
        device_info.protocol = PulseOne::ProtocolType::BACNET_IP;
    } else {
        device_info.protocol = PulseOne::ProtocolType::UNKNOWN;
        logger_->Warn("⚠️ Unknown protocol type: " + device_entity.getProtocolType());
    }
    
    // 연결 상태 초기화
    device_info.connection_status = PulseOne::ConnectionStatus::DISCONNECTED;
    device_info.auto_reconnect = true;
    device_info.maintenance_allowed = true;
    
    // 최종 검증
    if (!device_info.ValidateDeviceSettings()) {
        logger_->Error("❌ Final DeviceSettings validation failed for device " + device_entity.getName());
        device_info.SetIndustrialDefaults();
    }
    
    // 상세 로깅 (디버깅용)
    if (logger_->getLogLevel() <= LogLevel::DEBUG_LEVEL) {
        auto settings_json = device_info.GetDeviceSettingsJson();
        logger_->Debug("📊 Final DeviceInfo settings for " + device_entity.getName() + ": " + settings_json.dump());
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
void WorkerFactory::ApplyProtocolSpecificDefaults(
    PulseOne::Structs::DeviceInfo& device_info, 
    const std::string& protocol_type) const {
    
    if (protocol_type == "MODBUS_TCP") {
        // Modbus TCP 최적화 설정
        device_info.polling_interval_ms = 1000;     // 1초
        device_info.connection_timeout_ms = 5000;   // 5초
        device_info.read_timeout_ms = 3000;         // 3초
        device_info.write_timeout_ms = 3000;        // 3초
        device_info.retry_count = 3;
        device_info.keep_alive_enabled = false;     // Modbus는 보통 Keep-Alive 불필요
        
    } else if (protocol_type == "MQTT") {
        // MQTT 최적화 설정
        device_info.polling_interval_ms = 5000;     // 5초 (구독 기반이므로 길게)
        device_info.connection_timeout_ms = 10000;  // 10초
        device_info.keep_alive_enabled = true;      // MQTT는 Keep-Alive 중요
        device_info.keep_alive_interval_s = 60;     // 1분
        device_info.retry_count = 5;            // 네트워크 기반이므로 더 많이
        
    } else if (protocol_type == "BACNET") {
        // BACnet 최적화 설정
        device_info.polling_interval_ms = 2000;     // 2초
        device_info.connection_timeout_ms = 8000;   // 8초
        device_info.retry_count = 3;
        device_info.keep_alive_enabled = false;     // BACnet은 보통 Keep-Alive 불필요
        
    } else {
        // 알 수 없는 프로토콜 - 보수적 설정
        device_info.SetStabilityMode();
    }
    
    device_info.SyncCompatibilityFields();
    
    logger_->Info("✅ Applied protocol-specific defaults for " + protocol_type);
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
    
    // =========================================================================
    // 🔥 1단계: 기본 식별 정보
    // =========================================================================
    data_point.id = std::to_string(datapoint_entity.getId());
    data_point.device_id = device_id_string;  // 🔥 DeviceInfo에서 받은 UUID!
    data_point.name = datapoint_entity.getName();
    data_point.description = datapoint_entity.getDescription();
    
    // =========================================================================
    // 🔥 2단계: 주소 정보
    // =========================================================================
    data_point.address = static_cast<uint32_t>(datapoint_entity.getAddress());
    data_point.address_string = std::to_string(datapoint_entity.getAddress());
    data_point.SyncAddressFields();
    
    // =========================================================================
    // 🔥 3단계: 데이터 타입 및 접근성
    // =========================================================================
    data_point.data_type = datapoint_entity.getDataType();
    data_point.access_mode = datapoint_entity.getAccessMode();
    data_point.is_enabled = datapoint_entity.isEnabled();
    data_point.is_writable = datapoint_entity.isWritable();
    
    // =========================================================================
    // 🔥 4단계: 엔지니어링 정보
    // =========================================================================
    data_point.unit = datapoint_entity.getUnit();
    data_point.scaling_factor = datapoint_entity.getScalingFactor();
    data_point.scaling_offset = datapoint_entity.getScalingOffset();
    data_point.min_value = datapoint_entity.getMinValue();
    data_point.max_value = datapoint_entity.getMaxValue();
    
    // =========================================================================
    // 🔥 5단계: 로깅 설정
    // =========================================================================
    data_point.log_enabled = datapoint_entity.isLogEnabled();
    data_point.log_interval_ms = static_cast<uint32_t>(datapoint_entity.getLogInterval());
    data_point.log_deadband = datapoint_entity.getLogDeadband();
    data_point.last_log_time = std::chrono::system_clock::now();
    
    // =========================================================================
    // 🔥 6단계: 메타데이터
    // =========================================================================
    data_point.tags = datapoint_entity.getTags();
    data_point.metadata = datapoint_entity.getMetadata();
    
    // =========================================================================
    // 🔥 7단계: 시간 정보
    // =========================================================================
    data_point.created_at = datapoint_entity.getCreatedAt();
    data_point.updated_at = datapoint_entity.getUpdatedAt();
    data_point.last_read_time = datapoint_entity.getLastReadTime();
    data_point.last_write_time = datapoint_entity.getLastWriteTime();
    
    // =========================================================================
    // 🔥 8단계: 통계 정보
    // =========================================================================
    data_point.read_count = datapoint_entity.getReadCount();
    data_point.write_count = datapoint_entity.getWriteCount();
    data_point.error_count = datapoint_entity.getErrorCount();
    
    // =========================================================================
    // 🔥 9단계: 현재값 초기화 (기본값)
    // =========================================================================
    data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
    data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
    data_point.value_timestamp = std::chrono::system_clock::now();
    data_point.quality_timestamp = std::chrono::system_clock::now();
    
    logger_->Debug("✅ DataPoint converted: " + data_point.name + 
                  " (device_id: " + device_id_string + 
                  ", address: " + std::to_string(data_point.address) + ")");
    
    return data_point;
}

/**
 * @brief DataPoint에 현재값 로드 (별도 메서드로 분리)
 * @param data_point 현재값을 로드할 DataPoint (참조)
 */
void WorkerFactory::LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const {
    if (!current_value_repo_) {
        logger_->Debug("⚠️ CurrentValueRepository not available for: " + data_point.name);
        return;
    }
    
    try {
        int point_id = std::stoi(data_point.id);
        auto current_value = current_value_repo_->findByDataPointId(point_id);
        
        if (current_value.has_value()) {
            // 현재값 로드 성공
            data_point.current_value = PulseOne::BasicTypes::DataVariant(current_value->getValue());
            data_point.quality_code = current_value->getQuality();
            data_point.value_timestamp = current_value->getTimestamp();
            data_point.quality_timestamp = current_value->getUpdatedAt();
            
            logger_->Debug("💡 Current value loaded: " + data_point.name + 
                          " = " + data_point.GetCurrentValueAsString() + 
                          " (Quality: " + data_point.GetQualityCodeAsString() + ")");
        } else {
            logger_->Debug("⚠️ No current value found for: " + data_point.name);
            // 기본값 유지 (NOT_CONNECTED 상태)
        }
        
    } catch (const std::exception& e) {
        logger_->Warn("❌ Failed to load current value for " + data_point.name + ": " + std::string(e.what()));
        
        data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
        data_point.quality_timestamp = std::chrono::system_clock::now();
    }
}

/**
 * @brief LoadDataPointsForDevice 메서드 개선 (ConvertToDataPoint 사용)
 */
std::vector<PulseOne::Structs::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::Structs::DataPoint> result;
    
    try {
        logger_->Debug("🔍 Loading DataPoints for device ID: " + std::to_string(device_id));
        
        // =======================================================================
        // 🔥 1단계: DeviceEntity로부터 device_id_string 획득 
        // =======================================================================
        std::string device_id_string = std::to_string(device_id); // 기본값
        
        if (device_repo_) {
            try {
                auto device_entity = device_repo_->findById(device_id);
                if (device_entity.has_value()) {
                    // DeviceInfo 변환해서 UUID 얻기
                    auto device_info = ConvertToDeviceInfo(device_entity.value());
                    device_id_string = device_info.id;  // 🔥 UUID 문자열!
                    logger_->Debug("📋 Device UUID: " + device_id_string);
                } else {
                    logger_->Warn("⚠️ Device not found in DeviceRepository: " + std::to_string(device_id));
                }
            } catch (const std::exception& e) {
                logger_->Warn("⚠️ Failed to get device UUID: " + std::string(e.what()));
            }
        } else {
            logger_->Warn("⚠️ DeviceRepository not available, using numeric device_id");
        }
        
        // =======================================================================
        // 🔥 2단계: DataPointEntity들 로드
        // =======================================================================
        if (!datapoint_repo_) {
            if (repo_factory_) {
                // RepositoryFactory에서 DataPointRepository 가져오기
                try {
                    auto repo = repo_factory_->getDataPointRepository();
                    if (repo) {
                        auto entities = repo->findByDeviceId(device_id, true);
                        logger_->Debug("📊 Found " + std::to_string(entities.size()) + " DataPoint entities via RepositoryFactory");
                        
                        // Entity들을 DataPoint로 변환
                        for (const auto& entity : entities) {
                            auto data_point = ConvertToDataPoint(entity, device_id_string);
                            LoadCurrentValueForDataPoint(data_point);
                            result.push_back(data_point);
                        }
                    } else {
                        logger_->Error("❌ Failed to get DataPointRepository from RepositoryFactory");
                        return {};
                    }
                } catch (const std::exception& e) {
                    logger_->Error("❌ RepositoryFactory failed: " + std::string(e.what()));
                    return {};
                }
            } else {
                logger_->Error("❌ No DataPoint repository available");
                return {};
            }
        } else {
            // 직접 주입된 DataPointRepository 사용
            try {
                auto entities = datapoint_repo_->findByDeviceId(device_id, true);
                logger_->Debug("📊 Found " + std::to_string(entities.size()) + " DataPoint entities via direct repository");
                
                // Entity들을 DataPoint로 변환
                for (const auto& entity : entities) {
                    auto data_point = ConvertToDataPoint(entity, device_id_string);
                    LoadCurrentValueForDataPoint(data_point);
                    result.push_back(data_point);
                }
            } catch (const std::exception& e) {
                logger_->Error("❌ Direct repository failed: " + std::string(e.what()));
                return {};
            }
        }
        
        // =======================================================================
        // 🔥 3단계: 최종 검증 및 통계
        // =======================================================================
        if (result.empty()) {
            logger_->Warn("⚠️ No DataPoints found for device: " + std::to_string(device_id));
        } else {
            // 통계 정보 생성
            int enabled_count = 0;
            int writable_count = 0;
            int good_quality_count = 0;
            int with_current_value_count = 0;
            
            for (const auto& dp : result) {
                if (dp.is_enabled) enabled_count++;
                if (dp.IsWritable()) writable_count++;
                if (dp.IsGoodQuality()) good_quality_count++;
                if (dp.quality_code != PulseOne::Enums::DataQuality::NOT_CONNECTED) {
                    with_current_value_count++;
                }
            }
            
            logger_->Info("✅ Successfully loaded " + std::to_string(result.size()) + 
                         " DataPoints for device: " + device_id_string + 
                         " (Enabled: " + std::to_string(enabled_count) +
                         ", Writable: " + std::to_string(writable_count) +
                         ", GoodQuality: " + std::to_string(good_quality_count) +
                         ", WithCurrentValue: " + std::to_string(with_current_value_count) + ")");
        }
        
    } catch (const std::exception& e) {
        logger_->Error("❌ LoadDataPointsForDevice failed: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace Workers
} // namespace PulseOne