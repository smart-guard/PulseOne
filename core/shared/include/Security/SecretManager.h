#pragma once

/**
 * @file SecretManager.h
 * @brief 전용 시크릿 관리자 - ConfigManager와 완벽 조화
 * @author PulseOne Development Team
 * @date 2025-09-23
 *
 * 설계 원칙:
 * - ConfigManager: 설정 파일 읽기/쓰기 전담
 * - SecretManager: 시크릿 암/복호화 및 보안 관리 전담
 * - 두 클래스 간 느슨한 결합으로 조화
 *
 * 호환성:
 * - 기존 평문 시크릿 파일 완전 지원
 * - 새 암호화 시크릿 자동 감지 및 처리
 * - CSP Gateway 시크릿 완전 지원
 */

#include "Platform/PlatformCompat.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PulseOne {
namespace Security {

// =============================================================================
// SecureString 클래스 - 메모리 보안 강화
// =============================================================================

class SecureString {
public:
  SecureString() = default;
  explicit SecureString(const std::string &value);
  explicit SecureString(std::string &&value);

  SecureString(const SecureString &) = delete;
  SecureString &operator=(const SecureString &) = delete;
  SecureString(SecureString &&other) noexcept;
  SecureString &operator=(SecureString &&other) noexcept;

  ~SecureString();

  const std::string &get() const { return data_; }
  std::string release();
  bool empty() const { return data_.empty(); }
  size_t size() const { return data_.size(); }

  void clear();
  void secureErase();

private:
  std::string data_;
  void zeroMemory();
};

// =============================================================================
// SecretEntry 구조체 - 시크릿 메타데이터
// =============================================================================

struct SecretEntry {
  SecureString value;
  std::string file_path;
  std::chrono::system_clock::time_point last_loaded;
  std::chrono::system_clock::time_point expires_at;
  bool is_encrypted; // 암호화 여부
  bool is_cached;
  int file_permissions;

  SecretEntry() : is_encrypted(false), is_cached(false), file_permissions(0) {}

  bool isExpired() const {
    return std::chrono::system_clock::now() >= expires_at;
  }

  bool isValid() const { return !value.empty() && !isExpired(); }
};

// =============================================================================
// ConfigManagerInterface - 느슨한 결합을 위한 인터페이스
// =============================================================================

class ConfigManagerInterface {
public:
  virtual ~ConfigManagerInterface() = default;
  virtual std::string getOrDefault(const std::string &key,
                                   const std::string &defaultValue) const = 0;
  virtual std::string getSecretsDirectory() const = 0;
  virtual std::string expandVariables(const std::string &value) const = 0;
  virtual bool getBool(const std::string &key, bool defaultValue) const = 0;
};

// =============================================================================
// SecretManager 클래스 - 전용 시크릿 관리
// =============================================================================

class SecretManager {
public:
  // ==========================================================================
  // 싱글톤 패턴
  // ==========================================================================

  static SecretManager &getInstance() {
    static SecretManager instance;
    return instance;
  }

  /**
   * @brief ConfigManager 인스턴스 설정 (의존성 주입)
   */
  void setConfigManager(ConfigManagerInterface *config_manager) {
    config_manager_ = config_manager;
  }

  // ==========================================================================
  // 핵심 시크릿 관리 API
  // ==========================================================================

  /**
   * @brief 시크릿 값 조회 (자동 복호화)
   * @param secret_name 시크릿 이름 (캐시 키)
   * @param config_key 설정 키 (파일 경로 결정용, 기본: secret_name)
   * @param cache_duration_seconds 캐시 유지 시간
   * @return 복호화된 시크릿 값
   */
  SecureString getSecret(const std::string &secret_name,
                         const std::string &config_key = "",
                         int cache_duration_seconds = 3600);

  /**
   * @brief 시크릿 값 설정 (자동 암호화 후 파일 저장)
   * @param secret_name 시크릿 이름
   * @param value 평문 시크릿 값
   * @param config_key 설정 키 (파일 경로 결정용)
   * @param encrypt 암호화 여부 (기본: true)
   * @return 성공 여부
   */
  bool setSecret(const std::string &secret_name, const std::string &value,
                 const std::string &config_key = "", bool encrypt = true);

