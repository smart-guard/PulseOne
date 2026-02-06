/**
 * @file MqttTargetHandler.h
 * @brief Bridge header for backward compatibility
 */

#ifndef CSP_MQTT_TARGET_HANDLER_H
#define CSP_MQTT_TARGET_HANDLER_H

#include "Gateway/Target/MqttTargetHandler.h"

namespace PulseOne {
namespace CSP {
using MqttTargetHandler = PulseOne::Gateway::Target::MqttTargetHandler;
}
} // namespace PulseOne

#endif // CSP_MQTT_TARGET_HANDLER_H
