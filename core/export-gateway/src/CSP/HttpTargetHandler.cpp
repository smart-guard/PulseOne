/**
 * @file HttpTargetHandler.cpp
 * @brief CSP Gateway HTTP/HTTPS 타겟 핸들러 구현
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/HttpTargetHandler.cpp
 * 
 * 기존 PulseOne 패턴 100% 준수:
 * - HttpClient.cpp의 재시도 로직 패턴 차용
 * - S3Client.cpp의 지수 백오프 알고리즘 적용
 * - 표준 LogManager 사용법
 * - 인증 헤더 처리 (Bearer Token, Basic Auth, API Key)
 */

#include "CSP/HttpTargetHandler.h"
#include "Client/HttpClient.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cmath>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

HttpTargetHandler::HttpTargetHandler() {
    LogManager::getInstance().Info("HttpTargetHandler 초기화");
}

HttpTargetHandler::~HttpTargetHandler() {
    LogManager::getInstance().Info("HttpTargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool HttpTargetHandler::initialize(const json& config) {
    try {
        LogManager::getInstance().Info("HTTP 타겟 핸들러 초기화 시작");
        
        // 필수 설정 검증
        if (!config.contains("url") || config["url"].get<std::string>().empty()) {
            LogManager::getInstance().Error("HTTP URL이 설정되지 않음");
            return false;
        }
        
        std::string url = config["url"].get<std::string>();
        LogManager::getInstance().Info("HTTP 타겟 URL: " + url);
        
        // HTTP 클라이언트 옵션 구성 (HttpClient.cpp 패턴 차용)
        PulseOne::Client::HttpRequestOptions options;
        
        // 타임아웃 설정
        options.timeout_sec = config.value("timeout_sec", 30);
        options.connect_timeout_sec = config.value("connect_timeout_sec", 10);
        
        // SSL 설정
        options.verify_ssl = config.value("verify_ssl", true);
        
        // User-Agent 설정
        options.user_agent = config.value("user_agent", "PulseOne-CSPGateway/1.0");
        
        // 재시도 설정 (S3Client.cpp 패턴 차용)
        retry_config_.max_attempts = config.value("max_retry_attempts", 3);
        retry_config_.initial_delay_ms = config.value("initial_delay_ms", 1000);
        retry_config_.max_delay_ms = config.value("max_delay_ms", 30000);
        retry_config_.backoff_multiplier = config.value("backoff_multiplier", 2.0);
        
        // 인증 설정 파싱
        parseAuthenticationConfig(config);
        
        // HTTP 클라이언트 생성
        http_client_ = std::make_unique<PulseOne::Client::HttpClient>(url, options);
        
        LogManager::getInstance().Info("HTTP 타겟 핸들러 초기화 완료");
        LogManager::getInstance().Debug("설정 - timeout: " + std::to_string(options.timeout_sec) + 
                                       "s, max_retries: " + std::to_string(retry_config_.max_attempts) +
                                       ", auth_type: " + auth_config_.type);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("HTTP 타겟 핸들러 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult HttpTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "HTTP";
    result.target_name = getTargetName(config);
    result.success = false;
    
    try {
        if (!http_client_) {
            result.error_message = "HTTP 클라이언트가 초기화되지 않음";
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        LogManager::getInstance().Info("HTTP 알람 전송 시작: " + result.target_name);
        
        // 재시도 로직으로 전송 (S3Client.cpp 패턴 차용)
        result = executeWithRetry(alarm, config);
        
        if (result.success) {
            LogManager::getInstance().Info("HTTP 알람 전송 성공: " + result.target_name + 
                                          " (응답코드: " + std::to_string(result.http_status_code) + ")");
        } else {
            LogManager::getInstance().Error("HTTP 알람 전송 실패: " + result.target_name + 
                                           " - " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.error_message = "HTTP 전송 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

bool HttpTargetHandler::testConnection(const json& config) {
    try {
        LogManager::getInstance().Info("HTTP 연결 테스트 시작");
        
        if (!http_client_) {
            LogManager::getInstance().Error("HTTP 클라이언트가 초기화되지 않음");
            return false;
        }
        
        // 테스트 요청 생성 (GET 또는 HEAD)
        std::string test_endpoint = config.value("test_endpoint", "/health");
        std::string method = config.value("test_method", "GET");
        
        // 헤더 준비
        auto headers = buildRequestHeaders(config);
        
        // 테스트 요청 실행
        PulseOne::Client::HttpResponse response;
        
        if (method == "HEAD") {
            response = http_client_->head(test_endpoint, headers);
        } else if (method == "POST") {
            json test_payload = {{"test", true}, {"timestamp", getCurrentTimestamp()}};
            response = http_client_->post(test_endpoint, test_payload.dump(), headers);
        } else {
            // 기본적으로 GET 요청
            response = http_client_->get(test_endpoint, headers);
        }
        
        bool success = response.isSuccess();
        
        if (success) {
            LogManager::getInstance().Info("HTTP 연결 테스트 성공 (상태코드: " + 
                                          std::to_string(response.status_code) + ")");
        } else {
            LogManager::getInstance().Error("HTTP 연결 테스트 실패 (상태코드: " + 
                                           std::to_string(response.status_code) + 
                                           ", 메시지: " + response.body + ")");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("HTTP 연결 테스트 예외: " + std::string(e.what()));
        return false;
    }
}

std::string HttpTargetHandler::getTypeName() const {
    return "HTTP";
}

// =============================================================================
// 내부 구현 메서드들 (HttpClient.cpp 패턴 차용)
// =============================================================================

TargetSendResult HttpTargetHandler::executeWithRetry(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "HTTP";
    result.target_name = getTargetName(config);
    result.success = false;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int attempt = 0; attempt <= retry_config_.max_attempts; ++attempt) {
        try {
            // 지연 시간 적용 (첫 번째 시도는 지연 없음)
            if (attempt > 0) {
                uint32_t delay_ms = calculateBackoffDelay(attempt - 1);
                LogManager::getInstance().Debug("재시도 대기: " + std::to_string(delay_ms) + "ms (시도 " + 
                                               std::to_string(attempt + 1) + "/" + 
                                               std::to_string(retry_config_.max_attempts + 1) + ")");
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
            
            LogManager::getInstance().Debug("HTTP 전송 시도 " + std::to_string(attempt + 1) + "/" + 
                                           std::to_string(retry_config_.max_attempts + 1) + 
                                           ": " + result.target_name);
            
            // 실제 HTTP 요청 실행
            auto attempt_result = executeSingleRequest(alarm, config);
            
            if (attempt_result.success) {
                // 성공 시 결과 반환
                result = attempt_result;
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                result.response_time_ms = duration.count();
                
                LogManager::getInstance().Info("HTTP 전송 성공 (시도 " + std::to_string(attempt + 1) + 
                                              ", 소요시간: " + std::to_string(result.response_time_ms) + "ms)");
                return result;
            }
            
            // 실패 시 재시도 가능 여부 확인 (4xx 클라이언트 오류는 재시도 안함)
            if (attempt_result.http_status_code >= 400 && attempt_result.http_status_code < 500) {
                LogManager::getInstance().Error("클라이언트 오류로 재시도 중단 (상태코드: " + 
                                               std::to_string(attempt_result.http_status_code) + ")");
                result = attempt_result;
                break;
            }
            
            result = attempt_result;  // 마지막 실패 결과 보존
            
        } catch (const std::exception& e) {
            result.error_message = "HTTP 요청 예외: " + std::string(e.what());
            LogManager::getInstance().Error("시도 " + std::to_string(attempt + 1) + " 예외: " + result.error_message);
        }
    }
    
    // 모든 재시도 실패
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.response_time_ms = duration.count();
    
    LogManager::getInstance().Error("HTTP 전송 최종 실패 - 모든 재시도 소진 (" + 
                                   std::to_string(retry_config_.max_attempts + 1) + "회 시도, " +
                                   std::to_string(result.response_time_ms) + "ms)");
    
    return result;
}

TargetSendResult HttpTargetHandler::executeSingleRequest(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "HTTP";
    result.target_name = getTargetName(config);
    result.success = false;
    
    try {
        // HTTP 메서드 결정
        std::string method = config.value("method", "POST");
        std::transform(method.begin(), method.end(), method.begin(), ::toupper);
        
        // 엔드포인트 URL 구성
        std::string endpoint = config.value("endpoint", "/api/alarms");
        
        // 요청 헤더 구성
        auto headers = buildRequestHeaders(config);
        
        // 요청 본문 생성
        std::string request_body = buildRequestBody(alarm, config);
        
        LogManager::getInstance().Debug("HTTP 요청 - Method: " + method + 
                                       ", Endpoint: " + endpoint +
                                       ", Body length: " + std::to_string(request_body.length()));
        
        // HTTP 요청 실행
        PulseOne::Client::HttpResponse response;
        
        if (method == "POST") {
            response = http_client_->post(endpoint, request_body, headers);
        } else if (method == "PUT") {
            response = http_client_->put(endpoint, request_body, headers);
        } else if (method == "PATCH") {
            // HttpClient에 patch 메서드가 없으면 post로 대체하고 헤더로 오버라이드
            headers["X-HTTP-Method-Override"] = "PATCH";
            response = http_client_->post(endpoint, request_body, headers);
        } else {
            result.error_message = "지원하지 않는 HTTP 메서드: " + method;
            return result;
        }
        
        // 응답 처리
        result.http_status_code = response.status_code;
        result.response_body = response.body;
        result.success = response.isSuccess();
        
        if (!result.success) {
            result.error_message = "HTTP " + std::to_string(response.status_code) + ": " + response.body;
        }
        
    } catch (const std::exception& e) {
        result.error_message = "HTTP 요청 실행 예외: " + std::string(e.what());
    }
    
    return result;
}

std::unordered_map<std::string, std::string> HttpTargetHandler::buildRequestHeaders(const json& config) {
    std::unordered_map<std::string, std::string> headers;
    
    // 기본 헤더 설정
    headers["Content-Type"] = config.value("content_type", "application/json; charset=utf-8");
    headers["Accept"] = "application/json";
    headers["User-Agent"] = config.value("user_agent", "PulseOne-CSPGateway/1.0");
    
    // 인증 헤더 추가
    addAuthenticationHeaders(headers, config);
    
    // 사용자 정의 헤더 추가
    if (config.contains("headers") && config["headers"].is_object()) {
        for (auto& [key, value] : config["headers"].items()) {
            if (value.is_string()) {
                headers[key] = value.get<std::string>();
            }
        }
    }
    
    // 트레이싱 헤더 추가 (선택사항)
    headers["X-Request-ID"] = generateRequestId();
    headers["X-Timestamp"] = getCurrentTimestamp();
    
    return headers;
}

std::string HttpTargetHandler::buildRequestBody(const AlarmMessage& alarm, const json& config) {
    try {
        // 요청 형식 결정
        std::string format = config.value("body_format", "json");
        std::transform(format.begin(), format.end(), format.begin(), ::tolower);
        
        if (format == "json") {
            return buildJsonRequestBody(alarm, config);
        } else if (format == "xml") {
            return buildXmlRequestBody(alarm, config);
        } else if (format == "form") {
            return buildFormRequestBody(alarm, config);
        } else {
            LogManager::getInstance().Warn("알 수 없는 body_format: " + format + " (JSON 사용)");
            return buildJsonRequestBody(alarm, config);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("요청 본문 생성 실패: " + std::string(e.what()));
        return "{}";
    }
}

std::string HttpTargetHandler::buildJsonRequestBody(const AlarmMessage& alarm, const json& config) {
    json request_body;
    
    // 기본 알람 데이터 매핑
    request_body["building_id"] = alarm.bd;
    request_body["point_name"] = alarm.nm;
    request_body["value"] = alarm.vl;
    request_body["timestamp"] = alarm.tm;
    request_body["alarm_flag"] = alarm.al;
    request_body["status"] = alarm.st;
    request_body["description"] = alarm.des;
    
    // 추가 메타데이터
    request_body["source"] = "PulseOne-CSPGateway";
    request_body["version"] = "1.0";
    request_body["alarm_status"] = alarm.get_alarm_status_string();
    
    // 사용자 정의 필드 매핑
    if (config.contains("field_mapping") && config["field_mapping"].is_object()) {
        json mapped_body;
        for (auto& [target_field, source_field] : config["field_mapping"].items()) {
            if (source_field.is_string()) {
                std::string source = source_field.get<std::string>();
                if (request_body.contains(source)) {
                    mapped_body[target_field] = request_body[source];
                }
            }
        }
        
        // 매핑된 본문이 있으면 사용, 없으면 기본 본문 사용
        if (!mapped_body.empty()) {
            request_body = mapped_body;
        }
    }
    
    // 템플릿 기반 커스터마이징 (선택사항)
    if (config.contains("body_template") && config["body_template"].is_object()) {
        json template_body = config["body_template"];
        expandTemplateVariables(template_body, alarm);
        request_body = template_body;
    }
    
    return request_body.dump();
}

std::string HttpTargetHandler::buildXmlRequestBody(const AlarmMessage& alarm, const json& config) {
    // 간단한 XML 형식 구성
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<alarm>\n";
    xml << "  <building_id>" << alarm.bd << "</building_id>\n";
    xml << "  <point_name><![CDATA[" << alarm.nm << "]]></point_name>\n";
    xml << "  <value>" << alarm.vl << "</value>\n";
    xml << "  <timestamp><![CDATA[" << alarm.tm << "]]></timestamp>\n";
    xml << "  <alarm_flag>" << alarm.al << "</alarm_flag>\n";
    xml << "  <status>" << alarm.st << "</status>\n";
    xml << "  <description><![CDATA[" << alarm.des << "]]></description>\n";
    xml << "  <source>PulseOne-CSPGateway</source>\n";
    xml << "</alarm>\n";
    
    return xml.str();
}

std::string HttpTargetHandler::buildFormRequestBody(const AlarmMessage& alarm, const json& config) {
    // URL-encoded form 데이터
    std::ostringstream form;
    form << "building_id=" << alarm.bd;
    form << "&point_name=" << urlEncode(alarm.nm);
    form << "&value=" << alarm.vl;
    form << "&timestamp=" << urlEncode(alarm.tm);
    form << "&alarm_flag=" << alarm.al;
    form << "&status=" << alarm.st;
    form << "&description=" << urlEncode(alarm.des);
    form << "&source=PulseOne-CSPGateway";
    
    return form.str();
}

// =============================================================================
// 인증 관련 메서드들
// =============================================================================

void HttpTargetHandler::parseAuthenticationConfig(const json& config) {
    auth_config_ = {}; // 초기화
    
    if (!config.contains("auth") || !config["auth"].is_object()) {
        auth_config_.type = "none";
        return;
    }
    
    const json& auth = config["auth"];
    auth_config_.type = auth.value("type", "none");
    
    if (auth_config_.type == "bearer") {
        auth_config_.bearer_token = auth.value("token", "");
    } else if (auth_config_.type == "basic") {
        auth_config_.basic_username = auth.value("username", "");
        auth_config_.basic_password = auth.value("password", "");
    } else if (auth_config_.type == "api_key") {
        auth_config_.api_key = auth.value("key", "");
        auth_config_.api_key_header = auth.value("header", "X-API-Key");
    }
}

void HttpTargetHandler::addAuthenticationHeaders(std::unordered_map<std::string, std::string>& headers, 
                                                const json& config) {
    if (auth_config_.type == "bearer" && !auth_config_.bearer_token.empty()) {
        headers["Authorization"] = "Bearer " + auth_config_.bearer_token;
        
    } else if (auth_config_.type == "basic" && !auth_config_.basic_username.empty()) {
        std::string credentials = auth_config_.basic_username + ":" + auth_config_.basic_password;
        std::string encoded = base64Encode(credentials);
        headers["Authorization"] = "Basic " + encoded;
        
    } else if (auth_config_.type == "api_key" && !auth_config_.api_key.empty()) {
        headers[auth_config_.api_key_header] = auth_config_.api_key;
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

uint32_t HttpTargetHandler::calculateBackoffDelay(int attempt) const {
    // 지수 백오프 계산 (S3Client.cpp 패턴 차용)
    double delay = retry_config_.initial_delay_ms * std::pow(retry_config_.backoff_multiplier, attempt);
    
    // 최대 지연 시간 제한
    delay = std::min(delay, static_cast<double>(retry_config_.max_delay_ms));
    
    // 지터 추가 (20% 랜덤 변동)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.8, 1.2);
    delay *= dis(gen);
    
    return static_cast<uint32_t>(delay);
}

std::string HttpTargetHandler::getTargetName(const json& config) const {
    if (config.contains("name") && config["name"].is_string()) {
        return config["name"].get<std::string>();
    }
    
    if (config.contains("url") && config["url"].is_string()) {
        return config["url"].get<std::string>();
    }
    
    return "HTTP-Target";
}

std::string HttpTargetHandler::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string HttpTargetHandler::generateRequestId() const {
    // 간단한 요청 ID 생성 (UUID 대신 타임스탬프 기반)
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    
    std::ostringstream oss;
    oss << "req_" << ms.count();
    return oss.str();
}

void HttpTargetHandler::expandTemplateVariables(json& template_json, const AlarmMessage& alarm) const {
    // JSON 내의 템플릿 변수를 실제 값으로 치환
    std::function<void(json&)> expand = [&](json& obj) {
        if (obj.is_string()) {
            std::string str = obj.get<std::string>();
            str = std::regex_replace(str, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
            str = std::regex_replace(str, std::regex("\\{point_name\\}"), alarm.nm);
            str = std::regex_replace(str, std::regex("\\{value\\}"), std::to_string(alarm.vl));
            str = std::regex_replace(str, std::regex("\\{timestamp\\}"), alarm.tm);
            str = std::regex_replace(str, std::regex("\\{description\\}"), alarm.des);
            obj = str;
        } else if (obj.is_object()) {
            for (auto& [key, value] : obj.items()) {
                expand(value);
            }
        } else if (obj.is_array()) {
            for (auto& item : obj) {
                expand(item);
            }
        }
    };
    
    expand(template_json);
}

std::string HttpTargetHandler::urlEncode(const std::string& str) const {
    std::ostringstream encoded;
    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase << std::hex << (0xFF & c);
        }
    }
    return encoded.str();
}

std::string HttpTargetHandler::base64Encode(const std::string& input) const {
    // 간단한 Base64 인코딩 (실제 구현에서는 라이브러리 사용 권장)
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
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

} // namespace CSP
} // namespace PulseOne