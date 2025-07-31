#ifndef DRIVERS_DRIVER_LOGGER_H
#define DRIVERS_DRIVER_LOGGER_H


#include "Utils/LogManager.h"
#include <memory>
#include <string>
#include <sstream>
#include <mutex>
#include <thread>
#include <map>
#include <atomic>

namespace PulseOne {
namespace Drivers {



/**
 * @brief 드라이버 전용 로거 클래스 (기존 LogManager 래핑)
 */
class DriverLogger {
private:
    LogManager* legacy_logger_;
    DriverLogContext default_context_;
    LogLevel min_level_;
    std::map<DriverLogCategory, bool> category_enabled_;
    
    mutable std::atomic<uint64_t> log_count_debug_{0};
    mutable std::atomic<uint64_t> log_count_info_{0};
    mutable std::atomic<uint64_t> log_count_warn_{0};
    mutable std::atomic<uint64_t> log_count_error_{0};
    mutable std::atomic<uint64_t> log_count_fatal_{0};
    mutable std::atomic<uint64_t> log_count_trace_{0};      // 🆕 TRACE 추가
    mutable std::atomic<uint64_t> log_count_maintenance_{0}; // 🆕 MAINTENANCE 추가
    
    mutable std::mutex logger_mutex_;
    
    std::string CategoryToString(DriverLogCategory category) const noexcept {
        switch (category) {
            case DriverLogCategory::GENERAL: return "GENERAL";
            case DriverLogCategory::CONNECTION: return "CONNECTION";
            case DriverLogCategory::COMMUNICATION: return "COMMUNICATION";
            case DriverLogCategory::DATA_PROCESSING: return "DATA_PROCESSING";
            case DriverLogCategory::ERROR_HANDLING: return "ERROR_HANDLING";
            case DriverLogCategory::PERFORMANCE: return "PERFORMANCE";
            case DriverLogCategory::SECURITY: return "SECURITY";
            case DriverLogCategory::PROTOCOL_SPECIFIC: return "PROTOCOL_SPECIFIC";
            case DriverLogCategory::DIAGNOSTICS: return "DIAGNOSTICS";
            default: return "UNKNOWN";
        }
    }
    
    std::string FormatMessage(LogLevel level, DriverLogCategory category,
                            const std::string& message,
                            const DriverLogContext& context = DriverLogContext()) const {
        
        // ✅ unused parameter 경고 해결: 매개변수 활용
        std::ostringstream oss;
        
        // 🆕 level을 실제로 활용하여 경고 해결
        const char* level_str = [level]() {
            switch (level) {
                case LogLevel::TRACE: return "TRACE";
                case LogLevel::DEBUG_LEVEL: return "DEBUG";
                case LogLevel::INFO: return "INFO";
                case LogLevel::WARN: return "WARN";
                case LogLevel::ERROR: return "ERROR";
                case LogLevel::FATAL: return "FATAL";
                case LogLevel::MAINTENANCE: return "MAINT";
                default: return "UNKNOWN";
            }
        }();
        
        // 📝 포맷팅에 level 정보 포함
        oss << "[" << level_str << "] "
            << "[" << CategoryToString(category) << "] "
            << "[" << context.device_id << "] "
            << message;
        
        return oss.str();
    }

public:
    DriverLogger(const UUID& device_id = "", 
                 ProtocolType protocol = ProtocolType::UNKNOWN,
                 const std::string& endpoint = "")
        : legacy_logger_(nullptr), min_level_(LogLevel::INFO) {
        
        default_context_.device_id = device_id;
        default_context_.protocol = protocol;
        default_context_.endpoint = endpoint;
        
        // 모든 카테고리 기본 활성화
        for (int i = 0; i <= static_cast<int>(DriverLogCategory::DIAGNOSTICS); ++i) {
            category_enabled_[static_cast<DriverLogCategory>(i)] = true;
        }
        
        // 기존 LogManager 인스턴스 가져오기
        legacy_logger_ = &LogManager::getInstance();
    }
    
