/**
 * @file DriverLogger.h
 * @brief 기존 LogManager와 호환되는 드라이버 전용 로깅 시스템
 * @details 기존 PulseOne LogManager를 확장하여 드라이버 시스템에 특화된 로깅 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 1.0.0
 */

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

namespace PulseOne {
namespace Drivers {

/**
 * @brief 드라이버 로그 카테고리 열거형
 * @details 드라이버별 로그 분류를 위한 카테고리
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
 * @details 로그 항목과 함께 기록될 컨텍스트 정보
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
    
    /**
     * @brief 기본 생성자
     */
    DriverLogContext()
        : protocol(ProtocolType::UNKNOWN)
        , line_number(0)
        , thread_id(std::this_thread::get_id()) {}
    
    /**
     * @brief 컨텍스트를 JSON 문자열로 변환
     * @return JSON 형태의 컨텍스트 정보
     */
    std::string ToJson() const;
};

/**
 * @brief 드라이버 전용 로거 클래스
 * @details 기존 LogManager를 래핑하여 드라이버 시스템에 특화된 기능 제공
 * 
 * 주요 특징:
 * - 기존 LogManager와 완전 호환
 * - 드라이버별 컨텍스트 정보 자동 추가
 * - 성능 측정 및 통계 기능
 * - 프로토콜별 로그 필터링
 * - 스레드 안전성 보장
 */
class DriverLogger {
private:
    // 기존 LogManager 인스턴스에 대한 참조
    // 실제로는 싱글턴 인스턴스를 사용할 예정
    PulseOne::LogManager* legacy_logger_;
    
    // 드라이버별 컨텍스트 정보
    DriverLogContext default_context_;
    
    // 로그 레벨 필터 (드라이버별로 다르게 설정 가능)
    LogLevel min_level_;
    
    // 카테고리별 활성화 여부
    std::map<DriverLogCategory, bool> category_enabled_;
    
    // 성능 통계를 위한 카운터들
    mutable std::atomic<uint64_t> log_count_debug_{0};
    mutable std::atomic<uint64_t> log_count_info_{0};
    mutable std::atomic<uint64_t> log_count_warn_{0};
    mutable std::atomic<uint64_t> log_count_error_{0};
    mutable std::atomic<uint64_t> log_count_fatal_{0};
    
    // 스레드 안전성을 위한 뮤텍스
    mutable std::mutex logger_mutex_;
    
    /**
     * @brief 카테고리를 문자열로 변환
     * @param category 드라이버 로그 카테고리
     * @return 문자열 표현
     */
    std::string CategoryToString(DriverLogCategory category) const noexcept;
    
    /**
     * @brief 로그 메시지 포맷팅
     * @param level 로그 레벨
     * @param category 로그 카테고리
     * @param message 원본 메시지
     * @param context 컨텍스트 정보
     * @return 포맷된 메시지
     */
    std::string FormatMessage(LogLevel level, DriverLogCategory category,
                             const std::string& message, 
                             const DriverLogContext& context) const;

public:
    /**
     * @brief 생성자
     * @param device_id 디바이스 ID
     * @param protocol 프로토콜 타입
     * @param endpoint 연결 엔드포인트
     */
    DriverLogger(const UUID& device_id = "", 
                 ProtocolType protocol = ProtocolType::UNKNOWN,
                 const std::string& endpoint = "");
    
    /**
     * @brief 소멸자
     */
    ~DriverLogger() = default;
    
    // 복사 생성자와 대입 연산자 (스레드 안전성을 위해 삭제하지 않음)
    DriverLogger(const DriverLogger& other);
    DriverLogger& operator=(const DriverLogger& other);
    
    // =============================================================================
    // 기본 로깅 인터페이스 (기존 LogManager와 호환)
    // =============================================================================
    
    /**
     * @brief DEBUG 레벨 로그
     * @param message 로그 메시지
     * @param category 로그 카테고리 (기본값: GENERAL)
     */
    void Debug(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::GENERAL);
    
