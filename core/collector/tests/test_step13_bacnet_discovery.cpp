#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Entities/DeviceEntity.h"
#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Workers/WorkerFactory.h"
#include "Logging/LogManager.h"
#include "Common/Structs.h"

// Mock Repository Factory or direct usage if possible
// We will use the actual repositories with an in-memory or test SQLite DB if setup allows.
// Looking at previous tests, they use RepositoryFactory or direct instantiation.

// Simple test framework
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "❌ FAILED: " << #condition << " at line " << __LINE__ << std::endl; \
        return 1; \
    }

#define ASSERT_EQUAL(expected, actual) \
    if ((expected) != (actual)) { \
        std::cerr << "❌ FAILED: " << #expected << " == " << #actual << " at line " << __LINE__ << std::endl; \
        std::cerr << "   Expected: " << (expected) << ", Actual: " << (actual) << std::endl; \
        return 1; \
    }

using namespace PulseOne;
using namespace PulseOne::Database;
using namespace PulseOne::Workers;

int Test_SaveDiscoveredDevice() {
    std::cout << "\n[Test] Save Discovered Device to Database" << std::endl;
    
    // 1. Dependencies
    auto device_repo = std::make_shared<Repositories::DeviceRepository>();
    auto datapoint_repo = std::make_shared<Repositories::DataPointRepository>();
    auto cv_repo = std::make_shared<Repositories::CurrentValueRepository>();
    auto settings_repo = std::make_shared<Repositories::DeviceSettingsRepository>();
    
    // We can pass nullptr for WorkerFactory if we don't start dynamic discovery in this specific test,
    // or we mock it. For persistence test, we just focus on DB.
    auto discovery_service = std::make_shared<BACnetDiscoveryService>(
        device_repo, datapoint_repo, cv_repo, settings_repo, nullptr
    );
    
    // 2. Prepare mock discovery data
    PulseOne::Structs::DeviceInfo discovered_device;
    discovered_device.id = "99001"; // Test ID
    discovered_device.name = "Test BACnet Device 99001";
    discovered_device.description = "Discovered via Unit Test";
    discovered_device.protocol_type = "BACNET_IP";
    discovered_device.endpoint = "192.168.1.100:47808";
    discovered_device.is_enabled = true;
    discovered_device.properties["vendor_id"] = "123";
    
    // 3. Trigger Discovery Callback
    std::cout << "Triggering OnDeviceDiscovered..." << std::endl;
    // Note: We need to make sure ResolveProtocolTypeToId works or handled.
    // In BACnetDiscoveryService implementation, it uses ProtocolRepository.
    // Depending on DB state, this might fail if protocol not found. 
    // Ideally we should insert "BACNET_IP" into protocol table first if not exists.
    
    // Let's assume the test env DB (test.db) is pre-populated or we can rely on error handling.
    // Actually, checking BACnetDiscoveryService.cpp ResolveProtocolTypeToId:
    // It calls RepositoryFactory::getInstance().getProtocolRepository().
    // We should initialize RepositoryFactory if needed.
    // Many tests do this via main or setup.
    
    try {
        discovery_service->OnDeviceDiscovered(discovered_device);
    } catch (const std::exception& e) {
        std::cerr << "Exception during OnDeviceDiscovered: " << e.what() << std::endl;
        // Proceed to check if it saved anyway or failed
    }
    
    // 4. Verification
    auto saved_device = device_repo->findById(99001);
    
    ASSERT_TRUE(saved_device.has_value());
    ASSERT_EQUAL(99001, saved_device->getId());
    ASSERT_EQUAL("Test BACnet Device 99001", saved_device->getName());
    
    std::cout << "✅ Device correctly saved to RDB." << std::endl;
    return 0;
}

// ... (Existing includes)
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/Entities/ProtocolEntity.h"

// ... (Existing definitions)

void SeedProtocolData() {
    auto protocol_repo = Database::RepositoryFactory::getInstance().getProtocolRepository();
    try {
        auto existing = protocol_repo->findByType("BACNET_IP");
        if (!existing) {
            PulseOne::Database::Entities::ProtocolEntity protocol;
            protocol.setProtocolType("BACNET_IP");
            protocol.setDisplayName("BACnet IP");
            protocol.setDescription("BACnet IP Protocol");
            protocol.setCategory("building_automation");
            protocol.setEnabled(true);
            if (protocol_repo->save(protocol)) {
                std::cout << "Seeded BACNET_IP protocol." << std::endl;
            } else {
                 std::cerr << "Failed to seed BACNET_IP protocol" << std::endl;
            }
        }
    } catch (...) {
        // Ignore if already exists or other error, strictly for test setup
         PulseOne::Database::Entities::ProtocolEntity protocol;
         protocol.setProtocolType("BACNET_IP");
         protocol.setDisplayName("BACnet IP");
         protocol.setCategory("building_automation");
         protocol.setEnabled(true);
         protocol_repo->save(protocol);
         std::cout << "Seeded BACNET_IP protocol (fallback)." << std::endl;
    }
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "🚀 Running Step 13: BACnet Persistence Test " << std::endl;
    std::cout << "============================================" << std::endl;

    // Initialize LogManager (Automatic via getInstance)
    LogManager::getInstance().Info("Initializing test...");
    
    // Initialize Database
    DbLib::DatabaseConfig db_config;
    db_config.type = "SQLITE";
    db_config.sqlite_path = "test.db";
    
    if (!DbLib::DatabaseManager::getInstance().initialize(db_config)) {
        std::cerr << "Failed to initialize database!" << std::endl;
        // Proceeding might be dangerous but let's see if sqlite works regardless
    }
    
    // Initialize RepositoryFactory
    if (!Database::RepositoryFactory::getInstance().initialize()) {
        std::cerr << "Failed to initialize RepositoryFactory!" << std::endl;
        return 1;
    }
    
    // Seed Data
    SeedProtocolData();

    // Run Tests
    if (Test_SaveDiscoveredDevice() != 0) return 1;

    std::cout << "\n✅ ALL TESTS PASSED" << std::endl;
    return 0;
}
