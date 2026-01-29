/**
 * @file HttpClient.h
 * @brief HTTP 클라이언트 래퍼 - httplib/curl 통합 지원 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.0
 */

#ifndef PULSEONE_CLIENT_HTTP_CLIENT_H
#define PULSEONE_CLIENT_HTTP_CLIENT_H

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// HTTP 라이브러리 감지
#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
#include <httplib.h>
#endif

#if HAS_CURL
#include <curl/curl.h>
#endif

namespace PulseOne {
namespace Client {

/**
 * @brief HTTP 응답 구조체
 */
struct HttpResponse {
  int status_code = 0;
  std::string body;
  std::unordered_map<std::string, std::string> headers;
  std::string error_message;
  double response_time_ms = 0.0;

  bool isSuccess() const { return status_code >= 200 && status_code < 300; }
  bool isClientError() const { return status_code >= 400 && status_code < 500; }
  bool isServerError() const { return status_code >= 500; }
};

/**
 * @brief HTTP 요청 옵션
 */
struct HttpRequestOptions {
  int timeout_sec = 30;
  int connect_timeout_sec = 10;
  bool follow_redirects = true;
  int max_redirects = 5;
  bool verify_ssl = true;
  std::string user_agent = "PulseOne-Client/1.0";

  // 인증 정보
  std::string username;
  std::string password;
  std::string bearer_token;

  // 프록시 설정
  std::string proxy_url;
  std::string proxy_username;
  std::string proxy_password;
};

/**
 * @brief HTTP 클라이언트 래퍼 클래스 (Shared Library)
 *
 * httplib와 curl을 통합 지원하여 환경에 따라 자동 선택
 * 용도: CSP Gateway, REST API Server, 외부 서비스 연동 등
 */
class HttpClient {
public:
  /**
   * @brief 생성자
   * @param base_url 기본 URL (예: https://api.example.com)
   * @param options 요청 옵션
   */
  explicit HttpClient(const std::string &base_url = "",
                      const HttpRequestOptions &options = HttpRequestOptions{});

  /**
   * @brief 소멸자
   */
  ~HttpClient();

  // 복사/이동 생성자 비활성화
  HttpClient(const HttpClient &) = delete;
  HttpClient &operator=(const HttpClient &) = delete;
  HttpClient(HttpClient &&) = delete;
  HttpClient &operator=(HttpClient &&) = delete;

  // =======================================================================
  // HTTP 메서드들
  // =======================================================================

  /**
   * @brief GET 요청
   * @param path 요청 경로
   * @param headers 추가 헤더
   * @return HTTP 응답
   */
  HttpResponse
  get(const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {});

  /**
   * @brief POST 요청
   * @param path 요청 경로
   * @param body 요청 본문
   * @param content_type Content-Type 헤더
   * @param headers 추가 헤더
   * @return HTTP 응답
   */
  HttpResponse
  post(const std::string &path, const std::string &body,
       const std::string &content_type = "application/json",
       const std::unordered_map<std::string, std::string> &headers = {});

  /**
   * @brief PUT 요청
   * @param path 요청 경로
   * @param body 요청 본문
   * @param content_type Content-Type 헤더
   * @param headers 추가 헤더
   * @return HTTP 응답
   */
  HttpResponse
  put(const std::string &path, const std::string &body,
      const std::string &content_type = "application/json",
      const std::unordered_map<std::string, std::string> &headers = {});

  /**
   * @brief DELETE 요청
   * @param path 요청 경로
   * @param headers 추가 헤더
   * @return HTTP 응답
   */
  HttpResponse
  del(const std::string &path,
      const std::unordered_map<std::string, std::string> &headers = {});

  // =======================================================================
  // 설정 메서드들
  // =======================================================================

  /**
   * @brief 기본 URL 설정
   * @param base_url 새로운 기본 URL
   */
  void setBaseUrl(const std::string &base_url);

