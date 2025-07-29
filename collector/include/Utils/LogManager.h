#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file LogManager.h
 * @brief PulseOne 통합 로그 관리자 (UnifiedCommonTypes.h 기반)
 * @author PulseOne Development Team
 * @date 2025-07-24
 * @version 2.0.0 - UnifiedCommonTypes.h 적용
 */

#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace PulseOne {

namespace Utils = PulseOne::Utils;
/**
 * @brief 통합 로그 관리자 (싱글톤)
 * @details 파일/콘솔 출력, 포맷팅, 카테고리 관리 담당
 */
class LogManager {
public:
    static LogManager& getInstance();

    // =============================================================================
    // 기존 코드 호환성을 위한 메소드들
    // =============================================================================
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    void Debug(const std::string& message);
    void Trace(const std::string& message);        // 🆕 TRACE 레벨 추가
    void Maintenance(const std::string& message);  // 🆕 점검 로그

    // =============================================================================
    // 포맷 문자열을 지원하는 템플릿 메소드들
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

    template<typename... Args>
    void Maintenance(const std::string& format, Args&&... args) {
        std::string message = formatString(format, std::forward<Args>(args)...);
        Maintenance(message);
    }

    // =============================================================================
    // 확장된 로그 메소드들 (UnifiedCommonTypes 기반)
    // =============================================================================
    void log(const std::string& category, LogLevel level, const std::string& message);
    void log(const std::string& category, const std::string& level, const std::string& message);
    
    // 🆕 점검 관련 로그 메소드
    void logMaintenance(const UUID& device_id, const EngineerID& engineer_id, 
                       const std::string& message);
    void logMaintenanceStart(const DeviceInfo& device, const EngineerID& engineer_id);
    void logMaintenanceEnd(const DeviceInfo& device, const EngineerID& engineer_id);
    void logRemoteControlBlocked(const UUID& device_id, const std::string& reason);
    
    // 특수 목적 로그 (기존 호환)
    void logDriver(const std::string& driverName, const std::string& message);
    void logError(const std::string& message);
    void logPacket(const std::string& driver, const std::string& device,
                   const std::string& rawPacket, const std::string& decoded);

    // 🆕 드라이버 로그 (DriverLogCategory 지원)
    void logDriver(const UUID& device_id, DriverLogCategory category, 
                  LogLevel level, const std::string& message);
    
    // 🆕 데이터 품질 로그
    void logDataQuality(const UUID& device_id, const UUID& point_id,
                       DataQuality quality, const std::string& reason = "");

    // =============================================================================
    // 로그 레벨 관리 (UnifiedCommonTypes 기반)
    // =============================================================================
    void setLogLevel(LogLevel level) { 
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = level; 
    }
    
    LogLevel getLogLevel() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return minLevel_; 
    }

    // 🆕 카테고리별 로그 레벨 설정
    void setCategoryLogLevel(DriverLogCategory category, LogLevel level);
    LogLevel getCategoryLogLevel(DriverLogCategory category) const;
    
    // 🆕 점검 모드 설정
    void setMaintenanceMode(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        maintenance_mode_enabled_ = enabled;
    }
    
    bool isMaintenanceModeEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maintenance_mode_enabled_;
    }

    // =============================================================================
    // 통계 및 상태 관리
    // =============================================================================
    LogStatistics getStatistics() const;
    void resetStatistics();
    
    void flushAll();
    void rotateLogs();  // 🆕 로그 로테이션

private:
    LogManager();
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    // =============================================================================
    // 내부 유틸리티 메소드들
    // =============================================================================
    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string getCurrentTimestamp();  // 🆕 ISO 8601 타임스탬프
    std::string buildLogPath(const std::string& category);
    void writeToFile(const std::string& filePath, const std::string& message);
    bool shouldLog(LogLevel level) const;
    bool shouldLogCategory(DriverLogCategory category, LogLevel level) const;  // 🆕

    // 🆕 포맷팅 메소드들
    std::string formatLogMessage(LogLevel level, const std::string& category,
                                const std::string& message);
    std::string formatMaintenanceLog(const UUID& device_id, const EngineerID& engineer_id,
                                   const std::string& message);

    // 🆕 통계 업데이트
    void updateStatistics(LogLevel level);

    // =============================================================================
    // 포맷 문자열 구현 (기존 유지)
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
    mutable std::mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
    LogLevel minLevel_ = PulseOne::LogLevel::INFO;
    std::string defaultCategory_ = PulseOne::LOG_MODULE_SYSTEM;  // 🆕 Constants 사용
    
    // 🆕 카테고리별 로그 레벨
    std::map<DriverLogCategory, LogLevel> categoryLevels_;
    
    // 🆕 점검 모드
    bool maintenance_mode_enabled_ = false;
    
    // 🆕 통계 정보
    mutable LogStatistics statistics_;
    
    // 🆕 로그 로테이션 설정
    size_t max_log_size_mb_ = 100;
    int max_log_files_ = 30;
};

} // namespace PulseOne

// =============================================================================
// 전역 편의 함수들 (기존 호환성 + 새 기능)
// =============================================================================

/**
 * @brief 전역 로거 인스턴스 (기존 호환성)
 */
inline PulseOne::LogManager& Logger() {
    return PulseOne::LogManager::getInstance();
}

/**
 * @brief 점검 로그 편의 함수 (🆕)
 */
inline void LogMaintenance(const PulseOne::UUID& device_id, 
                          const PulseOne::EngineerID& engineer_id,
                          const std::string& message) {
    PulseOne::LogManager::getInstance().logMaintenance(device_id, engineer_id, message);
}

/**
 * @brief 드라이버 로그 편의 함수 (🆕)
 */
inline void LogDriver(const PulseOne::UUID& device_id, 
                     PulseOne::DriverLogCategory category,
                     PulseOne::LogLevel level,
                     const std::string& message) {
    PulseOne::LogManager::getInstance().logDriver(device_id, category, level, message);
}

#endif // LOG_MANAGER_H