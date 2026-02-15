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
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <regex>
#include <sstream>
#include <unistd.h>

using namespace PulseOne::Platform;
using namespace PulseOne::Security;

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
  /*
  LogManager::getInstance().log("config", LogLevel::INFO,
      "ConfigManager 초기화 시작...");
  */

  // 1. 설정 디렉토리 찾기
  configDir_ = findConfigDirectory();
  if (configDir_.empty()) {
    /*
    LogManager::getInstance().log("config", LogLevel::WARN,
        "설정 디렉토리 없음 - 환경변수만 사용");
    */
    initialized_.store(true);
    return false;
  }

  // 2. 기본 템플릿 생성
  createMainEnvFile();
  createDatabaseEnvFile();
  createRedisEnvFile();
  createTimeseriesEnvFile();
  createMessagingEnvFile();
  createSecurityEnvFile();
  createSecurityEnvFile();
  createSecretsDirectory();
  // 3. 설정 파일들 로드
  loadMainConfig();
  loadAdditionalConfigs();

  // 4. dataDir 설정 (설정 로드 후 실행하여 DATA_DIR 설정 반영)
  dataDir_ = findDataDirectory();

  // 4.1 데이터 디렉토리 생성 및 보장
  ensureDataDirectories();

  // 5. SecretManager 초기화 및 의존성 주입
  initializeSecretManager();

  initialized_.store(true);

  /*
  LogManager::getInstance().log("config", LogLevel::INFO,
      "ConfigManager 초기화 완료 (" + std::to_string(configMap.size()) +
      "개 설정, " + std::to_string(loadedFiles_.size()) + "개 파일)");
  */

  return true;
}

std::string ConfigManager::getInstanceKey() const {
  char hostname[1024];
  hostname[1023] = '\0';
  if (gethostname(hostname, 1023) != 0) {
    return "unknown-host";
  }

  // 한 PC에서 여러 인스턴스를 띄울 경우를 대비해 실행 경로의 해시를 조합
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    std::string path(cwd);
    size_t hash = std::hash<std::string>{}(path);
    std::stringstream ss;
    ss << hostname << ":" << std::hex << (hash & 0xFFFFFF);
    return ss.str();
  }

  return std::string(hostname);
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

void ConfigManager::initializeSecretManager() {
  if (secret_manager_initialized_.load()) {
    return;
  }

  try {
    auto &secret_manager = SecretManager::getInstance();
    secret_manager.setConfigManager(this);

    // 암호화 모드 설정 (설정에서 읽어와서 적용)
    std::string encryption_mode = getOrDefault("SECRET_ENCRYPTION_MODE", "XOR");
    if (encryption_mode == "NONE") {
      secret_manager.setEncryptionMode(SecretManager::EncryptionMode::NONE);
    } else if (encryption_mode == "AES") {
      secret_manager.setEncryptionMode(SecretManager::EncryptionMode::AES);
    } else {
      secret_manager.setEncryptionMode(SecretManager::EncryptionMode::XOR);
    }

    // 커스텀 암호화 키 설정 (있다면)
    std::string custom_key = getOrDefault("SECRET_ENCRYPTION_KEY", "");
    if (!custom_key.empty()) {
      secret_manager.setEncryptionKey(custom_key);
    }

    secret_manager_initialized_.store(true);

    LogManager::getInstance().log("config", LogLevel::INFO,
                                  "SecretManager 연동 완료");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("config", LogLevel::LOG_ERROR,
                                  "SecretManager 초기화 실패: " +
                                      std::string(e.what()));
  }
}

SecretManager &ConfigManager::getSecretManager() const {
  if (!secret_manager_initialized_.load()) {
    const_cast<ConfigManager *>(this)->initializeSecretManager();
  }
  return SecretManager::getInstance();
}

// =============================================================================
// 기존 경로 및 파일 관련 메서드들
// =============================================================================

