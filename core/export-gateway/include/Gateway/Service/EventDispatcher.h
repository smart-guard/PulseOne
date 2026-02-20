/**
 * @file EventDispatcher.h
 * @brief Event Dispatcher - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_EVENT_DISPATCHER_H
#define GATEWAY_SERVICE_EVENT_DISPATCHER_H

#include "Event/GatewayEventSubscriber.h"
#include "Gateway/Service/GatewayContext.h"
#include <memory>
#include <string>
#include <vector>
// Note: nlohmann/json.hpp is intentionally NOT included here.
// It is included only in EventDispatcher.cpp to avoid propagating
// 27,000+ lines to every translation unit that includes this header.

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief Dispatches events from EventSubscriber to appropriate services
 */
class EventDispatcher {
private:
  GatewayContext &context_;

public:
  explicit EventDispatcher(GatewayContext &context);
  ~EventDispatcher();

  void registerHandlers(PulseOne::Event::GatewayEventSubscriber &subscriber);

  // Event handlers (called by internal bridge classes)
  void handleAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  void handleScheduleEvent(const std::string &channel,
                           const std::string &message);
  void handleConfigEvent(const std::string &channel,
                         const std::string &message);
  void handleCommandEvent(const std::string &channel,
                          const std::string &message);
  // [v3.2] Alarm routing from base workerLoop (via GenericHandlerBridge)
  void routeAlarmMessage(const std::string &channel,
                         const std::string &message);

private:
  // payload_json: JSON serialized as std::string (parsed inside .cpp only)
  void handleManualExport(const std::string &payload_json);
  void sendManualExportResult(const std::string &target_name, bool success,
                              const std::string &error_message,
                              const std::string &payload_json,
                              int target_id = 0);

  void logExportResult(const PulseOne::Export::TargetSendResult &result,
                       const PulseOne::CSP::AlarmMessage *alarm = nullptr);
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_EVENT_DISPATCHER_H
