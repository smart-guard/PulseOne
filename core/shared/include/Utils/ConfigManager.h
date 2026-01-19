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
#include "Security/SecretManager.h"
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
class ConfigManagerInterface;
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
class ConfigManager : public PulseOne::Security::ConfigManagerInterface {
public:
  // ==========================================================================
  // 전역 싱글톤 패턴 (기존 유지)
  // ==========================================================================

  static ConfigManager &getInstance() {
    static ConfigManager instance;
    instance.ensureInitialized();
    return instance;
  }

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
                           const std::string &defaultValue) const override;
  void set(const std::string &key, const std::string &value);
  bool hasKey(const std::string &key) const;
  std::map<std::string, std::string> listAll() const;

  // 경로 관련 (기존)
  std::string getConfigDirectory() const { return configDir_; }
  std::string getDataDirectory() const;
  std::string getSQLiteDbPath() const;
  std::string getBackupDirectory() const;
  std::string getSecretsDirectory() const override;

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
  bool getBool(const std::string &key,
               bool defaultValue = false) const override;
  double getDouble(const std::string &key, double defaultValue = 0.0) const;

  // 변수 확장 (SecretManager에서 사용)
  std::string expandVariables(const std::string &value) const override;

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

  /**
   * @brief 시크릿 통계 (SecretManager에 위임)
   */
  PulseOne::Security::SecretManager::SecretStats getSecretStats() const;

  // ==========================================================================
  // CSP Gateway 전용 메서드들 (시크릿 자동 처리)
  // ==========================================================================

  bool isCSPGatewayEnabled() const {
    return getBool("CSP_GATEWAY_ENABLED", false);
  }

  std::string getInstanceKey() const;

  int getCollectorId() const {
    // 1. 환경변수 우선 확인 (Docker 등 환경에서 중요)
    const char *env_id = std::getenv("COLLECTOR_ID");
    if (env_id) {
      try {
        return std::stoi(env_id);
      } catch (...) {
      }
    }

    // 2. 설정 파일 확인
    int id = getInt("COLLECTOR_ID", 0);
    if (id <= 0) {
      // 3. 하위 호환을 위해 CSP_GATEWAY_BUILDING_ID 확인
      id = getInt("CSP_GATEWAY_BUILDING_ID", 1);
    }
    return id;
  }

  void setCollectorId(int id) { set("COLLECTOR_ID", std::to_string(id)); }

  int getCSPBuildingId() const {
    return getInt("CSP_GATEWAY_BUILDING_ID", 1001);
  }

  void setCSPBuildingId(int id) {
    set("CSP_GATEWAY_BUILDING_ID", std::to_string(id));
  }

  int getTenantId() const { return getInt("TENANT_ID", 0); }

  void setTenantId(int id) { set("TENANT_ID", std::to_string(id)); }

  std::string getCSPGatewayVersion() const {
    return getOrDefault("CSP_GATEWAY_VERSION", "1.8");
  }

  // CSP 설정 구조체들 (시크릿 자동 로드)
  struct CSPAPIConfig {
    bool enabled;
    std::string endpoint;
    std::string api_key; // SecretManager에서 자동 복호화
    int timeout_ms;
    int max_retry;
    int retry_delay_ms;
  };

  struct CSPS3Config {
    bool enabled;
    std::string bucket_name;
    std::string region;
    std::string access_key; // SecretManager에서 자동 복호화
    std::string secret_key; // SecretManager에서 자동 복호화
    std::string object_prefix;
    std::string file_name_pattern;
    int timeout_ms;
    int max_retry;
  };

  struct CSPInsiteConfig {
    bool enabled;
    std::string endpoint;
    std::string api_key; // SecretManager에서 자동 복호화
    int timeout_ms;
    bool control_disabled;
    int control_clear_interval_hours;
  };

  struct CSPInbaseConfig {
    bool enabled;
    std::string endpoint;
    std::string api_key; // SecretManager에서 자동 복호화
    int timeout_ms;
  };

  struct CSPBEMSConfig {
    bool enabled;
    std::string endpoint;
    std::string api_key; // SecretManager에서 자동 복호화
    int building_id;
    int timeout_ms;
  };

  struct CSPAlarmFilter {
    int min_priority;
    int max_priority;
    std::string tag_include_pattern;
    std::string tag_exclude_pattern;
    int max_age_hours;
  };

  struct CSPModbusConfig {
    std::string alarm_timezone;
    std::string update_time_source;
    long invalid_value_threshold;
    bool use_previous_on_invalid;
  };

  struct CSPBatchConfig {
    bool enabled;
    int batch_size;
    int timeout_ms;
    int flush_interval_ms;
  };

  struct CSPFailureConfig {
    bool save_enabled;
    std::string save_path;
    std::string file_pattern;
    bool retry_enabled;
    int retry_interval_minutes;
    int max_retry_count;
  };

  struct CSPLoggingConfig {
    bool detailed_logging;
    bool log_value_file_save;
    bool log_update_time_error;
    bool log_network_reconnect;
    bool performance_monitoring;
    int stats_update_interval_ms;
    int health_check_interval_ms;
  };

  struct CSPNetworkConfig {
    bool reconnect_enabled;
    int reconnect_timeout_ms;
    bool service_restart_on_reconnect;
  };

  struct CSPSSLConfig {
    bool enabled;
    bool verify_peer;
    std::string cert_file;
    std::string key_file;
    std::string ca_file;
  };

  // CSP 설정 조회 메서드들 (시크릿 자동 처리)
  CSPAPIConfig getCSPAPIConfig() const;
  CSPS3Config getCSPS3Config() const;
  CSPInsiteConfig getCSPInsiteConfig() const;
  CSPInbaseConfig getCSPInbaseConfig() const;
  CSPBEMSConfig getCSPBEMSConfig() const;
  CSPAlarmFilter getCSPAlarmFilter() const;
  CSPModbusConfig getCSPModbusConfig() const;
  CSPBatchConfig getCSPBatchConfig() const;
  CSPFailureConfig getCSPFailureConfig() const;
  CSPLoggingConfig getCSPLoggingConfig() const;
  CSPNetworkConfig getCSPNetworkConfig() const;
  CSPSSLConfig getCSPSSLConfig() const;

  // ==========================================================================
  // 편의 메서드들 (시크릿 관리 - SecretManager에 위임)
  // ==========================================================================

  /**
   * @brief CSP API 키 직접 설정
   */
  bool setCSPAPIKey(const std::string &api_key) {
    return setSecret("CSP_API_KEY_FILE", api_key);
  }

  /**
   * @brief AWS 자격증명 설정
   */
  bool setAWSCredentials(const std::string &access_key,
                         const std::string &secret_key);

  /**
   * @brief SSL 인증서 파일들 설정
   */
  bool setSSLCertificates(const std::string &cert_content,
                          const std::string &key_content,
                          const std::string &ca_content = "");

  /**
   * @brief 기존 데이터베이스 비밀번호들 설정 (하위 호환)
   */
  bool setDatabasePassword(const std::string &db_type,
                           const std::string &password);

  /**
   * @brief 모든 CSP 시크릿 일괄 설정
   */
  bool setAllCSPSecrets(const std::map<std::string, std::string> &csp_secrets);

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

  // 경로 탐색 (기존)
  std::string findConfigDirectory();
  std::string findDataDirectory();
  bool directoryExists(const std::string &path);
  std::string getExecutableDirectory();

  // 설정 파일 생성 (기존)
  void createMainEnvFile();
  void createDatabaseEnvFile();
  void createRedisEnvFile();
  void createTimeseriesEnvFile();
  void createMessagingEnvFile();
  void createSecurityEnvFile();
  void createCSPGatewayEnvFile();
  void createSecretsDirectory();
  bool createFileFromTemplate(const std::string &filepath,
                              const std::string &content);

  // 디렉토리 관리
  void ensureDataDirectories();
  void ensureCSPDirectories();

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
  std::string expandCSPVariables(const std::string &value) const;

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

// CSP Gateway 전용 편의 함수들
inline bool IsCSPGatewayEnabled() {
  return ConfigManager::getInstance().isCSPGatewayEnabled();
}

inline int GetCollectorId() {
  return ConfigManager::getInstance().getCollectorId();
}

inline void SetCollectorId(int id) {
  ConfigManager::getInstance().setCollectorId(id);
}

inline int GetCSPBuildingId() {
  return ConfigManager::getInstance().getCSPBuildingId();
}

inline ConfigManager::CSPAPIConfig GetCSPAPIConfig() {
  return ConfigManager::getInstance().getCSPAPIConfig();
}

inline ConfigManager::CSPS3Config GetCSPS3Config() {
  return ConfigManager::getInstance().getCSPS3Config();
}

// 신규: 시크릿 관리 편의 함수들
inline bool SetCSPAPIKey(const std::string &api_key) {
  return ConfigManager::getInstance().setCSPAPIKey(api_key);
}

inline bool SetAWSCredentials(const std::string &access_key,
                              const std::string &secret_key) {
  return ConfigManager::getInstance().setAWSCredentials(access_key, secret_key);
}