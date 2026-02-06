/**
 * @file S3Client.h
 * @brief S3 클라이언트 래퍼 - AWS S3 호환 스토리지 지원 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.0
 */

#ifndef PULSEONE_CLIENT_S3_CLIENT_H
#define PULSEONE_CLIENT_S3_CLIENT_H

#include "Client/HttpClient.h"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Client {

/**
 * @brief S3 업로드 결과
 */
struct S3UploadResult {
  bool success = false;
  std::string error_message;
  std::string file_url;
  std::string etag;
  std::chrono::system_clock::time_point upload_time;
  double upload_time_ms = 0.0;
  size_t file_size = 0;
  int status_code = 0;

  bool isSuccess() const { return success; }
};

/**
 * @brief S3 설정 구조체
 */
struct S3Config {
  std::string endpoint = "https://s3.amazonaws.com"; // S3 엔드포인트
  std::string region = "us-east-1";                  // 리전
  std::string access_key;                            // 액세스 키
  std::string secret_key;                            // 시크릿 키
  std::string bucket_name;                           // 버킷명
  std::string prefix = "";                           // 객체 키 접두사
  std::string service_name =
      "s3"; // 서명 서비스명 (기본값: s3, API Gateway 등은 execute-api)

  // 업로드 옵션
  std::string storage_class = "STANDARD";        // 스토리지 클래스
  std::string content_type = "application/json"; // 기본 Content-Type
  bool use_virtual_host_style = false;           // Virtual Host Style 사용 여부

  // 보안 옵션
  bool use_ssl = true;    // SSL 사용
  bool verify_ssl = true; // SSL 인증서 검증

  // 타임아웃 설정
  int connect_timeout_sec = 30; // 연결 타임아웃
  int upload_timeout_sec = 300; // 업로드 타임아웃 (5분)

  // 재시도 설정
  int max_retries = 3;       // 최대 재시도 횟수
  int retry_delay_ms = 1000; // 재시도 지연시간

  // 사용자 정의 헤더 (x-api-key 등)
  std::unordered_map<std::string, std::string> custom_headers;

  bool isValid() const {
    return !access_key.empty() && !secret_key.empty() && !bucket_name.empty();
  }
};

/**
 * @brief S3 객체 정보
 */
struct S3Object {
  std::string key;                                     // 객체 키
  std::string etag;                                    // ETag
  std::chrono::system_clock::time_point last_modified; // 마지막 수정 시간
  size_t size = 0;                                     // 파일 크기
  std::string storage_class;                           // 스토리지 클래스
};

/**
 * @brief S3 클라이언트 클래스 (Shared Library)
 *
 * AWS S3 API v4 Signature를 구현하여 S3 호환 스토리지에 파일 업로드
 * 용도: CSP Gateway, 데이터 백업, 로그 저장, Export 파일 업로드 등
 * - 알람 메시지를 JSON 파일로 S3에 업로드
 * - 배치 업로드 지원
 * - 재시도 로직 내장
 */
class S3Client {
public:
  /**
   * @brief 생성자
   * @param config S3 설정
   */
  explicit S3Client(const S3Config &config);

  /**
   * @brief 소멸자
   */
  ~S3Client() = default;

  // 복사/이동 생성자 비활성화
  S3Client(const S3Client &) = delete;
  S3Client &operator=(const S3Client &) = delete;
  S3Client(S3Client &&) = delete;
  S3Client &operator=(S3Client &&) = delete;

  // =======================================================================
  // 업로드 메서드들
  // =======================================================================

  /**
   * @brief 파일 업로드 (문자열 내용)
   * @param object_key 객체 키 (파일명)
   * @param content 파일 내용
   * @param content_type Content-Type (기본값: config의 content_type)
   * @param metadata 추가 메타데이터
   * @return 업로드 결과
   */
  S3UploadResult
  upload(const std::string &object_key, const std::string &content,
         const std::string &content_type = "",
         const std::unordered_map<std::string, std::string> &metadata = {});

  /**
   * @brief JSON 업로드 (범용)
   * @param object_key 객체 키
   * @param json_content JSON 문자열
   * @param metadata 추가 메타데이터
   * @return 업로드 결과
   */
  S3UploadResult
  uploadJson(const std::string &object_key, const std::string &json_content,
             const std::unordered_map<std::string, std::string> &metadata = {});

  /**
   * @brief 배치 업로드
   * @param files 업로드할 파일들 (key -> content)
   * @param content_type Content-Type
   * @param common_metadata 공통 메타데이터
   * @return 업로드 결과 리스트
   */
  std::vector<S3UploadResult> uploadBatch(
      const std::unordered_map<std::string, std::string> &files,
      const std::string &content_type = "",
      const std::unordered_map<std::string, std::string> &common_metadata = {});

  // =======================================================================
  // 조회 메서드들
  // =======================================================================

  /**
   * @brief 객체 존재 확인
   * @param object_key 객체 키
   * @return 존재하면 true
   */
  bool exists(const std::string &object_key);

  /**
   * @brief 객체 정보 조회
   * @param object_key 객체 키
   * @return 객체 정보 (실패 시 빈 옵션)
   */
  std::optional<S3Object> getObjectInfo(const std::string &object_key);

