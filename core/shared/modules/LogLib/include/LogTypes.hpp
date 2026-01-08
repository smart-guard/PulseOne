#ifndef LOG_TYPES_HPP
#define LOG_TYPES_HPP

#include <cstdint>
#include <string>
#include <chrono>

namespace LogLib {

/**
 * @brief Independent Log Level enumeration
 */
enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5,
    MAINTENANCE = 6,
    OFF = 255
};

/**
 * @brief Independent Log Statistics structure
 */
struct LogStatistics {
    uint64_t total_logs = 0;
    uint64_t trace_count = 0;
    uint64_t debug_count = 0;
    uint64_t info_count = 0;
    uint64_t warn_count = 0;
    uint64_t error_count = 0;
    uint64_t fatal_count = 0;
    uint64_t maintenance_count = 0;
    std::chrono::system_clock::time_point last_log_time;
};

/**
 * @brief Helper to convert LogLevel to string
 */
inline std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::MAINTENANCE: return "MAINT";
        default: return "UNKNOWN";
    }
}

} // namespace LogLib

#endif // LOG_TYPES_HPP
