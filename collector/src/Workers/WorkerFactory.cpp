// =============================================================================
// collector/src/Workers/WorkerFactory.cpp
// 🔥 완전한 DB 통합: DeviceSettings + CurrentValue + JSON 파싱 (컴파일 에러 수정 완료)
// =============================================================================

/**
 * @file WorkerFactory.cpp
 * @brief PulseOne WorkerFactory 구현 - 완전한 DB 통합 버전
 * @author PulseOne Development Team
 * @date 2025-08-08
 * 
 * 🎯 완성 기능:
 * - DeviceEntity → DeviceInfo (기본 정보)
 * - DeviceSettingsEntity → DeviceInfo (상세 설정)
 * - DataPointEntity → DataPoint (기본 정보)
 * - CurrentValueEntity → DataPoint (현재값/품질)
 * - JSON config/protocol_params 완전 파싱
 * - ProtocolConfigRegistry 통합
 * - endpoint → ip_address/port 파싱
 * 
 * 🔧 수정 사항:
 * - optional 값 처리 안전성 강화
 * - 존재하지 않는 메서드/필드 제거
 * - 헤더 extra qualification 에러 수정
 * - namespace 구조 정리
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
#include <regex>

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
#include "Database/RepositoryFactory.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include "Common/ProtocolConfigRegistry.h"

// Workers includes
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/ModbusRtuWorker.h"
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
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

// Utils includes
#include "Utils/LogManager.h"
#include "Common/Structs.h"

// JSON 라이브러리
#include <nlohmann/json.hpp>

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
// 🔧 싱글톤 구현
// =============================================================================

WorkerFactory& WorkerFactory::getInstance() {
    static WorkerFactory instance;
    return instance;
}

// =============================================================================
// 🔧 초기화 메서드들
// =============================================================================

bool WorkerFactory::Initialize() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    logger_ = &LogManager::getInstance();
    config_manager_ = &ConfigManager::getInstance();

    if (initialized_.load()) {
        logger_->Debug("WorkerFactory already initialized, skipping");
        return true;
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
    
    if (datapoint_repo_ && current_value_repo_) {
        logger_->Info("✅ CurrentValueRepository auto-connected to DataPointRepository");
    }
    
    logger_->Info("✅ DataPointRepository injected into WorkerFactory");
}

void WorkerFactory::SetCurrentValueRepository(std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    current_value_repo_ = current_value_repo;
    
    if (datapoint_repo_ && current_value_repo_) {
        logger_->Info("✅ CurrentValueRepository auto-connected to DataPointRepository");
    }
    
    logger_->Info("✅ CurrentValueRepository injected into WorkerFactory");
}

void WorkerFactory::SetDeviceSettingsRepository(std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    device_settings_repo_ = device_settings_repo;
    logger_->Info("✅ DeviceSettingsRepository injected into WorkerFactory");
}

void WorkerFactory::SetDatabaseClients(std::shared_ptr<RedisClient> redis_client, 
                                       std::shared_ptr<InfluxClient> influx_client) {
    redis_client_ = redis_client;
    influx_client_ = influx_client;
    if (logger_) {
        logger_->Info("✅ Database clients set in WorkerFactory");
    }
}

// =============================================================================
// 🔥 완전한 DeviceInfo 변환 (모든 DB 데이터 통합) - 수정된 버전
// =============================================================================

PulseOne::Structs::DeviceInfo WorkerFactory::ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    try {
        logger_->Debug("🔄 Converting DeviceEntity to complete DeviceInfo: " + device_entity.getName());
        
        // 1. 🔥 기본 DeviceInfo 생성
        PulseOne::Structs::DeviceInfo device_info;
        
        logger_->Debug("🔧 Step 1: Basic DeviceInfo creation started");
        
        // 기본 식별 정보
        device_info.id = std::to_string(device_entity.getId());
        device_info.tenant_id = device_entity.getTenantId();
        device_info.site_id = device_entity.getSiteId();
        device_info.device_group_id = device_entity.getDeviceGroupId();
        device_info.edge_server_id = device_entity.getEdgeServerId();
        
        // 디바이스 기본 정보
        device_info.name = device_entity.getName();
        device_info.description = device_entity.getDescription();
        device_info.device_type = device_entity.getDeviceType();
        device_info.manufacturer = device_entity.getManufacturer();
        device_info.model = device_entity.getModel();
        device_info.serial_number = device_entity.getSerialNumber();
        device_info.firmware_version = ""; // DeviceEntity에 없으면 빈 문자열
        
        // 통신 설정
        device_info.protocol_type = device_entity.getProtocolType();
        device_info.endpoint = device_entity.getEndpoint(); // IP주소는 endpoint에
        //device_info.connection_string = device_entity.getEndpoint(); // 별칭
        device_info.config = device_entity.getConfig(); // JSON설정은 config에
        
        // 상태 정보
        device_info.is_enabled = device_entity.isEnabled();
        device_info.connection_status = PulseOne::Enums::ConnectionStatus::DISCONNECTED; // 초기값
        
        // 시간 정보 - 🔧 optional 안전 처리
        device_info.created_at = device_entity.getCreatedAt();
        if (device_entity.getCreatedBy().has_value()) {
            device_info.created_by = device_entity.getCreatedBy().value();
        } else {
            device_info.created_by = 0; // 기본값
        }
        
        logger_->Debug("🔧 Step 2: Basic DeviceInfo fields set completed");
        
        // 2. 🔥 DeviceSettings 로드 및 통합
        std::optional<Database::Entities::DeviceSettingsEntity> settings_entity;
        if (device_settings_repo_) {
            try {
                logger_->Debug("🔧 Step 3: Loading DeviceSettings...");
                auto settings_opt = device_settings_repo_->findById(device_entity.getId());
                if (settings_opt.has_value()) {
                    settings_entity = settings_opt.value();
                    logger_->Debug("✅ DeviceSettings loaded for device: " + std::to_string(device_entity.getId()));
                } else {
                    logger_->Debug("⚠️ DeviceSettings not found, using defaults for device: " + std::to_string(device_entity.getId()));
                }
            } catch (const std::exception& e) {
                logger_->Error("❌ Failed to load DeviceSettings: " + std::string(e.what()));
            }
        } else {
            logger_->Debug("⚠️ DeviceSettingsRepository not available");
        }
        
        logger_->Debug("🔧 Step 4: DeviceSettings loading completed");
        
        // 3. 🔥 DeviceSettings 데이터 통합 (있는 경우)
        if (settings_entity.has_value()) {
            const auto& settings = settings_entity.value();
            
            logger_->Debug("🔧 Applying DeviceSettings data...");
            
            // 타이밍 설정
            device_info.connection_timeout_ms = settings.getConnectionTimeoutMs();
            device_info.read_timeout_ms = settings.getReadTimeoutMs();
            device_info.write_timeout_ms = settings.getWriteTimeoutMs();
            device_info.polling_interval_ms = settings.getPollingIntervalMs();
            device_info.timeout_ms = settings.getReadTimeoutMs(); // 호환성
            
            // 재시도 설정
            device_info.max_retry_count = settings.getMaxRetryCount();
            device_info.retry_count = settings.getMaxRetryCount(); // 호환성
            device_info.retry_interval_ms = settings.getRetryIntervalMs();
            device_info.backoff_multiplier = settings.getBackoffMultiplier();
            
            // Keep-alive 설정
            device_info.keep_alive_enabled = settings.isKeepAliveEnabled();
            device_info.keep_alive_interval_s = settings.getKeepAliveIntervalS();
            
            // 모니터링 설정
            device_info.data_validation_enabled = settings.isDataValidationEnabled();
            device_info.performance_monitoring_enabled = settings.isPerformanceMonitoringEnabled();
            device_info.performance_monitoring = settings.isPerformanceMonitoringEnabled(); // 호환성
            device_info.diagnostic_mode_enabled = settings.isDiagnosticModeEnabled();
            device_info.diagnostics_enabled = settings.isDiagnosticModeEnabled(); // 호환성
            
            // 🔧 optional 안전 처리
            std::string timeout_str = device_info.connection_timeout_ms.has_value() ? 
                                     std::to_string(device_info.connection_timeout_ms.value()) : "0";
            logger_->Debug("✅ DeviceSettings merged - Timeouts: " + timeout_str + "ms, " +
                          "Polling: " + std::to_string(device_info.polling_interval_ms) + "ms");
        } else {
            // 4. 🔥 기본값 설정
            logger_->Debug("🔧 Applying default DeviceSettings values...");
            ApplyDefaultSettings(device_info, device_entity.getProtocolType());
        }
        
        logger_->Debug("🔧 Step 5: DeviceSettings integration completed");
        
        // 5. 🔥 JSON config 파싱 → properties 맵
        logger_->Debug("🔧 Step 6: Starting JSON config parsing...");
        ParseDeviceConfigToProperties(device_info);
        logger_->Debug("🔧 Step 6: JSON config parsing completed");
        
        // 6. 🔥 ProtocolConfigRegistry로 프로토콜별 기본값 적용
        logger_->Debug("🔧 Step 7: Applying protocol defaults...");
        auto protocol_type = PulseOne::Utils::StringToProtocolType(device_info.protocol_type);
        if (protocol_type != PulseOne::Enums::ProtocolType::UNKNOWN) {
            PulseOne::Config::ApplyProtocolDefaults(protocol_type, device_info.properties);
            logger_->Debug("✅ Protocol defaults applied for: " + device_info.protocol_type + 
                        " (" + std::to_string(device_info.properties.size()) + " properties)");
            
            // 🔥 추가: 적용된 기본값들 로깅
            for (const auto& [key, value] : device_info.properties) {
                logger_->Debug("   Property: " + key + " = " + value);
            }
        } else {
            logger_->Warn("⚠️ Unknown protocol type: " + device_info.protocol_type + ", using manual defaults");
            // 🔧 수동 기본값 적용 메서드 호출
            ApplyProtocolSpecificDefaults(device_info, device_info.protocol_type);
        }
        logger_->Debug("🔧 Step 7: Protocol defaults completed");
        
        // 7. 🔥 endpoint 파싱 → ip_address, port 추출
        logger_->Debug("🔧 Step 8: Starting endpoint parsing...");
        ParseEndpoint(device_info);
        logger_->Debug("🔧 Step 8: Endpoint parsing completed");

        // 8. 🔥 설정 검증 (ProtocolConfigRegistry 사용)
        logger_->Debug("🔧 Step 9: Starting protocol config validation...");
        std::vector<std::string> validation_errors;
        if (!PulseOne::Config::ValidateProtocolConfig(protocol_type, device_info.properties, validation_errors)) {
            logger_->Warn("⚠️ Protocol config validation failed for " + device_info.name + ":");
            for (const auto& error : validation_errors) {
                logger_->Warn("   - " + error);
            }
        } else {
            logger_->Debug("✅ Protocol config validation passed for " + device_info.name);
        }        
        logger_->Debug("🔧 Step 9: Protocol config validation completed");
        
        // Step 10: DriverConfig 동기화 (에러 수정된 버전)
        logger_->Debug("🔧 Step 10: Starting enhanced DriverConfig sync (error-fixed)...");
        device_info.SyncToDriverConfig();

        // 🔥 동기화 결과 확인 로그
        const auto& config = device_info.GetDriverConfig();
        logger_->Info("✅ DriverConfig synchronized (all errors fixed):");
        logger_->Info("  - timeout_ms: " + std::to_string(config.timeout_ms));
        logger_->Info("  - retry_count: " + std::to_string(config.retry_count));  
        logger_->Info("  - polling_interval_ms: " + std::to_string(config.polling_interval_ms));
        logger_->Info("  - auto_reconnect: " + (config.auto_reconnect ? "true" : "false"));
        logger_->Info("  - properties count: " + std::to_string(config.properties.size()));

        // 🔥 재시도 정책 확인 (핵심 필드들)
        std::vector<std::string> key_properties = {
            "retry_interval_ms", "backoff_time_ms", "max_backoff_time_ms", 
            "backoff_multiplier", "keep_alive_enabled", "keep_alive_timeout_s"
        };

        logger_->Info("📋 Key retry policy properties:");
        for (const auto& key : key_properties) {
            if (config.properties.count(key)) {
                logger_->Info("  - " + key + ": " + config.properties.at(key));
            } else {
                logger_->Warn("  - " + key + ": NOT FOUND");
            }
        }

        // 전체 properties 디버그 로그 (필요시)
        logger_->Debug("📋 All DriverConfig properties:");
        for (const auto& [key, value] : config.properties) {
            logger_->Debug("    [" + key + "] = " + value);
        }

        logger_->Debug("🔧 Step 10: Enhanced DriverConfig sync completed (no errors)");


        // 11. 최종 검증
        logger_->Debug("🔧 Step 11: Starting final validation...");
        ValidateAndCorrectSettings(device_info);
        
        if (!device_info.IsValid()) {
            logger_->Warn("⚠️ DeviceInfo validation failed for device: " + device_info.name);
        }
        logger_->Debug("🔧 Step 11: Final validation completed");
        
        logger_->Info("✅ Complete DeviceInfo created: " + device_info.name + 
                     " (" + device_info.protocol_type + ") with " + 
                     std::to_string(device_info.properties.size()) + " properties");
        
        return device_info;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ ConvertToDeviceInfo failed: " + std::string(e.what()));
        
        // 🔥 실패 시 최소한의 DeviceInfo 반환
        PulseOne::Structs::DeviceInfo basic_device_info;
        basic_device_info.id = std::to_string(device_entity.getId());
        basic_device_info.name = device_entity.getName();
        basic_device_info.protocol_type = device_entity.getProtocolType();
        basic_device_info.endpoint = device_entity.getEndpoint();
        basic_device_info.is_enabled = device_entity.isEnabled();
        
        return basic_device_info;
    }
}

// =============================================================================
// 🔥 완전한 DataPoint 변환 (CurrentValue 통합) - 수정된 버전
// =============================================================================

PulseOne::Structs::DataPoint WorkerFactory::ConvertToDataPoint(
    const Database::Entities::DataPointEntity& datapoint_entity,
    const std::string& device_id_string) const {
    
    try {
        logger_->Debug("🔄 Converting DataPointEntity to complete DataPoint: " + datapoint_entity.getName());
        
        // 1. 🔥 기본 DataPoint 생성
        PulseOne::Structs::DataPoint data_point;
        
        // 기본 식별 정보
        data_point.id = std::to_string(datapoint_entity.getId());
        data_point.device_id = device_id_string.empty() ? std::to_string(datapoint_entity.getDeviceId()) : device_id_string;
        data_point.name = datapoint_entity.getName();
        data_point.description = datapoint_entity.getDescription();
        
        // 주소 정보
        data_point.address = static_cast<uint32_t>(datapoint_entity.getAddress());
        data_point.address_string = datapoint_entity.getAddressString();
        
        // 데이터 타입 및 접근성
        data_point.data_type = datapoint_entity.getDataType();
        data_point.access_mode = datapoint_entity.getAccessMode();
        data_point.is_enabled = datapoint_entity.isEnabled();
        data_point.is_writable = datapoint_entity.isWritable();
        
        // 엔지니어링 단위 및 스케일링
        data_point.unit = datapoint_entity.getUnit();
        data_point.scaling_factor = datapoint_entity.getScalingFactor();
        data_point.scaling_offset = datapoint_entity.getScalingOffset();
        data_point.min_value = datapoint_entity.getMinValue();
        data_point.max_value = datapoint_entity.getMaxValue();
        
        // 로깅 및 수집 설정 - 🔧 메서드명 수정
        data_point.log_enabled = datapoint_entity.isLogEnabled();
        data_point.log_interval_ms = static_cast<uint32_t>(datapoint_entity.getLogInterval()); // getLogIntervalMs() → getLogInterval()
        data_point.log_deadband = datapoint_entity.getLogDeadband();
        data_point.polling_interval_ms = static_cast<uint32_t>(datapoint_entity.getPollingInterval()); // getPollingIntervalMs() → getPollingInterval()
        
        // 메타데이터 - 🔧 존재하지 않는 필드 제거  
        // data_point.group_name = datapoint_entity.getGroupName(); // 이 필드가 존재하지 않을 수 있음
        data_point.tags = datapoint_entity.getTags().empty() ? "" : datapoint_entity.getTags()[0]; // 첫 번째 태그
        data_point.metadata = ""; // JSON을 문자열로 변환 필요시
        
        // 🔥 protocol_params 직접 복사 - DataPointEntity에서 이미 map<string,string>으로 반환
        const auto& protocol_params_map = datapoint_entity.getProtocolParams();
        if (!protocol_params_map.empty()) {
            // 이미 map 형태이므로 직접 복사
            data_point.protocol_params = protocol_params_map;
            logger_->Debug("✅ Protocol params copied: " + std::to_string(data_point.protocol_params.size()) + " items");
        }
        
        // 시간 정보
        data_point.created_at = datapoint_entity.getCreatedAt();
        data_point.updated_at = datapoint_entity.getUpdatedAt();
        
        // 2. 🔥 CurrentValue 로드 및 통합
        LoadCurrentValueForDataPoint(data_point);
        
        logger_->Debug("✅ Complete DataPoint created: " + data_point.name + 
                      " (addr=" + std::to_string(data_point.address) + 
                      ", type=" + data_point.data_type + ")");
        
        return data_point;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ ConvertToDataPoint failed: " + std::string(e.what()));
        
        // 최소한의 DataPoint 반환
        PulseOne::Structs::DataPoint basic_data_point;
        basic_data_point.id = std::to_string(datapoint_entity.getId());
        basic_data_point.device_id = device_id_string;
        basic_data_point.name = datapoint_entity.getName();
        basic_data_point.address = static_cast<uint32_t>(datapoint_entity.getAddress());
        basic_data_point.data_type = datapoint_entity.getDataType();
        
        return basic_data_point;
    }
}

// =============================================================================
// 🔥 헬퍼 메서드들 - 수정된 버전
// =============================================================================

/**
 * @brief DeviceEntity의 config JSON을 DeviceInfo.properties로 파싱
 */
