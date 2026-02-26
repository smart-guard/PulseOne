#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

/**
 * @file LogManager.h
 * @brief PulseOne 통합 로그 관리자 - LogLib 기반 Delegation Wrapper
 * @details
 * 이 클래스는 기존 PulseOne 프로젝트와의 호환성을 유지하기 위한 Wrapper
 * 클래스입니다. 실제 모든 로깅 로직(파일 관리, 로테이션 등)은 독립 라이브러리인
 * LogLib::LoggerEngine이 수행합니다. 이를 통해 핵심 로그 엔진을 다른
 * 프로젝트에서 재사용(DLL/SO)할 수 있습니다.
 */

#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Platform/PlatformCompat.h"

// LogLib (독립 라이브러리) 포함
#include "LoggerEngine.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

// 전역 네임스페이스 타입 별칭 (기존 코드 100% 호환성 유지)
using LogLevel = PulseOne::Enums::LogLevel;
using DriverLogCategory = PulseOne::Enums::DriverLogCategory;
using DataQuality = PulseOne::Enums::DataQuality;
using UniqueId = PulseOne::BasicTypes::UniqueId;
using EngineerID = PulseOne::BasicTypes::EngineerID;
using Timestamp = PulseOne::BasicTypes::Timestamp;

/**
 * @brief PulseOne 전용 로그 관리자 (Wrapper)
 */
class LogManager {
public:
  static LogManager &getInstance() {
    static LogManager instance;
    instance.ensureInitialized();
    return instance;
  }

  bool isInitialized() const {
    return initialized_.load(std::memory_order_acquire);
  }

  // =============================================================================
  // 기본 로그 메소드들 (기존 코드 호환성)
  // =============================================================================
  void Info(const std::string &message);
  void Warn(const std::string &message);
  void Error(const std::string &message);
  void Fatal(const std::string &message);
  void Debug(const std::string &message);
  void Trace(const std::string &message);
  void Maintenance(const std::string &message);

  // 포맷 문자열 지원 템플릿
  template <typename... Args>
  void Info(const std::string &format, Args &&...args) {
    Info(formatString(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Warn(const std::string &format, Args &&...args) {
    Warn(formatString(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Error(const std::string &format, Args &&...args) {
    Error(formatString(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Fatal(const std::string &format, Args &&...args) {
    Fatal(formatString(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Debug(const std::string &format, Args &&...args) {
    Debug(formatString(format, std::forward<Args>(args)...));
  }
  template <typename... Args>
  void Trace(const std::string &format, Args &&...args) {
    Trace(formatString(format, std::forward<Args>(args)...));
  }

  // 확장 로그 메소드들
  void log(const std::string &category, LogLevel level,
           const std::string &message);
  void log(const std::string &category, const std::string &level,
           const std::string &message);

  void logMaintenance(const UniqueId &device_id, const EngineerID &engineer_id,
                      const std::string &message);
  void logMaintenanceStart(const PulseOne::Structs::DeviceInfo &device,
                           const EngineerID &engineer_id);
  void logMaintenanceEnd(const PulseOne::Structs::DeviceInfo &device,
                         const EngineerID &engineer_id);
  void logRemoteControlBlocked(const UniqueId &device_id,
                               const std::string &reason);

  void logDriver(const std::string &driverName, const std::string &message);
  void logDriver(const UniqueId &device_id, DriverLogCategory category,
                 LogLevel level, const std::string &message);
  void logDataQuality(const UniqueId &device_id, const UniqueId &point_id,
                      DataQuality quality, const std::string &reason = "");

  void logError(const std::string &message);
  void logPacket(const std::string &driver, const std::string &device,
                 const std::string &rawPacket, const std::string &decoded);

  // 설정 및 제어 (LoggerEngine으로 위임)
  void setLogLevel(LogLevel level);
  LogLevel getLogLevel() const;
  void setCategoryLogLevel(DriverLogCategory category, LogLevel level);
  LogLevel getCategoryLogLevel(DriverLogCategory category) const;

  void reloadSettings();
  void setConsoleOutput(bool enabled);
  void setFileOutput(bool enabled);
  void setLogBasePath(const std::string &path);
  void setMaxLogSizeMB(size_t size_mb);
  void setMaxLogFiles(int count);

  // Log Retention & Disk Management
  void cleanupOldLogs(int retentionDays);
  size_t getAvailableDiskSpaceMB() const;
  void emergencyCleanupLogs(size_t targetFreeMB);

  void setMaintenanceMode(bool enabled);
  bool isMaintenanceModeEnabled() const;

  PulseOne::Structs::LogStatistics getStatistics() const;
  void resetStatistics();
  void flushAll();
  void rotateLogs();

private:
  LogManager();
  ~LogManager() = default;

  void ensureInitialized();
  bool doInitialize();
  void loadLogSettingsFromConfig();

  // 포맷팅 지원
  template <typename... Args>
  std::string formatString(const std::string &format, Args &&...args) {
    std::stringstream ss;
    size_t pos = 0;
    formatRecursive(ss, format, pos, std::forward<Args>(args)...);
    return ss.str();
  }

  template <typename T>
  void formatHelper(std::stringstream &ss, const std::string &format,
                    size_t &pos, const T &value) {
    size_t placeholder = format.find("{}", pos);
    if (placeholder != std::string::npos) {
      ss << format.substr(pos, placeholder - pos) << value;
      pos = placeholder + 2;
    } else {
      ss << format.substr(pos);
    }
  }
  void formatRecursive(std::stringstream &ss, const std::string &format,
                       size_t &pos) {
    ss << format.substr(pos);
  }
  template <typename T, typename... Args>
  void formatRecursive(std::stringstream &ss, const std::string &format,
                       size_t &pos, const T &value, Args &&...args) {
    formatHelper(ss, format, pos, value);
    formatRecursive(ss, format, pos, std::forward<Args>(args)...);
  }

  std::atomic<bool> initialized_;
  mutable std::recursive_mutex init_mutex_;
  std::map<DriverLogCategory, LogLevel> categoryLevels_;
  bool maintenance_mode_enabled_;
};

// 전역 편의 함수들
inline LogManager &Logger() { return LogManager::getInstance(); }
inline void LogMaintenance(const UniqueId &device_id,
                           const EngineerID &engineer_id,
                           const std::string &message) {
  LogManager::getInstance().logMaintenance(device_id, engineer_id, message);
}
inline void LogDriver(const UniqueId &device_id, DriverLogCategory category,
                      LogLevel level, const std::string &message) {
  LogManager::getInstance().logDriver(device_id, category, level, message);
}
inline void ReloadLogSettings() { LogManager::getInstance().reloadSettings(); }
inline void SetConsoleLogging(bool enabled) {
  LogManager::getInstance().setConsoleOutput(enabled);
}
inline void SetFileLogging(bool enabled) {
  LogManager::getInstance().setFileOutput(enabled);
}
inline void SetLogPath(const std::string &path) {
  LogManager::getInstance().setLogBasePath(path);
}

#endif // LOG_MANAGER_H