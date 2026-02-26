#include "LoggerEngine.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace LogLib {

LoggerEngine::LoggerEngine()
    : minLevel_(LogLevel::INFO), log_base_path_("./logs/"),
      console_output_enabled_(true), file_output_enabled_(true),
      max_log_size_mb_(100), max_log_files_(30) {
#ifdef _WIN32
  // Windows 콘솔에서 UTF-8 로그가 깨지지 않도록 강제 설정
  SetConsoleOutputCP(CP_UTF8);
#endif
}

LoggerEngine::~LoggerEngine() { flushAll(); }

LoggerEngine &LoggerEngine::getInstance() {
  static LoggerEngine instance;
  return instance;
}

void LoggerEngine::setLogLevel(LogLevel level) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  minLevel_ = level;
}

LogLevel LoggerEngine::getLogLevel() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (log_level_provider_) {
    // Callback exists, update minLevel periodically or every check
    // User is responsible for performance of the callback
    const_cast<LoggerEngine *>(this)->minLevel_ = log_level_provider_();
  }
  return minLevel_;
}

void LoggerEngine::setLogLevelProvider(LogLevelProvider provider) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  log_level_provider_ = provider;
}

void LoggerEngine::setLogBasePath(const std::string &path) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  log_base_path_ = path;
  if (!log_base_path_.empty() && log_base_path_.back() != '/' &&
      log_base_path_.back() != '\\') {
    log_base_path_ += std::filesystem::path::preferred_separator;
  }
}

std::string LoggerEngine::getLogBasePath() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return log_base_path_;
}

void LoggerEngine::setConsoleOutput(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  console_output_enabled_ = enabled;
}

void LoggerEngine::setFileOutput(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  file_output_enabled_ = enabled;
}

void LoggerEngine::setMaxLogSizeMB(size_t size_mb) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  max_log_size_mb_ = size_mb;
}

void LoggerEngine::setMaxLogFiles(int count) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  max_log_files_ = count;
}

void LoggerEngine::log(const std::string &category, LogLevel level,
                       const std::string &message) {
  if (static_cast<int>(level) < static_cast<int>(getLogLevel()))
    return;

  updateStatistics(level);

  std::ostringstream oss;
  oss << "[" << getCurrentTimestamp() << "]"
      << "[" << LogLevelToString(level) << "]";

  if (!category.empty()) {
    oss << "[" << category << "]";
  }

  oss << " " << message;
  std::string formatted = oss.str();

  std::string path = buildLogPath(category);
  writeToFile(path, formatted);
}

void LoggerEngine::logPacket(const std::string &driver,
                             const std::string &device,
                             const std::string &rawPacket,
                             const std::string &decoded) {
  std::ostringstream oss;
  oss << "[" << getCurrentTimestamp() << "]\n[RAW] " << rawPacket
      << "\n[DECODED] " << decoded;

  std::string category = "packet_" + driver + "/" + device;
  std::string path = buildLogPath(category);
  writeToFile(path, oss.str());
}

void LoggerEngine::flushAll() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto &kv : logFiles_) {
    if (kv.second.is_open()) {
      kv.second.flush();
      kv.second.close();
    }
  }
  logFiles_.clear();
}

void LoggerEngine::rotateLogs() { flushAll(); }

// =============================================================================
// Log Retention & Disk Management
// =============================================================================

