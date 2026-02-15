#ifndef PULSEONE_COMMON_STRUCTS_H
#define PULSEONE_COMMON_STRUCTS_H

/**
 * @file Structs.h
 * @brief PulseOne 핵심 구조체 정의
 * @author PulseOne Development Team
 * @date 2025-08-05
 */

#include <chrono>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "Alarm/AlarmTypes.h"
#include "BasicTypes.h"
#include "Constants.h"
#include "DriverError.h"
#include "DriverStatistics.h"
#include "Enums.h"
#include "IProtocolConfig.h"
#include "ProtocolConfigs.h"

// 🔥 전방 선언으로 순환 의존성 방지
namespace PulseOne::Structs {
class IProtocolConfig; // IProtocolConfig.h 전방 선언
}

namespace PulseOne {
namespace Structs {

// 🔥 타입 별칭 명시적 선언 (순환 참조 방지)
using namespace PulseOne::BasicTypes;
using namespace PulseOne::Enums;
using JsonType = nlohmann::json;

// 🔥 핵심 타입들 명시적 별칭 (필수!)
using DataValue = PulseOne::BasicTypes::DataVariant; // ✅ 매우 중요!
using Timestamp = PulseOne::BasicTypes::Timestamp;   // ✅ 매우 중요!
using UniqueId = PulseOne::BasicTypes::UniqueId;     // ✅ 매우 중요!
using Duration = PulseOne::BasicTypes::Duration;     // ✅ 중요!
using EngineerID = PulseOne::BasicTypes::EngineerID; // ✅ 중요!

// Protocols are now identified by std::string. Legacy ProtocolType enum
// removed.
using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
using DataQuality = PulseOne::Enums::DataQuality;
using LogLevel = PulseOne::Enums::LogLevel;
using MaintenanceStatus = PulseOne::Enums::MaintenanceStatus;

// 기존 Structs 네임스페이스에서 사용하던 타입들 (호환성)
using AlarmType = Alarm::AlarmType;
using AlarmSeverity = Alarm::AlarmSeverity;
using AlarmState = Alarm::AlarmState;
using DigitalTrigger = Alarm::DigitalTrigger;
using AnalogAlarmLevel = Alarm::AnalogAlarmLevel;
using TargetType = Alarm::TargetType;
using TriggerCondition = Alarm::TriggerCondition;

// 구조체들도 별칭으로 제공 (호환성)
using AlarmEvent = Alarm::AlarmEvent;
using AlarmEvaluation = Alarm::AlarmEvaluation;
using AlarmRule = Alarm::AlarmRule;
using AlarmOccurrence = Alarm::AlarmOccurrence;
using AlarmFilter = Alarm::AlarmFilter;
using AlarmStatistics = Alarm::AlarmStatistics;
// ❌ ErrorCode 별칭 제거 (DriverError.h와 충돌 방지)
// using ErrorCode = PulseOne::Enums::ErrorCode;  // 🔥 제거!

/**
 * @brief 상태 자동 판단 임계값 설정
 */
struct StatusThresholds {
  uint32_t offline_failure_count = 3;        // 3회 연속 실패 → OFFLINE
  std::chrono::seconds timeout_threshold{5}; // 5초 초과 → WARNING/ERROR
  double partial_failure_ratio = 0.3;        // 30% 포인트 실패 → WARNING
  double error_failure_ratio = 0.7;          // 70% 포인트 실패 → ERROR
  std::chrono::seconds offline_timeout{30};  // 30초간 응답 없음 → OFFLINE

  // JSON 직렬화
  nlohmann::json toJson() const {
    return nlohmann::json{{"offline_failure_count", offline_failure_count},
                          {"timeout_threshold_sec", timeout_threshold.count()},
                          {"partial_failure_ratio", partial_failure_ratio},
                          {"error_failure_ratio", error_failure_ratio},
                          {"offline_timeout_sec", offline_timeout.count()}};
  }

  // JSON 역직렬화
  static StatusThresholds fromJson(const nlohmann::json &j) {
    StatusThresholds thresholds;
    if (j.contains("offline_failure_count"))
      thresholds.offline_failure_count = j["offline_failure_count"];
    if (j.contains("timeout_threshold_sec"))
      thresholds.timeout_threshold =
          std::chrono::seconds(j["timeout_threshold_sec"]);
    if (j.contains("partial_failure_ratio"))
      thresholds.partial_failure_ratio = j["partial_failure_ratio"];
    if (j.contains("error_failure_ratio"))
      thresholds.error_failure_ratio = j["error_failure_ratio"];
    if (j.contains("offline_timeout_sec"))
      thresholds.offline_timeout =
          std::chrono::seconds(j["offline_timeout_sec"]);
    return thresholds;
  }
};
// =========================================================================
// 🔥 Phase 1: 타임스탬프 값 구조체 (기존 확장)
// =========================================================================

/**
 * @brief 타임스탬프가 포함된 데이터 값
 * @details 모든 드라이버에서 사용하는 표준 값 구조체
 */
struct TimestampedValue {
  // ==========================================================================
  // 🔥 기존 필드들 (그대로 유지)
  // ==========================================================================
  DataValue value;                         // 현재 값 (스케일링 적용 후)
  Timestamp timestamp;                     // 데이터 수집/생성 시간
  DataQuality quality = DataQuality::GOOD; // 데이터 품질 상태
  std::string source = "";                 // 데이터 소스 (worker명 등)
  int point_id = 0;                        // 데이터포인트 고유 ID
  bool is_virtual_point = false;           // 가상포인트 여부
  // ==========================================================================
  // 🔥 상태변화 감지용 필드들 (디지털/아날로그 조건부 저장)
  // ==========================================================================
  DataValue previous_value;   // 이전 값 (상태변화 비교용)
  bool value_changed = false; // 이전값 대비 변화 여부
  double change_threshold =
      0.0; // 아날로그 변화 임계값 (DataPoint 설정에서 복사)

  // ==========================================================================
  // 🔥 저장 제어용 필드들
  // ==========================================================================
  bool force_rdb_store = false; // 강제 RDB 저장 플래그 (중요 데이터)

  // ==========================================================================
  // 🔥 데이터 추적용 필드들
  // ==========================================================================
  uint32_t sequence_number = 0; // Worker내 시퀀스 번호 (패킷 순서)
  double raw_value = 0.0;       // 원시 값 (스케일링 적용 전)
  double scaling_factor = 1.0;  // 스케일링 인수 (DataPoint에서 복사)
  double scaling_offset = 0.0;  // 스케일링 오프셋 (DataPoint에서 복사)

  // ==========================================================================
  // 🔥 알람 관련 필드들
  // ==========================================================================
  std::vector<int> applicable_alarms; // 이 포인트에 적용되는 알람 규칙 ID들
  bool suppress_alarms = false;       // 알람 억제 여부 (점검중 등)
  bool trigger_alarm_check = true;    // 알람 체크 수행 여부

  // ==========================================================================
  // 🔥 확장 필드
  // ==========================================================================
  nlohmann::json metadata =
      nlohmann::json::object(); // 추가 메타데이터 (file_ref 등)

  // ==========================================================================
  // 🔥 편의 함수들 (인라인으로 성능 최적화)
  // ==========================================================================

  /**
   * @brief 디지털 포인트 상태변화 확인
   * @return true if digital state changed, false otherwise
   */
  inline bool HasDigitalStateChanged() const {
    if (!std::holds_alternative<bool>(value) ||
        !std::holds_alternative<bool>(previous_value)) {
      return false;
    }
    return std::get<bool>(value) != std::get<bool>(previous_value);
  }

  /**
   * @brief 아날로그 값 변화량 계산
   * @return 절댓값 변화량 (double)
   */
  inline double GetAnalogChangeAmount() const {
    if (!std::holds_alternative<double>(value) ||
        !std::holds_alternative<double>(previous_value)) {
      return 0.0;
    }
    return std::abs(std::get<double>(value) - std::get<double>(previous_value));
  }

  /**
   * @brief RDB 저장 필요 여부 판단 (조건부 저장용)
   * @return true if should store to RDB, false otherwise
   */
  inline bool ShouldStoreToRDB() const {
    // 강제 저장 플래그
    if (force_rdb_store)
      return true;

    // 품질 체크
    if (quality != DataQuality::GOOD)
      return false;

    // 디지털: 상태변화시
    if (std::holds_alternative<bool>(value)) {
      return HasDigitalStateChanged();
    }

    // 아날로그: 임계값 초과시
    return GetAnalogChangeAmount() > change_threshold;
  }

  /**
   * @brief Redis용 경량 JSON 생성 (성능 최적화)
   * @return JSON string for Redis storage
   */
  inline std::string ToRedisJSON() const {
    nlohmann::json j;
    j["point_id"] = point_id;

    // 현재값만 저장 (이전값은 Redis에 불필요)
    std::visit([&j](const auto &v) { j["value"] = v; }, value);

    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["quality"] = static_cast<int>(quality);
    j["source"] = source;
    j["sequence"] = sequence_number;

    // 원시값 (스케일링 정보 추적용)
    if (raw_value != 0.0) {
      j["raw_value"] = raw_value;
    }

    // 변화 플래그 (상태변화 표시용)
    if (value_changed) {
      j["changed"] = true;
    }

    return j.dump();
  }

  /**
   * @brief 완전한 JSON 생성 (RDB 저장용)
   * @return Complete JSON with all fields
   */
  inline std::string ToFullJSON() const {
    nlohmann::json j;
    j["point_id"] = point_id;

    // 현재값과 이전값 모두 저장
    std::visit([&j](const auto &v) { j["value"] = v; }, value);
    std::visit([&j](const auto &v) { j["previous_value"] = v; },
               previous_value);

    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["quality"] = static_cast<int>(quality);
    j["source"] = source;
    j["value_changed"] = value_changed;
    j["change_threshold"] = change_threshold;
    j["force_rdb_store"] = force_rdb_store;
    j["sequence_number"] = sequence_number;
    j["raw_value"] = raw_value;
    j["scaling_factor"] = scaling_factor;
    j["scaling_offset"] = scaling_offset;

    // 알람 정보
    if (!applicable_alarms.empty()) {
      j["applicable_alarms"] = applicable_alarms;
    }
    j["suppress_alarms"] = suppress_alarms;
    j["trigger_alarm_check"] = trigger_alarm_check;

    return j.dump();
  }

  /**
   * @brief 이전값 업데이트 (Worker에서 호출)
   */
  inline void UpdatePreviousValue() {
    previous_value = value;
    value_changed = false;
  }

  /**
   * @brief 값 변화 체크 및 플래그 설정 (Worker에서 호출)
   * @param new_value 새로운 값
   */
  inline void SetValueWithChange(const DataValue &new_value) {
    previous_value = value;
    value = new_value;
    value_changed = (value != previous_value);
    timestamp = std::chrono::system_clock::now();
  }

