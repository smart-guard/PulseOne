#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include "LogLevels.h"

namespace PulseOne {

class LogManager {
public:
    static LogManager& getInstance();

    // 기존 코드 호환성을 위한 메소드들
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    void Debug(const std::string& message);

    // 확장된 로그 메소드들
    void log(const std::string& category, LogLevel level, const std::string& message);
    void log(const std::string& category, const std::string& level, const std::string& message);
    
    // 특수 목적 로그
    void logDriver(const std::string& driverName, const std::string& message);
    void logError(const std::string& message);
    void logPacket(const std::string& driver, const std::string& device,
                   const std::string& rawPacket, const std::string& decoded);

    // 로그 레벨 설정
    void setLogLevel(LogLevel level) { minLevel_ = level; }
    LogLevel getLogLevel() const { return minLevel_; }

    void flushAll();

private:
    LogManager();
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string buildLogPath(const std::string& category);
    void writeToFile(const std::string& filePath, const std::string& message);
    bool shouldLog(LogLevel level) const;

    std::mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
    LogLevel minLevel_ = LogLevel::INFO;
    std::string defaultCategory_ = "system";
};

} // namespace PulseOne

// 전역 로거 편의 함수
inline PulseOne::LogManager& Logger() {
    return PulseOne::LogManager::getInstance();
}

#endif // LOG_MANAGER_H