void LoggerEngine::cleanupOldLogs(int retentionDays) {
  if (retentionDays <= 0)
    return;

  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    namespace fs = std::filesystem;
    auto cutoff = fs::file_time_type::clock::now() -
                  std::chrono::hours(24 * retentionDays);

    if (!fs::exists(log_base_path_))
      return;

    size_t deleted_count = 0;
    for (const auto &entry : fs::recursive_directory_iterator(
             log_base_path_, fs::directory_options::skip_permission_denied)) {
      if (!entry.is_regular_file())
        continue;
      auto ext = entry.path().extension().string();
      if (ext != ".log" && ext != ".gz")
        continue;

      if (entry.last_write_time() < cutoff) {
        // 파일이 현재 열려있으면 닫기
        auto path_str = entry.path().string();
        auto it = logFiles_.find(path_str);
        if (it != logFiles_.end()) {
          it->second.close();
          logFiles_.erase(it);
        }
        fs::remove(entry.path());
        ++deleted_count;
      }
    }

    if (deleted_count > 0) {
      // 삭제 후 새 로그에 기록 (재귀 방지 위해 직접 작성)
      std::string msg = "[" + getCurrentTimestamp() +
                        "][INFO] "
                        "🧹 Log cleanup: deleted " +
                        std::to_string(deleted_count) + " file(s) older than " +
                        std::to_string(retentionDays) + " days";
      if (console_output_enabled_)
        std::cout << msg << std::endl;
    }
  } catch (const std::exception &e) {
    if (console_output_enabled_)
      std::cout << "[LoggerEngine] cleanupOldLogs error: " << e.what()
                << std::endl;
  }
}

size_t LoggerEngine::getAvailableDiskSpaceMB() const {
  try {
    auto space = std::filesystem::space(log_base_path_);
    return static_cast<size_t>(space.available / (1024ULL * 1024ULL));
  } catch (...) {
    return SIZE_MAX; // 에러 시 무한대 반환 (긴급 삭제 미발동)
  }
}

void LoggerEngine::emergencyCleanupLogs(size_t targetFreeMB) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  try {
    namespace fs = std::filesystem;
    if (!fs::exists(log_base_path_))
      return;

    // 모든 .log 파일을 수정일 오름차순 수집
    std::vector<fs::directory_entry> log_files;
    for (const auto &entry : fs::recursive_directory_iterator(
             log_base_path_, fs::directory_options::skip_permission_denied)) {
      if (!entry.is_regular_file())
        continue;
      auto ext = entry.path().extension().string();
      if (ext == ".log" || ext == ".gz")
        log_files.push_back(entry);
    }

    std::sort(log_files.begin(), log_files.end(),
              [](const auto &a, const auto &b) {
                return a.last_write_time() < b.last_write_time();
              });

    size_t deleted_count = 0;
    for (const auto &entry : log_files) {
      if (getAvailableDiskSpaceMB() >= targetFreeMB)
        break;

      auto path_str = entry.path().string();
      auto it = logFiles_.find(path_str);
      if (it != logFiles_.end()) {
        it->second.close();
        logFiles_.erase(it);
      }
      fs::remove(entry.path());
      ++deleted_count;
    }

    if (deleted_count > 0 && console_output_enabled_) {
      std::cout << "[" << getCurrentTimestamp() << "][WARN] "
                << "🚨 Emergency cleanup: deleted " << deleted_count
                << " log file(s), free=" << getAvailableDiskSpaceMB() << " MB"
                << std::endl;
    }
  } catch (const std::exception &e) {
    if (console_output_enabled_)
      std::cout << "[LoggerEngine] emergencyCleanupLogs error: " << e.what()
                << std::endl;
  }
}

LogStatistics LoggerEngine::getStatistics() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return statistics_;
}

void LoggerEngine::resetStatistics() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  statistics_ = LogStatistics{};
}

bool LoggerEngine::loadFromConfigFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;

    size_t sep = line.find('=');
    if (sep != std::string::npos) {
      std::string key = trim(line.substr(0, sep));
      std::string value = trim(line.substr(sep + 1));
      applyConfig(key, value);
    }
  }
  return true;
}

// Internal Utilities

std::string LoggerEngine::trim(const std::string &s) {
  auto first = s.find_first_not_of(" \t\r\n\"");
  if (first == std::string::npos)
    return "";
  auto last = s.find_last_not_of(" \t\r\n\"");
  return s.substr(first, (last - first + 1));
}

