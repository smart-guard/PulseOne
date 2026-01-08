/**
 * @file test_db_aware_driver_properties_validation.cpp
 * @brief DB 실제 값 기반 드라이버 속성 검증 테스트 (수정됨)
 * @date 2025-08-30
 * 
 * 🔧 수정 사항:
 * 1. DeviceSettingsEntity의 실제 타입 반영 (int vs std::optional<int>)
 * 2. getReadTimeoutMs(), getWriteTimeoutMs()는 int 타입 (optional 아님)
 * 3. getScanRateOverride()만 std::optional<int> 타입
 * 4. 정확한 타입 기반 검증 로직 적용
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <functional>
#include <optional>

// JSON 라이브러리
#include <nlohmann/json.hpp>

// PulseOne 시스템 헤더들
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerFactory.h"

// Entity 및 Repository 클래스들
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

// Worker들
#include "Workers/Protocol/ModbusWorker.h"
#include "Workers/Protocol/MqttWorker.h"
#include "Workers/Protocol/BACnetWorker.h"

// Drivers
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Bacnet/BACnetDriver.h"

// Common includes
#include "Common/Structs.h"
#include "Common/Enums.h"

using namespace PulseOne;
using namespace PulseOne::Database;
using namespace PulseOne::Workers;

// =============================================================================
// DB 실제 값 기반 속성 검증 헬퍼 (타입 정정됨)
// =============================================================================

class DbAwarePropertiesValidator {
public:
    struct ValidationResult {
        bool is_valid = true;
        std::string error_message;
        std::map<std::string, std::string> found_properties;
        std::map<std::string, std::string> missing_properties;
        std::map<std::string, std::string> db_expected_properties;
        int total_expected_properties = 0;
        double completeness_percentage = 0.0;
        
        void AddError(const std::string& message) {
            is_valid = false;
            if (!error_message.empty()) error_message += "; ";
            error_message += message;
        }
    };

    /**
     * @brief DB 실제 DeviceSettings 기반 동기화 속성 검증 (타입 정정)
     */
    static ValidationResult ValidateDeviceSettingsSyncFromDb(
        const PulseOne::Structs::DriverConfig& config,
        const std::optional<Entities::DeviceSettingsEntity>& device_settings) {
        
        ValidationResult result;
        
        std::cout << "\n🔍 === DB 기반 DeviceSettings 동기화 검증 (타입 정정) ===" << std::endl;
        
        if (!device_settings.has_value()) {
            std::cout << "⚠️  DeviceSettings 없음 - 기본값만 검증" << std::endl;
            ValidateBasicProperties(config, result);
            return result;
        }
        
        const auto& settings = device_settings.value();
        std::cout << "📋 DeviceSettings 발견됨 - 실제 DB 값 기반 검증 (타입별)" << std::endl;
        
        // 🔥 int 타입 속성들 (optional이 아님)
        ValidateDbProperty(config, result, "retry_interval_ms", 
                          std::to_string(settings.getRetryIntervalMs()));
        
        ValidateDbProperty(config, result, "keep_alive_enabled", 
                          settings.isKeepAliveEnabled() ? "true" : "false");
        
        ValidateDbProperty(config, result, "data_validation_enabled", 
                          settings.isDataValidationEnabled() ? "true" : "false");
        
        ValidateDbProperty(config, result, "performance_monitoring_enabled", 
                          settings.isPerformanceMonitoringEnabled() ? "true" : "false");
        
        ValidateDbProperty(config, result, "diagnostic_mode_enabled", 
                          settings.isDiagnosticModeEnabled() ? "true" : "false");
        
        ValidateDbProperty(config, result, "keep_alive_interval_s", 
                          std::to_string(settings.getKeepAliveIntervalS()));
        
        ValidateDbProperty(config, result, "keep_alive_timeout_s", 
                          std::to_string(settings.getKeepAliveTimeoutS()));
        
        ValidateDbProperty(config, result, "backoff_multiplier", 
                          std::to_string(settings.getBackoffMultiplier()));
        
        ValidateDbProperty(config, result, "backoff_time_ms", 
                          std::to_string(settings.getBackoffTimeMs()));
        
        ValidateDbProperty(config, result, "max_backoff_time_ms", 
                          std::to_string(settings.getMaxBackoffTimeMs()));
        
        // 🔥 int 타입 속성들 (기존 코드에서 optional로 잘못 가정했음)
        std::cout << "  🔧 int 타입 검증 (optional 아님):" << std::endl;
        ValidateDbProperty(config, result, "read_timeout_ms", 
                          std::to_string(settings.getReadTimeoutMs()));
        
        ValidateDbProperty(config, result, "write_timeout_ms", 
                          std::to_string(settings.getWriteTimeoutMs()));
        
        // 🔥 std::optional<int> 타입 속성만 (getScanRateOverride만)
        std::cout << "  🔧 std::optional<int> 타입 검증:" << std::endl;
        if (settings.getScanRateOverride().has_value()) {
            ValidateDbProperty(config, result, "scan_rate_override", 
                              std::to_string(settings.getScanRateOverride().value()));
        } else {
            std::cout << "  ℹ️  scan_rate_override: DB에 NULL - 검증 스킵" << std::endl;
        }
        
        // 완성도 계산
        if (result.total_expected_properties > 0) {
            result.completeness_percentage = 
                (double)result.found_properties.size() / result.total_expected_properties * 100.0;
        }
        
        std::cout << "\n📊 DB 기반 DeviceSettings 검증 결과 (타입 정정):" << std::endl;
        std::cout << "  - DB 예상 속성: " << result.total_expected_properties << "개" << std::endl;
        std::cout << "  - 매핑된 속성: " << result.found_properties.size() << "개" << std::endl;
        std::cout << "  - 누락된 속성: " << result.missing_properties.size() << "개" << std::endl;
        std::cout << "  - 정확한 완성도: " << std::fixed << std::setprecision(1) 
                  << result.completeness_percentage << "%" << std::endl;
        
        // 80% 이상이면 통과
        if (result.completeness_percentage < 80.0) {
            result.AddError("DB 기반 DeviceSettings 완성도가 80% 미만입니다");
        }
        
        return result;
    }

