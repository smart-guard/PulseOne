/**
 * @file LogManager.cpp
 * @brief PulseOne 통합 로그 관리자 - 완전 크로스 컴파일 대응
 * @author PulseOne Development Team
 * @date 2025-09-06
 * 
 * 완전한 크로스 플랫폼 지원:
 * - Windows/Linux API 충돌 완전 해결
 * - 새 헤더와 100% 매칭 (올바른 enum 값들)
 * - LogStatistics 완전한 구조체 사용
 * - MinGW 크로스 컴파일 완전 대응
 */

#include "Utils/LogManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

// =============================================================================
// 🔥 크로스 플랫폼 파일시스템 처리 (헤더 이후)
// =============================================================================

#if PULSEONE_WINDOWS
    #include <direct.h>
    #include <io.h>
    #define PATH_SEPARATOR "\\"
    #define MKDIR(path) _mkdir(path)
    #define ACCESS(path, mode) _access(path, mode)
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define PATH_SEPARATOR "/"
    #define MKDIR(path) mkdir(path, 0755)
    #define ACCESS(path, mode) access(path, mode)
#endif

// =============================================================================
// 생성자/소멸자 및 초기화
// =============================================================================

LogManager::LogManager() 
    : initialized_(false)
    , minLevel_(LogLevel::INFO)
    , defaultCategory_("system")
    , maintenance_mode_enabled_(false)
    , max_log_size_mb_(100)
    , max_log_files_(30) {
    // 생성자에서는 기본값만 설정
}

LogManager::~LogManager() {
    flushAll();
}

void LogManager::ensureInitialized() {
    // 빠른 체크 (이미 초기화됨)
    if (initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    // 느린 체크 (뮤텍스 사용)
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_.load(std::memory_order_relaxed)) {
        return;
    }
    
    // 실제 초기화 수행
    doInitialize();
    initialized_.store(true, std::memory_order_release);
}

bool LogManager::doInitialize() {
    if (initialized_.load()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 다시 한 번 체크
    if (initialized_.load()) {
        return true;
    }
    
    try {
        std::cout << "LogManager 자동 초기화 시작...\n";
        
        // 🔥 올바른 enum 값들로 설정 (Common/Enums.h 기준)
        categoryLevels_[DriverLogCategory::GENERAL] = LogLevel::INFO;
        categoryLevels_[DriverLogCategory::CONNECTION] = LogLevel::INFO;
        categoryLevels_[DriverLogCategory::COMMUNICATION] = LogLevel::WARN;
        categoryLevels_[DriverLogCategory::DATA_PROCESSING] = LogLevel::INFO;
        categoryLevels_[DriverLogCategory::ERROR_HANDLING] = LogLevel::LOG_ERROR;
        categoryLevels_[DriverLogCategory::PERFORMANCE] = LogLevel::WARN;
        categoryLevels_[DriverLogCategory::SECURITY] = LogLevel::WARN;
        categoryLevels_[DriverLogCategory::PROTOCOL_SPECIFIC] = LogLevel::DEBUG;
        categoryLevels_[DriverLogCategory::DIAGNOSTICS] = LogLevel::DEBUG;
        categoryLevels_[DriverLogCategory::MAINTENANCE] = LogLevel::WARN;
        
        // 로그 디렉토리 확인 및 생성
        createDirectoryRecursive("logs");
        
        initialized_.store(true);
        
        std::cout << "LogManager 자동 초기화 완료\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "LogManager 초기화 실패: " << e.what() << "\n";
        return false;
    }
}

// =============================================================================
// 플랫폼별 디렉토리 생성
// =============================================================================

bool LogManager::createDirectoryRecursive(const std::string& path) {
    if (ACCESS(path.c_str(), 0) == 0) {
        return true; // 이미 존재함
    }
    
    // 부모 디렉토리 먼저 생성
    size_t pos = path.find_last_of(PATH_SEPARATOR);
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        if (!createDirectoryRecursive(parent)) {
            return false;
        }
    }
    
    return MKDIR(path.c_str()) == 0;
}

