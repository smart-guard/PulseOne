// =============================================================================
// collector/include/Alarm/AlarmTypes.h
// PulseOne 알람 시스템 - 공통 타입 정의 (완성본)
// =============================================================================

#ifndef ALARM_TYPES_H
#define ALARM_TYPES_H

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// 기존 프로젝트 타입들 import
#include "Common/BasicTypes.h"

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 기존 프로젝트 타입 별칭들
// =============================================================================
using UniqueId = PulseOne::BasicTypes::UniqueId;
using Timestamp = PulseOne::BasicTypes::Timestamp;
using DataValue = PulseOne::BasicTypes::DataVariant;
using JsonType = nlohmann::json;

// =============================================================================
// 열거형 타입들 - enum 값 수정 완료
// =============================================================================

// 알람 타입 정의
enum class AlarmType : uint8_t {
  ANALOG = 0,
  DIGITAL = 1,
  SCRIPT = 2,
  COMPOUND = 3,
  COMMUNICATION = 4,
  QUALITY = 5
};

// 알람 심각도 - 백엔드 표준에 맞게 수정
enum class AlarmSeverity : uint8_t {
  INFO = 0,    // 0=INFO (가장 낮은 심각도)
  LOW = 1,     // 1=LOW
  MEDIUM = 2,  // 2=MEDIUM (기본값)
  HIGH = 3,    // 3=HIGH
  CRITICAL = 4 // 4=CRITICAL (가장 높은 심각도)
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
  DATA_POINT = 0,
  VIRTUAL_POINT = 1,
  GROUP = 2
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

// =============================================================================
// AlarmEvent 구조체 - 수정된 enum 기본값 적용
// =============================================================================

struct AlarmEvent {
  // 핵심 식별자 및 연결
  UniqueId device_id;
  int point_id = 0;
  int rule_id = 0;
  int occurrence_id = 0;

  // 알람 데이터 및 상태
  DataValue current_value;
  double threshold_value = 0.0;
  TriggerCondition trigger_condition = TriggerCondition::NONE;

  // 알람 메타데이터 - 수정된 기본값
  AlarmType alarm_type = AlarmType::ANALOG;
  std::string message;
  AlarmSeverity severity = AlarmSeverity::MEDIUM; // 기본값: MEDIUM (enum 값 2)
  AlarmState state = AlarmState::ACTIVE;

  // 시간 정보
  Timestamp timestamp;
  Timestamp occurrence_time;

  // 추가 컨텍스트 정보
  std::string source_name;
  std::string location;
  int tenant_id = 1;
  int site_id = 0; // 사이트 식별자 추가
  DataValue trigger_value;
  bool condition_met = false;
  nlohmann::json extra_info =
      nlohmann::json::object(); // 추가 메타데이터 (file_ref 등)

  // 생성자들
  AlarmEvent()
      : timestamp(std::chrono::system_clock::now()),
        occurrence_time(std::chrono::system_clock::now()) {}

  AlarmEvent(const UniqueId &dev_id, int pt_id, const DataValue &value,
             AlarmSeverity sev, const std::string &msg,
             AlarmType type = AlarmType::ANALOG)
      : device_id(dev_id), point_id(pt_id), current_value(value),
        alarm_type(type), message(msg), severity(sev),
        state(AlarmState::ACTIVE), timestamp(std::chrono::system_clock::now()),
        occurrence_time(std::chrono::system_clock::now()) {}

  // 타입 변환 헬퍼 메서드들 - 수정된 enum 순서 반영
  std::string getSeverityString() const {
    switch (severity) {
    case AlarmSeverity::INFO:
      return "INFO";
    case AlarmSeverity::LOW:
      return "LOW";
    case AlarmSeverity::MEDIUM:
      return "MEDIUM";
    case AlarmSeverity::HIGH:
      return "HIGH";
    case AlarmSeverity::CRITICAL:
      return "CRITICAL";
    default:
      return "MEDIUM";
    }
  }

