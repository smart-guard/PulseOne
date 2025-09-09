#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file LogManager.h
 * @brief PulseOne 통합 로그 관리자 - 완전한 설정 적용 + 크로스 플랫폼 지원
 * @author PulseOne Development Team
 * @date 2025-09-09
 * 
 * 완전한 기능:
 * - LOG_LEVEL, LOG_TO_CONSOLE, LOG_TO_FILE, LOG_FILE_PATH 설정 완전 적용
 * - Windows/Linux 크로스 플랫폼 경로 처리
 * - 설정 파일 실시간 재로드 지원
 * - 로그 로테이션 및 크기 관리
 */

// PlatformCompat.h가 모든 플랫폼 문제를 해결함
#include "Platform/PlatformCompat.h"

// 표준 헤더들
#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include <sstream>
#include <iostream>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>
#include <filesystem>  // 크로스 플랫폼 경로 처리용

// PulseOne 타입 시스템
#include "Common/Enums.h"
#include "Common/BasicTypes.h"
#include "Common/Structs.h"

// 전역 네임스페이스 타입 별칭 (기존 코드 호환성)
using LogLevel = PulseOne::Enums::LogLevel;
using DriverLogCategory = PulseOne::Enums::DriverLogCategory;
using DataQuality = PulseOne::Enums::DataQuality;
using UUID = PulseOne::BasicTypes::UUID;
using EngineerID = PulseOne::BasicTypes::EngineerID;
using Timestamp = PulseOne::BasicTypes::Timestamp;

/**
 * @brief 완전 전역 싱글톤 로그 관리자
 * @details 설정 파일 기반 동작, 크로스 플랫폼 경로 처리 지원
 */
class LogManager {
public:
    // =============================================================================
    // 전역 싱글톤 패턴 (Meyer's Singleton + 자동 초기화)
    // =============================================================================
    
    static LogManager& getInstance() {
        static LogManager instance;
        instance.ensureInitialized();
        return instance;
    }
    
    bool isInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }

    // =============================================================================
    // 기본 로그 메소드들 (기존 코드 100% 호환성)
    // =============================================================================
    
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    void Debug(const std::string& message);
    void Trace(const std::string& message);
    void Maintenance(const std::string& message);

    // =============================================================================
    // 포맷 문자열 지원 템플릿
    // =============================================================================
    
    template<typename... Args>
    void Info(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Info(message);
    }

    template<typename... Args>
    void Warn(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Warn(message);
    }

    template<typename... Args>
    void Error(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Error(message);
    }

    template<typename... Args>
    void Fatal(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Fatal(message);
    }

    template<typename... Args>
    void Debug(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Debug(message);
    }

    template<typename... Args>
    void Trace(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Trace(message);
    }

    // =============================================================================
    // 확장 로그 메소드들
    // =============================================================================
    
    void log(const std::string& category, LogLevel level, const std::string& message);
    void log(const std::string& category, const std::string& level, const std::string& message);
    
    // 점검 관련 로그
    void logMaintenance(const UUID& device_id, const EngineerID& engineer_id, 
                       const std::string& message);
    void logMaintenanceStart(const PulseOne::Structs::DeviceInfo& device, const EngineerID& engineer_id);
    void logMaintenanceEnd(const PulseOne::Structs::DeviceInfo& device, const EngineerID& engineer_id);
    void logRemoteControlBlocked(const UUID& device_id, const std::string& reason);
    
    // 드라이버 로그
    void logDriver(const std::string& driverName, const std::string& message);
    void logDriver(const UUID& device_id, DriverLogCategory category, 
                  LogLevel level, const std::string& message);
    
    // 데이터 품질 로그
    void logDataQuality(const UUID& device_id, const UUID& point_id,
                       DataQuality quality, const std::string& reason = "");

    // 특수 로그
    void logError(const std::string& message);
    void logPacket(const std::string& driver, const std::string& device,
                   const std::string& rawPacket, const std::string& decoded);

    // =============================================================================
    // 로그 레벨 관리
    // =============================================================================
    
    void setLogLevel(LogLevel level) { 
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = level; 
    }
    
    LogLevel getLogLevel() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return minLevel_; 
    }

    void setCategoryLogLevel(DriverLogCategory category, LogLevel level);
    LogLevel getCategoryLogLevel(DriverLogCategory category) const;
    
    void setMaintenanceMode(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        maintenance_mode_enabled_ = enabled;
    }
    
    bool isMaintenanceModeEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maintenance_mode_enabled_;
    }

    // =============================================================================
    // 새로 추가: 설정 관리 메소드들
    // =============================================================================
    
    // 설정 파일에서 재로드
    void reloadSettings();
    
    // 콘솔/파일 출력 제어 (LOG_TO_CONSOLE, LOG_TO_FILE 설정 런타임 변경)
    void setConsoleOutput(bool enabled);
    void setFileOutput(bool enabled);
    
    bool isConsoleOutputEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return console_output_enabled_;
    }
    
    bool isFileOutputEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return file_output_enabled_;
    }
    
    // 로그 경로 설정 (LOG_FILE_PATH 런타임 변경)
    void setLogBasePath(const std::string& path);
    
    std::string getLogBasePath() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_base_path_;
    }
    
    // 로그 로테이션 설정
    void setMaxLogSizeMB(size_t size_mb) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_log_size_mb_ = size_mb;
    }
    
    void setMaxLogFiles(int count) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_log_files_ = count;
    }

    // =============================================================================
    // 통계 및 관리
    // =============================================================================
    
    PulseOne::Structs::LogStatistics getStatistics() const;
    void resetStatistics();
    void flushAll();
    void rotateLogs();

