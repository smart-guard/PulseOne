#include "Utils/LogLevelManager.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Core {

// =============================================================================
// 생성자 및 싱글톤
// =============================================================================

LogLevelManager::LogLevelManager() 
    : current_level_(PulseOne::LogLevel::INFO)
    , maintenance_level_(PulseOne::LogLevel::TRACE)
    , config_(nullptr)
    , db_manager_(nullptr)
    , running_(false)
    , maintenance_mode_(false)
    , last_db_check_(std::chrono::steady_clock::now())
    , last_file_check_(std::chrono::steady_clock::now()) {
    
    // 카테고리별 기본 레벨 설정
    category_levels_[DriverLogCategory::GENERAL] = PulseOne::LogLevel::INFO;
    category_levels_[DriverLogCategory::CONNECTION] = PulseOne::LogLevel::INFO;
    category_levels_[DriverLogCategory::COMMUNICATION] = PulseOne::LogLevel::WARN;
    category_levels_[DriverLogCategory::DATA_PROCESSING] = PulseOne::LogLevel::INFO;
    category_levels_[DriverLogCategory::ERROR_HANDLING] = PulseOne::LogLevel::ERROR;
    category_levels_[DriverLogCategory::PERFORMANCE] = PulseOne::LogLevel::WARN;
    category_levels_[DriverLogCategory::SECURITY] = PulseOne::LogLevel::WARN;
    category_levels_[DriverLogCategory::PROTOCOL_SPECIFIC] = PulseOne::LogLevel::DEBUG_LEVEL;
    category_levels_[DriverLogCategory::DIAGNOSTICS] = PulseOne::LogLevel::DEBUG_LEVEL;
}

LogLevelManager& LogLevelManager::getInstance() {
    static LogLevelManager instance;
    return instance;
}

// =============================================================================
// 초기화 및 생명주기
// =============================================================================

void LogLevelManager::Initialize(ConfigManager* config, DatabaseManager* db) {
    config_ = config;
    db_manager_ = db;
    
    PulseOne::LogManager& logger = PulseOne::LogManager::getInstance();
    logger.Info("🔧 LogLevelManager initializing...");
    
    // 초기 로그 레벨 설정 (우선순위)
    // 1. 데이터베이스에서 확인
    // 2. 설정 파일에서 확인  
    // 3. 기본값 (INFO)
    
    LogLevel level = LoadLogLevelFromDB();
    if (level == PulseOne::LogLevel::INFO) {  // DB에서 못 찾으면 (기본값과 같으면)
        LogLevel file_level = LoadLogLevelFromFile();
        if (file_level != PulseOne::LogLevel::INFO) {  // 파일에서 다른 값을 찾았으면
            level = file_level;
        }
    }
    
    SetLogLevel(level, LogLevelSource::FILE_CONFIG, "SYSTEM", "Initial load");
    
    // 카테고리별 레벨도 DB에서 로드
    LoadCategoryLevelsFromDB();
    
    // 실시간 모니터링 스레드 시작
    StartMonitoring();
    
    logger.Info("✅ LogLevelManager initialized successfully");
}

void LogLevelManager::Shutdown() {
    if (running_) {
        PulseOne::LogManager::getInstance().Info("🔧 LogLevelManager shutting down...");
        StopMonitoring();
        
        // 콜백 정리
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            change_callbacks_.clear();
        }
        
        PulseOne::LogManager::getInstance().Info("✅ LogLevelManager shutdown completed");
    }
}

// =============================================================================
// 로그 레벨 관리 - 기본
// =============================================================================

void LogLevelManager::SetLogLevel(LogLevel level, LogLevelSource source,
                                 const EngineerID& changed_by, const std::string& reason) {
    LogLevel old_level = current_level_;
    
    if (old_level != level) {
        current_level_ = level;
        level_change_count_++;
        
        // LogManager에 즉시 반영
        PulseOne::LogManager::getInstance().setLogLevel(level);
        
        // 이벤트 생성 및 알림
        LogLevelChangeEvent event;
        event.old_level = old_level;
        event.new_level = level;
        event.source = source;
        event.changed_by = changed_by;
        event.reason = reason;
        event.is_maintenance_related = (source == LogLevelSource::MAINTENANCE_OVERRIDE);
        
        NotifyLevelChange(event);
        AddToHistory(event);
        
        // DB에 저장 (파일/명령줄 소스가 아닌 경우)
        if (source != LogLevelSource::FILE_CONFIG && source != LogLevelSource::COMMAND_LINE) {
            SaveLogLevelToDB(level, source, changed_by, reason);
        }
        
        PulseOne::LogManager::getInstance().Info(
            "🎯 Log level changed: {} → {} (Source: {}, By: {})", 
            LogLevelToString(old_level), LogLevelToString(level),
            LogLevelSourceToString(source), changed_by);
    }
}