void WorkerFactory::ParseDeviceConfigToProperties(PulseOne::Structs::DeviceInfo& device_info) const {
    try {
        const std::string& config_json = device_info.config;
        
        if (config_json.empty()) {
            logger_->Debug("⚠️ Empty config JSON for device: " + device_info.name);
            return;
        }
        
        logger_->Debug("🔄 Parsing config JSON: " + config_json);
        
        // JSON 파싱 - 수정된 안전한 버전
        nlohmann::json json_obj = nlohmann::json::parse(config_json);
        
        // JSON 객체인지 확인
        if (json_obj.is_object()) {
            // 반복자를 사용하여 안전하게 순회
            for (auto it = json_obj.begin(); it != json_obj.end(); ++it) {
                const std::string& key = it.key();
                const auto& value = it.value();
                
                std::string value_str;
                
                if (value.is_string()) {
                    value_str = value.get<std::string>();
                } else if (value.is_number_integer()) {
                    value_str = std::to_string(value.get<int>());
                } else if (value.is_number_float()) {
                    value_str = std::to_string(value.get<double>());
                } else if (value.is_boolean()) {
                    value_str = value.get<bool>() ? "true" : "false";
                } else {
                    // 복잡한 객체/배열은 JSON 문자열로 저장
                    value_str = value.dump();
                }
                
                device_info.properties[key] = value_str;
                logger_->Debug("  ✅ Property: " + key + " = " + value_str);
            }
        } else {
            logger_->Warn("⚠️ Config JSON is not an object: " + config_json);
        }
        
        logger_->Debug("✅ Parsed " + std::to_string(device_info.properties.size()) + " properties from config");
        
        // 🔧 제거: 존재하지 않는 메서드 호출 제거
        // device_info.ParsePropertiesFromConfig();
        
    } catch (const std::exception& e) {
        logger_->Error("❌ ParseDeviceConfigToProperties failed: " + std::string(e.what()));
        logger_->Error("   Config JSON was: " + device_info.config);
    }
}