    /**
     * @brief INFO 레벨 로그
     * @param message 로그 메시지
     * @param category 로그 카테고리 (기본값: GENERAL)
     */
    void Info(const std::string& message, 
              DriverLogCategory category = DriverLogCategory::GENERAL);
    
    /**
     * @brief WARN 레벨 로그
     * @param message 로그 메시지
     * @param category 로그 카테고리 (기본값: GENERAL)
     */
    void Warn(const std::string& message, 
              DriverLogCategory category = DriverLogCategory::GENERAL);
    
    /**
     * @brief ERROR 레벨 로그
     * @param message 로그 메시지
     * @param category 로그 카테고리 (기본값: ERROR_HANDLING)
     */
    void Error(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::ERROR_HANDLING);
    
    /**
     * @brief FATAL 레벨 로그
     * @param message 로그 메시지
     * @param category 로그 카테고리 (기본값: ERROR_HANDLING)
     */
    void Fatal(const std::string& message, 
               DriverLogCategory category = DriverLogCategory::ERROR_HANDLING);
    
    // =============================================================================
    // 확장된 로깅 인터페이스 (컨텍스트 포함)
    // =============================================================================
    
    /**
     * @brief 컨텍스트가 포함된 로그
     * @param level 로그 레벨
     * @param message 로그 메시지
     * @param category 로그 카테고리
     * @param context 추가 컨텍스트 정보
     */
    void LogWithContext(LogLevel level, const std::string& message,
                       DriverLogCategory category, 
                       const DriverLogContext& context);
    
    /**
     * @brief 에러 정보와 함께 로그
     * @param error 에러 정보
     * @param additional_message 추가 메시지 (선택사항)
     */
    void LogError(const ErrorInfo& error, 
                  const std::string& additional_message = "");
    
    /**
     * @brief 연결 상태 변경 로그
     * @param old_status 이전 상태
     * @param new_status 새로운 상태
     * @param reason 상태 변경 이유 (선택사항)
     */
    void LogConnectionStatusChange(ConnectionStatus old_status, 
                                  ConnectionStatus new_status,
                                  const std::string& reason = "");
    
    /**
     * @brief 데이터 송수신 로그
     * @param direction 데이터 방향 ("TX" 또는 "RX")
     * @param data_size 데이터 크기 (바이트)
     * @param duration_ms 처리 시간 (밀리초)
     * @param success 성공 여부
     */
    void LogDataTransfer(const std::string& direction, size_t data_size,
                        int duration_ms, bool success);
    
    /**
     * @brief 성능 메트릭 로그
     * @param metric_name 메트릭 이름
     * @param value 값
     * @param unit 단위
     */
    void LogPerformanceMetric(const std::string& metric_name, 
                             double value, const std::string& unit = "");
    
    // =============================================================================
    // 설정 및 제어 인터페이스
    // =============================================================================
    
    /**
     * @brief 최소 로그 레벨 설정
     * @param level 최소 로그 레벨
     */
    void SetMinLevel(LogLevel level) noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        min_level_ = level;
    }
    
    /**
     * @brief 현재 최소 로그 레벨 반환
     * @return 최소 로그 레벨
     */
    LogLevel GetMinLevel() const noexcept {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        return min_level_;
    }
    
    /**
     * @brief 카테고리별 로그 활성화/비활성화
     * @param category 로그 카테고리
     * @param enabled 활성화 여부
     */
    void SetCategoryEnabled(DriverLogCategory category, bool enabled) {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        category_enabled_[category] = enabled;
    }
    
