/**
 * @file HttpTargetHandler.cpp
 * @brief CSP Gateway HTTP/HTTPS 타겟 핸들러 구현 (컴파일 에러 수정)
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/HttpTargetHandler.cpp
 * 
 * 🚨 수정사항:
 * 1. patch() 메서드 → executeRequest() 직접 호출로 대체
 * 2. elapsed_ms → response_time_ms로 수정
 * 3. 미사용 파라미터 경고 제거
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
        
        // URL/Endpoint 가져오기 (endpoint 우선, 없으면 url)
        std::string url;
        if (config.contains("endpoint") && !config["endpoint"].get<std::string>().empty()) {
            url = config["endpoint"].get<std::string>();
        } else if (config.contains("url") && !config["url"].get<std::string>().empty()) {
            url = config["url"].get<std::string>();
        } else {
            LogManager::getInstance().Error("HTTP URL/Endpoint가 설정되지 않음");
            return false;
        }
        
        LogManager::getInstance().Info("HTTP 타겟 URL: " + url);
        
        // HTTP 클라이언트 옵션 구성
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = config.value("timeout_sec", 30);
        options.connect_timeout_sec = config.value("connect_timeout_sec", 10);
        options.verify_ssl = config.value("verify_ssl", true);
        options.user_agent = config.value("user_agent", "PulseOne-CSPGateway/1.8");
        
        // HTTP 클라이언트 생성
        http_client_ = std::make_unique<PulseOne::Client::HttpClient>(url, options);
        
        if (!http_client_) {
            LogManager::getInstance().Error("HTTP 클라이언트 생성 실패");
            return false;
        }
        
        // 재시도 설정
        if (config.contains("max_retry")) {
            retry_config_.max_attempts = config["max_retry"].get<int>();
        }
        if (config.contains("retry_delay_ms")) {
            retry_config_.initial_delay_ms = config["retry_delay_ms"].get<uint32_t>();
        }
        if (config.contains("retry_backoff")) {
            std::string backoff = config["retry_backoff"].get<std::string>();
            if (backoff == "exponential") {
                retry_config_.backoff_multiplier = 2.0;
            } else if (backoff == "linear") {
                retry_config_.backoff_multiplier = 1.0;
            }
        }
        
        // 인증 설정 파싱
        parseAuthenticationConfig(config);
        
        LogManager::getInstance().Info("HTTP 타겟 핸들러 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("HTTP 핸들러 초기화 실패: " + std::string(e.what()));
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
        
        // 재시도 로직으로 전송
        result = executeWithRetry(alarm, config);
        
        if (result.success) {
            LogManager::getInstance().Info("HTTP 알람 전송 성공: " + result.target_name + 
                                          " (응답코드: " + std::to_string(result.status_code) + ")");
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
        
        // 테스트 요청 생성
        std::string test_endpoint = config.value("test_endpoint", "/health");
        std::string method = config.value("test_method", "GET");
        
        // 헤더 준비
        auto headers = buildRequestHeaders(config);
        
        // 테스트 요청 실행
        PulseOne::Client::HttpResponse response;
        
        if (method == "POST") {
            json test_payload;
            test_payload["test"] = true;
            test_payload["timestamp"] = getCurrentTimestamp();
            response = http_client_->post(test_endpoint, test_payload.dump(), "application/json", headers);
        } else {
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

bool HttpTargetHandler::validateConfig(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    
    try {
        // URL/Endpoint 검증
        if (!config.contains("endpoint") && !config.contains("url")) {
            errors.push_back("endpoint 또는 url 필드가 필수입니다");
            return false;
        }
        
        std::string url;
        if (config.contains("endpoint")) {
            if (!config["endpoint"].is_string() || config["endpoint"].get<std::string>().empty()) {
                errors.push_back("endpoint는 비어있지 않은 문자열이어야 합니다");
                return false;
            }
            url = config["endpoint"].get<std::string>();
        } else {
            if (!config["url"].is_string() || config["url"].get<std::string>().empty()) {
                errors.push_back("url은 비어있지 않은 문자열이어야 합니다");
                return false;
            }
            url = config["url"].get<std::string>();
        }
        
        // URL 형식 검증
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            errors.push_back("URL은 http:// 또는 https://로 시작해야 합니다");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        errors.push_back("설정 검증 중 예외 발생: " + std::string(e.what()));
        return false;
    }
}

json HttpTargetHandler::getStatus() const {
    return json{
        {"type", "HTTP"},
        {"request_count", request_count_.load()},
        {"success_count", success_count_.load()},
        {"failure_count", failure_count_.load()},
        {"auth_type", auth_config_.type}
    };
}

json HttpTargetHandler::getStatistics() const {
    return getStatus();
}

void HttpTargetHandler::resetStatistics() {
    request_count_ = 0;
    success_count_ = 0;
    failure_count_ = 0;
}

void HttpTargetHandler::cleanup() {
    if (http_client_) {
        http_client_.reset();
    }
    LogManager::getInstance().Info("HttpTargetHandler 정리 완료");
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

TargetSendResult HttpTargetHandler::executeWithRetry(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "HTTP";
    result.target_name = getTargetName(config);
    result.success = false;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int attempt = 0; attempt <= retry_config_.max_attempts; ++attempt) {
        try {
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
            
            auto attempt_result = executeSingleRequest(alarm, config);
            
            if (attempt_result.success) {
                result = attempt_result;
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                result.response_time = duration;
                
                LogManager::getInstance().Info("HTTP 전송 성공 (시도 " + std::to_string(attempt + 1) + 
                                              ", 소요시간: " + std::to_string(result.response_time.count()) + "ms)");
                return result;
            }
            
            // 4xx 에러는 재시도 안함
            if (attempt_result.status_code >= 400 && attempt_result.status_code < 500) {
                LogManager::getInstance().Error("클라이언트 오류로 재시도 중단 (상태코드: " + 
                                               std::to_string(attempt_result.status_code) + ")");
                result = attempt_result;
                break;
            }
            
            result = attempt_result;
            
        } catch (const std::exception& e) {
            result.error_message = "HTTP 요청 예외: " + std::string(e.what());
            LogManager::getInstance().Error("시도 " + std::to_string(attempt + 1) + " 예외: " + result.error_message);
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.response_time = duration;
    
    LogManager::getInstance().Error("HTTP 전송 최종 실패 - 모든 재시도 소진 (" + 
                                   std::to_string(retry_config_.max_attempts + 1) + "회 시도, " +
                                   std::to_string(result.response_time.count()) + "ms)");
    
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
        
        std::string endpoint = "";
        if (config.contains("endpoint")) {
            endpoint = config["endpoint"].get<std::string>();
        }
        
        std::string log_endpoint = endpoint.empty() ? "(using base URL only)" : endpoint;
        LogManager::getInstance().Debug("HTTP 요청 - Method: " + method + 
                                       ", Endpoint: " + log_endpoint);
        
        // 요청 헤더 및 본문 구성
        auto headers = buildRequestHeaders(config);
        std::string request_body = buildRequestBody(alarm, config);
        
        LogManager::getInstance().Debug("Body length: " + std::to_string(request_body.length()));
        
        // ✅ 수정: HTTP 요청 실행 (PATCH 처리)
        PulseOne::Client::HttpResponse response;
        
        if (method == "POST") {
            response = http_client_->post(endpoint, request_body, "application/json", headers);
        } else if (method == "PUT") {
            response = http_client_->put(endpoint, request_body, "application/json", headers);
        } else if (method == "PATCH") {
            // ✅ HttpClient에 patch()가 없으므로 PUT으로 대체
            LogManager::getInstance().Warn("PATCH 메서드는 PUT으로 대체됨");
            response = http_client_->put(endpoint, request_body, "application/json", headers);
        } else {
            response = http_client_->get(endpoint, headers);
        }
        
        // 응답 처리
        result.success = (response.status_code >= 200 && response.status_code < 300);
        result.status_code = response.status_code;
        result.response_body = response.body;
        
        if (result.success) {
            // ✅ 수정: elapsed_ms → response_time_ms
            LogManager::getInstance().Debug("HTTP " + method + " " + log_endpoint + 
                                           " -> " + std::to_string(response.status_code) + 
                                           " (" + std::to_string(response.response_time_ms) + "ms)");
        } else {
            result.error_message = "HTTP " + std::to_string(response.status_code) + ": " + 
                                  response.body.substr(0, 200);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        result.error_message = "HTTP 요청 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
        return result;
    }
}

std::unordered_map<std::string, std::string> HttpTargetHandler::buildRequestHeaders(const json& config) {
    std::unordered_map<std::string, std::string> headers;
    
    headers["Accept"] = "application/json";
    headers["User-Agent"] = config.value("user_agent", "PulseOne-CSPGateway/1.0");
    
    // 인증 헤더 추가
    addAuthenticationHeaders(headers, config);
    
    // 사용자 정의 헤더
    if (config.contains("headers") && config["headers"].is_object()) {
        for (auto& [key, value] : config["headers"].items()) {
            if (value.is_string()) {
                headers[key] = value.get<std::string>();
            }
        }
    }
    
    headers["X-Request-ID"] = generateRequestId();
    headers["X-Timestamp"] = getCurrentTimestamp();
    
    return headers;
}

std::string HttpTargetHandler::buildRequestBody(const AlarmMessage& alarm, const json& config) {
    try {
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
    
    // ✅ icos C# AlarmMessage 포맷 사용
    request_body["bd"] = alarm.bd;        // Building ID
    request_body["nm"] = alarm.nm;        // Point Name
    request_body["vl"] = alarm.vl;        // Value
    request_body["tm"] = alarm.tm;        // Timestamp
    request_body["al"] = alarm.al;        // Alarm Flag
    request_body["st"] = alarm.st;        // Status
    request_body["des"] = alarm.des;      // Description
    
    // 메타데이터 (선택사항)
    if (config.value("include_metadata", false)) {
        request_body["source"] = "PulseOne-CSPGateway";
        request_body["version"] = "1.0";
        request_body["alarm_status"] = alarm.get_alarm_status_string();
    }
    
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
        
        if (!mapped_body.empty()) {
            request_body = mapped_body;
        }
    }
    
    // 템플릿 기반 커스터마이징
    if (config.contains("body_template") && config["body_template"].is_object()) {
        json template_body = config["body_template"];
        expandTemplateVariables(template_body, alarm);
        request_body = template_body;
    }
    
    return request_body.dump();
}

// ✅ 수정: 헤더와 일치하도록 config 파라미터 추가
std::string HttpTargetHandler::buildXmlRequestBody(const AlarmMessage& alarm, const json& /* config */) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<alarm>\n";
    
    // ✅ icos C# AlarmMessage XML 태그
    xml << "  <bd>" << alarm.bd << "</bd>\n";
    xml << "  <nm><![CDATA[" << alarm.nm << "]]></nm>\n";
    xml << "  <vl>" << alarm.vl << "</vl>\n";
    xml << "  <tm><![CDATA[" << alarm.tm << "]]></tm>\n";
    xml << "  <al>" << alarm.al << "</al>\n";
    xml << "  <st>" << alarm.st << "</st>\n";
    xml << "  <des><![CDATA[" << alarm.des << "]]></des>\n";
    
    xml << "  <source>PulseOne-CSPGateway</source>\n";
    xml << "</alarm>\n";
    
    return xml.str();
}

