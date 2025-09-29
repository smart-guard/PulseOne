/**
 * @file CSPDynamicTargets.h
 * @brief CSP 동적 타겟 관련 모든 타입 정의 - 완전 통합 (컴파일 에러 해결)
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 3.0.0 (모든 타입 통합 + 타입 정의 순서 수정)
 * 저장 위치: core/export-gateway/include/CSP/CSPDynamicTargets.h
 * 
 * 🎯 모든 CSP Gateway 관련 타입들을 이 파일 하나에 정의:
 * - TargetSendResult (전송 결과) - ITargetHandler보다 먼저 정의
 * - FailureProtectorConfig/Stats (Circuit Breaker)
 * - DynamicTarget (타겟 정보 - atomic 복사 문제 해결)
 * - ITargetHandler (공통 인터페이스)
 * - TargetHandlerFactory (팩토리)
 * - BatchTargetResult (배치 처리)
 * - 유틸리티 함수들
 */

#ifndef CSP_DYNAMIC_TARGETS_H
#define CSP_DYNAMIC_TARGETS_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include "AlarmMessage.h"

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// 전방 선언
// =============================================================================
class ITargetHandler;
class DynamicTargetManager;
class TargetHandlerFactory;
class FailureProtector;

// =============================================================================
// 타겟 전송 결과 (ITargetHandler보다 먼저 정의해야 함)
// =============================================================================

/**
 * @brief 타겟 전송 결과 - 모든 Handler에서 공통 사용
 */
struct TargetSendResult {
    // 기본 결과 필드들
    bool success = false;
    std::string error_message = "";
    std::chrono::milliseconds response_time{0};
    size_t content_size = 0;
    int retry_count = 0;
    
    // 타겟 정보
    std::string target_name = "";
    std::string target_type = "";
    
    // HTTP 관련 필드들
    int status_code = 0;
    std::string response_body = "";
    
    // 파일/경로 관련 필드들
    std::string file_path = "";       // 파일 타겟용
    std::string s3_object_key = "";   // S3 타겟용  
    std::string mqtt_topic = "";      // MQTT 타겟용
    
    // 타임스탬프
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    
    // 편의 생성자들
    TargetSendResult() = default;
    
    TargetSendResult(const std::string& name, const std::string& type, bool result)
        : target_name(name), target_type(type), success(result) {}
    
    // 상태 확인 메서드들
    bool isHttpSuccess() const {
        return status_code >= 200 && status_code < 300;
    }
    
    bool isClientError() const {
        return status_code >= 400 && status_code < 500;
    }
    
    bool isServerError() const {
        return status_code >= 500 && status_code < 600;
    }
    
    // JSON 변환
    json toJson() const {
        return json{
            {"success", success},
            {"error_message", error_message},
            {"response_time_ms", response_time.count()},
            {"content_size", content_size},
            {"retry_count", retry_count},
            {"target_name", target_name},
            {"target_type", target_type},
            {"status_code", status_code},
            {"response_body", response_body},
            {"file_path", file_path},
            {"s3_object_key", s3_object_key},
            {"mqtt_topic", mqtt_topic}
        };
    }
};

// =============================================================================
// 실패 방지기 관련 타입들 (Circuit Breaker) - ITargetHandler보다 먼저 정의
// =============================================================================

/**
 * @brief 실패 방지기 설정
 */
struct FailureProtectorConfig {
    uint32_t failure_threshold = 5;                // 실패 임계치
    uint32_t recovery_timeout_ms = 60000;          // 복구 대기 시간 (밀리초)
    uint32_t half_open_max_attempts = 3;           // HALF_OPEN에서 최대 시도 횟수
    uint32_t half_open_success_threshold = 2;      // HALF_OPEN에서 CLOSED로 전환하기 위한 성공 횟수
    double backoff_multiplier = 2.0;               // 백오프 배수
    uint32_t max_recovery_timeout_ms = 1800000;    // 최대 복구 대기 시간 (30분)
    
    // 편의 생성자
    FailureProtectorConfig() = default;
    
    FailureProtectorConfig(uint32_t threshold, uint32_t timeout_ms, uint32_t max_attempts)
        : failure_threshold(threshold)
        , recovery_timeout_ms(timeout_ms)
        , half_open_max_attempts(max_attempts) {}
};

/**
 * @brief 실패 방지기 통계
 */
struct FailureProtectorStats {
    std::string target_name;
    std::string current_state;          // "CLOSED", "OPEN", "HALF_OPEN"
    uint32_t failure_count = 0;
    uint32_t success_count = 0;
    uint32_t total_attempts = 0;
    uint32_t total_successes = 0;
    uint32_t total_failures = 0;
    uint32_t half_open_attempts = 0;
    double success_rate = 0.0;
    int64_t state_duration_ms = 0;
    