/**
 * @brief endpoint 문자열에서 IP와 포트 추출
 */
void WorkerFactory::ParseEndpoint(PulseOne::Structs::DeviceInfo& device_info) const {
    try {
        const std::string& endpoint = device_info.endpoint;
        
        if (endpoint.empty()) {
            logger_->Warn("⚠️ Empty endpoint for device: " + device_info.name);
            return;
        }
        
        // TCP/IP 형태 파싱: "192.168.1.10:502" 또는 "tcp://192.168.1.10:502"
        std::string cleaned_endpoint = endpoint;
        
        // 프로토콜 prefix 제거
        size_t pos = cleaned_endpoint.find("://");
        if (pos != std::string::npos) {
            cleaned_endpoint = cleaned_endpoint.substr(pos + 3);
        }
        
        // IP:Port 분리
        pos = cleaned_endpoint.find(':');
        if (pos != std::string::npos) {
            device_info.ip_address = cleaned_endpoint.substr(0, pos);
            try {
                device_info.port = std::stoi(cleaned_endpoint.substr(pos + 1));
            } catch (...) {
                logger_->Warn("⚠️ Invalid port in endpoint: " + endpoint);
                device_info.port = 0;
            }
        } else {
            // 포트 없으면 IP만
            device_info.ip_address = cleaned_endpoint;
            device_info.port = 0;
        }
        
        logger_->Debug("✅ Endpoint parsed: " + device_info.ip_address + ":" + std::to_string(device_info.port));
        
    } catch (const std::exception& e) {
        logger_->Error("❌ ParseEndpoint failed: " + std::string(e.what()));
    }
}

