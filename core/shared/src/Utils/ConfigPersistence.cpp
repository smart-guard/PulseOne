/**
 * @file ConfigPersistence.cpp
 * @brief ConfigManager의 파일 지속성(Persistence) 로직 구현부
 *
 * 담당 역할:
 * - .env 설정 파일 로드 및 파싱
 * - 설정 변경사항 파일 쓰기 및 동기화
 * - 각 키별 저장 파일 결정
 */

#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"
#include "Utils/ConfigManager.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace PulseOne::Platform;
using namespace PulseOne::Utils;

// =============================================================================
// 설정 파일 로딩 로직
// =============================================================================

void ConfigManager::loadMainConfig() {
  std::string main_env_path = Path::Join(configDir_, ".env");

  if (FileSystem::FileExists(main_env_path)) {
    loadConfigFile(main_env_path);
    LogManager::getInstance().log("config", LogLevel::INFO,
                                  "메인 설정 로드: .env");
  } else {
    LogManager::getInstance().log("config", LogLevel::WARN,
                                  "메인 설정 파일 없음: .env");
  }
}

void ConfigManager::loadAdditionalConfigs() {
  LogManager::getInstance().log("config", LogLevel::INFO,
                                "추가 설정 파일 확인 시작");

  std::string config_files;
  {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find("CONFIG_FILES");
    config_files = (it != configMap.end()) ? it->second : "";
  }

  if (config_files.empty()) {
    LogManager::getInstance().log(
        "config", LogLevel::INFO,
        "추가 설정 파일 없음 (CONFIG_FILES 비어있음)");
    return;
  }

  std::stringstream ss(config_files);
  std::string filename;

  while (std::getline(ss, filename, ',')) {
    filename.erase(0, filename.find_first_not_of(" \t"));
    filename.erase(filename.find_last_not_of(" \t") + 1);

    if (!filename.empty()) {
      std::string full_path = Path::Join(configDir_, filename);
      try {
        if (FileSystem::FileExists(full_path)) {
          loadConfigFile(full_path);
          LogManager::getInstance().log("config", LogLevel::INFO,
                                        "추가 설정 로드: " + filename);
        } else {
          LogManager::getInstance().log("config", LogLevel::INFO,
                                        "추가 설정 파일 없음: " + filename);
        }
      } catch (const std::exception &e) {
        LogManager::getInstance().log("config", LogLevel::WARN,
                                      "설정 파일 로드 실패: " + filename +
                                          " - " + e.what());
      }
    }
  }

  LogManager::getInstance().log("config", LogLevel::INFO,
                                "추가 설정 파일 확인 완료");
}

void ConfigManager::loadConfigFile(const std::string &filepath) {
#if PULSEONE_WINDOWS
  std::wstring wPath = FileSystem::ToUtf16(filepath);
  std::ifstream file(wPath.c_str());
#else
  std::ifstream file(filepath);
#endif

  if (!file.is_open()) {
    LogManager::getInstance().log("config", LogLevel::LOG_ERROR,
                                  "파일 열기 실패: " + filepath);
    return;
  }

  std::string line;
  int line_count = 0;
  int parsed_count = 0;

  {
    std::lock_guard<std::mutex> lock(configMutex);
    size_t original_size = configMap.size();

    while (std::getline(file, line)) {
      line_count++;
      try {
        parseLine(line);
        if (configMap.size() > original_size + parsed_count) {
          parsed_count++;
        }
      } catch (const std::exception &) {
        continue;
      }
    }

    loadedFiles_.push_back(filepath);
  }

  file.close();

  LogManager::getInstance().log(
      "config", LogLevel::INFO,
      Path::GetFileName(filepath) + " - " + std::to_string(parsed_count) + "/" +
          std::to_string(line_count) + " 라인 파싱됨");
}

void ConfigManager::parseLine(const std::string &line) {
  if (line.empty() || line[0] == '#') {
    return;
  }

  size_t pos = line.find('=');
  if (pos == std::string::npos) {
    return;
  }

  std::string key = line.substr(0, pos);
  std::string value = line.substr(pos + 1);

  key.erase(0, key.find_first_not_of(" \t\r\n"));
  key.erase(key.find_last_not_of(" \t\r\n") + 1);
  value.erase(0, value.find_first_not_of(" \t\r\n"));
  value.erase(value.find_last_not_of(" \t\r\n") + 1);

  if (value.length() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1, value.length() - 2);
  }

  if (!key.empty()) {
    configMap[key] = value;
  }
}

// =============================================================================
// 설정 파일 쓰기/업데이트 로직
// =============================================================================

bool ConfigManager::updateConfig(const std::string &key,
                                 const std::string &value,
                                 const std::string &config_file) {
  set(key, value);
  std::string target_file =
      config_file.empty() ? determineConfigFile(key) : config_file;
  std::lock_guard<std::mutex> write_lock(write_mutex_);
  dirty_files_[target_file] = true;
  return rewriteConfigFile(target_file);
}

