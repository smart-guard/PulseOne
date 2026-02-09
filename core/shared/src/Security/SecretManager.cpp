/**
 * @file SecretManager.cpp
 * @brief SecretManager 구현부 - ConfigManager와 완벽 조화
 * @author PulseOne Development Team
 * @date 2025-09-23
 */

#include "Security/SecretManager.h"
#include "Platform/PlatformCompat.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

// LogManager 조건부 포함
#ifdef HAS_SHARED_LIBS
#include "Logging/LogManager.h"
#define LOG_SECRET(level, msg)                                                 \
  LogManager::getInstance().log("secret", level, msg)
#else
#define LOG_SECRET(level, msg)                                                 \
  std::cout << "[SECRET:" << #level << "] " << msg << std::endl
#endif

using namespace PulseOne::Platform;

namespace PulseOne {
namespace Security {

// =============================================================================
// SecureString 구현
// =============================================================================

SecureString::SecureString(const std::string &value) : data_(value) {}

SecureString::SecureString(std::string &&value) : data_(std::move(value)) {}

SecureString::SecureString(SecureString &&other) noexcept
    : data_(std::move(other.data_)) {
  other.secureErase();
}

SecureString &SecureString::operator=(SecureString &&other) noexcept {
  if (this != &other) {
    secureErase();
    data_ = std::move(other.data_);
    other.secureErase();
  }
  return *this;
}

SecureString::~SecureString() { secureErase(); }

std::string SecureString::release() {
  std::string result = std::move(data_);
  data_.clear();
  return result;
}

void SecureString::clear() { secureErase(); }

void SecureString::secureErase() {
  if (!data_.empty()) {
    zeroMemory();
    data_.clear();
  }
}

void SecureString::zeroMemory() {
  volatile char *ptr = const_cast<volatile char *>(data_.data());
  std::memset(const_cast<char *>(ptr), 0, data_.size());
}

// =============================================================================
// SecretManager 구현
// =============================================================================

SecretManager::SecretManager()
    : config_manager_(nullptr), encryption_mode_(EncryptionMode::XOR),
      encryption_key_(DEFAULT_ENCRYPTION_KEY) {

  // 암호화 키 설정 (환경 변수 우선)
  const char *env_key = std::getenv("PULSEONE_SECRET_KEY");
  if (env_key && std::strlen(env_key) > 0) {
    encryption_key_ = env_key;
    LOG_SECRET(LogLevel::INFO,
               "SecretManager: 환경 변수에서 암호화 키를 로드했습니다.");
  } else {
    LOG_SECRET(LogLevel::WARN,
               "SecretManager: 기본 암호화 키를 사용 중입니다. 보안을 위해 "
               "PULSEONE_SECRET_KEY 환경 변수를 설정하십시오.");
  }

  // 통계 초기화
  stats_.total_secrets = 0;
  stats_.cached_secrets = 0;
  stats_.expired_secrets = 0;
  stats_.encrypted_secrets = 0;
  stats_.plaintext_secrets = 0;
  stats_.invalid_permissions = 0;
  stats_.last_cleanup = std::chrono::system_clock::now();

  LOG_SECRET(LogLevel::INFO, "SecretManager 초기화됨");
}

SecretManager::~SecretManager() { secureShutdown(); }

// =============================================================================
// 핵심 시크릿 관리 API
// =============================================================================

SecureString SecretManager::getSecret(const std::string &secret_name,
                                      const std::string &config_key,
                                      int cache_duration_seconds) {
  try {
    // 캐시에서 먼저 확인
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      auto it = secret_cache_.find(secret_name);
      if (it != secret_cache_.end() && it->second.isValid()) {
        LOG_SECRET(LogLevel::DEBUG, "시크릿 캐시 히트: " + secret_name);
        return SecureString(it->second.value.get());
      }
    }

    // 파일 경로 결정
    std::string file_path = determineSecretFilePath(secret_name, config_key);
    if (file_path.empty()) {
      LOG_SECRET(LogLevel::WARN, "시크릿 파일 경로 없음: " + secret_name);
      return SecureString();
    }

    // 파일 권한 검증
    if (!validateFilePermissions(file_path)) {
      LOG_SECRET(LogLevel::LOG_ERROR,
                 "시크릿 파일 권한 불안전: " + maskSensitivePath(file_path));
      return SecureString();
    }

    // 파일에서 시크릿 읽기
    auto secret_value = readSecretFromFile(file_path);
    if (secret_value.empty()) {
      LOG_SECRET(LogLevel::WARN,
                 "시크릿 파일 읽기 실패: " + maskSensitivePath(file_path));
      return SecureString();
    }

    // 캐시에 저장
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);

