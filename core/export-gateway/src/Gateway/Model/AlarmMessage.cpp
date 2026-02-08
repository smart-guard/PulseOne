/**
 * @file AlarmMessage.cpp
 * @brief Gateway AlarmMessage implementation - PulseOne::Gateway::Model
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#include "Gateway/Model/AlarmMessage.h"
#include <iomanip>
#include <sstream>

#ifdef HAS_SHARED_LIBS
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Logging/LogManager.h"
#endif

namespace PulseOne {
namespace Gateway {
namespace Model {

json AlarmMessage::to_json() const {
  // [v3.2.1] Manual Override RAW Bypass: 사용자 입력 원본 그대로 반환
  if (manual_override && !extra_info.is_null()) {
    return extra_info;
  }

  // [v3.2.0] Descriptive Agnostic Standard (Template-Independent)
  json j = json::object();

  j["site_id"] = site_id;
  j["point_name"] = point_name;
  j["measured_value"] = measured_value;
  j["alarm_level"] = alarm_level;
  j["status_code"] = status_code;
  j["timestamp"] = timestamp;
  j["data_type"] = data_type;
  j["description"] = description;
  j["point_id"] = point_id;

  // Metadata Harvesting (원본 페이로드의 모든 필드 포함)
  if (!extra_info.is_null() && extra_info.is_object()) {
    for (auto it = extra_info.begin(); it != extra_info.end(); ++it) {
      if (!j.contains(it.key())) {
        j[it.key()] = it.value();
      }
    }
  }

  j["is_control"] =
      (data_type == "bit" || data_type == "bool" || data_type == "DIGITAL") ? 1
                                                                            : 0;

  return j;
}

bool AlarmMessage::from_json(const json &j) {
  try {
    // [v3.0.0] Agnostic Parsing Logic (Priority: Collector-Native Descriptive
    // Keys)

    // 1. Site ID
    if (j.contains("site_id")) {
      site_id = j["site_id"].is_number()
                    ? j["site_id"].get<int>()
                    : std::stoi(j["site_id"].get<std::string>());
    } else if (j.contains("tenant_id")) {
      site_id = j["tenant_id"].is_number()
                    ? j["tenant_id"].get<int>()
                    : std::stoi(j["tenant_id"].get<std::string>());
    } else if (j.contains("bd")) {
      site_id = j["bd"].is_number() ? j["bd"].get<int>()
                                    : std::stoi(j["bd"].get<std::string>());
    }

    // 2. Point Name (Priority: source_name -> point_name -> nm)
    if (j.contains("source_name"))
      point_name = j["source_name"].get<std::string>();
    else if (j.contains("point_name"))
      point_name = j["point_name"].get<std::string>();
    else if (j.contains("nm"))
      point_name = j["nm"].get<std::string>();

    original_name = point_name;

    // 3. Measured Value (Priority: trigger_value -> measured_value -> vl)
    if (j.contains("trigger_value")) {
      if (j["trigger_value"].is_number())
        measured_value = j["trigger_value"].get<double>();
      else if (j["trigger_value"].is_string())
        measured_value = std::stod(j["trigger_value"].get<std::string>());
    } else if (j.contains("measured_value")) {
      if (j["measured_value"].is_number())
        measured_value = j["measured_value"].get<double>();
    } else if (j.contains("vl")) {
      if (j["vl"].is_number())
        measured_value = j["vl"].get<double>();
    } else if (j.contains("value")) {
      if (j["value"].is_number())
        measured_value = j["value"].get<double>();
      else if (j["value"].is_string())
        measured_value = std::stod(j["value"].get<std::string>());
    }

    // 4. Timestamp
    if (j.contains("timestamp")) {
      if (j["timestamp"].is_string()) {
        timestamp = j["timestamp"].get<std::string>();
      } else if (j["timestamp"].is_number()) {
        int64_t ms = j["timestamp"].get<int64_t>();
        std::chrono::system_clock::time_point tp{std::chrono::milliseconds(ms)};
        timestamp = time_to_csharp_format(tp, true);
      }
    } else if (j.contains("tm")) {
      timestamp = j["tm"].get<std::string>();
    }

    // 5. Alarm Level / Severity (Priority: severity -> state -> alarm_level ->
    // al)
    if (j.contains("severity")) {
      std::string sev = j["severity"].get<std::string>();
      if (sev == "CRITICAL")
        alarm_level = 2;
      else if (sev == "WARNING")
        alarm_level = 1;
      else if (sev == "INFO")
        alarm_level = 0;
      else
        alarm_level = 1;
    } else if (j.contains("state")) {
      std::string st_str = j["state"].get<std::string>();
      alarm_level = (st_str == "ACTIVE" || st_str == "ALARM") ? 1 : 0;
    } else if (j.contains("alarm_level")) {
      alarm_level =
          j["alarm_level"].is_number() ? j["alarm_level"].get<int>() : 0;
    } else if (j.contains("al")) {
      alarm_level = j["al"].is_number() ? j["al"].get<int>() : 0;
    }

    // 6. Status Code
    if (j.contains("status_code")) {
      status_code =
          j["status_code"].is_number() ? j["status_code"].get<int>() : 1;
    } else if (j.contains("st")) {
      status_code = j["st"].is_number() ? j["st"].get<int>() : 1;
    }

    // 7. Description
    if (j.contains("message"))
      description = j["message"].get<std::string>();
    else if (j.contains("description"))
      description = j["description"].get<std::string>();
    else if (j.contains("des"))
      description = j["des"].get<std::string>();

    // 8. Data Type
    if (j.contains("data_type"))
      data_type = j["data_type"].get<std::string>();
    else if (j.contains("ty"))
      data_type = j["ty"].get<std::string>();

    // Save RAW context for transformer engine
    extra_info = j;

    if (j.contains("point_id")) {
      if (j["point_id"].is_number())
        point_id = j["point_id"].get<int>();
      else if (j["point_id"].is_string())
        point_id = std::stoi(j["point_id"].get<std::string>());
    }

    if (j.contains("rule_id")) {
      if (j["rule_id"].is_number())
        rule_id = j["rule_id"].get<int>();
    }

    return true;
  } catch (...) {
    return false;
  }
}

#ifdef HAS_SHARED_LIBS
AlarmMessage AlarmMessage::from_alarm_occurrence(
    const Database::Entities::AlarmOccurrenceEntity &occurrence,
    int building_id) {

  AlarmMessage msg;
  msg.site_id = building_id > 0 ? building_id : occurrence.getTenantId();

  if (occurrence.getPointId().has_value()) {
    msg.point_id = occurrence.getPointId().value();
  }

  if (!occurrence.getSourceName().empty()) {
    msg.point_name = occurrence.getSourceName();
  } else {
    msg.point_name = "Alarm_" + std::to_string(occurrence.getRuleId());
  }

  if (!occurrence.getTriggerValue().empty()) {
    try {
      msg.measured_value = std::stod(occurrence.getTriggerValue());
    } catch (...) {
      msg.measured_value = 0.0;
    }
  }

  msg.timestamp = time_to_csharp_format(occurrence.getOccurrenceTime(), true);

  auto state = occurrence.getState();
  msg.alarm_level = (state == PulseOne::Alarm::AlarmState::ACTIVE)    ? 1
                    : (state == PulseOne::Alarm::AlarmState::CLEARED) ? 0
                                                                      : 1;

  msg.status_code = occurrence.getAcknowledgedTime().has_value() ? 0 : 1;

  msg.description = occurrence.getAlarmMessage().empty()
                        ? "Alarm Message"
                        : occurrence.getAlarmMessage();

  return msg;
}
#endif

std::string AlarmMessage::current_time_to_csharp_format(bool use_local_time) {
  auto now = std::chrono::system_clock::now();
  return time_to_csharp_format(now, use_local_time);
}

std::string AlarmMessage::time_to_csharp_format(
    const std::chrono::system_clock::time_point &time_point,
    bool use_local_time) {
  auto time_t = std::chrono::system_clock::to_time_t(time_point);
  std::tm tm_buf;
  if (use_local_time)
    localtime_r(&time_t, &tm_buf);
  else
    gmtime_r(&time_t, &tm_buf);

  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

  // 밀리초 추가 (v3.2.0)
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                time_point.time_since_epoch()) %
            1000;
  oss << "." << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}

bool AlarmMessage::is_valid() const {
  if (!extra_info.empty())
    return true;
  return site_id > 0 && !point_name.empty();
}

std::string AlarmMessage::get_alarm_status_string() const {
  return (alarm_level == 1) ? "발생" : "해제";
}

std::string AlarmMessage::to_string() const {
  std::ostringstream oss;
  oss << "AlarmMessage{site_id=" << site_id << ", point_name='" << point_name
      << "', alarm_level=" << alarm_level << "}";
  return oss.str();
}

bool AlarmMessage::operator==(const AlarmMessage &other) const {
  return site_id == other.site_id && point_name == other.point_name &&
         timestamp == other.timestamp && alarm_level == other.alarm_level;
}

} // namespace Model
} // namespace Gateway
} // namespace PulseOne
