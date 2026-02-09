/**
 * @file MqttTargetHandler.cpp
 * @brief MQTT Target Handler implementation - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#include "Gateway/Target/MqttTargetHandler.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ConfigManager.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Gateway {
namespace Target {

MqttTargetHandler::MqttTargetHandler() {
  LogManager::getInstance().Info(
      "MqttTargetHandler initialized (Gateway::Target)");
}

MqttTargetHandler::~MqttTargetHandler() { disconnectFromBroker(); }

bool MqttTargetHandler::initialize(const json &config) {
  std::vector<std::string> errors;
  if (!validateConfig(config, errors)) {
    for (const auto &err : errors) {
      LogManager::getInstance().Error("MQTT Config Error: " + err);
    }
    return false;
  }
  return connectToBroker(config);
}

TargetSendResult MqttTargetHandler::sendAlarm(
    const json &payload_json,
    const PulseOne::Gateway::Model::AlarmMessage &alarm, const json &config) {
  TargetSendResult result;
  result.target_type = "MQTT";
  result.target_name = config.value("name", "MQTT_Target");
  result.success = false;

  if (!is_connected_.load()) {
    if (!connectToBroker(config)) {
      result.error_message = "MQTT not connected and failed to reconnect";
      return result;
    }
  }

  std::string topic = generateTopic(alarm, config);
  std::string payload = payload_json.dump();

  result = publishMessage(topic, payload, config.value("qos", 1),
                          config.value("retain", false));
  if (result.success) {
    success_count_++;
  } else {
    failure_count_++;
  }
  publish_count_++;

  return result;
}

std::vector<TargetSendResult> MqttTargetHandler::sendValueBatch(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config) {
  std::vector<TargetSendResult> results;
  for (size_t i = 0; i < values.size() && i < payloads.size(); ++i) {
    TargetSendResult res;
    // Note: generateTopic is usually for alarms, for values we might need a
    // different topic strategy but here we use a simple approach or follow
    // original pattern
    std::string topic = config.value("topic", "pulseone/values");
    std::string payload = payloads[i].dump();
    res = publishMessage(topic, payload, config.value("qos", 0), false);
    results.push_back(res);
  }
  return results;
}

bool MqttTargetHandler::testConnection(const json &config) {
  if (is_connected_.load())
    return true;
  return connectToBroker(config);
}

bool MqttTargetHandler::validateConfig(const json &config,
                                       std::vector<std::string> &errors) {
  if (!config.contains("broker_uri") && !config.contains("host")) {
    errors.push_back("broker_uri or host is required");
    return false;
  }
  return true;
}

void MqttTargetHandler::cleanup() { disconnectFromBroker(); }

json MqttTargetHandler::getStatus() const {
  return json{{"type", "MQTT"},
              {"status", is_connected_.load() ? "connected" : "disconnected"},
              {"publish_count", publish_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()}};
}

// Private implementations...
bool MqttTargetHandler::connectToBroker(const json &config) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  if (is_connected_.load())
    return true;

#if MQTT_AVAILABLE
  try {
    std::string host = ConfigManager::getInstance().expandVariables(
        config.value("host", "localhost"));
    int port = config.value("port", 1883);
    broker_uri_ = ConfigManager::getInstance().expandVariables(config.value(
        "broker_uri", "tcp://" + host + ":" + std::to_string(port)));

    client_id_ = ConfigManager::getInstance().expandVariables(config.value(
        "client_id",
        "PulseOne_Gateway_" +
            std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count())));

    mqtt_client_ =
        std::make_unique<mqtt::async_client>(broker_uri_, client_id_);

    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    if (config.contains("username")) {
      connOpts.set_user_name(
          ConfigManager::getInstance().expandVariables(config["username"]));
      if (config.contains("password")) {
        connOpts.set_password(
            ConfigManager::getInstance().expandVariables(config["password"]));
      }
    }

    mqtt_client_->connect(connOpts)->wait();
    is_connected_ = true;
    LogManager::getInstance().Info("MQTT Connected: " + broker_uri_);
    return true;
  } catch (const mqtt::exception &exc) {
    LogManager::getInstance().Error("MQTT Connection Error: " +
                                    std::string(exc.what()));
    return false;
  }
#else
  LogManager::getInstance().Warn("MQTT not available in this build");
  return false;
#endif
}

void MqttTargetHandler::disconnectFromBroker() {
  std::lock_guard<std::mutex> lock(client_mutex_);
#if MQTT_AVAILABLE
  if (mqtt_client_ && is_connected_.load()) {
    try {
      mqtt_client_->disconnect()->wait();
    } catch (...) {
    }
  }
  mqtt_client_.reset();
#endif
  is_connected_ = false;
}

std::string MqttTargetHandler::generateTopic(
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  // Simple fallback since expandTemplateVariables was removed
  std::string topic_val =
      config.value("topic", "pulseone/alarms/" + std::to_string(alarm.site_id) +
                                "/" + alarm.point_name);
  return ConfigManager::getInstance().expandVariables(topic_val);
}

TargetSendResult MqttTargetHandler::publishMessage(const std::string &topic,
                                                   const std::string &payload,
                                                   int qos, bool retain) {
  TargetSendResult result;
  result.target_type = "MQTT";
  result.success = false;

#if MQTT_AVAILABLE
  std::lock_guard<std::mutex> lock(client_mutex_);
  if (!mqtt_client_ || !is_connected_.load()) {
    result.error_message = "MQTT client not connected";
    return result;
  }

  try {
    auto msg = mqtt::make_message(topic, payload, qos, retain);
    mqtt_client_->publish(msg)->wait();
    result.success = true;
    result.sent_payload = payload;
  } catch (const mqtt::exception &exc) {
    result.error_message = "MQTT Publish Error: " + std::string(exc.what());
    result.sent_payload = payload;
    is_connected_ = false; // Flag for reconnection
  }
#else
  result.error_message = "MQTT not available in this build";
#endif

  return result;
}

REGISTER_TARGET_HANDLER("MQTT", MqttTargetHandler);

} // namespace Target
} // namespace Gateway
} // namespace PulseOne
