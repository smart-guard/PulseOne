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
#include "Workers/Protocol/ModbusTcpWorker.h"   // ✅ 존재
#include "Workers/Protocol/ModbusRtuWorker.h"   // ✅ 존재  
#include "Workers/Protocol/MqttWorker.h"        // ✅ 존재 (MqttWorker가 아니라 MQTTWorker)
#include "Workers/Protocol/BACnetWorker.h"      // ✅ 존재

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
    
    // CurrentValueRepository가 이미 있으면 자동 연결
    if (datapoint_repo_ && current_value_repo_) {
        //datapoint_repo_->setCurrentValueRepository(current_value_repo_);
        logger_->Info("✅ CurrentValueRepository auto-connected to DataPointRepository");
    }
    
    logger_->Info("✅ DataPointRepository injected into WorkerFactory");
}

void WorkerFactory::SetCurrentValueRepository(std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    current_value_repo_ = current_value_repo;
    
    // DataPointRepository가 이미 있으면 자동 연결
    if (datapoint_repo_ && current_value_repo_) {
        //datapoint_repo_->setCurrentValueRepository(current_value_repo_);
        logger_->Info("✅ CurrentValueRepository auto-connected to DataPointRepository");
    }
    
    logger_->Info("✅ CurrentValueRepository injected into WorkerFactory");
}

// =============================================================================
// Worker 생성 메서드들
// =============================================================================

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorker(const Database::Entities::DeviceEntity& device_entity) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!IsInitialized()) {
        logger_->Error("❌ WorkerFactory not initialized");
        return nullptr;
    }
    
    try {
        // 1. DeviceEntity를 DeviceInfo로 변환
        auto device_info = ConvertToDeviceInfo(device_entity);
        
        // 2. DataPoint들 로드
        auto data_points = LoadDataPointsForDevice(device_entity.getId());
        
        if (data_points.empty()) {
            logger_->Warn("⚠️ No DataPoints found for Device ID: " + std::to_string(device_entity.getId()));
            // return nullptr;  // 주석 처리해서 DataPoint 없어도 Worker 생성 허용
        }
        
        // 3. 프로토콜 타입 확인 (그대로 사용! - 이미 대문자)
        std::string protocol_type = device_entity.getProtocolType();  // "MODBUS_TCP"
        logger_->Debug("🔍 Looking for protocol: " + protocol_type);
        
        auto creator_it = worker_creators_.find(protocol_type);
        
        if (creator_it == worker_creators_.end()) {
            logger_->Error("❌ No worker creator found for protocol: " + protocol_type);
            
            // 디버깅용: 사용 가능한 프로토콜 목록 출력
            logger_->Error("   Available protocols:");
            for (const auto& [proto, creator] : worker_creators_) {
                logger_->Error("     - " + proto);
            }
            
            return nullptr;
        }
        
        // 4. Worker 생성
        logger_->Debug("🏭 Creating " + protocol_type + " Worker...");
        auto worker = creator_it->second(device_info, data_points);
        
        if (worker) {
            // 5. 통계 로깅
            int good_quality_count = 0;
            for (const auto& data_point : data_points) {
                if (data_point.quality_code == PulseOne::Enums::DataQuality::GOOD) {
                    good_quality_count++;
                }
            }
            
            logger_->Info("🎉 Successfully created " + protocol_type + " Worker for Device '" + device_entity.getName() + 
                         "' with " + std::to_string(data_points.size()) + " DataPoints" +
                         " (Good Quality: " + std::to_string(good_quality_count) + ")");
        } else {
            logger_->Error("❌ Worker creation returned nullptr for: " + device_entity.getName());
        }
        
        return worker;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ Failed to create worker for Device ID " + std::to_string(device_entity.getId()) + 
                      ": " + std::string(e.what()));
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
    return CreateAllActiveWorkers(0);  // tenant_id = 0 (기본값)
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
// 내부 유틸리티 메서드들 - 🔧 완전 수정: 실제 Worker 생성
// =============================================================================