  /**
   * @brief 데이터 타입 확인
   * @return true if digital (bool), false if analog
   */
  inline bool IsDigital() const { return std::holds_alternative<bool>(value); }

  /**
   * @brief 알람 체크 필요 여부
   * @return true if should check alarms
   */
  inline bool ShouldCheckAlarms() const {
    return trigger_alarm_check && !suppress_alarms &&
           !applicable_alarms.empty();
  }

  /**
   * @brief 값을 double로 변환 (수치 계산용)
   * @return double value or 0.0 if not numeric
   */
  inline double GetDoubleValue() const {
    return std::visit(
        [](const auto &v) -> double {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(v);
          }
          return 0.0;
        },
        value);
  }
};

// =========================================================================
// 🔥 Phase 1: 통합 DataPoint 구조체 (기존 여러 DataPoint 통합)
// =========================================================================

/**
 * @brief 통합 데이터 포인트 구조체 (완전판)
 * @details
 * - 설정 정보 + 실제 값 통합
 * - Database::DataPointEntity 호환
 * - Worker에서 직접 사용 가능
 * - Properties 맵 제거하고 직접 필드 사용
 */
struct DataPoint {
  // =======================================================================
  // 🔥 기본 식별 정보 (설정)
  // =======================================================================
  UniqueId id;                  // point_id
  UniqueId device_id;           // 소속 디바이스 ID
  std::string name = "";        // 표시 이름
  std::string description = ""; // 설명

  // =======================================================================
  // 🔥 주소 정보 (설정)
  // =======================================================================
  uint32_t address = 0; // 숫자 주소 (Modbus 레지스터, BACnet 인스턴스 등)
  std::string address_string = ""; // 문자열 주소 (MQTT 토픽, OPC UA NodeId 등)
  std::string mapping_key =
      ""; // JSONPath 또는 매핑 키 (예: sensors[0].temp) - Step 14 추가

  // =======================================================================
  // 🔥 데이터 타입 및 접근성 (설정)
  // =======================================================================
  std::string data_type = "UNKNOWN"; // INT16, UINT32, FLOAT, BOOL, STRING 등
  std::string access_mode = "read";  // read, write, read_write
  bool is_enabled = true;            // 활성화 상태
  bool is_writable = false;          // 쓰기 가능 여부

  // =======================================================================
  // 🔥 엔지니어링 단위 및 스케일링 (설정)
  // =======================================================================
  std::string unit = "";       // 단위 (℃, %, kW 등)
  double scaling_factor = 1.0; // 스케일 인수
  double scaling_offset = 0.0; // 오프셋
  double min_value = 0.0;      // 최소값
  double max_value = 0.0;      // 최대값

  // =======================================================================
  // 🔥 로깅 및 수집 설정 (설정)
  // =======================================================================
  bool is_log_enabled = true;       // 로깅 활성화
  uint32_t log_interval_ms = 0;     // 로깅 간격
  double log_deadband = 0.0;        // 로깅 데드밴드
  uint32_t polling_interval_ms = 0; // 개별 폴링 간격

  // =======================================================================
  // 🔥 메타데이터 (설정)
  // =======================================================================
  std::string group = "";                             // 그룹명
  std::string tags = "";                              // JSON 배열 형태
  std::string metadata = "";                          // JSON 객체 형태
  std::map<std::string, std::string> protocol_params; // 프로토콜 특화 매개변수

  // =======================================================================
  // 🔥 실제 값 필드들 (실시간 데이터) - 새로 추가!
  // =======================================================================

  /**
   * @brief 현재 값 (실제 데이터)
   * @details DataVariant = std::variant<bool, int16_t, uint16_t, int32_t,
   * uint32_t, float, double, string>
   */
  PulseOne::BasicTypes::DataVariant current_value{0.0};

  /**
   * @brief 원시 값 (스케일링 적용 전)
   */
  PulseOne::BasicTypes::DataVariant raw_value{0.0};

  /**
   * @brief 데이터 품질 코드
   */
  PulseOne::Enums::DataQuality quality_code =
      PulseOne::Enums::DataQuality::NOT_CONNECTED;

  /**
   * @brief 값 타임스탬프 (마지막 값 업데이트 시간)
   */
  PulseOne::BasicTypes::Timestamp value_timestamp;

  /**
   * @brief 품질 타임스탬프 (마지막 품질 변경 시간)
   */
  PulseOne::BasicTypes::Timestamp quality_timestamp;

  /**
   * @brief 마지막 로그 시간
   */
  PulseOne::BasicTypes::Timestamp last_log_time;

  // =======================================================================
  // 🔥 통계 필드들 (실시간 데이터) - atomic 제거
  // =======================================================================

  /**
   * @brief 마지막 읽기 시간
   */
  PulseOne::BasicTypes::Timestamp last_read_time;

  /**
   * @brief 마지막 쓰기 시간
   */
  PulseOne::BasicTypes::Timestamp last_write_time;

  /**
   * @brief 읽기 카운트 (atomic 제거, 단순 uint64_t 사용)
   */
  uint64_t read_count = 0;

  /**
   * @brief 쓰기 카운트 (atomic 제거, 단순 uint64_t 사용)
   */
  uint64_t write_count = 0;

  /**
   * @brief 에러 카운트 (atomic 제거, 단순 uint64_t 사용)
   */
  uint64_t error_count = 0;

  // =======================================================================
  // 🔥 시간 정보 (설정)
  // =======================================================================
  PulseOne::BasicTypes::Timestamp created_at;
  PulseOne::BasicTypes::Timestamp updated_at;

  // =======================================================================
  // 🔥 생성자들
  // =======================================================================
  DataPoint() {
    auto now = std::chrono::system_clock::now();
    created_at = now;
    updated_at = now;
    value_timestamp = now;
    quality_timestamp = now;
    last_log_time = now;
    last_read_time = now;
    last_write_time = now;
  }

  // 복사 생성자 (명시적 구현)
  DataPoint(const DataPoint &other)
      : id(other.id), device_id(other.device_id), name(other.name),
        description(other.description), address(other.address),
        address_string(other.address_string), mapping_key(other.mapping_key),
        data_type(other.data_type), access_mode(other.access_mode),
        is_enabled(other.is_enabled), is_writable(other.is_writable),
        unit(other.unit), scaling_factor(other.scaling_factor),
        scaling_offset(other.scaling_offset), min_value(other.min_value),
        max_value(other.max_value), is_log_enabled(other.is_log_enabled),
        log_interval_ms(other.log_interval_ms),
        log_deadband(other.log_deadband),
        polling_interval_ms(other.polling_interval_ms), group(other.group),
        tags(other.tags), metadata(other.metadata),
        protocol_params(other.protocol_params),
        current_value(other.current_value), raw_value(other.raw_value),
        quality_code(other.quality_code),
        value_timestamp(other.value_timestamp),
        quality_timestamp(other.quality_timestamp),
        last_log_time(other.last_log_time),
        last_read_time(other.last_read_time),
        last_write_time(other.last_write_time), read_count(other.read_count),
        write_count(other.write_count), error_count(other.error_count),
        created_at(other.created_at), updated_at(other.updated_at) {}

  // 이동 생성자
  DataPoint(DataPoint &&other) noexcept
      : id(std::move(other.id)), device_id(std::move(other.device_id)),
        name(std::move(other.name)), description(std::move(other.description)),
        address(other.address), address_string(std::move(other.address_string)),
        mapping_key(std::move(other.mapping_key)),
        data_type(std::move(other.data_type)),
        access_mode(std::move(other.access_mode)), is_enabled(other.is_enabled),
        is_writable(other.is_writable), unit(std::move(other.unit)),
        scaling_factor(other.scaling_factor),
        scaling_offset(other.scaling_offset), min_value(other.min_value),
        max_value(other.max_value), is_log_enabled(other.is_log_enabled),
        log_interval_ms(other.log_interval_ms),
        log_deadband(other.log_deadband),
        polling_interval_ms(other.polling_interval_ms),
        group(std::move(other.group)), tags(std::move(other.tags)),
        metadata(std::move(other.metadata)),
        protocol_params(std::move(other.protocol_params)),
        current_value(std::move(other.current_value)),
        raw_value(std::move(other.raw_value)), quality_code(other.quality_code),
        value_timestamp(other.value_timestamp),
        quality_timestamp(other.quality_timestamp),
        last_log_time(other.last_log_time),
        last_read_time(other.last_read_time),
        last_write_time(other.last_write_time), read_count(other.read_count),
        write_count(other.write_count), error_count(other.error_count),
        created_at(other.created_at), updated_at(other.updated_at) {}

  // 복사 할당 연산자
  DataPoint &operator=(const DataPoint &other) {
    if (this != &other) {
      id = other.id;
      device_id = other.device_id;
      name = other.name;
      description = other.description;
      address = other.address;
      address_string = other.address_string;
      mapping_key = other.mapping_key;
      data_type = other.data_type;
      access_mode = other.access_mode;
      is_enabled = other.is_enabled;
      is_writable = other.is_writable;
      unit = other.unit;
      scaling_factor = other.scaling_factor;
      scaling_offset = other.scaling_offset;
      min_value = other.min_value;
      max_value = other.max_value;
      is_log_enabled = other.is_log_enabled;
      log_interval_ms = other.log_interval_ms;
      log_deadband = other.log_deadband;
      polling_interval_ms = other.polling_interval_ms;
      group = other.group;
      tags = other.tags;
      metadata = other.metadata;
      protocol_params = other.protocol_params;
      current_value = other.current_value;
      raw_value = other.raw_value;
      quality_code = other.quality_code;
      value_timestamp = other.value_timestamp;
      quality_timestamp = other.quality_timestamp;
      last_log_time = other.last_log_time;
      last_read_time = other.last_read_time;
      last_write_time = other.last_write_time;
      read_count = other.read_count;
      write_count = other.write_count;
      error_count = other.error_count;
      created_at = other.created_at;
      updated_at = other.updated_at;
    }
    return *this;
  }