std::string ConfigManager::findConfigDirectory() {
  searchLog_.clear();

  const char *env_config = std::getenv("PULSEONE_CONFIG_DIR");
  if (env_config && FileSystem::DirectoryExists(env_config)) {
    searchLog_.push_back("환경변수: " + std::string(env_config));
    return std::string(env_config);
  }

  std::string exe_dir = Path::GetExecutableDirectory();
  std::vector<std::string> search_paths = {Path::Join(exe_dir, "config"),
                                           Path::Join(exe_dir, "../config"),
                                           Path::Join(exe_dir, "../../config"),
                                           "./config",
                                           "../config",
                                           "../../config"};

  for (const auto &path : search_paths) {
    std::string normalized_path = Path::Normalize(path);

    if (FileSystem::DirectoryExists(normalized_path)) {
      try {
        std::string absolute_path = Path::GetAbsolute(normalized_path);
        searchLog_.push_back("발견: " + path + " -> " + absolute_path);
        return absolute_path;
      } catch (const std::exception &) {
        searchLog_.push_back("발견: " + normalized_path +
                             " (절대경로 변환 실패)");
        return normalized_path;
      }
    } else {
      searchLog_.push_back("없음: " + normalized_path);
    }
  }

  searchLog_.push_back("설정 디렉토리를 찾을 수 없음");
  return "";
}

std::string ConfigManager::findDataDirectory() {
  const char *pulseone_data_dir = std::getenv("PULSEONE_DATA_DIR");
  const char *data_dir_env = std::getenv("DATA_DIR");
  std::cout << "[DEBUG] ConfigManager - PULSEONE_DATA_DIR env: "
            << (pulseone_data_dir ? pulseone_data_dir : "nullptr") << std::endl;
  std::cout << "[DEBUG] ConfigManager - DATA_DIR env: "
            << (data_dir_env ? data_dir_env : "nullptr") << std::endl;

  if (pulseone_data_dir) {
    std::cout << "[DEBUG] ConfigManager - Using PULSEONE_DATA_DIR: "
              << pulseone_data_dir << std::endl;
    return std::string(pulseone_data_dir);
  }
  if (data_dir_env) {
    std::cout << "[DEBUG] ConfigManager - Using DATA_DIR from env: "
              << data_dir_env << std::endl;
    return std::string(data_dir_env);
  }

  std::string data_dir_from_config = getOrDefault("DATA_DIR", "./data");

  if (data_dir_from_config[0] != '/' &&
      data_dir_from_config.find(":\\") == std::string::npos) {
    std::string exe_dir = Path::GetExecutableDirectory();
    data_dir_from_config = Path::Join(exe_dir, data_dir_from_config);
  }

  try {
    FileSystem::CreateDirectoryRecursive(data_dir_from_config);
    return Path::GetAbsolute(data_dir_from_config);
  } catch (const std::exception &) {
    return "./data";
  }
}

bool ConfigManager::directoryExists(const std::string &path) {
  return FileSystem::DirectoryExists(path);
}

std::string ConfigManager::getExecutableDirectory() {
  return Path::GetExecutableDirectory();
}

void ConfigManager::printConfigSearchLog() const {
  LogManager::getInstance().log("config", LogLevel::INFO,
                                "설정 디렉토리 탐색 로그:");
  for (const auto &log_entry : searchLog_) {
    LogManager::getInstance().log("config", LogLevel::INFO, "  " + log_entry);
  }
}

std::map<std::string, bool> ConfigManager::checkAllConfigFiles() const {
  std::map<std::string, bool> file_status;

  std::vector<std::string> config_files = {".env",          "database.env",
                                           "redis.env",     "timeseries.env",
                                           "messaging.env", "security.env"};

  for (const auto &filename : config_files) {
    std::string full_path = Path::Join(configDir_, filename);
    file_status[filename] = FileSystem::FileExists(full_path);
  }

  return file_status;
}

// =============================================================================
// 기존 설정 파일 생성 메서드들
// =============================================================================

