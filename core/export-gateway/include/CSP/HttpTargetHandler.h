/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS 타겟 핸들러 - Stateless 패턴 (v5.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 5.0.0 - Production-Ready 완성본
 * 저장 위치: core/export-gateway/include/CSP/HttpTargetHandler.h
 *
 * 🚀 v5.0 주요 변경:
 * - http_client_ 멤버 변수 제거 (Stateless)
 * - ClientCacheManager 사용
 * - retry_config_, auth_config_ 제거 (config에서 매번 읽음)
 * - Thread-safe 보장
 */

#ifndef HTTP_TARGET_HANDLER_H
#define HTTP_TARGET_HANDLER_H

#include "Export/ExportTypes.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

// PulseOne HttpClient 전방 선언
namespace PulseOne {
namespace Client {
class HttpClient;
struct HttpRequestOptions;
struct HttpResponse;
} // namespace Client
} // namespace PulseOne

namespace PulseOne {
namespace CSP {

/**
 * @brief 재시도 설정 구조체
 */
struct RetryConfig {
  int max_attempts = 3;
  uint32_t initial_delay_ms = 1000;
  uint32_t max_delay_ms = 30000;
  double backoff_multiplier = 2.0;
};

/**
 * @brief HTTP/HTTPS 타겟 핸들러 (Stateless v5.0)
 *
 * 특징:
 * - 상태를 가지지 않음 (http_client_ 멤버 제거)
 * - 각 sendAlarm() 호출마다 config 기반으로 클라이언트 획득
 * - ClientCacheManager로 클라이언트 재사용 (성능 최적화)
 * - initialize() 선택적 (호출 안 해도 동작)
 * - Thread-safe 보장
 */
class HttpTargetHandler : public ITargetHandler {
private:
  // ✅ 통계만 유지 (경량)
  std::atomic<size_t> request_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};

public:
  HttpTargetHandler();
  ~HttpTargetHandler() override;

  HttpTargetHandler(const HttpTargetHandler &) = delete;
  HttpTargetHandler &operator=(const HttpTargetHandler &) = delete;
  HttpTargetHandler(HttpTargetHandler &&) = delete;
  HttpTargetHandler &operator=(HttpTargetHandler &&) = delete;

  // =======================================================================
  // ITargetHandler 인터페이스 구현
  // =======================================================================

  /**
   * @brief 선택적 초기화 (설정 검증만 수행)
   */
  bool initialize(const json &config) override;

  /**
   * @brief 알람 전송 (Stateless - config 기반 동작)
   */
  TargetSendResult sendAlarm(const AlarmMessage &alarm,
                             const json &config) override;

  /**
   * @brief 주기적 데이터 배치 전송
   */
  std::vector<TargetSendResult>
  sendValueBatch(const std::vector<PulseOne::CSP::ValueMessage> &values,
                 const json &config) override;

  /**
   * @brief 연결 테스트
   */
  bool testConnection(const json &config) override;

  /**
   * @brief 핸들러 타입
   */
  std::string getHandlerType() const override { return "HTTP"; }

  /**
   * @brief 설정 검증
   */
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;

  /**
   * @brief 정리 (캐시 비우기)
   */
  void cleanup() override;

  /**
   * @brief 상태 조회
   */
  json getStatus() const override;

private:
  // =======================================================================
  // Private 핵심 메서드
  // =======================================================================

  /**
   * @brief HttpClient 가져오기 또는 생성 (캐시 사용)
   * @param config 설정
   * @param url 기본 URL
   * @return HttpClient 공유 포인터
   */
  std::shared_ptr<Client::HttpClient>
  getOrCreateClient(const json &config, const std::string &url);

  /**
   * @brief config에서 URL 추출
   */
  std::string extractUrl(const json &config) const;

  /**
   * @brief 재시도와 함께 HTTP 요청 실행
   */
  TargetSendResult executeWithRetry(const AlarmMessage &alarm,
                                    const json &config,
                                    const std::string &url);

  TargetSendResult executeWithRetry(const std::vector<ValueMessage> &values,
                                    const json &config,
                                    const std::string &url);

  /**
   * @brief 단일 HTTP 요청 실행
   */
  TargetSendResult executeSingleRequest(const AlarmMessage &alarm,
                                        const json &config,
                                        const std::string &url);

  TargetSendResult executeSingleRequest(const std::vector<ValueMessage> &values,
                                        const json &config,
                                        const std::string &url);

  /**
   * @brief 요청 헤더 생성
   */
  std::unordered_map<std::string, std::string>
  buildRequestHeaders(const json &config);

  /**
   * @brief 요청 본문 생성
   */
  std::string buildRequestBody(const AlarmMessage &alarm,
                               const json &config);

  std::string buildRequestBody(const std::vector<ValueMessage> &values,
                               const json &config);

  /**
   * @brief 백오프 지연 시간 계산
   */
  uint32_t calculateBackoffDelay(int attempt, const RetryConfig &config) const;

  /**
   * @brief 타겟 이름 추출
   */
  std::string getTargetName(const json &config) const;

  /**
   * @brief 현재 타임스탬프 (ISO 8601)
   */
  std::string getCurrentTimestamp() const;

  /**
   * @brief 요청 ID 생성
   */
  std::string generateRequestId() const;

  /**
   * @brief JSON 객체 템플릿 변수 확장
   */
  void expandTemplateVariables(nlohmann::json &template_json,
                               const AlarmMessage &alarm) const;

  void expandTemplateVariables(nlohmann::json &template_json,
                               const ValueMessage &value) const;

  /**
   * @brief Base64 인코딩
   */
  std::string base64Encode(const std::string &input) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // HTTP_TARGET_HANDLER_H