  // 이동 할당 연산자
  DataPoint &operator=(DataPoint &&other) noexcept {
    if (this != &other) {
      id = std::move(other.id);
      device_id = std::move(other.device_id);
      name = std::move(other.name);
      description = std::move(other.description);
      address = other.address;
      address_string = std::move(other.address_string);
      mapping_key = std::move(other.mapping_key);
      data_type = std::move(other.data_type);
      access_mode = std::move(other.access_mode);
      is_enabled = other.is_enabled;
      is_writable = other.is_writable;
      unit = std::move(other.unit);
      scaling_factor = other.scaling_factor;
      scaling_offset = other.scaling_offset;
      min_value = other.min_value;
      max_value = other.max_value;
      is_log_enabled = other.is_log_enabled;
      log_interval_ms = other.log_interval_ms;
      log_deadband = other.log_deadband;
      polling_interval_ms = other.polling_interval_ms;
      group = std::move(other.group);
      tags = std::move(other.tags);
      metadata = std::move(other.metadata);
      protocol_params = std::move(other.protocol_params);
      current_value = std::move(other.current_value);
      raw_value = std::move(other.raw_value);
      quality_code = other.quality_code;
      value_timestamp = other.value_timestamp;
      quality_timestamp = other.quality_timestamp;
      last_log_time = other.last_log_time;
      last_read_time = other.last_read_time;
      last_write_time = other.last_write_time;
      read_count = other.read_count;
      write_count = other.write_count;
      error_count = other.error_count;
      created_at = other.created_at;
      updated_at = other.updated_at;
    }
    return *this;
  }

  // =======================================================================
  // 🔥 실제 값 관리 메서드들 (핵심!)
  // =======================================================================

  /**
   * @brief 현재값 업데이트 (Worker에서 호출)
   * @param new_value 새로운 값
   * @param new_quality 새로운 품질 (기본: GOOD)
   * @param apply_scaling 스케일링 적용 여부 (기본: true)
   */
  void UpdateCurrentValue(const PulseOne::BasicTypes::DataVariant &new_value,
                          PulseOne::Enums::DataQuality new_quality =
                              PulseOne::Enums::DataQuality::GOOD,
                          bool apply_scaling = true) {

    // 원시값 저장
    raw_value = new_value;

    // 스케일링 적용 (숫자 타입만)
    if (apply_scaling) {
      current_value = std::visit(
          [this](const auto &v) -> PulseOne::BasicTypes::DataVariant {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
              double scaled =
                  (static_cast<double>(v) * scaling_factor) + scaling_offset;
              return PulseOne::BasicTypes::DataVariant{scaled};
            } else {
              return v; // 문자열은 그대로
            }
          },
          new_value);
    } else {
      current_value = new_value;
    }

    // 품질 업데이트
    if (quality_code != new_quality) {
      quality_code = new_quality;
      quality_timestamp = std::chrono::system_clock::now();
    }

    // 값 타임스탬프 업데이트
    value_timestamp = std::chrono::system_clock::now();
    updated_at = value_timestamp;

    // 읽기 카운트 증가 (atomic 제거)
    read_count++;
    last_read_time = value_timestamp;
  }

  /**
   * @brief 에러 상태로 설정
   * @param error_quality 에러 품질 (기본: BAD)
   */
  void SetErrorState(PulseOne::Enums::DataQuality error_quality =
                         PulseOne::Enums::DataQuality::BAD) {
    quality_code = error_quality;
    quality_timestamp = std::chrono::system_clock::now();
    error_count++;
  }

  /**
   * @brief 쓰기 작업 기록
   * @param written_value 쓴 값
   * @param success 성공 여부
   */
  void
  RecordWriteOperation(const PulseOne::BasicTypes::DataVariant &written_value,
                       bool success) {
    if (success) {
      // 쓰기 성공 시 현재값도 업데이트 (단방향 쓰기가 아닌 경우)
      if (access_mode == "write" || access_mode == "read_write") {
        UpdateCurrentValue(written_value, PulseOne::Enums::DataQuality::GOOD,
                           false);
      }
      write_count++;
    } else {
      error_count++;
    }
    last_write_time = std::chrono::system_clock::now();
  }

