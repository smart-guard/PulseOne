/**
 * @file AlarmMessage.h
 * @brief Gateway AlarmMessage - PulseOne::Gateway::Model namespace
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_MODEL_ALARM_MESSAGE_H
#define GATEWAY_MODEL_ALARM_MESSAGE_H

#include <chrono>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#ifdef HAS_SHARED_LIBS
#include "Database/Entities/AlarmOccurrenceEntity.h"
#endif

namespace PulseOne {
namespace Gateway {
namespace Model {

using json = nlohmann::json;

/**
 * @brief Gateway AlarmMessage structure (PulseOne::Gateway::Model)
 */
struct AlarmMessage {
  // Fields mapped from legacy C#
  int bd = 0;                  ///< Building ID
  std::string ty = "num";      ///< Type (num, bit, etc.)
  std::string nm;              ///< Point Name (Mapped)
  std::string original_nm;     ///< Original Collector Point Name
  double vl = 0.0;             ///< Value
  std::string il = "";         ///< Info Limit
  std::string xl = "";         ///< Extra Limit
  std::vector<double> mi = {}; ///< Min array
  std::vector<double> mx = {}; ///< Max array
  std::string tm;              ///< Timestamp (yyyy-MM-dd HH:mm:ss.fff)
  int st = 1;                  ///< Comm Status (1: Normal)
  int al = 0;                  ///< Alarm Status (1: Occur, 0: Clear)
  std::string des;             ///< Description

  // Internal Logic Fields
  int point_id = 0;
  int site_id = 0;
  int rule_id = 0;
  bool manual_override = false;
  json extra_info = json::object();

  /**
   * @brief Convert to JSON
   */
  json to_json() const;

  /**
   * @brief Load from JSON
   */
  bool from_json(const json &j);

#ifdef HAS_SHARED_LIBS
  /**
   * @brief Convert from AlarmOccurrenceEntity
   */
  static AlarmMessage from_alarm_occurrence(
      const Database::Entities::AlarmOccurrenceEntity &occurrence,
      int building_id = -1);
#endif

  static std::string current_time_to_csharp_format(bool use_local_time = true);
  static std::string
  time_to_csharp_format(const std::chrono::system_clock::time_point &time_point,
                        bool use_local_time = true);

  bool is_valid() const;
  std::string get_alarm_status_string() const;
  std::string to_string() const;

  bool operator==(const AlarmMessage &other) const;
  bool operator!=(const AlarmMessage &other) const { return !(*this == other); }
};

// nlohmann::json support
inline void to_json(json &j, const AlarmMessage &msg) { j = msg.to_json(); }
inline void from_json(const json &j, AlarmMessage &msg) { msg.from_json(j); }

} // namespace Model
} // namespace Gateway

// Backward compatibility alias
namespace CSP {
using AlarmMessage = PulseOne::Gateway::Model::AlarmMessage;
}

} // namespace PulseOne

#endif // GATEWAY_MODEL_ALARM_MESSAGE_H