// =============================================================================
// 시간 관련 유틸리티 (크로스 플랫폼 안전)
// =============================================================================

std::string LogManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    SAFE_LOCALTIME(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y%m%d");
    return oss.str();
}

std::string LogManager::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    SAFE_LOCALTIME(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    return oss.str();
}

std::string LogManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    SAFE_LOCALTIME(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// =============================================================================
// 로그 경로 및 파일 관리
// =============================================================================

std::string LogManager::buildLogPath(const std::string& category) {
    std::string date = getCurrentDate();
    std::string baseDir;

    if (category.rfind("packet_", 0) == 0) {
        baseDir = "logs" PATH_SEPARATOR "packets" PATH_SEPARATOR + date + PATH_SEPARATOR + category.substr(7);
    } else if (category == "maintenance") {
        baseDir = "logs" PATH_SEPARATOR "maintenance" PATH_SEPARATOR + date;
    } else {
        baseDir = "logs" PATH_SEPARATOR + date;
    }

    std::string fullPath;
    if (category.rfind("packet_", 0) == 0) {
        fullPath = baseDir + ".log";
    } else {
        fullPath = baseDir + PATH_SEPARATOR + category + ".log";
    }

    // 디렉토리 생성
    size_t pos = fullPath.find_last_of(PATH_SEPARATOR);
    if (pos != std::string::npos) {
        std::string dir = fullPath.substr(0, pos);
        createDirectoryRecursive(dir);
    }

    return fullPath;
}

// =============================================================================
// 로그 레벨 검사
// =============================================================================

bool LogManager::shouldLog(LogLevel level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(level) >= static_cast<int>(minLevel_);
}

bool LogManager::shouldLogCategory(DriverLogCategory category, LogLevel level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = categoryLevels_.find(category);
    LogLevel categoryLevel = (it != categoryLevels_.end()) ? it->second : minLevel_;
    
    return static_cast<int>(level) >= static_cast<int>(categoryLevel);
}

// =============================================================================
// 메시지 포맷팅
// =============================================================================

std::string LogManager::formatLogMessage(LogLevel level, const std::string& category,
                                       const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "]"
        << "[" << logLevelToString(level) << "]";
    
    if (!category.empty() && category != defaultCategory_) {
        oss << "[" << category << "]";
    }
    
    oss << " " << message;
    return oss.str();
}

std::string LogManager::formatMaintenanceLog(const UUID& device_id, 
                                           const EngineerID& engineer_id,
                                           const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "]"
        << "[MAINTENANCE]"
        << "[Device:" << device_id << "]"
        << "[Engineer:" << engineer_id << "]"
        << " " << message;
    return oss.str();
}

// =============================================================================
// 파일 쓰기 및 통계
// =============================================================================

void LogManager::writeToFile(const std::string& filePath, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 콘솔에도 출력
    std::cout << message << std::endl;
    
    std::ofstream& stream = logFiles_[filePath];
    if (!stream.is_open()) {
        stream.open(filePath, std::ios::app);
    }
    stream << message << std::endl;
    stream.flush();
}

void LogManager::updateStatistics(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (level) {
        case LogLevel::TRACE:
            statistics_.trace_count++;
            break;
        case LogLevel::DEBUG:
            statistics_.debug_count++;
            break;
        case LogLevel::INFO:
            statistics_.info_count++;
            break;
        case LogLevel::WARN:
            statistics_.warn_count++;
            statistics_.warning_count++;  // 별칭 동기화
            break;
        case LogLevel::LOG_ERROR:
            statistics_.error_count++;
            break;
        case LogLevel::LOG_FATAL:
            statistics_.fatal_count++;
            break;
        case LogLevel::MAINTENANCE:
            statistics_.maintenance_count++;
            break;
        default:
            break;
    }

    statistics_.total_logs++;
    statistics_.last_log_time = std::chrono::system_clock::now();
}

// =============================================================================
// 기본 로그 메소드들
// =============================================================================

void LogManager::Info(const std::string& message) {
    log(defaultCategory_, LogLevel::INFO, message);
}

void LogManager::Warn(const std::string& message) {
    log(defaultCategory_, LogLevel::WARN, message);
}

void LogManager::Error(const std::string& message) {
    log(defaultCategory_, LogLevel::LOG_ERROR, message);
}

void LogManager::Fatal(const std::string& message) {
    log(defaultCategory_, LogLevel::LOG_FATAL, message);
}

void LogManager::Debug(const std::string& message) {
    log(defaultCategory_, LogLevel::DEBUG, message);
}

void LogManager::Trace(const std::string& message) {
    log(defaultCategory_, LogLevel::TRACE, message);
}

void LogManager::Maintenance(const std::string& message) {
    if (maintenance_mode_enabled_) {
        log("maintenance", LogLevel::MAINTENANCE, message);
    }
}

// =============================================================================
// 확장 로그 메소드들
// =============================================================================

void LogManager::log(const std::string& category, LogLevel level, const std::string& message) {
    if (!shouldLog(level)) return;
    
    updateStatistics(level);
    std::string formatted = formatLogMessage(level, category, message);
    std::string path = buildLogPath(category);
    writeToFile(path, formatted);
}

void LogManager::log(const std::string& category, const std::string& level, const std::string& message) {
    LogLevel logLevel = stringToLogLevel(level);
    log(category, logLevel, message);
}

// =============================================================================
// 점검 관련 로그 메소드들
// =============================================================================

void LogManager::logMaintenance(const UUID& device_id, const EngineerID& engineer_id, 
                               const std::string& message) {
    std::string formatted = formatMaintenanceLog(device_id, engineer_id, message);
    std::string path = buildLogPath("maintenance");
    writeToFile(path, formatted);
}

void LogManager::logMaintenanceStart(const PulseOne::Structs::DeviceInfo& device, const EngineerID& engineer_id) {
    std::ostringstream oss;
    oss << "MAINTENANCE START - Device: " << device.name 
        << " (" << device.id << "), Engineer: " << engineer_id;
    
    logMaintenance(device.id, engineer_id, oss.str());
}

void LogManager::logMaintenanceEnd(const PulseOne::Structs::DeviceInfo& device, const EngineerID& engineer_id) {
    std::ostringstream oss;
    oss << "MAINTENANCE COMPLETED - Device: " << device.name 
        << " (" << device.id << ")";
    logMaintenance(device.id, engineer_id, oss.str());
}

void LogManager::logRemoteControlBlocked(const UUID& device_id, const std::string& reason) {
    std::ostringstream oss;
    oss << "REMOTE CONTROL BLOCKED - Device: " << device_id << ", Reason: " << reason;
    log("security", LogLevel::WARN, oss.str());
}

// =============================================================================
// 드라이버 로그
// =============================================================================

void LogManager::logDriver(const UUID& device_id, DriverLogCategory category, 
                          LogLevel level, const std::string& message) {
    if (!shouldLogCategory(category, level)) return;
    
    std::ostringstream oss;
    oss << "[Device:" << device_id << "][" << driverLogCategoryToString(category) << "] " << message;
    
    std::string categoryName = "driver_" + driverLogCategoryToString(category);
    log(categoryName, level, oss.str());
}

// =============================================================================
// 데이터 품질 로그
// =============================================================================

void LogManager::logDataQuality(const UUID& device_id, const UUID& point_id,
                               DataQuality quality, const std::string& reason) {
    if (quality == DataQuality::GOOD) return;
    
    std::ostringstream oss;
    oss << "DATA QUALITY ISSUE - Device: " << device_id 
        << ", Point: " << point_id 
        << ", Quality: " << dataQualityToString(quality);
    
    if (!reason.empty()) {
        oss << ", Reason: " << reason;
    }
    
    LogLevel level = (quality == DataQuality::BAD) ? LogLevel::LOG_ERROR : LogLevel::WARN;
    log("data_quality", level, oss.str());
}

// =============================================================================
// 기존 특수 로그 메소드들
// =============================================================================

void LogManager::logDriver(const std::string& driverName, const std::string& message) {
    log("driver_" + driverName, LogLevel::INFO, message);
}

void LogManager::logError(const std::string& message) {
    log("error", LogLevel::LOG_ERROR, message);
}

void LogManager::logPacket(const std::string& driver, const std::string& device,
                           const std::string& rawPacket, const std::string& decoded) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "]\n[RAW] " << rawPacket << "\n[DECODED] " << decoded;
    std::string category = "packet_" + driver + "/" + device;
    std::string path = buildLogPath(category);
    writeToFile(path, oss.str());
}

