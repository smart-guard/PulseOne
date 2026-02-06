/**
 * @file FileTargetHandler.h
 * @brief File Target Handler - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#ifndef GATEWAY_TARGET_FILE_TARGET_HANDLER_H
#define GATEWAY_TARGET_FILE_TARGET_HANDLER_H

#include "Gateway/Target/ITargetHandler.h"
#include <atomic>
#include <string>
#include <vector>

namespace PulseOne {
namespace Gateway {
namespace Target {

/**
 * @brief File Target Handler (v2.0 standardized)
 */
class FileTargetHandler : public ITargetHandler {
private:
  std::atomic<size_t> file_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};
  std::atomic<size_t> total_bytes_written_{0};

public:
  FileTargetHandler();
  ~FileTargetHandler() override;

  bool initialize(const json &config) override;

  TargetSendResult
  sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm,
            const json &config) override;

  std::vector<TargetSendResult> sendValueBatch(
      const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
      const json &config) override;

  bool testConnection(const json &config) override;
  std::string getHandlerType() const override { return "FILE"; }
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;
  void cleanup() override;
  json getStatus() const override;

private:
  std::string extractBasePath(const json &config) const;
  std::string
  generateFilePath(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   const json &config) const;
  void createDirectoriesForFile(const std::string &file_path) const;

  std::string
  buildFileContent(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                   const json &config) const;
  bool writeFile(const std::string &file_path, const std::string &content,
                 const json &config) const;

  std::string
  expandTemplate(const std::string &template_str,
                 const PulseOne::Gateway::Model::AlarmMessage &alarm) const;
};

} // namespace Target
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_TARGET_FILE_TARGET_HANDLER_H