/**
 * @brief JSON 값을 DataVariant로 파싱
 */
PulseOne::BasicTypes::DataVariant WorkerFactory::ParseJSONValue(
    const std::string& json_value, const std::string& data_type) const {
    
    try {
        if (json_value.empty()) {
            return PulseOne::BasicTypes::DataVariant{};
        }
        
        nlohmann::json json_obj = nlohmann::json::parse(json_value);
        
        // value 필드 추출
        if (json_obj.contains("value")) {
            const auto& value = json_obj["value"];
            
            if (data_type == "bool" || data_type == "BOOL") {
                return value.get<bool>();
            } else if (data_type == "int16" || data_type == "INT16") {
                return static_cast<int16_t>(value.get<int>());
            } else if (data_type == "uint16" || data_type == "UINT16") {
                return static_cast<uint16_t>(value.get<unsigned int>());
            } else if (data_type == "int32" || data_type == "INT32") {
                return value.get<int32_t>();
            } else if (data_type == "uint32" || data_type == "UINT32") {
                return value.get<uint32_t>();
            } else if (data_type == "float" || data_type == "FLOAT32") {
                return value.get<float>();
            } else if (data_type == "double" || data_type == "FLOAT64") {
                return value.get<double>();
            } else if (data_type == "string" || data_type == "STRING") {
                return value.get<std::string>();
            } else {
                // 기본값으로 문자열 반환
                return value.dump();
            }
        }
        
        logger_->Warn("⚠️ JSON value missing 'value' field: " + json_value);
        return PulseOne::BasicTypes::DataVariant{};
        
    } catch (const std::exception& e) {
        logger_->Error("❌ ParseJSONValue failed: " + std::string(e.what()) + " for: " + json_value);
        return PulseOne::BasicTypes::DataVariant{};
    }
}

