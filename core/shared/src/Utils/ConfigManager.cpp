/**
 * @file ConfigManager.cpp
 * @brief shared용 ConfigManager 구현부 - SecretManager에 완전 위임
 * @author PulseOne Development Team
 * @date 2025-09-23
 */

#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"
#include "Security/SecretManager.h"
#include "Utils/ConfigPathResolver.h"
#include "Utils/ConfigTemplateGenerator.h"
#include "Utils/ConfigVariableExpander.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace PulseOne::Platform;
using namespace PulseOne::Security;
using namespace PulseOne::Utils;

// =============================================================================
// ConfigManager 생성자
// =============================================================================

ConfigManager::ConfigManager()
    : initialized_(false), secret_manager_initialized_(false) {}

ConfigManager &ConfigManager::getInstance() {
  static ConfigManager instance;
  instance.ensureInitialized();
  return instance;
}

// =============================================================================
// 초기화 관련
// =============================================================================

bool ConfigManager::doInitialize() {
  // 1. 설정 디렉토리 찾기
  auto searchResult = ConfigPathResolver::findConfigDirectory();
  configDir_ = searchResult.foundPath;
  searchLog_ = searchResult.searchLog;

  if (configDir_.empty()) {
    initialized_.store(true);
    return false;
  }

  // 2. 기본 템플릿 생성 (새 모듈에 위임)
  ConfigTemplateGenerator::generateDefaultConfigs(configDir_);

  // 3. 설정 파일들 로드
  loadMainConfig();
  loadAdditionalConfigs();

  // 4. dataDir 설정
  dataDir_ =
      ConfigPathResolver::findDataDirectory(getOrDefault("DATA_DIR", ""));

  // 5. 데이터 디렉토리 구조 보장
  if (!dataDir_.empty()) {
    std::vector<std::string> sub_dirs = {"db", "logs", "backups", "cache",
                                         "temp"};
    for (const auto &dir : sub_dirs) {
      ConfigPathResolver::ensureDirectory(Path::Join(dataDir_, dir));
    }
  }

  // 6. SecretManager 초기화 및 의존성 주입
  initializeSecretManager();

  initialized_.store(true);
  return true;
}

void ConfigManager::ensureInitialized() {
  if (initialized_.load(std::memory_order_acquire)) {
    return;
  }

  static thread_local bool in_config_init = false;
  if (in_config_init) {
    return; // 현재 스레드에서 이미 초기화 중이면 재진입 방지
  }

  std::lock_guard<std::recursive_mutex> lock(init_mutex_);
  if (initialized_.load(std::memory_order_relaxed)) {
    return;
  }

  in_config_init = true;
  doInitialize();
  in_config_init = false;

  initialized_.store(true, std::memory_order_release);
}

void ConfigManager::reload() {
  LogManager::getInstance().log("config", LogLevel::INFO,
                                "ConfigManager 재로딩 시작...");

  {
    std::lock_guard<std::mutex> lock(configMutex);
    configMap.clear();
    loadedFiles_.clear();
    searchLog_.clear();
  }

  initialized_.store(false);
  doInitialize();
}

// =============================================================================
// SecretManager 연동 관련
// =============================================================================

// SecretManager 연동은 ConfigSecrets.cpp에서 구현됨

// setEncryptionMode는 ConfigSecrets.cpp에서 구현됨

// =============================================================================
// 기존 경로 및 파일 관련 메서드들
// =============================================================================

// =============================================================================
// 기존 설정 파일 생성 메서드들 (DEPRECATED - Moved to ConfigPathResolver)
// =============================================================================

// =============================================================================
// 기존 설정 파일 로딩
// =============================================================================

// 설정 파일 로딩 및 파싱 로직은 ConfigPersistence.cpp에서 구현됨

// parseLine는 ConfigPersistence.cpp에서 구현됨

// =============================================================================
// 기존 읽기 인터페이스
// =============================================================================

std::string ConfigManager::get(const std::string &key) const {
  // 1. 메모리 설정 확인 (Manual override or file load)
  {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    if (it != configMap.end() && !it->second.empty()) {
      return it->second;
    }
  }

  // 2. 환경변수 확인 (Fall-back or Docker defaults)
  const char *env_val = std::getenv(key.c_str());
  if (env_val) {
    return std::string(env_val);
  }

  return "";
}

std::string ConfigManager::getOrDefault(const std::string &key,
                                        const std::string &defaultValue) const {
  if (key == "SQLITE_DB_PATH" || key == "SQLITE_PATH") {
    return ConfigPathResolver::resolveSQLitePath(
        [this](const std::string &k) { return get(k); }, dataDir_);
  }

  std::string value = get(key);
  return value.empty() ? defaultValue : value;
}