      SecretEntry entry;
      entry.value = SecureString(secret_value.get());
      entry.file_path = file_path;
      entry.last_loaded = std::chrono::system_clock::now();
      entry.expires_at =
          entry.last_loaded + std::chrono::seconds(cache_duration_seconds);
      entry.is_cached = true;
      entry.file_permissions = getFilePermissions(file_path);

      secret_cache_[secret_name] = std::move(entry);

      // 캐시 크기 제한
      if (secret_cache_.size() > MAX_CACHE_SIZE) {
        cleanupExpiredCache();
      }
    }

    // 통계 업데이트
    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.total_secrets = static_cast<int>(secret_cache_.size());
    }

    LOG_SECRET(LogLevel::INFO,
               "시크릿 로드 완료: " + maskSensitivePath(file_path));
    return secret_value;

  } catch (const std::exception &e) {
    LOG_SECRET(LogLevel::LOG_ERROR, "시크릿 조회 실패: " + secret_name + " - " +
                                        std::string(e.what()));
    return SecureString();
  }
}

bool SecretManager::setSecret(const std::string &secret_name,
                              const std::string &value,
                              const std::string &config_key, bool encrypt) {
  try {
    // 파일 경로 결정
    std::string file_path = determineSecretFilePath(secret_name, config_key);
    if (file_path.empty()) {
      LOG_SECRET(LogLevel::LOG_ERROR,
                 "시크릿 파일 경로 결정 실패: " + secret_name);
      return false;
    }

    // 파일에 저장
    bool success = writeSecretToFile(file_path, value, encrypt);
    if (!success) {
      LOG_SECRET(LogLevel::LOG_ERROR,
                 "시크릿 파일 저장 실패: " + maskSensitivePath(file_path));
      return false;
    }

    // 캐시 업데이트
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);

      SecretEntry entry;
      entry.value = SecureString(value);
      entry.file_path = file_path;
      entry.last_loaded = std::chrono::system_clock::now();
      entry.expires_at =
          entry.last_loaded + std::chrono::seconds(DEFAULT_CACHE_DURATION);
      entry.is_encrypted = encrypt;
      entry.is_cached = true;
      entry.file_permissions = getFilePermissions(file_path);

      secret_cache_[secret_name] = std::move(entry);
    }

    LOG_SECRET(LogLevel::INFO,
               "시크릿 설정 완료: " + maskSensitivePath(file_path) +
                   (encrypt ? " (암호화됨)" : " (평문)"));
    return true;

  } catch (const std::exception &e) {
    LOG_SECRET(LogLevel::LOG_ERROR, "시크릿 설정 실패: " + secret_name + " - " +
                                        std::string(e.what()));
    return false;
  }
}

SecureString
SecretManager::getCachedSecret(const std::string &secret_name) const {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  auto it = secret_cache_.find(secret_name);
  if (it != secret_cache_.end() && it->second.isValid()) {
    return SecureString(it->second.value.get());
  }

  return SecureString();
}

bool SecretManager::refreshSecret(const std::string &secret_name) {
  try {
    if (secret_name.empty()) {
      // 모든 캐시 새로고침
      std::lock_guard<std::mutex> lock(cache_mutex_);
      for (auto &pair : secret_cache_) {
        pair.second.value.secureErase();
      }
      secret_cache_.clear();

      LOG_SECRET(LogLevel::INFO, "모든 시크릿 캐시 새로고침");
      return true;
    } else {
      // 특정 키만 새로고침
      std::lock_guard<std::mutex> lock(cache_mutex_);
      auto it = secret_cache_.find(secret_name);
      if (it != secret_cache_.end()) {
        it->second.value.secureErase();
        secret_cache_.erase(it);
        LOG_SECRET(LogLevel::INFO, "시크릿 캐시 새로고침: " + secret_name);
      }
      return true;
    }

  } catch (const std::exception &e) {
    LOG_SECRET(LogLevel::LOG_ERROR, "시크릿 새로고침 실패: " + secret_name +
                                        " - " + std::string(e.what()));
    return false;
  }
}

std::string SecretManager::decryptEncodedValue(const std::string &value) const {
  return decryptValue(value);
}

