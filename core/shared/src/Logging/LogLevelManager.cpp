// =============================================================================
// collector/src/Utils/LogLevelManager.cpp - 컴파일 에러 수정 완성본
// =============================================================================

#include "Logging/LogLevelManager.h"
#include "Common/Utils.h"
#include "Database/RuntimeSQLQueries.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

// SQL 네임스페이스 단축 alias
using namespace PulseOne::Database::SQL;

// =============================================================================
// 핵심: 실제 초기화 로직 (thread-safe)
// =============================================================================
void LogLevelManager::ensureInitialized() {
  if (initialized_.load(std::memory_order_acquire)) {
    return;
  }

  std::lock_guard<std::mutex> lock(init_mutex_);
  if (initialized_.load(std::memory_order_relaxed)) {
    return;
  }

  doInitialize(nullptr, nullptr);
  initialized_.store(true, std::memory_order_release);
}

LogLevelManager::LogLevelManager()
    : initialized_(false), current_level_(LogLevel::INFO),
      maintenance_level_(LogLevel::TRACE), config_(nullptr),
      db_manager_(nullptr), running_(false), maintenance_mode_(false),
      level_change_count_(0), db_check_count_(0), file_check_count_(0) {}

LogLevelManager::~LogLevelManager() { Shutdown(); }

bool LogLevelManager::doInitialize() {
  if (initialized_.load()) {
    return true;
  }

  std::lock_guard<std::mutex> lock(category_mutex_);

  if (initialized_.load()) {
    return true;
  }

  try {
    config_ = &ConfigManager::getInstance();
    db_manager_ = &DbLib::DatabaseManager::getInstance();

    category_levels_[DriverLogCategory::GENERAL] = LogLevel::INFO;
    category_levels_[DriverLogCategory::CONNECTION] = LogLevel::INFO;
    category_levels_[DriverLogCategory::COMMUNICATION] = LogLevel::WARN;
    category_levels_[DriverLogCategory::DATA_PROCESSING] = LogLevel::INFO;
    category_levels_[DriverLogCategory::ERROR_HANDLING] = LogLevel::LOG_ERROR;
    category_levels_[DriverLogCategory::PERFORMANCE] = LogLevel::WARN;
    category_levels_[DriverLogCategory::SECURITY] = LogLevel::WARN;
    category_levels_[DriverLogCategory::PROTOCOL_SPECIFIC] =
        LogLevel::DEBUG_LEVEL;
    category_levels_[DriverLogCategory::DIAGNOSTICS] = LogLevel::DEBUG_LEVEL;

    LogLevel level = LoadLogLevelFromDB();
    if (level == LogLevel::INFO) {
      LogLevel file_level = LoadLogLevelFromFile();
      if (file_level != LogLevel::INFO) {
        level = file_level;
      }
    }

    current_level_ = level;
    LogManager::getInstance().setLogLevel(level);
    LoadCategoryLevelsFromDB();
    StartMonitoring();

    initialized_.store(true);
    return true;

  } catch (const std::exception &e) {
    return false;
  }
}

