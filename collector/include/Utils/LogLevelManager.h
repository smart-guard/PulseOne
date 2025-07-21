// collector/include/Core/LogLevelManager.h
#pragma once

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h" 
#include "Database/DatabaseManager.h"
#include <thread>
#include <atomic>
#include <chrono>

namespace PulseOne {
namespace Core {

/**
 * @brief 실시간 로그 레벨 관리자
 * @details DB와 파일에서 로그 레벨을 실시간으로 모니터링하여 동적 변경
 */
class LogLevelManager {
private:
    LogLevel current_level_;
    ConfigManager* config_;
    DatabaseManager* db_manager_;
    
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    std::chrono::steady_clock::time_point last_db_check_;

public:
    static LogLevelManager& getInstance();
    
    void Initialize(ConfigManager* config, DatabaseManager* db = nullptr);
    void SetLogLevel(LogLevel level);
    LogLevel GetCurrentLevel() const { return current_level_; }
    
    // 웹 API에서 호출
    bool UpdateLogLevelInDB(LogLevel level);
    
    void StartMonitoring();
    void StopMonitoring();

private:
    LogLevelManager();
    ~LogLevelManager() { StopMonitoring(); }
    
    LogLevel LoadLogLevelFromDB();
    LogLevel LoadLogLevelFromFile();
};

} // namespace Core
} // namespace PulseOne