  std::string getAlarmTypeString() const {
    switch (alarm_type) {
    case AlarmType::ANALOG:
      return "ANALOG";
    case AlarmType::DIGITAL:
      return "DIGITAL";
    case AlarmType::SCRIPT:
      return "SCRIPT";
    case AlarmType::COMPOUND:
      return "COMPOUND";
    case AlarmType::COMMUNICATION:
      return "COMMUNICATION";
    case AlarmType::QUALITY:
      return "QUALITY";
    default:
      return "ANALOG";
    }
  }

  std::string getStateString() const {
    switch (state) {
    case AlarmState::INACTIVE:
      return "INACTIVE";
    case AlarmState::ACTIVE:
      return "ACTIVE";
    case AlarmState::ACKNOWLEDGED:
      return "ACKNOWLEDGED";
    case AlarmState::CLEARED:
      return "CLEARED";
    case AlarmState::SUPPRESSED:
      return "SUPPRESSED";
    case AlarmState::SHELVED:
      return "SHELVED";
    default:
      return "ACTIVE";
    }
  }

  std::string getTriggerConditionString() const {
    switch (trigger_condition) {
    case TriggerCondition::HIGH:
      return "HIGH";
    case TriggerCondition::LOW:
      return "LOW";
    case TriggerCondition::HIGH_HIGH:
      return "HIGH_HIGH";
    case TriggerCondition::LOW_LOW:
      return "LOW_LOW";
    case TriggerCondition::DIGITAL_TRUE:
      return "DIGITAL_TRUE";
    case TriggerCondition::DIGITAL_FALSE:
      return "DIGITAL_FALSE";
    case TriggerCondition::DIGITAL_CHANGE:
      return "DIGITAL_CHANGE";
    case TriggerCondition::RATE_CHANGE:
      return "RATE_CHANGE";
    case TriggerCondition::NONE:
      return "NONE";
    default:
      return "UNKNOWN";
    }
  }

  std::string getTriggerValueString() const {
    return std::visit(
        [](const auto &v) -> std::string {
          if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                       std::string>) {
            return v;
          } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                              bool>) {
            return v ? "true" : "false";
          } else {
            return std::to_string(v);
          }
        },
        trigger_value);
  }

  // 헬퍼 메서드들
  bool isNewOccurrence() const { return occurrence_id == 0; }
  bool isActive() const { return state == AlarmState::ACTIVE; }
  bool isCleared() const { return state == AlarmState::CLEARED; }

  // 심각도 수치 반환 (정렬용) - 수정된 enum 값 반영
  int getSeverityLevel() const {
    switch (severity) {
    case AlarmSeverity::CRITICAL:
      return 4; // 가장 높음
    case AlarmSeverity::HIGH:
      return 3;
    case AlarmSeverity::MEDIUM:
      return 2;
    case AlarmSeverity::LOW:
      return 1;
    case AlarmSeverity::INFO:
      return 0; // 가장 낮음
    default:
      return 2;
    }
  }

  // JSON 직렬화
  std::string ToJSON() const {
    JsonType j;

    j["device_id"] = device_id;
    j["point_id"] = point_id;
    j["rule_id"] = rule_id;
    j["occurrence_id"] = occurrence_id;

    j["severity"] = getSeverityString();
    j["alarm_type"] = getAlarmTypeString();
    j["state"] = getStateString();
    j["trigger_condition"] = getTriggerConditionString();

    j["message"] = message;
    j["threshold_value"] = threshold_value;
    j["source_name"] = source_name;
    j["location"] = location;
    j["tenant_id"] = tenant_id;
    j["site_id"] = site_id;
    j["condition_met"] = condition_met;

    std::visit([&j](const auto &v) { j["current_value"] = v; }, current_value);
    std::visit([&j](const auto &v) { j["trigger_value"] = v; }, trigger_value);

    auto convertTimestamp = [](const Timestamp &ts) -> std::string {
      auto time_t = std::chrono::system_clock::to_time_t(ts);
      std::tm tm_buf;
#ifdef _WIN32
      localtime_s(&tm_buf, &time_t);
#else
      localtime_r(&time_t, &tm_buf);
#endif
      char buffer[32];
      std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm_buf);
      return std::string(buffer);
    };

    j["timestamp"] = convertTimestamp(timestamp);
    j["occurrence_time"] = convertTimestamp(occurrence_time);

    return j.dump();
  }
};

