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
  return json{{"bd", bd}, {"ty", ty}, {"nm", nm}, {"vl", vl},
              {"il", il}, {"xl", xl}, {"mi", mi}, {"mx", mx},
              {"tm", tm}, {"st", st}, {"al", al}, {"des", des}};
}

bool AlarmMessage::from_json(const json &j) {
  try {
    if (j.contains("bd"))
      bd = j["bd"].is_number() ? j["bd"].get<int>()
                               : std::stoi(j["bd"].get<std::string>());
    else if (j.contains("site_id"))
      bd = j["site_id"].is_number()
               ? j["site_id"].get<int>()
               : std::stoi(j["site_id"].get<std::string>());

    if (j.contains("nm"))
      nm = j["nm"].get<std::string>();

    if (j.contains("vl")) {
      if (j["vl"].is_number())
        vl = j["vl"].get<double>();
    } else if (j.contains("trigger_value")) {
      if (j["trigger_value"].is_number())
        vl = j["trigger_value"].get<double>();
      else if (j["trigger_value"].is_string())
        vl = std::stod(j["trigger_value"].get<std::string>());
    }

    if (j.contains("tm"))
      tm = j["tm"].get<std::string>();

    if (j.contains("al")) {
      if (j["al"].is_number())
        al = j["al"].get<int>();
      else {
        std::string s = j["al"].get<std::string>();
        al = (s == "ALARM" || s == "1" || s == "active" || s == "ACTIVE") ? 1
                                                                          : 0;
      }
    }

    if (j.contains("st") && j["st"].is_number())
      st = j["st"].get<int>();
    if (j.contains("des"))
      des = j["des"].get<std::string>();

    // Internal mapping
    if (j.contains("site_id"))
      site_id = j["site_id"].get<int>();
    if (j.contains("point_id"))
      point_id = j["point_id"].get<int>();

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
  return oss.str();
}

bool AlarmMessage::is_valid() const {
  return bd > 0 && !nm.empty() && !tm.empty();
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
