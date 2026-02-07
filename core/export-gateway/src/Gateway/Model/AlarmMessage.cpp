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
  // [v3.2.0] Agnostic Data Carrier
  // 이 JSON은 원본 데이터를 보존하며, 템플릿 엔진({{{...}}})이 참조할 수 있는
  // 표준 변수들을 제공함.
  json j = extra_info.is_object() ? extra_info : json::object();

  // 표준 변수군 (Agnostic Variables)
  // 임의의 템플릿에서 공통적으로 사용할 수 있는 데이터 브릿지 역할
  j["site_id"] = bd;
  j["point_id"] = point_id;
  j["rule_id"] = rule_id;
  j["measured_value"] = vl;
  j["point_name"] = nm;
  j["original_name"] = original_nm;
  j["timestamp"] = tm;
  j["comm_status"] = st;
  j["alarm_level"] = al;
  j["description"] = des;
  j["data_type"] = ty;
  j["is_control"] = (ty == "bit" || ty == "bool") ? 1 : 0;

  return j;
}

bool AlarmMessage::from_json(const json &j) {
  try {
    // 1. Building ID / Site ID
    if (j.contains("bd"))
      bd = j["bd"].is_number() ? j["bd"].get<int>()
                               : std::stoi(j["bd"].get<std::string>());
    else if (j.contains("site_id"))
      bd = j["site_id"].is_number()
               ? j["site_id"].get<int>()
               : std::stoi(j["site_id"].get<std::string>());

    // 2. Name (nm / source_name)
    if (j.contains("nm"))
      nm = j["nm"].get<std::string>();
    else if (j.contains("source_name"))
      nm = j["source_name"].get<std::string>();

    original_nm = nm; // 컬렉터에서 받은 원본 이름을 보존

    // 3. Value (vl / trigger_value)
    if (j.contains("vl")) {
      if (j["vl"].is_number())
        vl = j["vl"].get<double>();
    } else if (j.contains("trigger_value")) {
      if (j["trigger_value"].is_number())
        vl = j["trigger_value"].get<double>();
      else if (j["trigger_value"].is_string())
        vl = std::stod(j["trigger_value"].get<std::string>());
    }

    // 4. Timestamp (tm / timestamp)
    if (j.contains("tm")) {
      tm = j["tm"].get<std::string>();
    } else if (j.contains("timestamp")) {
      if (j["timestamp"].is_number()) {
        int64_t ms = j["timestamp"].get<int64_t>();
        std::chrono::system_clock::time_point tp{std::chrono::milliseconds(ms)};
        tm = time_to_csharp_format(tp, true);
      }
    }

    // 5. Alarm Status (al / state)
    if (j.contains("al")) {
      if (j["al"].is_number())
        al = j["al"].get<int>();
      else {
        std::string s = j["al"].get<std::string>();
        al = (s == "ALARM" || s == "1" || s == "active" || s == "ACTIVE") ? 1
                                                                          : 0;
      }
    } else if (j.contains("state")) {
      std::string s = j["state"].get<std::string>();
      al = (s == "active" || s == "ACTIVE") ? 1 : 0;
    }

    // 6. Comm Status
    if (j.contains("st") && j["st"].is_number())
      st = j["st"].get<int>();

    // 7. Description (des / message)
    if (j.contains("des"))
      des = j["des"].get<std::string>();
    else if (j.contains("message"))
      des = j["message"].get<std::string>();

    // [v3.2.0] Save RAW input JSON for dynamic mapping (PayloadTransformer)
    extra_info = j;

    // Internal mapping (site_id, point_id)
    if (j.contains("site_id")) {
      if (j["site_id"].is_number())
        site_id = j["site_id"].get<int>();
      else if (j["site_id"].is_string())
        site_id = std::stoi(j["site_id"].get<std::string>());
    }
    if (j.contains("point_id")) {
      if (j["point_id"].is_number())
        point_id = j["point_id"].get<int>();
      else if (j["point_id"].is_string())
        point_id = std::stoi(j["point_id"].get<std::string>());
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
  msg.bd = building_id > 0 ? building_id : occurrence.getTenantId();

  if (occurrence.getPointId().has_value()) {
    msg.point_id = occurrence.getPointId().value();
  }

  if (!occurrence.getSourceName().empty()) {
    msg.nm = occurrence.getSourceName();
  } else {
    msg.nm = "Alarm_" + std::to_string(occurrence.getRuleId());
  }

  if (!occurrence.getTriggerValue().empty()) {
    try {
      msg.vl = std::stod(occurrence.getTriggerValue());
    } catch (...) {
      msg.vl = 0.0;
    }
  }

  msg.tm = time_to_csharp_format(occurrence.getOccurrenceTime(), true);

  auto state = occurrence.getState();
  msg.al = (state == PulseOne::Alarm::AlarmState::ACTIVE)    ? 1
           : (state == PulseOne::Alarm::AlarmState::CLEARED) ? 0
                                                             : 1;

  msg.st = occurrence.getAcknowledgedTime().has_value() ? 0 : 1;

  msg.des = occurrence.getAlarmMessage().empty() ? "Alarm Message"
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
  // Relaxed validity: if raw data exists, we consider it valid for
  // transformation
  if (!extra_info.empty())
    return true;
  // Fallback to legacy check
  return bd > 0 && !nm.empty();
}

std::string AlarmMessage::get_alarm_status_string() const {
  return (al == 1) ? "발생" : "해제";
}

std::string AlarmMessage::to_string() const {
  std::ostringstream oss;
  oss << "AlarmMessage{bd=" << bd << ", nm='" << nm << "', al=" << al << "}";
  return oss.str();
}

bool AlarmMessage::operator==(const AlarmMessage &other) const {
  return bd == other.bd && nm == other.nm && tm == other.tm && al == other.al;
}

} // namespace Model
} // namespace Gateway
} // namespace PulseOne
