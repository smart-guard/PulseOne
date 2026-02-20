#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace PulseOne {
namespace Security {

/**
 * @brief 암호화 모드 설정
 */
enum class EncryptionMode {
  NONE = 0, // 평문
  XOR = 1,  // 간단한 XOR
  AES = 2   // AES-256
};

/**
 * @brief 시크릿 통계 정보
 */
struct SecretStats {
  int total_secrets;
  int cached_secrets;
  int expired_secrets;
  int encrypted_secrets;
  int plaintext_secrets;
  int invalid_permissions;
  std::chrono::system_clock::time_point last_cleanup;
};

/**
 * @brief SSL 인증서 구조체
 */
struct SSLCertificates {
  std::string cert_content;
  std::string key_content;
  std::string ca_content;
};

/**
 * @brief ExpansionProvider - 변수 치환을 위한 콜백 함수 정의
 * (Utils 도메인에 의존하지 않기 위해 std::function 사용)
 */
using ExpansionProvider = std::function<std::string(const std::string &)>;

} // namespace Security
} // namespace PulseOne