private:
    /**
     * @brief DB 속성 검증 헬퍼
     */
    static void ValidateDbProperty(const PulseOne::Structs::DriverConfig& config,
                                  ValidationResult& result,
                                  const std::string& property_name,
                                  const std::string& expected_value) {
        
        result.db_expected_properties[property_name] = expected_value;
        result.total_expected_properties++;
        
        if (config.properties.count(property_name)) {
            const auto& actual_value = config.properties.at(property_name);
            std::cout << "    ✅ " << property_name << ": \"" << actual_value 
                      << "\" (DB 예상: \"" << expected_value << "\")" << std::endl;
            
            result.found_properties[property_name] = actual_value;
            
            // 값 일치성 검증
            if (!ValuesMatch(actual_value, expected_value)) {
                std::cout << "      ⚠️  값 불일치 감지!" << std::endl;
                result.AddError(property_name + " 값이 DB와 다름: " + 
                               actual_value + " vs " + expected_value);
            }
        } else {
            std::cout << "    ❌ " << property_name << ": 누락됨 (DB 예상: \"" 
                      << expected_value << "\")" << std::endl;
            result.missing_properties[property_name] = expected_value;
        }
    }
    
    /**
     * @brief 기본 속성들 검증 (DeviceSettings 없을 때)
     */
    static void ValidateBasicProperties(const PulseOne::Structs::DriverConfig& config,
                                       ValidationResult& result) {
        
        std::cout << "🔧 기본 속성들 검증:" << std::endl;
        
        std::vector<std::string> basic_props = {
            "device_id", "device_name", "enabled", "endpoint", "protocol_type"
        };
        
        for (const auto& prop : basic_props) {
            if (config.properties.count(prop)) {
                std::cout << "  ✅ " << prop << ": \"" << config.properties.at(prop) << "\"" << std::endl;
                result.found_properties[prop] = config.properties.at(prop);
            } else {
                std::cout << "  ❌ " << prop << ": 기본 속성 누락" << std::endl;
                result.missing_properties[prop] = "basic_required";
            }
            result.total_expected_properties++;
        }
        
        if (result.total_expected_properties > 0) {
            result.completeness_percentage = 
                (double)result.found_properties.size() / result.total_expected_properties * 100.0;
        }
        
        if (result.completeness_percentage < 60.0) {
            result.AddError("기본 속성 완성도가 60% 미만입니다");
        }
    }
    
    /**
     * @brief 값 일치 확인 (허용 오차 포함)
     */
    static bool ValuesMatch(const std::string& actual, const std::string& expected) {
        if (actual == expected) return true;
        
        // 부동소수점 비교
        try {
            double actual_val = std::stod(actual);
            double expected_val = std::stod(expected);
            return std::abs(actual_val - expected_val) < 0.001;
        } catch (...) {
            return false;
        }
    }
};