    /**
     * @brief 카테고리 활성화 상태 확인
     * @param category 로그 카테고리
     * @return 활성화되어 있으면 true
     */
    bool IsCategoryEnabled(DriverLogCategory category) const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        auto it = category_enabled_.find(category);
        return it != category_enabled_.end() ? it->second : true;
    }
    
    /**
     * @brief 기본 컨텍스트 정보 업데이트
     * @param context 새로운 기본 컨텍스트
     */
    void UpdateDefaultContext(const DriverLogContext& context) {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        default_context_ = context;
    }
    
    /**
     * @brief 현재 기본 컨텍스트 반환
     * @return 기본 컨텍스트 정보
     */
    DriverLogContext GetDefaultContext() const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        return default_context_;
    }
    
    // =============================================================================
    // 통계 및 진단 인터페이스
    // =============================================================================
    
    /**
     * @brief 로그 통계 정보 구조체
     */
    struct LogStatistics {
        uint64_t debug_count;       ///< DEBUG 로그 개수
        uint64_t info_count;        ///< INFO 로그 개수
        uint64_t warn_count;        ///< WARN 로그 개수
        uint64_t error_count;       ///< ERROR 로그 개수
        uint64_t fatal_count;       ///< FATAL 로그 개수
        uint64_t total_count;       ///< 전체 로그 개수
        
        /**
         * @brief 기본 생성자
         */
        LogStatistics() : debug_count(0), info_count(0), warn_count(0),
                         error_count(0), fatal_count(0), total_count(0) {}
        
        /**
         * @brief JSON 문자열로 변환
         * @return JSON 형태의 통계 정보
         */
        std::string ToJson() const {
            std::ostringstream oss;
            oss << "{"
                << "\"debug_count\":" << debug_count << ","
                << "\"info_count\":" << info_count << ","
                << "\"warn_count\":" << warn_count << ","
                << "\"error_count\":" << error_count << ","
                << "\"fatal_count\":" << fatal_count << ","
                << "\"total_count\":" << total_count
                << "}";
            return oss.str();
        }
    };
    
    /**
     * @brief 로그 통계 정보 반환
     * @return 로그 통계
     */
    LogStatistics GetStatistics() const noexcept {
        LogStatistics stats;
        stats.debug_count = log_count_debug_.load();
        stats.info_count = log_count_info_.load();
        stats.warn_count = log_count_warn_.load();
        stats.error_count = log_count_error_.load();
        stats.fatal_count = log_count_fatal_.load();
        stats.total_count = stats.debug_count + stats.info_count + 
                           stats.warn_count + stats.error_count + stats.fatal_count;
        return stats;
    }
    
    /**
     * @brief 로그 통계 초기화
     */
    void ResetStatistics() noexcept {
        log_count_debug_.store(0);
        log_count_info_.store(0);
        log_count_warn_.store(0);
        log_count_error_.store(0);
        log_count_fatal_.store(0);
    }
    
    /**
     * @brief 로거 상태 정보를 JSON으로 반환
     * @return JSON 형태의 상태 정보
     */
    std::string GetStatusJson() const {
        std::lock_guard<std::mutex> lock(logger_mutex_);
        std::ostringstream oss;
        oss << "{"
            << "\"device_id\":\"" << default_context_.device_id << "\","
            << "\"protocol\":\"" << ProtocolTypeToString(default_context_.protocol) << "\","
            << "\"endpoint\":\"" << default_context_.endpoint << "\","
            << "\"min_level\":" << static_cast<int>(min_level_) << ","
            << "\"statistics\":" << GetStatistics().ToJson()
            << "}";
        return oss.str();
    }
    
    // =============================================================================
    // 프로토콜별 특수 로깅 함수들
    // =============================================================================
    
    /**
     * @brief Modbus 특화 로그 (함수 코드, 주소, 값 포함)
     * @param function_code Modbus 함수 코드
     * @param address 레지스터/코일 주소
     * @param value 값 (선택사항)
     * @param success 성공 여부
     * @param duration_ms 응답 시간 (밀리초)
     */
    void LogModbusOperation(int function_code, int address, 
                           const std::string& value = "", 
                           bool success = true, int duration_ms = 0);
    
    /**
     * @brief MQTT 특화 로그 (토픽, QoS, 메시지 크기 포함)
     * @param operation 동작 ("PUBLISH", "SUBSCRIBE", "CONNECT" 등)
     * @param topic MQTT 토픽
     * @param qos QoS 레벨
     * @param message_size 메시지 크기 (바이트)
     * @param success 성공 여부
     */
    void LogMqttOperation(const std::string& operation, 
                         const std::string& topic,
                         int qos = 0, size_t message_size = 0, 
                         bool success = true);
    
    /**
     * @brief BACnet 특화 로그 (객체 타입, 인스턴스, 속성 포함)
     * @param service_choice BACnet 서비스 선택
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_identifier 속성 식별자
     * @param success 성공 여부
     */
    void LogBacnetOperation(const std::string& service_choice,
                           int object_type, int object_instance,
                           int property_identifier, bool success = true);
    
    // =============================================================================
    // 헬퍼 함수들 (템플릿 기반 로깅)
    // =============================================================================
    
    /**
     * @brief 템플릿 기반 포맷된 로깅
     * @tparam Args 가변 인자 타입들
     * @param level 로그 레벨
     * @param category 로그 카테고리
     * @param format 포맷 문자열
     * @param args 가변 인자들
     */
    template<typename... Args>
    void LogFormatted(LogLevel level, DriverLogCategory category,
                     const std::string& format, Args&&... args) {
        if (!ShouldLog(level, category)) {
            return;
        }
        
        try {
            // 간단한 문자열 포맷팅 (printf 스타일)
            char buffer[1024];
            std::snprintf(buffer, sizeof(buffer), format.c_str(), args...);
            
            switch (level) {
                case LogLevel::DEBUG: Debug(buffer, category); break;
                case LogLevel::INFO:  Info(buffer, category); break;
                case LogLevel::WARN:  Warn(buffer, category); break;
                case LogLevel::ERROR: Error(buffer, category); break;
                case LogLevel::FATAL: Fatal(buffer, category); break;
            }
        } catch (const std::exception& e) {
            // 포맷팅 에러 시 원본 메시지라도 출력
            Error("Log formatting error: " + std::string(e.what()) + 
                  ", Original format: " + format, DriverLogCategory::ERROR_HANDLING);
        }
    }
    
    /**
     * @brief 조건부 로깅
     * @param condition 로그 출력 조건
     * @param level 로그 레벨
     * @param message 로그 메시지
     * @param category 로그 카테고리
     */
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
    /**
     * @brief 로그 출력 여부 판단
     * @param level 로그 레벨
     * @param category 로그 카테고리
     * @return 출력해야 하면 true
     */
    bool ShouldLog(LogLevel level, DriverLogCategory category) const noexcept {
        return (level >= min_level_) && IsCategoryEnabled(category);
    }
    
    /**
     * @brief 통계 카운터 업데이트
     * @param level 로그 레벨
     */
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
// 편의 매크로 정의 (파일, 라인, 함수 정보 자동 포함)
// =============================================================================

