/**
 * @file ValueMessage.h
 * @brief Gateway ValueMessage - PulseOne::Gateway::Model namespace
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_MODEL_VALUE_MESSAGE_H
#define GATEWAY_MODEL_VALUE_MESSAGE_H

#include <nlohmann/json.hpp>
#include <string>

namespace PulseOne {
namespace Gateway {
namespace Model {

using json = nlohmann::json;

/**
 * @brief Value Message Structure (v3.5 compatibility)
 */
struct ValueMessage {
  int site_id = 0;
  int point_id = 0;
  std::string point_name;
  std::string measured_value;
  std::string timestamp;
  int status_code = 0;
  std::string data_type = "dbl";

  json extra_info;

  json to_json() const {
    // [v3.2.1] Engine Standard Format (Tokens for Templates)
    json j = json{
        {"bd", site_id},
        {"nm", point_name},
        {"vl", measured_value},
        {"tm", timestamp},
        {"st", status_code},
        {"ty", data_type},
        {"point_id", point_id},
        {"is_control", (data_type == "bit" || data_type == "bool") ? 1 : 0}};

    // Metadata Harvesting (mi, mx, il, xl 등)
    if (!extra_info.is_null() && extra_info.is_object()) {
      for (auto it = extra_info.begin(); it != extra_info.end(); ++it) {
        if (!j.contains(it.key())) {
          j[it.key()] = it.value();
        }
      }
    }
    return j;
  }
};

} // namespace Model
} // namespace Gateway

// Backward compatibility alias
namespace CSP {
using ValueMessage = PulseOne::Gateway::Model::ValueMessage;
}

} // namespace PulseOne

#endif // GATEWAY_MODEL_VALUE_MESSAGE_H
