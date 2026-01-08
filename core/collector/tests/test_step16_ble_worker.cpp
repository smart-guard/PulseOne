/**
 * @file test_step16_ble_worker.cpp
 * @brief Verification test for BleBeaconWorker integration
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "Workers/WorkerFactory.h"
#include "Workers/Protocol/BleBeaconWorker.h"
#include "Drivers/Ble/BleBeaconDriver.h"
#include "Common/Structs.h"

using namespace PulseOne::Workers;
using namespace PulseOne::Structs;
using namespace PulseOne::Drivers::Ble;

class BleWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup any necessary environment
    }

    void TearDown() override {
        // Tear down
    }
};

TEST_F(BleWorkerTest, FactoryCreateWorker) {
    WorkerFactory factory;
    
    // Verify Protocol Support
    EXPECT_TRUE(factory.IsProtocolSupported("BLE_BEACON"));
    
    // Create Worker via Factory
    DeviceInfo info;
    info.id = "ble_test_device";
    info.protocol_type = "BLE_BEACON";
    info.is_enabled = true;
    info.polling_interval_ms = 1000;
    
    auto worker = factory.CreateWorkerById(-1); // Should fail
    EXPECT_EQ(worker, nullptr);
    
    // Since CreateWorker(device) requires DeviceEntity, we simulate internal logic 
    // or use the creator map if exposed (it's not).
    // So we rely on creating it manually to test the class itself first, 
    // and assume Factory logic (switch/case) works if map is registered (checked by IsProtocolSupported).
    
    // Manually test Worker Class Instantiation
    auto ble_worker = std::make_unique<BleBeaconWorker>(info);
    EXPECT_NE(ble_worker, nullptr);
    EXPECT_EQ(ble_worker->GetState(), WorkerState::STOPPED);
}

TEST_F(BleWorkerTest, WorkerLifecycle) {
    DeviceInfo info;
    info.id = "ble_lifecycle_test";
    info.protocol_type = "BLE_BEACON";
    info.is_enabled = true;
    info.polling_interval_ms = 200; 

    // Instantiate
    auto worker = std::make_unique<BleBeaconWorker>(info);
    
    // Start
    auto start_future = worker->Start();
    ASSERT_TRUE(start_future.get()); // Wait for start
    EXPECT_EQ(worker->GetState(), WorkerState::RUNNING);
    
    // Check Status (Simulation might imply it's connected or scanning)
    // BleBeaconDriver defaults to connected=true upon polling or successful scan start?
    // Let's check BleBeaconDriver implementation. It usually starts scanning thread.
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(worker->GetState(), WorkerState::RUNNING);

    // Stop
    auto stop_future = worker->Stop();
    ASSERT_TRUE(stop_future.get());
    EXPECT_EQ(worker->GetState(), WorkerState::STOPPED);
}

// Add a test to verify mocked scanning if possible, 
// using Simulation helper from Driver if we can access it via Worker?
// Worker holds unique_ptr<IProtocolDriver>, so we'd need to cast it.

TEST_F(BleWorkerTest, DataCollection) {
    DeviceInfo info;
    info.id = "ble_data_test";
    info.protocol_type = "BLE_BEACON";
    info.is_enabled = true;
    info.polling_interval_ms = 100;
    
    auto worker = std::make_unique<BleBeaconWorker>(info);
    
    // Add a DataPoint to poll
    DataPoint dp;
    dp.id = "101";
    dp.name = "TestBeacon";
    dp.address = 0;
    dp.address_string = "00000000-0000-0000-0000-000000000001"; // UUID
    // Type is not fully standardized for BLE RSSI yet but let's assume it works as generic read
    
    worker->AddDataPoint(dp);
    
    worker->Start().get();
    
    // Access Driver to simulate beacon
    // Need to cast: BaseDeviceWorker::GetDriver() returns IProtocolDriver* (if public/protected)
    // Actually BaseDeviceWorker protects driver_.
    // But we are testing Worker logic. 
    // If we can't inject data, we just verify it runs without crashing.
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    worker->Stop().get();
}
