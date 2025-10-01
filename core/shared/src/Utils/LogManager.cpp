/**
 * @file LogManager.cpp
 * @brief PulseOne 통합 로그 관리자 - 순환 의존성 완전 제거
 * @author PulseOne Development Team
 * @date 2025-09-29
 * 
 * 핵심 변경:
 * - ConfigManager 의존성 제거 (초기화 시)
 * - 직접 .env 파일 읽기
 * - 호출 순서 무관
 */

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>

// =============================================================================
// 생성자/소멸자
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
        
        loadLogSettingsFromConfig();
        
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
        
        createLogDirectoriesRecursive();
        
        std::cout << "LogManager 자동 초기화 완료\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "LogManager 초기화 실패: " << e.what() << "\n";
        return false;
    }
}

// =============================================================================
// 직접 .env 읽기
// =============================================================================

void LogManager::loadEnvFileDirectly() {
    // 1. 환경변수 확인
    const char* config_dir_env = std::getenv("PULSEONE_CONFIG_DIR");
    if (config_dir_env) {
        std::string env_path = std::string(config_dir_env) + "/.env";
        if (tryLoadEnvFile(env_path)) {
            return;
        }
    }
    
    // 2. 실행 파일 기준 경로 탐색
    std::string exe_dir = getExecutableDirectory();
    std::vector<std::string> search_paths = {
        exe_dir + "/config/.env",
        exe_dir + "/../config/.env",
        exe_dir + "/../../config/.env",
        "./config/.env",
        "../config/.env",
        "../../config/.env",
        "/app/config/.env"
    };
    
    for (const auto& path : search_paths) {
        if (tryLoadEnvFile(path)) {
            std::cout << "  .env 파일 로드: " << path << "\n";
            return;
        }
    }
    
    std::cout << "  .env 파일 없음, 기본값 사용\n";
}

bool LogManager::tryLoadEnvFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 트림
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // 따옴표 제거
            if (value.length() >= 2 && 
                ((value.front() == '"' && value.back() == '"') || 
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.length() - 2);
            }
            
            if (!key.empty()) {
                #ifdef _WIN32
                _putenv_s(key.c_str(), value.c_str());
                #else
                setenv(key.c_str(), value.c_str(), 0);
                #endif
            }
        }
    }
    file.close();
    return true;
}

std::string LogManager::getExecutableDirectory() {
    #ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exe_path(buffer);
    size_t pos = exe_path.find_last_of("\\/");
    return (pos != std::string::npos) ? exe_path.substr(0, pos) : ".";
    #else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string exe_path(buffer);
        size_t pos = exe_path.find_last_of('/');
        return (pos != std::string::npos) ? exe_path.substr(0, pos) : ".";
    }
    return ".";
    #endif
}

// =============================================================================
// 설정 로드
// =============================================================================

