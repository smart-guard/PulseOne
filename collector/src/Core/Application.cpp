// collector/src/Core/CollectorApplication.cpp
#include "Core/Application.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : logger_(LogManager::getInstance())
    , start_time_(std::chrono::system_clock::now()) {
    
    logger_.Info("🚀 CollectorApplication v" + version_ + " created");
}

CollectorApplication::~CollectorApplication() {
    if (running_.load()) {
        Stop();
    }
    logger_.Info("💀 CollectorApplication destroyed");
}

void CollectorApplication::Run() {
    logger_.Info("🏁 Starting CollectorApplication...");
    
    try {
        if (!Initialize()) {
            logger_.Error("❌ Initialization failed");
            return;
        }
        
        logger_.Info("✅ Initialization successful");
        MainLoop();
        Cleanup();
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Fatal exception: " + std::string(e.what()));
    }
}

bool CollectorApplication::Initialize() {
    try {
        config_ = &ConfigManager::getInstance();
        config_->initialize();
        
        // 로드된 모든 설정 출력 (처음 10개만)
        auto all_config = config_->listAll();
        logger_.Info("📋 Loaded " + std::to_string(all_config.size()) + " configuration entries:");
        
        int count = 0;
        for (const auto& [key, value] : all_config) {
            if (count < 10) {  // 처음 10개만 출력
                logger_.Info("  " + key + " = " + value);
                count++;
            } else if (count == 10) {
                logger_.Info("  ... (총 " + std::to_string(all_config.size()) + "개 설정)");
                break;
            }
        }
        
        // 주요 설정 확인 (실제 존재하는 키들 확인)
        std::string db_type = config_->get("DB_TYPE");
        std::string node_env = config_->get("NODE_ENV");
        std::string postgres_host = config_->get("POSTGRES_MAIN_DB_HOST");
        
        if (!db_type.empty() || !node_env.empty() || !postgres_host.empty()) {
            logger_.Info("✅ Configuration successfully loaded!");
            logger_.Info("   DB_TYPE: " + (db_type.empty() ? "not set" : db_type));
            logger_.Info("   NODE_ENV: " + (node_env.empty() ? "not set" : node_env));
            logger_.Info("   POSTGRES_HOST: " + (postgres_host.empty() ? "not set" : postgres_host));
        } else {
            logger_.Warn("⚠️ Configuration file loaded but no expected keys found");
        }
        
        start_time_ = std::chrono::system_clock::now();
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Init exception: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    running_.store(true);
    int heartbeat_counter = 0;
    
    while (running_.load()) {
        try {
            heartbeat_counter++;
            
            if (heartbeat_counter % 12 == 0) {
                PrintStatus();
                heartbeat_counter = 0;
            } else {
                logger_.Debug("💗 Heartbeat #" + std::to_string(heartbeat_counter));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            logger_.Error("💥 MainLoop exception: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_.Info("🛑 Main loop stopped");
}

void CollectorApplication::Stop() {
    logger_.Info("🛑 Stop requested");
    running_.store(false);
}

void CollectorApplication::Cleanup() {
    logger_.Info("🧹 Cleanup completed");
}

void CollectorApplication::PrintStatus() {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
    auto seconds = uptime % std::chrono::minutes(1);
    
    std::stringstream status;
    status << "📊 STATUS: v" << version_ 
           << " | Uptime: " << hours.count() << "h " 
           << minutes.count() << "m " << seconds.count() << "s";
    
    logger_.Info(status.str());
    
    // 현재 설정 상태 출력
    if (config_) {
        std::string db_type = config_->getOrDefault("DB_TYPE", "none");
        std::string node_env = config_->getOrDefault("NODE_ENV", "none");
        logger_.Info("📋 Config: DB_TYPE=" + db_type + ", NODE_ENV=" + node_env);
    }
}

} // namespace Core
} // namespace PulseOne