// =============================================================================
// 배치 시크릿 관리
// =============================================================================

bool SecretManager::setMultipleSecrets(
    const std::map<std::string, std::pair<std::string, std::string>> &secrets) {
  bool all_success = true;

  for (const auto &secret_info : secrets) {
    const std::string &secret_name = secret_info.first;
    const std::string &value = secret_info.second.first;
    const std::string &config_key = secret_info.second.second;

    if (!setSecret(secret_name, value, config_key, true)) {
      all_success = false;
      LOG_SECRET(LogLevel::LOG_ERROR, "배치 시크릿 설정 실패: " + secret_name);
    }
  }

  LOG_SECRET(LogLevel::INFO,
             "배치 시크릿 설정 완료: " + std::to_string(secrets.size()) + "개");
  return all_success;
}

bool SecretManager::setCSPSecrets(const std::string &api_key,
                                  const std::string &aws_access_key,
                                  const std::string &aws_secret_key) {
  std::map<std::string, std::pair<std::string, std::string>> csp_secrets = {
      {"csp_api_key", {api_key, "CSP_API_KEY_FILE"}},
      {"aws_access_key", {aws_access_key, "CSP_S3_ACCESS_KEY_FILE"}},
      {"aws_secret_key", {aws_secret_key, "CSP_S3_SECRET_KEY_FILE"}}};

  return setMultipleSecrets(csp_secrets);
}

// =============================================================================
// SSL 인증서 관리
// =============================================================================

SecretManager::SSLCertificates SecretManager::getSSLCertificates() {
  SSLCertificates certs;

  auto cert_content = getSecret("ssl_cert", "CSP_SSL_CERT_FILE");
  auto key_content = getSecret("ssl_key", "CSP_SSL_KEY_FILE");
  auto ca_content = getSecret("ssl_ca", "CSP_SSL_CA_FILE");

  certs.cert_content = cert_content.get();
  certs.key_content = key_content.get();
  certs.ca_content = ca_content.get();

  return certs;
}

bool SecretManager::setSSLCertificates(const SSLCertificates &certs) {
  bool success = true;

  if (!certs.cert_content.empty()) {
    success &=
        setSecret("ssl_cert", certs.cert_content, "CSP_SSL_CERT_FILE", false);
  }

  if (!certs.key_content.empty()) {
    success &=
        setSecret("ssl_key", certs.key_content, "CSP_SSL_KEY_FILE", false);
  }

  if (!certs.ca_content.empty()) {
    success &= setSecret("ssl_ca", certs.ca_content, "CSP_SSL_CA_FILE", false);
  }

  return success;
}

// =============================================================================
// 유효성 검사 및 모니터링
// =============================================================================

SecretManager::SecretStats SecretManager::getStats() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  SecretStats current_stats = stats_;
  current_stats.total_secrets = static_cast<int>(secret_cache_.size());
  current_stats.cached_secrets = 0;
  current_stats.expired_secrets = 0;
  current_stats.encrypted_secrets = 0;
  current_stats.plaintext_secrets = 0;
  current_stats.invalid_permissions = 0;

  for (const auto &pair : secret_cache_) {
    if (pair.second.is_cached) {
      current_stats.cached_secrets++;
    }
    if (pair.second.isExpired()) {
      current_stats.expired_secrets++;
    }
    if (pair.second.is_encrypted) {
      current_stats.encrypted_secrets++;
    } else {
      current_stats.plaintext_secrets++;
    }
    if (!validateFilePermissions(pair.second.file_path)) {
      current_stats.invalid_permissions++;
    }
  }

  return current_stats;
}

std::map<std::string, bool> SecretManager::validateAllSecrets() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  std::map<std::string, bool> results;

  for (const auto &pair : secret_cache_) {
    bool valid =
        pair.second.isValid() && validateFilePermissions(pair.second.file_path);
    results[pair.first] = valid;
  }

  return results;
}

std::vector<std::string> SecretManager::getExpiredSecrets() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  std::vector<std::string> expired;

  for (const auto &pair : secret_cache_) {
    if (pair.second.isExpired()) {
      expired.push_back(pair.first);
    }
  }

  return expired;
}

// =============================================================================
// 보안 관리
// =============================================================================