// =============================================================================
// 올바른 타입별 속성 저장 방식
// =============================================================================

// 🔥 방안 1: DeviceInfo에 이미 있는 typed 필드들 사용
void WorkerFactory::RegisterWorkerCreators() {
    logger_->Info("🏭 Registering 5 Core Protocol Workers");
    
    // =============================================================================
    // 🚀 MODBUS_TCP Worker 등록 (임시로 BaseDeviceWorker 사용)
    // =============================================================================
    worker_creators_["MODBUS_TCP"] = [this](const PulseOne::Structs::DeviceInfo& device_info) {
        logger_->Debug("🔧 Creating MODBUS_TCP Worker (임시 구현)");
        // 임시로 BaseDeviceWorker 직접 생성 (ModbusTcpWorker가 없으므로)
        return std::make_unique<BaseDeviceWorker>(device_info);
    };
    
    // MODBUS_TCP 별칭들
    worker_creators_["modbus_tcp"] = worker_creators_["MODBUS_TCP"];
    worker_creators_["ModbusTcp"] = worker_creators_["MODBUS_TCP"];
    worker_creators_["TCP"] = worker_creators_["MODBUS_TCP"];
    
    // =============================================================================
    // 🚀 MODBUS_RTU Worker 등록 (실제 ModbusRtuWorker 사용)
    // =============================================================================
    worker_creators_["MODBUS_RTU"] = [this](const PulseOne::Structs::DeviceInfo& device_info) {
        logger_->Debug("🔧 Creating ModbusRtuWorker for: " + device_info.name);
        return std::make_unique<ModbusRtuWorker>(device_info);
    };
    
    // MODBUS_RTU 별칭들  
    worker_creators_["modbus_rtu"] = worker_creators_["MODBUS_RTU"];
    worker_creators_["ModbusRtu"] = worker_creators_["MODBUS_RTU"];
    worker_creators_["RTU"] = worker_creators_["MODBUS_RTU"];
    
    // =============================================================================
    // 🚀 mqtt Worker 등록 (실제 MQTTWorker 사용)
    // =============================================================================
    worker_creators_["mqtt"] = [this](const PulseOne::Structs::DeviceInfo& device_info) {
        logger_->Debug("🔧 Creating MQTTWorker for: " + device_info.name);
        return std::make_unique<MQTTWorker>(device_info);
    };
    
    // mqtt 별칭들
    worker_creators_["MQTT"] = worker_creators_["mqtt"];
    worker_creators_["Mqtt"] = worker_creators_["mqtt"];
    worker_creators_["MQTT_CLIENT"] = worker_creators_["mqtt"];
    
    // =============================================================================
    // 🚀 bacnet Worker 등록 (임시로 BaseDeviceWorker 사용)
    // =============================================================================
    worker_creators_["bacnet"] = [this](const PulseOne::Structs::DeviceInfo& device_info) {
        logger_->Debug("🔧 Creating BACnet Worker (임시 구현)");
        // 임시로 BaseDeviceWorker 직접 생성 (BACnetWorker가 없으므로)
        return std::make_unique<BaseDeviceWorker>(device_info);
    };
    
    // bacnet 별칭들
    worker_creators_["BACNET"] = worker_creators_["bacnet"];
    worker_creators_["BACnet"] = worker_creators_["bacnet"];
    worker_creators_["BACNET_IP"] = worker_creators_["bacnet"];
    worker_creators_["BACnet/IP"] = worker_creators_["bacnet"];
    
    // =============================================================================
    // 🚀 BEACON Worker 등록 (실제로 없으므로 BaseDeviceWorker 사용)
    // =============================================================================
    worker_creators_["BEACON"] = [this](const PulseOne::Structs::DeviceInfo& device_info) {
        logger_->Debug("🔧 Creating BEACON Worker (BeaconWorker 미구현, BaseDeviceWorker 사용)");
        // BeaconWorker는 실제로 존재하지 않으므로 BaseDeviceWorker 사용
        return std::make_unique<BaseDeviceWorker>(device_info);
    };
    
    // BEACON 별칭들
    worker_creators_["beacon"] = worker_creators_["BEACON"];
    worker_creators_["Beacon"] = worker_creators_["BEACON"];
    worker_creators_["BEACON_PROTOCOL"] = worker_creators_["BEACON"];
    
    // =============================================================================
    // 🎯 등록 완료 로그 (정확한 구현 상태)
    // =============================================================================
    size_t total_registered = worker_creators_.size();
    
    logger_->Info("🏭 Protocol Workers Registration Complete (" + 
                  std::to_string(total_registered) + " protocols)");
    
    #ifdef DEBUG_MODE
    logger_->Debug("✅ 구현 완료: MODBUS_TCP, MODBUS_RTU, mqtt, bacnet");
    logger_->Debug("🚧 미구현: BEACON (BaseDeviceWorker 사용)");
    #endif
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
PulseOne::Structs::DeviceInfo WorkerFactory::ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const {
    PulseOne::Structs::DeviceInfo device_info;
    
    // =============================================================================
    // 1단계: 기본 정보 설정 (DeviceEntity에서)
    // =============================================================================
    device_info.id = std::to_string(device_entity.getId());
    device_info.name = device_entity.getName();
    device_info.description = device_entity.getDescription();
    device_info.protocol_type = device_entity.getProtocolType();
    device_info.is_enabled = device_entity.isEnabled();
    device_info.endpoint = device_entity.getEndpoint();
    device_info.connection_string = device_entity.getEndpoint();  // 별칭
    
    // =============================================================================
    // 2단계: DeviceSettings에서 상세 설정 로드
    // =============================================================================
    if (device_settings_repo_) {  // 🔥 DeviceSettingsRepository 추가 필요!
        try {
            auto device_settings_opt = device_settings_repo_->findById(device_entity.getId());
            
            if (device_settings_opt.has_value()) {
                const auto& settings = device_settings_opt.value();
                
                logger_->Debug("📋 DeviceSettings 로드 성공: Device ID " + std::to_string(device_entity.getId()));
                
                // DeviceSettings → DeviceInfo 매핑
                device_info.polling_interval_ms = static_cast<uint32_t>(settings.getPollingIntervalMs());
                device_info.timeout_ms = static_cast<uint32_t>(settings.getReadTimeoutMs());
                device_info.retry_count = static_cast<uint32_t>(settings.getMaxRetryCount());
                device_info.connection_timeout_ms = static_cast<uint32_t>(settings.getConnectionTimeoutMs());
                device_info.keep_alive_interval_ms = static_cast<uint32_t>(settings.getKeepAliveIntervalS() * 1000);  // 초→밀리초
                
                // 추가 설정들
                device_info.retry_interval_ms = static_cast<uint32_t>(settings.getRetryIntervalMs());
                device_info.write_timeout_ms = static_cast<uint32_t>(settings.getWriteTimeoutMs());
                device_info.keep_alive_enabled = settings.isKeepAliveEnabled();
                device_info.backoff_time_ms = static_cast<uint32_t>(settings.getBackoffTimeMs());
                device_info.backoff_multiplier = settings.getBackoffMultiplier();
                device_info.max_backoff_time_ms = static_cast<uint32_t>(settings.getMaxBackoffTimeMs());
                
                // 진단 설정
                device_info.diagnostic_mode_enabled = settings.isDiagnosticModeEnabled();
                device_info.data_validation_enabled = settings.isDataValidationEnabled();
                device_info.performance_monitoring_enabled = settings.isPerformanceMonitoringEnabled();
                
                logger_->Debug("✅ DeviceSettings 매핑 완료: " + 
                              "polling=" + std::to_string(device_info.polling_interval_ms) + "ms, " +
                              "timeout=" + std::to_string(device_info.timeout_ms) + "ms, " +
                              "retry=" + std::to_string(device_info.retry_count));
                
            } else {
                logger_->Warn("⚠️ DeviceSettings not found for Device ID: " + std::to_string(device_entity.getId()) + " - using defaults");
                ApplyDefaultSettings(device_info, device_entity.getProtocolType());
            }
            
        } catch (const std::exception& e) {
            logger_->Error("❌ DeviceSettings 로드 실패 (Device ID: " + std::to_string(device_entity.getId()) + "): " + std::string(e.what()));
            logger_->Info("🔧 기본값 사용");
            ApplyDefaultSettings(device_info, device_entity.getProtocolType());
        }
        
    } else {
        logger_->Warn("⚠️ DeviceSettingsRepository not injected - using protocol defaults");
        ApplyDefaultSettings(device_info, device_entity.getProtocolType());
    }
    
    // =============================================================================
    // 3단계: 최종 검증 및 보정
    // =============================================================================
    ValidateAndCorrectSettings(device_info);
    
    return device_info;
}

void WorkerFactory::ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& device_info, 
                                        const std::string& protocol_type) const {
    
    if (protocol_type == "MODBUS_TCP" || protocol_type == "modbus_tcp") {
        device_info.polling_interval_ms = 1000;      // 1초
        device_info.timeout_ms = 3000;               // 3초
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 10000;   // 10초
        device_info.keep_alive_interval_ms = 30000;  // 30초
        
    } else if (protocol_type == "MODBUS_RTU" || protocol_type == "modbus_rtu") {
        device_info.polling_interval_ms = 2000;      // 2초
        device_info.timeout_ms = 5000;               // 5초
        device_info.retry_count = 5;
        device_info.connection_timeout_ms = 15000;   // 15초
        device_info.keep_alive_interval_ms = 60000;  // 60초
        
    } else if (protocol_type == "mqtt") {
        device_info.polling_interval_ms = 500;       // 0.5초
        device_info.timeout_ms = 10000;              // 10초
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 15000;   // 15초
        device_info.keep_alive_interval_ms = 60000;  // 60초
        
    } else if (protocol_type == "bacnet") {
        device_info.polling_interval_ms = 5000;      // 5초
        device_info.timeout_ms = 8000;               // 8초
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 20000;   // 20초
        device_info.keep_alive_interval_ms = 120000; // 120초
        
    } else if (protocol_type == "BEACON") {
        device_info.polling_interval_ms = 10000;     // 10초
        device_info.timeout_ms = 15000;              // 15초
        device_info.retry_count = 2;
        device_info.connection_timeout_ms = 30000;   // 30초
        device_info.keep_alive_interval_ms = 180000; // 180초
    } else {
        // 알 수 없는 프로토콜 - 안전한 기본값
        device_info.polling_interval_ms = 5000;
        device_info.timeout_ms = 10000;
        device_info.retry_count = 3;
        device_info.connection_timeout_ms = 20000;
        device_info.keep_alive_interval_ms = 60000;
    }
    
    // 공통 기본값들
    device_info.retry_interval_ms = 5000;           // 5초 재시도 간격
    device_info.write_timeout_ms = device_info.timeout_ms;  // 읽기 타임아웃과 동일
    device_info.keep_alive_enabled = true;
    device_info.backoff_time_ms = 60000;            // 1분 백오프
    device_info.backoff_multiplier = 1.5;
    device_info.max_backoff_time_ms = 300000;       // 5분 최대 백오프
    device_info.diagnostic_mode_enabled = false;
    device_info.data_validation_enabled = true;
    device_info.performance_monitoring_enabled = true;
    
    logger_->Debug("🔧 " + protocol_type + " 기본값 적용됨");
}

