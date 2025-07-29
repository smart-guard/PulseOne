#ifndef LOG_LEVEL_MANAGER_H
#define LOG_LEVEL_MANAGER_H

/**
 * @file LogLevelManager.h
 * @brief 실시간 로그 레벨 관리자 (UnifiedCommonTypes.h 기반)
 * @author PulseOne Development Team
 * @date 2025-07-24
 * @version 2.0.0 - UnifiedCommonTypes.h 적용
 */

#include "Common/UnifiedCommonTypes.h"
#include "Utils/ConfigManager.h" 
#include "Database/DatabaseManager.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <functional>
#include <vector>
#include <mutex>

namespace PulseOne {
namespace Core {

/**
 * @brief 로그 레벨 변경 콜백 타입
 */
using LogLevelChangeCallback = std::function<void(LogLevel old_level, LogLevel new_level)>;

/**
 * @brief 로그 레벨 소스 열거형
 */
enum class LogLevelSource : uint8_t {
    FILE_CONFIG = 0,        // 설정 파일에서
    DATABASE = 1,           // 데이터베이스에서
    WEB_API = 2,           // 웹 API에서
    COMMAND_LINE = 3,      // 명령줄에서
    MAINTENANCE_OVERRIDE = 4 // 점검 모드 오버라이드
};

/**
 * @brief 로그 레벨 변경 이벤트 정보
 */
struct LogLevelChangeEvent {
    LogLevel old_level;
    LogLevel new_level;
    LogLevelSource source;
    Timestamp change_time;
    EngineerID changed_by = "";     // 🆕 변경한 엔지니어 ID
    std::string reason = "";        // 🆕 변경 이유
    bool is_maintenance_related = false; // 🆕 점검 관련 변경인지
    
    LogLevelChangeEvent() : change_time(PulseOne::Utils::GetCurrentTimestamp()) {}
};

/**
 * @brief 실시간 로그 레벨 관리자 (싱글톤)
 * @details DB/파일/웹에서 로그 레벨을 실시간으로 모니터링하여 동적 변경
 */
class LogLevelManager {
private:
    LogLevel current_level_;
    LogLevel maintenance_level_;           // 🆕 점검 모드 시 로그 레벨
    ConfigManager* config_;
    DatabaseManager* db_manager_;
    
    std::atomic<bool> running_;
    std::atomic<bool> maintenance_mode_;   // 🆕 점검 모드 상태
    std::thread monitor_thread_;
    std::chrono::steady_clock::time_point last_db_check_;
    std::chrono::steady_clock::time_point last_file_check_;
    
    // 🆕 통계 카운터들
    std::atomic<uint64_t> level_change_count_;
    std::atomic<uint64_t> db_check_count_;
    std::atomic<uint64_t> file_check_count_;
    
    // 🆕 카테고리별 로그 레벨 관리
    std::map<DriverLogCategory, LogLevel> category_levels_;
    mutable std::mutex category_mutex_;
    
    // 🆕 콜백 및 이벤트 관리
    std::vector<LogLevelChangeCallback> change_callbacks_;
    std::vector<LogLevelChangeEvent> change_history_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex history_mutex_;

public:
    // =============================================================================
    // 싱글톤 및 생명주기
    // =============================================================================
    static LogLevelManager& getInstance();
    void Initialize(ConfigManager* config, DatabaseManager* db);
    void Shutdown();
    
    // =============================================================================
    // 기본 로그 레벨 관리
    // =============================================================================
    void SetLogLevel(LogLevel level, LogLevelSource source = LogLevelSource::WEB_API,
                    const EngineerID& changed_by = "SYSTEM", const std::string& reason = "");
    LogLevel GetLogLevel() const { return current_level_; }
    
    // =============================================================================
    // 카테고리별 로그 레벨 관리 (🆕)
    // =============================================================================
    void SetCategoryLogLevel(DriverLogCategory category, LogLevel level);
    LogLevel GetCategoryLogLevel(DriverLogCategory category) const;
    void ResetCategoryLogLevels();
    
    // =============================================================================
    // 점검 모드 관리 (🆕)
    // =============================================================================
    void SetMaintenanceMode(bool enabled, LogLevel maintenance_level = LogLevel::TRACE,
                           const EngineerID& engineer_id = "");
    bool IsMaintenanceMode() const { return maintenance_mode_.load(); }
    LogLevel GetMaintenanceLevel() const { return maintenance_level_; }
    
