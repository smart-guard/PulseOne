/**
 * @file test_step15_schedule_sync.cpp
 * @brief Step 15: BACnet Schedule Persistence & Sync Verification
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Workers/Protocol/BACnetWorker.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include "Database/RepositoryFactory.h"
#include "Database/Entities/DeviceScheduleEntity.h"
#include "Database/Repositories/DeviceScheduleRepository.h"
#include "DatabaseManager.hpp"
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace PulseOne::Workers;
using namespace PulseOne::Drivers;
using namespace PulseOne::Structs;
using namespace PulseOne::Database;
using namespace testing;

// Mock Driver to capture WriteProperty calls
class MockBACnetDriver : public BACnetDriver {
    bool IsConnected() const override { return true; }
    bool Initialize(const DriverConfig& config) override { (void)config; return true; }
    bool Connect() override { return true; }
    PulseOne::Enums::ProtocolType GetProtocolType() const override { return PulseOne::Enums::ProtocolType::BACNET; }
};

class BACnetScheduleSyncTest : public Test {
protected:
    void SetUp() override {
        // 1. Initialize Config and Database
        ConfigManager::getInstance().initialize();
        DbLib::DatabaseManager::getInstance().initialize();
        
        // 2. Run Migrations (Step 15 schema)
        DbLib::DatabaseAbstractionLayer db;
        std::string sql_path = "/app/core/collector/tests/db/migration_step15_schedules.sql";
        std::ifstream sql_file(sql_path);
        
        if (!sql_file.is_open()) {
            std::cerr << "❌ Failed to open migration file: " << sql_path << std::endl;
        } else {
            std::cout << "✅ Opened migration file: " << sql_path << std::endl;
            std::string line;
            std::string sql;
            while (std::getline(sql_file, line)) {
                if (line.empty() || line.find("--") == 0) continue;
                sql += line;
                if (line.find(";") != std::string::npos) {
                    std::cout << "Executing SQL: " << sql << std::endl;
                    if (!db.executeNonQuery(sql)) {
                         std::cerr << "❌ SQL Execution Failed: " << sql << std::endl;
                    }
                    sql.clear();
                }
            }
        }
        
        // Verify table existence
        if (!db.doesTableExist("device_schedules")) {
             std::cerr << "❌ Table 'device_schedules' does not exist after migration!" << std::endl;
             // Attempt manual creation if migration failed
             std::string create_sql = "CREATE TABLE IF NOT EXISTS device_schedules ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                      "device_id INTEGER NOT NULL, "
                                      "point_id INTEGER, "
                                      "schedule_type TEXT NOT NULL, "
                                      "schedule_data TEXT, "
                                      "is_synced INTEGER DEFAULT 0, "
                                      "last_sync_time DATETIME, "
                                      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                                      "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
             db.executeCreateTable(create_sql);
        } else {
             // Clear existing data for clean test state
             db.executeNonQuery("DELETE FROM device_schedules");
             db.executeNonQuery("DELETE FROM sqlite_sequence WHERE name='device_schedules'");
        }
        
        // 3. Register Mock Driver Factory
        DriverFactory::GetInstance().RegisterDriver("BACNET", []() {
            return std::make_unique<MockBACnetDriver>();
        });
    }
    
    void TearDown() override {
        DbLib::DatabaseManager::getInstance().disconnectAll();
    }
};

TEST_F(BACnetScheduleSyncTest, SyncPendingSchedule) {
    // 1. Prepare DeviceInfo
    DeviceInfo dev_info;
    dev_info.id = "101";
    dev_info.name = "Test BACnet Device";
    dev_info.protocol_type = "BACNET";
    
    // 2. Insert Pending Schedule into RDB
    auto& factory = RepositoryFactory::getInstance();
    factory.initialize(); // Ensure factory is ready
    auto repo = factory.getDeviceScheduleRepository();
    
    Entities::DeviceScheduleEntity schedule;
    schedule.setDeviceId(101);
    // Use numeric type 17 for SCHEDULE (supported by ParseBACnetObjectId in fallback or numeric check)
    schedule.setScheduleType("BACNET_WEEKLY:101.17:1");
    
    nlohmann::json schedule_data = {
        {"day", "monday"},
        {"events", {{{"time", "08:00"}, {"value", 22.5}}, {{"time", "18:00"}, {"value", 18.0}}}}
    };
    schedule.setScheduleData(schedule_data.dump());
    schedule.setSynced(false);
    
    repo->save(schedule);
    
    // 3. Create Worker
    BACnetWorker worker(dev_info);
    
    // 4. Trigger Sync manually
    worker.SyncSchedulesWithDevices();
    
    // 5. Verify RDB update
    auto updated = repo->findByDeviceId(101);
    ASSERT_FALSE(updated.empty());
    EXPECT_TRUE(updated[0].isSynced());
    EXPECT_TRUE(updated[0].getLastSyncTime().has_value());
}