  /**
   * @brief 현재값을 문자열로 반환
   */
  std::string GetCurrentValueAsString() const {
    return std::visit(
        [](const auto &v) -> std::string {
          if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
            return v ? "true" : "false";
          } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                              std::string>) {
            return v;
          } else {
            return std::to_string(v);
          }
        },
        current_value);
  }

  /**
   * @brief 품질을 문자열로 반환
   */
  std::string GetQualityCodeAsString() const {
    switch (quality_code) {
    case PulseOne::Enums::DataQuality::GOOD:
      return "GOOD";
    case PulseOne::Enums::DataQuality::BAD:
      return "BAD";
    case PulseOne::Enums::DataQuality::UNCERTAIN:
      return "UNCERTAIN";
    case PulseOne::Enums::DataQuality::NOT_CONNECTED:
      return "NOT_CONNECTED";
    case PulseOne::Enums::DataQuality::TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
    }
  }

  /**
   * @brief 품질이 양호한지 확인
   */
  bool IsGoodQuality() const {
    return quality_code == PulseOne::Enums::DataQuality::GOOD;
  }

  /**
   * @brief 값이 유효 범위 내인지 확인
   */
  bool IsValueInRange() const {
    if (max_value <= min_value)
      return true; // 범위 설정되지 않음

    return std::visit(
        [this](const auto &v) -> bool {
          if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
            double val = static_cast<double>(v);
            return val >= min_value && val <= max_value;
          } else {
            return true; // 문자열은 항상 유효
          }
        },
        current_value);
  }

  /**
   * @brief TimestampedValue로 변환 (IProtocolDriver 인터페이스 호환)
   */
  TimestampedValue ToTimestampedValue() const {
    TimestampedValue tv;
    tv.value = current_value;
    tv.timestamp = value_timestamp;
    tv.quality = quality_code;
    tv.source = name;

    // 🔥 포인트 ID 전파 (문자열 -> 숫자 변환)
    try {
      if (!id.empty()) {
        tv.point_id = std::stoi(id);
      } else {
        tv.point_id = 0;
      }
    } catch (...) {
      tv.point_id = 0;
    }

    return tv;
  }

  /**
   * @brief TimestampedValue에서 값 업데이트
   */
  void FromTimestampedValue(const TimestampedValue &tv) {
    current_value = tv.value;
    value_timestamp = tv.timestamp;
    quality_code = tv.quality;
    read_count++;
    last_read_time = tv.timestamp;
  }

  // =======================================================================
  // 🔥 기존 호환성 메서드들 (Properties 맵 제거)
  // =======================================================================

  bool isWritable() const { return is_writable; }
  void setWritable(bool writable) { is_writable = writable; }

  std::string getDataType() const { return data_type; }
  void setDataType(const std::string &type) { data_type = type; }

  std::string getUnit() const { return unit; }
  void setUnit(const std::string &u) { unit = u; }

  // =======================================================================
  // 🔥 프로토콜별 편의 메서드들
  // =======================================================================

  /**
   * @brief Modbus 레지스터 주소 설정
   */
  void SetModbusAddress(uint16_t register_addr,
                        const std::string &reg_type = "HOLDING_REGISTER") {
    address = register_addr;
    address_string = std::to_string(register_addr);
    protocol_params["register_type"] = reg_type;
    protocol_params["function_code"] = (reg_type == "HOLDING_REGISTER") ? "3"
                                       : (reg_type == "INPUT_REGISTER") ? "4"
                                       : (reg_type == "COIL")           ? "1"
                                                                        : "2";
  }

  /**
   * @brief MQTT 토픽 설정
   */
  void SetMqttTopic(const std::string &topic,
                    const std::string &json_path = "") {
    address_string = topic;
    protocol_params["topic"] = topic;
    if (!json_path.empty()) {
      protocol_params["json_path"] = json_path;
    }
  }

  /**
   * @brief BACnet Object 설정
   */
  void SetBACnetObject(uint32_t object_instance,
                       const std::string &object_type = "ANALOG_INPUT",
                       const std::string &property_id = "PRESENT_VALUE") {
    address = object_instance;
    address_string = std::to_string(object_instance);
    protocol_params["object_type"] = object_type;
    protocol_params["property_id"] = property_id;
  }

  /**
   * @brief 스케일링 적용
   */
  double ApplyScaling(double raw_val) const {
    return (raw_val * scaling_factor) + scaling_offset;
  }

  /**
   * @brief 역스케일링 적용 (쓰기 시 사용)
   */
  double RemoveScaling(double scaled_val) const {
    return (scaled_val - scaling_offset) / scaling_factor;
  }

  // =======================================================================
  // 🔥 JSON 직렬화 (현재값 포함)
  // =======================================================================

  JsonType ToJson() const {
    JsonType j;

    // 기본 정보
    j["id"] = id;
    j["device_id"] = device_id;
    j["name"] = name;
    j["description"] = description;

    // 주소 정보
    j["address"] = address;
    j["address_string"] = address_string;

    // 데이터 타입
    j["data_type"] = data_type;
    j["access_mode"] = access_mode;
    j["is_enabled"] = is_enabled;
    j["is_writable"] = is_writable;

    // 엔지니어링 정보
    j["unit"] = unit;
    j["scaling_factor"] = scaling_factor;
    j["scaling_offset"] = scaling_offset;
    j["min_value"] = min_value;
    j["max_value"] = max_value;

    // 실제 값 (핵심!)
    j["current_value"] = GetCurrentValueAsString();
    j["quality_code"] = static_cast<int>(quality_code);
    j["quality_string"] = GetQualityCodeAsString();

    // 타임스탬프들 (milliseconds)
    j["value_timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            value_timestamp.time_since_epoch())
            .count();
    j["quality_timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            quality_timestamp.time_since_epoch())
            .count();

    // 통계
    j["read_count"] = read_count;
    j["write_count"] = write_count;
    j["error_count"] = error_count;

    return j;
  }
};
// =========================================================================
// 🔥 Phase 1: 스마트 포인터 기반 DriverConfig (Union 대체)
// =========================================================================

/**
 * @brief 통합 드라이버 설정 (스마트 포인터 기반)
 * @details
 * - 기존 모든 *DriverConfig 구조체 대체
 * - 무제한 확장성 (새 프로토콜 추가 용이)
 * - 메모리 효율성 (필요한 크기만 할당)
 * - 타입 안전성 (dynamic_cast 활용)
 */
struct DriverConfig {
  // =======================================================================
  // 🔥 공통 필드들 (기존 호환)
  // =======================================================================
  UniqueId device_id;               // 디바이스 ID
  std::string name = "";            // 디바이스 이름
  std::string protocol = "UNKNOWN"; // 프로토콜 타입 (문자열 방식)
  std::string endpoint = "";        // 연결 엔드포인트

  // 타이밍 설정
  uint32_t polling_interval_ms = 1000; // 폴링 간격
  uint32_t timeout_ms = 5000;          // 타임아웃
  int retry_count = 3;                 // 재시도 횟수
  bool auto_reconnect = true;          // 자동 재연결

  std::map<std::string, std::string>
      properties; // 🔥 프로토콜별 속성 저장 (통합 시스템 핵심)
  std::map<std::string, std::string> custom_settings;
  // =======================================================================
  // 🔥 핵심: 스마트 포인터 기반 프로토콜 설정
  // =======================================================================
  std::unique_ptr<IProtocolConfig> protocol_config;

  // =======================================================================
  // 🔥 생성자들
  // =======================================================================

  DriverConfig() = default;

  explicit DriverConfig(const std::string &proto) : protocol(proto) {
    protocol_config = CreateProtocolConfig(proto);
  }

  // 복사 생성자 (Clone 사용)
  DriverConfig(const DriverConfig &other)
      : device_id(other.device_id), name(other.name), protocol(other.protocol),
        endpoint(other.endpoint),
        polling_interval_ms(other.polling_interval_ms),
        timeout_ms(other.timeout_ms), retry_count(other.retry_count),
        auto_reconnect(other.auto_reconnect), properties(other.properties),
        custom_settings(other.custom_settings),
        protocol_config(other.protocol_config ? other.protocol_config->Clone()
                                              : nullptr) {}

  // 할당 연산자
  DriverConfig &operator=(const DriverConfig &other) {
    if (this != &other) {
      device_id = other.device_id;
      name = other.name;
      protocol = other.protocol;
      endpoint = other.endpoint;
      polling_interval_ms = other.polling_interval_ms;
      timeout_ms = other.timeout_ms;
      retry_count = other.retry_count;
      auto_reconnect = other.auto_reconnect;
      properties = other.properties;
      custom_settings = other.custom_settings;
      protocol_config =
          other.protocol_config ? other.protocol_config->Clone() : nullptr;
    }
    return *this;
  }

  // =======================================================================
  // 🔥 기존 코드 호환성 메서드들
  // =======================================================================

  bool IsModbus() const {
    return protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU" ||
           protocol == "MODBUS";
  }

  bool IsMqtt() const { return protocol == "MQTT" || protocol == "MQTT_5"; }

  bool IsBacnet() const {
    return protocol == "BACNET" || protocol == "BACNET_IP" ||
           protocol == "BACNET_MSTP";
  }

  bool IsValid() const {
    return !protocol.empty() && protocol != "UNKNOWN" && !endpoint.empty();
  }

  std::string GetProtocolName() const { return protocol; }

private:
  // =======================================================================
  // 🔥 팩토리 메서드
  // =======================================================================
  static std::unique_ptr<IProtocolConfig>
  CreateProtocolConfig(const std::string &type) {
    if (type == "MODBUS_TCP" || type == "MODBUS_RTU" || type == "MODBUS")
      return std::make_unique<ModbusConfig>();
    if (type == "MQTT" || type == "MQTT_5")
      return std::make_unique<MqttConfig>();
    if (type == "BACNET" || type == "BACNET_IP" || type == "BACNET_MSTP")
      return std::make_unique<BACnetConfig>();
    return nullptr;
  }
};

// =========================================================================
// 🔥 완전한 DeviceInfo 구조체 (모든 DB 테이블 필드 포함)
// =========================================================================

/**
 * @brief 완전 통합 디바이스 정보 구조체 (최종 완성판)
 * @details
 * ✅ DeviceEntity 모든 필드
 * ✅ DeviceSettingsEntity 모든 필드
 * ✅ 프로토콜별 설정은 properties 맵에 저장
 * ✅ 기존 DeviceInfo 호환성 100%
 */
struct DeviceInfo {
  // =======================================================================
  // 🔥 Phase 1: DeviceEntity 기본 필드들 (기존 유지)
  // =======================================================================

  // 핵심: 스마트 포인터 기반 DriverConfig
  DriverConfig driver_config;

  // 기본 식별 정보
  UniqueId id;                        // device_id → id (Entity 호환)
  int tenant_id = 0;                  // 테넌트 ID
  int site_id = 0;                    // 사이트 ID
  std::optional<int> device_group_id; // 디바이스 그룹 ID
  std::optional<int> edge_server_id;  // 엣지 서버 ID

  // 디바이스 기본 정보 (Entity 호환)
  std::string name = "";             // 디바이스 이름
  std::string description = "";      // 설명
  std::string device_type = "";      // 디바이스 타입 (PLC, HMI, SENSOR 등)
  std::string manufacturer = "";     // 제조사
  std::string model = "";            // 모델명
  std::string serial_number = "";    // 시리얼 번호
  std::string firmware_version = ""; // 펌웨어 버전

  // 통신 설정 (Entity 호환)
  int protocol_id = 0;                // 프로토콜 ID
  std::string protocol_type = "";     // 프로토콜 타입 (문자열)
  std::string endpoint = "";          // 엔드포인트
  std::string connection_string = ""; // 연결 문자열 (endpoint 별칭)
  std::string config = "";            // JSON 설정 (Entity 호환)

  // 네트워크 설정 (Entity 확장)
  std::string ip_address = "";  // IP 주소
  int port = 0;                 // 포트 번호
  std::string mac_address = ""; // MAC 주소

  // 기본 타이밍 설정 (Entity 확장)
  int polling_interval_ms = 1000; // 폴링 간격 (기본)
  int timeout_ms = 5000;          // 타임아웃 (기본)
  int retry_count = 3;            // 재시도 횟수 (기본)

  // 상태 정보 (Entity 호환)
  bool is_enabled = true; // 활성화 상태
  bool enabled = true;    // is_enabled 별칭
  ConnectionStatus connection_status = ConnectionStatus::DISCONNECTED;

  // 위치 및 물리적 정보 (Entity 확장)
  std::string location = "";      // 설치 위치
  std::string rack_position = ""; // 랙 위치
  std::string building = "";      // 건물
  std::string floor = "";         // 층
  std::string room = "";          // 방

  // 유지보수 정보 (Entity 확장)
  Timestamp installation_date;        // 설치일
  Timestamp last_maintenance;         // 마지막 점검일
  Timestamp next_maintenance;         // 다음 점검 예정일
  std::string maintenance_notes = ""; // 점검 메모

  // 메타데이터 (Entity + DeviceSettings 호환)
  std::string tags = "";                         // JSON 배열 형태
  std::vector<std::string> tag_list;             // 태그 목록
  std::map<std::string, std::string> metadata;   // 추가 메타데이터
  std::map<std::string, std::string> properties; // 🔥 커스텀 속성들 (핵심!)

  // 시간 정보 (Entity 호환)
  Timestamp created_at;
  Timestamp updated_at;
  int created_by = 0; // 생성자 ID

  // 성능 및 모니터링 (DeviceSettings 호환)
  bool is_monitoring_enabled = true;             // 모니터링 활성화
  std::string log_level = "INFO";                // 로그 레벨
  bool is_diagnostics_enabled = false;           // 진단 모드
  bool is_performance_monitoring_enabled = true; // 성능 모니터링

  // 보안 설정 (DeviceSettings 확장)
  std::string security_level = "NORMAL"; // 보안 레벨
  bool encryption_enabled = false;       // 암호화 사용
  std::string certificate_path = "";     // 인증서 경로

  // =======================================================================
  // 🔥 Phase 2: DeviceSettingsEntity 필드들 직접 추가!
  // =======================================================================

  // 🔥 폴링 및 타이밍 설정 (device_settings 테이블)
  std::optional<int> scan_rate_override; // scan_rate_override

  // 🔥 연결 및 통신 설정 (device_settings 테이블)
  std::optional<int> connection_timeout_ms; // connection_timeout_ms
  std::optional<int> read_timeout_ms;       // read_timeout_ms
  std::optional<int> write_timeout_ms;      // write_timeout_ms

  // 🔥 재시도 정책 (device_settings 테이블)
  int max_retry_count = 3;          // max_retry_count
  int retry_interval_ms = 5000;     // retry_interval_ms
  double backoff_multiplier = 1.5;  // backoff_multiplier
  int backoff_time_ms = 60000;      // backoff_time_ms
  int max_backoff_time_ms = 300000; // max_backoff_time_ms

  // 🔥 Keep-alive 설정 (device_settings 테이블)
  bool is_keep_alive_enabled = true; // keep_alive_enabled
  int keep_alive_interval_s = 30;    // keep_alive_interval_s
  int keep_alive_timeout_s = 10;     // keep_alive_timeout_s

  // 🔥 데이터 품질 관리 (device_settings 테이블)
  bool is_data_validation_enabled = true;    // data_validation_enabled
  bool is_outlier_detection_enabled = false; // outlier_detection_enabled
  bool is_deadband_enabled = true;           // deadband_enabled

  // 🔥 로깅 및 진단 (device_settings 테이블)
  bool is_detailed_logging_enabled = false; // detailed_logging_enabled
  // performance_monitoring_enabled 필드는 상위와 중복되어 하나로 통합
  // (is_performance_monitoring_enabled)
  bool is_diagnostic_mode_enabled = false; // diagnostic_mode_enabled
  bool is_auto_registration_enabled =
      false; // 자동 등록 활성화 여부 (discovery)

  // 🔥 메타데이터 (device_settings 테이블)
  int updated_by = 0; // updated_by

  // =======================================================================
  // 🔥 생성자들
  // =======================================================================

  DeviceInfo() {
    auto now = std::chrono::system_clock::now();
    created_at = now;
    updated_at = now;
    installation_date = now;
    last_maintenance = now;
    next_maintenance = now;

    // 별칭 동기화
    enabled = is_enabled;
    connection_string = endpoint;
  }

  explicit DeviceInfo(const std::string &protocol) : DeviceInfo() {
    driver_config = DriverConfig(protocol);
    protocol_type = protocol;
    InitializeProtocolDefaults(protocol);
  }

  // =======================================================================
  // 🔥 프로토콜별 기본값 초기화 (properties 맵 활용)
  // =======================================================================

  /**
   * @brief 프로토콜별 기본 properties 설정 (기존 코드 완전 호환)
   * @note 🔥 Phase 1: 기존 메서드들 100% 유지하면서 내부만 설정 기반으로 변경
   */
  void InitializeProtocolDefaults(const std::string &protocol) {
    if (protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU") {
      properties["slave_id"] = "1";
      properties["function_code"] = "3";
      properties["byte_order"] = "big_endian";
      properties["word_order"] = "high_low";
      properties["register_type"] = "HOLDING_REGISTER";
      properties["max_registers_per_group"] = "125";
      properties["auto_group_creation"] = "true";
    } else if (protocol == "MQTT") {
      properties["client_id"] = "";
      properties["username"] = "";
      properties["password"] = "";
      properties["qos_level"] = "1";
      properties["clean_session"] = "true";
      properties["retain"] = "false";
      properties["keep_alive"] = "60";
    } else if (protocol == "BACNET" || protocol == "BACNET_IP") {
      properties["device_id"] = "1001";
      properties["network_number"] = "1";
      properties["max_apdu_length"] = "1476";
      properties["segmentation_support"] = "both";
      properties["vendor_id"] = "0";
    } else {
      properties["auto_reconnect"] = "true";
      properties["ssl_enabled"] = "false";
      properties["validate_certificates"] = "true";
    }
  }

  // =======================================================================
  // 🔥 properties 맵 헬퍼 메서드들
  // =======================================================================

  /**
   * @brief properties에서 값 가져오기 (기본값 포함)
   */
  std::string GetProperty(const std::string &key,
                          const std::string &default_value = "") const {
    auto it = properties.find(key);
    return (it != properties.end()) ? it->second : default_value;
  }

  /**
   * @brief properties에 값 설정
   */
  void SetProperty(const std::string &key, const std::string &value) {
    properties[key] = value;
    // DriverConfig와 동기화
    driver_config.properties[key] = value;
  }

  /**
   * @brief properties에서 정수값 가져오기
   */
  int GetPropertyInt(const std::string &key, int default_value = 0) const {
    std::string value = GetProperty(key);
    if (value.empty())
      return default_value;
    try {
      return std::stoi(value);
    } catch (...) {
      return default_value;
    }
  }

  /**
   * @brief properties에서 불린값 가져오기
   */
  bool GetPropertyBool(const std::string &key,
                       bool default_value = false) const {
    std::string value = GetProperty(key);
    if (value.empty())
      return default_value;
    return (value == "true" || value == "1" || value == "yes");
  }

  // =======================================================================
  // 🔥 프로토콜별 편의 메서드들 (범용 properties 기반 - 중복 제거!)
  // =======================================================================

  // 🔥 이미 GetProperty(), SetProperty(), GetPropertyInt(), GetPropertyBool()
  // 메서드들이 있으니 개별 프로토콜 메서드들은 필요 없음! 간단하게 범용
  // 메서드만 사용하자.

  // =======================================================================
  // 🔥 기존 DeviceEntity 호환 getter/setter 메서드들 (기존 API 100% 보존)
  // =======================================================================

  // 식별 정보
  const UniqueId &getId() const { return id; }
  void setId(const UniqueId &device_id) { id = device_id; }

  int getTenantId() const { return tenant_id; }
  void setTenantId(int tenant) { tenant_id = tenant; }

  int getSiteId() const { return site_id; }
  void setSiteId(int site) { site_id = site; }

  std::optional<int> getDeviceGroupId() const { return device_group_id; }
  void setDeviceGroupId(const std::optional<int> &group_id) {
    device_group_id = group_id;
  }
  void setDeviceGroupId(int group_id) { device_group_id = group_id; }

  std::optional<int> getEdgeServerId() const { return edge_server_id; }
  void setEdgeServerId(const std::optional<int> &server_id) {
    edge_server_id = server_id;
  }
  void setEdgeServerId(int server_id) { edge_server_id = server_id; }

  // 기본 정보
  const std::string &getName() const { return name; }
  void setName(const std::string &device_name) {
    name = device_name;
    driver_config.name = device_name; // 동기화
  }

  const std::string &getDescription() const { return description; }
  void setDescription(const std::string &desc) { description = desc; }

  const std::string &getDeviceType() const { return device_type; }
  void setDeviceType(const std::string &type) { device_type = type; }

  const std::string &getManufacturer() const { return manufacturer; }
  void setManufacturer(const std::string &mfg) { manufacturer = mfg; }

  const std::string &getModel() const { return model; }
  void setModel(const std::string &device_model) { model = device_model; }

  const std::string &getSerialNumber() const { return serial_number; }
  void setSerialNumber(const std::string &serial) { serial_number = serial; }

  const std::string &getFirmwareVersion() const { return firmware_version; }
  void setFirmwareVersion(const std::string &version) {
    firmware_version = version;
  }

  // 통신 설정
  const std::string &getProtocolType() const { return protocol_type; }
  void setProtocolType(const std::string &protocol) {
    protocol_type = protocol;
    driver_config.protocol = protocol;
  }

  const std::string &getEndpoint() const { return endpoint; }
  void setEndpoint(const std::string &ep) {
    endpoint = ep;
    connection_string = ep;      // 별칭 동기화
    driver_config.endpoint = ep; // 동기화
  }

  const std::string &getConfig() const { return config; }
  void setConfig(const std::string &cfg) { config = cfg; }

  const std::string &getIpAddress() const { return ip_address; }
  void setIpAddress(const std::string &ip) { ip_address = ip; }

  int getPort() const { return port; }
  void setPort(int port_num) { port = port_num; }

  /**
   * @brief 프로토콜 문자열을 ProtocolType 열거형으로 변환
   */
  /**
   * @brief 프로토콜 문자열 정규화 (StringToProtocolType 대체)
   */
  static std::string StringToProtocolType(const std::string &type_str) {
    // 이제 숫자로 변환하지 않고 문자열 그대로 사용하되, 정규화만 수행
    std::string upper = type_str;
    for (auto &c : upper)
      c = toupper(c);

    if (upper == "MODBUS")
      return "MODBUS_TCP";
    if (upper == "BACNET_IP")
      return "BACNET";
    if (upper == "ROS_BRIDGE")
      return "ROS";
    if (upper == "HTTP_REST")
      return "HTTP";

    return upper;
  }

  // 상태 정보
  bool isEnabled() const { return is_enabled; }
  void setEnabled(bool enable) {
    is_enabled = enable;
    enabled = enable; // 별칭 동기화
  }

  bool getEnabled() const { return enabled; } // 별칭 메서드

  ConnectionStatus getConnectionStatus() const { return connection_status; }
  void setConnectionStatus(ConnectionStatus status) {
    connection_status = status;
  }

  // 위치 정보
  const std::string &getLocation() const { return location; }
  void setLocation(const std::string &loc) { location = loc; }

  const std::string &getBuilding() const { return building; }
  void setBuilding(const std::string &bldg) { building = bldg; }

  const std::string &getFloor() const { return floor; }
  void setFloor(const std::string &fl) { floor = fl; }

  const std::string &getRoom() const { return room; }
  void setRoom(const std::string &rm) { room = rm; }

  // 시간 정보
  const Timestamp &getCreatedAt() const { return created_at; }
  void setCreatedAt(const Timestamp &timestamp) { created_at = timestamp; }

  const Timestamp &getUpdatedAt() const { return updated_at; }
  void setUpdatedAt(const Timestamp &timestamp) { updated_at = timestamp; }

  int getCreatedBy() const { return created_by; }
  void setCreatedBy(int user_id) { created_by = user_id; }

  // =======================================================================
  // 🔥 새로 추가된 DeviceSettings 필드들 getter/setter
  // =======================================================================

  // 타이밍 설정
  int getConnectionTimeoutMs() const {
    return connection_timeout_ms.value_or(timeout_ms);
  }
  void setConnectionTimeoutMs(int timeout) { connection_timeout_ms = timeout; }

  int getReadTimeoutMs() const { return read_timeout_ms.value_or(timeout_ms); }
  void setReadTimeoutMs(int timeout) { read_timeout_ms = timeout; }

  int getWriteTimeoutMs() const {
    return write_timeout_ms.value_or(timeout_ms);
  }
  void setWriteTimeoutMs(int timeout) { write_timeout_ms = timeout; }

  std::optional<int> getScanRateOverride() const { return scan_rate_override; }
  void setScanRateOverride(int rate) { scan_rate_override = rate; }

  // 재시도 정책
  int getMaxRetryCount() const { return max_retry_count; }
  void setMaxRetryCount(int count) { max_retry_count = count; }

  int getRetryIntervalMs() const { return retry_interval_ms; }
  void setRetryIntervalMs(int interval) { retry_interval_ms = interval; }

  double getBackoffMultiplier() const { return backoff_multiplier; }
  void setBackoffMultiplier(double multiplier) {
    backoff_multiplier = multiplier;
  }

  int getBackoffTimeMs() const { return backoff_time_ms; }
  void setBackoffTimeMs(int time) { backoff_time_ms = time; }

  int getMaxBackoffTimeMs() const { return max_backoff_time_ms; }
  void setMaxBackoffTimeMs(int time) { max_backoff_time_ms = time; }

  // Keep-alive 설정
  bool isKeepAliveEnabled() const { return is_keep_alive_enabled; }
  void setKeepAliveEnabled(bool enabled) { is_keep_alive_enabled = enabled; }

  int getKeepAliveIntervalS() const { return keep_alive_interval_s; }
  void setKeepAliveIntervalS(int interval) { keep_alive_interval_s = interval; }

  int getKeepAliveTimeoutS() const { return keep_alive_timeout_s; }
  void setKeepAliveTimeoutS(int timeout) { keep_alive_timeout_s = timeout; }

  // 데이터 품질 관리
  bool isDataValidationEnabled() const { return is_data_validation_enabled; }
  void setDataValidationEnabled(bool enabled) {
    is_data_validation_enabled = enabled;
  }

  bool isOutlierDetectionEnabled() const {
    return is_outlier_detection_enabled;
  }
  void setOutlierDetectionEnabled(bool enabled) {
    is_outlier_detection_enabled = enabled;
  }

  bool isDeadbandEnabled() const { return is_deadband_enabled; }
  void setDeadbandEnabled(bool enabled) { is_deadband_enabled = enabled; }

  // 로깅 및 진단
  bool isDetailedLoggingEnabled() const { return is_detailed_logging_enabled; }
  void setDetailedLoggingEnabled(bool enabled) {
    is_detailed_logging_enabled = enabled;
  }

  bool isPerformanceMonitoringEnabled() const {
    return is_performance_monitoring_enabled;
  }
  void setPerformanceMonitoringEnabled(bool enabled) {
    is_performance_monitoring_enabled = enabled;
  }

  bool isDiagnosticModeEnabled() const { return is_diagnostic_mode_enabled; }
  void setDiagnosticModeEnabled(bool enabled) {
    is_diagnostic_mode_enabled = enabled;
  }

  // 메타데이터
  int getUpdatedBy() const { return updated_by; }
  void setUpdatedBy(int user_id) { updated_by = user_id; }

  // =======================================================================
  // 🔥 Worker 호환 메서드들 (기존 Worker 클래스들 호환)
  // =======================================================================

  // 프로토콜 타입 확인 (기존 Worker 코드 호환)
  bool IsModbus() const { return driver_config.IsModbus(); }
  bool IsMqtt() const { return driver_config.IsMqtt(); }
  bool IsBacnet() const { return driver_config.IsBacnet(); }

  // 타이밍 설정 (Worker에서 사용)
  uint32_t GetPollingInterval() const {
    return static_cast<uint32_t>(polling_interval_ms);
  }
  void SetPollingInterval(uint32_t interval_ms) {
    polling_interval_ms = static_cast<int>(interval_ms);
    driver_config.polling_interval_ms = interval_ms;
  }

  uint32_t GetTimeout() const {
    return static_cast<uint32_t>(getConnectionTimeoutMs());
  }
  void SetTimeout(uint32_t timeout) {
    setConnectionTimeoutMs(static_cast<int>(timeout));
    driver_config.timeout_ms = timeout;
  }

  // =======================================================================
  // 🔥 DriverConfig 위임 메서드들
  // =======================================================================

  /**
   * @brief IProtocolDriver 호환 - DriverConfig 접근
   */
  DriverConfig &GetDriverConfig() {
    SyncToDriverConfig(); // 동기화 후 반환
    return driver_config;
  }
  const DriverConfig &GetDriverConfig() const { return driver_config; }

  // =======================================================================
  // 🔥 변환 및 동기화 메서드들
  // =======================================================================

  /**
   * @brief 필드 동기화 (별칭 필드들 동기화)
   */
  void SyncAliasFields() {
    enabled = is_enabled;
    connection_string = endpoint;

    // 🔥 DeviceSettings → DriverConfig 완전 매핑
    driver_config.protocol = protocol_type;
    driver_config.name = name;
    driver_config.endpoint = endpoint;
    driver_config.polling_interval_ms =
        static_cast<uint32_t>(polling_interval_ms);

    // 🔥 수정: connection_timeout_ms 사용 (optional 안전 처리)
    if (connection_timeout_ms.has_value()) {
      driver_config.timeout_ms =
          static_cast<uint32_t>(connection_timeout_ms.value());
    } else {
      driver_config.timeout_ms = static_cast<uint32_t>(timeout_ms); // fallback
    }

    // 🔥 수정: max_retry_count 사용
    driver_config.retry_count = static_cast<uint32_t>(max_retry_count);
  }

  /**
   * @brief DriverConfig로 완전 동기화 (DeviceSettings 포함)
   */
  void SyncToDriverConfig() {
    SyncAliasFields();
    driver_config.device_id = id;

    // 🔥 DeviceSettings 전용 필드들 추가 매핑 (타입별 올바른 처리)

    // =======================================================================
    // 추가 타이밍 설정 (optional 타입들)
    // =======================================================================
    if (read_timeout_ms.has_value()) {
      driver_config.properties["read_timeout_ms"] =
          std::to_string(read_timeout_ms.value());
    }
    if (write_timeout_ms.has_value()) {
      driver_config.properties["write_timeout_ms"] =
          std::to_string(write_timeout_ms.value());
    }
    if (scan_rate_override.has_value()) {
      driver_config.properties["scan_rate_override"] =
          std::to_string(scan_rate_override.value());
    }

    // =======================================================================
    // 🔥 재시도 정책 (int 타입들 - .has_value() 제거)
    // =======================================================================
    driver_config.properties["retry_interval_ms"] =
        std::to_string(retry_interval_ms);
    driver_config.properties["backoff_multiplier"] =
        std::to_string(backoff_multiplier);

    // 🔥 수정: int 타입이므로 직접 변환 (optional이 아님)
    driver_config.properties["backoff_time_ms"] =
        std::to_string(backoff_time_ms);
    driver_config.properties["max_backoff_time_ms"] =
        std::to_string(max_backoff_time_ms);

    // =======================================================================
    // 🔥 Keep-alive 설정 (int 타입들 - .has_value() 제거)
    // =======================================================================
    driver_config.properties["keep_alive_enabled"] =
        is_keep_alive_enabled ? "true" : "false";
    driver_config.properties["keep_alive_interval_s"] =
        std::to_string(keep_alive_interval_s);

    // 🔥 수정: int 타입이므로 직접 변환 (optional이 아님)
    driver_config.properties["keep_alive_timeout_s"] =
        std::to_string(keep_alive_timeout_s);

    // =======================================================================
    // 모니터링 설정
    // =======================================================================
    driver_config.properties["data_validation_enabled"] =
        is_data_validation_enabled ? "true" : "false";
    driver_config.properties["performance_monitoring_enabled"] =
        is_performance_monitoring_enabled ? "true" : "false";
    driver_config.properties["diagnostic_mode_enabled"] =
        is_diagnostic_mode_enabled ? "true" : "false";
    driver_config.properties["auto_registration_enabled"] =
        is_auto_registration_enabled ? "true" : "false";

    // =======================================================================
    // 🔥 마지막에 JSON config의 properties 복사 (오버라이드 가능)
    // =======================================================================
    for (const auto &[key, value] : properties) {
      driver_config.properties[key] = value;
    }

    // =======================================================================
    // 🔥 자동 재연결 설정 (필드가 없으면 기본값 사용)
    // =======================================================================
    // auto_reconnect 필드가 DeviceInfo에 없으므로 기본값으로 설정
    driver_config.auto_reconnect = true; // 기본값: 자동 재연결 활성화

    // 만약 properties에 설정이 있다면 오버라이드
    if (properties.count("auto_reconnect")) {
      driver_config.auto_reconnect =
          (properties.at("auto_reconnect") == "true");
    }

    // =======================================================================
    // 🔥 프로토콜 타입 설정
    // =======================================================================
    if (protocol_type == "MODBUS_TCP") {
      driver_config.protocol = "MODBUS_TCP";
    } else if (protocol_type == "MODBUS_RTU") {
      driver_config.protocol = "MODBUS_RTU";
    } else if (protocol_type == "MQTT") {
      driver_config.protocol = "MQTT";
    } else if (protocol_type == "BACNET_IP") {
      driver_config.protocol = "BACNET_IP";
    }
  }

  /**
   * @brief JSON config 문자열을 properties로 파싱
   */
  void ParseConfigToProperties() {
    if (config.empty() || config == "{}")
      return;

    try {
      JsonType json_config = JsonType::parse(config);
      for (auto &[key, value] : json_config.items()) {
        if (value.is_string()) {
          properties[key] = value.get<std::string>();
        } else if (value.is_number_integer()) {
          properties[key] = std::to_string(value.get<int>());
        } else if (value.is_number_float()) {
          properties[key] = std::to_string(value.get<double>());
        } else if (value.is_boolean()) {
          properties[key] = value.get<bool>() ? "true" : "false";
        }
      }
    } catch (const std::exception &e) {
      // JSON 파싱 실패 시 무시
    }
  }

  /**
   * @brief properties를 JSON config 문자열로 직렬화
   */
  void SerializePropertiesToConfig() {
    JsonType json_config = JsonType::object();
    for (const auto &[key, value] : properties) {
      json_config[key] = value;
    }
    config = json_config.dump();
  }

  /**
   * @brief 유효성 검증
   */
  bool IsValid() const {
    return !name.empty() && !protocol_type.empty() && !endpoint.empty() &&
           driver_config.IsValid();
  }

  /**
   * @brief JSON 직렬화 (DeviceEntity 호환)
   */
  JsonType ToJson() const {
    JsonType j;

    // 기본 정보
    j["id"] = id;
    j["tenant_id"] = tenant_id;
    j["site_id"] = site_id;
    if (device_group_id.has_value())
      j["device_group_id"] = device_group_id.value();
    if (edge_server_id.has_value())
      j["edge_server_id"] = edge_server_id.value();

    // 디바이스 정보
    j["name"] = name;
    j["description"] = description;
    j["device_type"] = device_type;
    j["manufacturer"] = manufacturer;
    j["model"] = model;
    j["serial_number"] = serial_number;
    j["firmware_version"] = firmware_version;

    // 통신 설정
    j["protocol_type"] = protocol_type;
    j["endpoint"] = endpoint;
    j["config"] = config;
    j["ip_address"] = ip_address;
    j["port"] = port;

    // 상태 정보
    j["is_enabled"] = is_enabled;
    j["connection_status"] = static_cast<int>(connection_status);

    // 🔥 DeviceSettings 필드들
    j["connection_timeout_ms"] = getConnectionTimeoutMs();
    j["read_timeout_ms"] = getReadTimeoutMs();
    j["write_timeout_ms"] = getWriteTimeoutMs();
    j["max_retry_count"] = max_retry_count;
    j["retry_interval_ms"] = retry_interval_ms;
    j["backoff_multiplier"] = backoff_multiplier;
    j["keep_alive_enabled"] = is_keep_alive_enabled;
    j["keep_alive_interval_s"] = keep_alive_interval_s;
    j["data_validation_enabled"] = is_data_validation_enabled;
    j["performance_monitoring_enabled"] = is_performance_monitoring_enabled;
    j["diagnostic_mode_enabled"] = is_diagnostic_mode_enabled;

    // 🔥 properties 포함
    j["properties"] = properties;

    // 시간 정보
    auto time_t = std::chrono::system_clock::to_time_t(created_at);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    j["created_at"] = std::string(buffer);

    j["created_by"] = created_by;

    return j;
  }

  // =======================================================================
  // 🔥 기존 typedef 호환성 (기존 코드에서 사용하던 타입명들)
  // =======================================================================

  // Worker에서 사용하던 별칭들
  std::string GetProtocolName() const {
    return driver_config.GetProtocolName();
  }
  std::string GetProtocol() const { return driver_config.protocol; }
};

// =========================================================================
// 🔥 메시지 전송용 확장 (향후 사용)
// =========================================================================
struct DeviceDataMessage {
  // ==========================================================================
  // 🔥 기존 필드들 (그대로 유지)
  // ==========================================================================
  std::string type = "device_data"; // 메시지 타입 (고정값)
  UniqueId device_id;               // 디바이스 고유 ID
  std::string protocol;             // 통신 프로토콜명 (modbus, bacnet, mqtt 등)
  std::string
      device_type; // 디바이스 타입 (ROBOT, PLC, SENSOR 등) - Hybrid Strategy용
  std::vector<TimestampedValue> points; // 수집된 데이터포인트들
  Timestamp timestamp;                  // 메시지 생성 시간
  uint32_t priority = 0;                // 처리 우선순위 (0=일반, 1=높음)

  // 멀티테넌트 지원
  int tenant_id = 0;      // 테넌트 ID (0=기본)
  int site_id = 0;        // 사이트 ID (0=기본)
  int edge_server_id = 0; // 엣지 서버 ID

  // 처리 제어
  bool trigger_alarms = true;          // 알람 체크 수행 여부
  bool trigger_virtual_points = false; // 가상포인트 계산 수행 여부
  bool high_priority = false;          // 고우선순위 처리 여부

  // 추적 정보
  std::string correlation_id = ""; // 요청 추적 ID (로그 연결용)
  std::string source_worker = "";  // 메시지 생성한 Worker명
  uint32_t batch_sequence = 0;     // 배치 내 시퀀스 번호

  // ==========================================================================
  // 🔥 디바이스 상태 정보 (새로 추가)
  // ==========================================================================

  // 현재 디바이스 상태 (5가지 기본 상태)
  Enums::DeviceStatus device_status =
      Enums::DeviceStatus::ONLINE; // 현재 디바이스 상태
  Enums::DeviceStatus previous_status =
      Enums::DeviceStatus::ONLINE; // 이전 상태 (상태변화 감지용)
  bool status_changed = false;     // 상태 변경 여부 (알림 트리거용)

  // 상태 관리 정보
  bool manual_status =
      false; // 수동 설정 여부 (관리자가 MAINTENANCE 설정시 true)
  std::string status_message = ""; // 상태 설명 메시지 (오류 내용, 점검 사유 등)
  Timestamp status_changed_time;   // 상태 마지막 변경 시간
  std::string status_changed_by = ""; // 상태 변경 주체 (system/admin/user_id)

  // 통신 상태 정보 (Worker에서 자동 업데이트, 상태 자동 판단용)
  bool is_connected = false;         // 현재 통신 연결 상태
  uint32_t consecutive_failures = 0; // 연속 실패 횟수 (3회 → OFFLINE)
  uint32_t total_failures = 0;       // 총 실패 횟수 (세션 시작부터)
  uint32_t total_attempts = 0;       // 총 시도 횟수 (성공률 계산용)
  std::chrono::milliseconds response_time{
      0};                      // 마지막 응답 시간 (5초 초과 → WARNING)
  Timestamp last_success_time; // 마지막 성공 통신 시간
  Timestamp last_attempt_time; // 마지막 통신 시도 시간
  std::string last_error_message =
      "";                  // 마지막 오류 메시지 (상태 메시지에 표시)
  int last_error_code = 0; // 마지막 오류 코드 (프로토콜별)

  // 포인트 상태 정보 (PARTIAL/ERROR 자동 판단용)
  uint32_t total_points_configured = 0; // 이 디바이스에 설정된 총 포인트 수
  uint32_t successful_points = 0;       // 성공적으로 읽은 포인트 수
  uint32_t failed_points =
      0; // 실패한 포인트 수 (30% 실패 → WARNING, 70% → ERROR)

  // ==========================================================================
  // 🔥 생성자들
  // ==========================================================================

  DeviceDataMessage() : timestamp(std::chrono::system_clock::now()) {}

  DeviceDataMessage(const UniqueId &id, const std::string &proto,
                    const std::string &worker = "")
      : device_id(id), protocol(proto),
        timestamp(std::chrono::system_clock::now()), source_worker(worker) {
    correlation_id = GenerateCorrelationId();
  }

  // ==========================================================================
  // 🔥 디바이스 상태 관리 함수들
  // ==========================================================================

  /**
   * @brief 디바이스 상태 자동 판단 및 업데이트
   * @param thresholds 임계값 설정
   */
  inline void
  UpdateDeviceStatus(const StatusThresholds &thresholds = StatusThresholds{}) {
    if (manual_status) {
      return; // 수동 설정된 상태는 자동 변경하지 않음
    }

    previous_status = device_status;

    // 연속 실패 횟수로 OFFLINE 판단
    if (consecutive_failures >= thresholds.offline_failure_count) {
      device_status = Enums::DeviceStatus::OFFLINE;
      status_message =
          "연속 " + std::to_string(consecutive_failures) + "회 통신 실패";
    }
    // 포인트 실패율로 ERROR/WARNING 판단
    else if (total_points_configured > 0) {
      double failure_ratio =
          static_cast<double>(failed_points) / total_points_configured;

      if (failure_ratio >= thresholds.error_failure_ratio) {
        device_status = Enums::DeviceStatus::DEVICE_ERROR;
        status_message = "포인트 " +
                         std::to_string(static_cast<int>(failure_ratio * 100)) +
                         "% 실패";
      } else if (failure_ratio >= thresholds.partial_failure_ratio) {
        device_status = Enums::DeviceStatus::WARNING;
        status_message = "포인트 " +
                         std::to_string(static_cast<int>(failure_ratio * 100)) +
                         "% 실패";
      }
      // 응답 시간으로 WARNING 판단
      else if (response_time > thresholds.timeout_threshold) {
        device_status = Enums::DeviceStatus::WARNING;
        status_message =
            "응답 지연 (" + std::to_string(response_time.count()) + "ms)";
      } else if (is_connected && consecutive_failures == 0) {
        device_status = Enums::DeviceStatus::ONLINE;
        status_message = "정상";
      }
    }

    // 상태 변경 감지
    if (device_status != previous_status) {
      status_changed = true;
      status_changed_time = std::chrono::system_clock::now();
      status_changed_by = "system";
    }
  }

  /**
   * @brief 수동 상태 설정 (관리자용)
   * @param new_status 새로운 상태
   * @param message 상태 메시지
   * @param user_id 설정한 사용자 ID
   */
  inline void SetManualStatus(Enums::DeviceStatus new_status,
                              const std::string &message = "",
                              const std::string &user_id = "admin") {
    previous_status = device_status;
    device_status = new_status;
    manual_status = (new_status == Enums::DeviceStatus::MAINTENANCE);
    status_message =
        message.empty() ? Enums::DeviceStatusToString(new_status) : message;
    status_changed = (device_status != previous_status);
    status_changed_time = std::chrono::system_clock::now();
    status_changed_by = user_id;
  }

  /**
   * @brief 통신 시도 결과 업데이트
   * @param success 성공 여부
   * @param error_msg 오류 메시지 (실패시)
   * @param error_code 오류 코드 (실패시)
   * @param resp_time 응답 시간
   */
  inline void UpdateCommunicationResult(
      bool success, const std::string &error_msg = "", int error_code = 0,
      std::chrono::milliseconds resp_time = std::chrono::milliseconds{0}) {
    total_attempts++;
    last_attempt_time = std::chrono::system_clock::now();
    response_time = resp_time;

    if (success) {
      consecutive_failures = 0;
      is_connected = true;
      last_success_time = last_attempt_time;
      last_error_message = "";
      last_error_code = 0;
    } else {
      consecutive_failures++;
      total_failures++;
      is_connected = false;
      last_error_message = error_msg;
      last_error_code = error_code;
    }
  }

  /**
   * @brief 포인트 처리 결과 업데이트
   * @param configured_count 설정된 포인트 수
   * @param success_count 성공한 포인트 수
   * @param fail_count 실패한 포인트 수
   */
  inline void UpdatePointResults(uint32_t configured_count,
                                 uint32_t success_count, uint32_t fail_count) {
    total_points_configured = configured_count;
    successful_points = success_count;
    failed_points = fail_count;
  }

  // ==========================================================================
  // 🔥 데이터 조회 및 분석 함수들
  // ==========================================================================

  /**
   * @brief 변화된 포인트들만 반환
   */
  inline std::vector<TimestampedValue> GetChangedPoints() const {
    std::vector<TimestampedValue> changed;
    for (const auto &point : points) {
      if (point.value_changed || point.force_rdb_store) {
        changed.push_back(point);
      }
    }
    return changed;
  }

  /**
   * @brief RDB 저장이 필요한 포인트들만 반환
   */
  inline std::vector<TimestampedValue> GetRDBStorePoints() const {
    std::vector<TimestampedValue> rdb_points;
    for (const auto &point : points) {
      if (point.ShouldStoreToRDB()) {
        rdb_points.push_back(point);
      }
    }
    return rdb_points;
  }

  /**
   * @brief 알람 체크가 필요한 포인트들만 반환
   */
  inline std::vector<TimestampedValue> GetAlarmCheckPoints() const {
    std::vector<TimestampedValue> alarm_points;
    for (const auto &point : points) {
      if (point.ShouldCheckAlarms()) {
        alarm_points.push_back(point);
      }
    }
    return alarm_points;
  }

  /**
   * @brief 디지털/아날로그 포인트 분류
   */
  inline std::pair<std::vector<TimestampedValue>, std::vector<TimestampedValue>>
  GetDigitalAndAnalogPoints() const {
    std::vector<TimestampedValue> digital, analog;
    for (const auto &point : points) {
      if (point.IsDigital()) {
        digital.push_back(point);
      } else {
        analog.push_back(point);
      }
    }
    return {digital, analog};
  }

  /**
   * @brief 메시지 통계 조회
   */
  struct MessageStats {
    size_t total_points = 0;
    size_t changed_points = 0;
    size_t rdb_store_points = 0;
    size_t alarm_check_points = 0;
    size_t digital_points = 0;
    size_t analog_points = 0;
    double success_rate = 0.0;
    double failure_rate = 0.0;
  };

  inline MessageStats GetStats() const {
    MessageStats stats;
    stats.total_points = points.size();

    for (const auto &point : points) {
      if (point.value_changed || point.force_rdb_store) {
        stats.changed_points++;
      }
      if (point.ShouldStoreToRDB()) {
        stats.rdb_store_points++;
      }
      if (point.ShouldCheckAlarms()) {
        stats.alarm_check_points++;
      }
      if (point.IsDigital()) {
        stats.digital_points++;
      } else {
        stats.analog_points++;
      }
    }

    if (total_points_configured > 0) {
      stats.success_rate =
          static_cast<double>(successful_points) / total_points_configured;
      stats.failure_rate =
          static_cast<double>(failed_points) / total_points_configured;
    }

    return stats;
  }

  // ==========================================================================
  // 🔥 JSON 직렬화 함수들
  // ==========================================================================

  /**
   * @brief Redis용 디바이스 상태 JSON (경량)
   */
  inline std::string ToDeviceStatusJSON() const {
    nlohmann::json j;
    j["device_id"] = device_id;
    j["protocol"] = protocol;
    j["status"] = static_cast<int>(device_status);
    j["status_str"] = Enums::DeviceStatusToString(device_status);
    j["status_message"] = status_message;
    j["is_connected"] = is_connected;
    j["manual_status"] = manual_status;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();

    if (status_changed) {
      j["status_changed"] = true;
      j["status_changed_time"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              status_changed_time.time_since_epoch())
              .count();
    }

    // 통신 통계
    j["comm_stats"] = {{"consecutive_failures", consecutive_failures},
                       {"total_failures", total_failures},
                       {"total_attempts", total_attempts},
                       {"response_time_ms", response_time.count()}};

    // 포인트 통계
    if (total_points_configured > 0) {
      j["point_stats"] = {
          {"total", total_points_configured},
          {"success", successful_points},
          {"failed", failed_points},
          {"success_rate",
           static_cast<double>(successful_points) / total_points_configured}};
    }

    return j.dump();
  }

  /**
   * @brief 완전한 메시지 JSON (RDB/로그용)
   */
  inline std::string ToFullJSON() const {
    nlohmann::json j;

    // 기본 정보
    j["type"] = type;
    j["device_id"] = device_id;
    j["protocol"] = protocol;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["tenant_id"] = tenant_id;
    j["site_id"] = site_id;
    j["correlation_id"] = correlation_id;
    j["source_worker"] = source_worker;
    j["batch_sequence"] = batch_sequence;

    // 처리 제어
    j["processing"] = {{"trigger_alarms", trigger_alarms},
                       {"trigger_virtual_points", trigger_virtual_points},
                       {"high_priority", high_priority},
                       {"priority", priority}};

    // 디바이스 상태
    j["device_status"] = {
        {"current", static_cast<int>(device_status)},
        {"current_str", Enums::DeviceStatusToString(device_status)},
        {"previous", static_cast<int>(previous_status)},
        {"changed", status_changed},
        {"manual", manual_status},
        {"message", status_message},
        {"changed_by", status_changed_by}};

    if (status_changed) {
      j["device_status"]["changed_time"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              status_changed_time.time_since_epoch())
              .count();
    }

    // 통신 상태
    j["communication"] = {{"connected", is_connected},
                          {"consecutive_failures", consecutive_failures},
                          {"total_failures", total_failures},
                          {"total_attempts", total_attempts},
                          {"response_time_ms", response_time.count()},
                          {"last_error_message", last_error_message},
                          {"last_error_code", last_error_code}};

    if (last_success_time.time_since_epoch().count() > 0) {
      j["communication"]["last_success_time"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              last_success_time.time_since_epoch())
              .count();
    }

    // 포인트 정보
    j["points_summary"] = {{"total_configured", total_points_configured},
                           {"successful", successful_points},
                           {"failed", failed_points},
                           {"in_message", points.size()}};

    // 포인트 데이터 (요약만)
    j["points"] = nlohmann::json::array();
    for (const auto &point : points) {
      j["points"].push_back(nlohmann::json::parse(point.ToRedisJSON()));
    }

    return j.dump();
  }

  /**
   * @brief 전송용 경량 JSON (파이프라인용)
   */
  inline std::string ToTransportJSON() const {
    nlohmann::json j;

    // 필수 필드만
    j["type"] = type;
    j["device_id"] = device_id;
    j["protocol"] = protocol;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["correlation_id"] = correlation_id;
    j["source_worker"] = source_worker;

    // 상태 정보 (핵심만)
    j["device_status"] = static_cast<int>(device_status);
    j["is_connected"] = is_connected;

    // 처리 제어
    j["trigger_alarms"] = trigger_alarms;
    j["trigger_virtual_points"] = trigger_virtual_points;

    // 포인트 데이터 (경량)
    j["points"] = nlohmann::json::array();
    for (const auto &point : points) {
      j["points"].push_back(nlohmann::json::parse(point.ToRedisJSON()));
    }

    return j.dump();
  }

  // ==========================================================================
  // 🔥 유틸리티 함수들
  // ==========================================================================

  /**
   * @brief 상태 변경 초기화 (처리 완료 후 호출)
   */
  inline void ResetStatusChange() { status_changed = false; }

  /**
   * @brief 통신 세션 초기화 (Worker 재시작시)
   */
  inline void ResetCommunicationStats() {
    consecutive_failures = 0;
    total_failures = 0;
    total_attempts = 0;
    is_connected = false;
    last_error_message = "";
    last_error_code = 0;
  }

  /**
   * @brief correlation_id 자동 생성
   */
  inline std::string GenerateCorrelationId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    return device_id + "_" + source_worker + "_" + std::to_string(timestamp);
  }

private:
  // (Enums.h에 정의된 함수들 사용하므로 여기서는 제거)
};