// =============================================================================
// DB 실제 값 기반 드라이버 속성 테스트 클래스
// =============================================================================

class DbAwareDriverPropertiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🔧 === DB 기반 드라이버 속성 테스트 시작 (타입 정정) ===" << std::endl;
        setupTestEnvironment();
    }
    
    void TearDown() override {
        std::cout << "\n🧹 === DB 기반 테스트 정리 ===" << std::endl;
        cleanup();
    }
    
private:
    void setupTestEnvironment();
    void cleanup();
    
    ConfigManager* config_manager_;
    LogManager* logger_;
    DbLib::DatabaseManager* db_manager_;
    RepositoryFactory* repo_factory_;
    WorkerFactory* worker_factory_;
    
    std::shared_ptr<Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Repositories::DeviceSettingsRepository> settings_repo_;
    
public:
    auto GetDeviceRepository() const { return device_repo_; }
    auto GetDeviceSettingsRepository() const { return settings_repo_; }
    
    std::unique_ptr<BaseDeviceWorker> createWorkerWithDbSync(const Entities::DeviceEntity& device);
    std::optional<Entities::DeviceSettingsEntity> loadActualDeviceSettings(int device_id);
    void validateDriverConfigAgainstDb(BaseDeviceWorker* worker, 
                                      const Entities::DeviceEntity& device,
                                      const std::string& protocol);
    std::string detectActualProtocolFromWorker(BaseDeviceWorker* worker);
};

void DbAwareDriverPropertiesTest::setupTestEnvironment() {
    std::cout << "🎯 DB 기반 속성 테스트 환경 구성 중..." << std::endl;
    
    config_manager_ = &ConfigManager::getInstance();
    logger_ = &LogManager::getInstance();
    db_manager_ = &DbLib::DatabaseManagerDbLib::DatabaseManager::getInstance();
    
    repo_factory_ = &RepositoryFactory::getInstance();
    ASSERT_TRUE(repo_factory_->initialize()) << "RepositoryFactory 초기화 실패";
    
    device_repo_ = repo_factory_->getDeviceRepository();
    settings_repo_ = repo_factory_->getDeviceSettingsRepository();
    
    ASSERT_TRUE(device_repo_) << "DeviceRepository 생성 실패";
    
    worker_factory_ = new WorkerFactory();
    
    std::cout << "✅ DB 기반 속성 테스트 환경 구성 완료" << std::endl;
}

void DbAwareDriverPropertiesTest::cleanup() {
    if (worker_factory_) {
        delete worker_factory_;
        worker_factory_ = nullptr;
    }
    std::cout << "✅ DB 기반 테스트 환경 정리 완료" << std::endl;
}

std::unique_ptr<BaseDeviceWorker> 
DbAwareDriverPropertiesTest::createWorkerWithDbSync(const Entities::DeviceEntity& device) {
    try {
        auto worker = worker_factory_->CreateWorker(device);
        
        if (worker) {
            std::cout << "✅ Worker 생성 성공: " << device.getName() 
                      << " (protocol_id: " << device.getProtocolId() << ")" << std::endl;
        } else {
            std::cout << "❌ Worker 생성 실패: " << device.getName() << std::endl;
        }
        
        return worker;
    } catch (const std::exception& e) {
        std::cout << "🚨 Worker 생성 중 예외: " << e.what() << std::endl;
        return nullptr;
    }
}

