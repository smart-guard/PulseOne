// =============================================================================
// collector/include/Utils/LogManager.h - 완전한 기능 보존 + 자동 초기화
// 기존 모든 기능 100% 유지 + getInstance()에서 자동 초기화 추가
// =============================================================================

#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file LogManager.h
 * @brief PulseOne 통합 로그 관리자 (완전 최종 버전) - 자동 초기화 지원
 * @author PulseOne Development Team
 * @date 2025-07-29
 * @version 5.1.0 - 자동 초기화 기능 추가
 * 
 * 🎯 최종 완성:
 * - 🔥 NEW: getInstance() 호출 시 자동 초기화
 * - Common/Enums.h와 Common/Structs.h 사용
 * - LogManager는 전역 네임스페이스
 * - 모든 PulseOne 타입들을 그대로 활용
 * - 확장 기능 완전 지원
 */

#include "Common/Enums.h"
#include "Common/Structs.h"
#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <atomic>

// ✅ PulseOne 타입들을 전역에서 사용하기 위한 별칭
using LogLevel = PulseOne::Enums::LogLevel;
using DriverLogCategory = PulseOne::Enums::DriverLogCategory;
using DataQuality = PulseOne::Enums::DataQuality;

// ✅ Structs에서 가져오는 타입들
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using LogStatistics = PulseOne::Structs::LogStatistics;
using DriverLogContext = PulseOne::Structs::DriverLogContext;
using ErrorInfo = PulseOne::Structs::ErrorInfo;

// ✅ BasicTypes에서 가져오는 타입들
using UUID = PulseOne::BasicTypes::UUID;
using EngineerID = PulseOne::BasicTypes::EngineerID;

/**
 * @brief 통합 로그 관리자 (싱글톤, 전역 네임스페이스) - 자동 초기화 지원
 * @details 파일/콘솔 출력, 포맷팅, 카테고리 관리, 점검 기능 모두 포함
 */
class LogManager {
public:
    // =============================================================================
    // 🔥 핵심 개선: 자동 초기화 getInstance
    // =============================================================================
    
    /**
     * @brief 싱글톤 인스턴스 반환 (자동 초기화됨)
     * @return LogManager 인스턴스 참조
     */
    static LogManager& getInstance() {
        static LogManager instance;
        
        // 🔥 자동 초기화: 처음 호출 시 한 번만 실행
        static std::once_flag initialized;
        std::call_once(initialized, [&instance] {
            instance.doInitialize();
        });
        
        return instance;
    }
    
    /**
     * @brief 초기화 상태 확인
     * @return 초기화 완료 시 true
     */
    bool isInitialized() const {
        return initialized_.load();
    }

    // =============================================================================
    // 기존 코드 호환성을 위한 메소드들 (100% 유지)
    // =============================================================================
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    void Debug(const std::string& message);
    void Trace(const std::string& message);
    void Maintenance(const std::string& message);

    // =============================================================================
    // 포맷 문자열을 지원하는 템플릿 메소드들 (기존 유지)
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
    // 확장된 로그 메소드들 (Complete Common Types 사용) (기존 유지)
    // =============================================================================
    void log(const std::string& category, LogLevel level, const std::string& message);
    void log(const std::string& category, const std::string& level, const std::string& message);
    
    // ✅ 점검 관련 로그 메소드 (모든 타입 완전 지원)
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

    // ✅ 드라이버 로그 (Complete Types 지원)
    void logDriver(const UUID& device_id, DriverLogCategory category, 
                  LogLevel level, const std::string& message);
    
    // ✅ 데이터 품질 로그 (Complete Types 지원)
    void logDataQuality(const UUID& device_id, const UUID& point_id,
                       DataQuality quality, const std::string& reason = "");

    // =============================================================================
    // 로그 레벨 관리 (Complete Enums 지원) (기존 유지)
    // =============================================================================
    void setLogLevel(LogLevel level) { 
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = level; 
    }
    
    LogLevel getLogLevel() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return minLevel_; 
    }

    // ✅ 카테고리별 로그 레벨 설정 (Complete Enums 지원)
    void setCategoryLogLevel(DriverLogCategory category, LogLevel level);
    LogLevel getCategoryLogLevel(DriverLogCategory category) const;
    
    // ✅ 점검 모드 설정
    void setMaintenanceMode(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        maintenance_mode_enabled_ = enabled;
    }
    
    bool isMaintenanceModeEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maintenance_mode_enabled_;
    }

    // =============================================================================
    // 통계 및 상태 관리 (Complete Structs 지원) (기존 유지)
    // =============================================================================
    LogStatistics getStatistics() const;
    void resetStatistics();
    
    void flushAll();
    void rotateLogs();

private:
    // =============================================================================
    // 생성자/소멸자 (싱글톤)
    // =============================================================================
    LogManager();
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    // =============================================================================
    // 🔥 핵심: 실제 초기화 로직 (내부용)
    // =============================================================================
    
    /**
     * @brief 실제 초기화 로직 (thread-safe)
     * @return 초기화 성공 여부
     */
    bool doInitialize();

    // =============================================================================
    // 내부 유틸리티 메소드들 (기존 유지)
    // =============================================================================
    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string getCurrentTimestamp();
    std::string buildLogPath(const std::string& category);
    void writeToFile(const std::string& filePath, const std::string& message);
    bool shouldLog(LogLevel level) const;
    bool shouldLogCategory(DriverLogCategory category, LogLevel level) const;

    // ✅ 포맷팅 메소드들 (Complete Types 지원)
    std::string formatLogMessage(LogLevel level, const std::string& category,
                                const std::string& message);
    std::string formatMaintenanceLog(const UUID& device_id, const EngineerID& engineer_id,
                                   const std::string& message);

    // ✅ 통계 업데이트
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
    // 멤버 변수들 (Complete Types 지원 + 초기화 상태)
    // =============================================================================
    
    /// 🔥 NEW: 초기화 상태 추적
    std::atomic<bool> initialized_{false};
    
    mutable std::mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
    LogLevel minLevel_ = LogLevel::INFO;
    std::string defaultCategory_ = "system";
    
    // ✅ 카테고리별 로그 레벨 (Complete Enums)
    std::map<DriverLogCategory, LogLevel> categoryLevels_;
    
    // ✅ 점검 모드
    bool maintenance_mode_enabled_ = false;
    
    // ✅ 통계 정보 (Complete Structs)
    mutable LogStatistics statistics_;
    
    // 로그 로테이션 설정
    size_t max_log_size_mb_ = 100;
    int max_log_files_ = 30;
};

// =============================================================================
// 전역 편의 함수들 (Complete Types 지원) (기존 유지)
// =============================================================================

/**
 * @brief 전역 로거 인스턴스 (기존 호환성)
 */
inline LogManager& Logger() {
    return LogManager::getInstance();
}

/**
 * @brief 점검 로그 편의 함수 (Complete Types)
 */
inline void LogMaintenance(const UUID& device_id, 
                          const EngineerID& engineer_id,
                          const std::string& message) {
    LogManager::getInstance().logMaintenance(device_id, engineer_id, message);
}

/**
 * @brief 드라이버 로그 편의 함수 (Complete Types)
 */
inline void LogDriver(const UUID& device_id, 
                     DriverLogCategory category,
                     LogLevel level,
                     const std::string& message) {
    LogManager::getInstance().logDriver(device_id, category, level, message);
}

#endif // LOG_MANAGER_H