// =============================================================================
// 기존 구조체들 - 그대로 유지
// =============================================================================

struct AlarmRule {
  int id = 0;
  int tenant_id = 0;
  std::string name;
  std::string description;

  std::string target_type;
  int target_id = 0;
  std::string target_group;

  AlarmType alarm_type = AlarmType::ANALOG;

  struct AnalogLimits {
    std::optional<double> high_high;
    std::optional<double> high;
    std::optional<double> low;
    std::optional<double> low_low;
    double deadband = 0.0;
  } analog_limits;

  DigitalTrigger digital_trigger = DigitalTrigger::ON_CHANGE;
  std::map<int, std::string> state_messages;

  std::string condition_script;
  std::string message_script;

  nlohmann::json message_config;
  std::string message_template;

  AlarmSeverity severity = AlarmSeverity::MEDIUM;
  int priority = 100;

  bool auto_acknowledge = false;
  std::chrono::minutes acknowledge_timeout{0};
  bool auto_clear = true;

  bool notification_enabled = true;
  std::chrono::seconds notification_delay{0};
  std::vector<std::string> notification_channels;

  bool is_enabled = true;
  bool is_latched = false;

  mutable AnalogAlarmLevel last_analog_level = AnalogAlarmLevel::NORMAL;
  mutable bool last_digital_state = false;
  mutable std::optional<double> last_value;
};

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

  std::optional<std::chrono::system_clock::time_point> acknowledged_time;
  std::optional<int> acknowledged_by;
  std::string acknowledge_comment;

  std::optional<std::chrono::system_clock::time_point> cleared_time;
  std::optional<nlohmann::json> cleared_value;

  nlohmann::json context_data;
};

struct AlarmEvaluation {
  bool should_trigger = false;
  bool should_clear = false;
  bool state_changed = false;

  AnalogAlarmLevel analog_level = AnalogAlarmLevel::NORMAL;

  std::string condition_met;
  std::string message;

  AlarmSeverity severity = AlarmSeverity::MEDIUM;

  nlohmann::json context_data;

  std::string triggered_value;
  std::string alarm_level;
  std::chrono::microseconds evaluation_time{0};
  std::chrono::system_clock::time_point timestamp;
  int rule_id = 0;
  int tenant_id = 0;

  AlarmEvaluation() : timestamp(std::chrono::system_clock::now()) {}

  std::string getSeverityString() const {
    switch (severity) {
    case AlarmSeverity::INFO:
      return "INFO";
    case AlarmSeverity::LOW:
      return "LOW";
    case AlarmSeverity::MEDIUM:
      return "MEDIUM";
    case AlarmSeverity::HIGH:
      return "HIGH";
    case AlarmSeverity::CRITICAL:
      return "CRITICAL";
    default:
      return "MEDIUM";
    }
  }

  std::string getAnalogLevelString() const {
    switch (analog_level) {
    case AnalogAlarmLevel::HIGH_HIGH:
      return "HIGH_HIGH";
    case AnalogAlarmLevel::HIGH:
      return "HIGH";
    case AnalogAlarmLevel::NORMAL:
      return "NORMAL";
    case AnalogAlarmLevel::LOW:
      return "LOW";
    case AnalogAlarmLevel::LOW_LOW:
      return "LOW_LOW";
    default:
      return "NORMAL";
    }
  }

  bool hasConditionMet() const { return !condition_met.empty(); }
};

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

struct AlarmProcessingStats {
  uint64_t total_evaluated = 0;
  uint64_t total_triggered = 0;
  uint64_t total_cleared = 0;
  uint64_t total_acknowledged = 0;

  uint64_t critical_count = 0;
  uint64_t high_count = 0;
  uint64_t medium_count = 0;
  uint64_t low_count = 0;
  uint64_t info_count = 0;