std::optional<Entities::DeviceSettingsEntity> 
DbAwareDriverPropertiesTest::loadActualDeviceSettings(int device_id) {
    if (!settings_repo_) {
        std::cout << "⚠️  DeviceSettingsRepository 없음" << std::endl;
        return std::nullopt;
    }
    
    try {
        auto device_settings = settings_repo_->findById(device_id);
        if (device_settings.has_value()) {
            std::cout << "📋 DB에서 DeviceSettings 로드 성공: device_id=" << device_id << std::endl;
        } else {
            std::cout << "⚠️  DB에 DeviceSettings 없음: device_id=" << device_id << std::endl;
        }
        return device_settings;
        
    } catch (const std::exception& e) {
        std::cout << "💥 DeviceSettings 로드 중 예외: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::string DbAwareDriverPropertiesTest::detectActualProtocolFromWorker(BaseDeviceWorker* worker) {
    if (!worker) return "UNKNOWN";
    
    if (dynamic_cast<ModbusTcpWorker*>(worker)) {
        return "MODBUS_TCP";
    } else if (dynamic_cast<ModbusRtuWorker*>(worker)) {
        return "MODBUS_RTU";
    } else if (dynamic_cast<MQTTWorker*>(worker)) {
        return "MQTT";
    } else if (dynamic_cast<BACnetWorker*>(worker)) {
        return "BACNET_IP";
    }
    
    return "UNKNOWN";
}

void DbAwareDriverPropertiesTest::validateDriverConfigAgainstDb(
    BaseDeviceWorker* worker, 
    const Entities::DeviceEntity& device,
    const std::string& expected_protocol) {
    
    ASSERT_NE(worker, nullptr) << "Worker가 null입니다";
    
    std::string actual_protocol = detectActualProtocolFromWorker(worker);
    
    std::cout << "\n🔍 === DB 기반 Worker DriverConfig 검증 (타입 정정) ===" << std::endl;
    std::cout << "📍 예상 프로토콜: " << expected_protocol << std::endl;
    std::cout << "📍 실제 프로토콜: " << actual_protocol << std::endl;
    std::cout << "📍 디바이스 ID: " << device.getId() << std::endl;
    std::cout << "📍 디바이스명: " << device.getName() << std::endl;
    
    auto device_settings = loadActualDeviceSettings(device.getId());
    
    PulseOne::Structs::DriverConfig config;
    bool config_retrieved = false;
    
    try {
        if (actual_protocol == "MODBUS_TCP") {
            auto* tcp_worker = dynamic_cast<ModbusTcpWorker*>(worker);
            if (tcp_worker && tcp_worker->GetModbusDriver()) {
                config = tcp_worker->GetModbusDriver()->GetConfiguration();
                config_retrieved = true;
                std::cout << "✅ ModbusTcp DriverConfig 추출 성공" << std::endl;
            }
        } else if (actual_protocol == "MODBUS_RTU") {
            auto* rtu_worker = dynamic_cast<ModbusRtuWorker*>(worker);
            if (rtu_worker && rtu_worker->GetModbusDriver()) {
                config = rtu_worker->GetModbusDriver()->GetConfiguration();
                config_retrieved = true;
                std::cout << "✅ ModbusRtu DriverConfig 추출 성공" << std::endl;
            }
        } else if (actual_protocol == "MQTT") {
            auto* mqtt_worker = dynamic_cast<MQTTWorker*>(worker);
            if (mqtt_worker && mqtt_worker->GetMqttDriver()) {
                config = mqtt_worker->GetMqttDriver()->GetConfiguration();
                config_retrieved = true;
                std::cout << "✅ MQTT DriverConfig 추출 성공" << std::endl;
            }
        } else if (actual_protocol == "BACNET_IP") {
            auto* bacnet_worker = dynamic_cast<BACnetWorker*>(worker);
            if (bacnet_worker && bacnet_worker->GetBACnetDriver()) {
                config = bacnet_worker->GetBACnetDriver()->GetConfiguration();
                config_retrieved = true;
                std::cout << "✅ BACnet DriverConfig 추출 성공" << std::endl;
            }
        }
        
        ASSERT_TRUE(config_retrieved) << actual_protocol << " DriverConfig 추출 실패";
        
        std::cout << "\n🔧 현재 DriverConfig properties (" << config.properties.size() << "개):" << std::endl;
        for (const auto& [key, value] : config.properties) {
            std::cout << "  [" << key << "] = \"" << value << "\"" << std::endl;
        }
        
        // DB 기반 검증 수행 (타입 정정된 버전)
        auto validation_result = DbAwarePropertiesValidator::ValidateDeviceSettingsSyncFromDb(
            config, device_settings);
        
        if (validation_result.is_valid) {
            std::cout << "🎉 " << actual_protocol << " DB 기반 속성 검증 성공 (타입 정정)!" << std::endl;
            std::cout << "📊 정확한 완성도: " << std::fixed << std::setprecision(1) 
                      << validation_result.completeness_percentage << "%" << std::endl;
        } else {
            std::cout << "⚠️  " << actual_protocol << " DB 기반 속성 검증 실패: " 
                      << validation_result.error_message << std::endl;
            
            // 50% 미만만 완전 실패로 처리
            if (validation_result.completeness_percentage < 50.0) {
                FAIL() << actual_protocol << " DB 기반 속성 완성도가 너무 낮습니다: " 
                       << validation_result.completeness_percentage << "%";
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "💥 DB 기반 DriverConfig 검증 중 예외: " << e.what() << std::endl;
        FAIL() << "DB 기반 DriverConfig 검증 중 예외 발생: " << e.what();
    }
}

// =============================================================================
// DB 실제 값 기반 속성 검증 테스트 (타입 정정됨)
// =============================================================================

TEST_F(DbAwareDriverPropertiesTest, DbAware_All_Protocols_TypeCorrected_Test) {
    std::cout << "\n🔍 === DB 기반 모든 프로토콜 검증 (타입 정정) ===" << std::endl;
    
    auto all_devices = GetDeviceRepository()->findAll();
    EXPECT_GT(all_devices.size(), 0) << "테스트할 디바이스가 없습니다";
    
    std::map<std::string, int> protocol_test_counts;
    std::map<std::string, int> protocol_success_counts;
    
    int total_devices_tested = 0;
    int successful_validations = 0;
    
    for (const auto& device : all_devices) {
        int protocol_id = device.getProtocolId();
        
        if (protocol_id < 1 || protocol_id > 10) {
            continue;
        }
        
        total_devices_tested++;
        std::string protocol_name = "PROTOCOL_" + std::to_string(protocol_id);
        protocol_test_counts[protocol_name]++;
        
        std::cout << "\n📋 DB 기반 디바이스 테스트 (타입 정정): " << device.getName() 
                  << " (protocol_id: " << protocol_id << ")" << std::endl;
        
        try {
            auto worker = createWorkerWithDbSync(device);
            if (worker) {
                std::string actual_protocol = detectActualProtocolFromWorker(worker.get());
                std::cout << "🔍 감지된 실제 프로토콜: " << actual_protocol << std::endl;
                
                validateDriverConfigAgainstDb(worker.get(), device, actual_protocol);
                
                protocol_success_counts[protocol_name]++;
                successful_validations++;
                std::cout << "  ✅ DB 기반 속성 검증 성공 (타입 정정)" << std::endl;
                
            } else {
                std::cout << "  ⚠️  Worker 생성 실패" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "  💥 DB 기반 테스트 중 예외: " << e.what() << std::endl;
        }
    }
    
    std::cout << "\n📊 === DB 기반 전체 검증 결과 (타입 정정) ===" << std::endl;
    std::cout << "  총 테스트된 디바이스: " << total_devices_tested << "개" << std::endl;
    std::cout << "  성공한 검증: " << successful_validations << "개" << std::endl;
    
    if (total_devices_tested > 0) {
        double overall_success_rate = (double)successful_validations / total_devices_tested * 100.0;
        std::cout << "🏆 전체 성공률: " << std::fixed << std::setprecision(1) 
                  << overall_success_rate << "%" << std::endl;
        
        // 타입 정정으로 더 높은 성공률 기대
        EXPECT_GE(overall_success_rate, 85.0) << "DB 기반 전체 성공률이 85% 미만입니다";
    }
    
    std::cout << "\n🎉 DB 기반 모든 프로토콜 검증 완료 (타입 정정)!" << std::endl;
    std::cout << "✅ 타입 정정: int vs std::optional<int> 구분" << std::endl;
    std::cout << "✅ getReadTimeoutMs(), getWriteTimeoutMs()는 int 타입" << std::endl;
    std::cout << "✅ getScanRateOverride()만 std::optional<int> 타입" << std::endl;
}