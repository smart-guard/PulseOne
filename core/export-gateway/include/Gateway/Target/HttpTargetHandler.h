/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS Target Handler - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_TARGET_HTTP_TARGET_HANDLER_H
#define GATEWAY_TARGET_HTTP_TARGET_HANDLER_H

#include "Gateway/Target/ITargetHandler.h"
#include <atomic>
#include <memory>
#include <unordered_map>

namespace PulseOne {
namespace Client {
class HttpClient;
}

namespace Gateway {
namespace Target {

/**
 * @brief Retry configuration
 */
struct RetryConfig {
  int max_attempts = 3;
  uint32_t initial_delay_ms = 1000;
  uint32_t max_delay_ms = 30000;
  double backoff_multiplier = 2.0;
};

/**
 * @brief HTTP/HTTPS Target Handler (v5.0 standardized)
 */
class HttpTargetHandler : public ITargetHandler {
private:
  std::atomic<size_t> request_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};

public:
  HttpTargetHandler();
  ~HttpTargetHandler() override;

  bool initialize(const json &config) override;

  TargetSendResult
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) override;

  std::vector<TargetSendResult> sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) override;

  bool testConnection(const json &config) override;
  std::string getHandlerType() const override { return "HTTP"; }
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;
  void cleanup() override;
  json getStatus() const override;

private:
  std::shared_ptr<PulseOne::Client::HttpClient>
  getOrCreateClient(const json &config, const std::string &url);
  std::string extractUrl(const json &config) const;

  TargetSendResult
  executeWithRetry(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   const json &config, const std::string &url);
  TargetSendResult executeWithRetry(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config, const std::string &url);

  TargetSendResult
  executeSingleRequest(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                       const json &config, const std::string &url);
  TargetSendResult executeSingleRequest(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config, const std::string &url);

  std::unordered_map<std::string, std::string>
  buildRequestHeaders(const json &config);
  std::string
  buildRequestBody(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   const json &config);
  std::string buildRequestBody(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config);

  uint32_t calculateBackoffDelay(int attempt, const RetryConfig &config) const;
  std::string getTargetName(const json &config) const;
  std::string getCurrentTimestamp() const;
  std::string generateRequestId() const;

  void expandTemplateVariables(
      json &template_json,
      const PulseOne::Gateway::Model::AlarmMessage &alarm) const;
  void expandTemplateVariables(
      json &template_json,
      const PulseOne::Gateway::Model::ValueMessage &value) const;

  std::string base64Encode(const std::string &input) const;
};

} // namespace Target
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_TARGET_HTTP_TARGET_HANDLER_H