// =============================================================================
// 카테고리별 로그 레벨 관리
// =============================================================================

void LogLevelManager::SetCategoryLogLevel(DriverLogCategory category, LogLevel level) {
    {
        std::lock_guard<std::mutex> lock(category_mutex_);
        category_levels_[category] = level;
    }
    
    // LogManager에도 반영
    PulseOne::LogManager::getInstance().setCategoryLogLevel(category, level);
    
    PulseOne::LogManager::getInstance().Info(
        "📊 Category log level set: {} = {}",
        DriverLogCategoryToString(category), LogLevelToString(level));
}

LogLevel LogLevelManager::GetCategoryLogLevel(DriverLogCategory category) const {
    std::lock_guard<std::mutex> lock(category_mutex_);
    auto it = category_levels_.find(category);
    return (it != category_levels_.end()) ? it->second : current_level_;
}

void LogLevelManager::ResetCategoryLogLevels() {
    std::lock_guard<std::mutex> lock(category_mutex_);
    category_levels_.clear();
    
    PulseOne::LogManager::getInstance().Info("🔄 All category log levels reset to default");
}

// =============================================================================
// 점검 모드 관리
// =============================================================================

void LogLevelManager::SetMaintenanceMode(bool enabled, LogLevel maintenance_level,
                                        const EngineerID& engineer_id) {
    bool was_enabled = maintenance_mode_.load();
    maintenance_mode_.store(enabled);
    
    if (enabled && !was_enabled) {
        // 점검 모드 시작
        maintenance_level_ = maintenance_level;
        SetLogLevel(maintenance_level, LogLevelSource::MAINTENANCE_OVERRIDE,
                   engineer_id, "Maintenance mode activated");
        
        PulseOne::LogManager::getInstance().Maintenance(
            "🔧 Maintenance mode STARTED by engineer: {}", engineer_id);
            
    } else if (!enabled && was_enabled) {
        // 점검 모드 종료 - 이전 레벨로 복원
        LogLevel restore_level = LoadLogLevelFromFile();  // 파일에서 복원
        if (restore_level == PulseOne::LogLevel::INFO) {
            restore_level = LoadLogLevelFromDB();  // DB에서 복원
        }
        
        SetLogLevel(restore_level, LogLevelSource::MAINTENANCE_OVERRIDE,
                   engineer_id, "Maintenance mode deactivated");
        
        PulseOne::LogManager::getInstance().Maintenance(
            "✅ Maintenance mode ENDED by engineer: {}", engineer_id);
    }
}

// =============================================================================
// 웹 API 지원
// =============================================================================

