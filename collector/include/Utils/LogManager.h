#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file LogManager.h
 * @brief PulseOne 통합 로그 관리자 - PlatformCompat.h 사용
 * @author PulseOne Development Team
 * @date 2025-09-06
 */

// 🔥 FIRST: PlatformCompat.h가 모든 플랫폼 문제를 해결함
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
 * @details 다른 코드에서 LogManager::getInstance()로 직접 접근
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
    bool createDirectoryRecursive(const std::string& path);

    // =============================================================================
    // 내부 유틸리티
    // =============================================================================
    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string getCurrentTimestamp();
    std::string buildLogPath(const std::string& category);
    void writeToFile(const std::string& filePath, const std::string& message);
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

#endif // LOG_MANAGER_H