    // =============================================================================
    // 웹 API 지원 (🆕)
    // =============================================================================
    bool UpdateLogLevelInDB(LogLevel level, const EngineerID& changed_by, 
                           const std::string& reason = "Updated via Web API");
    bool UpdateCategoryLogLevelInDB(DriverLogCategory category, LogLevel level,
                                   const EngineerID& changed_by);
    bool StartMaintenanceModeFromWeb(const EngineerID& engineer_id, 
                                    LogLevel maintenance_level = LogLevel::TRACE);
    bool EndMaintenanceModeFromWeb(const EngineerID& engineer_id);
    
    // =============================================================================
    // 콜백 관리 (🆕)
    // =============================================================================
    void RegisterChangeCallback(const LogLevelChangeCallback& callback);
    void UnregisterAllCallbacks();
    
    // =============================================================================
    // 이력 관리 (🆕)
    // =============================================================================
    std::vector<LogLevelChangeEvent> GetChangeHistory(size_t max_count = 100) const;
    void ClearChangeHistory();
    
    // =============================================================================
    // 상태 조회 및 진단 (🆕)
    // =============================================================================
    
    /**
     * @brief 매니저 상태 정보
     */
    struct ManagerStatus {
        LogLevel current_level;
        LogLevel maintenance_level;
        bool maintenance_mode;
        bool monitoring_active;
        uint64_t db_check_count;
        uint64_t file_check_count;
        uint64_t level_change_count;
        std::chrono::system_clock::time_point last_db_check;
        std::chrono::system_clock::time_point last_file_check;
        size_t active_callbacks;
        size_t history_entries;
        
        /**
         * @brief JSON 형태로 상태 정보 반환
         */
        std::string ToJson() const {
            std::ostringstream oss;
            oss << "{"
                << "\"current_level\":\"" << static_cast<int>(current_level) << "\","
                << "\"maintenance_level\":\"" << static_cast<int>(maintenance_level) << "\","
                << "\"maintenance_mode\":" << (maintenance_mode ? "true" : "false") << ","
                << "\"monitoring_active\":" << (monitoring_active ? "true" : "false") << ","
                << "\"db_check_count\":" << db_check_count << ","
                << "\"file_check_count\":" << file_check_count << ","
                << "\"level_change_count\":" << level_change_count << ","
                << "\"active_callbacks\":" << active_callbacks << ","
                << "\"history_entries\":" << history_entries
                << "}";
            return oss.str();
        }
    };
    
    ManagerStatus GetStatus() const;
    std::string GetStatusJson() const { return GetStatus().ToJson(); }
    
    // 🆕 진단 정보
    void RunDiagnostics();
    bool ValidateConfiguration() const;

private:
    LogLevelManager();
    ~LogLevelManager() { Shutdown(); }
    
    // 복사 방지
    LogLevelManager(const LogLevelManager&) = delete;
    LogLevelManager& operator=(const LogLevelManager&) = delete;
    
    // =============================================================================
    // 모니터링 관련
    // =============================================================================
    void StartMonitoring();
    void StopMonitoring();
    void MonitoringLoop();
    void CheckDatabaseChanges();
    void CheckFileChanges();
    
    // =============================================================================
    // 내부 구현 메소드들
    // =============================================================================
    LogLevel LoadLogLevelFromDB();
    LogLevel LoadLogLevelFromFile();
    bool SaveLogLevelToDB(LogLevel level, LogLevelSource source, 
                         const EngineerID& changed_by, const std::string& reason);
    
    // 🆕 카테고리 레벨 DB 관리
    void LoadCategoryLevelsFromDB();
    bool SaveCategoryLevelToDB(DriverLogCategory category, LogLevel level,
                              const EngineerID& changed_by);
    
    // 🆕 이벤트 처리
    void NotifyLevelChange(const LogLevelChangeEvent& event);
    void AddToHistory(const LogLevelChangeEvent& event);
    
    // 🆕 유틸리티
    std::string LogLevelSourceToString(LogLevelSource source) const {
        switch (source) {
            case LogLevelSource::FILE_CONFIG: return "FILE_CONFIG";
            case LogLevelSource::DATABASE: return "DATABASE";
            case LogLevelSource::WEB_API: return "WEB_API";
            case LogLevelSource::COMMAND_LINE: return "COMMAND_LINE";
            case LogLevelSource::MAINTENANCE_OVERRIDE: return "MAINTENANCE_OVERRIDE";
            default: return "UNKNOWN";
        }
    }
};

} // namespace Core
} // namespace PulseOne

#endif // LOG_LEVEL_MANAGER_H