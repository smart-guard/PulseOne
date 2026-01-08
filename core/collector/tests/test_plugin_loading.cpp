#include <gtest/gtest.h>
#include "Drivers/Common/PluginLoader.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"

using namespace PulseOne::Drivers;

class PluginLoadingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 경로 확인 및 설정
    }
};

TEST_F(PluginLoadingTest, LoadMqttPlugin) {
    auto& loader = PluginLoader::GetInstance();
    
    // 플랫폼별 확장자 결정
#if PULSEONE_WINDOWS
    std::string plugin_path = "./bin/plugins/MqttDriver.dll";
#else
    std::string plugin_path = "./bin/plugins/MqttDriver.so";
#endif
    
    // 플러그인 로드 시도
    bool success = loader.LoadPlugin(plugin_path);
    EXPECT_TRUE(success) << "Failed to load plugin: " << plugin_path;
    
    // DriverFactory에 등록되었는지 확인
    auto driver = DriverFactory::GetInstance().CreateDriver("MQTT");
    EXPECT_NE(driver, nullptr) << "Driver 'MQTT' not registered after plugin load";
    
    if (driver) {
        EXPECT_EQ(driver->GetProtocolType(), PulseOne::Enums::ProtocolType::MQTT);
        
        // 🚀 기능 검증: 초기화 및 상태 확인
        PulseOne::Structs::DriverConfig config;
        config.name = "TestMQTT";
        config.endpoint = "tcp://localhost:1883";
        // MQTT Driver requires client_id in many cases (though defaults might exist)
        config.properties["client_id"] = "test_client_id";
        
        EXPECT_TRUE(driver->Initialize(config)) << "MQTT Driver Initialize failed";
        // MqttDriver::Initialize sets status to INITIALIZED
        EXPECT_EQ(driver->GetStatus(), PulseOne::Enums::DriverStatus::INITIALIZED) << "Initial status should be INITIALIZED";
    }
}

TEST_F(PluginLoadingTest, LoadBacnetPlugin) {
    auto& loader = PluginLoader::GetInstance();
#if PULSEONE_WINDOWS
    std::string plugin_path = "./bin/plugins/BacnetDriver.dll";
#else
    std::string plugin_path = "./bin/plugins/BacnetDriver.so";
#endif
    
    bool success = loader.LoadPlugin(plugin_path);
    EXPECT_TRUE(success) << "Failed to load plugin: " << plugin_path;
    
    auto driver = DriverFactory::GetInstance().CreateDriver("BACNET_IP");
    EXPECT_NE(driver, nullptr);
    
    if (driver) {
        // 🚀 기능 검증
        PulseOne::Structs::DriverConfig config;
        config.name = "TestBacnet";
        config.endpoint = "127.0.0.1";
        
        // BACnet은 초기화 시 소켓을 열려 하므로 환경에 따라 다를 수 있음. 
        // 여기서는 크래시가 안 나는지, status 호출이 되는지를 중점 확인
        driver->Initialize(config); 
        EXPECT_NO_THROW(driver->GetStatus());
    }
}

TEST_F(PluginLoadingTest, LoadModbusPlugin) {
    auto& loader = PluginLoader::GetInstance();
#if PULSEONE_WINDOWS
    std::string plugin_path = "./bin/plugins/ModbusDriver.dll";
#else
    std::string plugin_path = "./bin/plugins/ModbusDriver.so";
#endif
    
    bool success = loader.LoadPlugin(plugin_path);
    EXPECT_TRUE(success) << "Failed to load plugin: " << plugin_path;
    
    // TCP 드라이버 확인
    auto driver_tcp = DriverFactory::GetInstance().CreateDriver("MODBUS_TCP");
    EXPECT_NE(driver_tcp, nullptr) << "Driver 'MODBUS_TCP' not registered";
    
    if (driver_tcp) {
        // 🚀 기능 검증 (TCP)
        PulseOne::Structs::DriverConfig config;
        config.name = "TestModbusTCP";
        config.endpoint = "127.0.0.1";
        // ModbusDriver는 properties에서 port를 읽거나 endpoint에서 파싱할 수 있음.
        // 드라이버 구현 확인 결과 properties["port"]를 우선하거나 endpoint 파싱함.
        // 안전하게 둘 다 설정.
        config.properties["port"] = "502"; 
        
        EXPECT_TRUE(driver_tcp->Initialize(config));
        // 초기화 직후에는 STOPPED 상태임 (Start를 안불렀으므로)
        EXPECT_EQ(driver_tcp->GetStatus(), PulseOne::Enums::DriverStatus::STOPPED);
    }
    
    // RTU 드라이버 확인
    auto driver_rtu = DriverFactory::GetInstance().CreateDriver("MODBUS_RTU");
    EXPECT_NE(driver_rtu, nullptr) << "Driver 'MODBUS_RTU' not registered";
    
    if (driver_rtu) {
        // 🚀 기능 검증 (RTU)
        PulseOne::Structs::DriverConfig config;
        config.name = "TestModbusRTU";
        config.endpoint = "/dev/ttyUSB0"; // Dummy path
        config.properties["baud_rate"] = "9600";
        config.properties["parity"] = "NONE";
        config.properties["data_bits"] = "8";
        config.properties["stop_bits"] = "1";
        
        EXPECT_TRUE(driver_rtu->Initialize(config));
        EXPECT_EQ(driver_rtu->GetStatus(), PulseOne::Enums::DriverStatus::STOPPED);
    }
}

TEST_F(PluginLoadingTest, LoadOpcuaPlugin) {
    auto& loader = PluginLoader::GetInstance();
#if PULSEONE_WINDOWS
    std::string plugin_path = "./bin/plugins/OpcuaDriver.dll";
#else
    std::string plugin_path = "./bin/plugins/OpcuaDriver.so";
#endif
    
    bool success = loader.LoadPlugin(plugin_path);
    EXPECT_TRUE(success) << "Failed to load plugin: " << plugin_path;
    
    // OPC-UA 드라이버 확인
    auto driver = DriverFactory::GetInstance().CreateDriver("OPC_UA");
    EXPECT_NE(driver, nullptr) << "Driver 'OPC_UA' not registered";

    if (driver) {
        EXPECT_EQ(driver->GetProtocolType(), PulseOne::Enums::ProtocolType::OPC_UA);
        
        // 🚀 기능 검증
        PulseOne::Structs::DriverConfig config;
        config.name = "TestOPCUA";
        config.endpoint = "opc.tcp://localhost:4840";
        
        EXPECT_TRUE(driver->Initialize(config));
        EXPECT_EQ(driver->GetStatus(), PulseOne::Enums::DriverStatus::STOPPED);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
