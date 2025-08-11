// =============================================================================
// collector/include/Alarm/AlarmTypes.h
// PulseOne 알람 시스템 - 공통 타입 정의 (중복 제거 완료)
// =============================================================================

#ifndef ALARM_TYPES_H
#define ALARM_TYPES_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 🎯 열거형 타입들
// =============================================================================

// 알람 타입 정의
enum class AlarmType : uint8_t {
    ANALOG = 0,
    DIGITAL = 1,
    SCRIPT = 2,
    COMPOUND = 3
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

// =============================================================================
// 🎯 구조체들 (정확히 한 번만 정의)
// =============================================================================

// 알람 규칙 정의
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

// 알람 발생 정보
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

// =============================================================================
// 🚨 알람 평가 결과 (단 한 번만 정의!)
// =============================================================================
struct AlarmEvaluation {
    bool should_trigger = false;
    bool should_clear = false;
    bool state_changed = false;
    
    AnalogAlarmLevel analog_level = AnalogAlarmLevel::NORMAL;
    std::string condition_met;
    std::string message;
    AlarmSeverity severity = AlarmSeverity::MEDIUM;
    nlohmann::json context_data;
    
    // 추가 필드들 (기존 코드 호환성)
    std::string triggered_value;
    std::string alarm_level;
    std::chrono::microseconds evaluation_time{0};
    std::chrono::system_clock::time_point timestamp;
    int rule_id = 0;
    int tenant_id = 0;
};

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
// 🎯 타입 변환 헬퍼 함수들
// =============================================================================

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
    if (str == "CRITICAL") return AlarmSeverity::CRITICAL;
    if (str == "HIGH") return AlarmSeverity::HIGH;
    if (str == "MEDIUM") return AlarmSeverity::MEDIUM;
    if (str == "LOW") return AlarmSeverity::LOW;
    if (str == "INFO") return AlarmSeverity::INFO;
    return AlarmSeverity::MEDIUM;
}

inline std::string alarmTypeToString(AlarmType type) {
    switch (type) {
        case AlarmType::ANALOG: return "ANALOG";
        case AlarmType::DIGITAL: return "DIGITAL";
        case AlarmType::SCRIPT: return "SCRIPT";
        case AlarmType::COMPOUND: return "COMPOUND";
        default: return "ANALOG";
    }
}

inline AlarmType stringToAlarmType(const std::string& str) {
    if (str == "ANALOG") return AlarmType::ANALOG;
    if (str == "DIGITAL") return AlarmType::DIGITAL;
    if (str == "SCRIPT") return AlarmType::SCRIPT;
    if (str == "COMPOUND") return AlarmType::COMPOUND;
    return AlarmType::ANALOG;
}

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

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_TYPES_H