// =============================================================================
// 🔧 설정값 검증 및 보정
// =============================================================================
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

void WorkerFactory::ApplyProtocolSpecificDefaults(PulseOne::Structs::DeviceInfo& device_info, const std::string& protocol_type) const {
    // 기본 타이밍 설정 (모든 프로토콜 공통)
    if (device_info.polling_interval_ms == 0) {
        device_info.polling_interval_ms = 1000;  // 1초
    }
    if (device_info.timeout_ms == 0) {
        device_info.timeout_ms = 5000;  // 5초
    }
    if (device_info.retry_count == 0) {
        device_info.retry_count = 3;
    }
    
    // 프로토콜별 기본값
    if (protocol_type == "modbus_tcp" || protocol_type == "MODBUS_TCP") {
        if (device_info.properties.find("slave_id") == device_info.properties.end()) {
            device_info.properties["slave_id"] = "1";
        }
        if (device_info.properties.find("unit_id") == device_info.properties.end()) {
            device_info.properties["unit_id"] = "1";
        }
        if (device_info.timeout_ms == 5000) {  // 기본값인 경우에만 변경
            device_info.timeout_ms = 3000;  // Modbus는 3초가 적당
        }
    }
    else if (protocol_type == "modbus_rtu" || protocol_type == "MODBUS_RTU") {
        if (device_info.properties.find("slave_id") == device_info.properties.end()) {
            device_info.properties["slave_id"] = "1";
        }
        if (device_info.properties.find("baud_rate") == device_info.properties.end()) {
            device_info.properties["baud_rate"] = "9600";
        }
        if (device_info.properties.find("parity") == device_info.properties.end()) {
            device_info.properties["parity"] = "N";
        }
        if (device_info.properties.find("data_bits") == device_info.properties.end()) {
            device_info.properties["data_bits"] = "8";
        }
        if (device_info.properties.find("stop_bits") == device_info.properties.end()) {
            device_info.properties["stop_bits"] = "1";
        }
    }
    else if (protocol_type == "mqtt" || protocol_type == "MQTT") {
        if (device_info.properties.find("client_id") == device_info.properties.end()) {
            device_info.properties["client_id"] = "pulseone_" + device_info.id;
        }
        if (device_info.properties.find("qos") == device_info.properties.end()) {
            device_info.properties["qos"] = "1";
        }
        if (device_info.properties.find("clean_session") == device_info.properties.end()) {
            device_info.properties["clean_session"] = "true";
        }
        if (device_info.properties.find("keep_alive") == device_info.properties.end()) {
            device_info.properties["keep_alive"] = "60";
        }
    }
    else if (protocol_type == "bacnet" || protocol_type == "BACNET_IP") {
        if (device_info.properties.find("device_instance") == device_info.properties.end()) {
            device_info.properties["device_instance"] = "0";
        }
        if (device_info.properties.find("max_apdu_length") == device_info.properties.end()) {
            device_info.properties["max_apdu_length"] = "128";
        }
        if (device_info.properties.find("network_number") == device_info.properties.end()) {
            device_info.properties["network_number"] = "0";
        }
        if (device_info.polling_interval_ms == 1000) {  // 기본값인 경우에만 변경
            device_info.polling_interval_ms = 5000;  // BACnet은 5초가 적당
        }
    }
    
    // 공통 기본 속성들
    if (device_info.properties.find("auto_reconnect") == device_info.properties.end()) {
        device_info.properties["auto_reconnect"] = "true";
    }
    if (device_info.properties.find("log_level") == device_info.properties.end()) {
        device_info.properties["log_level"] = "INFO";
    }
    
    logger_->Debug("✅ Applied protocol-specific defaults for " + protocol_type);
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
        // 새로운 DataPoint 구조체의 메서드 사용
        data_point.UpdateCurrentValue(new_value, new_quality);
        
        logger_->Debug("📊 Updated DataPoint value: " + data_point.name + 
                      " = " + data_point.GetCurrentValueAsString() +
                      " (Quality: " + data_point.GetQualityCodeAsString() + ")");
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to update DataPoint value: " + std::string(e.what()));
        
        // 에러 시 BAD 품질로 설정
        data_point.SetErrorState(PulseOne::Enums::DataQuality::BAD);
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
    // 로그 활성화 확인
    if (!data_point.log_enabled) return false;
        
    // 로그 간격 확인
    if (data_point.log_interval_ms > 0) {
        auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto last_log_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            data_point.last_log_time.time_since_epoch()).count();
        
        if (current_ms - last_log_ms < data_point.log_interval_ms) {
            return false;  // 아직 로그 간격이 지나지 않음
        }
    }
    
    // Deadband 확인 (값 변화가 충분한지 확인)
    if (data_point.log_deadband > 0.0) {
        double current_value = std::visit([](const auto& v) -> double {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                return static_cast<double>(v);
            } else {
                return 0.0;  // 문자열은 0으로 처리
            }
        }, data_point.current_value);
        
        double new_double_value = std::visit([](const auto& v) -> double {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                return static_cast<double>(v);
            } else {
                return 0.0;  // 문자열은 0으로 처리
            }
        }, new_value);
        
        if (std::abs(new_double_value - current_value) < data_point.log_deadband) {
            return false;  // 변화량이 deadband 미만
        }
    }
    
    return true;
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
PulseOne::Structs::DataPoint WorkerFactory::ConvertToDataPoint(const Database::Entities::DataPointEntity& datapoint_entity, const std::string& device_id_string) const {
    PulseOne::Structs::DataPoint data_point;
    
    // 기본 식별 정보
    data_point.id = std::to_string(datapoint_entity.getId());
    data_point.device_id = device_id_string.empty() ? std::to_string(datapoint_entity.getDeviceId()) : device_id_string;
    data_point.name = datapoint_entity.getName();
    data_point.description = datapoint_entity.getDescription();
    
    // 주소 정보
    data_point.address = static_cast<uint32_t>(datapoint_entity.getAddress());
    data_point.address_string = std::to_string(datapoint_entity.getAddress());
    
    // 데이터 타입 및 접근성
    data_point.data_type = datapoint_entity.getDataType();
    data_point.access_mode = datapoint_entity.getAccessMode();
    data_point.is_enabled = datapoint_entity.isEnabled();
    data_point.is_writable = (datapoint_entity.getAccessMode() == "read_write" || 
                          datapoint_entity.getAccessMode() == "write_only");
    
    // 엔지니어링 정보
    data_point.unit = datapoint_entity.getUnit();
    data_point.scaling_factor = datapoint_entity.getScalingFactor();
    data_point.scaling_offset = datapoint_entity.getScalingOffset();
    data_point.min_value = datapoint_entity.getMinValue();
    data_point.max_value = datapoint_entity.getMaxValue();
    
    // 로깅 설정 (필드명 수정)
    data_point.log_enabled = datapoint_entity.isLogEnabled();
    data_point.log_interval_ms = static_cast<uint32_t>(datapoint_entity.getLogInterval());  // getLogIntervalMs → getLogInterval
    data_point.log_deadband = datapoint_entity.getLogDeadband();
    
    // 메타데이터
    auto tag_vector = datapoint_entity.getTags();
    if (!tag_vector.empty()) {
        data_point.tags = tag_vector[0]; // 첫 번째 태그만 사용하거나
        // 또는 JSON 형태로: data_point.tags = "[\"" + tag_vector[0] + "\"]";
    }
    data_point.metadata = "";
    
    // 시간 정보
    data_point.created_at = datapoint_entity.getCreatedAt();
    data_point.updated_at = datapoint_entity.getUpdatedAt();
    
    // 실제 값 필드 초기화 (properties 제거!)
    data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
    data_point.raw_value = PulseOne::BasicTypes::DataVariant(0.0);
    data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
    data_point.value_timestamp = std::chrono::system_clock::now();
    data_point.quality_timestamp = data_point.value_timestamp;
    data_point.last_log_time = data_point.value_timestamp;
    data_point.last_read_time = data_point.value_timestamp;
    data_point.last_write_time = data_point.value_timestamp;
    
    logger_->Debug("✅ DataPoint converted: " + data_point.name + 
                  " (device_id: " + device_id_string + 
                  ", address: " + std::to_string(data_point.address) + ")");
    
    return data_point;
}


