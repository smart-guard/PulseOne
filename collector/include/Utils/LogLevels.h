#ifndef LOG_LEVELS_H
#define LOG_LEVELS_H

#include <string>

// 🔹 간단 매크로 방식 (직접 문자열 사용 가능)
#define LOG_LEVEL_DEBUG "DEBUG"
#define LOG_LEVEL_INFO  "INFO"
#define LOG_LEVEL_WARN  "WARN"
#define LOG_LEVEL_ERROR "ERROR"
#define LOG_LEVEL_FATAL "FATAL"

// 🔹 호환성을 위한 짧은 별칭 (기존 코드에서 사용 중)
#define LOG_DEBUG LOG_LEVEL_DEBUG
#define LOG_INFO  LOG_LEVEL_INFO
#define LOG_WARN  LOG_LEVEL_WARN
#define LOG_ERROR LOG_LEVEL_ERROR
#define LOG_FATAL LOG_LEVEL_FATAL

// 🔹 모듈 상수 추가 (DatabaseManager, main.cpp에서 사용)
#define LOG_MODULE_DATABASE "database"
#define LOG_MODULE_SYSTEM   "system"
#define LOG_MODULE_CONFIG   "config"
#define LOG_MODULE_PLUGIN   "plugin"
#define LOG_MODULE_ENGINE   "engine"
#define LOG_MODULE_DRIVER   "driver"

// 🔹 고급 enum + 매핑 함수 (선택적 사용 가능)
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// 🔹 enum → 문자열 변환 함수
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

#endif // LOG_LEVELS_H