bool SecretManager::validateFilePermissions(
    const std::string &file_path) const {
  if (!FileSystem::FileExists(file_path)) {
    return false;
  }

#ifdef _WIN32
  return true; // Windows에서는 파일 존재만 확인
#else
  struct stat file_stat;
  if (stat(file_path.c_str(), &file_stat) == 0) {
    return (file_stat.st_mode & 0777) <= 0600;
  }
  return false;
#endif
}

bool SecretManager::secureDirectory(const std::string &directory_path) const {
  if (!FileSystem::DirectoryExists(directory_path)) {
    try {
      FileSystem::CreateDirectoryRecursive(directory_path);
    } catch (const std::exception &e) {
      LOG_SECRET(LogLevel::LOG_ERROR,
                 "시크릿 디렉토리 생성 실패: " + std::string(e.what()));
      return false;
    }
  }

#ifndef _WIN32
  if (chmod(directory_path.c_str(), 0700) != 0) {
    LOG_SECRET(LogLevel::WARN,
               "시크릿 디렉토리 권한 설정 실패: " + directory_path);
    return false;
  }
#endif

  return true;
}

void SecretManager::cleanupExpiredCache() {
  // 캐시 뮤텍스가 이미 잠겨있다고 가정

  auto it = secret_cache_.begin();
  while (it != secret_cache_.end()) {
    if (it->second.isExpired()) {
      it->second.value.secureErase();
      it = secret_cache_.erase(it);
    } else {
      ++it;
    }
  }

  LOG_SECRET(LogLevel::DEBUG, "만료된 시크릿 캐시 정리 완료");
}

void SecretManager::secureShutdown() {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  LOG_SECRET(LogLevel::INFO, "SecretManager 보안 종료 시작...");

  for (auto &pair : secret_cache_) {
    pair.second.value.secureErase();
  }
  secret_cache_.clear();

  // 암호화 키도 제거
  {
    std::lock_guard<std::mutex> enc_lock(encryption_mutex_);
    std::fill(encryption_key_.begin(), encryption_key_.end(), 0);
    encryption_key_.clear();
  }

  LOG_SECRET(LogLevel::INFO, "SecretManager 보안 종료 완료");
}

// =============================================================================
// 암호화 설정
// =============================================================================

bool SecretManager::setEncryptionKey(const std::string &key) {
  if (key.empty()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(encryption_mutex_);
  encryption_key_ = key;

  LOG_SECRET(LogLevel::INFO, "암호화 키 설정됨");
  return true;
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

SecureString
SecretManager::readSecretFromFile(const std::string &file_path) const {
  try {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
      return SecureString();
    }

    // 파일 크기 제한 (1MB)
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    if (file_size > 1024 * 1024) {
      LOG_SECRET(LogLevel::LOG_ERROR,
                 "시크릿 파일 크기 초과: " + maskSensitivePath(file_path));
      return SecureString();
    }

    file.seekg(0, std::ios::beg);

    std::string content;
    std::string line;

    // 첫 번째 비어있지 않은 줄 읽기
    while (std::getline(file, line)) {
      // 공백 제거
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);

      // 주석이나 빈 줄 무시
      if (!line.empty() && line[0] != '#') {
        content = line;
        break;
      }
    }

    file.close();

    if (content.empty()) {
      LOG_SECRET(LogLevel::WARN,
                 "시크릿 파일이 비어있음: " + maskSensitivePath(file_path));
      return SecureString();
    }

    // 암호화 여부 확인 및 복호화
    if (isEncryptedContent(content)) {
      std::string decrypted = decryptValue(content);
      return SecureString(std::move(decrypted));
    } else {
      return SecureString(std::move(content));
    }

  } catch (const std::exception &e) {
    LOG_SECRET(LogLevel::LOG_ERROR,
               "시크릿 파일 읽기 예외: " + std::string(e.what()));
    return SecureString();
  }
}

