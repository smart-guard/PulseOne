// collector/src/Core/CollectorApplication.cpp
#include "Core/Application.h"
#include "Utils/ConfigManager.h"
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
        logger_.Info("🔧 ConfigManager 초기화 시작...");
        
        config_ = &ConfigManager::getInstance();
        config_->initialize();
 
        
        auto all_config = config_->listAll();
        logger_.Info("📋 Loaded " + std::to_string(all_config.size()) + " configuration entries");
        
        // 🔧 개선: 설정 로딩 확인을 더 안전하게
        if (all_config.empty()) {
            logger_.Warn("⚠️ No configuration entries loaded - using defaults");
        } else {
            int count = 0;
            for (const auto& [key, value] : all_config) {
                if (count < 5) {  // 처음 5개만 출력 (로그 줄이기)
                    logger_.Info("  " + key + " = " + value);
                    count++;
                } else if (count == 5) {
                    logger_.Info("  ... (총 " + std::to_string(all_config.size()) + "개 설정)");
                    break;
                }
            }
        }
        
        // 🔧 개선: 주요 설정 확인 (더 안전한 방식)
        std::string db_type = config_->getOrDefault("DATABASE_TYPE", "SQLITE");  // 기본값 제공
        std::string node_env = config_->getOrDefault("NODE_ENV", "development");
        std::string log_level = config_->getOrDefault("LOG_LEVEL", "info");
        
        logger_.Info("✅ Configuration successfully loaded!");
        logger_.Info("   DATABASE_TYPE: " + db_type);
        logger_.Info("   NODE_ENV: " + node_env);
        logger_.Info("   LOG_LEVEL: " + log_level);
        
        // 🔧 추가: 설정 디렉토리 정보
        std::string config_dir = config_->getConfigDirectory();
        if (!config_dir.empty()) {
            logger_.Info("   CONFIG_DIR: " + config_dir);
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
    
    logger_.Info("🔄 메인 루프 시작 (5초 간격)");
    
    while (running_.load()) {
        try {
            heartbeat_counter++;
            
            // 🔧 개선: 상태 출력 주기를 더 합리적으로 (60초마다)
            if (heartbeat_counter % 12 == 0) {  // 12 * 5초 = 60초
                PrintStatus();
                heartbeat_counter = 0;
            } else {
                // 🔧 개선: Debug 레벨로 하트비트 출력 (로그 spam 방지)
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
    logger_.Info("🧹 Cleanup started");
    
    try {
        // 🔧 추가: 설정 시스템 정리
        if (config_) {
            logger_.Debug("🧹 ConfigManager cleanup");
            // ConfigManager는 싱글톤이므로 명시적 정리 불필요
        }
        
        // 🔧 추가: 로그 시스템 플러시
        logger_.Debug("🧹 LogManager flush");
        // LogManager 플러시는 소멸자에서 자동 처리
        
        logger_.Info("✅ Cleanup completed successfully");
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Cleanup exception: " + std::string(e.what()));
    }
}

void CollectorApplication::PrintStatus() {
    try {
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
            std::string db_type = config_->getOrDefault("DATABASE_TYPE", "unknown");
            std::string node_env = config_->getOrDefault("NODE_ENV", "unknown");
            std::string data_dir = config_->getOrDefault("DATA_DIR", "unknown");
            
            logger_.Info("📋 Config: DB=" + db_type + ", ENV=" + node_env + ", DATA=" + data_dir);
        } else {
            logger_.Warn("⚠️ ConfigManager not available");
        }
        
    } catch (const std::exception& e) {
        logger_.Error("💥 PrintStatus exception: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace PulseOne
