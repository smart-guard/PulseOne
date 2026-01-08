/**
 * @file test_step17_opcua_e2e.cpp
 * @brief End-to-End Integration Test for OPC UA:
 *        DB -> OPCUAWorker -> External Server (Embedded) -> Pipeline -> Redis -> Alarm -> VirtualPoint
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
#include "Drivers/OPCUA/OPCUADriver.h"
#include "Workers/Protocol/OPCUAWorker.h"
#include "Workers/WorkerFactory.h"

// Engines
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmManager.h"

// Storage
#include "Client/RedisClientImpl.h"

// Embedded OPC UA Server (using Checkpoint 50+ approach)
#include "Drivers/OPCUA/open62541.h"

using namespace PulseOne;
using namespace PulseOne::Drivers;
using namespace PulseOne::Workers;
using namespace PulseOne::Pipeline;
using namespace PulseOne::Database;
using namespace PulseOne::Alarm;
using namespace PulseOne::VirtualPoint;

// Global control for embedded server
static std::atomic<bool> server_running(true);
static std::atomic<uint16_t> server_port(4850); // Use 4850+ to avoid conflict

// Embedded Server Thread
void RunEmbeddedServer() {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setMinimal(config, server_port.load(), NULL);
    
    // Add "the.answer" variable (ns=1, s=the.answer)
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Double myDouble = 0.0;
    UA_Variant_setScalar(&attr.value, &myDouble, &UA_TYPES[UA_TYPES_DOUBLE]);
    attr.description = UA_LOCALIZEDTEXT((char*)"en-US", (char*)"The Answer");
    attr.displayName = UA_LOCALIZEDTEXT((char*)"en-US", (char*)"the.answer");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_NodeId nodeId = UA_NODEID_STRING(1, (char*)"the.answer");
    UA_QualifiedName myName = UA_QUALIFIEDNAME(1, (char*)"the.answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    
    UA_Server_addVariableNode(server, nodeId, parentNodeId, parentReferenceNodeId, myName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

    UA_StatusCode retval = UA_Server_run_startup(server);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_Server_delete(server);
        return;
    }

    // Run loop
    while(server_running) {
        UA_Server_run_iterate(server, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    UA_Server_delete(server);
}

class OpcUaIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n============================================\n";
        std::cout << "🚀 OPC UA E2E Integration Test Setup\n";
        std::cout << "============================================\n";

        // 0. Start Embedded Server
        server_port++; // Increment port for safety
        server_running = true;
        std::cout << "[Test] Starting Embedded Server on port " << server_port.load() << std::endl;
        server_thread_ = std::thread(RunEmbeddedServer);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for server startup

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

        // 2. Insert Test Data (OPC UA Device & Point)
        std::cout << "[Test] Inserting Test Data..." << std::endl;
        auto db = &DbLib::DatabaseManagerDbLib::DatabaseManager::getInstance();
        
        // Device: OPC UA
        std::string endpoint = "opc.tcp://127.0.0.1:" + std::to_string(server_port.load());
        db->executeNonQuery(
            "INSERT INTO devices (id, name, type, protocol_id, connection_info, is_enabled, polling_interval, timeout) "
            "VALUES (1, 'OPCUA-Device', 'sensor', 5, '{\"endpoint\": \"" + endpoint + "\"}', 1, 500, 2000);"
        );

        // DataPoint: the.answer (ns=1;s=the.answer)
        db->executeNonQuery(
            "INSERT INTO data_points (id, device_id, name, address, data_type, is_enabled, polling_interval) "
            "VALUES (1, 1, 'TheAnswer', 'ns=1;s=the.answer', 'FLOAT64', 1, 500);"
        );

        // Virtual Point: TheAnswer * 2
        db->executeNonQuery(
            "INSERT INTO virtual_points (id, tenant_id, name, formula, dependencies, data_type, is_enabled, execution_type) "
            "VALUES (100, 1, 'VP_Answer_Double', 'TheAnswer * 2', '{\"inputs\": [{\"point_id\": 1, \"variable\": \"TheAnswer\"}]}', 'float', 1, 'javascript');"
        );

        // Alarm: If TheAnswer > 100
        db->executeNonQuery(
            "INSERT INTO alarm_rules (id, tenant_id, name, description, target_type, target_id, alarm_type, high_limit, severity, is_enabled) "
            "VALUES (10, 1, 'High Answer', 'Answer > 100', 'data_point', 1, 'analog', 100.0, 'high', 1);"
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
    }

    void TearDown() override {
        std::cout << "\n[Test] Teardown Start" << std::endl;
        if (opcua_worker_) {
            std::cout << "[Test] Stopping Worker..." << std::endl;
            opcua_worker_->Stop().get();
        }
        if (data_processing_service_) {
            std::cout << "[Test] Stopping TPS..." << std::endl;
            data_processing_service_->Stop();
        }
        
        PipelineManager::getInstance().Shutdown();
        VirtualPointEngine::getInstance().shutdown();
        AlarmManager::getInstance().shutdown();

        std::cout << "[Test] Stopping Server..." << std::endl;
        server_running = false;
        if (server_thread_.joinable()) server_thread_.join();
        std::cout << "[Test] Teardown Complete" << std::endl;
    }

    // Helper to update server value
    void SetServerValue(double value) {
         // Create a temporary client to write to the embedded server to simulate value change
         // Or, if we made the server class based, we could call a method.
         // Since RunEmbeddedServer is a simple function, we can use a client to write.
         UA_Client *client = UA_Client_new();
         UA_ClientConfig_setDefault(UA_Client_getConfig(client));
         std::string endpoint = "opc.tcp://127.0.0.1:" + std::to_string(server_port.load());
         
         if (UA_Client_connect(client, endpoint.c_str()) == UA_STATUSCODE_GOOD) {
             UA_Variant v;
             UA_Variant_init(&v);
             UA_Variant_setScalar(&v, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
             UA_Client_writeValueAttribute(client, UA_NODEID_STRING(1, (char*)"the.answer"), &v);
             UA_Client_disconnect(client);
         }
         UA_Client_delete(client);
    }

    std::thread server_thread_;
    std::unique_ptr<DataProcessingService> data_processing_service_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<OPCUAWorker> opcua_worker_;
};

TEST_F(OpcUaIntegrationTest, Full_Pipeline_Flow) {
    // 1. Manually create Worker (simulating WorkerManager)
    // We construct DeviceInfo from what we inserted into DB, but for test simplicity we build it directly
    // ensuring it matches DB IDs so Pipeline logic works.
    PulseOne::Structs::DeviceInfo dev_chk;
    dev_chk.id = "1"; // Must match DB device_id
    dev_chk.name = "OPCUA-Device";
    dev_chk.protocol_type = "OPC_UA";
    dev_chk.endpoint = "opc.tcp://127.0.0.1:" + std::to_string(server_port.load());
    dev_chk.polling_interval_ms = 500;
    dev_chk.tenant_id = 1;
    dev_chk.is_enabled = true;
    
    // Add Point to Worker (Pipeline needs to know point ID)
    PulseOne::Structs::DataPoint dp;
    dp.id = "1"; // Must match DB point_id
    dp.name = "TheAnswer";
    dp.address_string = "ns=1;s=the.answer"; // Matches server
    // dp.address is int, ignore or set dummy
    dp.address = 0;
    dp.data_type = "FLOAT64"; // For worker parsing (Internal type) - Driver usually returns Variant
    // The driver returns whatever the server sends, but Metadata uses this.
    
    opcua_worker_ = std::make_shared<OPCUAWorker>(dev_chk);
    opcua_worker_->AddDataPoint(dp);

    // 2. Start Worker
    std::cout << "[Test] Starting Worker..." << std::endl;
    ASSERT_TRUE(opcua_worker_->Start().get());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 3. Verify Initial Value (0.0)
    std::cout << "[Test] Waiting for initial poll..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::string redis_key = "point:1:latest";
    std::string val = redis_client_->get(redis_key);
    if(val.empty()) {
        std::cout << "[Test] ⚠️ Initial value not found in Redis, waiting more..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        val = redis_client_->get(redis_key);
    }
    
    EXPECT_FALSE(val.empty());
    if(!val.empty()) {
        std::cout << "[Test] Initial Redis Value: " << val << std::endl;
        auto j = nlohmann::json::parse(val);
        EXPECT_NEAR(std::stod(j.value("value", "0.0")), 0.0, 0.001);
    }

    // 4. Update Value to Trigger Alarm (150.0 > 100.0)
    std::cout << "[Test] Updating server value to 150.0..." << std::endl;
    SetServerValue(150.0);
    
    // 5. Wait for poll and processing
    std::cout << "[Test] Waiting for updated poll..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 6. Verify Redis Update
    val = redis_client_->get(redis_key);
    EXPECT_FALSE(val.empty());
    if(!val.empty()) {
        std::cout << "[Test] Updated Redis Value: " << val << std::endl;
        auto j = nlohmann::json::parse(val);
        EXPECT_NEAR(std::stod(j.value("value", "0.0")), 150.0, 0.001);
    }

    // 7. Verify Virtual Point (Should be 300.0)
    std::string vp_key = "point:100:latest";
    std::string vp_val = redis_client_->get(vp_key);
    if (vp_val.empty()) {
         std::cout << "[Test] ⚠️ VP value not found in Redis, waiting more..." << std::endl;
         std::this_thread::sleep_for(std::chrono::seconds(1));
         vp_val = redis_client_->get(vp_key);
    }

    if (!vp_val.empty()) {
        std::cout << "✅ Redis VP 100 Data: " << vp_val << std::endl;
        auto j = nlohmann::json::parse(vp_val);
        EXPECT_NEAR(std::stod(j.value("value", "0.0")), 300.0, 0.001); 
    } else {
        std::cout << "❌ Redis VP Key not found: " << vp_key << std::endl;
        EXPECT_FALSE(vp_val.empty());
    }

    // 8. Verify Alarm
    nlohmann::json stats = AlarmManager::getInstance().getStatistics();
    uint64_t raised = stats.value("alarms_raised", 0ULL);
    std::cout << "📊 Alarms raised: " << raised << std::endl;
    EXPECT_GT(static_cast<int64_t>(raised), 0);
}