bool SecretManager::writeSecretToFile(const std::string &file_path,
                                      const std::string &value,
                                      bool encrypt) const {
  try {
    // 디렉토리 생성
    std::string dir = Path::GetDirectory(file_path);
    if (!FileSystem::DirectoryExists(dir)) {
      FileSystem::CreateDirectoryRecursive(dir);
    }

    std::ofstream file(file_path);
    if (!file.is_open()) {
      return false;
    }

    // 파일 헤더
    file << "# PulseOne 시크릿 파일\n";
    auto now = std::time(nullptr);
    file << "# 생성시간: "
         << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "\n";
    file << "# 암호화: " << (encrypt ? "예" : "아니오") << "\n";

    // 내용 저장
    if (encrypt) {
      std::string encrypted = encryptValue(value);
      file << encrypted << "\n";
    } else {
      file << value << "\n";
    }

    file.close();

    // 파일 권한 설정 (Unix 시스템)
#ifndef _WIN32
    chmod(file_path.c_str(), 0600);
#endif

    LOG_SECRET(LogLevel::INFO,
               "시크릿 파일 저장: " + maskSensitivePath(file_path) +
                   (encrypt ? " (암호화됨)" : " (평문)"));

    return true;

  } catch (const std::exception &e) {
    LOG_SECRET(LogLevel::LOG_ERROR,
               "시크릿 파일 저장 실패: " + maskSensitivePath(file_path) +
                   " - " + std::string(e.what()));
    return false;
  }
}

std::string
SecretManager::determineSecretFilePath(const std::string &secret_name,
                                       const std::string &config_key) const {
  if (!config_manager_) {
    // ConfigManager가 없으면 기본 경로 생성
    std::string filename = secret_name + ".key";
    return "./config/secrets/" + filename;
  }

  std::string actual_config_key =
      config_key.empty() ? secret_name + "_FILE" : config_key;

  // ConfigManager에서 경로 조회
  std::string config_path =
      config_manager_->getOrDefault(actual_config_key, "");

  if (!config_path.empty()) {
    // 변수 확장 적용
    return config_manager_->expandVariables(config_path);
  }

  // 기본 경로 생성
  std::string filename = secret_name;
  std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
  filename += ".key";

  return Path::Join(config_manager_->getSecretsDirectory(), filename);
}

bool SecretManager::isEncryptedContent(const std::string &content) const {
  // ENC: 접두사로 암호화 여부 판단
  return content.find("ENC:") == 0;
}

std::string SecretManager::encryptValue(const std::string &value) const {
  std::lock_guard<std::mutex> lock(encryption_mutex_);

  switch (encryption_mode_) {
  case EncryptionMode::NONE:
    return value;

  case EncryptionMode::XOR: {
    std::string encrypted = xorEncrypt(value, encryption_key_);
    std::string encoded = base64Encode(encrypted);
    return "ENC:" + encoded;
  }

  case EncryptionMode::AES:
    // TODO: AES 구현
    LOG_SECRET(LogLevel::WARN, "AES 암호화는 아직 구현되지 않음. XOR 사용.");
    return encryptValue(value); // XOR로 폴백

  default:
    return value;
  }
}

std::string
SecretManager::decryptValue(const std::string &encrypted_value) const {
  if (!isEncryptedContent(encrypted_value)) {
    return encrypted_value; // 암호화되지 않은 평문
  }

  std::string payload = encrypted_value.substr(4); // "ENC:" 제거
  std::string decrypted;

  {
    std::lock_guard<std::mutex> lock(encryption_mutex_);

    switch (encryption_mode_) {
    case EncryptionMode::NONE:
      decrypted = payload;
      break;

    case EncryptionMode::XOR: {
      std::string decoded = base64Decode(payload);
      decrypted = xorDecrypt(decoded, encryption_key_);
      break;
    }

    case EncryptionMode::AES:
      // TODO: AES 구현
      LOG_SECRET(LogLevel::WARN, "AES 복호화는 아직 구현되지 않음. XOR 사용.");
      // Recursive call with lock released or just do XOR logic here.
      // For simplicity and matching Node.js, we assume XOR.
      {
        std::string decoded = base64Decode(payload);
        decrypted = xorDecrypt(decoded, encryption_key_);
      }
      break;

    default:
      decrypted = payload;
      break;
    }
  }

  // [FIX] Salted format 지원 (8 hex chars + ':')
  // Salted format: [8 hex chars]:[actual secret]
  if (decrypted.length() > 9 && decrypted[8] == ':') {
    std::string salt = decrypted.substr(0, 8);

    // Salt가 16진수인지 확인
    bool is_hex = true;
    for (char c : salt) {
      if (!std::isxdigit(static_cast<unsigned char>(c))) {
        is_hex = false;
        break;
      }
    }

    if (is_hex) {
      std::string result = decrypted.substr(9);
      // ✅ [Harden] 디코딩 후 보이지 않는 공백/제어문자 제거
      // (InvalidAccessKeyId 방지)
      result.erase(result.find_last_not_of(" \t\r\n") + 1);
      return result;
    }
  }

  // ✅ [Harden] 일반 복호화 결과도 트리밍
  std::string result = decrypted;
  result.erase(result.find_last_not_of(" \t\r\n") + 1);
  return result;
}

