/**
 * @file FileTargetHandler.cpp
 * @brief File Target Handler implementation - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#include "Gateway/Target/FileTargetHandler.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Gateway {
namespace Target {

FileTargetHandler::FileTargetHandler() {
  LogManager::getInstance().Info(
      "FileTargetHandler initialized (Gateway::Target)");
}

FileTargetHandler::~FileTargetHandler() {}

bool FileTargetHandler::initialize(const json &config) {
  std::string base_path = extractBasePath(config);
  if (!base_path.empty()) {
    std::filesystem::create_directories(base_path);
  }
  return true;
}

TargetSendResult FileTargetHandler::sendAlarm(
    const json &payload, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) {
  TargetSendResult result;
  result.target_type = "FILE";
  result.success = false;

  try {
    std::string path = generateFilePath(alarm, config);
    std::string content = payload.dump(2);

    result.success = writeFile(path, content, config);
    if (result.success) {
      file_count_++;
      success_count_++;
      total_bytes_written_ += content.length();
    } else {
      result.error_message = "File write failed";
      failure_count_++;
    }
  } catch (const std::exception &e) {
    result.error_message = std::string("File exception: ") + e.what();
    failure_count_++;
  }

  return result;
}

std::vector<TargetSendResult> FileTargetHandler::sendValueBatch(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config) {
  std::vector<TargetSendResult> results;
  return results;
}

bool FileTargetHandler::testConnection(const json &config) {
  std::string base_path = extractBasePath(config);
  return !base_path.empty() && std::filesystem::exists(base_path);
}

bool FileTargetHandler::validateConfig(const json &config,
                                       std::vector<std::string> &errors) {
  if (!config.contains("path") && !config.contains("base_path")) {
    errors.push_back("path or base_path is required");
    return false;
  }
  return true;
}

void FileTargetHandler::cleanup() {}

json FileTargetHandler::getStatus() const {
  return json{{"type", "FILE"},
              {"file_count", file_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()}};
}

// Private implementations...
std::string FileTargetHandler::extractBasePath(const json &config) const {
  std::string val = config.value("base_path", config.value("path", ""));
  return ConfigManager::getInstance().expandVariables(val);
}

std::string FileTargetHandler::generateFilePath(
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  std::string base = extractBasePath(config);
  return base + "/" + alarm.point_name + ".json";
}

void FileTargetHandler::createDirectoriesForFile(
    const std::string &file_path) const {
  std::filesystem::create_directories(
      std::filesystem::path(file_path).parent_path());
}

bool FileTargetHandler::writeFile(const std::string &file_path,
                                  const std::string &content,
                                  const json &config) const {
  createDirectoriesForFile(file_path);
  std::ofstream ofs(file_path, std::ios::app);
  if (!ofs)
    return false;
  ofs << content << std::endl;
  return true;
}

REGISTER_TARGET_HANDLER("FILE", FileTargetHandler);

} // namespace Target
} // namespace Gateway
} // namespace PulseOne
