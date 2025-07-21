// collector/src/Core/LogLevelManager.cpp
// 실시간 로그 레벨 관리자
#include "Core/LogLevelManager.h"
#include <thread>
#include <chrono>

namespace PulseOne {
namespace Core {

LogLevelManager& LogLevelManager::getInstance() {
    static LogLevelManager instance;
    return instance;
}

LogLevelManager::LogLevelManager() 
    : current_level_(LogLevel::INFO)
    , running_(false) {
}

void LogLevelManager::Initialize(ConfigManager* config, DatabaseManager* db) {
    config_ = config;
    db_manager_ = db;
    
    // 초기 로그 레벨 설정 (우선순위)
    // 1. 데이터베이스에서 확인
    // 2. 설정 파일에서 확인  
    // 3. 기본값 (INFO)
    
    LogLevel level = LoadLogLevelFromDB();
    if (level == LogLevel::INFO) {  // DB에서 못 찾으면
        level = LoadLogLevelFromFile();
    }
    
    SetLogLevel(level);
    
    // 실시간 모니터링 스레드 시작
    StartMonitoring();
}

LogLevel LogLevelManager::LoadLogLevelFromDB() {
    if (!db_manager_) return LogLevel::INFO;
    
    try {
        // system_settings 테이블에서 log_level 조회
        std::string query = "SELECT setting_value FROM system_settings WHERE setting_key = 'log_level'";
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (!result.empty()) {
            std::string level_str = result[0]["setting_value"].as<std::string>();
            LogLevel level = fromString(level_str);
            
            last_db_check_ = std::chrono::steady_clock::now();
            PulseOne::LogManager::getInstance().Info("📊 Log level loaded from DB: " + level_str);
            return level;
        }
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Warn("⚠️ Failed to load log level from DB: " + std::string(e.what()));
    }
    
    return LogLevel::INFO;  // 기본값
}

LogLevel LogLevelManager::LoadLogLevelFromFile() {
    if (!config_) return LogLevel::INFO;
    
    std::string level_str = config_->get("LOG_LEVEL");
    if (!level_str.empty()) {
        LogLevel level = fromString(level_str);
        PulseOne::LogManager::getInstance().Info("📁 Log level loaded from file: " + level_str);
        return level;
    }
    
    return LogLevel::INFO;  // 기본값
}

void LogLevelManager::SetLogLevel(LogLevel level) {
    if (current_level_ != level) {
        LogLevel old_level = current_level_;
        current_level_ = level;
        
        // LogManager에 즉시 반영
        PulseOne::LogManager::getInstance().setLogLevel(level);
        
        PulseOne::LogManager::getInstance().Info(
            "🎯 Log level changed: " + toString(old_level) + " → " + toString(level));
    }
}

void LogLevelManager::StartMonitoring() {
    if (running_) return;
    
    running_ = true;
    monitor_thread_ = std::thread([this]() {
        while (running_) {
            try {
                // 30초마다 DB에서 로그 레벨 체크
                auto now = std::chrono::steady_clock::now();
                if (now - last_db_check_ > std::chrono::seconds(30)) {
                    LogLevel new_level = LoadLogLevelFromDB();
                    SetLogLevel(new_level);
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(10));
                
            } catch (const std::exception& e) {
                PulseOne::LogManager::getInstance().Error("LogLevelManager monitoring error: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        }
    });
}

void LogLevelManager::StopMonitoring() {
    running_ = false;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

// 웹에서 호출할 수 있는 API
bool LogLevelManager::UpdateLogLevelInDB(LogLevel level) {
    if (!db_manager_) return false;
    
    try {
        std::string level_str = toString(level);
        std::string query = 
            "INSERT INTO system_settings (setting_key, setting_value, updated_at) "
            "VALUES ('log_level', '" + level_str + "', NOW()) "
            "ON CONFLICT (setting_key) DO UPDATE SET "
            "setting_value = EXCLUDED.setting_value, updated_at = NOW()";
            
        bool success = db_manager_->executeNonQueryPostgres(query);
        
        if (success) {
            SetLogLevel(level);  // 즉시 반영
            PulseOne::LogManager::getInstance().Info("✅ Log level updated in DB: " + level_str);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Error("Failed to update log level in DB: " + std::string(e.what()));
        return false;
    }
}

} // namespace Core
} // namespace PulseOne
