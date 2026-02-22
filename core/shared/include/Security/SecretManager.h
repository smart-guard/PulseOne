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
#include "Security/SecurityTypes.h"
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

  /**
   * @brief ENC: 접두사가 있는 암호화된 문자열을 복호화 (없으면 그대로 반환)
   * @param value 잠재적 암호화 문자열
   * @return 복호화된 문자열
   */
  std::string decryptEncodedValue(const std::string &value) const;

  /**
   * @brief config 값을 완전 해석: ${VAR} 변수 조회 + ENC: 복호화를 단일 호출로.
   *        - "${VAR_NAME}" → ConfigManager::expandVariables() → 값 조회
   *        - "ENC:..."     → XOR 복호화
   *        - 그 외        → 그대로 반환
   * @param config_value config JSON에서 읽은 원시 값
   * @return 복호화된 평문 문자열
   */
  std::string resolve(const std::string &config_value) const;

  /**
   * @brief JSON 문자열 내의 민감한 정보를 마스킹 (로그용)
   */
  std::string maskSensitiveJson(const std::string &json_str) const;

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

  // ==========================================================================
  // SSL 인증서 관리
  // ==========================================================================

  // SSLCertificates is now in SecurityTypes.h

  /**
   * @brief SSL 인증서 조회
   */
  ::PulseOne::Security::SSLCertificates getSSLCertificates();

  /**
   * @brief SSL 인증서 설정
   */
  bool setSSLCertificates(const ::PulseOne::Security::SSLCertificates &certs);

  // ==========================================================================
  // 유효성 검사 및 모니터링
  // ==========================================================================

  ::PulseOne::Security::SecretStats getStats() const;
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
  void setEncryptionMode(::PulseOne::Security::EncryptionMode mode) {
    encryption_mode_ = mode;
  }
  ::PulseOne::Security::EncryptionMode getEncryptionMode() const {
    return encryption_mode_;
  }

  /**
   * @brief 시크릿 관리자 초기화 (Passive Mode)
   * @param secrets_dir 시크릿 파일들이 위치한 디렉토리
   * @param expander 변수 치환을 위한 프로바이더
   */
  void initialize(const std::string &secrets_dir,
                  ::PulseOne::Security::ExpansionProvider expander);

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

  /**
   * @brief 민감한 키/값 마스킹 (로그용)
   */
  std::string maskValue(const std::string &value) const;

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  /// 시크릿 캐시 (thread-safe)
  mutable std::mutex cache_mutex_;
  std::map<std::string, SecretEntry> secret_cache_;

  /// 통계 정보
  mutable std::mutex stats_mutex_;
  ::PulseOne::Security::SecretStats stats_;

  /// 암호화 설정
  ::PulseOne::Security::EncryptionMode encryption_mode_;
  std::string encryption_key_;
  mutable std::mutex encryption_mutex_;

  /// 설정
  static constexpr int DEFAULT_CACHE_DURATION = 3600; // 1시간
  static constexpr size_t MAX_CACHE_SIZE = 100;
  static constexpr char DEFAULT_ENCRYPTION_KEY[] =
      "PulseOne_Secure_Vault_Key_2026_v3.2.0_Unified";

  /// 주입받은 설정
  std::string _secretsDir;
  ::PulseOne::Security::ExpansionProvider _expander = nullptr;
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