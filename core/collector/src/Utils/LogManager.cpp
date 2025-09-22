/**
 * @file LogManager.cpp
 * @brief PulseOne 통합 로그 관리자 - LEGACY 코드 완전 제거 + UUID→UniqueId 변경
 * @author PulseOne Development Team
 * @date 2025-09-09
 * 
 * 완전한 기능:
 * - LOG_LEVEL, LOG_TO_CONSOLE, LOG_TO_FILE, LOG_FILE_PATH 설정 적용
 * - Windows/Linux 크로스 플랫폼 경로 처리 (std::filesystem만 사용)
 * - LEGACY 폴백 코드 완전 제거
 * - UUID → UniqueId 변경 완료
 */

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

// =============================================================================
// 생성자/소멸자 및 초기화
// =============================================================================

LogManager::LogManager() 
    : initialized_(false)
    , minLevel_(LogLevel::INFO)
    , defaultCategory_("system")
    , maintenance_mode_enabled_(false)
    , max_log_size_mb_(100)
    , max_log_files_(30)
    , console_output_enabled_(true)
    , file_output_enabled_(true)
    , log_base_path_("./logs/")
{
    // 생성자에서는 기본값만 설정
}

LogManager::~LogManager() {
    flushAll();
}

void LogManager::ensureInitialized() {
    if (initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_.load(std::memory_order_relaxed)) {
        return;
    }
    
    doInitialize();
    initialized_.store(true, std::memory_order_release);
}

bool LogManager::doInitialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::cout << "LogManager 자동 초기화 시작...\n";
        
        // 1. 설정 파일에서 로그 설정 로드
        loadLogSettingsFromConfig();
        
        // 2. 카테고리별 로그 레벨 설정
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
        
        // 3. 로그 디렉토리 생성
        createLogDirectoriesRecursive();
        
        std::cout << "LogManager 자동 초기화 완료\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "LogManager 초기화 실패: " << e.what() << "\n";
        return false;
    }
}

// =============================================================================
// 설정 파일에서 로그 설정 로드
// =============================================================================

void LogManager::loadLogSettingsFromConfig() {
    try {
        auto& config = ConfigManager::getInstance();
        
        // LOG_LEVEL 적용
        std::string log_level_str = config.getOrDefault("LOG_LEVEL", "INFO");
        std::transform(log_level_str.begin(), log_level_str.end(), log_level_str.begin(), ::toupper);
        
        if (log_level_str == "DEBUG") {
            minLevel_ = LogLevel::DEBUG;
        } else if (log_level_str == "INFO") {
            minLevel_ = LogLevel::INFO;
        } else if (log_level_str == "WARN" || log_level_str == "WARNING") {
            minLevel_ = LogLevel::WARN;
        } else if (log_level_str == "ERROR") {
            minLevel_ = LogLevel::LOG_ERROR;
        } else if (log_level_str == "FATAL") {
            minLevel_ = LogLevel::LOG_FATAL;
        } else if (log_level_str == "TRACE") {
            minLevel_ = LogLevel::TRACE;
        } else {
            minLevel_ = LogLevel::INFO;
        }
        
        // 출력 제어 설정
        console_output_enabled_ = config.getBool("LOG_TO_CONSOLE", true);
        file_output_enabled_ = config.getBool("LOG_TO_FILE", true);
        
        // 로그 경로 설정
        std::string raw_path = config.getOrDefault("LOG_FILE_PATH", "./logs/");
        log_base_path_ = normalizePath(raw_path);
        
        // 로그 로테이션 설정
        max_log_size_mb_ = static_cast<size_t>(config.getInt("LOG_MAX_SIZE_MB", 100));
        max_log_files_ = config.getInt("LOG_MAX_FILES", 30);
        
        // 유지보수 모드 설정
        maintenance_mode_enabled_ = config.getBool("MAINTENANCE_MODE", false);
        
        std::cout << "로그 설정 적용됨:\n";
        std::cout << "  - LOG_LEVEL: " << log_level_str << "\n";
        std::cout << "  - LOG_TO_CONSOLE: " << (console_output_enabled_ ? "true" : "false") << "\n";
        std::cout << "  - LOG_TO_FILE: " << (file_output_enabled_ ? "true" : "false") << "\n";
        std::cout << "  - LOG_FILE_PATH: " << log_base_path_ << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "로그 설정 로드 실패: " << e.what() << "\n";
        // 기본값으로 폴백
        console_output_enabled_ = true;
        file_output_enabled_ = true;
        log_base_path_ = "./logs/";
        minLevel_ = LogLevel::INFO;
    }
}

// =============================================================================
// 크로스 플랫폼 경로 처리 (std::filesystem만 사용)
// =============================================================================

std::string LogManager::normalizePath(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        std::filesystem::path normalized = fs_path.lexically_normal();
        
        std::string result = normalized.string();
        if (!result.empty() && result.back() != '/' && result.back() != '\\') {
            result += std::filesystem::path::preferred_separator;
        }
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "경로 정규화 실패: " << e.what() << "\n";
        return path;
    }
}

std::filesystem::path LogManager::buildLogDirectoryPath(const std::string& category) {
    std::filesystem::path base_path(log_base_path_);
    std::string date = getCurrentDate();
    
    if (category.rfind("packet_", 0) == 0) {
        return base_path / "packets" / date;
    } else if (category == "maintenance") {
        return base_path / "maintenance" / date;
    } else {
        return base_path / date;
    }
}

