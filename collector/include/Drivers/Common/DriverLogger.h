#ifndef DRIVERS_DRIVER_LOGGER_H
#define DRIVERS_DRIVER_LOGGER_H

#include "CommonTypes.h"
#include "Utils/LogManager.h"      // 기존 LogManager 사용
#include "Utils/LogLevels.h"       // 기존 LogLevels 사용
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
 * @brief 드라이버 로그 카테고리 열거형
 */
enum class DriverLogCategory {
    GENERAL = 0,        ///< 일반적인 드라이버 로그
    CONNECTION,         ///< 연결 관련 로그
    COMMUNICATION,      ///< 통신 관련 로그
    DATA_PROCESSING,    ///< 데이터 처리 관련 로그
    ERROR_HANDLING,     ///< 에러 처리 관련 로그
    PERFORMANCE,        ///< 성능 관련 로그
    SECURITY,           ///< 보안 관련 로그
    PROTOCOL_SPECIFIC,  ///< 프로토콜별 특수 로그
    DIAGNOSTICS        ///< 진단 정보 로그
};

/**
 * @brief 드라이버 로그 컨텍스트 구조체
 */
struct DriverLogContext {
    UUID device_id;                    ///< 디바이스 ID
    ProtocolType protocol;             ///< 프로토콜 타입
    std::string endpoint;              ///< 연결 엔드포인트
    std::string function_name;         ///< 함수명
    std::string file_name;             ///< 파일명
    int line_number;                   ///< 라인 번호
    std::thread::id thread_id;         ///< 스레드 ID
    std::map<std::string, std::string> extra_data; ///< 추가 데이터
    
    DriverLogContext()
        : protocol(ProtocolType::UNKNOWN)
        , line_number(0)
        , thread_id(std::this_thread::get_id()) {}
    
    std::string ToJson() const {
        return "{\"device_id\":\"" + device_id + "\",\"protocol\":\"" + 
               std::to_string(static_cast<int>(protocol)) + "\"}";
    }
};

/**
 * @brief 드라이버 전용 로거 클래스 (기존 LogManager 래핑)
 */
class DriverLogger {
private:
    PulseOne::LogManager* legacy_logger_;
    DriverLogContext default_context_;
    LogLevel min_level_;
    std::map<DriverLogCategory, bool> category_enabled_;
    
    mutable std::atomic<uint64_t> log_count_debug_{0};
    mutable std::atomic<uint64_t> log_count_info_{0};
    mutable std::atomic<uint64_t> log_count_warn_{0};
    mutable std::atomic<uint64_t> log_count_error_{0};
    mutable std::atomic<uint64_t> log_count_fatal_{0};
    
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
    
    std::string FormatMessage(LogLevel /* level */, DriverLogCategory category,
                         const std::string& message,
                         const DriverLogContext& context = DriverLogContext()) const {
        std::ostringstream oss;
        oss << "[" << CategoryToString(category) << "] "
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
        legacy_logger_ = &PulseOne::LogManager::getInstance();
    }
    
    ~DriverLogger() = default;
    
    // 복사 생성자와 대입 연산자
    DriverLogger(const DriverLogger& other) {
        std::lock_guard<std::mutex> lock(other.logger_mutex_);
        legacy_logger_ = other.legacy_logger_;
        default_context_ = other.default_context_;
        min_level_ = other.min_level_;
        category_enabled_ = other.category_enabled_;
    }
    