// =============================================================================
// 카테고리별 로그 레벨 관리
// =============================================================================

void LogManager::setCategoryLogLevel(DriverLogCategory category, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    categoryLevels_[category] = level;
}

LogLevel LogManager::getCategoryLogLevel(DriverLogCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = categoryLevels_.find(category);
    return (it != categoryLevels_.end()) ? it->second : minLevel_;
}

// =============================================================================
// 통계 및 관리
// =============================================================================

PulseOne::Structs::LogStatistics LogManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void LogManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_ = PulseOne::Structs::LogStatistics{};
}

void LogManager::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : logFiles_) {
        if (kv.second.is_open()) {
            kv.second.flush();
            kv.second.close();
        }
    }
    logFiles_.clear();
}

void LogManager::rotateLogs() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 모든 열린 파일 플러시 및 닫기
    for (auto& kv : logFiles_) {
        if (kv.second.is_open()) {
            kv.second.flush();
            kv.second.close();
        }
    }
    logFiles_.clear();
    
    Info("Log rotation completed");
}

// =============================================================================
// 유틸리티 함수들 (크로스 플랫폼)
// =============================================================================

std::string LogManager::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::LOG_ERROR: return "ERROR";
        case LogLevel::LOG_FATAL: return "FATAL";
        case LogLevel::MAINTENANCE: return "MAINT";
        default: return "UNKNOWN";
    }
}

