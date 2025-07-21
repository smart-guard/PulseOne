#ifndef LOG_LEVELS_H
#define LOG_LEVELS_H

#include <string>

// DEBUG 매크로 충돌 방지
#ifdef DEBUG
#undef DEBUG
#endif

// 로그 레벨 enum
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

// enum → 문자열 변환
inline std::string toString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

// 문자열 → enum 변환
inline LogLevel fromString(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO; // 기본값
}

// 모듈 상수
#define LOG_MODULE_DATABASE "database"
#define LOG_MODULE_SYSTEM   "system"
#define LOG_MODULE_CONFIG   "config"
#define LOG_MODULE_PLUGIN   "plugin"
#define LOG_MODULE_ENGINE   "engine"
#define LOG_MODULE_DRIVER   "driver"

#endif // LOG_LEVELS_H