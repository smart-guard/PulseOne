#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include <sstream>
#include <iostream>
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

    // 포맷 문자열을 지원하는 템플릿 메소드들
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

    // 간단한 포맷 문자열 구현
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

    // 재귀 종료 조건
    void formatRecursive(std::stringstream& ss, const std::string& format, size_t& pos) {
        ss << format.substr(pos);
    }

    // 재귀 처리
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