    // JSON 변환
    json toJson() const {
        return json{
            {"target_name", target_name},
            {"current_state", current_state},
            {"failure_count", failure_count},
            {"success_count", success_count},
            {"total_attempts", total_attempts},
            {"total_successes", total_successes},
            {"total_failures", total_failures},
            {"half_open_attempts", half_open_attempts},
            {"success_rate", success_rate},
            {"state_duration_ms", state_duration_ms}
        };
    }
};

// =============================================================================
// 타겟 핸들러 인터페이스 (TargetSendResult가 정의된 후에 배치)
// =============================================================================

/**
 * @brief 타겟 핸들러 공통 인터페이스
 */
class ITargetHandler {
public:
    virtual ~ITargetHandler() = default;
    
    // 필수 메서드들 (이제 TargetSendResult가 정의되어 있음)
    virtual TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) = 0;
    virtual bool testConnection(const json& config) = 0;
    virtual std::string getHandlerType() const = 0;
    virtual bool validateConfig(const json& config, std::vector<std::string>& errors) = 0;
    
    // 선택적 메서드들 (기본 구현 제공)
    virtual bool initialize(const json& config) { return true; }
    virtual void cleanup() { /* 기본: 아무 작업 없음 */ }
    virtual json getStatus() const {
        return json{{"type", getHandlerType()}, {"status", "active"}};
    }
};

// =============================================================================
// 동적 타겟 정보 (atomic 복사/이동 문제 완전 해결)
// =============================================================================

/**
 * @brief 동적 타겟 정보 - atomic 멤버 복사/이동 문제 해결
 */
struct DynamicTarget {
    // 기본 설정 필드들 (복사 가능)
    std::string name;
    std::string type;
    bool enabled = true;
    int priority = 100;
    std::string description;
    json config;
    
    // 런타임 상태 (atomic 멤버들)
    mutable std::atomic<bool> healthy{true};
    mutable std::atomic<size_t> success_count{0};
    mutable std::atomic<size_t> failure_count{0};
    mutable std::atomic<size_t> consecutive_failures{0};
    mutable std::atomic<double> avg_response_time_ms{0.0};
    mutable std::atomic<size_t> total_bytes_sent{0};
    mutable std::atomic<size_t> total_retries{0};
    
    // 시간 정보 (복사 가능)
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point created_time;
    
    // 🔥 기본 생성자
    DynamicTarget() {
        auto now = std::chrono::system_clock::now();
        last_success_time = now;
        last_failure_time = now;
        created_time = now;
    }
    
    // 🔥 복사 생성자 (atomic 멤버들 처리)
    DynamicTarget(const DynamicTarget& other) 
        : name(other.name)
        , type(other.type)
        , enabled(other.enabled)
        , priority(other.priority)
        , description(other.description)
        , config(other.config)
        , healthy(other.healthy.load())                    // atomic 값 로드
        , success_count(other.success_count.load())
        , failure_count(other.failure_count.load())
        , consecutive_failures(other.consecutive_failures.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , total_bytes_sent(other.total_bytes_sent.load())
        , total_retries(other.total_retries.load())
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , created_time(other.created_time) {}
    
    // 🔥 이동 생성자 (atomic 멤버들 처리)
    DynamicTarget(DynamicTarget&& other) noexcept
        : name(std::move(other.name))
        , type(std::move(other.type))
        , enabled(other.enabled)
        , priority(other.priority)
        , description(std::move(other.description))
        , config(std::move(other.config))
        , healthy(other.healthy.load())                    // atomic 값 로드
        , success_count(other.success_count.load())
        , failure_count(other.failure_count.load())
        , consecutive_failures(other.consecutive_failures.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , total_bytes_sent(other.total_bytes_sent.load())
        , total_retries(other.total_retries.load())
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , created_time(other.created_time) {}
    
    // 🔥 복사 대입 연산자
    DynamicTarget& operator=(const DynamicTarget& other) {
        if (this != &other) {
            name = other.name;
            type = other.type;
            enabled = other.enabled;
            priority = other.priority;
            description = other.description;
            config = other.config;
            healthy.store(other.healthy.load());             // atomic 값 저장
            success_count.store(other.success_count.load());
            failure_count.store(other.failure_count.load());
            consecutive_failures.store(other.consecutive_failures.load());
            avg_response_time_ms.store(other.avg_response_time_ms.load());
            total_bytes_sent.store(other.total_bytes_sent.load());
            total_retries.store(other.total_retries.load());
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            created_time = other.created_time;
        }
        return *this;
    }
    
    // 🔥 이동 대입 연산자
    DynamicTarget& operator=(DynamicTarget&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            type = std::move(other.type);
            enabled = other.enabled;
            priority = other.priority;
            description = std::move(other.description);
            config = std::move(other.config);
            healthy.store(other.healthy.load());             // atomic 값 저장
            success_count.store(other.success_count.load());
            failure_count.store(other.failure_count.load());
            consecutive_failures.store(other.consecutive_failures.load());
            avg_response_time_ms.store(other.avg_response_time_ms.load());
            total_bytes_sent.store(other.total_bytes_sent.load());
            total_retries.store(other.total_retries.load());
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            created_time = other.created_time;
        }
        return *this;
    }
    
    // 성공률 계산
    double getSuccessRate() const {
        size_t total = success_count.load() + failure_count.load();
        return (total > 0) ? 
            (static_cast<double>(success_count.load()) / total * 100.0) : 0.0;
    }
    
    // JSON 변환
    json toJson() const {
        return json{
            {"name", name},
            {"type", type},
            {"enabled", enabled},
            {"priority", priority},
            {"description", description},
            {"healthy", healthy.load()},
            {"success_count", success_count.load()},
            {"failure_count", failure_count.load()},
            {"consecutive_failures", consecutive_failures.load()},
            {"success_rate", getSuccessRate()},
            {"avg_response_time_ms", avg_response_time_ms.load()},
            {"total_bytes_sent", total_bytes_sent.load()},
            {"total_retries", total_retries.load()},
            {"config", config}
        };
    }
};

// =============================================================================
// 타겟 핸들러 팩토리
// =============================================================================

using TargetHandlerCreator = std::function<std::unique_ptr<ITargetHandler>()>;

/**
 * @brief 타겟 핸들러 팩토리 (싱글톤)
 */
class TargetHandlerFactory {
private:
    mutable std::mutex factory_mutex_;
    std::map<std::string, TargetHandlerCreator> creators_;
    