/**
 * @brief 로그 통계 구조체
 */
struct LogStatistics {
  std::atomic<uint64_t> trace_count{0};
  std::atomic<uint64_t> debug_count{0};
  std::atomic<uint64_t> info_count{0};
  std::atomic<uint64_t> warn_count{0};
  std::atomic<uint64_t> warning_count{0}; // warn_count 별칭
  std::atomic<uint64_t> error_count{0};
  std::atomic<uint64_t> fatal_count{0};
  std::atomic<uint64_t> maintenance_count{0};
  std::atomic<uint64_t> total_logs{0}; // 🔥 추가: resetStatistics에서 사용

  Timestamp start_time;
  Timestamp last_log_time;
  Timestamp last_reset_time; // 🔥 추가: resetStatistics에서 사용

  LogStatistics() {
    start_time = std::chrono::system_clock::now();
    last_log_time = start_time;
    last_reset_time = start_time; // 🔥 초기화 추가
  }

  // 🔥 복사 생성자 명시적 구현 (atomic 때문에 필요)
  LogStatistics(const LogStatistics &other)
      : trace_count(other.trace_count.load()),
        debug_count(other.debug_count.load()),
        info_count(other.info_count.load()),
        warn_count(other.warn_count.load()),
        warning_count(other.warning_count.load()),
        error_count(other.error_count.load()),
        fatal_count(other.fatal_count.load()),
        maintenance_count(other.maintenance_count.load()),
        total_logs(other.total_logs.load()) // 🔥 추가
        ,
        start_time(other.start_time), last_log_time(other.last_log_time),
        last_reset_time(other.last_reset_time) { // 🔥 추가
  }

