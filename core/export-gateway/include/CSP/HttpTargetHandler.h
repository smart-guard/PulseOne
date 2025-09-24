/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS 타겟 핸들러 - 통합 타입 사용
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 1.1.0 (TargetTypes.h 통합 타입 사용)
 * 저장 위치: core/export-gateway/include/CSP/HttpTargetHandler.h
 */

#ifndef HTTP_TARGET_HANDLER_H
#define HTTP_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"
#include "TargetTypes.h"  // ✅ 통합 타입 사용
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
class HttpTargetHandler : public ITargetHandler {
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
    
    // ITargetHandler 인터페이스 구현 - 통합 타입 사용
    
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
     * @return 전송 결과 (TargetTypes.h의 TargetSendResult 사용)
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
    std::string getHandlerType() const override;
    
    /**
     * @brief 설정 유효성 검증
     */
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;

private:
    // 내부 구현 메서드들 - 통합 타입 사용
    
    /**
     * @brief 재시도와 함께 HTTP 요청 실행
     * @return TargetTypes.h의 TargetSendResult 사용
     */
    TargetSendResult executeWithRetry(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 단일 HTTP 요청 실행
     * @return TargetTypes.h의 TargetSendResult 사용
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
     * @brief JSON 요청 본문 생성
     */
    std::string buildJsonRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief XML 요청 본문 생성
     */
    std::string buildXmlRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief Form 요청 본문 생성
     */
    std::string buildFormRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 인증 설정 파싱
     */
    void parseAuthenticationConfig(const json& config);
    
    /**
     * @brief 인증 헤더 추가
     */
    void addAuthenticationHeaders(std::unordered_map<std::string, std::string>& headers,
                                 const json& config);
    
    /**
     * @brief 백오프 지연 시간 계산
     */
    uint32_t calculateBackoffDelay(int attempt) const;
    
    /**
     * @brief 타겟 이름 반환
     */
    std::string getTargetName(const json& config) const;
    
    /**
     * @brief 현재 타임스탬프 반환
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief 요청 ID 생성
     */
    std::string generateRequestId() const;
    
    /**
     * @brief 템플릿 변수 확장 (JSON용)
     */
    void expandTemplateVariables(json& template_json, const AlarmMessage& alarm) const;
    
    /**
     * @brief URL 인코딩
     */
    std::string urlEncode(const std::string& str) const;
    
    /**
     * @brief Base64 인코딩
     */
    std::string base64Encode(const std::string& input) const;
    
    /**
     * @brief 템플릿 변수 확장 (문자열용)
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // HTTP_TARGET_HANDLER_H