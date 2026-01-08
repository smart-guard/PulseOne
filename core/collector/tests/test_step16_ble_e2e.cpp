/**
 * @file test_step16_ble_e2e.cpp
 * @brief End-to-End Integration Test for BLE Beacon:
 *        DB -> BleBeaconWorker (Simulated) -> Pipeline -> Redis -> Alarm -> VirtualPoint
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <future>
#include <iomanip>

// Core Utilities
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"

// Database & Repositories
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/AlarmRuleRepository.h"

// Pipeline & Workers
#include "Pipeline/DataProcessingService.h"
#include "Drivers/Common/DriverFactory.h"
#include "Workers/Protocol/BleBeaconWorker.h"
#include "Workers/WorkerFactory.h"

// Engines
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmManager.h"

// Storage
#include "Client/RedisClientImpl.h"

using namespace PulseOne;
using namespace PulseOne::Drivers;
using namespace PulseOne::Workers;
using namespace PulseOne::Pipeline;
using namespace PulseOne::Database;
using namespace PulseOne::Alarm;
using namespace PulseOne::VirtualPoint;

class BleIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n============================================\n";
        std::cout << "🚀 BLE E2E Integration Test Setup\n";
        std::cout << "============================================\n";

        // 1. Initialize DB and Config
        std::cout << "[Test] Initializing DB..." << std::endl;
        ConfigManager::getInstance().initialize();
        DbLib::DatabaseManager::getInstance().initialize();
        
        // Reset DB Schema
        std::ifstream sql_file("db/test_schema_complete.sql");
        if (sql_file.is_open()) {
            std::stringstream buffer;
            buffer << sql_file.rdbuf();
            DbLib::DatabaseManager::getInstance().executeNonQuery(buffer.str());
        } else {
             std::cerr << "[Test] Failed to open schema file!" << std::endl;
        }

        // 2. Insert Test Data (BLE Device & Beacon Point)
        std::cout << "[Test] Inserting Test Data..." << std::endl;
        auto db = &DbLib::DatabaseManagerDbLib::DatabaseManager::getInstance();
        
        // Device: BLE Gateway
        // Protocol ID 13 = BLE_BEACON (Ensure this matches test schema or enum)
        // We'll rely on text "BLE_BEACON" if we used ProtocolManager, but DB requires ID.
        // Assuming ID 13 based on Common/Enums.h or previous checks.
        // Insert Protocol if missing? pure schema might not have data.
        // test_schema_complete.sql usually has Protocols table?
        // Let's assume protocol_id 13 is valid or insert it.
        db->executeNonQuery("INSERT OR IGNORE INTO protocols (id, name, protocol_type) VALUES (13, 'BLE_BEACON', 'ble_beacon');");

        db->executeNonQuery(
            "INSERT INTO devices (id, name, type, protocol_id, connection_info, is_enabled, polling_interval, timeout) "
            "VALUES (1, 'BLE-Gateway', 'gateway', 13, '{\"adapter\": \"default\"}', 1, 200, 2000);"
        );

        // DataPoint: Beacon UUID that Simulation Driver generates
        // UUID: 74278BDA-B644-4520-8F0C-720EAF059935
        db->executeNonQuery(
            "INSERT INTO data_points (id, device_id, name, address, address_string, data_type, is_enabled, polling_interval) "
            "VALUES (1, 1, 'LobbyBeacon', '0', '74278BDA-B644-4520-8F0C-720EAF059935', 'INT16', 1, 200);"
        );

        // Virtual Point: RSSI_Adjusted (LobbyBeacon + 100)
        // RSSI is usually -90 to -40. +100 makes it 10 to 60.
        db->executeNonQuery(
            "INSERT INTO virtual_points (id, tenant_id, name, formula, dependencies, data_type, is_enabled, execution_type) "
            "VALUES (100, 1, 'VP_RSSI_Plus100', 'LobbyBeacon + 100', '{\"inputs\": [{\"point_id\": 1, \"variable\": \"LobbyBeacon\"}]}', 'float', 1, 'javascript');"
        );

        // Alarm: Signal Weak (LobbyBeacon < -80)
        // Simulation generates -95 to -40. So this alarm should trigger intermittently or persistently depending on logic.
        // Let's set it to < -50 to almost always trigger, or > -200 to always trigger for "Presence"?
        // Alarm Type: 'analog', High Limit? Low Limit?
        // Schema usually has high_limit / low_limit.
        // Let's use 'Low Signal': value < -80.
        // 'analog' usually checks limits.
        // Let's verify 'AlarmRuleRepository' columns. test_step17 uses 'high_limit'.
        // Let's use 'High Signal' > -100 (which is always true for -95 to -40).
        db->executeNonQuery(
            "INSERT INTO alarm_rules (id, tenant_id, name, description, target_type, target_id, alarm_type, high_limit, severity, is_enabled) "
            "VALUES (10, 1, 'Beacon Detected', 'RSSI > -100', 'data_point', 1, 'analog', -100.0, 'info', 1);"
        );

        RepositoryFactory::getInstance().initialize();

        // 3. Start Pipeline Components
        std::cout << "[Test] Starting Pipeline..." << std::endl;
        PipelineManager::getInstance().initialize();
        data_processing_service_ = std::make_unique<DataProcessingService>();
        data_processing_service_->Start();

        // 4. Start Virtual Point & Alarm Engines
        VirtualPointEngine::getInstance().initialize();
        VirtualPointEngine::getInstance().loadVirtualPoints(1);
        AlarmManager::getInstance().initialize();
        AlarmManager::getInstance().loadAlarmRules(1);

        // 5. Redis Connection
        std::cout << "[Test] Connecting to Redis..." << std::endl;
        redis_client_ = std::make_shared<RedisClientImpl>();
        redis_client_->connect("pulseone-redis", 6379);
        redis_client_->select(0);
        // Manual Flush
        auto keys = redis_client_->keys("*");
        for(const auto& key : keys) {
            redis_client_->del(key);
        }
    }

    void TearDown() override {
        std::cout << "\n[Test] Teardown Start" << std::endl;
        if (ble_worker_) {
            std::cout << "[Test] Stopping Worker..." << std::endl;
            ble_worker_->Stop().get();
        }
        if (data_processing_service_) {
            std::cout << "[Test] Stopping TPS..." << std::endl;
            data_processing_service_->Stop();
        }
        
        PipelineManager::getInstance().Shutdown();
        VirtualPointEngine::getInstance().shutdown();
        AlarmManager::getInstance().shutdown();

        std::cout << "[Test] Teardown Complete" << std::endl;
    }

    std::unique_ptr<DataProcessingService> data_processing_service_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<BleBeaconWorker> ble_worker_;
};

TEST_F(BleIntegrationTest, Full_Pipeline_Flow) {
    // 1. Manually create Worker
    PulseOne::Structs::DeviceInfo dev_chk;
    dev_chk.id = "1"; // match DB
    dev_chk.name = "BLE-Gateway";
    dev_chk.protocol_type = "BLE_BEACON"; 
    // endpoint irrelevant for sim, but good for completeness
    dev_chk.endpoint = "default";
    dev_chk.polling_interval_ms = 200;
    dev_chk.tenant_id = 1;
    dev_chk.is_enabled = true;
    
    // Add Point
    PulseOne::Structs::DataPoint dp;
    dp.id = "1"; // match DB
    dp.name = "LobbyBeacon";
    dp.address_string = "74278BDA-B644-4520-8F0C-720EAF059935";
    dp.address = 0;
    dp.data_type = "INT16"; 
    
    ble_worker_ = std::make_shared<BleBeaconWorker>(dev_chk);
    ble_worker_->AddDataPoint(dp);

    // 2. Start Worker
    std::cout << "[Test] Starting BLE Worker (Sim Mode)..." << std::endl;
    ASSERT_TRUE(ble_worker_->Start().get());
    
    // 3. Verify Data Flow
    std::cout << "[Test] Waiting for simulation data..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for scan window (default 1000ms+?)
    
    std::string redis_key = "point:1:latest";
    std::string val = redis_client_->get(redis_key);
    
    // Retry logic
    if(val.empty()) {
        std::cout << "[Test] ⚠️ Initial value not found, waiting more..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        val = redis_client_->get(redis_key);
    }
    
    EXPECT_FALSE(val.empty());
    if(!val.empty()) {
        std::cout << "[Test] Redis RSSI Value: " << val << std::endl;
        auto j = nlohmann::json::parse(val);
        // Simulation range: -95 to -40
        double rssi = std::stod(j.value("value", "0.0"));
        EXPECT_GE(rssi, -95.0);
        EXPECT_LE(rssi, -40.0);
    }

    // 4. Verify Virtual Point (RSSI + 100) -> 5 to 60
    std::string vp_key = "point:100:latest";
    std::string vp_val = redis_client_->get(vp_key);
    if (vp_val.empty()) {
         std::cout << "[Test] ⚠️ VP value not found, waiting more..." << std::endl;
         std::this_thread::sleep_for(std::chrono::seconds(1));
         vp_val = redis_client_->get(vp_key);
    }

    if (!vp_val.empty()) {
        std::cout << "✅ Redis VP 100 Data: " << vp_val << std::endl;
        auto j = nlohmann::json::parse(vp_val);
        double vp_res = std::stod(j.value("value", "0.0"));
        EXPECT_GE(vp_res, 5.0); // -95 + 100
        EXPECT_LE(vp_res, 60.0); // -40 + 100
    } else {
        std::cout << "❌ Redis VP Key not found: " << vp_key << std::endl;
        EXPECT_FALSE(vp_val.empty());
    }

    // 5. Verify Alarm (Detected > -100)
    // Should be raised instantly as any RSSI > -100
    nlohmann::json stats = AlarmManager::getInstance().getStatistics();
    uint64_t raised = stats.value("alarms_raised", 0ULL);
    std::cout << "📊 Alarms raised: " << raised << std::endl;
    EXPECT_GT(static_cast<int64_t>(raised), 0);
}
