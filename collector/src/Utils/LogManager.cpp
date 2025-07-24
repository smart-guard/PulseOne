#include "Utils/LogManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

namespace PulseOne {

LogManager::LogManager() {
    // 🆕 카테고리별 기본 로그 레벨 설정
    categoryLevels_[DriverLogCategory::GENERAL] = PulseOne::LogLevel::INFO;
    categoryLevels_[DriverLogCategory::CONNECTION] = PulseOne::LogLevel::INFO;
    categoryLevels_[DriverLogCategory::COMMUNICATION] = PulseOne::LogLevel::WARN;
    categoryLevels_[DriverLogCategory::DATA_PROCESSING] = PulseOne::LogLevel::INFO;
    categoryLevels_[DriverLogCategory::ERROR_HANDLING] = PulseOne::LogLevel::ERROR;
    categoryLevels_[DriverLogCategory::PERFORMANCE] = PulseOne::LogLevel::WARN;
    categoryLevels_[DriverLogCategory::SECURITY] = PulseOne::LogLevel::WARN;
    categoryLevels_[DriverLogCategory::PROTOCOL_SPECIFIC] = PulseOne::LogLevel::DEBUG_LEVEL;
    categoryLevels_[DriverLogCategory::DIAGNOSTICS] = PulseOne::LogLevel::DEBUG_LEVEL;
}

LogManager::~LogManager() {
    flushAll();
}

LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

// =============================================================================
// 시간 관련 유틸리티 (기존 + 확장)
// =============================================================================

std::string LogManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y%m%d");
    return oss.str();
}

std::string LogManager::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    return oss.str();
}

std::string LogManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return TimestampToISOString(now);
}

// =============================================================================
// 로그 경로 및 파일 관리 (기존 + 확장)
// =============================================================================

std::string LogManager::buildLogPath(const std::string& category) {
    std::string date = getCurrentDate();
    std::string baseDir;

    if (category.rfind("packet_", 0) == 0) {
        baseDir = "logs/packets/" + date + "/" + category.substr(7);
    } else if (category == "maintenance") {
        baseDir = "logs/maintenance/" + date;
    } else {
        baseDir = "logs/" + date;
    }

    fs::path fullPath = (category.rfind("packet_", 0) == 0)
        ? fs::path(baseDir + ".log")
        : fs::path(baseDir + "/" + category + ".log");

    if (!fs::exists(fullPath.parent_path())) {
        fs::create_directories(fullPath.parent_path());
    }

    return fullPath.string();
}

// =============================================================================
// 로그 레벨 검사 (기존 + 카테고리 지원)
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
// 메시지 포맷팅 (🆕)
// =============================================================================

std::string LogManager::formatLogMessage(LogLevel level, const std::string& category,
                                       const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "]"
        << "[" << LogLevelToString(level) << "]";
    
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
// 파일 쓰기 및 통계 (기존 + 통계 추가)
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
    stream.flush();  // 🆕 즉시 플러시
}

void LogManager::updateStatistics(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (level) {
        case PulseOne::LogLevel::TRACE: statistics_.debug_count++; break;  // TRACE를 debug로 카운트
        case PulseOne::LogLevel::DEBUG_LEVEL: statistics_.debug_count++; break;
        case PulseOne::LogLevel::INFO: statistics_.info_count++; break;
        case PulseOne::LogLevel::WARN: statistics_.warning_count++; break;
        case PulseOne::LogLevel::ERROR: statistics_.error_count++; break;
        case PulseOne::LogLevel::FATAL: statistics_.error_count++; break;
        case PulseOne::LogLevel::MAINTENANCE: statistics_.maintenance_count++; break;
    }
}

// =============================================================================
// 기본 로그 메소드들 (기존 + 신규)
// =============================================================================

void LogManager::Info(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::INFO, message);
}

void LogManager::Warn(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::WARN, message);
}

void LogManager::Error(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::ERROR, message);
}

void LogManager::Fatal(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::FATAL, message);
}

void LogManager::Debug(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::DEBUG_LEVEL, message);
}

void LogManager::Trace(const std::string& message) {
    log(defaultCategory_, PulseOne::LogLevel::TRACE, message);
}

