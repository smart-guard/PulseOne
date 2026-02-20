/**
 * @file ValueMessage.h
 * @brief Gateway ValueMessage - PulseOne::Gateway::Model namespace
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_MODEL_VALUE_MESSAGE_H
#define GATEWAY_MODEL_VALUE_MESSAGE_H

#include <string>

namespace PulseOne {
namespace Gateway {
namespace Model {

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

  nlohmann::json extra_info; // default object; set in constructor

  ValueMessage(); // defined in ValueMessage.cpp (initializes extra_info)

  // Implementation in ValueMessage.cpp
  nlohmann::json to_json() const;
};

} // namespace Model
} // namespace Gateway

// Backward compatibility alias
namespace CSP {
using ValueMessage = PulseOne::Gateway::Model::ValueMessage;
}

} // namespace PulseOne

#endif // GATEWAY_MODEL_VALUE_MESSAGE_H
