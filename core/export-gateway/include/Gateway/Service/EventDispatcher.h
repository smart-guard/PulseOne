/**
 * @file EventDispatcher.h
 * @brief Event Dispatcher - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_EVENT_DISPATCHER_H
#define GATEWAY_SERVICE_EVENT_DISPATCHER_H

#include "Event/EventSubscriber.h"
#include "Gateway/Service/GatewayContext.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

  void registerHandlers(PulseOne::Event::EventSubscriber &subscriber);

  // Event handlers (called by internal bridge classes)
  void handleAlarm(const PulseOne::CSP::AlarmMessage &alarm);
  void handleScheduleEvent(const std::string &channel,
                           const std::string &message);
  void handleConfigEvent(const std::string &channel,
                         const std::string &message);
  void handleCommandEvent(const std::string &channel,
                          const std::string &message);

private:
  void handleManualExport(const nlohmann::json &payload);
  void sendManualExportResult(const std::string &target_name, bool success,
                              const std::string &error_message,
                              const nlohmann::json &payload, int target_id = 0);

  /**
   * @brief 통합 로깅 처리
   */
  void logExportResult(const PulseOne::Export::TargetSendResult &result,
                       const PulseOne::CSP::AlarmMessage *alarm = nullptr);
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_EVENT_DISPATCHER_H