bool LogLevelManager::UpdateLogLevelInDB(LogLevel level, const EngineerID& changed_by,
                                        const std::string& reason) {
    if (!db_manager_) {
        PulseOne::LogManager::getInstance().Warn("⚠️ Cannot update log level: DB not available");
        return false;
    }
    
    try {
        bool success = SaveLogLevelToDB(level, LogLevelSource::WEB_API, changed_by, reason);
        
        if (success) {
            SetLogLevel(level, LogLevelSource::WEB_API, changed_by, reason);
            PulseOne::LogManager::getInstance().Info(
                "✅ Log level updated via Web API: {} by {}", 
                LogLevelToString(level), changed_by);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Error(
            "❌ Failed to update log level in DB: {}", e.what());
        return false;
    }
}

bool LogLevelManager::UpdateCategoryLogLevelInDB(DriverLogCategory category, LogLevel level,
                                                const EngineerID& changed_by) {
    if (!db_manager_) return false;
    
    try {
        bool success = SaveCategoryLevelToDB(category, level, changed_by);
        
        if (success) {
            SetCategoryLogLevel(category, level);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Error(
            "❌ Failed to update category log level in DB: {}", e.what());
        return false;
    }
}

bool LogLevelManager::StartMaintenanceModeFromWeb(const EngineerID& engineer_id,
                                                 LogLevel maintenance_level) {
    if (engineer_id.empty() || !IsValidEngineerID(engineer_id)) {
        PulseOne::LogManager::getInstance().Error(
            "❌ Invalid engineer ID for maintenance mode: {}", engineer_id);
        return false;
    }
    
    SetMaintenanceMode(true, maintenance_level, engineer_id);
    
    // DB에 점검 시작 기록
    if (db_manager_) {
        try {
            std::string query = 
                "INSERT INTO maintenance_log (engineer_id, action, log_level, timestamp, source) "
                "VALUES ('" + engineer_id + "', 'START', '" + LogLevelToString(maintenance_level) + 
                "', NOW(), 'WEB_API')";
            db_manager_->executeNonQueryPostgres(query);
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().Warn(
                "⚠️ Failed to log maintenance start to DB: {}", e.what());
        }
    }
    
    return true;
}

bool LogLevelManager::EndMaintenanceModeFromWeb(const EngineerID& engineer_id) {
    if (!maintenance_mode_.load()) {
        PulseOne::LogManager::getInstance().Warn(
            "⚠️ Cannot end maintenance mode: not currently in maintenance mode");
        return false;
    }
    
    SetMaintenanceMode(false, PulseOne::LogLevel::INFO, engineer_id);
    
    // DB에 점검 종료 기록
    if (db_manager_) {
        try {
            std::string query = 
                "INSERT INTO maintenance_log (engineer_id, action, timestamp, source) "
                "VALUES ('" + engineer_id + "', 'END', NOW(), 'WEB_API')";
            db_manager_->executeNonQueryPostgres(query);
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().Warn(
                "⚠️ Failed to log maintenance end to DB: {}", e.what());
        }
    }
    
    return true;
}

// =============================================================================
// 모니터링
// =============================================================================

void LogLevelManager::StartMonitoring() {
    if (running_.load()) return;
    
    running_.store(true);
    monitor_thread_ = std::thread([this]() {
        PulseOne::LogManager::getInstance().Debug("🔍 LogLevelManager monitoring started");
        MonitoringLoop();
    });
}

void LogLevelManager::StopMonitoring() {
    if (!running_.load()) return;
    
    running_.store(false);
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    PulseOne::LogManager::getInstance().Debug("🔍 LogLevelManager monitoring stopped");
}

void LogLevelManager::MonitoringLoop() {
    while (running_.load()) {
        try {
            CheckDatabaseChanges();
            CheckFileChanges();
            
            // 10초 간격으로 체크
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().Error(
                "❌ LogLevelManager monitoring error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
}

void LogLevelManager::CheckDatabaseChanges() {
    auto now = std::chrono::steady_clock::now();
    
    // 30초마다 DB 체크
    if (now - last_db_check_ > std::chrono::seconds(30)) {
        db_check_count_++;
        
        LogLevel new_level = LoadLogLevelFromDB();
        if (new_level != current_level_ && !maintenance_mode_.load()) {
            SetLogLevel(new_level, LogLevelSource::DATABASE, "SYSTEM", "Database change detected");
        }
        
        last_db_check_ = now;
    }
}

void LogLevelManager::CheckFileChanges() {
    auto now = std::chrono::steady_clock::now();
    
    // 60초마다 파일 체크
    if (now - last_file_check_ > std::chrono::seconds(60)) {
        file_check_count_++;
        
        LogLevel new_level = LoadLogLevelFromFile();
        if (new_level != current_level_ && !maintenance_mode_.load()) {
            SetLogLevel(new_level, LogLevelSource::FILE_CONFIG, "SYSTEM", "Config file change detected");
        }
        
        last_file_check_ = now;
    }
}

// =============================================================================
// 콜백 및 이벤트 관리
// =============================================================================

void LogLevelManager::RegisterChangeCallback(const LogLevelChangeCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    change_callbacks_.push_back(callback);
}

void LogLevelManager::UnregisterAllCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    change_callbacks_.clear();
}

void LogLevelManager::NotifyLevelChange(const LogLevelChangeEvent& event) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    for (const auto& callback : change_callbacks_) {
        try {
            callback(event.old_level, event.new_level);
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().Error(
                "❌ Error in log level change callback: {}", e.what());
        }
    }
}

void LogLevelManager::AddToHistory(const LogLevelChangeEvent& event) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    change_history_.push_back(event);
    
    // 최대 1000개 이벤트만 유지
    if (change_history_.size() > 1000) {
        change_history_.erase(change_history_.begin(), change_history_.begin() + 100);
    }
}

std::vector<LogLevelChangeEvent> LogLevelManager::GetChangeHistory(size_t max_count) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (change_history_.size() <= max_count) {
        return change_history_;
    }
    
    return std::vector<LogLevelChangeEvent>(
        change_history_.end() - max_count, change_history_.end());
}

void LogLevelManager::ClearChangeHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    change_history_.clear();
}