LogLevel LogManager::stringToLogLevel(const std::string& level) {
    if (level == "TRACE") return LogLevel::TRACE;
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::LOG_ERROR;
    if (level == "FATAL") return LogLevel::LOG_FATAL;
    if (level == "MAINTENANCE" || level == "MAINT") return LogLevel::MAINTENANCE;
    return LogLevel::INFO;
}

std::string LogManager::driverLogCategoryToString(DriverLogCategory category) {
    switch (category) {
        case DriverLogCategory::GENERAL: return "GENERAL";
        case DriverLogCategory::CONNECTION: return "CONNECTION";
        case DriverLogCategory::COMMUNICATION: return "COMMUNICATION";
        case DriverLogCategory::DATA_PROCESSING: return "DATA_PROCESSING";
        case DriverLogCategory::ERROR_HANDLING: return "ERROR_HANDLING";
        case DriverLogCategory::PERFORMANCE: return "PERFORMANCE";
        case DriverLogCategory::SECURITY: return "SECURITY";
        case DriverLogCategory::PROTOCOL_SPECIFIC: return "PROTOCOL_SPECIFIC";
        case DriverLogCategory::DIAGNOSTICS: return "DIAGNOSTICS";
        case DriverLogCategory::MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN";
    }
}

std::string LogManager::dataQualityToString(DataQuality quality) {
    switch (quality) {
        case DataQuality::GOOD: return "GOOD";
        case DataQuality::UNCERTAIN: return "UNCERTAIN";
        case DataQuality::BAD: return "BAD";
        case DataQuality::NOT_CONNECTED: return "NOT_CONNECTED";
        default: return "UNKNOWN";
    }
}