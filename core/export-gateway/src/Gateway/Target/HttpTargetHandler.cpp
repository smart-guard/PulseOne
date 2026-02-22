/**
 * @file HttpTargetHandler.cpp
 * @brief HTTP/HTTPS Target Handler implementation - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#include "Gateway/Target/HttpTargetHandler.h"
#include "Client/HttpClient.h"
#include "Constants/ExportConstants.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Logging/LogManager.h"
#include "Security/SecretManager.h"
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
namespace Gateway {
namespace Target {

namespace HttpConst = PulseOne::Constants::Export::Http;

// Static Client Cache
static Utils::ClientCacheManager<Client::HttpClient,
                                 Client::HttpRequestOptions> &
getHttpClientCache() {
  static Utils::ClientCacheManager<Client::HttpClient,
                                   Client::HttpRequestOptions>
      cache(
          [](const Client::HttpRequestOptions &options) {
            return std::make_shared<Client::HttpClient>("", options);
          },
          300);
  return cache;
}

HttpTargetHandler::HttpTargetHandler() {
  LogManager::getInstance().Info(
      "HttpTargetHandler initialized (Gateway::Target)");
}

HttpTargetHandler::~HttpTargetHandler() {
  LogManager::getInstance().Info("HttpTargetHandler destroyed");
}

bool HttpTargetHandler::initialize(const json &config) {
  target_name_ = config.value("name", "HTTP_Target");
  target_type_ = "HTTP";
  std::vector<std::string> errors;
  return validateConfig(config, errors);
}

TargetSendResult HttpTargetHandler::sendAlarm(
    const json &payload, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) {
  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName();
  result.success = false;

  try {
    std::string url = extractUrl(config);
    if (url.empty()) {
      result.error_message = "URL not configured";
      return result;
    }

    result = executeWithRetry(payload, alarm, config, url);
    if (result.success)
      success_count_++;
    else
      failure_count_++;
    request_count_++;
  } catch (const std::exception &e) {
    result.error_message = std::string("HTTP exception: ") + e.what();
    failure_count_++;
  }

  return result;
}

TargetSendResult HttpTargetHandler::sendValue(
    const json &payload, const PulseOne::Gateway::Model::ValueMessage &value,
    const json &config) {
  TargetSendResult result;
  result.target_type = "HTTP";
  result.target_name = getTargetName();
  result.success = false;

  try {
    std::string url = extractUrl(config);
    if (url.empty()) {
      result.error_message = "URL not configured";
      return result;
    }

    result = executeWithRetry(payload, value, config, url);
    if (result.success)
      success_count_++;
    else
      failure_count_++;
    request_count_++;
  } catch (const std::exception &e) {
    result.error_message = std::string("HTTP exception: ") + e.what();
    failure_count_++;
  }

  return result;
}

std::vector<TargetSendResult> HttpTargetHandler::sendValueBatch(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config) {

  std::vector<TargetSendResult> results;
  if (values.empty())
    return results;

  try {
    std::string url = extractUrl(config);
    if (url.empty()) {
      TargetSendResult res;
      res.error_message = "URL not configured";
      results.push_back(res);
      return results;
    }

    TargetSendResult result = executeWithRetry(payloads, values, config, url);
    results.push_back(result);
    if (result.success)
      success_count_++;
    else
      failure_count_++;
    request_count_++;
  } catch (const std::exception &e) {
    TargetSendResult err_res;
    err_res.error_message = std::string("Batch exception: ") + e.what();
    results.push_back(err_res);
    failure_count_++;
  }

  return results;
}

bool HttpTargetHandler::testConnection(const json &config) {
  try {
    std::string url = extractUrl(config);
    if (url.empty())
      return false;

    auto client = getOrCreateClient(config, url);
    if (!client)
      return false;

    auto headers = buildRequestHeaders(config);
    auto response =
        client->get(config.value("test_endpoint", "/health"), headers);
    return response.isSuccess();
  } catch (...) {
    return false;
  }
}

bool HttpTargetHandler::validateConfig(const json &config,
                                       std::vector<std::string> &errors) {
  if (!config.contains("endpoint") && !config.contains("url")) {
    errors.push_back("endpoint OR url is required");
    return false;
  }
  return true;
}

json HttpTargetHandler::getStatus() const {
  return json{{"type", "HTTP"},
              {"request_count", request_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()}};
}

void HttpTargetHandler::cleanup() { getHttpClientCache().clear(); }

// Private methods (Logic mapped from original CSP::HttpTargetHandler)
std::shared_ptr<Client::HttpClient>
HttpTargetHandler::getOrCreateClient(const json &config,
                                     const std::string &url) {
  Client::HttpRequestOptions options;
  options.timeout_sec = config.value("timeout_sec", 30);
  options.connect_timeout_sec = config.value("connect_timeout_sec", 10);
  options.verify_ssl = config.value("verify_ssl", true);

  return getHttpClientCache().getOrCreate(url, options);
}

std::string HttpTargetHandler::extractUrl(const json &config) const {
  std::string url = config.value("endpoint", config.value("url", ""));
  if (!url.empty()) {
    return ConfigManager::getInstance().expandVariables(url);
  }
  return "";
}

// ─── Retry helper (shared by all overloads) ──────────────────────────────────
namespace {

RetryConfig loadRetryConfig(const nlohmann::json &config) {
  RetryConfig rc;
  rc.max_attempts = config.value("retry_max_attempts", rc.max_attempts);
  rc.initial_delay_ms =
      config.value("retry_initial_delay_ms", rc.initial_delay_ms);
  rc.max_delay_ms = config.value("retry_max_delay_ms", rc.max_delay_ms);
  rc.backoff_multiplier =
      config.value("retry_backoff_multiplier", rc.backoff_multiplier);
  return rc;
}

bool shouldRetry(const PulseOne::Export::TargetSendResult &r) {
  // Retry on network errors (status 0) or 5xx server errors
  return !r.success &&
         (r.status_code == 0 || (r.status_code >= 500 && r.status_code < 600));
}

} // anonymous namespace
// ─────────────────────────────────────────────────────────────────────────────

TargetSendResult HttpTargetHandler::executeWithRetry(
    const json &payload, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config, const std::string &url) {
  RetryConfig rc = loadRetryConfig(config);
  TargetSendResult result;
  uint32_t delay_ms = rc.initial_delay_ms;

  for (int attempt = 1; attempt <= rc.max_attempts; ++attempt) {
    result = executeSingleRequest(payload, alarm, config, url);
    if (!shouldRetry(result))
      break;

    if (attempt < rc.max_attempts) {
      LogManager::getInstance().Warn(
          "[RETRY] Alarm send failed (attempt " + std::to_string(attempt) +
          "/" + std::to_string(rc.max_attempts) +
          "), HTTP=" + std::to_string(result.status_code) + ". Retrying in " +
          std::to_string(delay_ms) + "ms...");
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      delay_ms =
          std::min(static_cast<uint32_t>(delay_ms * rc.backoff_multiplier),
                   rc.max_delay_ms);
    }
  }
  return result;
}

TargetSendResult HttpTargetHandler::executeWithRetry(
    const json &payload, const PulseOne::Gateway::Model::ValueMessage &value,
    const json &config, const std::string &url) {
  RetryConfig rc = loadRetryConfig(config);
  TargetSendResult result;
  uint32_t delay_ms = rc.initial_delay_ms;

  for (int attempt = 1; attempt <= rc.max_attempts; ++attempt) {
    result = executeSingleRequest(payload, value, config, url);
    if (!shouldRetry(result))
      break;

    if (attempt < rc.max_attempts) {
      LogManager::getInstance().Warn(
          "[RETRY] Value send failed (attempt " + std::to_string(attempt) +
          "/" + std::to_string(rc.max_attempts) +
          "), HTTP=" + std::to_string(result.status_code) + ". Retrying in " +
          std::to_string(delay_ms) + "ms...");
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      delay_ms =
          std::min(static_cast<uint32_t>(delay_ms * rc.backoff_multiplier),
                   rc.max_delay_ms);
    }
  }
  return result;
}

TargetSendResult HttpTargetHandler::executeWithRetry(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config, const std::string &url) {
  RetryConfig rc = loadRetryConfig(config);
  TargetSendResult result;
  uint32_t delay_ms = rc.initial_delay_ms;

  for (int attempt = 1; attempt <= rc.max_attempts; ++attempt) {
    result = executeSingleRequest(payloads, values, config, url);
    if (!shouldRetry(result))
      break;

    if (attempt < rc.max_attempts) {
      LogManager::getInstance().Warn(
          "[RETRY] Batch send failed (attempt " + std::to_string(attempt) +
          "/" + std::to_string(rc.max_attempts) +
          "), HTTP=" + std::to_string(result.status_code) + ". Retrying in " +
          std::to_string(delay_ms) + "ms...");
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      delay_ms =
          std::min(static_cast<uint32_t>(delay_ms * rc.backoff_multiplier),
                   rc.max_delay_ms);
    }
  }
  return result;
}

TargetSendResult HttpTargetHandler::executeSingleRequest(
    const json &payload, const PulseOne::Gateway::Model::ValueMessage &value,
    const json &config, const std::string &url) {
  TargetSendResult result;
  auto client = getOrCreateClient(config, url);
  if (!client)
    return result;

  auto headers = buildRequestHeaders(config);
  std::string body = buildRequestBody(payload, value, config);

  auto response =
      client->post(url, body, HttpConst::CONTENT_TYPE_JSON_UTF8, headers);
  result.success = response.isSuccess();
  result.status_code = response.status_code;
  result.sent_payload = body;
  result.response_body = response.body;
  return result;
}

TargetSendResult HttpTargetHandler::executeSingleRequest(
    const json &payload, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config, const std::string &url) {
  TargetSendResult result;
  auto client = getOrCreateClient(config, url);
  if (!client)
    return result;

  auto headers = buildRequestHeaders(config);
  std::string body = buildRequestBody(payload, alarm, config);

  LogManager::getInstance().Info(
      "[TRACE-4-SEND] HTTP Request Body: " +
      Security::SecretManager::getInstance().maskSensitiveJson(body));

  auto response =
      client->post(url, body, HttpConst::CONTENT_TYPE_JSON_UTF8, headers);
  result.success = response.isSuccess();
  result.status_code = response.status_code;
  result.sent_payload = body;
  result.response_body = response.body;
  return result;
}

TargetSendResult HttpTargetHandler::executeSingleRequest(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config, const std::string &url) {
  TargetSendResult result;
  auto client = getOrCreateClient(config, url);
  if (!client)
    return result;

  auto headers = buildRequestHeaders(config);
  std::string body = buildRequestBody(payloads, values, config);

  auto response =
      client->post(url, body, HttpConst::CONTENT_TYPE_JSON_UTF8, headers);
  result.success = response.isSuccess();
  result.status_code = response.status_code;
  result.sent_payload = body;
  result.response_body = response.body;
  return result;
}

std::unordered_map<std::string, std::string>
HttpTargetHandler::buildRequestHeaders(const json &config) {
  std::unordered_map<std::string, std::string> headers;
  headers[HttpConst::HEADER_ACCEPT] = HttpConst::CONTENT_TYPE_JSON;
  // [Modified] User request: Make User-Agent configurable (Default:
  // PulseOne-ExportGateway/1.0)
  headers[HttpConst::HEADER_USER_AGENT] =
      config.value("user_agent", "iCos5/1.1");

  // Custom headers support with Secret expansion
  if (config.contains("headers") && config["headers"].is_object()) {
    auto &config_manager = ConfigManager::getInstance();
    // SecretManager usage depends on how secrets are exposed.
    // Typically expandVariables resolves ${SECRET_REF} to the actual secret via
    // ConfigManager::getSecret logic internally or we might need explicit
    // decrypt calls if the value itself is encrypted string "ENC(...)". For
    // now, consistent with S3, we rely on expandVariables or explicit handling
    // if needed.

    for (const auto &item : config["headers"].items()) {
      std::string key = item.key();
      std::string value = item.value().get<std::string>();

      // resolve(): ${VAR} 조회 + ENC: 복호화를 단일 호출로
      std::string final_value =
          Security::SecretManager::getInstance().resolve(value);

      headers[key] = final_value;
    }
  }

  // [v3.2.1] Auth support (x-api-key)
  // [v3.2.1] Auth support (x-api-key)
  LogManager::getInstance().Info("[DEBUG] config contains auth? " +
                                 std::to_string(config.contains("auth")));
  if (config.contains("auth")) {
    LogManager::getInstance().Info("[DEBUG] auth type: " +
                                   std::string(config["auth"].type_name()));
    LogManager::getInstance().Info("[DEBUG] auth dump: " +
                                   config["auth"].dump());
  }

  if (config.contains("auth") && config["auth"].is_object()) {
    json auth = config["auth"];
    std::string type = auth.value("type", "");
    LogManager::getInstance().Info("[DEBUG] Auth type: " + type);

    if (type == "x-api-key") {
      std::string key = auth.value("apiKey", "");
      if (!key.empty()) {
        std::string final_value =
            Security::SecretManager::getInstance().resolve(key);

        // Log masked final value
        std::string masked = "****";
        if (final_value.length() >= 4) {
          masked = final_value.substr(0, 2) + "..." +
                   final_value.substr(final_value.length() - 2);
        } else if (!final_value.empty()) {
          masked = final_value.substr(0, 1) + "...";
        }
        LogManager::getInstance().Info("[DEBUG] Final decrypted key: " +
                                       masked);

        headers["x-api-key"] = final_value;
      } else {
        LogManager::getInstance().Warn("[DEBUG] apiKey key is empty");
      }
    } else if (type == "bearer") {
      std::string token = auth.value("token", "");
      if (!token.empty()) {
        std::string final_value =
            Security::SecretManager::getInstance().resolve(token);
        headers["Authorization"] = "Bearer " + final_value;
      }
    } else {
      LogManager::getInstance().Warn("[DEBUG] Unknown auth type: " + type);
    }
  } else {
    LogManager::getInstance().Warn("[DEBUG] Auth block missing or not object");
  }

  return headers;
}

std::string HttpTargetHandler::buildRequestBody(
    const json &payload, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) {
  LogManager::getInstance().Info(
      "[DEBUG-HTTP] buildRequestBody(Pre-mapped) called for point: " +
      alarm.point_name);

  return payload.dump();
}

std::string HttpTargetHandler::buildRequestBody(
    const json &payload, const PulseOne::Gateway::Model::ValueMessage &value,
    const json &config) {
  return payload.dump();
}

std::string HttpTargetHandler::buildRequestBody(
    const std::vector<json> &payloads,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config) {
  LogManager::getInstance().Info(
      "[DEBUG-HTTP] buildRequestBody(Pre-mapped Batch) called. Count: " +
      std::to_string(payloads.size()));

  json arr = json::array();
  for (const auto &p : payloads) {
    arr.push_back(p);
  }
  return arr.dump();
}

uint32_t
HttpTargetHandler::calculateBackoffDelay(int attempt,
                                         const RetryConfig &config) const {
  return config.initial_delay_ms * std::pow(config.backoff_multiplier, attempt);
}

std::string HttpTargetHandler::getTargetName(const json &config) const {
  return config.value("name", "Unnamed_HTTP");
}

std::string HttpTargetHandler::getCurrentTimestamp() const {
  return PulseOne::Gateway::Model::AlarmMessage::current_time_to_csharp_format(
      true);
}

std::string HttpTargetHandler::generateRequestId() const {
  return "req_" +
         std::to_string(
             std::chrono::system_clock::now().time_since_epoch().count());
}

std::string HttpTargetHandler::base64Encode(const std::string &input) const {
  return ""; // Helper implementation
}

REGISTER_TARGET_HANDLER("HTTP", HttpTargetHandler);

} // namespace Target
} // namespace Gateway
} // namespace PulseOne