  /**
   * @brief 파일 경로에서 직접 시크릿 읽기 (자동 복호화)
   * @param file_path 파일 경로
   * @return 복호화된 시크릿 값
   */
  SecureString getSecretFromFile(const std::string &file_path) {
    return readSecretFromFile(file_path);
  }

  /**
   * @brief 캐시된 시크릿 조회 (파일 읽기 없음)
   * @param secret_name 시크릿 이름
   * @return 캐시된 시크릿 값 (없으면 빈 값)
   */
  SecureString getCachedSecret(const std::string &secret_name) const;

  /**
   * @brief 시크릿 강제 새로고침
   * @param secret_name 시크릿 이름 (빈 값이면 전체)
   * @return 성공 여부
   */
  bool refreshSecret(const std::string &secret_name = "");

  // ==========================================================================
  // CSP Gateway 전용 편의 메서드들
  // ==========================================================================

  SecureString getCSPAPIKey() {
    return getSecret("csp_api_key", "CSP_API_KEY_FILE");
  }

  SecureString getAWSAccessKey() {
    return getSecret("aws_access_key", "CSP_S3_ACCESS_KEY_FILE");
  }

  SecureString getAWSSecretKey() {
    return getSecret("aws_secret_key", "CSP_S3_SECRET_KEY_FILE");
  }

  SecureString getInsiteAPIKey() {
    return getSecret("insite_api_key", "CSP_INSITE_API_KEY_FILE");
  }

  SecureString getInbaseAPIKey() {
    return getSecret("inbase_api_key", "CSP_INBASE_API_KEY_FILE");
  }

  SecureString getBEMSAPIKey() {
    return getSecret("bems_api_key", "CSP_BEMS_API_KEY_FILE");
  }

  SecureString getJWTSecret() {
    return getSecret("jwt_secret", "JWT_SECRET_FILE");
  }

  SecureString getSessionSecret() {
    return getSecret("session_secret", "SESSION_SECRET_FILE");
  }

  // 기존 시크릿들 (평문 호환)
  SecureString getRedisPassword() {
    return getSecret("redis_primary", "REDIS_PRIMARY_PASSWORD_FILE");
  }

  SecureString getPostgresPassword() {
    return getSecret("postgres_primary", "POSTGRES_PASSWORD_FILE");
  }

  SecureString getMySQLPassword() {
    return getSecret("mysql", "MYSQL_PASSWORD_FILE");
  }

  SecureString getRabbitMQPassword() {
    return getSecret("rabbitmq", "RABBITMQ_PASSWORD_FILE");
  }

  SecureString getInfluxToken() {
    return getSecret("influx_token", "INFLUX_TOKEN_FILE");
  }

  // ==========================================================================
  // 배치 시크릿 관리
  // ==========================================================================

  /**
   * @brief 여러 시크릿 한번에 설정
   */
  bool setMultipleSecrets(
      const std::map<std::string, std::pair<std::string, std::string>>
          &secrets);

  /**
   * @brief CSP Gateway 시크릿 일괄 설정
   */
  bool setCSPSecrets(const std::string &api_key,
                     const std::string &aws_access_key,
                     const std::string &aws_secret_key);

  // ==========================================================================
  // SSL 인증서 관리
  // ==========================================================================

  struct SSLCertificates {
    std::string cert_content;
    std::string key_content;
    std::string ca_content;
  };

  /**
   * @brief SSL 인증서 조회
   */
  SSLCertificates getSSLCertificates();

  /**
   * @brief SSL 인증서 설정
   */
  bool setSSLCertificates(const SSLCertificates &certs);

  // ==========================================================================
  // 유효성 검사 및 모니터링
  // ==========================================================================

  struct SecretStats {
    int total_secrets;
    int cached_secrets;
    int expired_secrets;
    int encrypted_secrets;
    int plaintext_secrets;
    int invalid_permissions;
    std::chrono::system_clock::time_point last_cleanup;
  };

  SecretStats getStats() const;
  std::map<std::string, bool> validateAllSecrets() const;
  std::vector<std::string> getExpiredSecrets() const;

  // ==========================================================================
  // 보안 관리
  // ==========================================================================

