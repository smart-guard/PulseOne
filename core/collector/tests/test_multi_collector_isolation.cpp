/**
 * @file test_multi_collector_isolation.cpp
 * @brief Multi-Collector Isolation Test (Step 13/14)
 * @date 2026-01-14
 */

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <vector>

#include "Utils/ConfigManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Workers/WorkerScheduler.h"
#include "Workers/WorkerRegistry.h"
#include "Storage/RedisDataWriter.h"
#include "DatabaseManager.hpp"
#include "Database/SQLQueries.h"

using namespace PulseOne;
using namespace PulseOne::Database::Repositories;
using namespace PulseOne::Workers;

class MultiCollectorIsolationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 1. Config 초기화
        auto& config = ConfigManager::getInstance();
        config.initialize();
        
        // 2. Database 초기화
        auto& db = DbLib::DatabaseManager::getInstance();
        DbLib::DatabaseConfig db_config;
        db_config.sqlite_path = "test_multi_collector.db";
        db.initialize(db_config);
        
        // 2.5 RepositoryFactory 초기화 추가
        PulseOne::Database::RepositoryFactory::getInstance().initialize();
        
        // 3. 테이블 정리 및 초기화
        DbLib::DatabaseAbstractionLayer db_layer;
        db_layer.executeNonQuery("DROP TABLE IF EXISTS devices");
        db_layer.executeNonQuery(PulseOne::Database::SQL::Device::CREATE_TABLE);
    }

    void TearDown() override {
        // 리소스 정리
    }

    void AddTestDevice(int id, int tenant_id, int site_id, std::optional<int> edge_server_id, const std::string& name) {
        DbLib::DatabaseAbstractionLayer db_layer;
        std::string edge_id_str = edge_server_id.has_value() ? std::to_string(edge_server_id.value()) : "NULL";
        
        std::string query = "INSERT INTO devices (id, tenant_id, site_id, edge_server_id, name, device_type, protocol_id, endpoint, config, is_enabled) "
                           "VALUES (" + std::to_string(id) + ", " + std::to_string(tenant_id) + ", " + std::to_string(site_id) + ", " + 
                           edge_id_str + ", '" + name + "', 'SENSOR', 1, 'localhost:502', '{}', 1)";
        
        db_layer.executeNonQuery(query);
    }
};

TEST_F(MultiCollectorIsolationTest, Test_CollectorIsolation) {
    // 1. 테스트 데이터 준비
    // Collector 1001용 디바이스 2개
    AddTestDevice(1, 1, 1, 1001, "Device_A_Coll_1001");
    AddTestDevice(2, 1, 1, 1001, "Device_B_Coll_1001");
    
    // Collector 1002용 디바이스 1개
    AddTestDevice(3, 1, 1, 1002, "Device_C_Coll_1002");
    
    // 미할당 디바이스 1개 (edge_server_id IS NULL)
    AddTestDevice(4, 1, 1, std::nullopt, "Device_D_Unassigned");

    // 2. 현재 컬렉터 ID를 1001로 설정
    ConfigManager::getInstance().set("CSP_GATEWAY_BUILDING_ID", "1001");
    
    // 3. WorkerScheduler 실행
    auto registry = std::make_shared<WorkerRegistry>();
    auto redis_writer = std::make_shared<Storage::RedisDataWriter>(); // Mock or real Redis (test environment logic)
    WorkerScheduler scheduler(registry, redis_writer);
    
    int started_count = scheduler.StartAllActiveWorkers();
    
    // 4. 검증: Collector 1001은 2개의 디바이스만 시작해야 함
    EXPECT_EQ(started_count, 2);
    
    // 5. Registry 확인
    EXPECT_TRUE(registry->GetWorker("1") != nullptr);
    EXPECT_TRUE(registry->GetWorker("2") != nullptr);
    EXPECT_FALSE(registry->GetWorker("3") != nullptr); // Collector 1002 것
    EXPECT_FALSE(registry->GetWorker("4") != nullptr); // 미할당 것
    
    std::cout << "✅ Multi-Collector Isolation Test Passed: Started " << started_count << " workers (IDs: 1, 2)" << std::endl;
}

TEST_F(MultiCollectorIsolationTest, Test_CollectorIsolation_Empty) {
    // 1. 테스트 데이터: 다른 컬렉터 것만 존재
    AddTestDevice(5, 1, 1, 2000, "Device_Other_Collector");
    
    // 2. 현재 컬렉터 ID를 1001로 설정
    ConfigManager::getInstance().set("CSP_GATEWAY_BUILDING_ID", "1001");
    
    // 3. WorkerScheduler 실행
    auto registry = std::make_shared<WorkerRegistry>();
    auto redis_writer = std::make_shared<Storage::RedisDataWriter>();
    WorkerScheduler scheduler(registry, redis_writer);
    
    int started_count = scheduler.StartAllActiveWorkers();
    
    // 4. 검증: 시작된 것이 없어야 함
    EXPECT_EQ(started_count, 0);
    EXPECT_EQ(registry->GetWorkerCount(), 0u);
    
    std::cout << "✅ Empty Isolation Test Passed: No workers started for mismatched collector ID." << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
