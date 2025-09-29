/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS 타겟 핸들러 - 완성본 (include 정리 + 전체 메서드)
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 3.0.0 (CSPDynamicTargets.h 단일 include 사용)
 * 저장 위치: core/export-gateway/include/CSP/HttpTargetHandler.h
 */

#ifndef HTTP_TARGET_HANDLER_H
#define HTTP_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"  // 모든 타입이 여기 정의됨 (단일 include)
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

// PulseOne HttpClient 전방 선언
namespace PulseOne {
namespace Client {
    class HttpClient;
    struct HttpRequestOptions;
    struct HttpResponse;
}
}

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
 * @brief 인증 설정 구조체
 */
struct AuthConfig {
    std::string type = "none";              // "none", "bearer", "basic", "api_key"
    std::string bearer_token;
    std::string basic_username;
    std::string basic_password;
    std::string api_key;
    std::string api_key_header = "X-API-Key";
};

/**
 * @brief HTTP/HTTPS 타겟 핸들러
 * 
 * 주요 기능:
 * - REST API 호출 (GET, POST, PUT, PATCH)
 * - CSP API 전송 (C# 버전 완전 호환)
 * - 웹훅 전송
 * - Bearer Token 인증, Basic Auth 인증, API Key 인증
 * - 커스텀 헤더 지원
 * - SSL/TLS 인증서 검증
 * - 재시도 로직 (지수 백오프)
 * - 요청/응답 압축, 타임아웃 설정
 */
class HttpTargetHandler : public ITargetHandler {  // ITargetHandler는 CSPDynamicTargets.h에서 정의됨
private:
    mutable std::mutex client_mutex_;
    std::atomic<size_t> request_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    
    // HTTP 클라이언트 및 설정
    std::unique_ptr<PulseOne::Client::HttpClient> http_client_;
    RetryConfig retry_config_;
    AuthConfig auth_config_;
    
public:
    HttpTargetHandler();
    ~HttpTargetHandler() override;
    
    // 복사/이동 생성자 비활성화
    HttpTargetHandler(const HttpTargetHandler&) = delete;
    HttpTargetHandler& operator=(const HttpTargetHandler&) = delete;
    HttpTargetHandler(HttpTargetHandler&&) = delete;
    HttpTargetHandler& operator=(HttpTargetHandler&&) = delete;
    
    // =======================================================================
    // ITargetHandler 인터페이스 구현 (CSPDynamicTargets.h에서 정의됨)
    // =======================================================================
    
    /**
     * @brief 핸들러 초기화
     * @param config JSON 설정 객체
     * @return 초기화 성공 여부
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송 (HTTP 요청)
     * @param alarm 전송할 알람 메시지
     * @param config 타겟별 설정
     * @return 전송 결과 (CSPDynamicTargets.h의 TargetSendResult 사용)
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 테스트 성공 여부
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 반환
     */
    std::string getHandlerType() const override { return "HTTP"; }
    
    /**
     * @brief 설정 유효성 검증
     */
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;
    
    /**
     * @brief 정리 작업
     */
    void cleanup() override;
    
    /**
     * @brief 상태 정보 반환
     */
    json getStatus() const override;

    // =======================================================================
    // HTTP 특화 공개 메서드들
    // =======================================================================
    
    /**
     * @brief 통계 정보 반환
     */
    json getStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStatistics();

private:
    // =======================================================================
    // 내부 구현 메서드들 (CSPDynamicTargets.h의 TargetSendResult 사용)
    // =======================================================================
    
    /**
     * @brief 재시도와 함께 HTTP 요청 실행
     * @return CSPDynamicTargets.h의 TargetSendResult 사용
     */
    TargetSendResult executeWithRetry(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 단일 HTTP 요청 실행
     * @return CSPDynamicTargets.h의 TargetSendResult 사용
     */
    TargetSendResult executeSingleRequest(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 요청 헤더 생성
     */
    std::unordered_map<std::string, std::string> buildRequestHeaders(const json& config);
    
    /**
     * @brief 요청 본문 생성
     */
    std::string buildRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 인증 헤더 추가
     */
    void addAuthHeaders(std::unordered_map<std::string, std::string>& headers, const json& config);
    
    /**
     * @brief URL 파라미터 확장
     */
    std::string expandUrlParameters(const std::string& url, const AlarmMessage& alarm);
    
    /**
     * @brief JSON 템플릿 확장
     */
    std::string expandJsonTemplate(const std::string& template_str, const AlarmMessage& alarm);
    
    /**
     * @brief HTTP 메서드 검증
     */
    bool isValidHttpMethod(const std::string& method);
    
    /**
     * @brief Content-Type 검증
     */
    bool isValidContentType(const std::string& content_type);
    
    /**
     * @brief 백오프 지연 시간 계산
     */
    uint32_t calculateBackoffDelay(int attempt) const;
    
    /**
     * @brief 타겟 이름 추출
     */
    std::string getTargetName(const json& config) const;
    
    /**
     * @brief URL 유효성 검증
     */
    bool isValidUrl(const std::string& url) const;
    
    /**
     * @brief CSP API 특화 요청 본문 생성
     */
    std::string buildCSPRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 웹훅 특화 요청 본문 생성
     */
    std::string buildWebhookRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 커스텀 JSON 템플릿 처리
     */
    std::string processCustomJsonTemplate(const json& template_obj, const AlarmMessage& alarm);
    
    /**
     * @brief 응답 검증
     */
    bool validateResponse(const PulseOne::Client::HttpResponse& response, const json& config);
    
    /**
     * @brief 에러 응답 처리
     */
    std::string extractErrorMessage(const PulseOne::Client::HttpResponse& response);
    
    /**
     * @brief HTTP 클라이언트 옵션 구성
     */
    PulseOne::Client::HttpRequestOptions buildRequestOptions(const json& config);
    
    /**
     * @brief 압축 처리
     */
    std::string compressContent(const std::string& content, const std::string& compression_type);
    
    /**
     * @brief 응답 압축 해제
     */
    std::string decompressResponse(const std::string& content, const std::string& encoding);
    
    /**
     * @brief 쿠키 처리
     */
    void processCookies(const PulseOne::Client::HttpResponse& response, const json& config);
    
    /**
     * @brief 리다이렉트 처리
     */
    bool shouldFollowRedirect(int status_code, const json& config);
    
    /**
     * @brief 변수 치환 ({{variable}} 형태)
     */
    std::string replaceVariables(const std::string& text, const AlarmMessage& alarm);
    
    /**
     * @brief 날짜/시간 포맷팅
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp, 
                               const std::string& format = "ISO8601");
    
    /**
     * @brief 알람 심각도 문자열 변환
     */
    std::string severityToString(int severity);
    
    /**
     * @brief URL 인코딩
     */
    std::string urlEncode(const std::string& value);
    
    /**
     * @brief Base64 인코딩 (Basic Auth용)
     */
    std::string base64Encode(const std::string& input);
    
    /**
     * @brief JWT 토큰 검증 (Bearer Auth용)
     */
    bool isValidJwtToken(const std::string& token);
    
    /**
     * @brief 통계 업데이트
     */
    void updateStatistics(bool success, std::chrono::milliseconds response_time, size_t content_size);
    
    /**
     * @brief 로그 메시지 생성
     */
    std::string buildLogMessage(const std::string& level, const std::string& message, 
                               const std::string& target_name = "");
};

} // namespace CSP
} // namespace PulseOne

#endif // HTTP_TARGET_HANDLER_H