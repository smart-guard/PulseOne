#include "Logging/LogManager.h"
#include "Common/Utils.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <iostream>

// LogLib의 타입을 PulseOne 타입으로 변환하거나 그 반대로 변환하는 헬퍼
static LogLib::LogLevel MapToLogLibLevel(PulseOne::Enums::LogLevel level) {
  switch (level) {
  case PulseOne::Enums::LogLevel::TRACE:
    return LogLib::LogLevel::TRACE;
  case PulseOne::Enums::LogLevel::DEBUG:
    return LogLib::LogLevel::DEBUG;
  case PulseOne::Enums::LogLevel::INFO:
    return LogLib::LogLevel::INFO;
  case PulseOne::Enums::LogLevel::WARN:
    return LogLib::LogLevel::WARN;
  case PulseOne::Enums::LogLevel::LOG_ERROR:
    return LogLib::LogLevel::LOG_ERROR;
  case PulseOne::Enums::LogLevel::LOG_FATAL:
    return LogLib::LogLevel::LOG_FATAL;
  case PulseOne::Enums::LogLevel::MAINTENANCE:
    return LogLib::LogLevel::MAINTENANCE;
  case PulseOne::Enums::LogLevel::OFF:
    return LogLib::LogLevel::OFF;
  default:
    return LogLib::LogLevel::INFO;
  }
}

LogManager::LogManager()
    : initialized_(false), maintenance_mode_enabled_(false) {}

void LogManager::ensureInitialized() {
  if (initialized_.load(std::memory_order_acquire))
    return;

  static thread_local bool in_log_init = false;
  if (in_log_init)
    return; // 재진입 방지

  std::lock_guard<std::recursive_mutex> lock(init_mutex_);
  if (initialized_.load(std::memory_order_relaxed))
    return;

  in_log_init = true;
  doInitialize();
  in_log_init = false;

  initialized_.store(true, std::memory_order_release);
}

bool LogManager::doInitialize() {
  loadLogSettingsFromConfig();
  return true;
}

void LogManager::loadLogSettingsFromConfig() {
  try {
    auto &config = ConfigManager::getInstance();
    auto &engine = LogLib::LoggerEngine::getInstance();

    // LOG_LEVEL
    std::string level_str = config.getOrDefault("LOG_LEVEL", "INFO");
    std::transform(level_str.begin(), level_str.end(), level_str.begin(),
                   ::toupper);

    if (level_str == "TRACE")
      engine.setLogLevel(LogLib::LogLevel::TRACE);
    else if (level_str == "DEBUG")
      engine.setLogLevel(LogLib::LogLevel::DEBUG);
    else if (level_str == "INFO")
      engine.setLogLevel(LogLib::LogLevel::INFO);
    else if (level_str == "WARN" || level_str == "WARNING")
      engine.setLogLevel(LogLib::LogLevel::WARN);
    else if (level_str == "ERROR")
      engine.setLogLevel(LogLib::LogLevel::LOG_ERROR);
    else if (level_str == "FATAL")
      engine.setLogLevel(LogLib::LogLevel::LOG_FATAL);

    // Settings
    engine.setConsoleOutput(config.getOrDefault("LOG_TO_CONSOLE", "true") ==
                            "true");
    engine.setFileOutput(config.getOrDefault("LOG_TO_FILE", "true") == "true");
    engine.setLogBasePath(config.getOrDefault("LOG_FILE_PATH", "./logs/"));
    engine.setMaxLogSizeMB(
        std::stoul(config.getOrDefault("LOG_MAX_SIZE_MB", "100")));
    engine.setMaxLogFiles(
        std::stoi(config.getOrDefault("LOG_MAX_FILES", "30")));

    maintenance_mode_enabled_ =
        (config.getOrDefault("MAINTENANCE_MODE", "false") == "true");

  } catch (...) {
    // Fallback to defaults
    LogLib::LoggerEngine::getInstance().setLogBasePath("./logs/");
  }
}