    DriverLogger& operator=(const DriverLogger& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(logger_mutex_);
            std::lock_guard<std::mutex> lock2(other.logger_mutex_);
            
            legacy_logger_ = other.legacy_logger_;
            default_context_ = other.default_context_;
            min_level_ = other.min_level_;
            category_enabled_ = other.category_enabled_;
        }
        return *this;
    }
    
    // =============================================================================
    // 기본 로깅 인터페이스
    // =============================================================================
    
    void Debug(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (!ShouldLog(LogLevel::DEBUG, category)) return;
        
        UpdateStatistics(LogLevel::DEBUG);
        std::string formatted = FormatMessage(LogLevel::DEBUG, category, message, default_context_);
        
        // legacy_logger_가 있으면 사용, 없으면 기본 출력
        if (legacy_logger_) {
            legacy_logger_->Debug(formatted);
        } else {
            std::cout << "[DEBUG] " << formatted << std::endl;
        }
    }
    
    void Info(const std::string& message, 
              DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (!ShouldLog(LogLevel::INFO, category)) return;
        
        UpdateStatistics(LogLevel::INFO);
        std::string formatted = FormatMessage(LogLevel::INFO, category, message, default_context_);
        
        if (legacy_logger_) {
            legacy_logger_->Info(formatted);
        } else {
            std::cout << "[INFO] " << formatted << std::endl;
        }
    }
    
    void Warn(const std::string& message, 
              DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (!ShouldLog(LogLevel::WARN, category)) return;
        
        UpdateStatistics(LogLevel::WARN);
        std::string formatted = FormatMessage(LogLevel::WARN, category, message, default_context_);
        
        if (legacy_logger_) {
            legacy_logger_->Warn(formatted);
        } else {
            std::cout << "[WARN] " << formatted << std::endl;
        }
    }
    
    void Error(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (!ShouldLog(LogLevel::ERROR, category)) return;
        
        UpdateStatistics(LogLevel::ERROR);
        std::string formatted = FormatMessage(LogLevel::ERROR, category, message, default_context_);
        
        if (legacy_logger_) {
            legacy_logger_->Error(formatted);
        } else {
            std::cerr << "[ERROR] " << formatted << std::endl;
        }
    }
    
    void Fatal(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::GENERAL) {
        UpdateStatistics(LogLevel::FATAL);
        std::string formatted = FormatMessage(LogLevel::FATAL, category, message, default_context_);
        
        if (legacy_logger_) {
            legacy_logger_->Fatal(formatted);
        } else {
            std::cerr << "[FATAL] " << formatted << std::endl;
        }
    }
    
    // =============================================================================
    // IProtocolDriver에서 호출하는 특수 메소드들 추가
    // =============================================================================
    
    /**
     * @brief 연결 상태 변경 로그
     * @param old_status 이전 상태
     * @param new_status 새로운 상태
     */
    void LogConnectionStatusChange(ConnectionStatus old_status, ConnectionStatus new_status) {
        std::ostringstream oss;
        oss << "Connection status changed from " 
            << ConnectionStatusToString(old_status) 
            << " to " 
            << ConnectionStatusToString(new_status);
        Info(oss.str(), DriverLogCategory::CONNECTION);
    }
    
    /**
     * @brief 에러 정보 로그
     * @param error 에러 정보 구조체
     */
    void LogError(const ErrorInfo& error) {
        std::ostringstream oss;
        oss << "Error occurred: [" << ErrorCodeToString(error.code) << "] " 
            << error.message;
        
        if (!error.details.empty()) {
            oss << " | Details: " << error.details;
        }
        
        // 에러 코드에 따라 로그 레벨 결정
        switch (error.code) {
            case ErrorCode::SUCCESS:
                // SUCCESS는 로그하지 않음
                break;
            case ErrorCode::WARNING_DATA_STALE:
            case ErrorCode::WARNING_PARTIAL_SUCCESS:
                Warn(oss.str(), DriverLogCategory::ERROR_HANDLING);
                break;
            case ErrorCode::FATAL_INTERNAL_ERROR:
            case ErrorCode::FATAL_MEMORY_EXHAUSTED:
                Fatal(oss.str(), DriverLogCategory::ERROR_HANDLING);
                break;
            default:
                Error(oss.str(), DriverLogCategory::ERROR_HANDLING);
                break;
        }
    }
    
    // =============================================================================
    // 설정 및 상태 관리
    // =============================================================================
    
    void SetMinLevel(LogLevel level) noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        min_level_ = level;
    }
    
    LogLevel GetMinLevel() const noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        return min_level_;
    }
    
    void SetCategoryEnabled(DriverLogCategory category, bool enabled) noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        category_enabled_[category] = enabled;
    }
    
    bool IsCategoryEnabled(DriverLogCategory category) const noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        auto it = category_enabled_.find(category);
        return (it != category_enabled_.end()) ? it->second : false;
    }
    
    DriverLogContext GetDefaultContext() const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        return default_context_;
    }
    
    void UpdateContext(const DriverLogContext& context) {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        default_context_ = context;
    }
    
    // =============================================================================
    // 성능 측정 및 통계
    // =============================================================================
    
    void LogPerformanceMetric(const std::string& metric_name, 
                             double value, 
                             const std::string& unit) {
        std::ostringstream oss;
        oss << "Performance metric: " << metric_name 
            << " = " << value << " " << unit;
        Info(oss.str(), DriverLogCategory::PERFORMANCE);
    }
    
    struct LogStatistics {
        uint64_t debug_count = 0;
        uint64_t info_count = 0;
        uint64_t warn_count = 0;
        uint64_t error_count = 0;
        uint64_t fatal_count = 0;
        
        uint64_t GetTotalCount() const {
            return debug_count + info_count + warn_count + error_count + fatal_count;
        }
    };
    
    LogStatistics GetStatistics() const noexcept {
        LogStatistics stats;
        stats.debug_count = log_count_debug_.load();
        stats.info_count = log_count_info_.load();
        stats.warn_count = log_count_warn_.load();
        stats.error_count = log_count_error_.load();
        stats.fatal_count = log_count_fatal_.load();
        return stats;
    }
    
    void ResetStatistics() noexcept {
        log_count_debug_.store(0);
        log_count_info_.store(0);
        log_count_warn_.store(0);
        log_count_error_.store(0);
        log_count_fatal_.store(0);
    }
    
    // =============================================================================
    // 고급 로깅 기능
    // =============================================================================
    
    void LogWithContext(LogLevel level, const std::string& message,
                       DriverLogCategory category, const DriverLogContext& context) {
        if (!ShouldLog(level, category)) return;
        
        UpdateStatistics(level);
        std::string formatted = FormatMessage(level, category, message, context);
        
        switch (level) {
            case LogLevel::DEBUG:
                if (legacy_logger_) legacy_logger_->Debug(formatted);
                else std::cout << "[DEBUG] " << formatted << std::endl;
                break;
            case LogLevel::INFO:
                if (legacy_logger_) legacy_logger_->Info(formatted);
                else std::cout << "[INFO] " << formatted << std::endl;
                break;
            case LogLevel::WARN:
                if (legacy_logger_) legacy_logger_->Warn(formatted);
                else std::cout << "[WARN] " << formatted << std::endl;
                break;
            case LogLevel::ERROR:
                if (legacy_logger_) legacy_logger_->Error(formatted);
                else std::cerr << "[ERROR] " << formatted << std::endl;
                break;
            case LogLevel::FATAL:
                if (legacy_logger_) legacy_logger_->Fatal(formatted);
                else std::cerr << "[FATAL] " << formatted << std::endl;
                break;
        }
    }
    
    template<typename... Args>
    void LogFormatted(LogLevel level, DriverLogCategory category,
                     const std::string& format, Args&&... args) {
        if (!ShouldLog(level, category)) return;
        
        try {
            // 간단한 sprintf 스타일 포맷팅 (C++20의 std::format 대체)
            char buffer[4096];
            std::snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            
            switch (level) {
                case LogLevel::DEBUG: Debug(std::string(buffer), category); break;
                case LogLevel::INFO:  Info(std::string(buffer), category); break;
                case LogLevel::WARN:  Warn(std::string(buffer), category); break;
                case LogLevel::ERROR: Error(std::string(buffer), category); break;
                case LogLevel::FATAL: Fatal(std::string(buffer), category); break;
            }
        } catch (const std::exception& e) {
            Error("Log formatting error: " + std::string(e.what()) + 
                  ", Original format: " + format, DriverLogCategory::ERROR_HANDLING);
        }
    }
    
    void LogIf(bool condition, LogLevel level, const std::string& message,
               DriverLogCategory category = DriverLogCategory::GENERAL) {
        if (condition) {
            switch (level) {
                case LogLevel::DEBUG: Debug(message, category); break;
                case LogLevel::INFO:  Info(message, category); break;
                case LogLevel::WARN:  Warn(message, category); break;
                case LogLevel::ERROR: Error(message, category); break;
                case LogLevel::FATAL: Fatal(message, category); break;
            }
        }
    }
    
