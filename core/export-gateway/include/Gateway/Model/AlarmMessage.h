/**
 * @file AlarmMessage.h
 * @brief Gateway AlarmMessage - PulseOne::Gateway::Model namespace
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_MODEL_ALARM_MESSAGE_H
#define GATEWAY_MODEL_ALARM_MESSAGE_H

#include <chrono>
#include <set>
#include <string>
#include <vector>

#ifdef HAS_SHARED_LIBS
#include "Database/Entities/AlarmOccurrenceEntity.h"
#endif

namespace PulseOne {
namespace Gateway {
namespace Model {

/**
 * @brief Gateway AlarmMessage structure (PulseOne::Gateway::Model)
 */
struct AlarmMessage {
  // [v3.2.0] Agnostic Descriptive Fields
  int site_id = 0;
  std::string data_type = "num";
  std::string point_name;
  std::string original_name;
  double measured_value = 0.0;
  std::string timestamp;
  int status_code = 1;
  int alarm_level = 0;
  std::string description;

  // Internal Logic Fields
  int point_id = 0;
  int rule_id = 0;
  bool manual_override = false;
  nlohmann::json
      extra_info; // default-initialized to null; set to object() in constructor

  AlarmMessage(); // defined in AlarmMessage.cpp (initializes extra_info)

  /**
   * @brief Convert to JSON
   */
  nlohmann::json to_json() const;

  /**
   * @brief Load from JSON
   */
  bool from_json(const nlohmann::json &j);

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

// nlohmann::json support (implementations in AlarmMessage.cpp)
void to_json(nlohmann::json &j, const AlarmMessage &msg);
void from_json(const nlohmann::json &j, AlarmMessage &msg);

} // namespace Model
} // namespace Gateway

// Backward compatibility alias
namespace CSP {
using AlarmMessage = PulseOne::Gateway::Model::AlarmMessage;
}

} // namespace PulseOne

#endif // GATEWAY_MODEL_ALARM_MESSAGE_H
