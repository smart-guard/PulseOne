/**
 * @file MqttTargetHandler.h
 * @brief MQTT Target Handler - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_TARGET_MQTT_TARGET_HANDLER_H
#define GATEWAY_TARGET_MQTT_TARGET_HANDLER_H

#include "Gateway/Target/ITargetHandler.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#ifdef HAS_PAHO_MQTT
#include <mqtt/async_client.h>
#define MQTT_AVAILABLE 1
#else
#define MQTT_AVAILABLE 0
#endif

namespace PulseOne {
namespace Gateway {
namespace Target {

/**
 * @brief MQTT Target Handler (v2.1 standardized)
 */
class MqttTargetHandler : public ITargetHandler {
private:
#if MQTT_AVAILABLE
  std::unique_ptr<mqtt::async_client> mqtt_client_;
#endif

  std::string broker_uri_;
  std::string client_id_;

  mutable std::mutex client_mutex_;
  std::atomic<bool> is_connected_{false};
  std::atomic<size_t> publish_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};

public:
  MqttTargetHandler();
  ~MqttTargetHandler() override;

  bool initialize(const json &config) override;

  TargetSendResult
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) override;

  std::vector<TargetSendResult> sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) override;

  bool testConnection(const json &config) override;
  std::string getHandlerType() const override { return "MQTT"; }
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;
  void cleanup() override;
  json getStatus() const override;

private:
  bool connectToBroker(const json &config);
  void disconnectFromBroker();

  std::string generateTopic(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                            const json &config) const;
  std::string
  generatePayload(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                  const json &config) const;

  TargetSendResult publishMessage(const std::string &topic,
                                  const std::string &payload, int qos,
                                  bool retain);

  std::string expandTemplateVariables(
      const std::string &template_str,
      const PulseOne::Gateway::Model::AlarmMessage &alarm) const;
};

} // namespace Target
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_TARGET_MQTT_TARGET_HANDLER_H