bool ConfigManager::updateMultipleConfigs(
    const std::map<std::string, std::string> &updates,
    const std::string &config_file) {
  for (const auto &[key, value] : updates) {
    set(key, value);
  }

  std::lock_guard<std::mutex> write_lock(write_mutex_);

  if (!config_file.empty()) {
    dirty_files_[config_file] = true;
    return rewriteConfigFile(config_file);
  } else {
    std::map<std::string, std::vector<std::string>> file_keys;
    for (const auto &[key, value] : updates) {
      std::string target_file = determineConfigFile(key);
      file_keys[target_file].push_back(key);
      dirty_files_[target_file] = true;
    }

    bool all_success = true;
    for (const auto &[file, keys] : file_keys) {
      if (!rewriteConfigFile(file)) {
        all_success = false;
      }
    }
    return all_success;
  }
}

bool ConfigManager::saveAllChanges() {
  std::lock_guard<std::mutex> write_lock(write_mutex_);

  bool all_success = true;
  for (const auto &[file, is_dirty] : dirty_files_) {
    if (is_dirty) {
      if (rewriteConfigFile(file)) {
        dirty_files_[file] = false;
      } else {
        all_success = false;
      }
    }
  }

  return all_success;
}

bool ConfigManager::rewriteConfigFile(const std::string &config_file) {
  std::string file_path = Path::Join(configDir_, config_file);
  std::map<std::string, std::string> file_configs = parseConfigFile(file_path);

  std::lock_guard<std::mutex> lock(configMutex);
  for (const auto &[key, value] : configMap) {
    if (determineConfigFile(key) == config_file) {
      file_configs[key] = value;
    }
  }

  return writeConfigToFile(file_path, file_configs);
}

bool ConfigManager::writeConfigToFile(
    const std::string &filepath,
    const std::map<std::string, std::string> &config_data) {
  try {
    std::string dir = Path::GetDirectory(filepath);
    if (!FileSystem::DirectoryExists(dir)) {
      FileSystem::CreateDirectoryRecursive(dir);
    }

#if PULSEONE_WINDOWS
    std::wstring wPath = FileSystem::ToUtf16(filepath);
    std::ofstream file(wPath.c_str());
#else
    std::ofstream file(filepath);
#endif

    if (!file.is_open()) {
      return false;
    }

    std::string filename = Path::GetFileName(filepath);
    file << "# " << filename << " - PulseOne 설정 파일\n";

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    file << "# 자동 생성됨: "
         << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
         << "\n\n";

    for (const auto &[key, value] : config_data) {
      if (!key.empty() && key[0] != '#') {
        file << key << "=" << value << "\n";
      }
    }

    file.close();
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("config", LogLevel::LOG_ERROR,
                                  "설정 파일 쓰기 실패: " + filepath + " - " +
                                      e.what());
    return false;
  }
}

std::map<std::string, std::string>
ConfigManager::parseConfigFile(const std::string &filepath) const {
  std::map<std::string, std::string> result;

  if (!FileSystem::FileExists(filepath)) {
    return result;
  }

#if PULSEONE_WINDOWS
  std::wstring wPath = FileSystem::ToUtf16(filepath);
  std::ifstream file(wPath.c_str());
#else
  std::ifstream file(filepath);
#endif

  if (!file.is_open()) {
    return result;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    size_t pos = line.find('=');
    if (pos != std::string::npos) {
      std::string key = line.substr(0, pos);
      std::string value = line.substr(pos + 1);

      key.erase(0, key.find_first_not_of(" \t"));
      key.erase(key.find_last_not_of(" \t") + 1);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t") + 1);

      if (!key.empty()) {
        result[key] = value;
      }
    }
  }

  file.close();
  return result;
}

std::string ConfigManager::determineConfigFile(const std::string &key) const {
  if (key.find("DATABASE") != std::string::npos ||
      key.find("POSTGRES") != std::string::npos ||
      key.find("MYSQL") != std::string::npos ||
      key.find("SQLITE") != std::string::npos) {
    return "database.env";
  }

  if (key.find("REDIS") != std::string::npos) {
    return "redis.env";
  }

  if (key.find("INFLUX") != std::string::npos) {
    return "timeseries.env";
  }

  if (key.find("RABBITMQ") != std::string::npos ||
      key.find("MQTT") != std::string::npos ||
      key.find("MESSAGING") != std::string::npos) {
    return "messaging.env";
  }

  if (key.find("JWT") != std::string::npos ||
      key.find("SSL") != std::string::npos ||
      key.find("ACCESS_CONTROL") != std::string::npos) {
    return "security.env";
  }

  return ".env";
}