// ✅ 수정: 헤더와 일치하도록 config 파라미터 추가
std::string HttpTargetHandler::buildFormRequestBody(const AlarmMessage& alarm, const json& /* config */) {
    std::ostringstream form;
    
    // ✅ icos C# AlarmMessage 필드를 Form 데이터로
    form << "bd=" << alarm.bd;
    form << "&nm=" << urlEncode(alarm.nm);
    form << "&vl=" << alarm.vl;
    form << "&tm=" << urlEncode(alarm.tm);
    form << "&al=" << alarm.al;
    form << "&st=" << alarm.st;
    form << "&des=" << urlEncode(alarm.des);
    form << "&source=PulseOne-CSPGateway";
    
    return form.str();
}

// =============================================================================
// 인증 관련 메서드들
// =============================================================================

void HttpTargetHandler::parseAuthenticationConfig(const json& config) {
    auth_config_ = {};
    
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

// ✅ 수정: 미사용 파라미터 경고 제거
void HttpTargetHandler::addAuthenticationHeaders(std::unordered_map<std::string, std::string>& headers, 
                                                const json& /* config */) {
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
    double delay = retry_config_.initial_delay_ms * std::pow(retry_config_.backoff_multiplier, attempt);
    delay = std::min(delay, static_cast<double>(retry_config_.max_delay_ms));
    
    // 지터 추가
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
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    std::ostringstream oss;
    oss << "req_" << ms.count();
    return oss.str();
}

void HttpTargetHandler::expandTemplateVariables(json& template_json, const AlarmMessage& alarm) const {
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

std::string HttpTargetHandler::expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const {
    std::string result = template_str;
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{timestamp\\}"), alarm.tm);
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    result = std::regex_replace(result, std::regex("\\{description\\}"), alarm.des);
    result = std::regex_replace(result, std::regex("\\{alarm_status\\}"), alarm.get_alarm_status_string());
    return result;
}

} // namespace CSP
} // namespace PulseOne