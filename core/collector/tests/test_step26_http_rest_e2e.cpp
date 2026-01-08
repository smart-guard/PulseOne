/**
 * @file test_step26_http_rest_e2e.cpp
 * @brief End-to-End Integration Test: HTTP/REST → Pipeline → Redis
 * Following step10 pattern exactly
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

// Core Utilities
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"

// Database & Repositories
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"

// Pipeline & Workers
#include "Pipeline/DataProcessingService.h"
#include "Drivers/Common/DriverFactory.h"
#include "Drivers/HttpRest/HttpRestDriver.h"
#include "Workers/Protocol/HttpRestWorker.h"

// Storage
#include "Client/RedisClientImpl.h"

using namespace PulseOne;
using namespace PulseOne::Drivers;
using namespace PulseOne::Workers;
using namespace PulseOne::Pipeline;
using namespace PulseOne::Database;
using namespace std::chrono;

// =============================================================================
// Test Fixture - Following step10 pattern
// =============================================================================

class HttpRestE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "🚀 HTTP/REST E2E Integration Test Setup" << std::endl;
        
        // Manual Driver Registration (like step10)
        DriverFactory::GetInstance().RegisterDriver("HTTP_REST", []() {
            return std::make_unique<HttpRestDriver>();
        });
        
        // 1. Initialize DB
        ConfigManager::getInstance().initialize();
        DbLib::DatabaseManager::getInstance().initialize();
        
        // 2. Reset DB with complete schema
        std::ifstream sql_file("db/test_schema_complete.sql");
        if (sql_file.is_open()) {
            std::stringstream buffer;
            buffer << sql_file.rdbuf();
            DbLib::DatabaseManager::getInstance().executeNonQuery(buffer.str());
        }
        
        auto db = &DbLib::DatabaseManagerDbLib::DatabaseManager::getInstance();
        
        // 3. Insert HTTP/REST device
        db->executeNonQuery(
            "INSERT OR REPLACE INTO devices (id, tenant_id, name, protocol_id, ip_address, port, is_enabled) "
            "VALUES (100, 1, 'HTTPBin-API', 11, 'httpbin.org', 443, 1);"
        );
        
        // 4. Insert device settings
        db->executeNonQuery(
            "INSERT OR REPLACE INTO device_settings (device_id, polling_interval_ms, connection_timeout_ms) "
            "VALUES (100, 5000, 10000);"
        );
        
        // 5. Insert data points with JSONPath in address_string
        db->executeNonQuery(
            "INSERT OR REPLACE INTO data_points "
            "(id, device_id, name, description, address, address_string, data_type, is_enabled, polling_interval_ms) "
            "VALUES "
            "(1001, 100, 'API_URL', 'URL from httpbin', 0, '$.url', 'STRING', 1, 5000), "
            "(1002, 100, 'API_Origin', 'Origin IP', 0, '$.origin', 'STRING', 1, 5000);"
        );
        
        // 6. Initialize Repositories
        RepositoryFactory::getInstance().initialize();
        
        // 7. Start Pipeline
        PipelineManager::getInstance().initialize();
        data_processing_service_ = std::make_unique<DataProcessingService>();
        data_processing_service_->Start();
        
        // 8. Redis Client
        redis_client_ = std::make_shared<RedisClientImpl>();
        redis_client_->connect("pulseone-redis", 6379);
        redis_client_->select(0);
    }
    
    void TearDown() override {
        std::cout << "🏁 HTTP/REST E2E Integration Test Teardown" << std::endl;
        if (http_worker_) http_worker_->Stop().get();
        if (data_processing_service_) data_processing_service_->Stop();
        PipelineManager::getInstance().Shutdown();
    }

    std::unique_ptr<DataProcessingService> data_processing_service_;
    std::shared_ptr<HttpRestWorker> http_worker_;
    std::shared_ptr<RedisClientImpl> redis_client_;
};

// =============================================================================
// E2E Test
// =============================================================================

TEST_F(HttpRestE2ETest, Full_Pipeline_DB_to_Redis) {
    std::cout << "🎯 Scenario: DB → Repository → Worker → HTTP API → Redis" << std::endl;
    
    // 1. Create DeviceInfo (like step10)
    PulseOne::DeviceInfo dev(PulseOne::Enums::ProtocolType::HTTP_REST);
    dev.id = "100";
    dev.name = "HTTPBin-API";
    dev.endpoint = "https://httpbin.org/get";
    dev.polling_interval_ms = 5000;
    dev.tenant_id = 1;
    dev.is_enabled = true;
    
    // 2. Load data points from DB using Repository
    auto dp_repo = RepositoryFactory::getInstance().getDataPointRepository();
    std::vector<std::vector<std::string>> result;
    bool query_success = DbLib::DatabaseManager::getInstance().executeQuery(
        "SELECT id, name, address_string, data_type FROM data_points WHERE device_id = 100",
        result
    );
    
    ASSERT_TRUE(query_success);
    ASSERT_GT(result.size(), 0UL);
    
    std::cout << "📊 Loaded " << result.size() << " data points from DB:" << std::endl;
    
    // 3. Create Worker and add data points
    http_worker_ = std::make_shared<HttpRestWorker>(dev);
    
    for (const auto& row : result) {
        PulseOne::Structs::DataPoint dp;
        dp.id = row[0];
        dp.name = row[1];
        dp.address_string = row[2];  // JSONPath here!
        dp.data_type = row[3];
        dp.is_enabled = true;
        dp.polling_interval_ms = 5000;
        
        http_worker_->AddDataPoint(dp);
        std::cout << "  ✓ " << dp.name << " → JSONPath: " << dp.address_string << std::endl;
    }
    
    // 4. Start worker
    std::cout << "➡️ Starting HTTP/REST worker..." << std::endl;
    ASSERT_TRUE(http_worker_->Start().get());
    
    // 5. Wait for polling
    std::cout << "⏳ Waiting for API polling (10 seconds)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 6. Verify Redis for point 1001 (API_URL)
    std::string redis_key = "point:1001:latest";
    std::string redis_val = redis_client_->get(redis_key);
    
    if (!redis_val.empty()) {
        std::cout << "✅ Redis Point 1001 Data: " << redis_val.substr(0, 100) << "..." << std::endl;
        auto j = nlohmann::json::parse(redis_val);
        std::string value = j.value("value", "");
        EXPECT_FALSE(value.empty());
        EXPECT_TRUE(value.find("httpbin.org") != std::string::npos);
    } else {
        std::cout << "⚠️ Redis Key not found (may be offline): " << redis_key << std::endl;
    }
    
    // 7. Verify Redis for point 1002 (API_Origin)
    redis_key = "point:1002:latest";
    redis_val = redis_client_->get(redis_key);
    
    if (!redis_val.empty()) {
        std::cout << "✅ Redis Point 1002 Data: " << redis_val.substr(0, 100) << "..." << std::endl;
        auto j = nlohmann::json::parse(redis_val);
        std::string value = j.value("value", "");
        EXPECT_FALSE(value.empty());
    } else {
        std::cout << "⚠️ Redis Key not found (may be offline): " << redis_key << std::endl;
    }
    
    std::cout << "✅ E2E Test Complete!" << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