  bool validateFilePermissions(const std::string &file_path) const;
  bool secureDirectory(const std::string &directory_path) const;
  void cleanupExpiredCache();
  void secureShutdown();

  // ==========================================================================
  // 암호화 설정
  // ==========================================================================

  /**
   * @brief 암호화 키 설정 (기본: 자동 생성)
   */
  bool setEncryptionKey(const std::string &key);

  /**
   * @brief 암호화 모드 설정 (AES, XOR 등)
   */
  enum class EncryptionMode {
    NONE = 0, // 평문
    XOR = 1,  // 간단한 XOR
    AES = 2   // AES-256 (향후 구현)
  };

  void setEncryptionMode(EncryptionMode mode) { encryption_mode_ = mode; }
  EncryptionMode getEncryptionMode() const { return encryption_mode_; }

private:
  // ==========================================================================
  // 생성자/소멸자 (싱글톤)
  // ==========================================================================

  SecretManager();
  ~SecretManager();
  SecretManager(const SecretManager &) = delete;
  SecretManager &operator=(const SecretManager &) = delete;
  SecretManager(SecretManager &&) = delete;
  SecretManager &operator=(SecretManager &&) = delete;

  // ==========================================================================
  // 내부 구현 메서드들
  // ==========================================================================

  /**
   * @brief 파일에서 시크릿 읽기 (평문/암호문 자동 감지)
   */
  SecureString readSecretFromFile(const std::string &file_path) const;

  /**
   * @brief 시크릿을 파일에 저장 (암호화 옵션)
   */
  bool writeSecretToFile(const std::string &file_path, const std::string &value,
                         bool encrypt) const;

  /**
   * @brief 시크릿 파일 경로 결정
   */
  std::string determineSecretFilePath(const std::string &secret_name,
                                      const std::string &config_key) const;

  /**
   * @brief 파일 내용이 암호화되었는지 감지
   */
  bool isEncryptedContent(const std::string &content) const;

  /**
   * @brief 암호화/복호화 (모드에 따라)
   */
  std::string encryptValue(const std::string &value) const;
  std::string decryptValue(const std::string &encrypted_value) const;

  /**
   * @brief XOR 암호화/복호화
   */
  std::string xorEncrypt(const std::string &value,
                         const std::string &key) const;
  std::string xorDecrypt(const std::string &encrypted_value,
                         const std::string &key) const;

  /**
   * @brief Base64 인코딩/디코딩
   */
  std::string base64Encode(const std::string &value) const;
  std::string base64Decode(const std::string &encoded_value) const;

  /**
   * @brief 플랫폼별 파일 권한 확인
   */
  int getFilePermissions(const std::string &file_path) const;

  /**
   * @brief 민감한 경로 마스킹 (로그용)
   */
  std::string maskSensitivePath(const std::string &file_path) const;

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  /// ConfigManager 인터페이스 (의존성 주입)
  ConfigManagerInterface *config_manager_;

  /// 시크릿 캐시 (thread-safe)
  mutable std::mutex cache_mutex_;
  std::map<std::string, SecretEntry> secret_cache_;

  /// 통계 정보
  mutable std::mutex stats_mutex_;
  SecretStats stats_;

  /// 암호화 설정
  EncryptionMode encryption_mode_;
  std::string encryption_key_;
  mutable std::mutex encryption_mutex_;

  /// 설정
  static constexpr int DEFAULT_CACHE_DURATION = 3600; // 1시간
  static constexpr size_t MAX_CACHE_SIZE = 100;
  static constexpr char DEFAULT_ENCRYPTION_KEY[] = "PulseOne2025SecretKey";
};

// =============================================================================
// 전역 편의 함수들
// =============================================================================

inline SecretManager &Secrets() { return SecretManager::getInstance(); }

inline SecureString GetSecret(const std::string &secret_name,
                              const std::string &config_key = "") {
  return SecretManager::getInstance().getSecret(secret_name, config_key);
}

inline bool SetSecret(const std::string &secret_name, const std::string &value,
                      const std::string &config_key = "", bool encrypt = true) {
  return SecretManager::getInstance().setSecret(secret_name, value, config_key,
                                                encrypt);
}

} // namespace Security
} // namespace PulseOne