void LogManager::loadLogSettingsFromConfig() {
    try {
        const char* log_level_env = std::getenv("LOG_LEVEL");
        
        if (!log_level_env) {
            loadEnvFileDirectly();
            log_level_env = std::getenv("LOG_LEVEL");
        }
        
        if (log_level_env) {
            std::string level_str(log_level_env);
            std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::toupper);
            
            if (level_str == "DEBUG") minLevel_ = LogLevel::DEBUG;
            else if (level_str == "INFO") minLevel_ = LogLevel::INFO;
            else if (level_str == "WARN" || level_str == "WARNING") minLevel_ = LogLevel::WARN;
            else if (level_str == "ERROR") minLevel_ = LogLevel::LOG_ERROR;
            else if (level_str == "FATAL") minLevel_ = LogLevel::LOG_FATAL;
            else if (level_str == "TRACE") minLevel_ = LogLevel::TRACE;
            else minLevel_ = LogLevel::INFO;
            
            std::cout << "  LOG_LEVEL: " << level_str << "\n";
        } else {
            minLevel_ = LogLevel::INFO;
            std::cout << "  LOG_LEVEL: INFO (기본값)\n";
        }
        
        const char* console_log = std::getenv("LOG_TO_CONSOLE");
        if (console_log) {
            std::string val(console_log);
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            console_output_enabled_ = (val == "true" || val == "1" || val == "yes");
        } else {
            console_output_enabled_ = true;
        }
        
        const char* file_log = std::getenv("LOG_TO_FILE");
        if (file_log) {
            std::string val(file_log);
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            file_output_enabled_ = (val == "true" || val == "1" || val == "yes");
        } else {
            file_output_enabled_ = true;
        }
        
        const char* log_path = std::getenv("LOG_FILE_PATH");
        log_base_path_ = log_path ? normalizePath(std::string(log_path)) : "./logs/";
        
        const char* max_size = std::getenv("LOG_MAX_SIZE_MB");
        max_log_size_mb_ = max_size ? std::atoi(max_size) : 100;
        
        const char* max_files = std::getenv("LOG_MAX_FILES");
        max_log_files_ = max_files ? std::atoi(max_files) : 30;
        
        const char* maint_mode = std::getenv("MAINTENANCE_MODE");
        if (maint_mode) {
            std::string val(maint_mode);
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            maintenance_mode_enabled_ = (val == "true" || val == "1" || val == "yes");
        } else {
            maintenance_mode_enabled_ = false;
        }
        
        std::cout << "  LOG_TO_CONSOLE: " << (console_output_enabled_ ? "true" : "false") << "\n";
        std::cout << "  LOG_TO_FILE: " << (file_output_enabled_ ? "true" : "false") << "\n";
        std::cout << "  LOG_FILE_PATH: " << log_base_path_ << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "로그 설정 실패: " << e.what() << "\n";
        console_output_enabled_ = true;
        file_output_enabled_ = true;
        log_base_path_ = "./logs/";
        minLevel_ = LogLevel::INFO;
    }
}

// =============================================================================
// 경로 처리
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
// 시간 유틸리티
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
    
    if (console_output_enabled_) {
        std::cout << message << std::endl;
    }
    
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
                        std::cout << "로그 로테이션: " << filePath << " -> " << backup_path.string() << std::endl;
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
        case LogLevel::TRACE: statistics_.trace_count++; break;
        case LogLevel::DEBUG: statistics_.debug_count++; break;
        case LogLevel::INFO: statistics_.info_count++; break;
        case LogLevel::WARN: 
            statistics_.warn_count++;
            statistics_.warning_count++;
            break;
        case LogLevel::LOG_ERROR: statistics_.error_count++; break;
        case LogLevel::LOG_FATAL: statistics_.fatal_count++; break;
        case LogLevel::MAINTENANCE: statistics_.maintenance_count++; break;
        default: break;
    }

    statistics_.total_logs++;
    statistics_.last_log_time = std::chrono::system_clock::now();
}

// =============================================================================
// 기본 로그 메소드
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
// 확장 로그
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
// 점검 로그
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
// 드라이버 로그
// =============================================================================

void LogManager::logDriver(const UniqueId& device_id, DriverLogCategory category, 
                          LogLevel level, const std::string& message) {
    if (!shouldLogCategory(category, level)) return;
    
    std::ostringstream oss;
    oss << "[Device:" << device_id << "][" << driverLogCategoryToString(category) << "] " << message;
    
    std::string categoryName = "driver_" + driverLogCategoryToString(category);
    log(categoryName, level, oss.str());
}

void LogManager::logDriver(const std::string& driverName, const std::string& message) {
    log("driver_" + driverName, LogLevel::INFO, message);
}

// =============================================================================
// 데이터 품질 로그
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
// 카테고리 관리
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
// 설정 관리
// =============================================================================

void LogManager::reloadSettings() {
    std::lock_guard<std::mutex> lock(mutex_);
    loadLogSettingsFromConfig();
    if (console_output_enabled_) {
        std::cout << "로그 설정 재로드 완료" << std::endl;
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
            std::cerr << "로그 경로 변경 실패: " << e.what() << std::endl;
        }
    }
}

// =============================================================================
// 통계
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
// 유틸리티
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