/**
 * @file ConfigSecrets.cpp
 * @brief ConfigManager의 SecretManager 연동 및 위임 로직 구현부
 *
 * 담당 역할:
 * - SecretManager 초기화 및 의존성(Interface) 주입
 * - 시크릿 조회/설정 API 위임
 */

#include "Logging/LogManager.h"
#include "Security/SecretManager.h"
#include "Utils/ConfigManager.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace PulseOne::Security;

// =============================================================================
// SecretManager 연동 로직
// =============================================================================

void ConfigManager::initializeSecretManager() {
  if (secret_manager_initialized_.load()) {
    return;
  }

  try {
    auto &secret_manager = SecretManager::getInstance();

    // 1. 의존성 역전: 데이터 및 치환기 주입 (Pure Passive Mode)
    secret_manager.initialize(
        getSecretsDirectory(),
        [this](const std::string &val) { return getOrDefault(val, ""); });

    // 2. 암호화 모드 및 키 설정
    std::string encryption_mode = getOrDefault("SECRET_ENCRYPTION_MODE", "XOR");
    if (encryption_mode == "NONE") {
      secret_manager.setEncryptionMode(EncryptionMode::NONE);
    } else if (encryption_mode == "AES") {
      secret_manager.setEncryptionMode(EncryptionMode::AES);
    } else {
      secret_manager.setEncryptionMode(EncryptionMode::XOR);
    }

    // 커스텀 암호화 키 설정 (있다면)
    std::string custom_key = getOrDefault("SECRET_ENCRYPTION_KEY", "");
    if (!custom_key.empty()) {
      secret_manager.setEncryptionKey(custom_key);
    }

    secret_manager_initialized_.store(true);

    LogManager::getInstance().log("config", LogLevel::INFO,
                                  "SecretManager 연동 완료 (Passive Mode)");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("config", LogLevel::LOG_ERROR,
                                  "SecretManager 초기화 실패: " +
                                      std::string(e.what()));
  }
}

PulseOne::Security::SecretManager &ConfigManager::getSecretManager() const {
  if (!secret_manager_initialized_.load()) {
    const_cast<ConfigManager *>(this)->initializeSecretManager();
  }
  return SecretManager::getInstance();
}

void ConfigManager::setEncryptionMode(EncryptionMode mode) {
  auto &secret_manager = SecretManager::getInstance();
  secret_manager.setEncryptionMode(mode);
}

// =============================================================================
// 시크릿 관련 위임 API
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

PulseOne::Security::SecretStats ConfigManager::getSecretStats() const {
  return SecretManager::getInstance().getStats();
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
