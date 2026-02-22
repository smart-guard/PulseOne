/**
 * @file ConfigTemplateGenerator.cpp
 * @brief ConfigTemplateGenerator 구현부
 */

#include "Utils/ConfigTemplateGenerator.h"
#include "Platform/PlatformCompat.h"
#include <fstream>
#include <iostream>

namespace PulseOne {
namespace Utils {

using namespace PulseOne::Platform;

void ConfigTemplateGenerator::generateDefaultConfigs(
    const std::string &configDir) {
  createMainEnvFile(configDir);
  createDatabaseEnvFile(configDir);
  createRedisEnvFile(configDir);
  createTimeseriesEnvFile(configDir);
  createMessagingEnvFile(configDir);
  createSecurityEnvFile(configDir);
  createSecretsDirectory(configDir);
}

void ConfigTemplateGenerator::createMainEnvFile(const std::string &configDir) {
  std::string path = Path::Join(configDir, ".env");
  if (FileSystem::FileExists(path))
    return;

  std::string content = R"(# PulseOne Main Configuration
NODE_ENV=production
LOG_LEVEL=INFO
LOG_TO_CONSOLE=true
LOG_TO_FILE=true

# Directories (Relative to executable or absolute)
DATA_DIR=./data
LOG_DIR=./logs
CONFIG_DIR=./config
SECRETS_DIR=${CONFIG_DIR}/secrets

# Device Identification
TENANT_ID=0
COLLECTOR_ID=1
)";

  createFile(path, content);
}

void ConfigTemplateGenerator::createDatabaseEnvFile(
    const std::string &configDir) {
  std::string path = Path::Join(configDir, "database.env");
  if (FileSystem::FileExists(path))
    return;

  std::string content = R"(# Database Configuration
DATABASE_TYPE=SQLITE
SQLITE_PATH=

# Secondary Database (Optional)
SECONDARY_DATABASE_ENABLED=false
)";

  createFile(path, content);
}

void ConfigTemplateGenerator::createRedisEnvFile(const std::string &configDir) {
  std::string path = Path::Join(configDir, "redis.env");
  if (FileSystem::FileExists(path))
    return;

  std::string content = R"(# Redis 설정
REDIS_ENABLED=true
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/redis_primary.key
)";

  createFile(path, content);
}

void ConfigTemplateGenerator::createTimeseriesEnvFile(
    const std::string &configDir) {
  std::string path = Path::Join(configDir, "timeseries.env");
  if (FileSystem::FileExists(path))
    return;

  std::string content = R"(# InfluxDB 설정
INFLUX_ENABLED=true
INFLUX_HOST=localhost
INFLUX_PORT=8086
INFLUX_ORG=pulseone
INFLUX_BUCKET=telemetry_data
INFLUX_TOKEN_FILE=${SECRETS_DIR}/influx_token.key
)";

  createFile(path, content);
}

void ConfigTemplateGenerator::createMessagingEnvFile(
    const std::string &configDir) {
  std::string path = Path::Join(configDir, "messaging.env");
  if (FileSystem::FileExists(path))
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

  createFile(path, content);
}

void ConfigTemplateGenerator::createSecurityEnvFile(
    const std::string &configDir) {
  std::string path = Path::Join(configDir, "security.env");
  if (FileSystem::FileExists(path))
    return;

  std::string content = R"(# 보안 설정
ACCESS_CONTROL_ENABLED=true
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SSL_ENABLED=false
)";

  createFile(path, content);
}

void ConfigTemplateGenerator::createSecretsDirectory(
    const std::string &configDir) {
  std::string secrets_dir = Path::Join(configDir, "secrets");

  if (!FileSystem::DirectoryExists(secrets_dir)) {
    FileSystem::CreateDirectoryRecursive(secrets_dir);

    std::string gitignore_path = Path::Join(secrets_dir, ".gitignore");
    createFile(gitignore_path, "*\n!.gitignore\n!README.md\n");

    std::string readme_path = Path::Join(secrets_dir, "README.md");
    std::string readme_content = R"(# Secrets Directory

이 디렉토리는 민감한 정보를 저장합니다.
이 파일들은 절대 Git에 커밋하지 마세요!
)";
    createFile(readme_path, readme_content);
  }
}

bool ConfigTemplateGenerator::createFile(const std::string &filepath,
                                         const std::string &content) {
  try {
    std::ofstream file(filepath);
    if (!file.is_open())
      return false;
    file << content;
    file.close();
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace Utils
} // namespace PulseOne
