/**
 * @file AlarmMessage.h
 * @brief Bridge header for backward compatibility
 */

#ifndef CSP_ALARM_MESSAGE_H
#define CSP_ALARM_MESSAGE_H

#include "Gateway/Model/AlarmMessage.h"
#include "Gateway/Model/ValueMessage.h"

namespace PulseOne {
namespace CSP {
using AlarmMessage = PulseOne::Gateway::Model::AlarmMessage;
using ValueMessage = PulseOne::Gateway::Model::ValueMessage;
} // namespace CSP
} // namespace PulseOne

#endif // CSP_ALARM_MESSAGE_H
