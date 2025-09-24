/**
 * @file CSPDynamicTargets.h
 * @brief CSP 동적 타겟 관련 타입 및 구조체 정의 - TargetSendResult 중복 제거
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 2.2.0 (TargetSendResult 중복 제거, 통합 타입 사용)
 * 저장 위치: core/export-gateway/include/CSP/CSPDynamicTargets.h
 */

#ifndef CSP_DYNAMIC_TARGETS_H
#define CSP_DYNAMIC_TARGETS_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include "AlarmMessage.h"
#include "TargetTypes.h"  // ✅ 통합 타입 사용

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// ✅ TargetSendResult 정의 제거 - TargetTypes.h에서 import

// =============================================================================
// 전방 선언
// =============================================================================

class ITargetHandler;
class DynamicTargetManager;
class FailureProtector;

// =============================================================================
// 동적 타겟 구조체 - atomic 복사/이동 문제 해결
// =============================================================================

/**
 * @brief 동적 타겟 정보 (atomic 멤버 복사/이동 문제 해결)
 */
struct DynamicTarget {
    // 기본 설정 필드들
    std::string name;
    std::string type;
    bool enabled = true;
    int priority = 100;
    std::string description;
    json config;
    
    // 런타임 상태 (atomic 멤버들)
    std::atomic<bool> healthy{true};
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> failure_count{0};
    std::atomic<double> avg_response_time_ms{0.0};
    std::atomic<size_t> total_bytes_sent{0};
    std::atomic<size_t> consecutive_failures{0};
    std::atomic<size_t> total_retries{0};
    
    // 시간 필드들 (atomic이 아님)
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point created_time;
    
    /**
     * @brief 기본 생성자
     */
    DynamicTarget() {
        auto now = std::chrono::system_clock::now();
        last_success_time = now;
        last_failure_time = now;
        created_time = now;
    }
    
    /**
     * @brief 복사 생성자 - atomic 값들을 load/store로 처리
     */
    DynamicTarget(const DynamicTarget& other) 
        : name(other.name)
        , type(other.type)
        , enabled(other.enabled)
        , priority(other.priority)
        , description(other.description)
        , config(other.config)
        , healthy(other.healthy.load())
        , success_count(other.success_count.load())
        , failure_count(other.failure_count.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , total_bytes_sent(other.total_bytes_sent.load())
        , consecutive_failures(other.consecutive_failures.load())
        , total_retries(other.total_retries.load())
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , created_time(other.created_time) {
    }
    
    /**
     * @brief 이동 생성자 - atomic 값들을 load/store로 처리
     */
    DynamicTarget(DynamicTarget&& other) noexcept
        : name(std::move(other.name))
        , type(std::move(other.type))
        , enabled(other.enabled)
        , priority(other.priority)
        , description(std::move(other.description))
        , config(std::move(other.config))
        , healthy(other.healthy.load())
        , success_count(other.success_count.load())
        , failure_count(other.failure_count.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , total_bytes_sent(other.total_bytes_sent.load())
        , consecutive_failures(other.consecutive_failures.load())
        , total_retries(other.total_retries.load())
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , created_time(other.created_time) {
    }
    
    /**
     * @brief 복사 할당 연산자 - atomic 값들을 load/store로 처리
     */
    DynamicTarget& operator=(const DynamicTarget& other) {
        if (this != &other) {
            name = other.name;
            type = other.type;
            enabled = other.enabled;
            priority = other.priority;
            description = other.description;
            config = other.config;
            
            // atomic 멤버들은 load/store 사용
            healthy.store(other.healthy.load());
            success_count.store(other.success_count.load());
            failure_count.store(other.failure_count.load());
            avg_response_time_ms.store(other.avg_response_time_ms.load());
            total_bytes_sent.store(other.total_bytes_sent.load());
            consecutive_failures.store(other.consecutive_failures.load());
            total_retries.store(other.total_retries.load());
            
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            created_time = other.created_time;
        }
        return *this;
    }
    
    /**
     * @brief 이동 할당 연산자 - atomic 값들을 load/store로 처리
     */
    DynamicTarget& operator=(DynamicTarget&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            type = std::move(other.type);
            enabled = other.enabled;
            priority = other.priority;
            description = std::move(other.description);
            config = std::move(other.config);
            
            // atomic 멤버들은 load/store 사용
            healthy.store(other.healthy.load());
            success_count.store(other.success_count.load());
            failure_count.store(other.failure_count.load());
            avg_response_time_ms.store(other.avg_response_time_ms.load());
            total_bytes_sent.store(other.total_bytes_sent.load());
            consecutive_failures.store(other.consecutive_failures.load());
            total_retries.store(other.total_retries.load());
            
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            created_time = other.created_time;
        }
        return *this;
    }
    
    /**
     * @brief 우선순위 비교 (낮은 숫자가 높은 우선순위)
     */
    bool operator<(const DynamicTarget& other) const {
        return priority < other.priority;
    }
    
    /**
     * @brief 성공률 계산
     */
    double getSuccessRate() const {
        size_t total = success_count.load() + failure_count.load();
        return total > 0 ? 
            (static_cast<double>(success_count.load()) / total * 100.0) : 0.0;
    }
    
    /**
     * @brief JSON 변환
     */
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
            {"success_rate", getSuccessRate()},
            {"avg_response_time_ms", avg_response_time_ms.load()},
            {"total_bytes_sent", total_bytes_sent.load()},
            {"consecutive_failures", consecutive_failures.load()},
            {"config", config}
        };
    }
};

// =============================================================================
// 타겟 핸들러 인터페이스
// =============================================================================

/**
 * @brief 타겟 핸들러 인터페이스
 */
class ITargetHandler {
public:
    virtual ~ITargetHandler() = default;
    
    /**
     * @brief 알람 전송
     * @param alarm 알람 메시지
     * @param config 타겟별 설정
     * @return 전송 결과
     */
    virtual TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) = 0;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟 설정
     * @return 연결 성공 여부
     */
    virtual bool testConnection(const json& config) = 0;
    
    /**
     * @brief 핸들러 타입 반환
     * @return 핸들러 타입 문자열
     */
    virtual std::string getHandlerType() const = 0;
    
    /**
     * @brief 설정 유효성 검증
     * @param config 검증할 설정
     * @param errors 에러 메시지 목록 (출력)
     * @return 유효하면 true
     */
    virtual bool validateConfig(const json& config, std::vector<std::string>& errors) = 0;
};

// =============================================================================
// 타겟 핸들러 팩토리
// =============================================================================

/**
 * @brief 타겟 핸들러 생성 함수 타입
 */
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
    
    /**
     * @brief 핸들러 등록
     * @param type_name 타입 이름
     * @param creator 생성 함수
     */
    void registerHandler(const std::string& type_name, TargetHandlerCreator creator) {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        creators_[type_name] = creator;
    }
    
    /**
     * @brief 핸들러 생성
     * @param type_name 타입 이름
     * @return 생성된 핸들러 (실패 시 nullptr)
     */
    std::unique_ptr<ITargetHandler> createHandler(const std::string& type_name) {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        auto it = creators_.find(type_name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    /**
     * @brief 지원하는 타입 목록 반환
     */
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
// 전방 선언
// =============================================================================

class HttpTargetHandler;
class S3TargetHandler;
class MqttTargetHandler;
class FileTargetHandler;
class FailureProtector;
class CSPGatewayEnhanced;

} // namespace CSP
} // namespace PulseOne

#endif // CSP_DYNAMIC_TARGETS_H