std::filesystem::path LogManager::buildLogFilePath(const std::string& category) {
    std::filesystem::path dir_path = buildLogDirectoryPath(category);
    
    if (category.rfind("packet_", 0) == 0) {
        std::string driver_name = category.substr(7);
        return dir_path / (driver_name + ".log");
    } else {
        return dir_path / (category + ".log");
    }
}

void LogManager::createLogDirectoriesRecursive() {
    try {
        std::filesystem::path base_path(log_base_path_);
        std::filesystem::create_directories(base_path);
        
        std::string today = getCurrentDate();
        std::filesystem::create_directories(base_path / today);
        std::filesystem::create_directories(base_path / "packets" / today);
        std::filesystem::create_directories(base_path / "maintenance" / today);
        
    } catch (const std::exception& e) {
        std::cerr << "로그 디렉토리 생성 실패: " << e.what() << "\n";
    }
}

std::string LogManager::buildLogPath(const std::string& category) {
    try {
        std::filesystem::path log_file_path = buildLogFilePath(category);
        std::filesystem::create_directories(log_file_path.parent_path());
        return log_file_path.string();
        
    } catch (const std::exception& e) {
        std::cerr << "로그 경로 빌드 실패: " << e.what() << "\n";
        return "./error.log";
    }
}

// =============================================================================
// 시간 관련 유틸리티
// =============================================================================

std::string LogManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    
    // PlatformCompat.h의 SAFE_LOCALTIME 사용
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

std::string LogManager::formatMaintenanceLog(const UniqueId& device_id, 
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
// 파일 쓰기
// =============================================================================

void LogManager::writeToFile(const std::string& filePath, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 콘솔 출력
    if (console_output_enabled_) {
        std::cout << message << std::endl;
    }
    
    // 파일 출력
    if (file_output_enabled_) {
        std::ofstream& stream = logFiles_[filePath];
        if (!stream.is_open()) {
            stream.open(filePath, std::ios::app);
            if (!stream.is_open()) {
                if (console_output_enabled_) {
                    std::cerr << "로그 파일 열기 실패: " << filePath << std::endl;
                }
                return;
            }
        }
        stream << message << std::endl;
        stream.flush();
        
        checkAndRotateLogFile(filePath, stream);
    }
}

void LogManager::checkAndRotateLogFile(const std::string& filePath, std::ofstream& stream) {
    try {
        if (stream.is_open()) {
            auto current_pos = stream.tellp();
            if (current_pos > 0) {
                size_t current_size_mb = static_cast<size_t>(current_pos) / (1024 * 1024);
                
                if (current_size_mb >= max_log_size_mb_) {
                    stream.close();
                    
                    std::filesystem::path original_path(filePath);
                    std::string backup_name = original_path.stem().string() + "_" + 
                                            getCurrentTimestamp() + original_path.extension().string();
                    std::filesystem::path backup_path = original_path.parent_path() / backup_name;
                    
                    std::filesystem::rename(original_path, backup_path);
                    stream.open(filePath, std::ios::app);
                    
                    if (console_output_enabled_) {
                        std::cout << "로그 로테이션 수행: " << filePath << " -> " << backup_path.string() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        if (console_output_enabled_) {
            std::cerr << "로그 로테이션 실패: " << e.what() << std::endl;
        }
    }
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
            statistics_.warning_count++;
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
// 점검 관련 로그 메소드들 (UUID → UniqueId 변경)
// =============================================================================

void LogManager::logMaintenance(const UniqueId& device_id, const EngineerID& engineer_id, 
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

void LogManager::logRemoteControlBlocked(const UniqueId& device_id, const std::string& reason) {
    std::ostringstream oss;
    oss << "REMOTE CONTROL BLOCKED - Device: " << device_id << ", Reason: " << reason;
    log("security", LogLevel::WARN, oss.str());
}

// =============================================================================
// 드라이버 로그 (UUID → UniqueId 변경)
// =============================================================================

void LogManager::logDriver(const UniqueId& device_id, DriverLogCategory category, 
                          LogLevel level, const std::string& message) {
    if (!shouldLogCategory(category, level)) return;
    
    std::ostringstream oss;
    oss << "[Device:" << device_id << "][" << driverLogCategoryToString(category) << "] " << message;
    
    std::string categoryName = "driver_" + driverLogCategoryToString(category);
    log(categoryName, level, oss.str());
}

// =============================================================================
// 데이터 품질 로그 (UUID → UniqueId 변경)
// =============================================================================

void LogManager::logDataQuality(const UniqueId& device_id, const UniqueId& point_id,
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
// 설정 재로드
// =============================================================================

void LogManager::reloadSettings() {
    std::lock_guard<std::mutex> lock(mutex_);
    loadLogSettingsFromConfig();
    if (console_output_enabled_) {
        std::cout << "로그 설정이 재로드되었습니다." << std::endl;
    }
}

void LogManager::setConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_output_enabled_ = enabled;
}

void LogManager::setFileOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_output_enabled_ = enabled;
}

void LogManager::setLogBasePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_base_path_ = normalizePath(path);
    
    try {
        createLogDirectoriesRecursive();
    } catch (const std::exception& e) {
        if (console_output_enabled_) {
            std::cerr << "새 로그 경로 디렉토리 생성 실패: " << e.what() << std::endl;
        }
    }
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
// 유틸리티 함수들
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