  // 🔥 할당 연산자 명시적 구현
  LogStatistics &operator=(const LogStatistics &other) {
    if (this != &other) {
      trace_count.store(other.trace_count.load());
      debug_count.store(other.debug_count.load());
      info_count.store(other.info_count.load());
      warn_count.store(other.warn_count.load());
      warning_count.store(other.warning_count.load());
      error_count.store(other.error_count.load());
      fatal_count.store(other.fatal_count.load());
      maintenance_count.store(other.maintenance_count.load());
      total_logs.store(other.total_logs.load()); // 🔥 추가
      start_time = other.start_time;
      last_log_time = other.last_log_time;
      last_reset_time = other.last_reset_time; // 🔥 추가
    }
    return *this;
  }

  // 🔥 총 로그 수 계산 메소드 (기존 멤버와 중복되지 않도록)
  uint64_t CalculateTotalLogs() const {
    return trace_count.load() + debug_count.load() + info_count.load() +
           warn_count.load() + error_count.load() + fatal_count.load() +
           maintenance_count.load();
  }

  // 🔥 GetTotalLogs 메소드 (기존 에러 메시지에서 제안된 이름)
  uint64_t GetTotalLogs() const { return total_logs.load(); }

  // 🔥 total_logs 업데이트 (로그 추가 시 호출)
  void IncrementTotalLogs() { total_logs.fetch_add(1); }