/**
 * @brief 드라이버 로거 인스턴스를 얻는 매크로
 * @details 각 드라이버 클래스에서 logger_ 멤버 변수로 사용하는 것을 가정
 */
#define DRIVER_LOG_CONTEXT() \
    [&]() -> DriverLogContext { \
        DriverLogContext ctx = this->logger_.GetDefaultContext(); \
        ctx.function_name = __FUNCTION__; \
        ctx.file_name = __FILE__; \
        ctx.line_number = __LINE__; \
        ctx.thread_id = std::this_thread::get_id(); \
        return ctx; \
    }()

/**
 * @brief 컨텍스트가 포함된 드라이버 로그 매크로들
 */
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

/**
 * @brief 간단한 드라이버 로그 매크로들 (카테고리 생략)
 */
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

/**
 * @brief 조건부 로깅 매크로
 */
#define DRIVER_LOG_IF(logger, condition, level, message, category) \
    do { \
        if (condition) { \
            DRIVER_LOG_##level(logger, message, category); \
        } \
    } while(0)

/**
 * @brief 성능 측정 매크로
 */
#define DRIVER_LOG_DURATION(logger, name, code_block) \
    do { \
        auto start_time = std::chrono::high_resolution_clock::now(); \
        code_block; \
        auto end_time = std::chrono::high_resolution_clock::now(); \
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
            end_time - start_time).count(); \
        (logger).LogPerformanceMetric(name, static_cast<double>(duration_ms), "ms"); \
    } while(0)

/**
 * @brief 함수 진입/종료 추적 매크로 (RAII 기반)
 */
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