  uint64_t analog_alarms = 0;
  uint64_t digital_alarms = 0;
  uint64_t script_alarms = 0;

  double avg_evaluation_time_us = 0.0;
  uint64_t evaluation_errors = 0;

  std::chrono::system_clock::time_point last_alarm_time;
  std::chrono::system_clock::time_point stats_start_time;

  AlarmProcessingStats() : stats_start_time(std::chrono::system_clock::now()) {}

  uint64_t getTotalActiveAlarms() const {
    return total_triggered - total_cleared;
  }
  double getAlarmRate() const {
    return (total_evaluated > 0) ? static_cast<double>(total_triggered) /
                                       static_cast<double>(total_evaluated)
                                 : 0.0;
  }

  nlohmann::json toJson() const {
    nlohmann::json j;
    j["total_evaluated"] = total_evaluated;
    j["total_triggered"] = total_triggered;
    j["total_cleared"] = total_cleared;
    j["total_acknowledged"] = total_acknowledged;
    j["total_active"] = getTotalActiveAlarms();

    j["by_severity"] = {{"critical", critical_count},
                        {"high", high_count},
                        {"medium", medium_count},
                        {"low", low_count},
                        {"info", info_count}};

    j["by_type"] = {{"analog", analog_alarms},
                    {"digital", digital_alarms},
                    {"script", script_alarms}};

    j["performance"] = {{"avg_evaluation_time_us", avg_evaluation_time_us},
                        {"evaluation_errors", evaluation_errors},
                        {"alarm_rate", getAlarmRate()}};

    return j;
  }
};

struct AlarmSystemStatus {
  bool engine_initialized = false;
  bool repositories_available = false;
  bool script_engine_ready = false;

  size_t active_rules_count = 0;
  size_t total_rules_count = 0;

  std::chrono::system_clock::time_point last_evaluation_time;
  std::chrono::system_clock::time_point system_start_time;

  AlarmSystemStatus() : system_start_time(std::chrono::system_clock::now()) {}

  std::chrono::duration<double> getUptime() const {
    return std::chrono::system_clock::now() - system_start_time;
  }

  bool isFullyReady() const {
    return engine_initialized && repositories_available && script_engine_ready;
  }

  nlohmann::json toJson() const {
    nlohmann::json j;
    j["engine_initialized"] = engine_initialized;
    j["repositories_available"] = repositories_available;
    j["script_engine_ready"] = script_engine_ready;
    j["fully_ready"] = isFullyReady();

    j["rules"] = {{"active", active_rules_count}, {"total", total_rules_count}};

    j["uptime_seconds"] = getUptime().count();

    return j;
  }
};

struct AlarmMetrics {
  AlarmProcessingStats processing;
  AlarmSystemStatus system;

  nlohmann::json toJson() const {
    nlohmann::json j;
    j["processing"] = processing.toJson();
    j["system"] = system.toJson();
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return j;
  }
};

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
// 타입 변환 헬퍼 함수들 - 대소문자 모두 지원
// =============================================================================

// AlarmType 변환
inline std::string alarmTypeToString(AlarmType type) {
  switch (type) {
  case AlarmType::ANALOG:
    return "analog";
  case AlarmType::DIGITAL:
    return "digital";
  case AlarmType::SCRIPT:
    return "script";
  case AlarmType::COMPOUND:
    return "compound";
  case AlarmType::COMMUNICATION:
    return "communication";
  case AlarmType::QUALITY:
    return "quality";
  default:
    return "analog";
  }
}

inline AlarmType stringToAlarmType(const std::string &str) {
  if (str == "ANALOG" || str == "analog")
    return AlarmType::ANALOG;
  if (str == "DIGITAL" || str == "digital")
    return AlarmType::DIGITAL;
  if (str == "SCRIPT" || str == "script")
    return AlarmType::SCRIPT;
  if (str == "COMPOUND" || str == "compound")
    return AlarmType::COMPOUND;
  if (str == "COMMUNICATION" || str == "communication")
    return AlarmType::COMMUNICATION;
  if (str == "QUALITY" || str == "quality")
    return AlarmType::QUALITY;
  return AlarmType::ANALOG;
}

