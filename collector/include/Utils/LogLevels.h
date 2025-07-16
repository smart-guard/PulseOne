#ifndef LOG_LEVELS_H
#define LOG_LEVELS_H

#include <string>

// 🔹 간단 매크로 방식 (직접 문자열 사용 가능)
#define LOG_LEVEL_DEBUG "DEBUG"
#define LOG_LEVEL_INFO  "INFO"
#define LOG_LEVEL_WARN  "WARN"
#define LOG_LEVEL_ERROR "ERROR"
#define LOG_LEVEL_FATAL "FATAL"

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

// (선택) 문자열 → enum 역변환이 필요한 경우 아래 함수도 추가할 수 있음
/*
inline LogLevel fromString(const std::string& str) {
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO")  return LogLevel::INFO;
    if (str == "WARN")  return LogLevel::WARN;
    if (str == "ERROR") return LogLevel::ERROR;
    if (str == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO; // default
}
*/

#endif // LOG_LEVELS_H
