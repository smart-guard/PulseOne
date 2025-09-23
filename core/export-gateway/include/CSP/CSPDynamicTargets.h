/**
 * @file CSPDynamicTargets.h
 * @brief CSP Gateway 동적 전송 대상 시스템 - 기본 타입 및 인터페이스
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/CSPDynamicTargets.h
 */

#ifndef CSP_DYNAMIC_TARGETS_H
#define CSP_DYNAMIC_TARGETS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <future>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include "AlarmMessage.h"

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// 전송 결과 구조체
// =============================================================================

/**
 * @brief 타겟 전송 결과
 */
struct TargetSendResult {
    bool success = false;
    int status_code = 0;
    std::string response_body = "";
    std::string error_message = "";
    std::chrono::milliseconds response_time{0};
    std::string target_name = "";
    std::string target_type = "";
    
    // 추가 메타데이터
    std::string file_path = "";           // 파일 타겟용
    std::string s3_object_key = "";       // S3 타겟용
    std::string mqtt_topic = "";          // MQTT 타겟용
    size_t content_size = 0;              // 전송된 데이터 크기
    int retry_count = 0;                  // 실제 재시도 횟수
    
    /**
     * @brief 성공 여부 확인 (상태 코드 기준)
     */
    bool isHttpSuccess() const {
        return status_code >= 200 && status_code < 300;
    }
    
    /**
     * @brief 클라이언트 오류 여부 확인
     */
    bool isClientError() const {
        return status_code >= 400 && status_code < 500;
    }
    
    /**
     * @brief 서버 오류 여부 확인
     */
    bool isServerError() const {
        return status_code >= 500 && status_code < 600;
    }
    
    /**
     * @brief 결과를 JSON으로 변환
     */
    json toJson() const {
        return json{
            {"success", success},
            {"status_code", status_code},
            {"response_body", response_body},
            {"error_message", error_message},
            {"response_time_ms", response_time.count()},
            {"target_name", target_name},
            {"target_type", target_type},
            {"file_path", file_path},
            {"s3_object_key", s3_object_key},
            {"mqtt_topic", mqtt_topic},
            {"content_size", content_size},
            {"retry_count", retry_count}
        };
    }
};

// =============================================================================
// 동적 타겟 구조체
// =============================================================================

/**
 * @brief 동적 타겟 정보
 */
struct DynamicTarget {
    std::string name;
    std::string type;
    bool enabled;
    int priority;
    std::string description;
    json config;
    
    // 런타임 상태
    std::atomic<bool> healthy{true};
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> failure_count{0};
    std::atomic<double> avg_response_time_ms{0.0};
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    std::chrono::system_clock::time_point created_time;
    
    // 추가 메트릭
    std::atomic<size_t> total_bytes_sent{0};
    std::atomic<size_t> consecutive_failures{0};
    std::atomic<size_t> total_retries{0};
    
    /**
     * @brief 생성자
     */
    DynamicTarget() {
        auto now = std::chrono::system_clock::now();
        last_success_time = now;
        last_failure_time = now;
        created_time = now;
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
        return total > 0 ? (static_cast<double>(success_count.load()) / total * 100.0) : 0.0;
    }
    
    /**
     * @brief 현재 상태를 JSON으로 변환
     */
    json getStatusJson() const {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - created_time);
        
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
            {"total_retries", total_retries.load()},
            {"uptime_seconds", uptime.count()}
        };
    }
};

// =============================================================================
// 배치 처리 결과
// =============================================================================

/**
 * @brief 배치 처리 결과 (멀티빌딩 지원)
 */
struct BatchTargetResult {
    int building_id = 0;
    std::vector<TargetSendResult> target_results;
    std::chrono::system_clock::time_point processed_time;
    size_t total_alarms = 0;
    size_t successful_targets = 0;
    size_t failed_targets = 0;
    std::chrono::milliseconds total_processing_time{0};
    
    /**
     * @brief 전체 성공 여부 (모든 타겟 성공)
     */
    bool isCompleteSuccess() const {
        return failed_targets == 0 && successful_targets > 0;
    }
    
    /**
     * @brief 부분 성공 여부 (일부 타겟 성공)
     */
    bool isPartialSuccess() const {
        return successful_targets > 0 && failed_targets > 0;
    }
    
    /**
     * @brief 전체 실패 여부
     */
    bool isCompleteFailure() const {
        return successful_targets == 0;
    }
};

// =============================================================================
// 추상 타겟 핸들러 인터페이스
// =============================================================================

/**
 * @brief 추상 타겟 핸들러 인터페이스
 */
class ITargetHandler {
public:
    virtual ~ITargetHandler() = default;
    
    /**
     * @brief 핸들러 초기화
     * @param config 설정 객체
     * @return 초기화 성공 여부
     */
    virtual bool initialize(const json& config) = 0;
    
    /**
     * @brief 알람 메시지 전송
     * @param alarm 전송할 알람 메시지
     * @param config 타겟별 설정
     * @return 전송 결과
     */
    virtual TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) = 0;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 연결 성공 여부
     */
    virtual bool testConnection(const json& config) = 0;
    
    /**
     * @brief 핸들러 타입 이름 반환
     * @return 타입 이름 (예: "http", "s3", "mqtt", "file")
     */
    virtual std::string getTypeName() const = 0;
    
    /**
     * @brief 핸들러 정리 (선택적 구현)
     */
    virtual void cleanup() {}
    
    /**
     * @brief 핸들러 상태 확인 (선택적 구현)
     * @return 상태 정보
     */
    virtual json getStatus() const {
        return json{
            {"type", getTypeName()},
            {"status", "unknown"}
        };
    }
};

// =============================================================================
// 타겟 핸들러 팩토리
// =============================================================================

/**
 * @brief 타겟 핸들러 생성 함수 타입
 */
using TargetHandlerCreator = std::function<std::unique_ptr<ITargetHandler>()>;

/**
 * @brief 타겟 핸들러 팩토리
 */
class TargetHandlerFactory {
private:
    std::unordered_map<std::string, TargetHandlerCreator> creators_;
    mutable std::mutex factory_mutex_;
    
public:
    /**
     * @brief 싱글톤 인스턴스 반환
     */
    static TargetHandlerFactory& getInstance() {
        static TargetHandlerFactory instance;
        return instance;
    }
    
    /**
     * @brief 핸들러 크리에이터 등록
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
class DynamicTargetManager;
class CSPGatewayEnhanced;

} // namespace CSP
} // namespace PulseOne

#endif // CSP_DYNAMIC_TARGETS_H