    TargetHandlerFactory() = default;
    
public:
    static TargetHandlerFactory& getInstance() {
        static TargetHandlerFactory instance;
        return instance;
    }
    
    // 복사/이동 생성자 비활성화
    TargetHandlerFactory(const TargetHandlerFactory&) = delete;
    TargetHandlerFactory& operator=(const TargetHandlerFactory&) = delete;
    TargetHandlerFactory(TargetHandlerFactory&&) = delete;
    TargetHandlerFactory& operator=(TargetHandlerFactory&&) = delete;
    
    void registerHandler(const std::string& type_name, TargetHandlerCreator creator) {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        creators_[type_name] = creator;
    }
    
    std::unique_ptr<ITargetHandler> createHandler(const std::string& type_name) {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        auto it = creators_.find(type_name);
        return (it != creators_.end()) ? it->second() : nullptr;
    }
    
    std::vector<std::string> getSupportedTypes() const {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        std::vector<std::string> types;
        for (const auto& [type, _] : creators_) {
            types.push_back(type);
        }
        return types;
    }
};

// =============================================================================
// 배치 처리 결과
// =============================================================================

/**
 * @brief 배치 타겟 처리 결과
 */
struct BatchTargetResult {
    std::vector<TargetSendResult> results;
    size_t total_targets = 0;
    size_t successful_targets = 0;
    size_t failed_targets = 0;
    std::chrono::milliseconds total_time{0};
    
    // 편의 메서드들
    double getSuccessRate() const {
        return (total_targets > 0) ? 
            (static_cast<double>(successful_targets) / total_targets * 100.0) : 0.0;
    }
    
    json toJson() const {
        json result_array = json::array();
        for (const auto& result : results) {
            result_array.push_back(result.toJson());
        }
        
        return json{
            {"results", result_array},
            {"total_targets", total_targets},
            {"successful_targets", successful_targets},
            {"failed_targets", failed_targets},
            {"success_rate", getSuccessRate()},
            {"total_time_ms", total_time.count()}
        };
    }
};

// =============================================================================
// 유틸리티 매크로
// =============================================================================

/**
 * @brief 핸들러 등록 매크로
 */
#define REGISTER_TARGET_HANDLER(type_name, handler_class) \
    static bool register_##handler_class = []() { \
        TargetHandlerFactory::getInstance().registerHandler( \
            type_name, []() -> std::unique_ptr<ITargetHandler> { \
                return std::make_unique<handler_class>(); \
            }); \
        return true; \
    }()

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * @brief 알람 메시지 유효성 검증
 */
inline bool isValidAlarmMessage(const AlarmMessage& alarm) {
    return !alarm.device_name.empty() && !alarm.alarm_type.empty();
}

/**
 * @brief 타겟 설정 유효성 검증
 */
inline bool isValidTargetConfig(const json& config, const std::string& target_type) {
    if (config.empty()) return false;
    
    if (target_type == "HTTP") {
        return config.contains("url") && config["url"].is_string();
    } else if (target_type == "S3") {
        return config.contains("bucket") && config.contains("access_key") && 
               config.contains("secret_key");
    } else if (target_type == "FILE") {
        return config.contains("base_path") && config["base_path"].is_string();
    }
    
    return true; // 기본적으로 허용
}

// =============================================================================
// 전방 선언 (호환성)
// =============================================================================

class HttpTargetHandler;
class S3TargetHandler;
class MqttTargetHandler;
class FileTargetHandler;
class CSPGatewayEnhanced;

} // namespace CSP
} // namespace PulseOne

#endif // CSP_DYNAMIC_TARGETS_H