  // 🔥 모든 카운터 리셋
  void ResetAllCounters() {
    trace_count.store(0);
    debug_count.store(0);
    info_count.store(0);
    warn_count.store(0);
    warning_count.store(0);
    error_count.store(0);
    fatal_count.store(0);
    maintenance_count.store(0);
    total_logs.store(0);
    last_reset_time = std::chrono::system_clock::now();
  }

  // 🔥 별칭 동기화
  void SyncWarningCount() { warning_count.store(warn_count.load()); }
};
/**
 * @brief 파이프라인 통계 구조체
 * @details WorkerPipelineManager 성능 모니터링용
 */
struct PipelineStatistics {
  std::atomic<uint64_t> total_processed{0};        // 총 처리된 데이터 포인트 수
  std::atomic<uint64_t> total_dropped{0};          // 큐 오버플로우로 버려진 수
  std::atomic<uint64_t> redis_writes{0};           // Redis 쓰기 횟수
  std::atomic<uint64_t> influx_writes{0};          // InfluxDB 쓰기 횟수
  std::atomic<uint64_t> rabbitmq_publishes{0};     // RabbitMQ 발행 횟수
  std::atomic<uint64_t> alarm_events{0};           // 알람 이벤트 수
  std::atomic<uint64_t> high_priority_events{0};   // 높은 우선순위 이벤트 수
  std::atomic<size_t> current_queue_size{0};       // 현재 큐 크기
  std::atomic<double> avg_processing_time_ms{0.0}; // 평균 처리 시간
  std::chrono::system_clock::time_point start_time; // 시작 시간