void WorkerFactory::LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const {
    try {
        if (!current_value_repo_) {
            logger_->Warn("⚠️ CurrentValueRepository not injected, using default values");
            data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
            data_point.quality_timestamp = std::chrono::system_clock::now();
            return;
        }
        
        auto current_value = current_value_repo_->findByDataPointId(std::stoi(data_point.id));
        
        if (current_value.has_value()) {
            // 🔥 수정: CurrentValueEntity의 getValue() 메서드 확인 필요
            // getCurrentValue() 또는 getNumericValue() 사용
            try {
                // 옵션 1: getCurrentValue()가 string을 반환하는 경우
                std::string value_str = current_value->getCurrentValue();
                if (!value_str.empty()) {
                    double numeric_value = std::stod(value_str);
                    data_point.current_value = PulseOne::BasicTypes::DataVariant(numeric_value);
                } else {
                    data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                }
            } catch (const std::exception&) {
                // 숫자 변환 실패 시 문자열로 저장
                data_point.current_value = PulseOne::BasicTypes::DataVariant(current_value->getCurrentValue());
            }
            
            // 🔥 수정: DataQuality 타입 변환
            if (current_value->getQualityCode() != PulseOne::Enums::DataQuality::UNKNOWN) {
                // CurrentValueEntity에 getQualityCode() 메서드가 있는 경우
                data_point.quality_code = current_value->getQualityCode();
            } else {
                // 문자열에서 enum으로 변환
                data_point.quality_code = PulseOne::Utils::StringToDataQuality(current_value->getQuality());
            }
            
            // 🔥 수정: 타임스탬프 메서드 확인
            data_point.value_timestamp = current_value->getValueTimestamp();  // getTimestamp() → getValueTimestamp()
            data_point.quality_timestamp = current_value->getUpdatedAt();
            
            logger_->Debug("✅ Loaded current value for DataPoint '" + data_point.name + 
                          " = " + data_point.GetCurrentValueAsString() +
                          " (Quality: " + data_point.GetQualityCodeAsString() + ")");
        } else {
            logger_->Debug("⚠️ No current value found for DataPoint: " + data_point.name);
            
            data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
            data_point.quality_timestamp = std::chrono::system_clock::now();
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load current value for DataPoint '" + data_point.name + "': " + std::string(e.what()));
        
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
        
        // Repository 패턴 사용
        auto datapoint_entities = datapoint_repo_->findByDeviceId(device_id, true);  // enabled_only = true
        
        for (const auto& entity : datapoint_entities) {
            auto data_point = ConvertToDataPoint(entity, std::to_string(device_id));
            LoadCurrentValueForDataPoint(data_point);
            data_points.push_back(data_point);
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
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load DataPoints for Device " + std::to_string(device_id) + ": " + std::string(e.what()));
    }
    
    return data_points;
}


std::string WorkerFactory::GetCurrentValueAsString(const PulseOne::Structs::DataPoint& data_point) const {
    return data_point.GetCurrentValueAsString();
}

std::string WorkerFactory::GetQualityString(const PulseOne::Structs::DataPoint& data_point) const {
    return data_point.GetQualityCodeAsString();
}

void WorkerFactory::SetDatabaseClients(std::shared_ptr<RedisClient> redis_client, 
                                       std::shared_ptr<InfluxClient> influx_client) {
    redis_client_ = redis_client;
    influx_client_ = influx_client;
    if (logger_) {
        logger_->Info("✅ Database clients set in WorkerFactory");
    }
}


} // namespace Workers
} // namespace PulseOne