void LogManager::Info(const std::string &message) {
  log("", LogLevel::INFO, message);
}
void LogManager::Warn(const std::string &message) {
  log("", LogLevel::WARN, message);
}
void LogManager::Error(const std::string &message) {
  log("", LogLevel::LOG_ERROR, message);
}
void LogManager::Fatal(const std::string &message) {
  log("", LogLevel::LOG_FATAL, message);
}
void LogManager::Debug(const std::string &message) {
  log("", LogLevel::DEBUG, message);
}
void LogManager::Trace(const std::string &message) {
  log("", LogLevel::TRACE, message);
}
void LogManager::Maintenance(const std::string &message) {
  if (maintenance_mode_enabled_) {
    log("maintenance", LogLevel::MAINTENANCE, message);
  }
}

void LogManager::log(const std::string &category, LogLevel level,
                     const std::string &message) {
  LogLib::LoggerEngine::getInstance().log(category, MapToLogLibLevel(level),
                                          message);
}

void LogManager::log(const std::string &category, const std::string &level,
                     const std::string &message) {
  // string to enum mapping (simplified)
  LogLevel lv = LogLevel::INFO;
  if (level == "TRACE")
    lv = LogLevel::TRACE;
  else if (level == "DEBUG")
    lv = LogLevel::DEBUG;
  else if (level == "WARN")
    lv = LogLevel::WARN;
  else if (level == "ERROR")
    lv = LogLevel::LOG_ERROR;
  else if (level == "FATAL")
    lv = LogLevel::LOG_FATAL;

  log(category, lv, message);
}

void LogManager::logMaintenance(const UniqueId &device_id,
                                const EngineerID &engineer_id,
                                const std::string &message) {
  std::ostringstream oss;
  oss << "[Device:" << device_id << "][Engineer:" << engineer_id << "] "
      << message;
  log("maintenance", LogLevel::MAINTENANCE, oss.str());
}

void LogManager::logMaintenanceStart(
    const PulseOne::Structs::DeviceInfo &device,
    const EngineerID &engineer_id) {
  logMaintenance(device.id, engineer_id,
                 "MAINTENANCE START - Device: " + device.name);
}

void LogManager::logMaintenanceEnd(const PulseOne::Structs::DeviceInfo &device,
                                   const EngineerID &engineer_id) {
  logMaintenance(device.id, engineer_id,
                 "MAINTENANCE COMPLETED - Device: " + device.name);
}

void LogManager::logRemoteControlBlocked(const UniqueId &device_id,
                                         const std::string &reason) {
  log("security", LogLevel::WARN,
      "REMOTE CONTROL BLOCKED - Device: " + device_id + ", Reason: " + reason);
}

void LogManager::logDriver(const std::string &driverName,
                           const std::string &message) {
  log("driver_" + driverName, LogLevel::INFO, message);
}

void LogManager::logDriver(const UniqueId &device_id,
                           DriverLogCategory category, LogLevel level,
                           const std::string &message) {
  std::lock_guard<std::recursive_mutex> lock(init_mutex_);
  auto it = categoryLevels_.find(category);
  LogLevel catLevel =
      (it != categoryLevels_.end()) ? it->second : LogLevel::INFO;

  if (static_cast<int>(level) < static_cast<int>(catLevel))
    return;

  std::ostringstream oss;
  oss << "[Device:" << device_id << "] " << message;
  log("driver", level, oss.str());
}

void LogManager::logDataQuality(const UniqueId &device_id,
                                const UniqueId &point_id, DataQuality quality,
                                const std::string &reason) {
  if (quality == DataQuality::GOOD)
    return;
  std::ostringstream oss;
  oss << "DATA QUALITY ISSUE - Device: " << device_id << ", Point: " << point_id
      << ", Reason: " << reason;
  log("data_quality",
      (quality == DataQuality::BAD ? LogLevel::LOG_ERROR : LogLevel::WARN),
      oss.str());
}

