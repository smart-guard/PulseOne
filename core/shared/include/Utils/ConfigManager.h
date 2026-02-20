#pragma once

/**
 * @file ConfigManager.h
 * @brief 통합 설정 관리자 - SecretManager와 완벽 조화
 * @author PulseOne Development Team
 * @date 2025-09-23
 *
 * 설계 원칙:
 * - ConfigManager: 설정 파일 읽기/쓰기 전담
 * - SecretManager: 시크릿 암/복호화 및 보안 관리 전담
 * - 두 클래스 간 느슨한 결합과 완벽한 조화
 *
 * 호환성:
 * - 기존 ConfigManager 인터페이스 100% 유지
 * - 시크릿 관리는 SecretManager에 위임
 * - CSP Gateway 설정 완전 지원
 * - 기존 평문 시크릿 파일 완전 호환
 */

#include "Platform/PlatformCompat.h"
#include "Security/SecurityTypes.h"
#include <atomic>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Forward declaration
namespace PulseOne {
namespace Security {
class SecretManager;
} // namespace Security
} // namespace PulseOne

/**
 * @class ConfigManager
 * @brief 통합 설정 관리자 (SecretManager와 조화)
 *
 * 기존 기능:
 * - Windows/Linux 경로 처리 자동화
 * - 설정 파일 자동 탐색 및 생성/수정
 * - 환경변수 기반 유연한 설정
 * - 멀티스레드 안전성
 *
 * 신규 기능:
 * - SecretManager와 완벽한 협업
 * - 설정 파일 쓰기 기능
 * - CSP Gateway 설정 완전 지원
 */
class ConfigManager {
public:
  // ==========================================================================
  // 전역 싱글톤 패턴 (기존 유지)
  // ==========================================================================

  static ConfigManager &getInstance();

  bool isInitialized() const {
    return initialized_.load(std::memory_order_acquire);
  }

  // ==========================================================================
  // 기존 읽기 인터페이스 (100% 호환 유지)
  // ==========================================================================

  void initialize() { doInitialize(); }
  void reload();
  void load(const std::string &filepath) { loadConfigFile(filepath); }
  std::string get(const std::string &key) const;
  std::string getOrDefault(const std::string &key,
                           const std::string &defaultValue) const;
  void set(const std::string &key, const std::string &value);
  bool hasKey(const std::string &key) const;
  std::map<std::string, std::string> listAll() const;

  // 경로 관련 (기존)
  std::string getConfigDirectory() const { return configDir_; }
  std::string getDataDirectory() const;
  std::string getSQLiteDbPath() const;
  std::string getBackupDirectory() const;
  std::string getSecretsDirectory() const;

  // 파일 관리 (기존)
  std::vector<std::string> getLoadedFiles() const { return loadedFiles_; }
  void printConfigSearchLog() const;
  std::map<std::string, bool> checkAllConfigFiles() const;

  // 모듈 관리 (기존)
  std::string getActiveDatabaseType() const;
  bool isModuleEnabled(const std::string &module_name) const;

  // 기존 보안 관련 (하위 호환용)
  std::string loadPasswordFromFile(const std::string &password_file_key) const;
  void triggerVariableExpansion();

  // 편의 기능들 (기존)
  int getInt(const std::string &key, int defaultValue = 0) const;
  bool getBool(const std::string &key, bool defaultValue = false) const;
  double getDouble(const std::string &key, double defaultValue = 0.0) const;

  // 변수 확장 (SecretManager에서 사용)
  std::string expandVariables(const std::string &value) const;

  // ==========================================================================
  // 신규: 설정 파일 쓰기 기능
  // ==========================================================================

  /**
   * @brief 설정값 변경 및 파일에 저장
   * @param key 설정 키
   * @param value 설정값
   * @param config_file 저장할 설정 파일 (기본: 자동 결정)
   */
  bool updateConfig(const std::string &key, const std::string &value,
                    const std::string &config_file = "");

  /**
   * @brief 여러 설정값 한번에 변경
   * @param updates 변경할 설정들 (key -> value)
   * @param config_file 저장할 설정 파일
   */
  bool updateMultipleConfigs(const std::map<std::string, std::string> &updates,
                             const std::string &config_file = "");

  /**
   * @brief 메모리의 모든 변경사항을 파일에 저장
   */
  bool saveAllChanges();

  /**
   * @brief 특정 설정 파일 다시 쓰기
   * @param config_file 파일명 (.env, database.env 등)
   */
  bool rewriteConfigFile(const std::string &config_file);

  // ==========================================================================
  // 신규: SecretManager 연동 API
  // ==========================================================================

  /**
   * @brief 시크릿 값 조회 (SecretManager에 위임)
   * @param config_key 설정 키 (파일 경로 지정용)
   * @return 복호화된 시크릿 값
   */
  std::string getSecret(const std::string &config_key) const;

  /**
   * @brief 시크릿 값 설정 (SecretManager에 위임)
   * @param config_key 설정 키
   * @param value 평문 시크릿 값
   * @param encrypt 암호화 여부 (기본: true)
   * @return 성공 여부
   */
  bool setSecret(const std::string &config_key, const std::string &value,
                 bool encrypt = true);

  /**
   * @brief 시크릿 캐시 새로고침 (SecretManager에 위임)
   */
  bool refreshSecret(const std::string &config_key = "");

  PulseOne::Security::SecretStats getSecretStats() const;

  /**
   * @brief 암호화 모드 설정 (SecretManager에 위임)
   */
  void setEncryptionMode(PulseOne::Security::EncryptionMode mode);