  /**
   * @brief 요청 옵션 업데이트
   * @param options 새로운 옵션
   */
  void setOptions(const HttpRequestOptions &options);

  /**
   * @brief 기본 헤더 설정
   * @param headers 기본으로 사용할 헤더들
   */
  void setDefaultHeaders(
      const std::unordered_map<std::string, std::string> &headers);

  /**
   * @brief Bearer 토큰 인증 설정
   * @param token Bearer 토큰
   */
  void setBearerToken(const std::string &token);

  /**
   * @brief Basic 인증 설정
   * @param username 사용자명
   * @param password 비밀번호
   */
  void setBasicAuth(const std::string &username, const std::string &password);

  // =======================================================================
  // 유틸리티 메서드들
  // =======================================================================

  /**
   * @brief 연결 테스트
   * @param test_path 테스트할 경로 (기본: "/")
   * @return 연결 성공 여부
   */
  bool testConnection(const std::string &test_path = "/");

  /**
   * @brief 현재 사용 중인 HTTP 라이브러리 확인
   * @return "httplib", "curl", 또는 "none"
   */
  std::string getHttpLibrary() const;

  /**
   * @brief SSL 인증서 검증 설정
   * @param verify true=검증, false=무시
   */
  void setSSLVerification(bool verify);

private:
  // =======================================================================
  // 내부 구현 메서드들
  // =======================================================================

  /**
   * @brief HTTP 라이브러리 초기화
   */
  void initializeHttpLibrary();

  /**
   * @brief 요청 실행 (내부 구현)
   * @param method HTTP 메서드
   * @param path 요청 경로
   * @param body 요청 본문
   * @param content_type Content-Type
   * @param headers 헤더들
   * @return HTTP 응답
   */
  HttpResponse
  executeRequest(const std::string &method, const std::string &path,
                 const std::string &body, const std::string &content_type,
                 const std::unordered_map<std::string, std::string> &headers);

#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
  /**
   * @brief httplib을 사용한 요청 실행
   */
  HttpResponse executeWithHttplib(
      const std::string &method, const std::string &path,
      const std::string &body, const std::string &content_type,
      const std::unordered_map<std::string, std::string> &headers);
#endif

#if HAS_CURL
  /**
   * @brief curl을 사용한 요청 실행
   */
  HttpResponse
  executeWithCurl(const std::string &method, const std::string &path,
                  const std::string &body, const std::string &content_type,
                  const std::unordered_map<std::string, std::string> &headers);

  /**
   * @brief curl 응답 콜백
   */
  static size_t curlWriteCallback(void *contents, size_t size, size_t nmemb,
                                  std::string *response);

  /**
   * @brief curl 헤더 콜백
   */
  static size_t
  curlHeaderCallback(char *buffer, size_t size, size_t nitems,
                     std::unordered_map<std::string, std::string> *headers);
#endif

  /**
   * @brief URL 파싱 및 검증
   * @param url 파싱할 URL
   * @return 파싱된 컴포넌트들 (host, port, path, scheme)
   */
  std::unordered_map<std::string, std::string> parseUrl(const std::string &url);

  // =======================================================================
  // 멤버 변수들
  // =======================================================================

  std::string base_url_;       ///< 기본 URL
  HttpRequestOptions options_; ///< 요청 옵션
  std::unordered_map<std::string, std::string>
      default_headers_; ///< 기본 헤더들

  // HTTP 라이브러리별 핸들들
#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
  std::unique_ptr<httplib::Client> httplib_client_;
#endif

#if HAS_CURL
  CURL *curl_handle_;
  static bool curl_global_initialized_;
  static std::mutex curl_global_mutex_;
#endif

  mutable std::mutex http_mutex_;

  enum class HttpLibraryType { NONE, HTTPLIB, CURL } library_type_;
};

} // namespace Client
} // namespace PulseOne

#endif // PULSEONE_CLIENT_HTTP_CLIENT_H