/**
 * @file test_step10_e2e_integration_recovery.cpp
 * @brief End-to-End Integration Test: Modbus -> Pipeline -> VP -> Alarm -> Recovery
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <arpa/inet.h>

// Core Utilities
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

// Database & Repositories
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"

// Pipeline & Workers
#include "Pipeline/PipelineManager.h"
#include "Pipeline/DataProcessingService.h"
#include "Workers/Protocol/ModbusWorker.h"

// Engines
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmManager.h"
#include "Alarm/AlarmEngine.h"
#include "Alarm/AlarmStartupRecovery.h"

// Storage
#include "Client/RedisClientImpl.h"
#include "Storage/RedisDataWriter.h"

// Virtual Servers
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

// Forward declaration of the plugin registration function from ModbusDriver.cpp
extern "C" void RegisterPlugin();

using namespace PulseOne;
using namespace PulseOne::Workers;
using namespace PulseOne::Pipeline;
using namespace PulseOne::VirtualPoint;
using namespace PulseOne::Alarm;
using namespace PulseOne::Database;
using namespace PulseOne::Storage;
using namespace std::chrono;

// =============================================================================
// 가상 Modbus TCP 서버 (Step 9 기반으로 복구)
// =============================================================================
class VirtualModbusTcpServer {
public:
    VirtualModbusTcpServer(int port = 1502) : port_(port), running_(false), server_fd_(-1) {}
    ~VirtualModbusTcpServer() { Stop(); }

    void Start() {
        if (running_) return;
        running_ = true;
        server_thread_ = std::thread(&VirtualModbusTcpServer::Run, this);
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
    }

    void Stop() {
        if (!running_) return;
        running_ = false;
        if (server_fd_ != -1) { shutdown(server_fd_, SHUT_RDWR); close(server_fd_); server_fd_ = -1; }
        if (server_thread_.joinable()) server_thread_.join();
        std::lock_guard<std::mutex> lock(client_threads_mutex_);
        for (auto& t : client_threads_) { if (t.joinable()) t.join(); }
        client_threads_.clear();
    }

    void SetRegister(uint16_t addr, uint16_t val) { std::lock_guard<std::mutex> lock(mutex_); regs_[addr] = val; }

private:
    void Run() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1; setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(port_);
        bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr));
        listen(server_fd_, 10);
        while (running_) {
            struct sockaddr_in c_addr; socklen_t len = sizeof(c_addr);
            int c_fd = accept(server_fd_, (struct sockaddr*)&c_addr, &len);
            if (c_fd < 0) continue;
            std::lock_guard<std::mutex> lock(client_threads_mutex_);
            client_threads_.push_back(std::thread(&VirtualModbusTcpServer::Handle, this, c_fd));
        }
    }
    void Handle(int fd) {
        uint8_t buf[1024];
        while (running_) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n < 12) break;
            uint8_t fc = buf[7]; uint16_t start = (buf[8] << 8) | buf[9]; uint16_t qty = (buf[10] << 8) | buf[11];
            if (fc == 0x03) {
                uint16_t d_len = 3 + (qty * 2); uint8_t resp[256]; memcpy(resp, buf, 4);
                resp[4] = (d_len >> 8) & 0xFF; resp[5] = d_len & 0xFF; resp[6] = buf[6]; resp[7] = 0x03; resp[8] = qty * 2;
                { std::lock_guard<std::mutex> lock(mutex_); for(int i=0; i<qty; i++) { uint16_t v = regs_[start+i]; resp[9+i*2]=(v>>8)&0xFF; resp[10+i*2]=v&0xFF; } }
                send(fd, resp, 6 + d_len, 0);
            }
        }
        close(fd);
    }
    int port_; std::atomic<bool> running_; int server_fd_; std::thread server_thread_;
    std::vector<std::thread> client_threads_; std::mutex client_threads_mutex_;
    std::map<uint16_t, uint16_t> regs_; std::mutex mutex_;
};

class E2EIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "🚀 End-to-End Integration Test Setup" << std::endl;
        
        // Register Modbus Driver (since we are statically linking but it uses plugin factory)
        RegisterPlugin();

        // 1. Initialize DB
        ConfigManager::getInstance().initialize();
        DatabaseManager::getInstance().initialize();
        
        // Reset DB with complete schema
        std::ifstream sql_file("db/test_schema_complete.sql");
        if (sql_file.is_open()) {
            std::stringstream buffer;
            buffer << sql_file.rdbuf();
            DatabaseManager::getInstance().executeNonQuery(buffer.str());
        }

        // 2. Clear and Setup specific data for this test
        auto db = &DatabaseManager::getInstance();
        db->executeNonQuery("DELETE FROM alarm_rules;");
        db->executeNonQuery("DELETE FROM virtual_points;");
        
        // Create virtual_points table if not exists
        db->executeNonQuery(
            "CREATE TABLE IF NOT EXISTS virtual_points ("
            "id INTEGER PRIMARY KEY, "
            "tenant_id INTEGER, "
            "name TEXT, "
            "description TEXT, "
            "formula TEXT, "
            "data_type TEXT, "
            "calculation_interval INTEGER DEFAULT 1000, "
            "calculation_trigger TEXT DEFAULT 'change', "
            "is_enabled INTEGER DEFAULT 1, "
            "execution_type TEXT DEFAULT 'javascript', "
            "dependencies TEXT DEFAULT '[]', "
            "error_handling TEXT DEFAULT 'return_null'"
            ");"
        );

        // Explicitly Insert Alarm Rule and Virtual Point for Tenant 1
        db->executeNonQuery(
            "INSERT OR REPLACE INTO alarm_rules (id, tenant_id, name, description, target_type, target_id, alarm_type, high_limit, severity, is_enabled, message_template) "
            "VALUES (10, 1, 'High Count', 'Count > 100', 'data_point', 1, 'analog', 100.0, 'high', 1, 'High count detected: {{value}}');"
        );
        
        db->executeNonQuery(
            "INSERT OR REPLACE INTO virtual_points (id, tenant_id, name, formula, dependencies, data_type, is_enabled, execution_type) "
            "VALUES (100, 1, 'VP_Double', 'Production_Count * 2', '{\"inputs\": [{\"point_id\": 1, \"variable\": \"Production_Count\"}]}', 'float', 1, 'javascript');"
        );

        // 3. Initialize Repositories
        RepositoryFactory::getInstance().initialize();
        
        // 4. Start Pipeline Components
        PipelineManager::GetInstance().Start();

        data_processing_service_ = std::make_unique<DataProcessingService>();
        data_processing_service_->Start();
        
        // 5. Start Virtual Modbus Server
        modbus_server_ = std::make_unique<VirtualModbusTcpServer>(1502);
        modbus_server_->Start();

        // 6. Redis Client
        redis_client_ = std::make_shared<RedisClientImpl>();
        redis_client_->connect("pulseone-redis", 6379);
        redis_client_->select(0);
    }
    
    void TearDown() override {
        std::cout << "🏁 End-to-End Integration Test Teardown" << std::endl;
        if (modbus_worker_) modbus_worker_->Stop().get();
        if (modbus_server_) modbus_server_->Stop();
        if (data_processing_service_) data_processing_service_->Stop();
        PipelineManager::GetInstance().Shutdown();
        VirtualPointEngine::getInstance().shutdown();
        AlarmManager::getInstance().shutdown();
    }

    std::unique_ptr<DataProcessingService> data_processing_service_;
    std::unique_ptr<VirtualModbusTcpServer> modbus_server_;
    std::shared_ptr<ModbusWorker> modbus_worker_;
    std::shared_ptr<RedisClientImpl> redis_client_;
};

TEST_F(E2EIntegrationTest, Full_Pipeline_Flow_Modbus_to_Alarm) {
    std::cout << "🎯 Scenario: Modbus -> Pipeline -> VP -> Alarm" << std::endl;

    // 1. Setup Data Point (In-memory for worker, matched with Production_Count in schema)
    PulseOne::Structs::DataPoint dp1;
    dp1.id = "1";
    dp1.name = "Production_Count";
    dp1.data_type = "UINT16";
    dp1.address = 1000; // Use raw wire address (1000) instead of 4XXXX notation (41001)
    dp1.scaling_factor = 1.0;
    dp1.scaling_offset = 0.0;
    dp1.is_enabled = true;
    dp1.is_writable = false;
    dp1.polling_interval_ms = 1000;
    dp1.protocol_params["slave_id"] = "1";
    dp1.protocol_params["function_code"] = "3";
    
    // 2. Set Modbus Server Value
    // Modbus protocol address is 0-indexed. Register 1000 in config -> address 1000.
    modbus_server_->SetRegister(1000, 150); // High value
    
    // 3. Start Engines & Load Rules
    auto& vp_engine = VirtualPointEngine::getInstance();
    vp_engine.initialize();
    bool vp_loaded = vp_engine.loadVirtualPoints(1);
    std::cout << "📈 Virtual Points Loaded: " << (vp_loaded ? "YES" : "NO") << std::endl;
    
    auto& am = AlarmManager::getInstance();
    bool am_loaded = am.loadAlarmRules(1);
    std::cout << "🔔 Alarm Rules Loaded: " << (am_loaded ? "YES" : "NO") << std::endl;
    
    // 4. Start Worker
    PulseOne::DeviceInfo dev(PulseOne::Enums::ProtocolType::MODBUS_TCP);
    dev.id = "1";
    dev.name = "PLC-001";
    dev.endpoint = "127.0.0.1:1502";
    dev.polling_interval_ms = 500;
    dev.tenant_id = 1;
    dev.is_enabled = true;
    
    modbus_worker_ = std::make_shared<ModbusWorker>(dev);
    modbus_worker_->AddDataPoint(dp1);
    
    std::cout << "➡️ Starting worker..." << std::endl;
    ASSERT_TRUE(modbus_worker_->Start().get());
    
    // 5. Wait for multiple polls
    std::cout << "⏳ Waiting for polling (5 seconds)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 6. Verify Redis for Raw Point
    std::string redis_key = "point:1:latest";
    std::string redis_val = redis_client_->get(redis_key);
    if (!redis_val.empty()) {
        std::cout << "✅ Redis Point 1 Data: " << redis_val << std::endl;
        auto j = nlohmann::json::parse(redis_val);
        std::string val_str = j.value("value", "0.0");
        EXPECT_NEAR(std::stod(val_str), 150.0, 0.001);
    } else {
        std::cout << "❌ Redis Key not found: " << redis_key << std::endl;
    }

    // 7. Verify Redis for Virtual Point (ID 100)
    std::string vp_key = "point:100:latest";
    std::string vp_val = redis_client_->get(vp_key);
    if (!vp_val.empty()) {
        std::cout << "✅ Redis VP 100 Data: " << vp_val << std::endl;
        auto j = nlohmann::json::parse(vp_val);
        std::string val_str = j.value("value", "0.0");
        EXPECT_NEAR(std::stod(val_str), 300.0, 0.001); // 150 * 2
    } else {
        std::cout << "❌ Redis VP Key not found: " << vp_key << std::endl;
    }

    // 8. Verify Alarms
    nlohmann::json stats = am.getStatistics();
    uint64_t evaluations = stats.value("total_evaluations", 0ULL);
    uint64_t raised = stats.value("alarms_raised", 0ULL);
    
    std::cout << "📊 Alarms evaluated: " << evaluations << std::endl;
    std::cout << "📊 Alarms raised: " << raised << std::endl;
    
    EXPECT_GT(static_cast<int64_t>(evaluations), 0);
    EXPECT_GT(static_cast<int64_t>(raised), 0);
}

TEST_F(E2EIntegrationTest, Alarm_Startup_Recovery_Flow) {
    std::cout << "🎯 Scenario: Alarm Startup Recovery" << std::endl;

    auto db = &DatabaseManager::getInstance();
    db->executeNonQuery("DELETE FROM alarm_occurrences;");
    db->executeNonQuery(
        "INSERT INTO alarm_occurrences (rule_id, tenant_id, occurrence_time, trigger_value, severity, state) "
        "VALUES (10, 1, datetime('now'), '150.5', 'HIGH', 'active');"
    );

    auto& recovery = AlarmStartupRecovery::getInstance();
    size_t recovered = recovery.RecoverActiveAlarms();
    
    std::cout << "📈 Recovered alarms: " << recovered << std::endl;
    EXPECT_GE(static_cast<int64_t>(recovered), 1);

    std::string redis_key = "alarm:active:10";
    std::string redis_data = redis_client_->get(redis_key);
    
    EXPECT_FALSE(redis_data.empty());
    if (!redis_data.empty()) {
        std::cout << "✅ Redis recovered alarm: " << redis_key << " -> " << redis_data.substr(0, 100) << "..." << std::endl;
    }
}