void WorkerFactory::LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const {
    try {
        if (!current_value_repo_) {
            logger_->Warn("⚠️ CurrentValueRepository not injected, using default values");
            data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
            data_point.quality_timestamp = std::chrono::system_clock::now();
            return;
        }
        
        auto current_value_opt = current_value_repo_->findById(std::stoi(data_point.id));
        
        if (current_value_opt.has_value()) {
            const auto& cv = current_value_opt.value();
            // 🔥 JSON 값 파싱
            data_point.current_value = ParseJSONValue(cv.getCurrentValue(), data_point.data_type);
            data_point.raw_value = ParseJSONValueAsRaw(cv.getRawValue());
            
            // 품질 및 타임스탬프 - 🔧 존재하지 않는 필드 제거
            data_point.quality_code = static_cast<PulseOne::Enums::DataQuality>(cv.getQualityCode());
            // data_point.quality = cv.getQuality(); // 이 필드가 존재하지 않을 수 있음
            data_point.value_timestamp = cv.getValueTimestamp();
            data_point.quality_timestamp = cv.getQualityTimestamp();
            data_point.last_log_time = cv.getLastLogTime();
            data_point.last_read_time = cv.getLastReadTime();
            data_point.last_write_time = cv.getLastWriteTime();
            
            // 통계
            data_point.read_count = cv.getReadCount();
            data_point.write_count = cv.getWriteCount();
            data_point.error_count = cv.getErrorCount();
            
        } else {
            logger_->Debug("⚠️ CurrentValue not found for DataPoint: " + std::to_string(std::stoi(data_point.id)));
            
            // 기본값 설정 - 🔧 존재하지 않는 필드 제거
            data_point.quality_code = PulseOne::Enums::DataQuality::UNCERTAIN;
            // data_point.quality = "uncertain"; // 이 필드가 존재하지 않을 수 있음
            data_point.value_timestamp = std::chrono::system_clock::now();
            data_point.quality_timestamp = std::chrono::system_clock::now();
            data_point.read_count = 0;
            data_point.write_count = 0;
            data_point.error_count = 0;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("❌ Failed to load CurrentValue for DataPoint " + data_point.id + ": " + std::string(e.what()));
        
        // 에러 시 기본값 설정
        data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
        data_point.quality_timestamp = std::chrono::system_clock::now();
    }
}

std::vector<PulseOne::Structs::DataPoint> WorkerFactory::LoadDataPointsForDevice(int device_id) const {
    std::vector<PulseOne::Structs::DataPoint> data_points;
    
    try {
        if (!datapoint_repo_) {
            logger_->Error("❌ DataPointRepository not injected");
            return data_points;
        }
        
        logger_->Debug("🔄 Loading complete DataPoints for device: " + std::to_string(device_id));
        
        // DataPointEntity들 로드
        auto datapoint_entities = datapoint_repo_->findByDeviceId(device_id);
        
        logger_->Debug("📊 Found " + std::to_string(datapoint_entities.size()) + " DataPoints");
        
        std::string device_id_string = std::to_string(device_id);
        
        for (const auto& dp_entity : datapoint_entities) {
            try {
                // 완전한 DataPoint 변환 (CurrentValue 포함)
                auto data_point = ConvertToDataPoint(dp_entity, device_id_string);
                data_points.push_back(data_point);
                
            } catch (const std::exception& e) {
                logger_->Error("❌ Failed to process DataPoint " + std::to_string(dp_entity.getId()) + ": " + e.what());
                // 개별 DataPoint 실패는 전체를 중단하지 않음
            }
        }
        
        // 통계 로깅
        if (!data_points.empty()) {
            int total_count = static_cast<int>(data_points.size());
            int writable_count = 0;
            int good_quality_count = 0;
            int connected_count = 0;
            
            for (const auto& dp : data_points) {
                if (dp.is_writable) writable_count++;
                
                if (dp.quality_code == PulseOne::Enums::DataQuality::GOOD) {
                    good_quality_count++;
                }
                if (dp.quality_code != PulseOne::Enums::DataQuality::NOT_CONNECTED) {
                    connected_count++;
                }
            }
            
            logger_->Info("📊 Loaded " + std::to_string(total_count) + " DataPoints for Device " + std::to_string(device_id) +
                         " (Writable: " + std::to_string(writable_count) + 
                         ", Good Quality: " + std::to_string(good_quality_count) +
                         ", Connected: " + std::to_string(connected_count) + ")");
        }
        
        logger_->Info("✅ Loaded " + std::to_string(data_points.size()) + " complete DataPoints for device " + std::to_string(device_id));
        
        return data_points;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ LoadDataPointsForDevice failed: " + std::string(e.what()));
        return data_points;
    }
}

// =============================================================================
// Worker 생성 메서드들
// =============================================================================

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorker(const Database::Entities::DeviceEntity& device_entity) {
    //std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!IsInitialized()) {
        logger_->Error("❌ WorkerFactory not initialized");
        return nullptr;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        logger_->Info("🏭 Creating Worker with complete DB integration for device: " + device_entity.getName());
        
        // 1. 🔥 완전한 DeviceInfo 생성 (DeviceSettings 포함)
        auto device_info = ConvertToDeviceInfo(device_entity);
        
        logger_->Debug("✅ DeviceInfo conversion completed:");
        logger_->Debug("   - Name: " + device_info.name);
        logger_->Debug("   - Protocol: " + device_info.protocol_type);
        logger_->Debug("   - Endpoint: " + device_info.endpoint);
        logger_->Debug("   - IP:Port: " + device_info.ip_address + ":" + std::to_string(device_info.port));
        logger_->Debug("   - Properties: " + std::to_string(device_info.properties.size()));
        std::string enabled_str = device_info.is_enabled ? "yes" : "no";
        logger_->Debug("   - Enabled: " + enabled_str);
        
        // 2. 🔥 완전한 DataPoint들 로드 (CurrentValue 포함)
        logger_->Debug("🔄 Loading DataPoints for Device ID: " + std::to_string(device_entity.getId()));
        auto data_points = LoadDataPointsForDevice(device_entity.getId());
        
        if (data_points.empty()) {
            logger_->Warn("⚠️ No DataPoints found for Device ID: " + std::to_string(device_entity.getId()));
            logger_->Info("   Worker will be created without DataPoints (allowed)");
        } else {
            logger_->Debug("✅ Loaded " + std::to_string(data_points.size()) + " complete DataPoints");
        }
        
        // 3. 프로토콜 타입 확인 및 정규화
        std::string protocol_type = device_entity.getProtocolType();
        logger_->Debug("🔍 Original protocol: '" + protocol_type + "'");
        
        // 프로토콜 이름 정규화 시도 (대소문자 변환)
        std::string normalized_protocol = protocol_type;
        std::transform(normalized_protocol.begin(), normalized_protocol.end(), 
                      normalized_protocol.begin(), ::tolower);
        logger_->Debug("🔍 Normalized protocol: '" + normalized_protocol + "'");
        
        // 4. Creator 찾기
        auto creator_it = worker_creators_.find(protocol_type);
        std::string used_protocol = protocol_type;
        
        if (creator_it == worker_creators_.end()) {
            logger_->Debug("🔄 Original protocol not found, trying normalized: '" + normalized_protocol + "'");
            creator_it = worker_creators_.find(normalized_protocol);
            used_protocol = normalized_protocol;
        }
        
        if (creator_it == worker_creators_.end()) {
            logger_->Error("❌ No worker creator found for protocol: '" + protocol_type + "'");
            logger_->Error("   (Also tried normalized: '" + normalized_protocol + "')");
            
            // 사용 가능한 프로토콜 목록 출력
            logger_->Error("📋 Available protocols (" + std::to_string(worker_creators_.size()) + "):");
            int count = 0;
            for (const auto& [proto, creator] : worker_creators_) {
                if (count++ < 15) {
                    logger_->Error("   - '" + proto + "'");
                }
            }
            if (worker_creators_.size() > 15) {
                logger_->Error("   ... (+" + std::to_string(worker_creators_.size() - 15) + " more)");
            }
            
            creation_failures_.fetch_add(1);
            return nullptr;
        }
        
        logger_->Debug("✅ Found worker creator for: '" + used_protocol + "'");
        
        // 5. Worker 생성 실행
        logger_->Info("🏭 Creating " + used_protocol + " Worker for '" + device_entity.getName() + "'");
        logger_->Debug("📊 Using DeviceInfo with " + std::to_string(data_points.size()) + " DataPoints");
        
        auto worker = creator_it->second(device_info);
        
        // 6. 생성 결과 검증 및 로깅
        auto end_time = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (worker) {
            workers_created_.fetch_add(1);
            
            // 성공 통계
            int writable_count = 0;
            int good_quality_count = 0;
            for (const auto& data_point : data_points) {
                if (data_point.is_writable) writable_count++;
                if (data_point.quality_code == PulseOne::Enums::DataQuality::GOOD) {
                    good_quality_count++;
                }
            }
            
            logger_->Info("🎉 ✅ Worker created successfully!");
            logger_->Info("   📝 Device: " + device_entity.getName() + " (" + used_protocol + ")");
            logger_->Info("   📊 DataPoints: " + std::to_string(data_points.size()) + 
                         " (Writable: " + std::to_string(writable_count) + 
                         ", Good: " + std::to_string(good_quality_count) + ")");
            logger_->Info("   ⏱️ Creation Time: " + std::to_string(creation_time.count()) + "ms");
            logger_->Info("   🎯 Worker Type: " + std::string(typeid(*worker).name()));

            logger_->Debug("🔧 Protocol Configuration Details:");
            logger_->Debug(GetProtocolConfigInfo(device_entity.getProtocolType()));
            
        } else {
            creation_failures_.fetch_add(1);
            logger_->Error("❌ Worker creation returned nullptr for: " + device_entity.getName());
            logger_->Error("   🔧 Protocol: " + used_protocol);
            logger_->Error("   ⏱️ Failed after: " + std::to_string(creation_time.count()) + "ms");
        }
        
        return worker;
        
    } catch (const std::exception& e) {
        creation_failures_.fetch_add(1);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        logger_->Error("❌ Worker creation EXCEPTION for Device ID " + std::to_string(device_entity.getId()));
        logger_->Error("   💥 Exception: " + std::string(e.what()));
        logger_->Error("   📝 Device: " + device_entity.getName());
        logger_->Error("   🔧 Protocol: " + device_entity.getProtocolType());
        logger_->Error("   ⏱️ Failed after: " + std::to_string(creation_time.count()) + "ms");
        
        return nullptr;
    }
}

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorkerById(int device_id) {
    if (!device_repo_) {
        logger_->Error("❌ DeviceRepository not injected");
        return nullptr;
    }
    
    try {
        auto device = device_repo_->findById(device_id);
        if (!device.has_value()) {
            logger_->Error("❌ Device not found with ID: " + std::to_string(device_id));
            return nullptr;
        }
        
        return CreateWorker(device.value());
        
    } catch (const std::exception& e) {
        logger_->Error("❌ Failed to create worker by ID " + std::to_string(device_id) + 
                      ": " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateAllActiveWorkers() {
    return CreateAllActiveWorkers(0);  // max_workers = 0 (unlimited)
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateAllActiveWorkers(int max_workers) {
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("❌ DeviceRepository not injected");
        return workers;
    }
    
    try {
        auto devices = device_repo_->findAll();
        
        int created_count = 0;
        for (const auto& device : devices) {
            if (!device.isEnabled()) continue;
            
            auto worker = CreateWorker(device);
            if (worker) {
                workers.push_back(std::move(worker));
                created_count++;
                
                if (max_workers > 0 && created_count >= max_workers) {
                    logger_->Info("🚫 Worker creation limit reached: " + std::to_string(max_workers));
                    break;
                }
            }
        }
        
        logger_->Info("✅ Created " + std::to_string(created_count) + " active workers");
        return workers;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ Failed to create active workers: " + std::string(e.what()));
        return workers;
    }
}

std::vector<std::unique_ptr<BaseDeviceWorker>> WorkerFactory::CreateWorkersByProtocol(const std::string& protocol_type, int max_workers) {
    std::vector<std::unique_ptr<BaseDeviceWorker>> workers;
    
    if (!device_repo_) {
        logger_->Error("❌ DeviceRepository not injected");
        return workers;
    }
    
    try {
        auto devices = device_repo_->findAll();
        
        int created_count = 0;
        for (const auto& device : devices) {
            if (!device.isEnabled() || device.getProtocolType() != protocol_type) continue;
            
            auto worker = CreateWorker(device);
            if (worker) {
                workers.push_back(std::move(worker));
                created_count++;
                
                if (max_workers > 0 && created_count >= max_workers) {
                    logger_->Info("🚫 Protocol worker creation limit reached: " + std::to_string(max_workers));
                    break;
                }
            }
        }
        
        logger_->Info("✅ Created " + std::to_string(created_count) + " " + protocol_type + " workers");
        return workers;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ Failed to create " + protocol_type + " workers: " + std::string(e.what()));
        return workers;
    }
}

// =============================================================================
// Worker Creator 등록
// =============================================================================

void WorkerFactory::RegisterWorkerCreators() {
    logger_->Info("🏭 Registering Protocol Workers (COMPLETE DB INTEGRATION VERSION)");
    
    // MODBUS_TCP
    worker_creators_["MODBUS_TCP"] = [this](const PulseOne::Structs::DeviceInfo& device_info) -> std::unique_ptr<BaseDeviceWorker> {
        logger_->Debug("🔧 Creating ModbusTcpWorker for: " + device_info.name);
        try {
            return std::make_unique<ModbusTcpWorker>(device_info);
        } catch (const std::exception& e) {
            logger_->Error("❌ ModbusTcpWorker failed: " + std::string(e.what()));
            return nullptr;
        }
    };
    
    // MODBUS_RTU
    worker_creators_["MODBUS_RTU"] = [this](const PulseOne::Structs::DeviceInfo& device_info) -> std::unique_ptr<BaseDeviceWorker> {
        logger_->Debug("🔧 Creating ModbusRtuWorker for: " + device_info.name);
        try {
            return std::make_unique<ModbusRtuWorker>(device_info);
        } catch (const std::exception& e) {
            logger_->Error("❌ ModbusRtuWorker failed: " + std::string(e.what()));
            return nullptr;
        }
    };
    
    // MQTT
    worker_creators_["mqtt"] = [this](const PulseOne::Structs::DeviceInfo& device_info) -> std::unique_ptr<BaseDeviceWorker> {
        logger_->Debug("🔧 Creating MQTTWorker for: " + device_info.name);
        try {
            return std::make_unique<MQTTWorker>(device_info, MQTTWorkerMode::BASIC);
        } catch (const std::exception& e) {
            logger_->Error("❌ MQTTWorker failed: " + std::string(e.what()));
            return nullptr;
        }
    };
    
    // BACnet
    worker_creators_["bacnet"] = [this](const PulseOne::Structs::DeviceInfo& device_info) -> std::unique_ptr<BaseDeviceWorker> {
        logger_->Debug("🔧 Creating BACnetWorker for: " + device_info.name);
        try {
            return std::make_unique<BACnetWorker>(device_info);
        } catch (const std::exception& e) {
            logger_->Error("❌ BACnetWorker failed: " + std::string(e.what()));
            return nullptr;
        }
    };
    
    // 별칭들
    worker_creators_["modbus_tcp"] = worker_creators_["MODBUS_TCP"];
    worker_creators_["modbus_rtu"] = worker_creators_["MODBUS_RTU"];
    worker_creators_["MQTT"] = worker_creators_["mqtt"];
    worker_creators_["BACNET"] = worker_creators_["bacnet"];
    worker_creators_["BACNET_IP"] = worker_creators_["bacnet"];
    
    LogSupportedProtocols();
    
    logger_->Info("✅ Registered " + std::to_string(worker_creators_.size()) + " protocol creators");
    logger_->Info("🔧 ProtocolConfigRegistry provides automatic defaults and validation");
}

// =============================================================================
// 설정 및 유틸리티 메서드들
// =============================================================================

void WorkerFactory::ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& device_info, 
                                        const std::string& protocol_type) const {
    
    if (protocol_type == "MODBUS_TCP" || protocol_type == "modbus_tcp") {
        device_info.polling_interval_ms = 1000;
        device_info.timeout_ms = 3000;
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 10000;
        
    } else if (protocol_type == "MODBUS_RTU" || protocol_type == "modbus_rtu") {
        device_info.polling_interval_ms = 2000;
        device_info.timeout_ms = 5000;
        device_info.retry_count = 5;
        device_info.connection_timeout_ms = 15000;
        
    } else if (protocol_type == "mqtt") {
        device_info.polling_interval_ms = 500;
        device_info.timeout_ms = 10000;
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 15000;
        
    } else if (protocol_type == "bacnet") {
        device_info.polling_interval_ms = 5000;
        device_info.timeout_ms = 8000;
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 20000;
        
    } else {
        // 알 수 없는 프로토콜 - 안전한 기본값
        device_info.polling_interval_ms = 5000;
        device_info.timeout_ms = 10000;
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 20000;
    }
    
    // 공통 기본값들
    device_info.read_timeout_ms = device_info.timeout_ms;
    device_info.write_timeout_ms = device_info.timeout_ms;
    device_info.retry_interval_ms = 5000;
    device_info.max_retry_count = device_info.retry_count;
    device_info.backoff_multiplier = 1.5;
    device_info.keep_alive_enabled = true;
    device_info.keep_alive_interval_s = 30;
    device_info.data_validation_enabled = true;
    device_info.performance_monitoring_enabled = true;
    device_info.performance_monitoring = true; // 호환성
    device_info.diagnostic_mode_enabled = false;
    device_info.diagnostics_enabled = false; // 호환성
    
    logger_->Debug("🔧 " + protocol_type + " 기본값 적용됨");
}

// 🔧 추가: ApplyProtocolSpecificDefaults 메서드 구현
void WorkerFactory::ApplyProtocolSpecificDefaults(PulseOne::Structs::DeviceInfo& device_info, 
                                                  const std::string& protocol_type) const {
    // 프로토콜별 기본 properties 설정
    if (protocol_type == "MODBUS_TCP" || protocol_type == "modbus_tcp") {
        device_info.properties["unit_id"] = "1";
        device_info.properties["function_code"] = "3";
        device_info.properties["byte_order"] = "big_endian";
        device_info.properties["word_order"] = "high_low";
        
    } else if (protocol_type == "MODBUS_RTU" || protocol_type == "modbus_rtu") {
        device_info.properties["unit_id"] = "1";
        device_info.properties["baud_rate"] = "9600";
        device_info.properties["data_bits"] = "8";
        device_info.properties["stop_bits"] = "1";
        device_info.properties["parity"] = "none";
        
    } else if (protocol_type == "mqtt") {
        device_info.properties["client_id"] = "pulseone_collector";
        device_info.properties["clean_session"] = "true";
        device_info.properties["keep_alive"] = "60";
        device_info.properties["qos"] = "1";
        
    } else if (protocol_type == "bacnet") {
        device_info.properties["device_instance"] = "1";
        device_info.properties["object_type"] = "analog_input";
        device_info.properties["max_apdu_length"] = "1476";
        device_info.properties["segmentation"] = "both";
    }
    
    logger_->Debug("🔧 Manual protocol defaults applied for: " + protocol_type);
}

void WorkerFactory::ValidateAndCorrectSettings(PulseOne::Structs::DeviceInfo& device_info) const {
    // 최소값 보장
    if (device_info.polling_interval_ms < 100) {
        logger_->Warn("⚠️ polling_interval_ms too small, corrected to 100ms");
        device_info.polling_interval_ms = 100;
    }
    
    if (device_info.timeout_ms < 1000) {
        logger_->Warn("⚠️ timeout_ms too small, corrected to 1000ms");
        device_info.timeout_ms = 1000;
    }
    
    if (device_info.retry_count > 10) {
        logger_->Warn("⚠️ retry_count too large, corrected to 10");
        device_info.retry_count = 10;
    }
    
    // connection_timeout_ms가 optional이므로 체크
    if (device_info.connection_timeout_ms.has_value()) {
        if (device_info.connection_timeout_ms.value() < device_info.timeout_ms) {
            device_info.connection_timeout_ms = device_info.timeout_ms * 2;
            logger_->Debug("🔧 connection_timeout_ms adjusted to " + 
                          std::to_string(device_info.connection_timeout_ms.value()) + "ms");
        }
    }
}

std::string WorkerFactory::ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const {
    if (device_entity.getName().empty()) {
        return "Device name is empty";
    }
    
    if (device_entity.getProtocolType().empty()) {
        return "Protocol type is empty";
    }
    
    return "";  // 검증 통과
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
    worker_creators_[protocol_type] = creator;
    logger_->Info("✅ Registered worker creator for protocol: " + protocol_type);
}

// =============================================================================
// 헬퍼 함수들
// =============================================================================

void WorkerFactory::UpdateDataPointValue(PulseOne::Structs::DataPoint& data_point, 
                                         const PulseOne::BasicTypes::DataVariant& new_value,
                                         PulseOne::Enums::DataQuality new_quality) const {
    try {
        data_point.UpdateCurrentValue(new_value, new_quality);
        
        logger_->Debug("📊 Updated DataPoint value: " + data_point.name + 
                      " = " + data_point.GetCurrentValueAsString() +
                      " (Quality: " + data_point.GetQualityCodeAsString() + ")");
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to update DataPoint value: " + std::string(e.what()));
        data_point.SetErrorState(PulseOne::Enums::DataQuality::BAD);
    }
}

bool WorkerFactory::ShouldLogDataPoint(const PulseOne::Structs::DataPoint& data_point, 
                                       const PulseOne::BasicTypes::DataVariant& new_value) const {
    if (!data_point.log_enabled) return false;
        
    if (data_point.log_interval_ms > 0) {
        auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto last_log_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            data_point.last_log_time.time_since_epoch()).count();
        
        if (current_ms - last_log_ms < data_point.log_interval_ms) {
            return false;
        }
    }
    
    if (data_point.log_deadband > 0.0) {
        double current_value = std::visit([](const auto& v) -> double {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                return static_cast<double>(v);
            } else {
                return 0.0;
            }
        }, data_point.current_value);
        
        double new_double_value = std::visit([](const auto& v) -> double {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                return static_cast<double>(v);
            } else {
                return 0.0;
            }
        }, new_value);
        
        if (std::abs(new_double_value - current_value) < data_point.log_deadband) {
            return false;
        }
    }
    
    return true;
}

