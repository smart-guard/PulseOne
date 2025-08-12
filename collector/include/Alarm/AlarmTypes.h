// =============================================================================
// collector/include/Alarm/AlarmTypes.h
// PulseOne 알람 시스템 - 공통 타입 정의 (컴파일 에러 완전 해결)
// =============================================================================

#ifndef ALARM_TYPES_H
#define ALARM_TYPES_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <nlohmann/json.hpp>

// 🔥 기존 프로젝트 타입들 import (필수!)
#include "Common/BasicTypes.h"

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 🎯 기존 프로젝트 타입 별칭들 (추가된 부분)
// =============================================================================
using UUID = PulseOne::BasicTypes::UUID;
using Timestamp = PulseOne::BasicTypes::Timestamp;
using DataValue = PulseOne::BasicTypes::DataVariant;
using JsonType = nlohmann::json;

// =============================================================================
// 🎯 열거형 타입들 (기존 유지 + 누락된 값 추가)
// =============================================================================

// 알람 타입 정의 (COMMUNICATION, QUALITY 추가!)
enum class AlarmType : uint8_t {
    ANALOG = 0,
    DIGITAL = 1,
    SCRIPT = 2,
    COMPOUND = 3,
    COMMUNICATION = 4,   // 🔥 추가됨
    QUALITY = 5          // 🔥 추가됨
};

// 알람 심각도
enum class AlarmSeverity : uint8_t {
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3,
    INFO = 4
};

// 알람 상태
enum class AlarmState : uint8_t {
    INACTIVE = 0,
    ACTIVE = 1,
    ACKNOWLEDGED = 2,
    CLEARED = 3,
    SUPPRESSED = 4,
    SHELVED = 5
};

// 디지털 트리거
enum class DigitalTrigger : uint8_t {
    ON_TRUE = 0,
    ON_FALSE = 1,
    ON_CHANGE = 2,
    ON_RISING = 3,
    ON_FALLING = 4
};

// 대상 타입
enum class TargetType : uint8_t {
    UNKNOWN = 255,
    DATA_POINT = 0,     // 데이터 포인트
    VIRTUAL_POINT = 1,  // 가상 포인트
    GROUP = 2           // 그룹
};

// 아날로그 레벨
enum class AnalogAlarmLevel : uint8_t {
    NORMAL = 0,
    LOW_LOW = 1,
    LOW = 2,
    HIGH = 3,
    HIGH_HIGH = 4
};

// 에러 코드
enum class AlarmErrorCode : int {
    SUCCESS = 0,
    INVALID_RULE = -1,
    DATABASE_ERROR = -2,
    REDIS_ERROR = -3,
    SCRIPT_ERROR = -4,
    QUEUE_FULL = -5,
    NOT_INITIALIZED = -6
};

enum class TriggerCondition : uint8_t {
    NONE = 0,
    HIGH = 1,
    LOW = 2,
    HIGH_HIGH = 3,
    LOW_LOW = 4,
    DIGITAL_TRUE = 5,
    DIGITAL_FALSE = 6,
    DIGITAL_CHANGE = 7,
    RATE_CHANGE = 8
};

/**
 * @brief 알람 이벤트 구조체
 * @details RabbitMQ로 전송되는 알람 정보
 */    
// =============================================================================
// 🚨 경고 해결된 AlarmEvent 구조체 - PulseOne 알람 시스템 통합 (기존 유지!)
// =============================================================================

struct AlarmEvent {
    // =============================================================================
    // 🎯 핵심 식별자 및 연결 (기존 그대로!)
    // =============================================================================
    UUID device_id;                    // 디바이스 UUID
    int point_id = 0;                  // ✅ 원래대로 int 유지! (TimestampedValue.point_id와 일치)
    int rule_id = 0;                   // 알람 규칙 ID
    int occurrence_id = 0;             // 알람 발생 ID (0 = 신규)
    
    // =============================================================================
    // 🎯 알람 데이터 및 상태 (기존 그대로!)
    // =============================================================================
    DataValue current_value;           // 현재 값
    double threshold_value = 0.0;      // 임계값
    
    // ✅ enum 타입으로 변경 + 변환 함수 제공
    TriggerCondition trigger_condition = TriggerCondition::NONE;
    