bool LogLevelManager::doInitialize(ConfigManager *config,
                                   DbLib::DatabaseManager *db) {
  if (initialized_.load()) {
    return true;
  }

  std::lock_guard<std::mutex> lock(category_mutex_);

  if (initialized_.load()) {
    return true;
  }

  try {
    config_ = config;
    if (db) {
      db_manager_ = db;
    } else {
      db_manager_ = &DbLib::DatabaseManager::getInstance();
    }

    LogManager::getInstance().Info("LogLevelManager initializing...");

    LogLevel level = LoadLogLevelFromDB();
    if (level == LogLevel::INFO) {
      LogLevel file_level = LoadLogLevelFromFile();
      if (file_level != LogLevel::INFO) {
        level = file_level;
      }
    }

    SetLogLevel(level, LogLevelSource::FILE_CONFIG, "SYSTEM", "Initial load");
    LoadCategoryLevelsFromDB();
    StartMonitoring();

    initialized_.store(true);

    LogManager::getInstance().Info("LogLevelManager initialized successfully");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("LogLevelManager 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

void LogLevelManager::Shutdown() {
  if (running_) {
    LogManager::getInstance().Info("LogLevelManager shutting down...");
    StopMonitoring();

    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      change_callbacks_.clear();
    }

    LogManager::getInstance().Info("LogLevelManager shutdown completed");
  }
}

void LogLevelManager::SetLogLevel(LogLevel level, LogLevelSource source,
                                  const EngineerID &changed_by,
                                  const std::string &reason) {
  LogLevel old_level = current_level_;

  if (old_level != level) {
    current_level_ = level;
    level_change_count_++;

    LogManager::getInstance().setLogLevel(level);

    LogLevelChangeEvent event;
    event.old_level = old_level;
    event.new_level = level;
    event.source = source;
    event.changed_by = changed_by;
    event.reason = reason;
    event.is_maintenance_related =
        (source == LogLevelSource::MAINTENANCE_OVERRIDE);

    NotifyLevelChange(event);
    AddToHistory(event);

    if (source != LogLevelSource::FILE_CONFIG &&
        source != LogLevelSource::COMMAND_LINE) {
      SaveLogLevelToDB(level, source, changed_by, reason);
    }

    LogManager::getInstance().Info(
        "Log level changed: " + PulseOne::Utils::LogLevelToString(old_level) +
        " -> " + PulseOne::Utils::LogLevelToString(level) + " (Source: " +
        LogLevelSourceToString(source) + ", By: " + changed_by + ")");
  }
}

void LogLevelManager::SetCategoryLogLevel(DriverLogCategory category,
                                          LogLevel level) {
  {
    std::lock_guard<std::mutex> lock(category_mutex_);
    category_levels_[category] = level;
  }

  LogManager::getInstance().setCategoryLogLevel(category, level);

  LogManager::getInstance().Info(
      "Category log level set: " +
      PulseOne::Utils::DriverLogCategoryToString(category) + " = " +
      PulseOne::Utils::LogLevelToString(level));
}

LogLevel
LogLevelManager::GetCategoryLogLevel(DriverLogCategory category) const {
  std::lock_guard<std::mutex> lock(category_mutex_);
  auto it = category_levels_.find(category);
  return (it != category_levels_.end()) ? it->second : current_level_;
}

void LogLevelManager::ResetCategoryLogLevels() {
  std::lock_guard<std::mutex> lock(category_mutex_);
  category_levels_.clear();

  LogManager::getInstance().Info("All category log levels reset to default");
}

// 수정된 SetMaintenanceMode 메소드 (템플릿 에러 해결)
void LogLevelManager::SetMaintenanceMode(bool enabled,
                                         LogLevel maintenance_level,
                                         const EngineerID &engineer_id) {
  bool was_enabled = maintenance_mode_.load();
  maintenance_mode_.store(enabled);

  if (enabled && !was_enabled) {
    maintenance_level_ = maintenance_level;
    SetLogLevel(maintenance_level, LogLevelSource::MAINTENANCE_OVERRIDE,
                engineer_id, "Maintenance mode activated");

    // 수정: 템플릿 방식 대신 문자열 연결 사용
    LogManager::getInstance().Maintenance(
        "Maintenance mode STARTED by engineer: " + engineer_id);

  } else if (!enabled && was_enabled) {
    LogLevel restore_level = LoadLogLevelFromFile();
    if (restore_level == LogLevel::INFO) {
      restore_level = LoadLogLevelFromDB();
    }

    SetLogLevel(restore_level, LogLevelSource::MAINTENANCE_OVERRIDE,
                engineer_id, "Maintenance mode deactivated");

    LogManager::getInstance().Maintenance(
        "Maintenance mode ENDED by engineer: " + engineer_id);
  }
}

bool LogLevelManager::UpdateLogLevelInDB(LogLevel level,
                                         const EngineerID &changed_by,
                                         const std::string &reason) {
  if (!db_manager_) {
    LogManager::getInstance().Warn("Cannot update log level: DB not available");
    return false;
  }

  try {
    bool success =
        SaveLogLevelToDB(level, LogLevelSource::WEB_API, changed_by, reason);

    if (success) {
      SetLogLevel(level, LogLevelSource::WEB_API, changed_by, reason);
      LogManager::getInstance().Info("Log level updated via Web API: " +
                                     PulseOne::Utils::LogLevelToString(level) +
                                     " by " + changed_by);
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to update log level in DB: " +
                                    std::string(e.what()));
    return false;
  }
}

bool LogLevelManager::UpdateCategoryLogLevelInDB(DriverLogCategory category,
                                                 LogLevel level,
                                                 const EngineerID &changed_by) {
  if (!db_manager_)
    return false;

  try {
    bool success = SaveCategoryLevelToDB(category, level, changed_by);

    if (success) {
      SetCategoryLogLevel(category, level);
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error(
        "Failed to update category log level in DB: " + std::string(e.what()));
    return false;
  }
}

// 수정된 StartMaintenanceModeFromWeb (PostgreSQL → SQLite)
bool LogLevelManager::StartMaintenanceModeFromWeb(const EngineerID &engineer_id,
                                                  LogLevel maintenance_level) {
  if (engineer_id.empty() || !PulseOne::Utils::IsValidEngineerID(engineer_id)) {
    LogManager::getInstance().Error(
        "Invalid engineer ID for maintenance mode: " + engineer_id);
    return false;
  }

  SetMaintenanceMode(true, maintenance_level, engineer_id);

  if (db_manager_) {
    try {
      // ✅ RuntimeSQLQueries::MaintenanceLog::INSERT_ENGINEER_START
      db_manager_->executeNonQuery(
          Runtime::MaintenanceLog::INSERT_ENGINEER_START(
              engineer_id,
              PulseOne::Utils::LogLevelToString(maintenance_level)));
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("Failed to log maintenance start to DB: " +
                                     std::string(e.what()));
    }
  }

  return true;
}

// 수정된 EndMaintenanceModeFromWeb (PostgreSQL → SQLite)
bool LogLevelManager::EndMaintenanceModeFromWeb(const EngineerID &engineer_id) {
  if (!maintenance_mode_.load()) {
    LogManager::getInstance().Warn(
        "Cannot end maintenance mode: not currently in maintenance mode");
    return false;
  }

  SetMaintenanceMode(false, LogLevel::INFO, engineer_id);

  if (db_manager_) {
    try {
      // ✅ RuntimeSQLQueries::MaintenanceLog::INSERT_ENGINEER_END
      db_manager_->executeNonQuery(
          Runtime::MaintenanceLog::INSERT_ENGINEER_END(engineer_id));
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("Failed to log maintenance end to DB: " +
                                     std::string(e.what()));
    }
  }

  return true;
}

void LogLevelManager::StartMonitoring() {
  if (running_.load())
    return;

  running_.store(true);
  monitor_thread_ = std::thread([this]() {
    LogManager::getInstance().Debug("LogLevelManager monitoring started");
    MonitoringLoop();
  });
}

void LogLevelManager::StopMonitoring() {
  if (!running_.load())
    return;

  running_.store(false);
  if (monitor_thread_.joinable()) {
    monitor_thread_.join();
  }

  LogManager::getInstance().Debug("LogLevelManager monitoring stopped");
}

void LogLevelManager::MonitoringLoop() {
  while (running_.load()) {
    try {
      CheckDatabaseChanges();
      CheckFileChanges();

      std::this_thread::sleep_for(std::chrono::seconds(10));

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("LogLevelManager monitoring error: " +
                                      std::string(e.what()));
      std::this_thread::sleep_for(std::chrono::seconds(30));
    }
  }
}

void LogLevelManager::CheckDatabaseChanges() {
  auto now = std::chrono::steady_clock::now();

  if (now - last_db_check_ > std::chrono::seconds(30)) {
    db_check_count_++;

    LogLevel new_level = LoadLogLevelFromDB();
    if (new_level != current_level_ && !maintenance_mode_.load()) {
      SetLogLevel(new_level, LogLevelSource::DATABASE, "SYSTEM",
                  "Database change detected");
    }

    last_db_check_ = now;
  }
}

void LogLevelManager::CheckFileChanges() {
  auto now = std::chrono::steady_clock::now();

  if (now - last_file_check_ > std::chrono::seconds(60)) {
    file_check_count_++;

    LogLevel new_level = LoadLogLevelFromFile();
    if (new_level != current_level_ && !maintenance_mode_.load()) {
      SetLogLevel(new_level, LogLevelSource::FILE_CONFIG, "SYSTEM",
                  "Config file change detected");
    }

    last_file_check_ = now;
  }
}

void LogLevelManager::RegisterChangeCallback(
    const LogLevelChangeCallback &callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  change_callbacks_.push_back(callback);
}

void LogLevelManager::UnregisterAllCallbacks() {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  change_callbacks_.clear();
}

void LogLevelManager::NotifyLevelChange(const LogLevelChangeEvent &event) {
  std::lock_guard<std::mutex> lock(callback_mutex_);

  for (const auto &callback : change_callbacks_) {
    try {
      callback(event.old_level, event.new_level);
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Error in log level change callback: " +
                                      std::string(e.what()));
    }
  }
}

void LogLevelManager::AddToHistory(const LogLevelChangeEvent &event) {
  std::lock_guard<std::mutex> lock(history_mutex_);

  change_history_.push_back(event);

  if (change_history_.size() > 1000) {
    change_history_.erase(change_history_.begin(),
                          change_history_.begin() + 100);
  }
}

std::vector<LogLevelChangeEvent>
LogLevelManager::GetChangeHistory(size_t max_count) const {
  std::lock_guard<std::mutex> lock(history_mutex_);

  if (change_history_.size() <= max_count) {
    return change_history_;
  }

  return std::vector<LogLevelChangeEvent>(change_history_.end() - max_count,
                                          change_history_.end());
}

void LogLevelManager::ClearChangeHistory() {
  std::lock_guard<std::mutex> lock(history_mutex_);
  change_history_.clear();
}

LogLevelManager::ManagerStatus LogLevelManager::GetStatus() const {
  ManagerStatus status;

  status.current_level = current_level_;
  status.maintenance_level = maintenance_level_;
  status.maintenance_mode = maintenance_mode_.load();
  status.monitoring_active = running_.load();
  status.db_check_count = db_check_count_.load();
  status.file_check_count = file_check_count_.load();
  status.level_change_count = level_change_count_.load();
  status.last_db_check =
      std::chrono::system_clock::now() -
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::steady_clock::now() - last_db_check_);
  status.last_file_check =
      std::chrono::system_clock::now() -
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::steady_clock::now() - last_file_check_);

  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    status.active_callbacks = change_callbacks_.size();
  }

  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    status.history_entries = change_history_.size();
  }

  return status;
}