private:
    bool ShouldLog(LogLevel level, DriverLogCategory category) const noexcept {
        return (level >= min_level_) && IsCategoryEnabled(category);
    }
    
    void UpdateStatistics(LogLevel level) const noexcept {
        switch (level) {
            case LogLevel::DEBUG: log_count_debug_++; break;
            case LogLevel::INFO:  log_count_info_++; break;
            case LogLevel::WARN:  log_count_warn_++; break;
            case LogLevel::ERROR: log_count_error_++; break;
            case LogLevel::FATAL: log_count_fatal_++; break;
        }
    }
};

// =============================================================================
// 편의 매크로 정의
// =============================================================================

#define DRIVER_LOG_CONTEXT() \
    [&]() -> DriverLogContext { \
        DriverLogContext ctx = this->logger_.GetDefaultContext(); \
        ctx.function_name = __FUNCTION__; \
        ctx.file_name = __FILE__; \
        ctx.line_number = __LINE__; \
        ctx.thread_id = std::this_thread::get_id(); \
        return ctx; \
    }()

#define DRIVER_LOG_DEBUG(logger, message, category) \
    do { \
        if ((logger).GetMinLevel() <= LogLevel::DEBUG) { \
            auto ctx = DRIVER_LOG_CONTEXT(); \
            (logger).LogWithContext(LogLevel::DEBUG, message, category, ctx); \
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

#define DRIVER_LOG_FATAL(logger, message, category) \
    do { \
        auto ctx = DRIVER_LOG_CONTEXT(); \
        (logger).LogWithContext(LogLevel::FATAL, message, category, ctx); \
    } while(0)

#define DLOG_DEBUG(logger, message) \
    DRIVER_LOG_DEBUG(logger, message, DriverLogCategory::GENERAL)
    
#define DLOG_INFO(logger, message) \
    DRIVER_LOG_INFO(logger, message, DriverLogCategory::GENERAL)
    
#define DLOG_WARN(logger, message) \
    DRIVER_LOG_WARN(logger, message, DriverLogCategory::GENERAL)
    
#define DLOG_ERROR(logger, message) \
    DRIVER_LOG_ERROR(logger, message, DriverLogCategory::ERROR_HANDLING)
    
#define DLOG_FATAL(logger, message) \
    DRIVER_LOG_FATAL(logger, message, DriverLogCategory::ERROR_HANDLING)

#define DRIVER_LOG_IF(logger, condition, level, message, category) \
    do { \
        if (condition) { \
            DRIVER_LOG_##level(logger, message, category); \
        } \
    } while(0)

#define DRIVER_LOG_DURATION(logger, name, code_block) \
    do { \
        auto start_time = std::chrono::high_resolution_clock::now(); \
        code_block; \
        auto end_time = std::chrono::high_resolution_clock::now(); \
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            end_time - start_time).count(); \
        (logger).LogPerformanceMetric(name, static_cast<double>(duration_ms), "ms"); \
    } while(0)

#define DRIVER_LOG_FUNCTION_TRACE(logger) \
    struct FunctionTracer { \
        DriverLogger& logger_ref; \
        std::string func_name; \
        std::chrono::high_resolution_clock::time_point start_time; \
        \
        FunctionTracer(DriverLogger& logger, const std::string& name) \
            : logger_ref(logger), func_name(name), \
              start_time(std::chrono::high_resolution_clock::now()) { \
            logger_ref.Debug("ENTER: " + func_name, DriverLogCategory::GENERAL); \
        } \
        \
        ~FunctionTracer() { \
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>( \
                std::chrono::high_resolution_clock::now() - start_time).count(); \
            logger_ref.Debug("EXIT: " + func_name + " (took " + \
                            std::to_string(duration) + " μs)", \
                            DriverLogCategory::PERFORMANCE); \
        } \
    } _func_tracer(logger, __FUNCTION__)

} // namespace Drivers
} // namespace PulseOne

#endif // DRIVERS_DRIVER_LOGGER_H