#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "Drivers/OPCUA/OPCUADriver.h"
#include "Drivers/OPCUA/open62541.h" // Amalgamation header
#include "Drivers/Common/DriverFactory.h"
#include "Common/Structs.h"
#include "Workers/WorkerFactory.h"

using namespace PulseOne::Drivers;
using namespace PulseOne::Structs;
using namespace PulseOne::Enums;

// Global flag to control server lifecycle
std::atomic<bool> server_running(true);
std::atomic<uint16_t> server_port(4840);

// Server Thread Function
void RunServer() {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    // Use minimal config with specific port
    UA_ServerConfig_setMinimal(config, server_port.load(), NULL);
    
    // Custom App URI (Optional but good for consistent ns=1)
    config->applicationDescription.applicationUri = UA_STRING_ALLOC("urn:open62541.server.application");

    // Add a Variable Node
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Double myDouble = 42.0;
    UA_Variant_setScalar(&attr.value, &myDouble, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.displayName = UA_LOCALIZEDTEXT((char*)"en-US", (char*)"the.answer");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_UInt16 nsIdx = 1;
    UA_NodeId myNodeId = UA_NODEID_STRING(nsIdx, (char*)"the.answer");
    UA_QualifiedName myName = UA_QUALIFIEDNAME(nsIdx, (char*)"the.answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    
    UA_Server_addVariableNode(server, myNodeId, parentNodeId,
                              parentReferenceNodeId, myName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);

    // Add Second Variable Node (for MultiPoint Test)
    UA_NodeId myNodeId2 = UA_NODEID_STRING(nsIdx, (char*)"the.question");
    UA_QualifiedName myName2 = UA_QUALIFIEDNAME(nsIdx, (char*)"the.question");
    
    UA_VariableAttributes attr2 = UA_VariableAttributes_default;
    UA_Int32 myInt = 999;
    UA_Variant_setScalar(&attr2.value, &myInt, &UA_TYPES[UA_TYPES_INT32]);
    attr2.displayName = UA_LOCALIZEDTEXT((char*)"en-US", (char*)"the.question");
    attr2.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_Server_addVariableNode(server, myNodeId2, parentNodeId,
                              parentReferenceNodeId, myName2,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr2, NULL, NULL);

    UA_StatusCode retval = UA_Server_run_startup(server);
    if(retval != UA_STATUSCODE_GOOD) {
        printf("Server Startup Failed: %s\n", UA_StatusCode_name(retval));
    } else {
        printf("Server Started Successfully\n");
    }

    while(server_running) {
        UA_Server_run_iterate(server, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    UA_Server_delete(server);
}

class OpcUaDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Increment Port for each test to avoid "Address in use"
        server_port++;
        
        // Start Server in thread
        server_running = true;
        server_thread_ = std::thread(RunServer);
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for server start
    }

    void TearDown() override {
        // Stop Server
        server_running = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    std::thread server_thread_;
};

TEST_F(OpcUaDriverTest, ConnectAndReadWrite) {
    OPCUADriver driver;
    DriverConfig config;
    config.endpoint = "opc.tcp://127.0.0.1:" + std::to_string(server_port.load());
    
    // Initialize
    ASSERT_TRUE(driver.Initialize(config));
    
    // Connect
    ASSERT_TRUE(driver.Connect());
    ASSERT_TRUE(driver.IsConnected());
    ASSERT_EQ(driver.GetStatus(), DriverStatus::RUNNING);

    // Write Value
    DataPoint point;
    point.id = "1";
    point.address_string = "ns=1;s=the.answer";
    point.name = "The Answer";
    
    DataValue val = 123.456;
    ASSERT_TRUE(driver.WriteValue(point, val));

    // Read Value
    std::vector<DataPoint> points = { point };
    std::vector<TimestampedValue> results;
    
    ASSERT_TRUE(driver.ReadValues(points, results));
    ASSERT_EQ(results.size(), 1UL);
    ASSERT_TRUE(std::holds_alternative<double>(results[0].value));
    EXPECT_DOUBLE_EQ(std::get<double>(results[0].value), 123.456);
    
    ASSERT_TRUE(driver.Disconnect());
    ASSERT_FALSE(driver.IsConnected());
}

TEST_F(OpcUaDriverTest, MultiPointAndReconnection) {
    OPCUADriver driver;
    DriverConfig config;
    config.endpoint = "opc.tcp://127.0.0.1:" + std::to_string(server_port.load());
    
    // 1. Initial Connection
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_TRUE(driver.Connect());

    // 2. Multi-Point Read
    DataPoint p1, p2;
    p1.id = "1"; p1.address_string = "ns=1;s=the.answer";
    p2.id = "2"; p2.address_string = "ns=1;s=the.question";
    
    std::vector<DataPoint> points = { p1, p2 };
    std::vector<TimestampedValue> results;
    
    ASSERT_TRUE(driver.ReadValues(points, results));
    ASSERT_EQ(results.size(), 2UL);
    
    // Check p1 (Double)
    ASSERT_TRUE(std::holds_alternative<double>(results[0].value));
    // Fresh server state value is 42.0
    EXPECT_DOUBLE_EQ(std::get<double>(results[0].value), 42.0);

    // Check p2 (Int32)
    ASSERT_TRUE(std::holds_alternative<double>(results[1].value)); // Driver converts all numerics to double in ReadValues currently?
    // Let's check OPCUADriver.cpp:138: tv.value = (double)*(UA_Int32*)val->data; Yes.
    EXPECT_DOUBLE_EQ(std::get<double>(results[1].value), 999.0);

    // 3. Reconnection Test
    // Disconnect
    ASSERT_TRUE(driver.Disconnect());
    ASSERT_FALSE(driver.IsConnected());
    
    // Attempt Read (Should fail)
    results.clear();
    ASSERT_FALSE(driver.ReadValues(points, results));
    
    // Reconnect
    ASSERT_TRUE(driver.Connect());
    ASSERT_TRUE(driver.IsConnected());
    
    // Read Again (Should succeed)
    results.clear();
    ASSERT_TRUE(driver.ReadValues(points, results));
    ASSERT_EQ(results.size(), 2UL);
    EXPECT_DOUBLE_EQ(std::get<double>(results[0].value), 42.0);
    
    driver.Disconnect();
}

TEST_F(OpcUaDriverTest, InvalidConnection) {
    OPCUADriver driver;
    DriverConfig config;
    config.endpoint = "opc.tcp://localhost:9999"; // Invalid Port
    
    ASSERT_TRUE(driver.Initialize(config));
    ASSERT_FALSE(driver.Connect());
    ASSERT_EQ(driver.GetStatus(), DriverStatus::DRIVER_ERROR);
}

TEST_F(OpcUaDriverTest, WorkerIntegration) {
    // 1. Verify Protocol Support
    PulseOne::Workers::WorkerFactory factory;
    EXPECT_TRUE(factory.IsProtocolSupported("OPC_UA"));
    
    // 2. Verify GetSupportedProtocols includes OPC_UA
    auto protocols = factory.GetSupportedProtocols();
    bool found = false;
    for (const auto& p : protocols) {
        if (p == "OPC_UA") found = true;
    }
    EXPECT_TRUE(found);

    // 3. (Optional) Try to create worker with Dummy DeviceInfo
    PulseOne::Structs::DeviceInfo info;
    info.id = "test_opcua";
    info.protocol_type = "OPC_UA";
    info.endpoint = "opc.tcp://127.0.0.1:4840";
    
    // Use factory internal map access or just rely on CreateWorker logic
    // We can't easily call CreateWorker without DeviceEntity, but we verified the registration map.
}