void ConfigManager::createMainEnvFile() {
  std::string env_path = Path::Join(configDir_, ".env");

  if (FileSystem::FileExists(env_path)) {
    return;
  }

  std::string content =
      R"(# =============================================================================
# PulseOne 메인 설정 파일 (.env) - 자동 생성됨
# =============================================================================

# 환경 설정
NODE_ENV=development
LOG_LEVEL=info
LOG_TO_CONSOLE=true
LOG_TO_FILE=true
LOG_FILE_PATH=./logs/

# 기본 데이터베이스 설정
DATABASE_TYPE=SQLITE

# 기본 디렉토리 설정
DATA_DIR=./data

# 추가 설정 파일들 (모듈별 분리)
CONFIG_FILES=database.env,timeseries.env,redis.env,messaging.env,security.env

# 시스템 설정
MAX_WORKER_THREADS=4
DEFAULT_TIMEOUT_MS=5000

# 로그 로테이션 설정
LOG_MAX_SIZE_MB=100
LOG_MAX_FILES=30
MAINTENANCE_MODE=false

# 시크릿 관리 설정
SECRET_ENCRYPTION_MODE=XOR
# SECRET_ENCRYPTION_KEY=사용자정의키
)";

  createFileFromTemplate(env_path, content);
}

void ConfigManager::createDatabaseEnvFile() {
  std::string db_path = Path::Join(configDir_, "database.env");
  if (FileSystem::FileExists(db_path))
    return;

  std::string content = R"(# 데이터베이스 설정
DATABASE_TYPE=SQLITE
SQLITE_DB_PATH=./data/db/pulseone.db

# PostgreSQL 설정
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=pulseone
POSTGRES_USER=postgres
POSTGRES_PASSWORD_FILE=${SECRETS_DIR}/postgres_primary.key

# MySQL 설정
MYSQL_HOST=localhost
MYSQL_PORT=3306
MYSQL_DATABASE=pulseone
MYSQL_USER=user
MYSQL_PASSWORD_FILE=${SECRETS_DIR}/mysql.key
)";

  createFileFromTemplate(db_path, content);
}

void ConfigManager::createRedisEnvFile() {
  std::string redis_path = Path::Join(configDir_, "redis.env");
  if (FileSystem::FileExists(redis_path))
    return;

  std::string content = R"(# Redis 설정
REDIS_PRIMARY_ENABLED=true
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/redis_primary.key
REDIS_PRIMARY_DB=0
)";

  createFileFromTemplate(redis_path, content);
}

void ConfigManager::createTimeseriesEnvFile() {
  std::string influx_path = Path::Join(configDir_, "timeseries.env");
  if (FileSystem::FileExists(influx_path))
    return;

  std::string content = R"(# InfluxDB 설정
INFLUX_ENABLED=true
INFLUX_HOST=localhost
INFLUX_PORT=8086
INFLUX_ORG=pulseone
INFLUX_BUCKET=telemetry_data
INFLUX_TOKEN_FILE=${SECRETS_DIR}/influx_token.key
)";

  createFileFromTemplate(influx_path, content);
}

void ConfigManager::createMessagingEnvFile() {
  std::string msg_path = Path::Join(configDir_, "messaging.env");
  if (FileSystem::FileExists(msg_path))
    return;

  std::string content = R"(# 메시징 설정
MESSAGING_TYPE=RABBITMQ

# RabbitMQ 설정
RABBITMQ_ENABLED=true
RABBITMQ_HOST=localhost
RABBITMQ_PORT=5672
RABBITMQ_USERNAME=guest
RABBITMQ_PASSWORD_FILE=${SECRETS_DIR}/rabbitmq.key

# MQTT 설정
MQTT_ENABLED=false
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_CLIENT_ID=pulseone_collector
)";

  createFileFromTemplate(msg_path, content);
}

void ConfigManager::createSecurityEnvFile() {
  std::string sec_path = Path::Join(configDir_, "security.env");
  if (FileSystem::FileExists(sec_path))
    return;

  std::string content = R"(# 보안 설정
ACCESS_CONTROL_ENABLED=true
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SSL_ENABLED=false
)";

  createFileFromTemplate(sec_path, content);
}