  PipelineStatistics() : start_time(std::chrono::system_clock::now()) {}

  /**
   * @brief 성공률 계산
   */
  double GetSuccessRate() const {
    uint64_t total = total_processed.load();
    uint64_t dropped = total_dropped.load();
    if (total == 0)
      return 100.0;
    return ((double)(total - dropped) / total) * 100.0;
  }

  /**
   * @brief 초당 처리량 계산
   */
  double GetThroughputPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    if (duration.count() == 0)
      return 0.0;
    return static_cast<double>(total_processed.load()) / duration.count();
  }

  /**
   * @brief 통계 초기화
   */
  void Reset() {
    total_processed = 0;
    total_dropped = 0;
    redis_writes = 0;
    influx_writes = 0;
    rabbitmq_publishes = 0;
    alarm_events = 0;
    high_priority_events = 0;
    current_queue_size = 0;
    avg_processing_time_ms = 0.0;
    start_time = std::chrono::system_clock::now();
  }

  /**
   * @brief JSON 직렬화
   */
  std::string ToJSON() const {
    JsonType j;
    j["total_processed"] = total_processed.load();
    j["total_dropped"] = total_dropped.load();
    j["redis_writes"] = redis_writes.load();
    j["influx_writes"] = influx_writes.load();
    j["rabbitmq_publishes"] = rabbitmq_publishes.load();
    j["alarm_events"] = alarm_events.load();
    j["high_priority_events"] = high_priority_events.load();
    j["current_queue_size"] = current_queue_size.load();
    j["avg_processing_time_ms"] = avg_processing_time_ms.load();
    j["success_rate"] = GetSuccessRate();
    j["throughput_per_second"] = GetThroughputPerSecond();

    // 시작 시간
    auto time_t = std::chrono::system_clock::to_time_t(start_time);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    j["start_time"] = std::string(buffer);

    return j.dump();
  }
};
/**
 * @brief 드라이버 로그 컨텍스트
 */
struct DriverLogContext {
  UniqueId device_id;
  std::string device_name;
  std::string protocol;
  std::string endpoint;
  std::string thread_id;
  std::string operation;

  DriverLogContext() = default;

  DriverLogContext(const UniqueId &dev_id, const std::string &dev_name,
                   const std::string &proto, const std::string &ep)
      : device_id(dev_id), device_name(dev_name), protocol(proto),
        endpoint(ep) {}
};

} // namespace Structs

// =========================================================================
// 🔥 전역 네임스페이스 호환성 (최상위 PulseOne에서 직접 사용 가능)
// =========================================================================
using DeviceInfo = Structs::DeviceInfo;
using DataPoint = Structs::DataPoint;
using TimestampedValue = Structs::TimestampedValue;

// Drivers 네임스페이스 호환성 유지
namespace Drivers {
using DeviceInfo = PulseOne::DeviceInfo;
using DataPoint = PulseOne::DataPoint;
using TimestampedValue = PulseOne::TimestampedValue;
} // namespace Drivers

} // namespace PulseOne

#endif // PULSEONE_COMMON_STRUCTS_H