  int getCollectorId() const {
    // 1. memory check
    int id = getInt("COLLECTOR_ID", 0);
    if (id > 0) {
      // LogManager might not be available here safely if not initialized,
      // but it's a member of static singleton so it should be fine.
      return id;
    }

    // 2. env check
    const char *env_id = std::getenv("COLLECTOR_ID");
    if (env_id) {
      try {
        return std::stoi(env_id);
      } catch (...) {
      }
    }

    return getInt("CSP_GATEWAY_BUILDING_ID", 1);
  }

  void setCollectorId(int id) { set("COLLECTOR_ID", std::to_string(id)); }

  void setCSPBuildingId(int id) {
    set("CSP_GATEWAY_BUILDING_ID", std::to_string(id));
  }

  int getTenantId() const { return getInt("TENANT_ID", 0); }

  void setTenantId(int id) { set("TENANT_ID", std::to_string(id)); }

  std::string getInstanceKey() const;

  // CSP Gateway 설정 조회 메서드들 - DEPRECATED 및 삭제됨

  // ==========================================================================
  // 편의 메서드들
  // ==========================================================================

  bool setDatabasePassword(const std::string &db_type,
                           const std::string &password);

private:
  // ==========================================================================
  // 생성자/소멸자 (싱글톤)
  // ==========================================================================

  ConfigManager();
  ~ConfigManager() = default;
  ConfigManager(const ConfigManager &) = delete;
  ConfigManager &operator=(const ConfigManager &) = delete;
  ConfigManager(ConfigManager &&) = delete;
  ConfigManager &operator=(ConfigManager &&) = delete;

  // ==========================================================================
  // 기존 초기화 관련
  // ==========================================================================

  void ensureInitialized();
  bool doInitialize();

  // 설정 파일 처리 (기존)
  void parseLine(const std::string &line);
  void loadMainConfig();
  void loadAdditionalConfigs();
  void loadConfigFile(const std::string &filepath);

  // 디렉토리 관리
  void ensureDataDirectories();

  // ==========================================================================
  // 신규: 파일 쓰기 관련
  // ==========================================================================

  /**
   * @brief 설정 파일에 키-값 쓰기
   */
  bool writeConfigToFile(const std::string &filepath,
                         const std::map<std::string, std::string> &config_data);

  /**
   * @brief 설정 파일 파싱해서 맵으로 반환
   */
  std::map<std::string, std::string>
  parseConfigFile(const std::string &filepath) const;

  /**
   * @brief 키가 어느 설정 파일에 속하는지 판단
   */
  std::string determineConfigFile(const std::string &key) const;

  // ==========================================================================
  // SecretManager 연동 관련
  // ==========================================================================

  /**
   * @brief SecretManager 초기화 및 의존성 주입
   */
  void initializeSecretManager();

  /**
   * @brief SecretManager 인스턴스 가져오기
   */
  PulseOne::Security::SecretManager &getSecretManager() const;

  // 변수 확장 (기존)
  void expandAllVariables();

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  // 기존 멤버들
  std::atomic<bool> initialized_;
  mutable std::recursive_mutex init_mutex_;
  std::map<std::string, std::string> configMap;
  mutable std::mutex configMutex;
  std::string envFilePath;
  std::string configDir_;
  std::string dataDir_;
  std::vector<std::string> loadedFiles_;
  std::vector<std::string> searchLog_;

  // 신규: 파일 쓰기 추적용
  mutable std::mutex write_mutex_;
  std::map<std::string, bool> dirty_files_; // 변경된 파일들 추적

  // SecretManager 연동 플래그
  mutable std::atomic<bool> secret_manager_initialized_;
};

// =============================================================================
// 전역 편의 함수들 (기존 + 신규)
// =============================================================================

inline ConfigManager &Config() { return ConfigManager::getInstance(); }

// 기존 읽기 함수들
inline std::string GetConfig(const std::string &key,
                             const std::string &defaultValue = "") {
  return ConfigManager::getInstance().getOrDefault(key, defaultValue);
}

inline int GetConfigInt(const std::string &key, int defaultValue = 0) {
  return ConfigManager::getInstance().getInt(key, defaultValue);
}

inline bool GetConfigBool(const std::string &key, bool defaultValue = false) {
  return ConfigManager::getInstance().getBool(key, defaultValue);
}

// 신규: 쓰기 함수들
inline bool SetConfig(const std::string &key, const std::string &value) {
  return ConfigManager::getInstance().updateConfig(key, value);
}

inline bool SetConfigInt(const std::string &key, int value) {
  return ConfigManager::getInstance().updateConfig(key, std::to_string(value));
}

inline bool SetConfigBool(const std::string &key, bool value) {
  return ConfigManager::getInstance().updateConfig(key,
                                                   value ? "true" : "false");
}

// 신규: 시크릿 관련 편의 함수들 (SecretManager에 위임)
inline std::string GetSecret(const std::string &config_key) {
  return ConfigManager::getInstance().getSecret(config_key);
}

inline bool SetSecret(const std::string &config_key, const std::string &value,
                      bool encrypt = true) {
  return ConfigManager::getInstance().setSecret(config_key, value, encrypt);
}

// 경로 관련 (기존)
inline std::string GetConfigDir() {
  return ConfigManager::getInstance().getConfigDirectory();
}

inline std::string GetDataDir() {
  return ConfigManager::getInstance().getDataDirectory();
}

inline std::string GetSecretsDir() {
  return ConfigManager::getInstance().getSecretsDirectory();
}

inline int GetCollectorId() {
  return ConfigManager::getInstance().getCollectorId();
}

inline void SetCollectorId(int id) {
  ConfigManager::getInstance().setCollectorId(id);
}