  /**
   * @brief 버킷 내 객체 목록 조회
   * @param prefix 접두사 필터
   * @param max_keys 최대 조회 개수
   * @return 객체 목록
   */
  std::vector<S3Object> listObjects(const std::string &prefix = "",
                                    int max_keys = 1000);

  // =======================================================================
  // 설정 및 유틸리티
  // =======================================================================

  /**
   * @brief 설정 업데이트
   * @param new_config 새로운 설정
   */
  void updateConfig(const S3Config &new_config);

  /**
   * @brief 현재 설정 조회
   * @return 현재 설정
   */
  const S3Config &getConfig() const { return config_; }

  /**
   * @brief 연결 테스트
   * @return 연결 성공 여부
   */
  bool testConnection();

  /**
   * @brief 타임스탬프 기반 파일명 생성
   * @param prefix 접두사 (기본값: "file")
   * @param extension 확장자 (기본값: "json")
   * @return 생성된 파일명
   */
  std::string
  generateTimestampFileName(const std::string &prefix = "file",
                            const std::string &extension = "json") const;

private:
  // =======================================================================
  // AWS Signature V4 구현
  // =======================================================================

  /**
   * @brief AWS Signature V4 생성
   * @param method HTTP 메서드
   * @param uri 요청 URI
   * @param query_params 쿼리 파라미터
   * @param headers HTTP 헤더
   * @param payload 요청 본문
   * @param timestamp 요청 시간
   * @return Authorization 헤더 값
   */
  std::string createSignatureV4(
      const std::string &method, const std::string &uri,
      const std::unordered_map<std::string, std::string> &query_params,
      const std::unordered_map<std::string, std::string> &headers,
      const std::string &payload,
      const std::chrono::system_clock::time_point &timestamp);

  /**
   * @brief Canonical Request 생성
   * @param method HTTP 메서드
   * @param uri URI
   * @param query_params 쿼리 파라미터
   * @param headers 헤더
   * @param payload 본문
   * @return Canonical Request 문자열
   */
  std::string createCanonicalRequest(
      const std::string &method, const std::string &uri,
      const std::unordered_map<std::string, std::string> &query_params,
      const std::unordered_map<std::string, std::string> &headers,
      const std::string &payload);

  /**
   * @brief String to Sign 생성
   * @param canonical_request Canonical Request
   * @param timestamp 요청 시간
   * @return String to Sign
   */
  std::string
  createStringToSign(const std::string &canonical_request,
                     const std::chrono::system_clock::time_point &timestamp);

  /**
   * @brief Signing Key 생성
   * @param timestamp 요청 시간
   * @return Signing Key
   */
  std::string
  createSigningKey(const std::chrono::system_clock::time_point &timestamp);

  // =======================================================================
  // 유틸리티 메서드들
  // =======================================================================

  /**
   * @brief URL 인코딩
   * @param str 인코딩할 문자열
   * @param encode_slash '/' 문자도 인코딩할지 여부
   * @return 인코딩된 문자열
   */
  std::string urlEncode(const std::string &str,
                        bool encode_slash = false) const;

  /**
   * @brief SHA256 해시 계산
   * @param data 해시할 데이터
   * @return SHA256 해시 (hex 문자열)
   */
  std::string sha256Hash(const std::string &data) const;

  /**
   * @brief HMAC-SHA256 계산
   * @param key 키
   * @param data 데이터
   * @return HMAC-SHA256 (hex 문자열)
   */
  std::string hmacSha256(const std::string &key, const std::string &data) const;

  /**
   * @brief HMAC-SHA256 계산 (Raw Bytes)
   * @param key 키
   * @param data 데이터
   * @return HMAC-SHA256 (Raw binary string)
   */
  std::string hmacSha256Raw(const std::string &key,
                            const std::string &data) const;

  /**
   * @brief 시간을 ISO8601 형식으로 변환
   * @param timestamp 변환할 시간
   * @return ISO8601 문자열 (YYYYMMDDTHHMMSSZ)
   */
  std::string
  formatTimestamp(const std::chrono::system_clock::time_point &timestamp) const;

  /**
   * @brief 날짜를 YYYYMMDD 형식으로 변환
   * @param timestamp 변환할 시간
   * @return 날짜 문자열
   */
  std::string
  formatDate(const std::chrono::system_clock::time_point &timestamp) const;

  /**
   * @brief S3 엔드포인트 URL 생성
   * @param object_key 객체 키
   * @return 완전한 URL
   */
  std::string buildS3Url(const std::string &object_key) const;

  /**
   * @brief 재시도 로직이 포함된 업로드 실행
   * @param object_key 객체 키
   * @param content 파일 내용
   * @param content_type Content-Type
   * @param metadata 메타데이터
   * @return 업로드 결과
   */
  S3UploadResult executeUploadWithRetry(
      const std::string &object_key, const std::string &content,
      const std::string &content_type,
      const std::unordered_map<std::string, std::string> &metadata);

  // =======================================================================
  // 멤버 변수들
  // =======================================================================

  S3Config config_;                         ///< S3 설정
  std::unique_ptr<HttpClient> http_client_; ///< HTTP 클라이언트
};

} // namespace Client
} // namespace PulseOne

#endif // PULSEONE_CLIENT_S3_CLIENT_H