void ConfigManager::createSecretsDirectory() {
  std::string secrets_dir = Path::Join(configDir_, "secrets");

  if (!FileSystem::DirectoryExists(secrets_dir)) {
    FileSystem::CreateDirectoryRecursive(secrets_dir);

    std::string gitignore_path = Path::Join(secrets_dir, ".gitignore");
    createFileFromTemplate(gitignore_path, "*\n!.gitignore\n!README.md\n");

    std::string readme_path = Path::Join(secrets_dir, "README.md");
    std::string readme_content = R"(# Secrets Directory

이 디렉토리는 민감한 정보를 저장합니다.
이 파일들은 절대 Git에 커밋하지 마세요!

## 파일 형식

### 평문 (기존 호환)
```
# redis_primary.key
my_plain_password
```

### 암호화 (새 방식)
```
# csp_api_key.key
ENC:base64_encoded_encrypted_value
```

SecretManager가 자동으로 감지하여 처리합니다.
)";
    createFileFromTemplate(readme_path, readme_content);

    std::vector<std::string> key_files = {// 기존 시크릿들 (평문 호환)
                                          "postgres_primary.key",
                                          "mysql.key",
                                          "redis_primary.key",
                                          "influx_token.key",
                                          "rabbitmq.key",
                                          "jwt_secret.key",
                                          "session_secret.key",
                                          "postgres_primary.key",
                                          "mysql.key",
                                          "redis_primary.key",
                                          "influx_token.key",
                                          "rabbitmq.key",
                                          "jwt_secret.key",
                                          "session_secret.key"};

    for (const auto &key_file : key_files) {
      std::string key_path = Path::Join(secrets_dir, key_file);
      createFileFromTemplate(
          key_path, "# " + key_file + " - 실제 키/비밀번호로 교체하세요\n");
    }

    LogManager::getInstance().log("config", LogLevel::INFO,
                                  "secrets/ 디렉토리 생성 (평문/암호화 호환)");
  }
}

bool ConfigManager::createFileFromTemplate(const std::string &filepath,
                                           const std::string &content) {
  try {
    if (FileSystem::FileExists(filepath)) {
      return true;
    }

    std::string dir = Path::GetDirectory(filepath);
    if (!FileSystem::DirectoryExists(dir)) {
      FileSystem::CreateDirectoryRecursive(dir);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
      return false;
    }

    file << content;
    file.close();

    LogManager::getInstance().log("config", LogLevel::INFO,
                                  "설정 파일 생성: " +
                                      Path::GetFileName(filepath));

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("config", LogLevel::WARN,
                                  "파일 생성 중 예외: " +
                                      std::string(e.what()));
    return false;
  }
}

void ConfigManager::ensureDataDirectories() {
  if (dataDir_.empty()) {
    return;
  }

  std::vector<std::string> sub_dirs = {"db", "logs", "backups", "cache",
                                       "temp"};

  for (const auto &dir : sub_dirs) {
    try {
      std::string full_path = Path::Join(dataDir_, dir);
      FileSystem::CreateDirectoryRecursive(full_path);
    } catch (const std::exception &) {
      // 실패해도 계속 진행
    }
  }
}

// =============================================================================
// 기존 설정 파일 로딩
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
  std::ifstream file(filepath);
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
  if (key == "SQLITE_DB_PATH") {
    std::string value = get("SQLITE_DB_PATH");
    if (!value.empty())
      return value;

    value = get("SQLITE_PATH");
    if (!value.empty())
      return value;

    return defaultValue;
  }

  if (key == "SQLITE_PATH") {
    std::string value = get("SQLITE_PATH");
    if (!value.empty())
      return value;

    value = get("SQLITE_DB_PATH");
    if (!value.empty())
      return value;

    return defaultValue;
  }

  std::string value = get(key);
  if (!value.empty()) {
    return value;
  }

  return defaultValue;
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

std::string ConfigManager::getDataDirectory() const {
  return dataDir_.empty() ? "./data" : dataDir_;
}

