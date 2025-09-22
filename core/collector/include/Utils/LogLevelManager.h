// =============================================================================
// collector/include/Utils/LogLevelManager.h - 경고 없는 자동 초기화 버전
// 🔥 FIX: std::call_once 관련 모든 경고 해결
// =============================================================================

#ifndef LOG_LEVEL_MANAGER_H
#define LOG_LEVEL_MANAGER_H

/**
 * @file LogLevelManager.h
 * @brief 실시간 로그 레벨 관리자 (전역 네임스페이스) - 안전한 자동 초기화
 * @author PulseOne Development Team
 * @date 2025-07-30
 * @version 4.1.1 - 경고 없는 자동 초기화
 */

// ✅ 필요한 헤더만 선택적으로 import
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Utils/ConfigManager.h" 
#include "Database/DatabaseManager.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <functional>
#include <vector>
#include <mutex>
#include <sstream>
#include <string>

// ✅ 타입 별칭으로 깔끔하게 사용
using LogLevel = PulseOne::Enums::LogLevel;
using DriverLogCategory = PulseOne::Enums::DriverLogCategory;
using EngineerID = PulseOne::BasicTypes::EngineerID;
using Timestamp = PulseOne::BasicTypes::Timestamp;
using UniqueId = PulseOne::BasicTypes::UniqueId;

// ✅ 필요한 구조체만 별칭
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using LogStatistics = PulseOne::Structs::LogStatistics;

// ✅ 전역 네임스페이스에 정의
using LogLevelChangeCallback = std::function<void(LogLevel old_level, LogLevel new_level)>;

/**
 * @brief 로그 레벨 소스 열거형
 */
enum class LogLevelSource : uint8_t {
    FILE_CONFIG = 0,
    DATABASE = 1,
    WEB_API = 2,
    COMMAND_LINE = 3,
    MAINTENANCE_OVERRIDE = 4
};

/**
 * @brief 로그 레벨 변경 이벤트 정보
 */
struct LogLevelChangeEvent {
    LogLevel old_level;
    LogLevel new_level;
    LogLevelSource source;
    Timestamp change_time;
    EngineerID changed_by = "";
    std::string reason = "";
    bool is_maintenance_related = false;
    
    LogLevelChangeEvent() : change_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief 실시간 로그 레벨 관리자 (싱글톤) - 경고 없는 자동 초기화
 */
class LogLevelManager {
public:
    // =============================================================================
    // 🔥 FIX: 경고 없는 자동 초기화 getInstance
    // =============================================================================
    
    /**
     * @brief 싱글톤 인스턴스 반환 (자동 초기화됨)
     * @return LogLevelManager 인스턴스 참조
     */
    static LogLevelManager& getInstance() {
        static LogLevelManager instance;
        instance.ensureInitialized();
        return instance;
    }
    
    /**
     * @brief 초기화 상태 확인
     * @return 초기화 완료 시 true
     */
    bool isInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }
    
    // =============================================================================
    // 기존 API 호환성 (100% 유지)
    // =============================================================================
    
    /**
     * @brief 수동 초기화 (기존 호환성)
     */
    void Initialize(ConfigManager* config = nullptr, DatabaseManager* db = nullptr) {
        doInitialize(config, db);
    }
    
    void Shutdown();
    
    // =============================================================================
    // 기본 로그 레벨 관리
    // =============================================================================
    void SetLogLevel(LogLevel level, LogLevelSource source = LogLevelSource::WEB_API,
                    const EngineerID& changed_by = "SYSTEM", const std::string& reason = "");
    LogLevel GetLogLevel() const { return current_level_; }
    
    // =============================================================================
    // 카테고리별 로그 레벨 관리
    // =============================================================================
    void SetCategoryLogLevel(DriverLogCategory category, LogLevel level);
    LogLevel GetCategoryLogLevel(DriverLogCategory category) const;
    void ResetCategoryLogLevels();
    
