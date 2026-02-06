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
  int bd = 0;             ///< Building ID
  std::string nm;         ///< Point Name
  std::string vl;         ///< Value (String format for flexibility)
  std::string tm;         ///< Timestamp (yyyy-MM-dd HH:mm:ss.fff)
  int st = 0;             ///< Status (Communication Status)
  std::string ty = "dbl"; ///< Type (dbl or str)

  json to_json() const {
    return json{{"bd", bd}, {"nm", nm}, {"vl", vl},
                {"tm", tm}, {"st", st}, {"ty", ty}};
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