    ~DriverLogger() = default;
    
    // 기본 로깅 메서드들
    void Debug(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::DEBUG_LEVEL, message, category, default_context_);
    }
    
    void Info(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::INFO, message, category, default_context_);
    }
    
    void Warn(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::WARN, message, category, default_context_);
    }
    
    void Error(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::ERROR, message, category, default_context_);
    }
    
    void Fatal(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::FATAL, message, category, default_context_);
    }
    
    void Trace(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::TRACE, message, category, default_context_);
    }
    
    void Maintenance(const std::string& message, DriverLogCategory category = DriverLogCategory::GENERAL) {
        LogWithContext(LogLevel::MAINTENANCE, message, category, default_context_);
    }
    
    // 컨텍스트 포함 로깅
    void LogWithContext(LogLevel level, const std::string& message, 
                       DriverLogCategory category, const DriverLogContext& context) {
        
        if (!ShouldLog(level, category)) return;
        
        std::string formatted_message = FormatMessage(level, category, message, context);
        
        // 기존 LogManager로 전달
        if (legacy_logger_) {
            switch (level) {
                case LogLevel::DEBUG_LEVEL:
                    legacy_logger_->Debug(formatted_message);
                    break;
                case LogLevel::INFO:
                    legacy_logger_->Info(formatted_message);
                    break;
                case LogLevel::WARN:
                    legacy_logger_->Warn(formatted_message);
                    break;
                case LogLevel::ERROR:
                    legacy_logger_->Error(formatted_message);
                    break;
                case LogLevel::FATAL:
                    legacy_logger_->Fatal(formatted_message);
                    break;
                case LogLevel::TRACE:  // 🆕 TRACE 처리
                    legacy_logger_->Trace(formatted_message);
                    break;
                case LogLevel::MAINTENANCE:  // 🆕 MAINTENANCE 처리
                    legacy_logger_->Maintenance(formatted_message);
                    break;
            }
        }
        
        UpdateStatistics(level);
    }
    
    // 설정 메서드들
    void SetMinLevel(LogLevel level) { min_level_ = level; }
    LogLevel GetMinLevel() const { return min_level_; }
    
    void SetCategoryEnabled(DriverLogCategory category, bool enabled) {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        category_enabled_[category] = enabled;
    }
    
    bool IsCategoryEnabled(DriverLogCategory category) const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        auto it = category_enabled_.find(category);
        return it != category_enabled_.end() ? it->second : false;
    }
    
    bool ShouldLog(LogLevel level, DriverLogCategory category) const {
        return level >= min_level_ && IsCategoryEnabled(category);
    }
    
    DriverLogContext GetDefaultContext() const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        return default_context_;
    }
    
    void UpdateContext(const DriverLogContext& context) {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        default_context_ = context;
    }
    
    // 고급 로깅 메서드들 (IProtocolDriver에서 요구)
    void LogConnectionStatusChange(ConnectionStatus old_status, ConnectionStatus new_status) {
        std::ostringstream oss;
        oss << "Connection status changed from " << static_cast<int>(old_status) 
            << " to " << static_cast<int>(new_status);
        Info(oss.str(), DriverLogCategory::CONNECTION);
    }
    
    void LogError(const ErrorInfo& error) {
        std::ostringstream oss;
        oss << "Error occurred: " << error.message 
            << " (Code: " << static_cast<int>(error.code) << ")";
        if (!error.details.empty()) {
            oss << " Details: " << error.details;
        }
        Error(oss.str(), DriverLogCategory::ERROR_HANDLING);
    }
    
    // 조건부 로깅
    void LogIf(bool condition, LogLevel level, const std::string& message, 
              DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (condition) {
            switch (level) {
                case LogLevel::DEBUG_LEVEL: Debug(message, category); break;
                case LogLevel::INFO: Info(message, category); break;
                case LogLevel::WARN: Warn(message, category); break;
                case LogLevel::ERROR: Error(message, category); break;
                case LogLevel::FATAL: Fatal(message, category); break;
                case LogLevel::TRACE: Trace(message, category); break;  // 🆕
                case LogLevel::MAINTENANCE: Maintenance(message, category); break;  // 🆕
            }
        }
    }
    
    // 통계 업데이트
    void UpdateStatistics(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG_LEVEL: log_count_debug_++; break;
            case LogLevel::INFO: log_count_info_++; break;
            case LogLevel::WARN: log_count_warn_++; break;
            case LogLevel::ERROR: log_count_error_++; break;
            case LogLevel::FATAL: log_count_fatal_++; break;
            case LogLevel::TRACE: log_count_trace_++; break;  // 🆕
            case LogLevel::MAINTENANCE: log_count_maintenance_++; break;  // 🆕
        }
    }
    
    // 성능 측정
    void LogPerformanceMetric(const std::string& metric_name, 
                             double value, 
                             const std::string& unit) {
        std::ostringstream oss;
        oss << "Performance metric: " << metric_name 
            << " = " << value << " " << unit;
        Info(oss.str(), DriverLogCategory::PERFORMANCE);
    }
    
    // 통계 조회
    LogStatistics getStatistics() const {
        LogStatistics stats;
        stats.debug_count = log_count_debug_.load();
        stats.info_count = log_count_info_.load();
        stats.warning_count = log_count_warn_.load();  // warning_count로 수정
        stats.error_count = log_count_error_.load();
        stats.trace_count = log_count_trace_.load();
        stats.maintenance_count = log_count_maintenance_.load();
        stats.total_logs = stats.debug_count + stats.info_count + stats.warning_count + 
                          stats.error_count + stats.trace_count + stats.maintenance_count;
        return stats;
    }
    
    void ResetStatistics() noexcept {
        log_count_debug_.store(0);
        log_count_info_.store(0);
        log_count_warn_.store(0);
        log_count_error_.store(0);
        log_count_fatal_.store(0);
        log_count_trace_.store(0);
        log_count_maintenance_.store(0);
    }
};