std::string ConfigManager::getSQLiteDbPath() const {
  std::string db_path = getOrDefault("SQLITE_DB_PATH", "./data/db/pulseone.db");
  std::cout << "[DEBUG] ConfigManager - SQLITE_DB_PATH from config: " << db_path
            << std::endl;

  if (db_path[0] != '/' && db_path.find(":\\") == std::string::npos) {
    std::string data_dir = getDataDirectory();
    std::cout << "[DEBUG] ConfigManager - data_dir for joining: " << data_dir
              << std::endl;
    // db_path가 상대 경로일 경우 data_dir와 결합
    // 사용자가 설정한 경로를 존중하도록 수정 (기존 하드코딩 제거)
    db_path = Path::Join(data_dir, db_path);
  }

  std::cout << "[DEBUG] ConfigManager - Final SQLite DB Path: " << db_path
            << std::endl;

  try {
    std::string dir = Path::GetDirectory(db_path);
    FileSystem::CreateDirectoryRecursive(dir);
  } catch (const std::exception &) {
    // 실패해도 경로는 반환
  }

  return db_path;
}

std::string ConfigManager::getBackupDirectory() const {
  std::string backup_dir = getOrDefault("BACKUP_DIR", "");

  if (backup_dir.empty()) {
    backup_dir = Path::Join(getDataDirectory(), "backups");
  }

  if (backup_dir[0] != '/' && backup_dir.find(":\\") == std::string::npos) {
    backup_dir = Path::Join(getDataDirectory(), backup_dir);
  }

  try {
    FileSystem::CreateDirectoryRecursive(backup_dir);
  } catch (const std::exception &) {
    // 실패해도 경로는 반환
  }

  return backup_dir;
}

std::string ConfigManager::getSecretsDirectory() const {
  return Path::Join(configDir_, "secrets");
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
  std::string result = value;

  // ${VARIABLE} 패턴 처리
  std::regex var_pattern(R"(\$\{([^}]+)\})");
  std::smatch match;

  while (std::regex_search(result, match, var_pattern)) {
    std::string var_name = match[1].str();
    std::string replacement;

    // 특별한 변수들 처리
    if (var_name == "SECRETS_DIR") {
      replacement = getSecretsDirectory();
    } else if (var_name == "DATA_DIR") {
      replacement = getDataDirectory();
    } else if (var_name == "CONFIG_DIR") {
      replacement = getConfigDirectory();
    } else if (var_name == "CSP_GATEWAY_BUILDING_ID") {
      replacement = std::to_string(getInt("CSP_GATEWAY_BUILDING_ID", 1001));
    } else {
      // 일반 설정값에서 찾기
      auto it = configMap.find(var_name);
      if (it != configMap.end()) {
        replacement = it->second;
      } else {
        // 환경변수에서 찾기
        const char *env_val = std::getenv(var_name.c_str());
        if (env_val) {
          replacement = std::string(env_val);
        } else if (var_name.compare(0, 7, "INSITE_") == 0) {
          // [HOTFIX] 환경변수 주입 지연 대응: security.env 강제 재로드
          std::string sec_path = Path::Join(configDir_, "security.env");
          if (FileSystem::FileExists(sec_path)) {
            const_cast<ConfigManager *>(this)->loadConfigFile(sec_path);
            auto it_retry = configMap.find(var_name);
            if (it_retry != configMap.end()) {
              replacement = it_retry->second;
            }
          }
        }
      }
    }

    result = result.replace(match.position(), match.length(), replacement);
  }

  return result;
}

// =============================================================================
// 신규: 설정 파일 쓰기 기능
// =============================================================================

bool ConfigManager::updateConfig(const std::string &key,
                                 const std::string &value,
                                 const std::string &config_file) {
  // 메모리에 설정값 업데이트
  set(key, value);

  // 파일 결정
  std::string target_file =
      config_file.empty() ? determineConfigFile(key) : config_file;

  std::lock_guard<std::mutex> write_lock(write_mutex_);
  dirty_files_[target_file] = true;

  // 즉시 파일에 쓰기
  return rewriteConfigFile(target_file);
}