void LoggerEngine::applyConfig(const std::string &key,
                               const std::string &value) {
  if (key == "LOG_LEVEL") {
    setLogLevel(stringToLogLevel(value));
  } else if (key == "LOG_FILE_PATH") {
    setLogBasePath(value);
  } else if (key == "LOG_TO_CONSOLE") {
    setConsoleOutput(value == "true" || value == "1");
  } else if (key == "LOG_TO_FILE") {
    setFileOutput(value == "true" || value == "1");
  } else if (key == "LOG_MAX_SIZE_MB") {
    setMaxLogSizeMB(static_cast<size_t>(std::stoul(value)));
  } else if (key == "LOG_MAX_FILES") {
    setMaxLogFiles(std::stoi(value));
  }
}

LogLevel LoggerEngine::stringToLogLevel(const std::string &level) {
  std::string s = level;
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);

  if (s == "TRACE")
    return LogLevel::TRACE;
  if (s == "DEBUG")
    return LogLevel::DEBUG;
  if (s == "INFO")
    return LogLevel::INFO;
  if (s == "WARN" || s == "WARNING")
    return LogLevel::WARN;
  if (s == "ERROR")
    return LogLevel::LOG_ERROR;
  if (s == "FATAL")
    return LogLevel::LOG_FATAL;
  if (s == "MAINTENANCE")
    return LogLevel::MAINTENANCE;
  return LogLevel::INFO;
}

std::string LoggerEngine::buildLogPath(const std::string &category) {
  try {
    std::filesystem::path log_file_path = buildLogFilePath(category);
    std::filesystem::create_directories(log_file_path.parent_path());
    return log_file_path.string();
  } catch (...) {
    return "./logs/fallback.log";
  }
}

std::filesystem::path
LoggerEngine::buildLogDirectoryPath(const std::string &category) {
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

std::filesystem::path
LoggerEngine::buildLogFilePath(const std::string &category) {
  std::filesystem::path dir_path = buildLogDirectoryPath(category);

  if (category.rfind("packet_", 0) == 0) {
    // packet_driver/device -> driver.log (simplified for standalone)
    std::string name = category.substr(7);
    std::replace(name.begin(), name.end(), '/', '_');
    return dir_path / (name + ".log");
  } else {
    return dir_path / (category + ".log");
  }
}

void LoggerEngine::writeToFile(const std::string &filePath,
                               const std::string &message) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  if (console_output_enabled_) {
    std::cout << message << std::endl;
  }

  if (file_output_enabled_) {
    std::ofstream &stream = logFiles_[filePath];
    if (!stream.is_open()) {
      stream.open(filePath, std::ios::app);
    }
    if (stream.is_open()) {
      stream << message << std::endl;
      stream.flush();
      checkAndRotateLogFile(filePath, stream);
    }
  }
}

void LoggerEngine::checkAndRotateLogFile(const std::string &filePath,
                                         std::ofstream &stream) {
  if (!stream.is_open())
    return;

  auto current_pos = stream.tellp();
  if (current_pos <= 0)
    return;

  size_t current_size_mb = static_cast<size_t>(current_pos) / (1024 * 1024);
  if (current_size_mb >= max_log_size_mb_) {
    stream.close();

    std::filesystem::path original_path(filePath);
    std::string backup_name = original_path.stem().string() + "_" +
                              getCurrentTimestamp() +
                              original_path.extension().string();
    std::replace(backup_name.begin(), backup_name.end(), ':', '-');
    std::replace(backup_name.begin(), backup_name.end(), ' ', '_');

    std::filesystem::path backup_path =
        original_path.parent_path() / backup_name;

    try {
      std::filesystem::rename(original_path, backup_path);
    } catch (...) {
    }

    stream.open(filePath, std::ios::app);
  }
}

std::string LoggerEngine::getCurrentDate() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf;
#ifdef _WIN32
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y%m%d");
  return oss.str();
}

std::string LoggerEngine::getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf;
#ifdef _WIN32
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

void LoggerEngine::updateStatistics(LogLevel level) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

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

} // namespace LogLib