void LogLevelManager::RunDiagnostics() {
  LogManager &logger = LogManager::getInstance();

  logger.Info("Running LogLevelManager diagnostics...");

  bool config_valid = ValidateConfiguration();
  logger.Info("Configuration valid: " +
              std::string(config_valid ? "YES" : "NO"));

  ManagerStatus status = GetStatus();
  logger.Info("Status: " + status.ToJson());

  if (db_manager_) {
    try {
      LogLevel test_level = LoadLogLevelFromDB();
      logger.Info("Database connection: OK (current level: " +
                  PulseOne::Utils::LogLevelToString(test_level) + ")");
    } catch (const std::exception &e) {
      logger.Error("Database connection: FAILED (" + std::string(e.what()) +
                   ")");
    }
  } else {
    logger.Warn("Database: NOT CONFIGURED");
  }

  if (config_) {
    try {
      LogLevel test_level = LoadLogLevelFromFile();
      logger.Info("Config file: OK (current level: " +
                  PulseOne::Utils::LogLevelToString(test_level) + ")");
    } catch (const std::exception &e) {
      logger.Error("Config file: FAILED (" + std::string(e.what()) + ")");
    }
  } else {
    logger.Warn("Config: NOT CONFIGURED");
  }

  logger.Info("LogLevelManager diagnostics completed");
}