// =============================================================================
// 상태 조회 및 진단
// =============================================================================

LogLevelManager::ManagerStatus LogLevelManager::GetStatus() const {
    ManagerStatus status;
    
    status.current_level = current_level_;
    status.maintenance_level = maintenance_level_;
    status.maintenance_mode = maintenance_mode_.load();
    status.monitoring_active = running_.load();
    status.db_check_count = db_check_count_.load();
    status.file_check_count = file_check_count_.load();
    status.level_change_count = level_change_count_.load();
    status.last_db_check = std::chrono::system_clock::now() - 
                          std::chrono::duration_cast<std::chrono::system_clock::duration>(
                              std::chrono::steady_clock::now() - last_db_check_);
    status.last_file_check = std::chrono::system_clock::now() - 
                            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                std::chrono::steady_clock::now() - last_file_check_);
    
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        status.active_callbacks = change_callbacks_.size();
    }
    
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        status.history_entries = change_history_.size();
    }
    
    return status;
}

void LogLevelManager::RunDiagnostics() {
    PulseOne::LogManager& logger = PulseOne::LogManager::getInstance();
    
    logger.Info("🔧 Running LogLevelManager diagnostics...");
    
    // 설정 검증
    bool config_valid = ValidateConfiguration();
    logger.Info("📋 Configuration valid: {}", config_valid ? "YES" : "NO");
    
    // 상태 정보
    ManagerStatus status = GetStatus();
    logger.Info("📊 Status: {}", status.ToJson());
    
    // DB 연결 테스트
    if (db_manager_) {
        try {
            LogLevel test_level = LoadLogLevelFromDB();
            logger.Info("🗄️ Database connection: OK (current level: {})", 
                       LogLevelToString(test_level));
        } catch (const std::exception& e) {
            logger.Error("🗄️ Database connection: FAILED ({})", e.what());
        }
    } else {
        logger.Warn("🗄️ Database: NOT CONFIGURED");
    }
    
    // 파일 설정 테스트
    if (config_) {
        try {
            LogLevel test_level = LoadLogLevelFromFile();
            logger.Info("📁 Config file: OK (current level: {})", 
                       LogLevelToString(test_level));
        } catch (const std::exception& e) {
            logger.Error("📁 Config file: FAILED ({})", e.what());
        }
    } else {
        logger.Warn("📁 Config: NOT CONFIGURED");
    }
    
    logger.Info("✅ LogLevelManager diagnostics completed");
}