void LogManager::Maintenance(const std::string& message) {
    if (maintenance_mode_enabled_) {
        log("maintenance", PulseOne::LogLevel::MAINTENANCE, message);
    }
}

// =============================================================================
// 확장 로그 메소드들 (기존 + 신규)
// =============================================================================

void LogManager::log(const std::string& category, LogLevel level, const std::string& message) {
    if (!shouldLog(level)) return;
    
    updateStatistics(level);
    std::string formatted = formatLogMessage(level, category, message);
    std::string path = buildLogPath(category);
    writeToFile(path, formatted);
}

void LogManager::log(const std::string& category, const std::string& level, const std::string& message) {
    log(category, StringToLogLevel(level), message);
}

// 🆕 점검 관련 로그 메소드들
void LogManager::logMaintenance(const UUID& device_id, const EngineerID& engineer_id, 
                               const std::string& message) {
    std::string formatted = formatMaintenanceLog(device_id, engineer_id, message);
    std::string path = buildLogPath("maintenance");
    writeToFile(path, formatted);
}

void LogManager::logMaintenanceStart(const DeviceInfo& device, const EngineerID& engineer_id) {
    std::ostringstream oss;
    oss << "MAINTENANCE STARTED - Device: " << device.name 
        << " (" << device.id << "), Protocol: " << ProtocolTypeToString(device.protocol);
    logMaintenance(device.id, engineer_id, oss.str());
}

void LogManager::logMaintenanceEnd(const DeviceInfo& device, const EngineerID& engineer_id) {
    std::ostringstream oss;
    oss << "MAINTENANCE COMPLETED - Device: " << device.name 
        << " (" << device.id << ")";
    logMaintenance(device.id, engineer_id, oss.str());
}

void LogManager::logRemoteControlBlocked(const UUID& device_id, const std::string& reason) {
    std::ostringstream oss;
    oss << "REMOTE CONTROL BLOCKED - Device: " << device_id << ", Reason: " << reason;
    log("security", PulseOne::LogLevel::WARN, oss.str());
}

// 🆕 드라이버 로그 (카테고리 지원)
void LogManager::logDriver(const UUID& device_id, DriverLogCategory category, 
                          LogLevel level, const std::string& message) {
    if (!shouldLogCategory(category, level)) return;
    
    std::ostringstream oss;
    oss << "[Device:" << device_id << "][" << DriverLogCategoryToString(category) << "] " << message;
    
    std::string categoryName = "driver_" + DriverLogCategoryToString(category);
    log(categoryName, level, oss.str());
}

// 🆕 데이터 품질 로그
void LogManager::logDataQuality(const UUID& device_id, const UUID& point_id,
                               DataQuality quality, const std::string& reason) {
    if (quality == DataQuality::GOOD) return;  // 정상 데이터는 로깅 안함
    
    std::ostringstream oss;
    oss << "DATA QUALITY ISSUE - Device: " << device_id 
        << ", Point: " << point_id 
        << ", Quality: " << DataQualityToString(quality);
    
    if (!reason.empty()) {
        oss << ", Reason: " << reason;
    }
    
    LogLevel level = (quality == DataQuality::BAD) ? PulseOne::LogLevel::ERROR : PulseOne::LogLevel::WARN;
    log("data_quality", level, oss.str());
}

// 기존 특수 로그 메소드들 (유지)
void LogManager::logDriver(const std::string& driverName, const std::string& message) {
    log("driver_" + driverName, PulseOne::LogLevel::INFO, message);
}

void LogManager::logError(const std::string& message) {
    log("error", PulseOne::LogLevel::ERROR, message);
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
// 카테고리별 로그 레벨 관리 (🆕)
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
// 통계 및 관리 (🆕)
// =============================================================================

LogStatistics LogManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void LogManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_.total_logs = 0;
    statistics_.error_count = 0;
    statistics_.warning_count = 0;
    statistics_.info_count = 0;
    statistics_.debug_count = 0;
    statistics_.trace_count = 0;
    statistics_.maintenance_count = 0;
    statistics_.last_reset_time = PulseOne::Utils::GetCurrentTimestamp();
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
    
    // TODO: 오래된 로그 파일 삭제/압축 로직 추가
    Info("Log rotation completed");
}

} // namespace PulseOne