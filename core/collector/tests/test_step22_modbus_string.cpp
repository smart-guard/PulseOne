#include <gtest/gtest.h>
#include "Workers/Protocol/ModbusWorker.h"
#include "Common/Structs.h"

using namespace PulseOne::Workers;
using namespace PulseOne::Structs;

class ModbusWorkerTest : public ::testing::Test {
protected:
    std::unique_ptr<ModbusWorker> worker;
    
    void SetUp() override {
        DeviceInfo info;
        info.id = "test_modbus";
        info.protocol_type = "MODBUS_TCP";
        worker = std::make_unique<ModbusWorker>(info);
    }
};

// Accessing private method for testing purpose via subclass or just testing public behavior
// Since ParseModbusAddress is private, I'll use a wrapper or just check it works via internal logic
// Actually, I'll make a Friend class for testing if needed, but ParseModbusAddress is likely internal.
// Let's see if I can use a subclass.

class TestModbusWorker : public ModbusWorker {
public:
    using ModbusWorker::ModbusWorker;
    using ModbusWorker::ParseModbusAddress;
};

TEST_F(ModbusWorkerTest, ParseStringAddresses) {
    DeviceInfo info;
    info.id = "test_modbus";
    info.protocol_type = "MODBUS_TCP";
    TestModbusWorker test_worker(info);
    
    uint8_t slave_id;
    ModbusRegisterType reg_type;
    uint16_t address;
    
    DataPoint dp;
    
    // Test HR
    dp.address_string = "HR:100";
    test_worker.ParseModbusAddress(dp, slave_id, reg_type, address);
    EXPECT_EQ(reg_type, ModbusRegisterType::HOLDING_REGISTER);
    EXPECT_EQ(address, 100);
    
    // Test IR
    dp.address_string = "IR:5";
    test_worker.ParseModbusAddress(dp, slave_id, reg_type, address);
    EXPECT_EQ(reg_type, ModbusRegisterType::INPUT_REGISTER);
    EXPECT_EQ(address, 5);
    
    // Test DI
    dp.address_string = "DI:200";
    test_worker.ParseModbusAddress(dp, slave_id, reg_type, address);
    EXPECT_EQ(reg_type, ModbusRegisterType::DISCRETE_INPUT);
    EXPECT_EQ(address, 200);
    
    // Test CO
    dp.address_string = "CO:1";
    test_worker.ParseModbusAddress(dp, slave_id, reg_type, address);
    EXPECT_EQ(reg_type, ModbusRegisterType::COIL);
    EXPECT_EQ(address, 1);
    
    // Test Legacy Fallback
    dp.address_string = "";
    dp.address = 40001;
    test_worker.ParseModbusAddress(dp, slave_id, reg_type, address);
    EXPECT_EQ(reg_type, ModbusRegisterType::HOLDING_REGISTER);
    EXPECT_EQ(address, 0);
}
