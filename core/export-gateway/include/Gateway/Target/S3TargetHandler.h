/**
 * @file S3TargetHandler.h
 * @brief S3 Target Handler - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_TARGET_S3_TARGET_HANDLER_H
#define GATEWAY_TARGET_S3_TARGET_HANDLER_H

#include "Gateway/Target/ITargetHandler.h"
#include <atomic>
#include <memory>
#include <unordered_map>

namespace PulseOne {
namespace Client {
class S3Client;
struct S3Config;
} // namespace Client

namespace Gateway {
namespace Target {

/**
 * @brief S3 Target Handler (v2.0 standardized)
 */
class S3TargetHandler : public ITargetHandler {
private:
  std::atomic<size_t> upload_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};
  std::atomic<size_t> total_bytes_uploaded_{0};

public:
  S3TargetHandler();
  ~S3TargetHandler() override;

  bool initialize(const json &config) override;

  TargetSendResult
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) override;

  TargetSendResult sendFile(const std::string &local_path,
                            const json &config) override;

  std::vector<TargetSendResult> sendAlarmBatch(
      const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
      const json &config) override;

  std::vector<TargetSendResult> sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) override;

  bool testConnection(const json &config) override;
  std::string getHandlerType() const override { return "S3"; }
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;
  void cleanup() override;
  json getStatus() const override;

private:
  std::shared_ptr<PulseOne::Client::S3Client>
  getOrCreateClient(const json &config, const std::string &bucket_name);
  std::string extractBucketName(const json &config) const;

  std::string
  generateObjectKey(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                    const json &config) const;
  std::string
  expandTemplate(const std::string &template_str,
                 const PulseOne::Gateway::Model::AlarmMessage &alarm) const;

  void
  expandTemplateVariables(json &template_json,
                          const PulseOne::Gateway::Model::AlarmMessage &alarm,
                          const json &config) const;
  void
  expandTemplateVariables(json &template_json,
                          const PulseOne::Gateway::Model::ValueMessage &value,
                          const json &config) const;

  std::string
  buildJsonContent(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   const json &config) const;
  std::unordered_map<std::string, std::string>
  buildMetadata(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                const json &config) const;

  std::string compressContent(const std::string &content, int level) const;
  std::string getTargetName(const json &config) const;
  std::string getCurrentTimestamp() const;
  std::string generateRequestId() const;
  std::string generateTimestampString() const;
  std::string generateDateString() const;
  std::string generateYearString() const;
  std::string generateMonthString() const;
  std::string generateDayString() const;
  std::string generateHourString() const;
  std::string generateMinuteString() const;
  std::string generateSecondString() const;

  std::string expandEnvironmentVariables(const std::string &str) const;
};

} // namespace Target
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_TARGET_S3_TARGET_HANDLER_H
