/**
 * @file ITargetHandler.h
 * @brief Gateway Target Handler Interface - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_TARGET_I_TARGET_HANDLER_H
#define GATEWAY_TARGET_I_TARGET_HANDLER_H

#include "Export/GatewayExportTypes.h"
#include "Export/TargetHandlerFactory.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Gateway/Model/ValueMessage.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

/**
 * @brief Macro to register target handlers
 */
#define REGISTER_TARGET_HANDLER(type_name, class_name)                         \
  namespace {                                                                  \
  struct class_name##Registrar {                                               \
    class_name##Registrar() {                                                  \
      PulseOne::Export::TargetHandlerFactory::getInstance().registerCreator(   \
          type_name,                                                           \
          [](const std::string & /* config */)                                 \
              -> std::unique_ptr<PulseOne::Export::ITargetHandler> {           \
            return std::make_unique<class_name>();                             \
          });                                                                  \
    }                                                                          \
  };                                                                           \
  static class_name##Registrar g_##class_name##_registrar;                     \
  }

namespace PulseOne {
namespace Gateway {
namespace Target {

using TargetSendResult = PulseOne::Export::TargetSendResult;

/**
 * @brief Common interface for all target handlers
 */
class ITargetHandler : public PulseOne::Export::ITargetHandler {
protected:
  std::string target_name_;
  std::string target_type_;

public:
  virtual ~ITargetHandler() = default;

  // Essential methods
  virtual TargetSendResult
  sendAlarm(const json &payload,
            const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) = 0;

  virtual TargetSendResult
  sendValue(const json &payload,
            const PulseOne::Gateway::Model::ValueMessage &value,
            const json &config) = 0;

  virtual bool testConnection(const json &config) = 0;
  virtual std::string getHandlerType() const = 0;
  virtual bool validateConfig(const json &config,
                              std::vector<std::string> &errors) = 0;

  std::string getTargetName() const override { return target_name_; }
  std::string getTargetType() const override { return target_type_; }

  // Optional file transfer
  virtual TargetSendResult sendFile(const std::string &local_path,
                                    const json &config) {
    TargetSendResult result;
    result.error_message = "File export not supported by this handler";
    return result;
  }

  virtual bool initialize(const json & /* config */) { return true; }
  virtual void cleanup() {}

  virtual json getStatus() const {
    return json{{"type", getHandlerType()}, {"status", "active"}};
  }

  // Batch methods
  virtual std::vector<TargetSendResult> sendAlarmBatch(
      const std::vector<json> &payloads,
      const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
      const json &config) {
    std::vector<TargetSendResult> results;
    for (size_t i = 0; i < alarms.size() && i < payloads.size(); ++i) {
      results.push_back(sendAlarm(payloads[i], alarms[i], config));
    }
    return results;
  }

  virtual std::vector<TargetSendResult> sendValueBatch(
      const std::vector<json> &payloads,
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) {
    return {};
  }
};

} // namespace Target
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_TARGET_I_TARGET_HANDLER_H