std::string WorkerFactory::GetCurrentValueAsString(const PulseOne::Structs::DataPoint& data_point) const {
    return data_point.GetCurrentValueAsString();
}

std::string WorkerFactory::GetQualityString(const PulseOne::Structs::DataPoint& data_point) const {
    return data_point.GetQualityCodeAsString();
}

std::string WorkerFactory::GetProtocolConfigInfo(const std::string& protocol_type) const {
    auto protocol_enum = PulseOne::Utils::StringToProtocolType(protocol_type);
    const auto* schema = PulseOne::Config::GetProtocolSchema(protocol_enum);
    
    if (!schema) {
        return "Unknown protocol: " + protocol_type;
    }
    
    std::ostringstream oss;
    oss << "Protocol: " << schema->name << "\n";
    oss << "Description: " << schema->description << "\n";
    oss << "Default Port: " << schema->default_port << "\n";
    oss << "Endpoint Format: " << schema->endpoint_format << "\n";
    oss << "Parameters (" << schema->parameters.size() << "):\n";
    
    for (const auto& param : schema->parameters) {
        oss << "  - " << param.key << " (" << param.type << ")";
        if (param.required) oss << " [REQUIRED]";
        oss << ": " << param.description;
        oss << " (default: " << param.default_value << ")\n";
    }
    
    return oss.str();
}

