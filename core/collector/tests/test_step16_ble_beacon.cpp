#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include "Drivers/Ble/BleBeaconDriver.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"


using namespace PulseOne;
using namespace PulseOne::Drivers::Ble;

class Step16BleBeaconTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 1. Initialize DB
        auto& db = DbLib::DatabaseManager::getInstance();
        ASSERT_TRUE(db.initialize()); 
        
        // 2. Load Schema
        std::string schemaPath = "db/test_schema_complete.sql";
        std::ifstream schemaFile(schemaPath);
        ASSERT_TRUE(schemaFile.is_open()) << "Failed to open " << schemaPath;
        std::stringstream buffer;
        buffer << schemaFile.rdbuf();
        ASSERT_TRUE(db.executeNonQuery(buffer.str()));

        // 3. Load BLE Data
        std::string dataPath = "db/test_data_ble.sql";
        std::ifstream dataFile(dataPath);
        ASSERT_TRUE(dataFile.is_open()) << "Failed to open " << dataPath;
        std::stringstream dataBuffer;
        dataBuffer << dataFile.rdbuf();
        ASSERT_TRUE(db.executeNonQuery(dataBuffer.str()));
        
        // 4. Initialize Repository Factory
        PulseOne::Database::RepositoryFactory::getInstance().initialize();
    }

    void TearDown() override {
        DbLib::DatabaseManager::getInstance().disconnectAll();
    }
};

TEST_F(Step16BleBeaconTest, DriverInitializationAndScan) {
    // 1. Create Driver
    auto driver = std::make_unique<BleBeaconDriver>();
    
    // 2. Initialize
    PulseOne::Structs::DriverConfig driverConfig;
    driverConfig.properties["adapter"] = "default";
    driverConfig.properties["scan_window_ms"] = "100";
    ASSERT_TRUE(driver->Initialize(driverConfig));
    
    // 3. Connect (Start Scanning)
    ASSERT_TRUE(driver->Connect());
    ASSERT_TRUE(driver->IsConnected());
    
    // 4. Verify Scanning (Simulation)
    std::string beaconUuid = "74278BDA-B644-4520-8F0C-720EAF059935"; // From SQL
    std::string beaconId = "1"; // From ID in DB insert? Assuming 1 is the first point.
    // Wait, DB auto-increment ID. In test_data_ble.sql we insert protocols/devices.
    // We need to fetch the point ID? Or just mock it?
    // In ReadValues we need DataPoint structs.
    
    // Let's manually construct DataPoint for test
    PulseOne::Structs::DataPoint dp;
    dp.id = "1";
    dp.address_string = beaconUuid;
    dp.data_type = "INT16";
    
    // Allow some time for simulated scan
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 5. Read RSSI
    std::vector<PulseOne::Structs::DataPoint> points = {dp};
    std::vector<PulseOne::Structs::TimestampedValue> values;
    
    bool result = driver->ReadValues(points, values);
    
    ASSERT_TRUE(result) << "Failed to read beacon RSSI";
    ASSERT_EQ(values.size(), 1UL);
    ASSERT_EQ(values[0].quality, PulseOne::Enums::DataQuality::GOOD);
    ASSERT_TRUE(std::holds_alternative<int>(values[0].value));
    int rssi = std::get<int>(values[0].value);
    std::cout << "Read RSSI: " << rssi << std::endl;
    ASSERT_GE(rssi, -100);
    ASSERT_LE(rssi, -30);
    
    // 6. Test Manual Simulation
    std::string newUuid = "11111111-2222-3333-4444-555555555555";
    driver->SimulateBeaconDiscovery(newUuid, -55);
    
    PulseOne::Structs::DataPoint dp2;
    dp2.id = "2";
    dp2.address_string = newUuid;
    
    points = {dp2};
    values.clear();
    
    result = driver->ReadValues(points, values);
    ASSERT_TRUE(result);
    ASSERT_EQ(values.size(), 1UL);
    EXPECT_EQ(std::get<int>(values[0].value), -55);
    
    // 7. Test Disconnect
    driver->Disconnect();
    ASSERT_FALSE(driver->IsConnected());
}