std::string ConfigManager::getInstanceKey() const {
  std::string key = get("INSTANCE_KEY");
  if (!key.empty())
    return key;

  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname);
  }
  return "unknown-instance";
}

void ConfigManager::set(const std::string &key, const std::string &value) {
  std::lock_guard<std::mutex> lock(configMutex);
  configMap[key] = value;

  // Sync to environment so that get() (which checks getenv first) sees the
  // update
#ifndef _WIN32
  setenv(key.c_str(), value.c_str(), 1);
#else
  _putenv_s(key.c_str(), value.c_str());
#endif
}

bool ConfigManager::hasKey(const std::string &key) const {
  std::lock_guard<std::mutex> lock(configMutex);
  return configMap.find(key) != configMap.end();
}

std::map<std::string, std::string> ConfigManager::listAll() const {
  std::lock_guard<std::mutex> lock(configMutex);
  return configMap;
}

int ConfigManager::getInt(const std::string &key, int defaultValue) const {
  std::string value = get(key);
  if (value.empty())
    return defaultValue;
  try {
    return std::stoi(value);
  } catch (...) {
    return defaultValue;
  }
}

bool ConfigManager::getBool(const std::string &key, bool defaultValue) const {
  std::string value = get(key);
  if (value.empty())
    return defaultValue;

  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  return (value == "true" || value == "yes" || value == "1" || value == "on");
}

double ConfigManager::getDouble(const std::string &key,
                                double defaultValue) const {
  std::string value = get(key);
  if (value.empty())
    return defaultValue;
  try {
    return std::stod(value);
  } catch (...) {
    return defaultValue;
  }
}

// =============================================================================
// 경로 관련 메서드들
// =============================================================================

std::string ConfigManager::getDataDirectory() const { return dataDir_; }

std::string ConfigManager::getSQLiteDbPath() const {
  return ConfigPathResolver::resolveSQLitePath(
      [this](const std::string &k) { return get(k); }, dataDir_);
}

std::string ConfigManager::getBackupDirectory() const {
  return ConfigPathResolver::resolveBackupDirectory(
      [this](const std::string &k) { return get(k); }, dataDir_);
}

std::string ConfigManager::getSecretsDirectory() const {
  return ConfigPathResolver::getSecretsDirectory(configDir_);
}

// =============================================================================
// 모듈 관리
// =============================================================================

std::string ConfigManager::getActiveDatabaseType() const {
  std::string db_type = getOrDefault("DATABASE_TYPE", "SQLITE");
  std::transform(db_type.begin(), db_type.end(), db_type.begin(), ::toupper);
  return db_type;
}

bool ConfigManager::isModuleEnabled(const std::string &module_name) const {
  std::string key = module_name + "_ENABLED";
  return getBool(key, false);
}

// =============================================================================
// 기존 보안 관련 (하위 호환용)
// =============================================================================

std::string ConfigManager::loadPasswordFromFile(
    const std::string &password_file_key) const {
  return getSecret(password_file_key);
}

void ConfigManager::triggerVariableExpansion() { expandAllVariables(); }

// =============================================================================
// 변수 확장
// =============================================================================

void ConfigManager::expandAllVariables() {
  std::lock_guard<std::mutex> lock(configMutex);

  for (auto &[key, value] : configMap) {
    value = expandVariables(value);
  }
}

std::string ConfigManager::expandVariables(const std::string &value) const {
  return ConfigVariableExpander::expandPulseOneVariables(
      value,
      [this](const std::string &var_name) -> std::string {
        std::string val = get(var_name);
        if (!val.empty())
          return val;

        // [HOTFIX] INSITE_ 계열 변수가 없을 경우 보안 파일에서 지연 로딩 시도
        if (var_name.compare(0, 7, "INSITE_") == 0) {
          std::string sec_path = Path::Join(configDir_, "security.env");
          if (FileSystem::FileExists(sec_path)) {
            // const_cast를 통해 지연 로딩 수행
            const_cast<ConfigManager *>(this)->loadConfigFile(sec_path);
            return get(var_name);
          }
        }
        return "";
      },
      configDir_, dataDir_);
}

// =============================================================================
// 신규: 설정 파일 쓰기 기능
// =============================================================================

// 시크릿 및 파일 쓰기 관련 모든 메서드들은 ConfigPersistence.cpp 및
// ConfigSecrets.cpp에서 구현됨