// 🔥 수정: LogSupportedProtocols 메서드를 클래스 내부 구현으로 변경
void WorkerFactory::LogSupportedProtocols() const {
    logger_->Info("🔧 Supported Protocols with ProtocolConfigRegistry:");
    
    auto& registry = PulseOne::Config::ProtocolConfigRegistry::getInstance();
    auto protocols = registry.GetRegisteredProtocols();
    
    for (const auto& protocol : protocols) {
        const auto* schema = registry.GetSchema(protocol);
        if (schema) {
            logger_->Info("   📋 " + schema->name + " (port: " + std::to_string(schema->default_port) + 
                         ", params: " + std::to_string(schema->parameters.size()) + ")");
        }
    }
    
    logger_->Info("   📊 Total: " + std::to_string(protocols.size()) + " protocols registered");
}

// 🔥 새로운 메소드 추가: raw_value 전용 파싱 (타입 무관)
PulseOne::BasicTypes::DataVariant WorkerFactory::ParseJSONValueAsRaw(
    const std::string& json_value) const {
    
    try {
        if (json_value.empty()) {
            return 0.0;  // 기본값
        }
        
        auto json_obj = nlohmann::json::parse(json_value);
        
        // value 필드 추출
        if (json_obj.contains("value")) {
            const auto& value = json_obj["value"];
            
            // 🔥 raw_value는 항상 숫자로 처리 (원시 센서 값이므로)
            if (value.is_number()) {
                return value.get<double>();
            } else if (value.is_boolean()) {
                return value.get<bool>() ? 1.0 : 0.0;  // boolean → 숫자 변환
            } else if (value.is_string()) {
                try {
                    return std::stod(value.get<std::string>());
                } catch (...) {
                    return 0.0;  // 변환 실패시 기본값
                }
            }
        }
        
        return 0.0;  // 기본값
        
    } catch (const std::exception& e) {
        logger_->Warn("⚠️ ParseJSONValueAsRaw failed: " + std::string(e.what()) + " for: " + json_value);
        return 0.0;
    }
}

} // namespace Workers
} // namespace PulseOne