bool LogLevelManager::ValidateConfiguration() const {
    // 기본 검증 사항들
    if (current_level_ < PulseOne::LogLevel::TRACE || current_level_ > PulseOne::LogLevel::MAINTENANCE) {
        return false;
    }
    
    if (maintenance_level_ < PulseOne::LogLevel::TRACE || maintenance_level_ > PulseOne::LogLevel::MAINTENANCE) {
        return false;
    }
    
    // 카테고리 레벨 검증
    std::lock_guard<std::mutex> lock(category_mutex_);
    for (const auto& pair : category_levels_) {
        if (pair.second < PulseOne::LogLevel::TRACE || pair.second > PulseOne::LogLevel::MAINTENANCE) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// 내부 DB 및 파일 처리
// =============================================================================

LogLevel LogLevelManager::LoadLogLevelFromDB() {
    if (!db_manager_) return PulseOne::LogLevel::INFO;
    
    try {
        std::string query = "SELECT setting_value FROM system_settings WHERE setting_key = 'log_level'";
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (!result.empty()) {
            std::string level_str = result[0]["setting_value"].as<std::string>();
            LogLevel level = StringToLogLevel(level_str);
            
            last_db_check_ = std::chrono::steady_clock::now();
            return level;
        }
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Debug(
            "🔍 Failed to load log level from DB: {}", e.what());
    }
    
    return PulseOne::LogLevel::INFO;  // 기본값
}

LogLevel LogLevelManager::LoadLogLevelFromFile() {
    if (!config_) return PulseOne::LogLevel::INFO;
    
    std::string level_str = config_->get("LOG_LEVEL");
    if (!level_str.empty()) {
        LogLevel level = StringToLogLevel(level_str);
        return level;
    }
    
    return PulseOne::LogLevel::INFO;  // 기본값
}

bool LogLevelManager::SaveLogLevelToDB(LogLevel level, LogLevelSource source,
                                      const EngineerID& changed_by, const std::string& reason) {
    if (!db_manager_) return false;
    
    try {
        std::string level_str = LogLevelToString(level);
        std::string source_str = LogLevelSourceToString(source);
        
        // system_settings 테이블 업데이트
        std::string query = 
            "INSERT INTO system_settings (setting_key, setting_value, updated_at, updated_by) "
            "VALUES ('log_level', '" + level_str + "', NOW(), '" + changed_by + "') "
            "ON CONFLICT (setting_key) DO UPDATE SET "
            "setting_value = EXCLUDED.setting_value, "
            "updated_at = EXCLUDED.updated_at, "
            "updated_by = EXCLUDED.updated_by";
            
        bool success = db_manager_->executeNonQueryPostgres(query);
        
        // 변경 이력도 기록
        if (success) {
            std::string history_query = 
                "INSERT INTO log_level_history (old_level, new_level, source, changed_by, reason, change_time) "
                "VALUES ('" + LogLevelToString(current_level_) + "', '" + level_str + "', '" + 
                source_str + "', '" + changed_by + "', '" + reason + "', NOW())";
            db_manager_->executeNonQueryPostgres(history_query);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Error(
            "❌ Failed to save log level to DB: {}", e.what());
        return false;
    }
}

void LogLevelManager::LoadCategoryLevelsFromDB() {
    if (!db_manager_) return;
    
    try {
        std::string query = "SELECT category, log_level FROM category_log_levels";
        auto result = db_manager_->executeQueryPostgres(query);
        
        std::lock_guard<std::mutex> lock(category_mutex_);
        
        for (const auto& row : result) {
            std::string category_str = row["category"].as<std::string>();
            std::string level_str = row["log_level"].as<std::string>();
            
            // 문자열을 enum으로 변환 (간단한 매핑)
            DriverLogCategory category = DriverLogCategory::GENERAL;  // 기본값
            if (category_str == "CONNECTION") category = DriverLogCategory::CONNECTION;
            else if (category_str == "COMMUNICATION") category = DriverLogCategory::COMMUNICATION;
            else if (category_str == "DATA_PROCESSING") category = DriverLogCategory::DATA_PROCESSING;
            else if (category_str == "ERROR_HANDLING") category = DriverLogCategory::ERROR_HANDLING;
            else if (category_str == "PERFORMANCE") category = DriverLogCategory::PERFORMANCE;
            else if (category_str == "SECURITY") category = DriverLogCategory::SECURITY;
            else if (category_str == "PROTOCOL_SPECIFIC") category = DriverLogCategory::PROTOCOL_SPECIFIC;
            else if (category_str == "DIAGNOSTICS") category = DriverLogCategory::DIAGNOSTICS;
            
            LogLevel level = StringToLogLevel(level_str);
            category_levels_[category] = level;
        }
        
        PulseOne::LogManager::getInstance().Debug(
            "📊 Loaded {} category log levels from DB", result.size());
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Debug(
            "🔍 Failed to load category levels from DB: {}", e.what());
    }
}

bool LogLevelManager::SaveCategoryLevelToDB(DriverLogCategory category, LogLevel level,
                                           const EngineerID& changed_by) {
    if (!db_manager_) return false;
    
    try {
        std::string category_str = DriverLogCategoryToString(category);
        std::string level_str = LogLevelToString(level);
        
        std::string query = 
            "INSERT INTO category_log_levels (category, log_level, updated_by, updated_at) "
            "VALUES ('" + category_str + "', '" + level_str + "', '" + changed_by + "', NOW()) "
            "ON CONFLICT (category) DO UPDATE SET "
            "log_level = EXCLUDED.log_level, "
            "updated_by = EXCLUDED.updated_by, "
            "updated_at = EXCLUDED.updated_at";
            
        return db_manager_->executeNonQueryPostgres(query);
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().Error(
            "❌ Failed to save category log level to DB: {}", e.what());
        return false;
    }
}

} // namespace Core
} // namespace PulseOne