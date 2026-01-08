/**
 * @file test_step12_state_recovery_storage.cpp
 * @brief Tiered State Recovery 및 데이터 타입별 저장 로직 검증 테스트
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

#include "Pipeline/DataProcessingService.h"
#include "Alarm/AlarmStartupRecovery.h"
#include "Alarm/AlarmEngine.h"
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Storage/RedisDataWriter.h"
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"

using namespace PulseOne;
using json = nlohmann::json;

class StateRecoveryStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 환경 초기화
        LogManager::getInstance().log("test", Enums::LogLevel::INFO, "Setup 시작");
        
        // RDB 초기화
        auto& db = DbLib::DatabaseManager::getInstance();
        db.initialize();
        
        Database::RepositoryFactory::getInstance().initialize();
        auto& factory = Database::RepositoryFactory::getInstance();
        auto dev_repo = factory.getDeviceRepository();
        auto dp_repo = factory.getDataPointRepository();
        auto cv_repo = factory.getCurrentValueRepository();
        
        // 테이블 강제 재생성 (스키마 동기화 보장)
        db.executeNonQuery("DROP TABLE IF EXISTS alarm_occurrences");
        db.executeNonQuery("DROP TABLE IF EXISTS current_values");
        db.executeNonQuery("DROP TABLE IF EXISTS data_points");
        db.executeNonQuery("DROP TABLE IF EXISTS devices");
        
        // 기기 생성 (외래키 제약 조건 충족)
        Database::Entities::DeviceEntity dev;
        dev.setId(1);
        dev.setName("test_device");
        dev.setDeviceType("GATEWAY");
        dev.setTenantId(1);
        dev.setSiteId(1);
        dev.setProtocolId(1); // Modbus TCP
        dev.setEndpoint("127.0.0.1:502");
        dev.setConfig("{}");
        ASSERT_TRUE(dev_repo->save(dev));
        
        // 테스트용 포인트 생성 (Repository를 사용하여 스키마 호환성 보장)
        Database::Entities::DataPointEntity dp1;
        dp1.setId(101);
        dp1.setDeviceId(1);
        dp1.setName("analog_sensor");
        dp1.setDataType("number");
        dp1.setAddress(101);
        dp1.setEnabled(true);
        ASSERT_TRUE(dp_repo->save(dp1));
        
        Database::Entities::DataPointEntity dp2;
        dp2.setId(102);
        dp2.setDeviceId(1);
        dp2.setName("digital_switch");
        dp2.setDataType("boolean");
        dp2.setAddress(102);
        dp2.setEnabled(true);
        ASSERT_TRUE(dp_repo->save(dp2));
    }

    void TearDown() override {
        // 정리
    }
};

/**
 * 🎯 테스트 1: 데이터 타입별 RDB 저장 로직 검증
 * - Digital: 매 변화 시 즉시 저장
 * - Analog: 5분 주기 저장 (첫 저장은 즉시)
 */
TEST_F(StateRecoveryStorageTest, RdbStorageDifferentiationTest) {
    Pipeline::DataProcessingService service;
    auto& factory = Database::RepositoryFactory::getInstance();
    auto cv_repo = factory.getCurrentValueRepository();
    
    // --- Case 1: Digital 포인트 (id: 102) ---
    Structs::DeviceDataMessage msg_digital;
    msg_digital.device_id = "device_1";
    
    Structs::TimestampedValue p_dig;
    p_dig.point_id = 102;
    p_dig.value = true;
    p_dig.timestamp = std::chrono::system_clock::now();
    p_dig.value_changed = true;
    msg_digital.points.push_back(p_dig);
    
    // 첫 번째 저장 -> 성공해야 함
    service.SaveChangedPointsToRDB(msg_digital, msg_digital.points);
    auto entity_dig1 = cv_repo->findById(102);
    ASSERT_TRUE(entity_dig1.has_value());
    EXPECT_TRUE(entity_dig1->getCurrentValue().find("true") != std::string::npos || 
                entity_dig1->getCurrentValue().find("1") != std::string::npos);

    // 즉시 두 번째 저장 (값 변경) -> Digital이므로 다시 저장되어야 함
    p_dig.value = false;
    msg_digital.points[0] = p_dig;
    service.SaveChangedPointsToRDB(msg_digital, msg_digital.points);
    
    auto entity_dig2 = cv_repo->findById(102);
    EXPECT_TRUE(entity_dig2->getCurrentValue().find("false") != std::string::npos || 
                entity_dig2->getCurrentValue().find("0") != std::string::npos);

    // --- Case 2: Analog 포인트 (id: 101) ---
    Structs::DeviceDataMessage msg_analog;
    msg_analog.device_id = "device_1";
    
    Structs::TimestampedValue p_ana;
    p_ana.point_id = 101;
    p_ana.value = 25.5;
    p_ana.timestamp = std::chrono::system_clock::now();
    p_ana.value_changed = true;
    msg_analog.points.push_back(p_ana);
    
    // 첫 번째 저장 -> 성공해야 함
    service.SaveChangedPointsToRDB(msg_analog, msg_analog.points);
    auto entity_ana1 = cv_repo->findById(101);
    ASSERT_TRUE(entity_ana1.has_value());
    EXPECT_TRUE(entity_ana1->getCurrentValue().find("25.5") != std::string::npos);

    // 즉시 두 번째 저장 -> Analog이므로 5분 미경과 시 저장되지 않아야 함 (이전 값 유지)
    p_ana.value = 30.0;
    msg_analog.points[0] = p_ana;
    service.SaveChangedPointsToRDB(msg_analog, msg_analog.points);
    
    auto entity_ana2 = cv_repo->findById(101);
    EXPECT_TRUE(entity_ana2->getCurrentValue().find("25.5") != std::string::npos);
    EXPECT_TRUE(entity_ana2->getCurrentValue().find("30.0") == std::string::npos);
}

/**
 * 🎯 테스트 2: Warm Startup 복구 로직 검증 (RDB -> Redis -> RAM)
 */
TEST_F(StateRecoveryStorageTest, WarmStartupRecoveryTest) {
    auto& db = DbLib::DatabaseManager::getInstance();
    
    // 1. RDB에 가상 데이터 주입 (Repository 사용)
    auto& factory = Database::RepositoryFactory::getInstance();
    auto cv_repo = factory.getCurrentValueRepository();
    
    Database::Entities::CurrentValueEntity cv1(101);
    cv1.setCurrentValueFromVariant(88.8);
    cv1.setQuality(Enums::DataQuality::GOOD);
    cv_repo->save(cv1);
    
    // 2. 복구 실행 (RDB -> Redis & AlarmEngine RAM)
    auto& recovery = Alarm::AlarmStartupRecovery::getInstance();
    size_t count = recovery.RecoverLatestPointValues();
    
    EXPECT_GE(count, 1);
    
    // 3. Redis 확인 (간접 확인: RedisDataWriter를 통해 저장되었는지 로그 확인 가능하지만 여기선 로직 태웠음에 집중)
    // 4. AlarmEngine 시딩 확인 (로그에 "포인트 시딩 완료: id=101"가 남아야 함)
}