private:
    // =============================================================================
    // 생성자/소멸자 (싱글톤이므로 private)
    // =============================================================================
    LogManager();
    ~LogManager();
    
    // 복사/이동 방지
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    // =============================================================================
    // 초기화 관련
    // =============================================================================
    void ensureInitialized();
    bool doInitialize();
    
    // 새로 추가: 설정 파일에서 로그 설정 로드
    void loadLogSettingsFromConfig();

    // =============================================================================
    // 크로스 플랫폼 경로 처리 (새로 추가)
    // =============================================================================
    
    // 경로 정규화 (Windows/Linux 호환)
    std::string normalizePath(const std::string& path);
    
    // 로그 디렉토리 및 파일 경로 생성
    std::filesystem::path buildLogDirectoryPath(const std::string& category);
    std::filesystem::path buildLogFilePath(const std::string& category);
    
    // 디렉토리 생성 (std::filesystem 우선, 레거시 폴백)
    void createLogDirectoriesRecursive();
    
    // 레거시 디렉토리 생성 (폴백용)
    bool createDirectoryRecursive(const std::string& path);

    // =============================================================================
    // 로그 경로 관리 (개선된 버전)
    // =============================================================================
    
    std::string buildLogPath(const std::string& category);
    std::string buildLogPathLegacy(const std::string& category);  // 폴백용

    // =============================================================================
    // 내부 유틸리티
    // =============================================================================
    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string getCurrentTimestamp();
    
    // 파일 쓰기 (설정 적용 버전)
    void writeToFile(const std::string& filePath, const std::string& message);
    
    // 로그 로테이션 체크
    void checkAndRotateLogFile(const std::string& filePath, std::ofstream& stream);
    
    bool shouldLog(LogLevel level) const;
    bool shouldLogCategory(DriverLogCategory category, LogLevel level) const;

    // 포맷팅
    std::string formatLogMessage(LogLevel level, const std::string& category,
                                const std::string& message);
    std::string formatMaintenanceLog(const UUID& device_id, const EngineerID& engineer_id,
                                   const std::string& message);

    // 통계
    void updateStatistics(LogLevel level);

    // 유틸리티 함수들
    std::string logLevelToString(LogLevel level);
    LogLevel stringToLogLevel(const std::string& level);
    std::string driverLogCategoryToString(DriverLogCategory category);
    std::string dataQualityToString(DataQuality quality);

    // =============================================================================
    // 포맷 문자열 구현
    // =============================================================================
    template<typename T>
    void formatHelper(std::stringstream& ss, const std::string& format, size_t& pos, const T& value) {
        size_t placeholder = format.find("{}", pos);
        if (placeholder != std::string::npos) {
            ss << format.substr(pos, placeholder - pos);
            ss << value;
            pos = placeholder + 2;
        } else {
            ss << format.substr(pos);
        }
    }

    void formatRecursive(std::stringstream& ss, const std::string& format, size_t& pos) {
        ss << format.substr(pos);
    }

    template<typename T, typename... Args>
    void formatRecursive(std::stringstream& ss, const std::string& format, size_t& pos, const T& value, Args&&... args) {
        formatHelper(ss, format, pos, value);
        formatRecursive(ss, format, pos, std::forward<Args>(args)...);
    }

    template<typename... Args>
    std::string formatString(const std::string& format, Args&&... args) {
        std::stringstream ss;
        size_t pos = 0;
        formatRecursive(ss, format, pos, std::forward<Args>(args)...);
        return ss.str();
    }

    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_;
    mutable std::mutex init_mutex_;
    
    // 로그 파일 및 설정
    mutable std::mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
    LogLevel minLevel_;
    std::string defaultCategory_;
    
    // 카테고리별 로그 레벨
    std::map<DriverLogCategory, LogLevel> categoryLevels_;
    
    // 점검 모드
    bool maintenance_mode_enabled_;
    
    // 통계 정보
    mutable PulseOne::Structs::LogStatistics statistics_;
    
    // 로그 로테이션 설정
    size_t max_log_size_mb_;
    int max_log_files_;
    
    // =============================================================================
    // 새로 추가: 설정 기반 제어 변수들
    // =============================================================================
    
    // 출력 제어 (LOG_TO_CONSOLE, LOG_TO_FILE 설정 적용)
    bool console_output_enabled_;
    bool file_output_enabled_;
    
    // 로그 경로 (LOG_FILE_PATH 설정 적용)
    std::string log_base_path_;
};