    // =============================================================================
    // 🎯 알람 메타데이터 (기존 그대로!)
    // =============================================================================
    AlarmType alarm_type = AlarmType::ANALOG;           // ✅ enum 타입
    std::string message;                                // 알람 메시지
    AlarmSeverity severity = AlarmSeverity::INFO;       // ✅ enum 타입 (기존)
    AlarmState state = AlarmState::ACTIVE;              // ✅ enum 타입
    
    // =============================================================================
    // 🎯 시간 정보 (기존 그대로!)
    // =============================================================================
    Timestamp timestamp;               // 기존 타임스탬프 (현재 시간)
    Timestamp occurrence_time;         // 실제 알람 발생 시간
    
    // =============================================================================
    // 🎯 추가 컨텍스트 정보 (기존 그대로!)
    // =============================================================================
    std::string source_name;           // 소스 이름 (디바이스명 등)
    std::string location;              // 위치 정보
    int tenant_id = 1;                 // 테넌트 ID
    
    DataValue trigger_value;           // ✅ DataValue 타입 유지 (일관성)
    bool condition_met = false;        // bool 타입 유지
    
    // =============================================================================
    // 🎯 생성자들 (기존 그대로!)
    // =============================================================================
    AlarmEvent() : timestamp(std::chrono::system_clock::now()),
                occurrence_time(std::chrono::system_clock::now()) {}
    
    AlarmEvent(const UUID& dev_id, int pt_id, 
            const DataValue& value, AlarmSeverity sev,
            const std::string& msg, AlarmType type = AlarmType::ANALOG) 
        : device_id(dev_id), 
        point_id(pt_id),              // ✅ int 타입
        current_value(value),
        alarm_type(type),             // ✅ enum 직접 할당
        message(msg),
        severity(sev),                // ✅ enum 직접 할당
        state(AlarmState::ACTIVE),    // ✅ enum 직접 할당
        timestamp(std::chrono::system_clock::now()),
        occurrence_time(std::chrono::system_clock::now()) {}
    
    // =============================================================================
    // 🎯 타입 변환 헬퍼 메서드들 (기존 그대로! COMMUNICATION, QUALITY 추가)
    // =============================================================================
    