// AlarmSeverity 변환 - 수정된 enum 순서 반영
inline std::string severityToString(AlarmSeverity severity) {
  switch (severity) {
  case AlarmSeverity::INFO:
    return "info";
  case AlarmSeverity::LOW:
    return "low";
  case AlarmSeverity::MEDIUM:
    return "medium";
  case AlarmSeverity::HIGH:
    return "high";
  case AlarmSeverity::CRITICAL:
    return "critical";
  default:
    return "medium";
  }
}

inline AlarmSeverity stringToSeverity(const std::string &str) {
  if (str == "INFO" || str == "info")
    return AlarmSeverity::INFO;
  if (str == "LOW" || str == "low")
    return AlarmSeverity::LOW;
  if (str == "MEDIUM" || str == "medium")
    return AlarmSeverity::MEDIUM;
  if (str == "HIGH" || str == "high")
    return AlarmSeverity::HIGH;
  if (str == "CRITICAL" || str == "critical")
    return AlarmSeverity::CRITICAL;
  return AlarmSeverity::MEDIUM;
}

// AlarmState 변환
inline std::string stateToString(AlarmState state) {
  switch (state) {
  case AlarmState::INACTIVE:
    return "inactive";
  case AlarmState::ACTIVE:
    return "active";
  case AlarmState::ACKNOWLEDGED:
    return "acknowledged";
  case AlarmState::CLEARED:
    return "cleared";
  case AlarmState::SUPPRESSED:
    return "suppressed";
  case AlarmState::SHELVED:
    return "shelved";
  default:
    return "active";
  }
}

inline AlarmState stringToState(const std::string &str) {
  if (str == "INACTIVE" || str == "inactive")
    return AlarmState::INACTIVE;
  if (str == "ACTIVE" || str == "active")
    return AlarmState::ACTIVE;
  if (str == "ACKNOWLEDGED" || str == "acknowledged")
    return AlarmState::ACKNOWLEDGED;
  if (str == "CLEARED" || str == "cleared")
    return AlarmState::CLEARED;
  if (str == "SUPPRESSED" || str == "suppressed")
    return AlarmState::SUPPRESSED;
  if (str == "SHELVED" || str == "shelved")
    return AlarmState::SHELVED;
  return AlarmState::ACTIVE;
}

// DigitalTrigger 변환
inline std::string digitalTriggerToString(DigitalTrigger trigger) {
  switch (trigger) {
  case DigitalTrigger::ON_TRUE:
    return "on_true";
  case DigitalTrigger::ON_FALSE:
    return "on_false";
  case DigitalTrigger::ON_CHANGE:
    return "on_change";
  case DigitalTrigger::ON_RISING:
    return "on_rising";
  case DigitalTrigger::ON_FALLING:
    return "on_falling";
  default:
    return "on_change";
  }
}

inline DigitalTrigger stringToDigitalTrigger(const std::string &str) {
  if (str == "ON_TRUE" || str == "on_true")
    return DigitalTrigger::ON_TRUE;
  if (str == "ON_FALSE" || str == "on_false")
    return DigitalTrigger::ON_FALSE;
  if (str == "ON_CHANGE" || str == "on_change")
    return DigitalTrigger::ON_CHANGE;
  if (str == "ON_RISING" || str == "on_rising")
    return DigitalTrigger::ON_RISING;
  if (str == "ON_FALLING" || str == "on_falling")
    return DigitalTrigger::ON_FALLING;
  return DigitalTrigger::ON_CHANGE;
}

// TargetType 변환
inline std::string targetTypeToString(TargetType type) {
  switch (type) {
  case TargetType::DATA_POINT:
    return "data_point";
  case TargetType::VIRTUAL_POINT:
    return "virtual_point";
  case TargetType::GROUP:
    return "group";
  default:
    return "data_point";
  }
}