std::string SecretManager::xorEncrypt(const std::string &value,
                                      const std::string &key) const {
  if (key.empty()) {
    return value;
  }

  std::string result = value;
  size_t key_len = key.length();

  for (size_t i = 0; i < result.length(); ++i) {
    result[i] ^= key[i % key_len];
  }

  return result;
}

std::string SecretManager::xorDecrypt(const std::string &encrypted_value,
                                      const std::string &key) const {
  // XOR은 대칭 연산이므로 암호화와 동일
  return xorEncrypt(encrypted_value, key);
}

std::string SecretManager::base64Encode(const std::string &value) const {
  // 간단한 Base64 인코딩 구현
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;

  int val = 0, valb = -6;
  for (unsigned char c : value) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      result.push_back(chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (result.size() % 4) {
    result.push_back('=');
  }

  return result;
}

std::string
SecretManager::base64Decode(const std::string &encoded_value) const {
  // 간단한 Base64 디코딩 구현
  static const int T[128] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
      -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
      -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

  std::string result;
  int val = 0, valb = -8;

  for (unsigned char c : encoded_value) {
    if (T[c] == -1)
      break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      result.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }

  return result;
}

int SecretManager::getFilePermissions(const std::string &file_path) const {
#ifdef _WIN32
  return FileSystem::FileExists(file_path) ? 0 : -1;
#else
  struct stat file_stat;
  if (stat(file_path.c_str(), &file_stat) == 0) {
    return file_stat.st_mode;
  }
  return -1;
#endif
}

std::string
SecretManager::maskSensitivePath(const std::string &file_path) const {
  // 민감한 경로 부분 마스킹
  auto pos = file_path.find_last_of("/\\");
  if (pos != std::string::npos && pos < file_path.length() - 1) {
    std::string dir = file_path.substr(0, pos + 1);
    std::string filename = file_path.substr(pos + 1);

    if (filename.length() > 3) {
      return dir + filename.substr(0, 2) + "***" +
             filename.substr(filename.length() - 1);
    }
  }

  return "***";
}

std::string
SecretManager::maskSensitiveJson(const std::string &json_str) const {
  if (json_str.empty())
    return json_str;

  std::string masked = json_str;
  // 민감한 키 목록 (JSON 키 형식: "key":)
  std::vector<std::string> sensitive_keys = {
      "\"apiKey\":",          "\"password\":",   "\"token\":",
      "\"access_key\":",      "\"secret_key\":", "\"AccessKeyID\":",
      "\"SecretAccessKey\":", "\"x-api-key\":",  "\"Authorization\":"};

  for (const auto &key : sensitive_keys) {
    size_t pos = 0;
    while ((pos = masked.find(key, pos)) != std::string::npos) {
      // 키 다음의 값 시작 위치 찾기
      size_t value_start = masked.find(':', pos);
      if (value_start == std::string::npos)
        break;
      value_start++; // : 다음으로

      // 공백 무시
      while (value_start < masked.length() &&
             std::isspace(static_cast<unsigned char>(masked[value_start]))) {
        value_start++;
      }

      // 값이 따옴표로 시작하는지 확인
      if (value_start < masked.length() && masked[value_start] == '\"') {
        size_t start_quote = value_start;
        size_t end_quote = masked.find('\"', start_quote + 1);
        if (end_quote != std::string::npos) {
          std::string raw_value =
              masked.substr(start_quote + 1, end_quote - start_quote - 1);
          std::string replacement = maskValue(raw_value);
          masked.replace(start_quote + 1, end_quote - start_quote - 1,
                         replacement);
          pos = start_quote + replacement.length() + 2;
        } else {
          break;
        }
      } else {
        // 따옴표가 아닌 경우 (숫자나 불리언 등 - 드묾)
        size_t end_val = masked.find_first_of(",}", value_start);
        if (end_val == std::string::npos)
          end_val = masked.length();
        masked.replace(value_start, end_val - value_start, "\"***\"");
        pos = value_start + 5;
      }
    }
  }

  return masked;
}

std::string SecretManager::maskValue(const std::string &value) const {
  if (value.length() <= 4)
    return "****";
  return value.substr(0, 2) + "..." + value.substr(value.length() - 2);
}

} // namespace Security
} // namespace PulseOne