// =============================================================================
// 전역 편의 함수들 (기존 코드에서 사용하는 패턴)
// =============================================================================

inline LogManager& Logger() {
    return LogManager::getInstance();
}

inline void LogMaintenance(const UUID& device_id, 
                          const EngineerID& engineer_id,
                          const std::string& message) {
    LogManager::getInstance().logMaintenance(device_id, engineer_id, message);
}

inline void LogDriver(const UUID& device_id, 
                     DriverLogCategory category,
                     LogLevel level,
                     const std::string& message) {
    LogManager::getInstance().logDriver(device_id, category, level, message);
}

// =============================================================================
// 새로 추가: 설정 기반 로그 제어 편의 함수들
// =============================================================================

/**
 * @brief 로그 설정을 즉시 재로드
 * 설정 파일이 변경되었을 때 호출
 */
inline void ReloadLogSettings() {
    LogManager::getInstance().reloadSettings();
}

/**
 * @brief 콘솔 출력 즉시 제어
 * @param enabled true=콘솔 출력, false=콘솔 출력 안함
 */
inline void SetConsoleLogging(bool enabled) {
    LogManager::getInstance().setConsoleOutput(enabled);
}

/**
 * @brief 파일 출력 즉시 제어
 * @param enabled true=파일 출력, false=파일 출력 안함
 */
inline void SetFileLogging(bool enabled) {
    LogManager::getInstance().setFileOutput(enabled);
}

/**
 * @brief 로그 경로 즉시 변경
 * @param path 새 로그 경로 (Windows/Linux 자동 처리)
 */
inline void SetLogPath(const std::string& path) {
    LogManager::getInstance().setLogBasePath(path);
}

#endif // LOG_MANAGER_H