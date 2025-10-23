/**
 * @file HttpTargetHandler.h
 * @brief HTTP/HTTPS 타겟 핸들러 - 완전 수정
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 4.0.0 (구현 파일과 완전 일치)
 * 저장 위치: core/export-gateway/include/CSP/HttpTargetHandler.h
 * 
 * 🚨 수정사항:
 * - 구현 파일의 모든 메서드 헤더에 선언
 * - urlEncode(), base64Encode() const 추가
 * - expandTemplateVariables() 오버로드 추가
 */

#ifndef HTTP_TARGET_HANDLER_H
#define HTTP_TARGET_HANDLER_H

#include "ExportTypes.h"
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
    std::string type = "none";
    std::string bearer_token;
    std::string basic_username;
    std::string basic_password;
    std::string api_key;
    std::string api_key_header = "X-API-Key";
};

/**
 * @brief HTTP/HTTPS 타겟 핸들러
 */
class HttpTargetHandler : public ITargetHandler {
private:
    mutable std::mutex client_mutex_;
    std::atomic<size_t> request_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    
    std::unique_ptr<PulseOne::Client::HttpClient> http_client_;
    RetryConfig retry_config_;
    AuthConfig auth_config_;
    
public:
    HttpTargetHandler();
    ~HttpTargetHandler() override;
    
    HttpTargetHandler(const HttpTargetHandler&) = delete;
    HttpTargetHandler& operator=(const HttpTargetHandler&) = delete;
    HttpTargetHandler(HttpTargetHandler&&) = delete;
    HttpTargetHandler& operator=(HttpTargetHandler&&) = delete;
    
    // =======================================================================
    // ITargetHandler 인터페이스 구현
    // =======================================================================
    
    bool initialize(const json& config) override;
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    bool testConnection(const json& config) override;
    std::string getHandlerType() const override { return "HTTP"; }
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;
    void cleanup() override;
    json getStatus() const override;

    // =======================================================================
    // HTTP 특화 공개 메서드들
    // =======================================================================
    
    json getStatistics() const;
    void resetStatistics();

private:
    // =======================================================================
    // 내부 구현 메서드들 (구현 파일과 완전 일치)
    // =======================================================================
    
    /**
     * @brief 재시도와 함께 HTTP 요청 실행
     */
    TargetSendResult executeWithRetry(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 단일 HTTP 요청 실행
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
     * @brief JSON 형식 요청 본문 생성
     */
    std::string buildJsonRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief XML 형식 요청 본문 생성
     */
    std::string buildXmlRequestBody(const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief Form 형식 요청 본문 생성
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
     * @brief 타겟 이름 추출
     */
    std::string getTargetName(const json& config) const;
    
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
    void expandTemplateVariables(json& template_json, const AlarmMessage& alarm) const;
    
    /**
     * @brief 문자열 템플릿 변수 확장
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief URL 인코딩
     */
    std::string urlEncode(const std::string& str) const;
    
    /**
     * @brief Base64 인코딩
     */
    std::string base64Encode(const std::string& input) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // HTTP_TARGET_HANDLER_H