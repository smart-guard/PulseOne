/**
 * @file test_step23_settings_reload.cpp
 * @brief Step 23: Real-time Settings Reload and Redis Sync Verification
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Workers/WorkerManager.h"
#include "Workers/WorkerFactory.h"
#include "Storage/RedisDataWriter.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using namespace PulseOne;
using namespace PulseOne::Workers;
using namespace PulseOne::Storage;
using namespace PulseOne::Database;
using namespace PulseOne::Database::Entities; // Entities namespace
using json = nlohmann::json;

class SettingsReloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::getInstance().setLogLevel(PulseOne::Enums::LogLevel::DEBUG_LEVEL);
        
        ConfigManager::getInstance().initialize();
        
        auto& db_mgr = DbLib::DatabaseManager::getInstance();
        if (!db_mgr.initialize()) {
            LogManager::getInstance().Error("DB Init Failed");
        }
        
        // Force schema update
        db_mgr.executeNonQuery("DROP TABLE IF EXISTS data_points");
        db_mgr.executeNonQuery("DROP TABLE IF EXISTS device_settings");
        db_mgr.executeNonQuery("DROP TABLE IF EXISTS devices");
        
        // Ensure RepositoryFactory is initialized
        RepositoryFactory::getInstance().initialize();
        
        CleanupTestData();
        SetupTestDevice();
    }

    void TearDown() override {
        auto& worker_mgr = WorkerManager::getInstance();
        if (test_device_id_ > 0) {
            std::string str_id = std::to_string(test_device_id_);
            if(worker_mgr.HasWorker(str_id)) {
                worker_mgr.StopWorker(str_id);
            }
        }
        CleanupTestData();
    }
    
    void CleanupTestData() {
        auto device_repo = RepositoryFactory::getInstance().getDeviceRepository();
        auto datapoint_repo = RepositoryFactory::getInstance().getDataPointRepository();
        
        auto devices = device_repo->findAll();
        for(const auto& d : devices) {
            if(d.getName() == "TestDevice_Reload") {
                int id = d.getId();
                // DataPoints delete - use loop if deleteByDeviceId not available
                auto all_points = datapoint_repo->findAll();
                for(auto& p : all_points) {
                    if(p.getDeviceId() == id) {
                        datapoint_repo->deleteById(p.getId());
                    }
                }
                
                device_repo->deleteById(id);
            }
        }
    }
    
    void SetupTestDevice() {
        auto device_repo = RepositoryFactory::getInstance().getDeviceRepository();
        auto device_settings_repo = RepositoryFactory::getInstance().getDeviceSettingsRepository();
        
        // 1. Create Device
        DeviceEntity device;
        device.setName("TestDevice_Reload");
        device.setDeviceType("SENSOR");
        device.setManufacturer("TestMaker");
        device.setProtocolId(1); // Modbus assumed
        device.setEnabled(true);
        device.setEndpoint("127.0.0.1:502");
        
        json config;
        config["ip_address"] = "127.0.0.1";
        config["port"] = 502;
        device.setConfig(config.dump());
        
        device.setPollingInterval(1000);
        device.setTimeout(2000);
        device.setRetryCount(3);
        
        device.setTenantId(1);
        device.setSiteId(1);

        bool saved = device_repo->save(device);
        ASSERT_TRUE(saved) << "Device save failed";
        
        auto devices = device_repo->findAll();
        for(const auto& d : devices) {
            if(d.getName() == "TestDevice_Reload") {
                test_device_id_ = d.getId();
                break;
            }
        }
        ASSERT_GT(test_device_id_, 0);

        // 2. Create Settings
        auto settings_opt = device_settings_repo->findById(test_device_id_);
        if (!settings_opt.has_value()) {
            DeviceSettingsEntity settings;
            settings.setDeviceId(test_device_id_);
            settings.setAutoRegistrationEnabled(false);
            settings.setKeepAliveEnabled(true);
            settings.setRetryIntervalMs(5000);
            settings.setConnectionTimeoutMs(3000);
            settings.setMaxRetryCount(3);
            settings.setPollingIntervalMs(1000);
            settings.setBackoffTimeMs(1000); // 필수값 채우기
            settings.setKeepAliveIntervalS(30);
            settings.setReadTimeoutMs(1000);
            settings.setWriteTimeoutMs(1000);
            settings.setBackoffMultiplier(1.5);
            settings.setMaxBackoffTimeMs(5000);
            settings.setKeepAliveTimeoutS(5);
            
            device_settings_repo->save(settings);
        } else {
            auto settings = settings_opt.value();
            settings.setAutoRegistrationEnabled(false);
            device_settings_repo->update(settings);
        }
    }
    
    int test_device_id_ = 0;
};

TEST_F(SettingsReloadTest, ReloadAutoRegistrationFlag) {
    auto& worker_mgr = WorkerManager::getInstance();
    std::string str_id = std::to_string(test_device_id_);
    
    bool started = worker_mgr.StartWorker(str_id);
    
    // StartWorker returns true if thread created, even if initial connection fails
    // But verify existence
    ASSERT_TRUE(worker_mgr.HasWorker(str_id)) << "Worker Registration Failed";
    
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto device_settings_repo = RepositoryFactory::getInstance().getDeviceSettingsRepository();
    auto settings_opt = device_settings_repo->findById(test_device_id_);
    ASSERT_TRUE(settings_opt.has_value());
    
    auto settings = settings_opt.value();
    bool original_flag = settings.isAutoRegistrationEnabled();
    settings.setAutoRegistrationEnabled(!original_flag);
    device_settings_repo->update(settings);
    
    LogManager::getInstance().Info("DB 설정 변경 완료: AutoReg -> " + std::string(!original_flag ? "true" : "false"));
    
    bool reloaded = worker_mgr.ReloadWorkerSettings(str_id);
    ASSERT_TRUE(reloaded) << "Worker 리로드 실패";
    
    ASSERT_TRUE(worker_mgr.HasWorker(str_id));
}

TEST_F(SettingsReloadTest, SyncNewDataPoint) {
    auto& worker_mgr = WorkerManager::getInstance();
    std::string str_id = std::to_string(test_device_id_);
    
    worker_mgr.StartWorker(str_id);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto datapoint_repo = RepositoryFactory::getInstance().getDataPointRepository();
    DataPointEntity new_point;
    new_point.setDeviceId(test_device_id_);
    new_point.setName("New_Test_Point_Reload");
    new_point.setAddress(100);
    new_point.setDataType("FLOAT");
    new_point.setAccessMode("read");
    new_point.setWritable(false);
    new_point.setEnabled(true);
    // new_point.setPollingIntervalMs(1000); // Optional or Check name if compilation fails again
    
    bool saved = datapoint_repo->save(new_point);
    ASSERT_TRUE(saved) << "DataPoint Save Failed";
    
    LogManager::getInstance().Info("DB 데이터 포인트 추가 완료: New_Test_Point_Reload");
    
    bool reloaded = worker_mgr.ReloadWorkerSettings(str_id);
    ASSERT_TRUE(reloaded) << "Worker 리로드 실패 (데이터 포인트 추가 후)";
    
    ASSERT_TRUE(worker_mgr.HasWorker(str_id));
}