void LogManager::logError(const std::string &message) {
  log("error", LogLevel::LOG_ERROR, message);
}
void LogManager::logPacket(const std::string &driver, const std::string &device,
                           const std::string &rawPacket,
                           const std::string &decoded) {
  LogLib::LoggerEngine::getInstance().logPacket(driver, device, rawPacket,
                                                decoded);
}

void LogManager::setLogLevel(LogLevel level) {
  LogLib::LoggerEngine::getInstance().setLogLevel(MapToLogLibLevel(level));
}
LogLevel LogManager::getLogLevel() const {
  auto libLevel = LogLib::LoggerEngine::getInstance().getLogLevel();
  // Reverse mapping if needed, but usually we just return what we have in
  // minLevel_ or just return from libLevel cast
  return static_cast<LogLevel>(libLevel);
}

void LogManager::setCategoryLogLevel(DriverLogCategory category,
                                     LogLevel level) {
  std::lock_guard<std::recursive_mutex> lock(init_mutex_);
  categoryLevels_[category] = level;
}

LogLevel LogManager::getCategoryLogLevel(DriverLogCategory category) const {
  std::lock_guard<std::recursive_mutex> lock(init_mutex_);
  auto it = categoryLevels_.find(category);
  return (it != categoryLevels_.end()) ? it->second : LogLevel::INFO;
}

void LogManager::reloadSettings() { loadLogSettingsFromConfig(); }
void LogManager::setConsoleOutput(bool enabled) {
  LogLib::LoggerEngine::getInstance().setConsoleOutput(enabled);
}
void LogManager::setFileOutput(bool enabled) {
  LogLib::LoggerEngine::getInstance().setFileOutput(enabled);
}
void LogManager::setLogBasePath(const std::string &path) {
  LogLib::LoggerEngine::getInstance().setLogBasePath(path);
}
void LogManager::setMaxLogSizeMB(size_t size_mb) {
  LogLib::LoggerEngine::getInstance().setMaxLogSizeMB(size_mb);
}
void LogManager::setMaxLogFiles(int count) {
  LogLib::LoggerEngine::getInstance().setMaxLogFiles(count);
}

void LogManager::setMaintenanceMode(bool enabled) {
  maintenance_mode_enabled_ = enabled;
}
bool LogManager::isMaintenanceModeEnabled() const {
  return maintenance_mode_enabled_;
}

PulseOne::Structs::LogStatistics LogManager::getStatistics() const {
  auto libStats = LogLib::LoggerEngine::getInstance().getStatistics();
  PulseOne::Structs::LogStatistics stats;
  stats.total_logs = libStats.total_logs;
  stats.trace_count = libStats.trace_count;
  stats.debug_count = libStats.debug_count;
  stats.info_count = libStats.info_count;
  stats.warn_count = libStats.warn_count;
  stats.error_count = libStats.error_count;
  stats.fatal_count = libStats.fatal_count;
  stats.maintenance_count = libStats.maintenance_count;
  stats.last_log_time = libStats.last_log_time;
  return stats;
}

void LogManager::resetStatistics() {
  LogLib::LoggerEngine::getInstance().resetStatistics();
}
void LogManager::flushAll() { LogLib::LoggerEngine::getInstance().flushAll(); }
void LogManager::rotateLogs() {
  LogLib::LoggerEngine::getInstance().rotateLogs();
}

void LogManager::cleanupOldLogs(int retentionDays) {
  LogLib::LoggerEngine::getInstance().cleanupOldLogs(retentionDays);
}

size_t LogManager::getAvailableDiskSpaceMB() const {
  return LogLib::LoggerEngine::getInstance().getAvailableDiskSpaceMB();
}

void LogManager::emergencyCleanupLogs(size_t targetFreeMB) {
  LogLib::LoggerEngine::getInstance().emergencyCleanupLogs(targetFreeMB);
}