// =============================================================================
// 편의 매크로들
// =============================================================================

#define DRIVER_LOG_CONTEXT() \
    [&]() -> DriverLogContext { \
        DriverLogContext ctx = this->logger_.GetDefaultContext(); \
        ctx.device_name = __FUNCTION__; \
        ctx.endpoint = __FILE__; \
        return ctx; \
    }()

#define DRIVER_LOG_DEBUG(logger, message, category) \
    do { \
        if ((logger).GetMinLevel() <= LogLevel::DEBUG_LEVEL) { \
            auto ctx = DRIVER_LOG_CONTEXT(); \
            (logger).LogWithContext(LogLevel::DEBUG_LEVEL, message, category, ctx); \
        } \
    } while(0)

#define DRIVER_LOG_INFO(logger, message, category) \
    do { \
        if ((logger).GetMinLevel() <= LogLevel::INFO) { \
            auto ctx = DRIVER_LOG_CONTEXT(); \
            (logger).LogWithContext(LogLevel::INFO, message, category, ctx); \
        } \
    } while(0)

#define DRIVER_LOG_WARN(logger, message, category) \
    do { \
        if ((logger).GetMinLevel() <= LogLevel::WARN) { \
            auto ctx = DRIVER_LOG_CONTEXT(); \
            (logger).LogWithContext(LogLevel::WARN, message, category, ctx); \
        } \
    } while(0)

#define DRIVER_LOG_ERROR(logger, message, category) \
    do { \
        if ((logger).GetMinLevel() <= LogLevel::ERROR) { \
            auto ctx = DRIVER_LOG_CONTEXT(); \
            (logger).LogWithContext(LogLevel::ERROR, message, category, ctx); \
        } \
    } while(0)

} // namespace Drivers
} // namespace PulseOne

#endif // DRIVERS_DRIVER_LOGGER_H