bool ConfigManager::updateMultipleConfigs(
    const std::map<std::string, std::string> &updates,
    const std::string &config_file) {
  // 메모리에 모든 설정값 업데이트
  for (const auto &[key, value] : updates) {
    set(key, value);
  }

  std::lock_guard<std::mutex> write_lock(write_mutex_);

  if (!config_file.empty()) {
    dirty_files_[config_file] = true;
    return rewriteConfigFile(config_file);
  } else {
    // 각 키가 속한 파일별로 그룹화하여 업데이트
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

  // 기존 파일 파싱하여 구조 유지
  std::map<std::string, std::string> file_configs = parseConfigFile(file_path);

  // 현재 메모리의 설정으로 업데이트
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

    std::ofstream file(filepath);
    if (!file.is_open()) {
      return false;
    }

    // 파일별 헤더 추가
    std::string filename = Path::GetFileName(filepath);
    file << "# " << filename << " - PulseOne 설정 파일\n";

    // 현재 시간 생성
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    file << "# 자동 생성됨: "
         << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
         << "\n\n";

    // 설정값들 쓰기
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

  std::ifstream file(filepath);
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

      // 트림 처리
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
  // 키 패턴에 따라 적절한 설정 파일 결정
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

  if (key.find("CSP") != std::string::npos) {
    return "csp-gateway.env";
  }

  // 기본은 메인 설정 파일
  return ".env";
}

// =============================================================================
// 신규: SecretManager 연동 API (완전 위임)
// =============================================================================

std::string ConfigManager::getSecret(const std::string &config_key) const {
  auto secure_string = getSecretManager().getSecret(config_key);
  return secure_string.get();
}

bool ConfigManager::setSecret(const std::string &config_key,
                              const std::string &value, bool encrypt) {
  return getSecretManager().setSecret(value, "", config_key, encrypt);
}

bool ConfigManager::refreshSecret(const std::string &config_key) {
  return getSecretManager().refreshSecret(config_key);
}

SecretManager::SecretStats ConfigManager::getSecretStats() const {
  return getSecretManager().getStats();
}

// =============================================================================
// CSP Gateway 설정 구조체 메서드들 (시크릿 자동 처리)
// =============================================================================

ConfigManager::CSPAPIConfig ConfigManager::getCSPAPIConfig() const {
  return {.enabled = getBool("CSP_API_ENABLED", false),
          .endpoint = getOrDefault("CSP_API_ENDPOINT", ""),
          .api_key = getSecret("CSP_API_KEY_FILE"),
          .timeout_ms = getInt("CSP_API_TIMEOUT_MS", 10000),
          .max_retry = getInt("CSP_API_MAX_RETRY", 3),
          .retry_delay_ms = getInt("CSP_API_RETRY_DELAY_MS", 1000)};
}

ConfigManager::CSPS3Config ConfigManager::getCSPS3Config() const {
  return {
      .enabled = getBool("CSP_S3_ENABLED", false),
      .bucket_name = getOrDefault("CSP_S3_BUCKET_NAME", ""),
      .region = getOrDefault("CSP_S3_REGION", "ap-northeast-2"),
      .access_key = getSecret("CSP_S3_ACCESS_KEY_FILE"),
      .secret_key = getSecret("CSP_S3_SECRET_KEY_FILE"),
      .object_prefix = expandVariables(
          getOrDefault("CSP_S3_OBJECT_PREFIX", "alarms/{building_id}/")),
      .file_name_pattern = expandVariables(getOrDefault(
          "CSP_S3_FILE_NAME_PATTERN", "{building_id}_{timestamp}_alarm.json")),
      .timeout_ms = getInt("CSP_S3_TIMEOUT_MS", 10000),
      .max_retry = getInt("CSP_S3_MAX_RETRY", 3)};
}

ConfigManager::CSPInsiteConfig ConfigManager::getCSPInsiteConfig() const {
  return {.enabled = getBool("CSP_INSITE_ENABLED", false),
          .endpoint = getOrDefault("CSP_INSITE_ENDPOINT", ""),
          .api_key = getSecret("CSP_INSITE_API_KEY_FILE"),
          .timeout_ms = getInt("CSP_INSITE_TIMEOUT_MS", 5000),
          .control_disabled = getBool("CSP_INSITE_CONTROL_DISABLED", true),
          .control_clear_interval_hours =
              getInt("CSP_INSITE_CONTROL_CLEAR_INTERVAL_HOURS", 24)};
}

ConfigManager::CSPInbaseConfig ConfigManager::getCSPInbaseConfig() const {
  return {.enabled = getBool("CSP_INBASE_ENABLED", false),
          .endpoint = getOrDefault("CSP_INBASE_ENDPOINT", ""),
          .api_key = getSecret("CSP_INBASE_API_KEY_FILE"),
          .timeout_ms = getInt("CSP_INBASE_TIMEOUT_MS", 8000)};
}

ConfigManager::CSPBEMSConfig ConfigManager::getCSPBEMSConfig() const {
  return {.enabled = getBool("CSP_BEMS_ENABLED", false),
          .endpoint = getOrDefault("CSP_BEMS_ENDPOINT", ""),
          .api_key = getSecret("CSP_BEMS_API_KEY_FILE"),
          .building_id = getInt("CSP_BEMS_BUILDING_ID",
                                getInt("CSP_GATEWAY_BUILDING_ID", 1001)),
          .timeout_ms = getInt("CSP_BEMS_TIMEOUT_MS", 15000)};
}

ConfigManager::CSPAlarmFilter ConfigManager::getCSPAlarmFilter() const {
  return {.min_priority = getInt("CSP_ALARM_MIN_PRIORITY", 1),
          .max_priority = getInt("CSP_ALARM_MAX_PRIORITY", 4),
          .tag_include_pattern =
              getOrDefault("CSP_ALARM_TAG_INCLUDE_PATTERN", ".*"),
          .tag_exclude_pattern =
              getOrDefault("CSP_ALARM_TAG_EXCLUDE_PATTERN", "^TEST_.*"),
          .max_age_hours = getInt("CSP_ALARM_MAX_AGE_HOURS", 24)};
}

ConfigManager::CSPModbusConfig ConfigManager::getCSPModbusConfig() const {
  return {.alarm_timezone =
              getOrDefault("CSP_MODBUS_ALARM_TIMEZONE", "LOCAL_TIME"),
          .update_time_source =
              getOrDefault("CSP_MODBUS_UPDATE_TIME_SOURCE", "DATA_TIME"),
          .invalid_value_threshold = static_cast<long>(
              getInt("CSP_MODBUS_INVALID_VALUE_THRESHOLD", -2147483638)),
          .use_previous_on_invalid =
              getBool("CSP_MODBUS_USE_PREVIOUS_ON_INVALID", true)};
}

ConfigManager::CSPBatchConfig ConfigManager::getCSPBatchConfig() const {
  return {.enabled = getBool("CSP_BATCH_PROCESSING_ENABLED", true),
          .batch_size = getInt("CSP_BATCH_SIZE", 100),
          .timeout_ms = getInt("CSP_BATCH_TIMEOUT_MS", 5000),
          .flush_interval_ms = getInt("CSP_BATCH_FLUSH_INTERVAL_MS", 10000)};
}

ConfigManager::CSPFailureConfig ConfigManager::getCSPFailureConfig() const {
  return {.save_enabled = getBool("CSP_FAILED_ALARM_SAVE_ENABLED", true),
          .save_path = expandVariables(getOrDefault(
              "CSP_FAILED_ALARM_SAVE_PATH", "${DATA_DIR}/failed_alarms/")),
          .file_pattern = getOrDefault("CSP_FAILED_ALARM_FILE_PATTERN",
                                       "failed_{timestamp}_alarm.json"),
          .retry_enabled = getBool("CSP_FAILED_ALARM_RETRY_ENABLED", true),
          .retry_interval_minutes =
              getInt("CSP_FAILED_ALARM_RETRY_INTERVAL_MINUTES", 30),
          .max_retry_count = getInt("CSP_FAILED_ALARM_MAX_RETRY_COUNT", 5)};
}

ConfigManager::CSPLoggingConfig ConfigManager::getCSPLoggingConfig() const {
  return {.detailed_logging = getBool("CSP_DETAILED_LOGGING", true),
          .log_value_file_save = getBool("CSP_LOG_VALUE_FILE_SAVE", true),
          .log_update_time_error = getBool("CSP_LOG_UPDATE_TIME_ERROR", true),
          .log_network_reconnect = getBool("CSP_LOG_NETWORK_RECONNECT", true),
          .performance_monitoring = getBool("CSP_PERFORMANCE_MONITORING", true),
          .stats_update_interval_ms =
              getInt("CSP_STATS_UPDATE_INTERVAL_MS", 30000),
          .health_check_interval_ms =
              getInt("CSP_HEALTH_CHECK_INTERVAL_MS", 60000)};
}

ConfigManager::CSPNetworkConfig ConfigManager::getCSPNetworkConfig() const {
  return {.reconnect_enabled = getBool("CSP_NETWORK_RECONNECT_ENABLED", true),
          .reconnect_timeout_ms =
              getInt("CSP_NETWORK_RECONNECT_TIMEOUT_MS", 30000),
          .service_restart_on_reconnect =
              getBool("CSP_NETWORK_SERVICE_RESTART_ON_RECONNECT", true)};
}

ConfigManager::CSPSSLConfig ConfigManager::getCSPSSLConfig() const {
  return {.enabled = getBool("CSP_SSL_ENABLED", true),
          .verify_peer = getBool("CSP_SSL_VERIFY_PEER", true),
          .cert_file = getSecret("CSP_SSL_CERT_FILE"),
          .key_file = getSecret("CSP_SSL_KEY_FILE"),
          .ca_file = getSecret("CSP_SSL_CA_FILE")};
}

// =============================================================================
// 편의 메서드들 (시크릿 관리 - SecretManager에 위임)
// =============================================================================

bool ConfigManager::setAWSCredentials(const std::string &access_key,
                                      const std::string &secret_key) {
  bool success1 = setSecret("CSP_S3_ACCESS_KEY_FILE", access_key);
  bool success2 = setSecret("CSP_S3_SECRET_KEY_FILE", secret_key);
  return success1 && success2;
}

bool ConfigManager::setSSLCertificates(const std::string &cert_content,
                                       const std::string &key_content,
                                       const std::string &ca_content) {
  bool success1 = setSecret("CSP_SSL_CERT_FILE", cert_content);
  bool success2 = setSecret("CSP_SSL_KEY_FILE", key_content);
  bool success3 =
      ca_content.empty() || setSecret("CSP_SSL_CA_FILE", ca_content);
  return success1 && success2 && success3;
}

bool ConfigManager::setDatabasePassword(const std::string &db_type,
                                        const std::string &password) {
  std::string key;
  if (db_type == "POSTGRES" || db_type == "PostgreSQL") {
    key = "POSTGRES_PASSWORD_FILE";
  } else if (db_type == "MYSQL" || db_type == "MySQL") {
    key = "MYSQL_PASSWORD_FILE";
  } else if (db_type == "REDIS" || db_type == "Redis") {
    key = "REDIS_PRIMARY_PASSWORD_FILE";
  } else {
    return false;
  }

  return setSecret(key, password);
}

bool ConfigManager::setAllCSPSecrets(
    const std::map<std::string, std::string> &csp_secrets) {
  bool all_success = true;

  for (const auto &[key, value] : csp_secrets) {
    if (!setSecret(key, value)) {
      all_success = false;
      LogManager::getInstance().log("config", LogLevel::WARN,
                                    "CSP 시크릿 설정 실패: " + key);
    }
  }

  return all_success;
}