inline TargetType stringToTargetType(const std::string &str) {
  if (str == "DATA_POINT" || str == "data_point")
    return TargetType::DATA_POINT;
  if (str == "VIRTUAL_POINT" || str == "virtual_point")
    return TargetType::VIRTUAL_POINT;
  if (str == "GROUP" || str == "group")
    return TargetType::GROUP;
  return TargetType::DATA_POINT;
}

// AnalogAlarmLevel 변환
inline std::string analogAlarmLevelToString(AnalogAlarmLevel level) {
  switch (level) {
  case AnalogAlarmLevel::NORMAL:
    return "normal";
  case AnalogAlarmLevel::LOW_LOW:
    return "low_low";
  case AnalogAlarmLevel::LOW:
    return "low";
  case AnalogAlarmLevel::HIGH:
    return "high";
  case AnalogAlarmLevel::HIGH_HIGH:
    return "high_high";
  default:
    return "normal";
  }
}

inline AnalogAlarmLevel stringToAnalogAlarmLevel(const std::string &str) {
  if (str == "NORMAL" || str == "normal")
    return AnalogAlarmLevel::NORMAL;
  if (str == "LOW_LOW" || str == "low_low")
    return AnalogAlarmLevel::LOW_LOW;
  if (str == "LOW" || str == "low")
    return AnalogAlarmLevel::LOW;
  if (str == "HIGH" || str == "high")
    return AnalogAlarmLevel::HIGH;
  if (str == "HIGH_HIGH" || str == "high_high")
    return AnalogAlarmLevel::HIGH_HIGH;
  return AnalogAlarmLevel::NORMAL;
}

// =============================================================================
// 소문자 변환 함수들
// =============================================================================

inline std::string alarmTypeToLowerString(AlarmType type) {
  switch (type) {
  case AlarmType::ANALOG:
    return "analog";
  case AlarmType::DIGITAL:
    return "digital";
  case AlarmType::SCRIPT:
    return "script";
  case AlarmType::COMPOUND:
    return "compound";
  case AlarmType::COMMUNICATION:
    return "communication";
  case AlarmType::QUALITY:
    return "quality";
  default:
    return "analog";
  }
}

inline std::string severityToLowerString(AlarmSeverity severity) {
  switch (severity) {
  case AlarmSeverity::INFO:
    return "info";
  case AlarmSeverity::LOW:
    return "low";
  case AlarmSeverity::MEDIUM:
    return "medium";
  case AlarmSeverity::HIGH:
    return "high";
  case AlarmSeverity::CRITICAL:
    return "critical";
  default:
    return "medium";
  }
}

inline std::string stateToLowerString(AlarmState state) {
  switch (state) {
  case AlarmState::INACTIVE:
    return "inactive";
  case AlarmState::ACTIVE:
    return "active";
  case AlarmState::ACKNOWLEDGED:
    return "acknowledged";
  case AlarmState::CLEARED:
    return "cleared";
  case AlarmState::SUPPRESSED:
    return "suppressed";
  case AlarmState::SHELVED:
    return "shelved";
  default:
    return "active";
  }
}

// =============================================================================
// 컴파일 타임 검증 - enum 값 확인
// =============================================================================

static_assert(static_cast<int>(AlarmSeverity::INFO) == 0, "INFO should be 0");
static_assert(static_cast<int>(AlarmSeverity::LOW) == 1, "LOW should be 1");
static_assert(static_cast<int>(AlarmSeverity::MEDIUM) == 2,
              "MEDIUM should be 2");
static_assert(static_cast<int>(AlarmSeverity::HIGH) == 3, "HIGH should be 3");
static_assert(static_cast<int>(AlarmSeverity::CRITICAL) == 4,
              "CRITICAL should be 4");

static_assert(static_cast<int>(AlarmState::INACTIVE) == 0,
              "INACTIVE should be 0");
static_assert(static_cast<int>(AlarmState::ACTIVE) == 1, "ACTIVE should be 1");
static_assert(static_cast<int>(AlarmState::ACKNOWLEDGED) == 2,
              "ACKNOWLEDGED should be 2");
static_assert(static_cast<int>(AlarmState::CLEARED) == 3,
              "CLEARED should be 3");

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_TYPES_H