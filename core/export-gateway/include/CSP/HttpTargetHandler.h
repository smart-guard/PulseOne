/**
 * @file HttpTargetHandler.h
 * @brief Bridge header for backward compatibility
 */

#ifndef CSP_HTTP_TARGET_HANDLER_H
#define CSP_HTTP_TARGET_HANDLER_H

#include "Gateway/Target/HttpTargetHandler.h"

namespace PulseOne {
namespace CSP {
using HttpTargetHandler = PulseOne::Gateway::Target::HttpTargetHandler;
}
} // namespace PulseOne

#endif // CSP_HTTP_TARGET_HANDLER_H
