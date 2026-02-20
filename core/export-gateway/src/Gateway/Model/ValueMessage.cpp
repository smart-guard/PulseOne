/**
 * @file ValueMessage.cpp
 * @brief Gateway ValueMessage implementation - PulseOne::Gateway::Model
 * @author PulseOne Development Team
 * @date 2026-02-20
 *
 * NOTE: ValueMessage.h intentionally uses <nlohmann/json_fwd.hpp> to avoid
 * pulling in the full json.hpp into every translation unit. The actual
 * implementation (to_json, constructor) lives here.
 */

#include "Gateway/Model/ValueMessage.h"
#include <nlohmann/json.hpp> // full json here; header only has json_fwd.hpp

namespace PulseOne {
namespace Gateway {
namespace Model {

// Default-initialize extra_info to empty object (was implicit in header)
ValueMessage::ValueMessage() : extra_info(nlohmann::json::object()) {}

nlohmann::json ValueMessage::to_json() const {
  using json = nlohmann::json;

  // [v3.2.1] Engine Standard Format (Tokens for Templates)
  json j =
      json{{"bd", site_id},
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

} // namespace Model
} // namespace Gateway
} // namespace PulseOne