    /**
     * @brief 심각도를 문자열로 반환
     */
    std::string getSeverityString() const {
        switch(severity) {
            case AlarmSeverity::CRITICAL: return "CRITICAL";
            case AlarmSeverity::HIGH: return "HIGH";
            case AlarmSeverity::MEDIUM: return "MEDIUM";
            case AlarmSeverity::LOW: return "LOW";
            case AlarmSeverity::INFO: return "INFO";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 알람 타입을 문자열로 반환 (COMMUNICATION, QUALITY 추가!)
     */
    std::string getAlarmTypeString() const {
        switch(alarm_type) {
            case AlarmType::ANALOG: return "ANALOG";
            case AlarmType::DIGITAL: return "DIGITAL";
            case AlarmType::COMMUNICATION: return "COMMUNICATION";  // 🔥 추가
            case AlarmType::QUALITY: return "QUALITY";              // 🔥 추가
            case AlarmType::SCRIPT: return "SCRIPT";
            case AlarmType::COMPOUND: return "COMPOUND";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 알람 상태를 문자열로 반환
     */
    std::string getStateString() const {
        switch(state) {
            case AlarmState::ACTIVE: return "ACTIVE";
            case AlarmState::ACKNOWLEDGED: return "ACKNOWLEDGED";
            case AlarmState::CLEARED: return "CLEARED";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 트리거 조건을 문자열로 반환
     */
    std::string getTriggerConditionString() const {
        switch(trigger_condition) {
            case TriggerCondition::HIGH: return "HIGH";
            case TriggerCondition::LOW: return "LOW";
            case TriggerCondition::HIGH_HIGH: return "HIGH_HIGH";
            case TriggerCondition::LOW_LOW: return "LOW_LOW";
            case TriggerCondition::DIGITAL_TRUE: return "DIGITAL_TRUE";
            case TriggerCondition::DIGITAL_FALSE: return "DIGITAL_FALSE";
            case TriggerCondition::DIGITAL_CHANGE: return "DIGITAL_CHANGE";
            case TriggerCondition::RATE_CHANGE: return "RATE_CHANGE";
            case TriggerCondition::NONE: return "NONE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief trigger_value를 문자열로 반환
     */
    std::string getTriggerValueString() const {
        return std::visit([](const auto& v) -> std::string {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
                return v ? "true" : "false";
            } else {
                return std::to_string(v);
            }
        }, trigger_value);
    }
    
    // =============================================================================
    // 🎯 기존 헬퍼 메서드들 (그대로 유지!)
    // =============================================================================
    
    bool isNewOccurrence() const {
        return occurrence_id == 0;
    }
    
    bool isActive() const {
        return state == AlarmState::ACTIVE;
    }
    
    bool isCleared() const {
        return state == AlarmState::CLEARED;
    }
    
    /**
     * @brief 심각도 수치 반환 (정렬용)
     */
    int getSeverityLevel() const {
        switch(severity) {
            case AlarmSeverity::CRITICAL: return 5;
            case AlarmSeverity::HIGH: return 4;
            case AlarmSeverity::MEDIUM: return 3;
            case AlarmSeverity::LOW: return 2;
            case AlarmSeverity::INFO: return 1;
            default: return 0;
        }
    }
    
    // =============================================================================
    // 🎯 JSON 직렬화 (기존 그대로! JsonType 사용)
    // =============================================================================
    std::string ToJSON() const {
        JsonType j;
        
        // 기본 정보
        j["device_id"] = device_id;
        j["point_id"] = point_id;
        j["rule_id"] = rule_id;
        j["occurrence_id"] = occurrence_id;
        
        // enum → string 변환
        j["severity"] = getSeverityString();
        j["alarm_type"] = getAlarmTypeString();
        j["state"] = getStateString();
        j["trigger_condition"] = getTriggerConditionString();
        
        j["message"] = message;
        j["threshold_value"] = threshold_value;
        
        // 추가 정보
        j["source_name"] = source_name;
        j["location"] = location;
        j["tenant_id"] = tenant_id;
        j["condition_met"] = condition_met;
        
        // variant 값 처리
        std::visit([&j](const auto& v) {
            j["current_value"] = v;
        }, current_value);
        
        std::visit([&j](const auto& v) {
            j["trigger_value"] = v;
        }, trigger_value);
        
        // 타임스탬프를 ISO 문자열로 변환
        auto convertTimestamp = [](const Timestamp& ts) -> std::string {
            auto time_t = std::chrono::system_clock::to_time_t(ts);
            std::tm tm_buf;
#ifdef _WIN32
            gmtime_s(&tm_buf, &time_t);
#else
            gmtime_r(&time_t, &tm_buf);
#endif
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
            return std::string(buffer);
        };
        
        j["timestamp"] = convertTimestamp(timestamp);
        j["occurrence_time"] = convertTimestamp(occurrence_time);
        
        return j.dump();
    }
};

// =============================================================================
// 기존 구조체들 모두 그대로 유지!
// =============================================================================

// 알람 규칙 정의 (기존 그대로!)
struct AlarmRule {
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    
    // 대상
    std::string target_type;
    int target_id = 0;
    std::string target_group;
    
    // 타입별 설정
    AlarmType alarm_type = AlarmType::ANALOG;
    
    // 아날로그 설정
    struct AnalogLimits {
        std::optional<double> high_high;
        std::optional<double> high;
        std::optional<double> low;
        std::optional<double> low_low;
        double deadband = 0.0;
    } analog_limits;
    
    // 디지털 설정
    DigitalTrigger digital_trigger = DigitalTrigger::ON_CHANGE;
    std::map<int, std::string> state_messages;
    
    // 스크립트
    std::string condition_script;
    std::string message_script;
    
    // 메시지
    nlohmann::json message_config;
    std::string message_template;
    
    // 우선순위
    AlarmSeverity severity = AlarmSeverity::MEDIUM;
    int priority = 100;
    
    // 자동 처리
    bool auto_acknowledge = false;
    std::chrono::minutes acknowledge_timeout{0};
    bool auto_clear = true;
    
    // 알림
    bool notification_enabled = true;
    std::chrono::seconds notification_delay{0};
    std::vector<std::string> notification_channels;
    
    // 상태
    bool is_enabled = true;
    bool is_latched = false;
    
    // 런타임 상태
    mutable AnalogAlarmLevel last_analog_level = AnalogAlarmLevel::NORMAL;
    mutable bool last_digital_state = false;
    mutable std::optional<double> last_value;
};

// 알람 발생 정보 (기존 그대로!)
struct AlarmOccurrence {
    int64_t id = 0;
    int rule_id = 0;
    int tenant_id = 0;
    
    std::chrono::system_clock::time_point occurrence_time;
    nlohmann::json trigger_value;
    std::string trigger_condition;
    std::string alarm_message;
    AlarmSeverity severity;
    AlarmState state = AlarmState::ACTIVE;
    
    // 확인 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time;
    std::optional<int> acknowledged_by;
    std::string acknowledge_comment;
    
    // 해제 정보
    std::optional<std::chrono::system_clock::time_point> cleared_time;
    std::optional<nlohmann::json> cleared_value;
    
    // 컨텍스트
    nlohmann::json context_data;
};

// 알람 평가 결과 (기존 그대로!)
struct AlarmEvaluation {
    // =============================================================================
    // 🎯 기본 평가 결과 (기존 유지)
    // =============================================================================
    bool should_trigger = false;
    bool should_clear = false;
    bool state_changed = false;
    
    // =============================================================================
    // 🎯 아날로그 알람 레벨 (기존 유지)
    // =============================================================================
    AnalogAlarmLevel analog_level = AnalogAlarmLevel::NORMAL;
    
    // =============================================================================
    // 🎯 조건 및 메시지 (기존 유지)
    // =============================================================================
    std::string condition_met;           // ✅ 기존 string 필드 유지
    std::string message;                 // ✅ 기존 string 필드 유지
    
    // =============================================================================
    // 🎯 심각도 (기존 enum 유지)
    // =============================================================================
    AlarmSeverity severity = AlarmSeverity::MEDIUM;  // ✅ 기존 enum 유지
    
    // =============================================================================
    // 🎯 컨텍스트 데이터 (기존 유지)
    // =============================================================================
    nlohmann::json context_data;        // ✅ 기존 JSON 필드 유지
    
    // =============================================================================
    // 🎯 추가 필드들 - 기존 코드 호환성 (모두 유지)
    // =============================================================================
    std::string triggered_value;        // ✅ 기존 string 필드 유지
    std::string alarm_level;            // ✅ 기존 string 필드 유지
    std::chrono::microseconds evaluation_time{0};  // ✅ 기존 시간 필드 유지
    std::chrono::system_clock::time_point timestamp;  // ✅ 기존 타임스탬프 유지
    int rule_id = 0;                    // ✅ 기존 ID 필드 유지
    int tenant_id = 0;                  // ✅ 기존 테넌트 ID 유지
    
    // =============================================================================
    // 🎯 생성자 (기존 호환성)
    // =============================================================================
    AlarmEvaluation() : timestamp(std::chrono::system_clock::now()) {}
    
    // =============================================================================
    // 🎯 헬퍼 메서드들 (편의성)
    // =============================================================================
    
    /**
     * @brief 심각도를 문자열로 반환
     */
    std::string getSeverityString() const {
        switch(severity) {
            case AlarmSeverity::CRITICAL: return "CRITICAL";
            case AlarmSeverity::HIGH: return "HIGH";
            case AlarmSeverity::MEDIUM: return "MEDIUM";
            case AlarmSeverity::LOW: return "LOW";
            case AlarmSeverity::INFO: return "INFO";
            default: return "MEDIUM";
        }
    }
    
    /**
     * @brief 아날로그 레벨을 문자열로 반환
     */
    std::string getAnalogLevelString() const {
        switch(analog_level) {
            case AnalogAlarmLevel::HIGH_HIGH: return "HIGH_HIGH";
            case AnalogAlarmLevel::HIGH: return "HIGH";
            case AnalogAlarmLevel::NORMAL: return "NORMAL";
            case AnalogAlarmLevel::LOW: return "LOW";
            case AnalogAlarmLevel::LOW_LOW: return "LOW_LOW";
            default: return "NORMAL";
        }
    }
    
    /**
     * @brief condition_met 값이 비어있지 않으면 true 반환
     */
    bool hasConditionMet() const {
        return !condition_met.empty();
    }
};

// =============================================================================
// 기존 구조체들 계속 그대로 유지!
// =============================================================================

// 알람 필터
struct AlarmFilter {
    std::optional<int> tenant_id;
    std::optional<AlarmState> state;
    std::optional<AlarmSeverity> min_severity;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<int> point_id;
    int limit = 100;
    int offset = 0;
};


/**
 * @brief 알람 처리 통계
 * @details 시스템 전체의 알람 처리 현황을 추적
 */
struct AlarmProcessingStats {
    uint64_t total_evaluated = 0;      // 총 평가된 알람 수
    uint64_t total_triggered = 0;      // 총 발생한 알람 수
    uint64_t total_cleared = 0;        // 총 해제된 알람 수
    uint64_t total_acknowledged = 0;   // 총 인지된 알람 수
    
    // 심각도별 통계
    uint64_t critical_count = 0;       // Critical 알람 수
    uint64_t high_count = 0;           // High 알람 수
    uint64_t medium_count = 0;         // Medium 알람 수
    uint64_t low_count = 0;            // Low 알람 수
    uint64_t info_count = 0;           // Info 알람 수
    
    // 타입별 통계
    uint64_t analog_alarms = 0;        // 아날로그 알람 수
    uint64_t digital_alarms = 0;       // 디지털 알람 수
    uint64_t script_alarms = 0;        // 스크립트 알람 수
    
    // 성능 통계
    double avg_evaluation_time_us = 0.0;  // 평균 평가 시간 (마이크로초)
    uint64_t evaluation_errors = 0;       // 평가 에러 수
    
    // 시간 정보
    std::chrono::system_clock::time_point last_alarm_time;
    std::chrono::system_clock::time_point stats_start_time;
    
    /**
     * @brief 기본 생성자 - 시작 시간 초기화
     */
    AlarmProcessingStats() 
        : stats_start_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief 총 활성 알람 수 계산
     */
    uint64_t getTotalActiveAlarms() const {
        return total_triggered - total_cleared;
    }
    
    /**
     * @brief 알람 발생률 계산 (알람/평가)
     */
    double getAlarmRate() const {
        return (total_evaluated > 0) ? 
               static_cast<double>(total_triggered) / static_cast<double>(total_evaluated) : 0.0;
    }
    
    /**
     * @brief 통계를 JSON으로 변환
     */
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["total_evaluated"] = total_evaluated;
        j["total_triggered"] = total_triggered;
        j["total_cleared"] = total_cleared;
        j["total_acknowledged"] = total_acknowledged;
        j["total_active"] = getTotalActiveAlarms();
        
        j["by_severity"] = {
            {"critical", critical_count},
            {"high", high_count},
            {"medium", medium_count},
            {"low", low_count},
            {"info", info_count}
        };
        
        j["by_type"] = {
            {"analog", analog_alarms},
            {"digital", digital_alarms},
            {"script", script_alarms}
        };
        
        j["performance"] = {
            {"avg_evaluation_time_us", avg_evaluation_time_us},
            {"evaluation_errors", evaluation_errors},
            {"alarm_rate", getAlarmRate()}
        };
        
        return j;
    }
};

/**
 * @brief 알람 시스템 상태 정보
 */
struct AlarmSystemStatus {
    bool engine_initialized = false;
    bool repositories_available = false;
    bool script_engine_ready = false;
    
    size_t active_rules_count = 0;
    size_t total_rules_count = 0;
    
    std::chrono::system_clock::time_point last_evaluation_time;
    std::chrono::system_clock::time_point system_start_time;
    
    AlarmSystemStatus() 
        : system_start_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief 시스템 가동 시간 계산
     */
    std::chrono::duration<double> getUptime() const {
        return std::chrono::system_clock::now() - system_start_time;
    }
    
    /**
     * @brief 시스템이 완전히 준비되었는지 확인
     */
    bool isFullyReady() const {
        return engine_initialized && repositories_available && script_engine_ready;
    }
    
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["engine_initialized"] = engine_initialized;
        j["repositories_available"] = repositories_available;
        j["script_engine_ready"] = script_engine_ready;
        j["fully_ready"] = isFullyReady();
        
        j["rules"] = {
            {"active", active_rules_count},
            {"total", total_rules_count}
        };
        
        j["uptime_seconds"] = getUptime().count();
        
        return j;
    }
};

/**
 * @brief 통합 알람 메트릭스
 * @details 모든 알람 관련 메트릭을 포함하는 최상위 구조체
 */
struct AlarmMetrics {
    AlarmProcessingStats processing;
    AlarmSystemStatus system;
    
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["processing"] = processing.toJson();
        j["system"] = system.toJson();
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return j;
    }
};

// 알람 통계
struct AlarmStatistics {
    int total_rules = 0;
    int active_alarms = 0;
    int suppressed_alarms = 0;
    std::map<AlarmSeverity, int> alarms_by_severity;
    std::map<AlarmState, int> alarms_by_state;
    double avg_evaluation_time_us = 0.0;
    int alarms_last_hour = 0;
    int alarms_last_24h = 0;
};

// =============================================================================
// 🎯 타입 변환 헬퍼 함수들 (기존 그대로! COMMUNICATION, QUALITY 추가만)
// =============================================================================

// AlarmType 변환 (COMMUNICATION, QUALITY 추가!)
inline std::string alarmTypeToString(AlarmType type) {
    switch (type) {
        case AlarmType::ANALOG: return "analog";
        case AlarmType::DIGITAL: return "digital";
        case AlarmType::SCRIPT: return "script";
        case AlarmType::COMPOUND: return "compound";
        case AlarmType::COMMUNICATION: return "communication";  // 🔥 추가
        case AlarmType::QUALITY: return "quality";              // 🔥 추가
        default: return "analog";
    }
}

inline AlarmType stringToAlarmType(const std::string& str) {
    if (str == "analog" || str == "ANALOG") return AlarmType::ANALOG;
    if (str == "digital" || str == "DIGITAL") return AlarmType::DIGITAL;
    if (str == "script" || str == "SCRIPT") return AlarmType::SCRIPT;
    if (str == "compound" || str == "COMPOUND") return AlarmType::COMPOUND;
    if (str == "communication" || str == "COMMUNICATION") return AlarmType::COMMUNICATION;  // 🔥 추가
    if (str == "quality" || str == "QUALITY") return AlarmType::QUALITY;                    // 🔥 추가
    return AlarmType::ANALOG;
}

// 나머지 모든 변환 함수들 기존 그대로 유지...
inline std::string severityToString(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::CRITICAL: return "CRITICAL";
        case AlarmSeverity::HIGH: return "HIGH";
        case AlarmSeverity::MEDIUM: return "MEDIUM";
        case AlarmSeverity::LOW: return "LOW";
        case AlarmSeverity::INFO: return "INFO";
        default: return "MEDIUM";
    }
}

inline AlarmSeverity stringToSeverity(const std::string& str) {
    if (str == "CRITICAL" || str == "critical") return AlarmSeverity::CRITICAL;
    if (str == "HIGH" || str == "high") return AlarmSeverity::HIGH;
    if (str == "MEDIUM" || str == "medium") return AlarmSeverity::MEDIUM;
    if (str == "LOW" || str == "low") return AlarmSeverity::LOW;
    if (str == "INFO" || str == "info") return AlarmSeverity::INFO;
    return AlarmSeverity::MEDIUM;
}

// AlarmState 변환
inline std::string stateToString(AlarmState state) {
    switch (state) {
        case AlarmState::INACTIVE: return "INACTIVE";
        case AlarmState::ACTIVE: return "ACTIVE";
        case AlarmState::ACKNOWLEDGED: return "ACKNOWLEDGED";
        case AlarmState::CLEARED: return "CLEARED";
        case AlarmState::SUPPRESSED: return "SUPPRESSED";
        case AlarmState::SHELVED: return "SHELVED";
        default: return "ACTIVE";
    }
}

inline AlarmState stringToState(const std::string& str) {
    if (str == "INACTIVE") return AlarmState::INACTIVE;
    if (str == "ACTIVE") return AlarmState::ACTIVE;
    if (str == "ACKNOWLEDGED") return AlarmState::ACKNOWLEDGED;
    if (str == "CLEARED") return AlarmState::CLEARED;
    if (str == "SUPPRESSED") return AlarmState::SUPPRESSED;
    if (str == "SHELVED") return AlarmState::SHELVED;
    return AlarmState::ACTIVE;
}

// DigitalTrigger 변환
inline std::string digitalTriggerToString(DigitalTrigger trigger) {
    switch (trigger) {
        case DigitalTrigger::ON_TRUE: return "ON_TRUE";
        case DigitalTrigger::ON_FALSE: return "ON_FALSE";
        case DigitalTrigger::ON_CHANGE: return "ON_CHANGE";
        case DigitalTrigger::ON_RISING: return "ON_RISING";
        case DigitalTrigger::ON_FALLING: return "ON_FALLING";
        default: return "ON_CHANGE";
    }
}

inline DigitalTrigger stringToDigitalTrigger(const std::string& str) {
    if (str == "ON_TRUE") return DigitalTrigger::ON_TRUE;
    if (str == "ON_FALSE") return DigitalTrigger::ON_FALSE;
    if (str == "ON_CHANGE") return DigitalTrigger::ON_CHANGE;
    if (str == "ON_RISING") return DigitalTrigger::ON_RISING;
    if (str == "ON_FALLING") return DigitalTrigger::ON_FALLING;
    return DigitalTrigger::ON_CHANGE;
}

// TargetType 변환
inline std::string targetTypeToString(TargetType type) {
    switch (type) {
        case TargetType::DATA_POINT: return "DATA_POINT";
        case TargetType::VIRTUAL_POINT: return "VIRTUAL_POINT";
        case TargetType::GROUP: return "GROUP";
        default: return "DATA_POINT";
    }
}

inline TargetType stringToTargetType(const std::string& str) {
    if (str == "DATA_POINT") return TargetType::DATA_POINT;
    if (str == "VIRTUAL_POINT") return TargetType::VIRTUAL_POINT;
    if (str == "GROUP") return TargetType::GROUP;
    return TargetType::DATA_POINT;
}

// AnalogAlarmLevel 변환
inline std::string analogAlarmLevelToString(AnalogAlarmLevel level) {
    switch (level) {
        case AnalogAlarmLevel::NORMAL: return "NORMAL";
        case AnalogAlarmLevel::LOW_LOW: return "LOW_LOW";
        case AnalogAlarmLevel::LOW: return "LOW";
        case AnalogAlarmLevel::HIGH: return "HIGH";
        case AnalogAlarmLevel::HIGH_HIGH: return "HIGH_HIGH";
        default: return "NORMAL";
    }
}

inline AnalogAlarmLevel stringToAnalogAlarmLevel(const std::string& str) {
    if (str == "NORMAL") return AnalogAlarmLevel::NORMAL;
    if (str == "LOW_LOW") return AnalogAlarmLevel::LOW_LOW;
    if (str == "LOW") return AnalogAlarmLevel::LOW;
    if (str == "HIGH") return AnalogAlarmLevel::HIGH;
    if (str == "HIGH_HIGH") return AnalogAlarmLevel::HIGH_HIGH;
    return AnalogAlarmLevel::NORMAL;
}

// =============================================================================
// 🎯 소문자 변환 헬퍼 함수들 (기존 그대로! COMMUNICATION, QUALITY 추가만)
// =============================================================================

// 소문자 변환 함수들
inline std::string alarmTypeToLowerString(AlarmType type) {
    switch (type) {
        case AlarmType::ANALOG: return "analog";
        case AlarmType::DIGITAL: return "digital"; 
        case AlarmType::SCRIPT: return "script";
        case AlarmType::COMPOUND: return "compound";
        case AlarmType::COMMUNICATION: return "communication";  // 🔥 추가
        case AlarmType::QUALITY: return "quality";              // 🔥 추가
        default: return "analog";
    }
}

inline std::string severityToLowerString(AlarmSeverity severity) {
    switch (severity) {
        case AlarmSeverity::CRITICAL: return "critical";
        case AlarmSeverity::HIGH: return "high";
        case AlarmSeverity::MEDIUM: return "medium";
        case AlarmSeverity::LOW: return "low";
        case AlarmSeverity::INFO: return "info";
        default: return "medium";
    }
}

inline std::string stateToLowerString(AlarmState state) {
    switch (state) {
        case AlarmState::INACTIVE: return "inactive";
        case AlarmState::ACTIVE: return "active";
        case AlarmState::ACKNOWLEDGED: return "acknowledged";
        case AlarmState::CLEARED: return "cleared";
        case AlarmState::SUPPRESSED: return "suppressed";
        case AlarmState::SHELVED: return "shelved";
        default: return "active";
    }
}

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_TYPES_H