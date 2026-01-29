/**
 * @file HttpTargetHandler.cpp
 * @brief HTTP/HTTPS 타겟 핸들러 - Stateless 패턴 (v5.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 5.0.0 - Production-Ready 완성본
 * 저장 위치: core/export-gateway/src/CSP/HttpTargetHandler.cpp
 *
 * 🚀 v5.0 주요 변경:
 * - Stateless 핸들러 패턴 적용
 * - ClientCacheManager 기반 클라이언트 캐싱
 * - initialize() 선택적 (없어도 동작)
 * - Thread-safe 보장
 * - 메모리 효율적
 */

#include "CSP/HttpTargetHandler.h"
#include "CSP/AlarmMessage.h"
#include "Client/HttpClient.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ClientCacheManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace PulseOne {
namespace CSP {

using json = nlohmann::json;
using json = nlohmann::json;

// =============================================================================
// Static Client Cache (모든 인스턴스 공유)
// =============================================================================
static Utils::ClientCacheManager<Client::HttpClient,
                                 Client::HttpRequestOptions> &
getHttpClientCache() {
  static Utils::ClientCacheManager<Client::HttpClient,
                                   Client::HttpRequestOptions>
      cache(
          [](const Client::HttpRequestOptions &options) {
            // 팩토리: HttpClient 생성
            // base_url은 나중에 설정되므로 빈 문자열로 생성
            return std::make_shared<Client::HttpClient>("", options);
          },
          300 // 5분 유휴 시간
      );
  return cache;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

HttpTargetHandler::HttpTargetHandler() {
  LogManager::getInstance().Info("HttpTargetHandler 초기화 (Stateless)");
}

HttpTargetHandler::~HttpTargetHandler() {
  LogManager::getInstance().Info("HttpTargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool HttpTargetHandler::initialize(const json &config) {
  // ✅ Stateless 패턴: initialize()는 선택적
  // 설정 검증만 수행
  std::vector<std::string> errors;
  bool valid = validateConfig(config, errors);

  if (!valid) {
    for (const auto &error : errors) {
      LogManager::getInstance().Error("초기화 검증 실패: " + error);
    }
  }

  LogManager::getInstance().Info("HTTP 타겟 핸들러 초기화 완료 (Stateless)");
  return valid;
}

TargetSendResult HttpTargetHandler::sendAlarm(const AlarmMessage &alarm,
                                              const json &config) {
  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName(config);
  result.success = false;

  try {
    // ✅ URL 추출
    std::string url = extractUrl(config);
    if (url.empty()) {
      result.error_message = "URL/Endpoint가 설정되지 않음";
      LogManager::getInstance().Error(result.error_message);
      return result;
    }

    LogManager::getInstance().Info("HTTP 알람 전송 시작: " +
                                   result.target_name);

    // ✅ 재시도 로직으로 전송
    result = executeWithRetry(alarm, config, url);

    if (result.success) {
      success_count_++;
      LogManager::getInstance().Info(
          "HTTP 알람 전송 성공: " + result.target_name +
          " (응답코드: " + std::to_string(result.status_code) + ")");
    } else {
      failure_count_++;
      LogManager::getInstance().Error(
          "HTTP 알람 전송 실패: " + result.target_name + " - " +
          result.error_message);
    }

    request_count_++;

  } catch (const std::exception &e) {
    result.error_message = "HTTP 전송 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
    failure_count_++;
  }

  return result;
}

std::vector<TargetSendResult> HttpTargetHandler::sendValueBatch(
    const std::vector<PulseOne::CSP::ValueMessage> &values,
    const json &config) {

  std::vector<TargetSendResult> results;
  if (values.empty())
    return results;

  try {
    std::string url = extractUrl(config);
    if (url.empty()) {
      TargetSendResult res;
      res.success = false;
      res.error_message = "URL/Endpoint가 설정되지 않음";
      results.push_back(res);
      return results;
    }

    LogManager::getInstance().Info(
        "HTTP 주기 데이터 배치 전송 시작: " + getTargetName(config) + " (" +
        std::to_string(values.size()) + "개)");

    // ✅ 재시도 로직으로 전송 (배치)
    TargetSendResult batch_result = executeWithRetry(values, config, url);
    results.push_back(batch_result);

    if (batch_result.success) {
      success_count_++;
    } else {
      failure_count_++;
    }
    request_count_++;

  } catch (const std::exception &e) {
    TargetSendResult err_res;
    err_res.error_message = "HTTP 배치 전송 예외: " + std::string(e.what());
    results.push_back(err_res);
    failure_count_++;
  }

  return results;
}

bool HttpTargetHandler::testConnection(const json &config) {
  try {
    LogManager::getInstance().Info("HTTP 연결 테스트 시작");

    std::string url = extractUrl(config);
    if (url.empty()) {
      LogManager::getInstance().Error("테스트 실패: URL이 없음");
      return false;
    }

    // 클라이언트 획득
    auto client = getOrCreateClient(config, url);
    if (!client) {
      LogManager::getInstance().Error("클라이언트 생성 실패");
      return false;
    }

    // 테스트 요청
    std::string test_endpoint = config.value("test_endpoint", "/health");
    std::string method = config.value("test_method", "GET");
    auto headers = buildRequestHeaders(config);

    Client::HttpResponse response;
    if (method == "POST") {
      json test_payload;
      test_payload["test"] = true;
      test_payload["timestamp"] = getCurrentTimestamp();
      response = client->post(test_endpoint, test_payload.dump(),
                              "application/json", headers);
    } else {
      response = client->get(test_endpoint, headers);
    }

    bool success = response.isSuccess();
    LogManager::getInstance().Info(
        "HTTP 연결 테스트 " + std::string(success ? "성공" : "실패") +
        " (상태코드: " + std::to_string(response.status_code) + ")");

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("HTTP 연결 테스트 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

bool HttpTargetHandler::validateConfig(const json &config,
                                       std::vector<std::string> &errors) {
  errors.clear();

  // URL/Endpoint 검증
  if (!config.contains("endpoint") && !config.contains("url")) {
    errors.push_back("endpoint 또는 url 필드가 필수입니다");
    return false;
  }

  std::string url = extractUrl(config);
  if (url.empty()) {
    errors.push_back("URL이 비어있습니다");
    return false;
  }

  if (url.find("http://") != 0 && url.find("https://") != 0) {
    errors.push_back("URL은 http:// 또는 https://로 시작해야 합니다");
    return false;
  }

  return true;
}

json HttpTargetHandler::getStatus() const {
  return json{{"type", "HTTP"},
              {"request_count", request_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()},
              {"cache_stats", getHttpClientCache().getStats().active_clients}};
}

void HttpTargetHandler::cleanup() {
  getHttpClientCache().clear();
  LogManager::getInstance().Info("HttpTargetHandler 정리 완료");
}

// =============================================================================
// Private 핵심 메서드
// =============================================================================

std::shared_ptr<Client::HttpClient>
HttpTargetHandler::getOrCreateClient(const json &config,
                                     const std::string &url) {

  // ✅ 캐시 키: URL + 주요 설정 조합
  std::string cache_key = url;

  // HTTP 클라이언트 옵션 구성
  Client::HttpRequestOptions options;
  options.timeout_sec = config.value("timeout_sec", 30);
  options.connect_timeout_sec = config.value("connect_timeout_sec", 10);
  options.verify_ssl = config.value("verify_ssl", true);
  options.user_agent = config.value("user_agent", "iCos5/1.1");

  // ✅ 캐시에서 가져오거나 생성
  auto client = getHttpClientCache().getOrCreate(cache_key, options);

  return client;
}

std::string HttpTargetHandler::extractUrl(const json &config) const {
  if (config.contains("endpoint") &&
      !config["endpoint"].get<std::string>().empty()) {
    return config["endpoint"].get<std::string>();
  }
  if (config.contains("url") && !config["url"].get<std::string>().empty()) {
    return config["url"].get<std::string>();
  }
  return "";
}

TargetSendResult HttpTargetHandler::executeWithRetry(const AlarmMessage &alarm,
                                                     const json &config,
                                                     const std::string &url) {

  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName(config);
  result.success = false;

  // 재시도 설정
  RetryConfig retry_config;
  if (config.contains("max_retry")) {
    retry_config.max_attempts = config["max_retry"].get<int>();
  }
  if (config.contains("retry_delay_ms")) {
    retry_config.initial_delay_ms = config["retry_delay_ms"].get<uint32_t>();
  }

  auto start_time = std::chrono::steady_clock::now();

  for (int attempt = 0; attempt <= retry_config.max_attempts; ++attempt) {
    if (attempt > 0) {
      uint32_t delay_ms = calculateBackoffDelay(attempt - 1, retry_config);
      LogManager::getInstance().Debug(
          "재시도 대기: " + std::to_string(delay_ms) + "ms");
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    auto attempt_result = executeSingleRequest(alarm, config, url);

    if (attempt_result.success) {
      result = attempt_result;
      auto end_time = std::chrono::steady_clock::now();
      result.response_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);
      return result;
    }

    // 4xx 에러는 재시도 안함
    if (attempt_result.status_code >= 400 && attempt_result.status_code < 500) {
      result = attempt_result;
      break;
    }

    result = attempt_result;
  }

  auto end_time = std::chrono::steady_clock::now();
  result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  return result;
}

TargetSendResult HttpTargetHandler::executeSingleRequest(
    const AlarmMessage &alarm, const json &config, const std::string &url) {

  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName(config);
  result.success = false;

  try {
    // ✅ 클라이언트 획득 (캐시에서 가져오거나 생성)
    auto client = getOrCreateClient(config, url);
    if (!client) {
      result.error_message = "HTTP 클라이언트 생성 실패";
      return result;
    }

    // HTTP 메서드 및 엔드포인트
    std::string method = config.value("method", "POST");
    std::transform(method.begin(), method.end(), method.begin(), ::toupper);

    std::string endpoint = config.value("endpoint", url);

    // 요청 구성
    auto headers = buildRequestHeaders(config);
    std::string request_body = buildRequestBody(alarm, config);

    // HTTP 요청 실행
    Client::HttpResponse response;
    if (method == "POST") {
      response = client->post(endpoint, request_body,
                              "application/json; charset=utf-8", headers);
    } else if (method == "PUT") {
      response = client->put(endpoint, request_body,
                             "application/json; charset=utf-8", headers);
    } else if (method == "PATCH") {
      LogManager::getInstance().Warn("PATCH는 PUT으로 대체됨");
      response = client->put(endpoint, request_body,
                             "application/json; charset=utf-8", headers);
    } else {
      response = client->get(endpoint, headers);
    }

    // 결과 처리
    result.success = response.isSuccess();
    result.status_code = response.status_code;
    result.response_body = response.body;

    if (!result.success) {
      result.error_message = "HTTP " + std::to_string(response.status_code) +
                             ": " + response.body.substr(0, 200);
    }

    return result;

  } catch (const std::exception &e) {
    result.error_message = "HTTP 요청 예외: " + std::string(e.what());
    return result;
  }
}

TargetSendResult
HttpTargetHandler::executeWithRetry(const std::vector<ValueMessage> &values,
                                    const json &config,
                                    const std::string &url) {

  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName(config);
  result.success = false;

  RetryConfig retry_config;
  if (config.contains("max_retry")) {
    retry_config.max_attempts = config["max_retry"].get<int>();
  }

  auto start_time = std::chrono::steady_clock::now();

  for (int attempt = 0; attempt <= retry_config.max_attempts; ++attempt) {
    if (attempt > 0) {
      uint32_t delay_ms = calculateBackoffDelay(attempt - 1, retry_config);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    auto attempt_result = executeSingleRequest(values, config, url);

    if (attempt_result.success) {
      result = attempt_result;
      auto end_time = std::chrono::steady_clock::now();
      result.response_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);
      return result;
    }

    if (attempt_result.status_code >= 400 && attempt_result.status_code < 500) {
      result = attempt_result;
      break;
    }
    result = attempt_result;
  }

  auto end_time = std::chrono::steady_clock::now();
  result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  return result;
}

TargetSendResult
HttpTargetHandler::executeSingleRequest(const std::vector<ValueMessage> &values,
                                        const json &config,
                                        const std::string &url) {

  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName(config);
  result.success = false;

  try {
    auto client = getOrCreateClient(config, url);
    if (!client) {
      result.error_message = "HTTP 클라이언트 생성 실패";
      return result;
    }

    std::string method = config.value("method", "POST");
    std::string endpoint = config.value("endpoint", url);
    auto headers = buildRequestHeaders(config);
    std::string request_body = buildRequestBody(values, config);

    Client::HttpResponse response;
    if (method == "POST") {
      response = client->post(endpoint, request_body,
                              "application/json; charset=utf-8", headers);
    } else if (method == "PUT") {
      response = client->put(endpoint, request_body,
                             "application/json; charset=utf-8", headers);
    } else {
      response = client->get(endpoint, headers);
    }

    result.success = response.isSuccess();
    result.status_code = response.status_code;
    result.response_body = response.body;

    if (!result.success) {
      result.error_message = "HTTP " + std::to_string(response.status_code) +
                             ": " + response.body.substr(0, 200);
    }

    return result;
  } catch (const std::exception &e) {
    result.error_message = "HTTP 요청 예외: " + std::string(e.what());
    return result;
  }
}

std::unordered_map<std::string, std::string>
HttpTargetHandler::buildRequestHeaders(const json &config) {
  std::unordered_map<std::string, std::string> headers;

  headers["Accept"] = "application/json";
  headers["User-Agent"] = config.value("user_agent", "iCos5/1.1");
  headers["X-Request-ID"] = generateRequestId();
  headers["X-Timestamp"] = getCurrentTimestamp();

  // 인증 헤더
  if (config.contains("auth") && config["auth"].is_object()) {
    const json &auth = config["auth"];
    std::string auth_type = auth.value("type", "none");

    if (auth_type == "bearer" && auth.contains("token")) {
      headers["Authorization"] = "Bearer " + auth["token"].get<std::string>();
    } else if (auth_type == "basic" && auth.contains("username") &&
               auth.contains("password")) {
      std::string credentials = auth["username"].get<std::string>() + ":" +
                                auth["password"].get<std::string>();
      headers["Authorization"] = "Basic " + base64Encode(credentials);
    } else if (auth_type == "api_key" && auth.contains("key")) {
      std::string header_name = auth.value("header", "x-api-key");
      headers[header_name] = auth["key"].get<std::string>();
    }
  }

  // 사용자 정의 헤더
  if (config.contains("headers") && config["headers"].is_object()) {
    for (auto &[key, value] : config["headers"].items()) {
      if (value.is_string()) {
        headers[key] = value.get<std::string>();
      }
    }
  }

  return headers;
}

std::string HttpTargetHandler::buildRequestBody(const AlarmMessage &alarm,
                                                const json &config) {
  json request_body;

  // ✅ 템플릿이 있으면 템플릿을 기반으로 생성 (기본 필드 무시)
  if (config.contains("body_template")) {
    json template_json = config["body_template"];
    expandTemplateVariables(template_json, alarm);
    return template_json.dump();
  }

  // ✅ 템플릿이 없으면 기본 AlarmMessage 포맷 사용
  request_body["bd"] = alarm.bd;
  request_body["nm"] = alarm.nm;
  request_body["vl"] = alarm.vl;
  request_body["tm"] = alarm.tm;
  request_body["al"] = alarm.al;
  request_body["st"] = alarm.st;
  request_body["des"] = alarm.des;

  return json::array({request_body}).dump();
}

std::string
HttpTargetHandler::buildRequestBody(const std::vector<ValueMessage> &values,
                                    const json &config) {
  json request_body = json::array();

  for (const auto &val : values) {
    if (config.contains("body_template")) {
      json item = config["body_template"];
      expandTemplateVariables(item, val);

      // 개별 아이템이 배열이면 (예: [[...], [...]]), 그대로 추가할지 고민 필요
      // 일반적으로는 개별 아이템을 객체로 기대함. 일단 그대로 추가.
      request_body.push_back(item);
    } else {
      request_body.push_back(val.to_json());
    }
  }

  return request_body.dump();
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

uint32_t
HttpTargetHandler::calculateBackoffDelay(int attempt,
                                         const RetryConfig &config) const {
  double delay =
      config.initial_delay_ms * std::pow(config.backoff_multiplier, attempt);
  delay = std::min(delay, static_cast<double>(config.max_delay_ms));

  // 지터 추가 (±20%)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.8, 1.2);
  delay *= dis(gen);

  return static_cast<uint32_t>(delay);
}

std::string HttpTargetHandler::getTargetName(const json &config) const {
  if (config.contains("name") && config["name"].is_string()) {
    return config["name"].get<std::string>();
  }
  return extractUrl(config);
}

std::string HttpTargetHandler::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  // Using AlarmMessage's format for consistency with C# legacy
  // 2024-01-29 15:00:00.000 (Local time or UTC depending on preference, here
  // using UTC for headers usually, but user asked for specific format
  // verification) Let's stick to the exact format requested: "2024-01-29
  // 15:00:00.000" Note: AlarmMessage::time_to_csharp_format handles ms and
  // formatting. We'll use UTC (false) for headers to avoid TZ confusion unless
  // specified otherwise.
  // C# 명세에 따라 T/Z 없는 포맷 사용: yyyy-MM-dd HH:mm:ss.fff
  return AlarmMessage::time_to_csharp_format(now, true);
}

std::string HttpTargetHandler::generateRequestId() const {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  return "req_" + std::to_string(ms.count());
}

// ✅ PayloadTransformer 포함 (상단으로 이동됨)

void HttpTargetHandler::expandTemplateVariables(
    json &template_json, const AlarmMessage &alarm) const {
  try {
    // 기존 알람 메시지 필드값 준비
    std::string target_field_name = ""; // 값 없음 (알람 메시지 내 사용 안함)
    std::string target_description = "";
    std::string converted_value = std::to_string(alarm.vl);

    // ✅ PayloadTransformer 인스턴스 참조 (Singleton)
    auto &transformer =
        ::PulseOne::Transform::PayloadTransformer::getInstance();

    auto context = transformer.createContext(
        alarm, target_field_name, target_description, converted_value);

    // ✅ 변환 수행 (재귀적으로 처리됨)
    template_json = transformer.transform(template_json, context);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("PayloadTransformer 변환 실패: " +
                                    std::string(e.what()));
  }
}

void HttpTargetHandler::expandTemplateVariables(
    json &template_json, const ValueMessage &value) const {
  try {
    std::string target_field_name = "";
    std::string target_description = "";
    std::string converted_value = value.vl;

    auto &transformer =
        ::PulseOne::Transform::PayloadTransformer::getInstance();
    auto context = transformer.createContext(
        value, target_field_name, target_description, converted_value);

    template_json = transformer.transform(template_json, context);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("PayloadTransformer(Value) 변환 실패: " +
                                    std::string(e.what()));
  }
}

std::string HttpTargetHandler::base64Encode(const std::string &input) const {
  const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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