bool LogLevelManager::ValidateConfiguration() const {
  if (current_level_ < LogLevel::TRACE ||
      current_level_ > LogLevel::MAINTENANCE) {
    return false;
  }

  if (maintenance_level_ < LogLevel::TRACE ||
      maintenance_level_ > LogLevel::MAINTENANCE) {
    return false;
  }

  std::lock_guard<std::mutex> lock(category_mutex_);
  for (const auto &pair : category_levels_) {
    if (pair.second < LogLevel::TRACE || pair.second > LogLevel::MAINTENANCE) {
      return false;
    }
  }

  return true;
}

LogLevel LogLevelManager::LoadLogLevelFromDB() {
  if (!db_manager_)
    return LogLevel::INFO;

  try {
    // ✅ RuntimeSQLQueries::MaintenanceLog::GET_DEFAULT_LOG_LEVEL
    std::vector<std::vector<std::string>> results;
    if (db_manager_->executeQuery(
            Runtime::MaintenanceLog::GET_DEFAULT_LOG_LEVEL, results) &&
        !results.empty() && !results[0].empty()) {
      return PulseOne::Utils::StringToLogLevel(results[0][0]);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Debug("Failed to load log level from DB: " +
                                    std::string(e.what()));
  }

  return LogLevel::INFO;
}

LogLevel LogLevelManager::LoadLogLevelFromFile() {
  if (!config_)
    return LogLevel::INFO;

  std::string level_str = config_->get("LOG_LEVEL");
  if (!level_str.empty()) {
    LogLevel level = PulseOne::Utils::StringToLogLevel(level_str);
    return level;
  }

  return LogLevel::INFO;
}

// ✅ SaveLogLevelToDB - RuntimeSQLQueries + 표준 executeNonQuery 사용
bool LogLevelManager::SaveLogLevelToDB(LogLevel level, LogLevelSource source,
                                       const EngineerID &changed_by,
                                       const std::string &reason) {
  if (!db_manager_)
    return false;

  try {
    // ✅ RuntimeSQLQueries::MaintenanceLog::UPSERT_DEFAULT_LOG_LEVEL
    bool success = db_manager_->executeNonQuery(
        Runtime::MaintenanceLog::UPSERT_DEFAULT_LOG_LEVEL(
            PulseOne::Utils::LogLevelToString(level), changed_by, reason));

    if (success) {
      // ✅ RuntimeSQLQueries::MaintenanceLog::INSERT_LEVEL_HISTORY
      db_manager_->executeNonQuery(
          Runtime::MaintenanceLog::INSERT_LEVEL_HISTORY(
              PulseOne::Utils::LogLevelToString(current_level_),
              PulseOne::Utils::LogLevelToString(level),
              LogLevelSourceToString(source), changed_by, reason));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to save log level to DB: " +
                                    std::string(e.what()));
    return false;
  }
}

// ✅ LoadCategoryLevelsFromDB - 표준 executeQuery + vector 처리
void LogLevelManager::LoadCategoryLevelsFromDB() {
  if (!db_manager_)
    return;

  try {
    // ✅ RuntimeSQLQueries::MaintenanceLog::GET_CATEGORY_LEVELS
    std::vector<std::vector<std::string>> results;
    if (db_manager_->executeQuery(
            Runtime::MaintenanceLog::GET_CATEGORY_LEVELS, results)) {
      for (const auto &row : results) {
        if (row.size() >= 2) {
          DriverLogCategory category =
              PulseOne::Utils::StringToDriverLogCategory(row[0]);
          LogLevel level = PulseOne::Utils::StringToLogLevel(row[1]);
          SetCategoryLogLevel(category, level);
        }
      }
    }

    LogManager::getInstance().Info("Loaded category log levels from DB");

  } catch (const std::exception &e) {
    LogManager::getInstance().Debug("Failed to load category levels from DB: " +
                                    std::string(e.what()));
  }
}

// ✅ SaveCategoryLevelToDB - RuntimeSQLQueries + 표준 executeNonQuery 사용
bool LogLevelManager::SaveCategoryLevelToDB(DriverLogCategory category,
                                            LogLevel level,
                                            const EngineerID &changed_by) {
  if (!db_manager_)
    return false;

  try {
    // ✅ RuntimeSQLQueries::MaintenanceLog::UPSERT_CATEGORY_LEVEL
    return db_manager_->executeNonQuery(
        Runtime::MaintenanceLog::UPSERT_CATEGORY_LEVEL(
            PulseOne::Utils::DriverLogCategoryToString(category),
            PulseOne::Utils::LogLevelToString(level), changed_by));

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to save category level to DB: " +
                                    std::string(e.what()));
    return false;
  }
}