/**
 * @file test_step15_network_scan.cpp
 * @brief Test for Network Scan API and Discovery Logic
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <thread>
#include <chrono>

using namespace PulseOne;
using namespace PulseOne::Workers;

class NetworkScanTest : public ::testing::Test {
protected:
    std::shared_ptr<BACnetDiscoveryService> discovery_service;
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo;

    void SetUp() override {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        device_repo = repo_factory.getDeviceRepository();
        datapoint_repo = repo_factory.getDataPointRepository();
        
        // Use default factory (might be null in test env, but service handles it)
        std::shared_ptr<WorkerFactory> worker_factory = nullptr; 
        
        discovery_service = std::make_shared<BACnetDiscoveryService>(
            device_repo,
            datapoint_repo,
            nullptr, // CurrentValue (optional)
            nullptr, // DeviceSettings (optional)
            worker_factory
        );
    }

    void TearDown() override {
        // Cleanup
    }
};

#include "DatabaseManager.hpp"
#include "DatabaseAbstractionLayer.hpp"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/Entities/ProtocolEntity.h"

// Test 1: Verify StartNetworkScan initiates the scan state
TEST_F(NetworkScanTest, StartNetworkScan_SetsState) {
    bool result = discovery_service->StartNetworkScan("192.168.1.0/24");
    EXPECT_TRUE(result);
    EXPECT_TRUE(discovery_service->IsNetworkScanActive()); // You might need to add this getter if missing
    
    // Stop it
    discovery_service->StopNetworkScan();
    EXPECT_FALSE(discovery_service->IsNetworkScanActive());
}

// Test 2: Verify OnDeviceDiscovered saves to DB
TEST_F(NetworkScanTest, OnDeviceDiscovered_SavesToDB) {
    // 1. Create a dummy discovered device
    PulseOne::Structs::DeviceInfo device_info;
    device_info.id = "TEST_SCAN_DEV_001";
    device_info.name = "Test Scanned Device";
    device_info.protocol_type = "BACNET_IP";
    device_info.endpoint = "192.168.1.50:47808";
    device_info.is_enabled = true;
    device_info.properties["vendor_id"] = "123";

    // 2. Clear existing if any
    auto existing_device = device_repo->findById(12345); // Unlikely to exist but good practice
    
    // 3. Trigger callback manually (simulate discovery)
    discovery_service->OnDeviceDiscovered(device_info);
    
    // 4. Verify DB persistence
    // Because ID generation is hash-based or auto-inc, we search by Name or endpoint
    auto results = device_repo->findAll();
    bool found = false;
    for (const auto& dev : results) {
        if (dev.getName() == "Test Scanned Device") {
            found = true;
            EXPECT_EQ(dev.getEndpoint(), "192.168.1.50:47808");
            break;
        }
    }
    EXPECT_TRUE(found) << "Device should be saved to database after discovery";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize Database (SQLite for testing)
    DbLib::DatabaseConfig db_config;
    db_config.type = "SQLITE";
    db_config.sqlite_path = "test_step15.db"; // Use unique DB for this test
    
    if (!DbLib::DatabaseManager::getInstance().initialize(db_config)) {
        std::cerr << "Failed to initialize database!" << std::endl;
        return 1;
    }
    
    // Initialize RepositoryFactory
    if (!PulseOne::Database::RepositoryFactory::getInstance().initialize()) {
        std::cerr << "Failed to initialize RepositoryFactory!" << std::endl;
        return 1;
    }
    
    // Seed Protocol
    DbLib::DatabaseAbstractionLayer db_layer;
    db_layer.executeNonQuery("PRAGMA foreign_keys = OFF;");

    try {
        std::string check_sql = "SELECT COUNT(*) as count FROM protocols WHERE protocol_type = 'BACNET_IP'";
        auto results = db_layer.executeQuery(check_sql);
        int count = 0;
        if (!results.empty()) {
            count = std::stoi(results[0].at("count"));
        }
        
        if (count == 0) {
            std::string insert_sql = "INSERT INTO protocols (protocol_type, display_name, category, is_enabled) VALUES ('BACNET_IP', 'BACnet IP', 'building_automation', 1)";
            if (db_layer.executeNonQuery(insert_sql)) {
                 std::cout << "Seeded BACNET_IP protocol via raw SQL." << std::endl;
            } else {
                 std::cerr << "Failed to seed BACNET_IP protocol via raw SQL!" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception seeding protocol: " << e.what() << std::endl;
    }

    return RUN_ALL_TESTS();
}