    // =============================================================================
    // 점검 모드 관리
    // =============================================================================
    void SetMaintenanceMode(bool enabled, LogLevel maintenance_level = LogLevel::TRACE,
                           const EngineerID& engineer_id = "");
    bool IsMaintenanceMode() const { return maintenance_mode_.load(); }
    LogLevel GetMaintenanceLevel() const { return maintenance_level_; }
    
    // =============================================================================
    // 웹 API 지원
    // =============================================================================
    bool UpdateLogLevelInDB(LogLevel level, const EngineerID& changed_by, 
                           const std::string& reason = "Updated via Web API");
    bool UpdateCategoryLogLevelInDB(DriverLogCategory category, LogLevel level,
                                   const EngineerID& changed_by);
    bool StartMaintenanceModeFromWeb(const EngineerID& engineer_id, 
                                    LogLevel maintenance_level = LogLevel::TRACE);
    bool EndMaintenanceModeFromWeb(const EngineerID& engineer_id);
    
    // =============================================================================
    // 콜백 관리
    // =============================================================================
    void RegisterChangeCallback(const LogLevelChangeCallback& callback);
    void UnregisterAllCallbacks();
    
    // =============================================================================
    // 이력 관리
    // =============================================================================
    std::vector<LogLevelChangeEvent> GetChangeHistory(size_t max_count = 100) const;
    void ClearChangeHistory();
    
    // =============================================================================
    // 상태 조회 및 진단
    // =============================================================================
    
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
        
        std::string ToJson() const {
            std::ostringstream oss;
            oss << "{"
                << "\"current_level\":" << static_cast<int>(current_level) << ","
                << "\"maintenance_level\":" << static_cast<int>(maintenance_level) << ","
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
    
    void RunDiagnostics();
    bool ValidateConfiguration() const;

private:
    // =============================================================================
    // 생성자/소멸자 (싱글톤)
    // =============================================================================
    
    LogLevelManager();
    ~LogLevelManager();
    
    // 복사/이동 방지
    LogLevelManager(const LogLevelManager&) = delete;
    LogLevelManager& operator=(const LogLevelManager&) = delete;
    LogLevelManager(LogLevelManager&&) = delete;
    LogLevelManager& operator=(LogLevelManager&&) = delete;
    
    // =============================================================================
    // 🔥 경고 없는 초기화 로직
    // =============================================================================
    void ensureInitialized();
    bool doInitialize();
    bool doInitialize(ConfigManager* config, DatabaseManager* db);
    
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
    
    void LoadCategoryLevelsFromDB();
    bool SaveCategoryLevelToDB(DriverLogCategory category, LogLevel level,
                              const EngineerID& changed_by);
    
    void NotifyLevelChange(const LogLevelChangeEvent& event);
    void AddToHistory(const LogLevelChangeEvent& event);
    
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
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    /// 초기화 상태 (원자적 연산)
    std::atomic<bool> initialized_;
    
    /// 초기화용 뮤텍스
    mutable std::mutex init_mutex_;
    
    /// 기본 상태
    LogLevel current_level_;
    LogLevel maintenance_level_;
    ConfigManager* config_;
    DatabaseManager* db_manager_;
    
    /// 스레드 및 모니터링
    std::atomic<bool> running_;
    std::atomic<bool> maintenance_mode_;
    std::thread monitor_thread_;
    std::chrono::steady_clock::time_point last_db_check_;
    std::chrono::steady_clock::time_point last_file_check_;
    
    /// 통계 카운터들
    std::atomic<uint64_t> level_change_count_;
    std::atomic<uint64_t> db_check_count_;
    std::atomic<uint64_t> file_check_count_;
    
    /// 카테고리별 로그 레벨 관리
    std::map<DriverLogCategory, LogLevel> category_levels_;
    mutable std::mutex category_mutex_;
    
    /// 콜백 및 이벤트 관리
    std::vector<LogLevelChangeCallback> change_callbacks_;
    std::vector<LogLevelChangeEvent> change_history_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex history_mutex_;
};

#endif // LOG_LEVEL_MANAGER_H