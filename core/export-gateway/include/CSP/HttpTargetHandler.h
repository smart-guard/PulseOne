/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS 타겟 핸들러 - REST API, 웹훅, CSP API 전송
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/HttpTargetHandler.h
 */

#ifndef HTTP_TARGET_HANDLER_H
#define HTTP_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"
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
 * @brief HTTP/HTTPS 타겟 핸들러
 * 
 * 지원 기능:
 * - REST API 호출 (GET, POST, PUT, PATCH)
 * - CSP API 전송 (C# 버전 완전 호환)
 * - 웹훅 전송
 * - Bearer Token 인증
 * - Basic Auth 인증
 * - API Key 인증 (헤더/쿼리)
 * - 커스텀 헤더 지원
 * - SSL/TLS 인증서 검증
 * - 재시도 로직 (지수 백오프)
 * - 요청/응답 압축
 * - 타임아웃 설정
 */
class HttpTargetHandler : public ITargetHandler {
private:
    mutable std::mutex client_mutex_;  // HTTP 클라이언트 보호용
    std::atomic<size_t> request_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    
public:
    /**
     * @brief 생성자
     */
    HttpTargetHandler();
    
    /**
     * @brief 소멸자
     */
    ~HttpTargetHandler() override;
    
    // 복사/이동 생성자 비활성화
    HttpTargetHandler(const HttpTargetHandler&) = delete;
    HttpTargetHandler& operator=(const HttpTargetHandler&) = delete;
    HttpTargetHandler(HttpTargetHandler&&) = delete;
    HttpTargetHandler& operator=(HttpTargetHandler&&) = delete;
    
    /**
     * @brief 핸들러 초기화
     * @param config JSON 설정 객체
     * @return 초기화 성공 여부
     * 
     * 필수 설정:
     * - endpoint: HTTP(S) 엔드포인트 URL
     * 
     * 선택 설정:
     * - method: HTTP 메서드 (기본: POST)
     * - content_type: Content-Type 헤더 (기본: application/json)
     * - auth_type: 인증 타입 (bearer, basic, api_key)
     * - auth_key_file: 인증 키 파일 경로
     * - username: Basic Auth 사용자명
     * - password_file: Basic Auth 암호 파일
     * - api_key_header: API Key 헤더명 (기본: X-API-Key)
     * - headers: 추가 HTTP 헤더 (객체)
     * - timeout_ms: 요청 타임아웃 (기본: 10000ms)
     * - connect_timeout_ms: 연결 타임아웃 (기본: 5000ms)
     * - max_retry: 최대 재시도 횟수 (기본: 3)
     * - retry_delay_ms: 재시도 간격 (기본: 1000ms)
     * - retry_backoff: 백오프 타입 (linear, exponential)
     * - verify_ssl: SSL 인증서 검증 (기본: true)
     * - user_agent: User-Agent 헤더 (기본: PulseOne-CSPGateway/1.8)
     * - follow_redirects: 리다이렉트 따라가기 (기본: true)
     * - max_redirects: 최대 리다이렉트 수 (기본: 3)
     * - compression: 요청 압축 사용 (기본: false)
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송
     * @param alarm 전송할 알람 메시지
     * @param config 타겟별 설정
     * @return 전송 결과
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 연결 성공 여부
     * 
     * 테스트 방법:
     * 1. GET /health 요청 (있는 경우)
     * 2. HEAD 요청으로 기본 URL 확인
     * 3. OPTIONS 요청으로 지원 메서드 확인
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     */
    std::string getTypeName() const override { return "http"; }
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;

private:
    /**
     * @brief HTTP 클라이언트 옵션 구성
     * @param config 설정 객체
     * @return HTTP 요청 옵션
     */
    PulseOne::Client::HttpRequestOptions createHttpOptions(const json& config) const;
    
    /**
     * @brief 인증 키 로드 (암호화된 파일에서)
     * @param key_file 키 파일 경로
     * @return 복호화된 인증 키
     */
    std::string loadAuthKey(const std::string& key_file) const;
    
    /**
     * @brief HTTP 헤더 준비
     * @param config 설정 객체
     * @param alarm 알람 메시지 (동적 헤더용)
     * @return 헤더 맵
     */
    std::unordered_map<std::string, std::string> prepareHeaders(
        const json& config, const AlarmMessage& alarm) const;
    
    /**
     * @brief 인증 헤더 추가
     * @param headers 헤더 맵 (입출력)
     * @param config 설정 객체
     */
    void addAuthHeaders(std::unordered_map<std::string, std::string>& headers, 
                       const json& config) const;
    
    /**
     * @brief 요청 본문 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 요청 본문 문자열
     */
    std::string createRequestBody(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 재시도 로직 실행
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 최종 전송 결과
     */
    TargetSendResult executeWithRetry(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 단일 HTTP 요청 실행
     * @param endpoint 요청 URL
     * @param method HTTP 메서드
     * @param body 요청 본문
     * @param headers 헤더 맵
     * @param options HTTP 옵션
     * @return HTTP 응답 결과
     */
    TargetSendResult executeSingleRequest(
        const std::string& endpoint,
        const std::string& method,
        const std::string& body,
        const std::unordered_map<std::string, std::string>& headers,
        const PulseOne::Client::HttpRequestOptions& options) const;
    
    /**
     * @brief HTTP 응답을 TargetSendResult로 변환
     * @param response HTTP 응답
     * @param response_time 응답 시간
     * @return 타겟 전송 결과
     */
    TargetSendResult convertHttpResponse(
        const PulseOne::Client::HttpResponse& response,
        std::chrono::milliseconds response_time) const;
    
    /**
     * @brief URL 유효성 검증
     * @param url 검증할 URL
     * @return 유효하면 true
     */
    bool isValidUrl(const std::string& url) const;
    
    /**
     * @brief HTTP 메서드 유효성 검증
     * @param method HTTP 메서드
     * @return 지원하는 메서드면 true
     */
    bool isValidHttpMethod(const std::string& method) const;
    
    /**
     * @brief Content-Type 유효성 검증
     * @param content_type Content-Type 문자열
     * @return 유효한 타입이면 true
     */
    bool isValidContentType(const std::string& content_type) const;
    
    /**
     * @brief 응답 코드가 성공인지 확인
     * @param status_code HTTP 상태 코드
     * @return 2xx 범위면 true
     */
    bool isSuccessStatusCode(int status_code) const {
        return status_code >= 200 && status_code < 300;
    }
    
    /**
     * @brief 응답 코드가 재시도 가능한지 확인
     * @param status_code HTTP 상태 코드
     * @return 재시도 가능하면 true (5xx, 408, 429 등)
     */
    bool isRetryableStatusCode(int status_code) const;
    
    /**
     * @brief 에러 메시지 생성
     * @param status_code HTTP 상태 코드
     * @param response_body 응답 본문
     * @return 사용자 친화적인 에러 메시지
     */
    std::string createErrorMessage(int status_code, const std::string& response_body) const;
    
    /**
     * @brief 재시도 지연 시간 계산
     * @param attempt 현재 시도 횟수 (0부터 시작)
     * @param base_delay_ms 기본 지연 시간
     * @param backoff_type 백오프 타입 ("linear" 또는 "exponential")
     * @return 지연 시간 (밀리초)
     */
    int calculateRetryDelay(int attempt, int base_delay_ms, const std::string& backoff_type) const;
    
    /**
     * @brief 요청 본문 압축
     * @param data 원본 데이터
     * @param headers 헤더 맵 (Content-Encoding 추가)
     * @return 압축된 데이터
     */
    std::string compressRequestBody(const std::string& data, 
                                   std::unordered_map<std::string, std::string>& headers) const;
    
    /**
     * @brief 템플릿 변수 확장 (URL, 헤더, 본문용)
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief 설정 검증
     * @param config 검증할 설정
     * @param error_message 오류 메시지 (출력용)
     * @return 설정이 유효한지 여부
     */
    bool validateConfig(const json& config, std::string& error_message) const;
    
    /**
     * @brief CSP API 호환 본문 생성 (C# 버전 호환)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return CSP API 형식의 JSON 본문
     */
    std::string createCSPCompatibleBody(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 요청 로깅 (디버그용)
     * @param method HTTP 메서드
     * @param url 요청 URL
     * @param headers 헤더 맵
     * @param body 요청 본문
     */
    void logRequest(const std::string& method, const std::string& url,
                   const std::unordered_map<std::string, std::string>& headers,
                   const std::string& body) const;
    
    /**
     * @brief 응답 로깅 (디버그용)
     * @param response HTTP 응답
     * @param response_time 응답 시간
     */
    void logResponse(const PulseOne::Client::HttpResponse& response,
                    std::chrono::milliseconds response_time) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // HTTP_TARGET_HANDLER_H