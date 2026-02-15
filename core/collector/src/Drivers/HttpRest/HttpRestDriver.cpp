// =============================================================================
// HTTP/REST Protocol Driver Implementation
// Configuration-driven REST API integration with JSONPath support
// =============================================================================

#include "Drivers/HttpRest/HttpRestDriver.h"
#include "Common/Enums.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <iostream>
#if HAS_CURL
#include <curl/curl.h>
#endif
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// Implementation
// =============================================================================

// =============================================================================
// CURL Context (PIMPL)
// =============================================================================
struct HttpRestDriver::CurlContext {
#if HAS_CURL
  CURL *curl_handle;
  struct curl_slist *headers_list;
#endif
  std::string response_buffer;

  CurlContext()
#if HAS_CURL
      : curl_handle(nullptr), headers_list(nullptr)
#endif
  {
  }

  ~CurlContext() {
#if HAS_CURL
    if (headers_list) {
      curl_slist_free_all(headers_list);
    }
    if (curl_handle) {
      curl_easy_cleanup(curl_handle);
    }
#endif
  }
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

HttpRestDriver::HttpRestDriver()
    : endpoint_url_(), http_method_("GET"), headers_(),
      request_body_template_(), timeout_ms_(5000), retry_count_(3),
      is_connected_(false), status_(DriverStatus::UNINITIALIZED),
      driver_statistics_("HTTP_REST"), curl_context_(nullptr) {
  LogManager::getInstance().Info("[HTTP/REST] Driver created");
}

HttpRestDriver::~HttpRestDriver() {
  Stop();
  Disconnect();
  CleanupHttpClient();
  LogManager::getInstance().Info("[HTTP/REST] Driver destroyed");
}

// =============================================================================
// IProtocolDriver Interface Implementation
// =============================================================================

bool HttpRestDriver::Initialize(const DriverConfig &config) {
  try {
    LogManager::getInstance().Info("[HTTP/REST] Initializing driver");

    if (!ParseDriverConfig(config)) {
      SetError("Failed to parse driver configuration");
      return false;
    }

    if (!InitializeHttpClient()) {
      SetError("Failed to initialize HTTP client");
      return false;
    }

    status_ = DriverStatus::INITIALIZED;
    LogManager::getInstance().Info(
        "[HTTP/REST] Driver initialized successfully");
    return true;

  } catch (const std::exception &e) {
    SetError("Exception during initialization: " + std::string(e.what()));
    return false;
  }
}

bool HttpRestDriver::Connect() {
  try {
    // For HTTP/REST, "connection" means verifying endpoint reachability
    LogManager::getInstance().Info("[HTTP/REST] Testing connection to: " +
                                   endpoint_url_);

    auto start = high_resolution_clock::now();
    std::string response = ExecuteHttpRequest(endpoint_url_, "HEAD");
    auto end = high_resolution_clock::now();

    double duration_ms = duration_cast<milliseconds>(end - start).count();

    if (!response.empty()) {
      is_connected_ = true;
      status_ = DriverStatus::RUNNING;
      driver_statistics_.successful_connections.fetch_add(1);
      LogManager::getInstance().Info("[HTTP/REST] Connected successfully (" +
                                     std::to_string(duration_ms) + "ms)");
      return true;
    }

#if HAS_CURL
    if (curl_context_->curl_handle) {
      is_connected_ = true;
      status_ = DriverStatus::RUNNING;
      driver_statistics_.successful_connections.fetch_add(1);
      LogManager::getInstance().Info(
          "[HTTP/REST] Connected successfully (Prepared)");
      return true;
    }
#endif

    SetError("Failed to connect to endpoint");
    driver_statistics_.failed_connections.fetch_add(1);
    return false;

  } catch (const std::exception &e) {
    SetError("Connection exception: " + std::string(e.what()));
    return false;
  }
}

bool HttpRestDriver::Disconnect() {
  is_connected_ = false;
  status_ = DriverStatus::STOPPED;
  LogManager::getInstance().Info("[HTTP/REST] Disconnected");
  return true;
}

bool HttpRestDriver::IsConnected() const { return is_connected_.load(); }

bool HttpRestDriver::Start() {
  if (status_ == DriverStatus::RUNNING) {
    return true;
  }

  status_ = DriverStatus::RUNNING;
  LogManager::getInstance().Info("[HTTP/REST] Driver started");
  return true;
}

bool HttpRestDriver::Stop() {
  status_ = DriverStatus::STOPPED;
  LogManager::getInstance().Info("[HTTP/REST] Driver stopped");
  return true;
}

DriverStatus HttpRestDriver::GetStatus() const { return status_.load(); }

ErrorInfo HttpRestDriver::GetLastError() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_;
}

const DriverStatistics &HttpRestDriver::GetStatistics() const {
  return driver_statistics_;
}

void HttpRestDriver::ResetStatistics() {
  driver_statistics_.ResetStatistics();
  LogManager::getInstance().Info("[HTTP/REST] Statistics reset");
}

std::string HttpRestDriver::GetProtocolType() const { return "HTTP_REST"; }

// =============================================================================
// Core Functionality
// =============================================================================

bool HttpRestDriver::ReadValues(const std::vector<DataPoint> &points,
                                std::vector<TimestampedValue> &values) {
  if (!IsConnected()) {
    SetError("Driver not connected");
    return false;
  }

  try {
    auto start = high_resolution_clock::now();

    // Execute HTTP request
    std::string response = ExecuteHttpRequest(endpoint_url_, http_method_);

    auto end = high_resolution_clock::now();
    double duration_ms = duration_cast<milliseconds>(end - start).count();

    if (response.empty()) {
      UpdateStatistics("read", false, duration_ms);
      return false;
    }

    // Extract values using JSONPath
    values.clear();
    values.reserve(points.size());

    for (const auto &point : points) {
      TimestampedValue tv;
      tv.point_id = std::stoi(point.id);
      tv.timestamp = Utils::GetCurrentTimestamp();
      tv.quality = DataQuality::GOOD;

      // Get JSONPath from point configuration
      std::string jsonpath = point.address_string; // e.g., "$.data.temperature"
      std::string data_type = "double";            // Default

      if (point.protocol_params.count("jsonpath")) {
        jsonpath = point.protocol_params.at("jsonpath");
      }
      if (point.protocol_params.count("data_type")) {
        data_type = point.protocol_params.at("data_type");
      }

      // Extract value from JSON response
      tv.value = ExtractValueFromJson(response, jsonpath, data_type);
      values.push_back(tv);
    }

    UpdateStatistics("read", true, duration_ms);
    return true;

  } catch (const std::exception &e) {
    SetError("Read exception: " + std::string(e.what()));
    UpdateStatistics("read", false);
    return false;
  }
}

bool HttpRestDriver::WriteValue(const DataPoint &point,
                                const DataValue &value) {
  if (!IsConnected()) {
    SetError("Driver not connected");
    return false;
  }

  try {
    auto start = high_resolution_clock::now();

    // Build request body
    std::string body = BuildRequestBody(point, value);

    // Execute HTTP request (POST/PUT)
    std::string method = http_method_;
    if (point.protocol_params.count("http_method")) {
      method = point.protocol_params.at("http_method");
    }

    std::string url = endpoint_url_;
    if (point.protocol_params.count("endpoint")) {
      url = point.protocol_params.at("endpoint");
    }

    std::string response = ExecuteHttpRequest(url, method, body);

    auto end = high_resolution_clock::now();
    double duration_ms = duration_cast<milliseconds>(end - start).count();

    bool success = !response.empty();
    UpdateStatistics("write", success, duration_ms);

    return success;

  } catch (const std::exception &e) {
    SetError("Write exception: " + std::string(e.what()));
    UpdateStatistics("write", false);
    return false;
  }
}

// =============================================================================
// Helper Methods
// =============================================================================

bool HttpRestDriver::ParseDriverConfig(const DriverConfig &config) {
  endpoint_url_ = config.endpoint;

  // Parse properties for HTTP-specific settings
  if (config.properties.count("http_method")) {
    http_method_ = config.properties.at("http_method");
  }

  if (config.properties.count("timeout_ms")) {
    timeout_ms_ = std::stoi(config.properties.at("timeout_ms"));
  }

  if (config.properties.count("retry_count")) {
    retry_count_ = std::stoi(config.properties.at("retry_count"));
  }

  // Parse headers (JSON format)
  if (config.properties.count("headers")) {
    try {
      json headers_json = json::parse(config.properties.at("headers"));
      for (auto &[key, value] : headers_json.items()) {
        headers_[key] = value.get<std::string>();
      }
    } catch (...) {
      LogManager::getInstance().Warn(
          "[HTTP/REST] Failed to parse headers JSON");
    }
  }

  if (config.properties.count("request_body")) {
    request_body_template_ = config.properties.at("request_body");
  }

  return true;
}

bool HttpRestDriver::InitializeHttpClient() {
  curl_context_ = std::make_unique<CurlContext>();

#if HAS_CURL
  curl_context_->curl_handle = curl_easy_init();

  if (!curl_context_->curl_handle) {
    return false;
  }

  // Set timeout
  curl_easy_setopt(curl_context_->curl_handle, CURLOPT_TIMEOUT_MS, timeout_ms_);

  // Set write callback
  curl_easy_setopt(curl_context_->curl_handle, CURLOPT_WRITEFUNCTION,
                   WriteCallback);
  curl_easy_setopt(curl_context_->curl_handle, CURLOPT_WRITEDATA,
                   &curl_context_->response_buffer);

  // Set headers
  for (const auto &[key, value] : headers_) {
    std::string header = key + ": " + value;
    curl_context_->headers_list =
        curl_slist_append(curl_context_->headers_list, header.c_str());
  }

  if (curl_context_->headers_list) {
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_HTTPHEADER,
                     curl_context_->headers_list);
  }
  return true;
#else
  LogManager::getInstance().Warn(
      "[HTTP/REST] CURL not supported in this build");
  return true; // Return true to allow initialization without CURL
#endif
}

void HttpRestDriver::CleanupHttpClient() { curl_context_.reset(); }

std::string HttpRestDriver::ExecuteHttpRequest(const std::string &url,
                                               const std::string &method,
                                               const std::string &body) {
#if HAS_CURL
  if (!curl_context_ || !curl_context_->curl_handle) {
    return "";
  }

  curl_context_->response_buffer.clear();

  // Set URL
  curl_easy_setopt(curl_context_->curl_handle, CURLOPT_URL, url.c_str());

  // Set method
  if (method == "POST") {
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_POSTFIELDS,
                     body.c_str());
  } else if (method == "PUT") {
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_POSTFIELDS,
                     body.c_str());
  } else if (method == "DELETE") {
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_CUSTOMREQUEST,
                     "DELETE");
  } else if (method == "HEAD") {
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_NOBODY, 1L);
  } else {
    // GET (default)
    curl_easy_setopt(curl_context_->curl_handle, CURLOPT_HTTPGET, 1L);
  }

  // Execute with retry logic
  CURLcode res = CURLE_FAILED_INIT;
  for (int attempt = 0; attempt < retry_count_; ++attempt) {
    res = curl_easy_perform(curl_context_->curl_handle);

    if (res == CURLE_OK) {
      break;
    }

    // Exponential backoff
    if (attempt < retry_count_ - 1) {
      int delay_ms = static_cast<int>(std::pow(2, attempt) * 100);
      std::this_thread::sleep_for(milliseconds(delay_ms));
    }
  }

  if (res != CURLE_OK) {
    SetError("CURL error: " + std::string(curl_easy_strerror(res)));
    return "";
  }

  return curl_context_->response_buffer;
#else
  return "";
#endif
}

DataValue HttpRestDriver::ExtractValueFromJson(const std::string &json_response,
                                               const std::string &jsonpath,
                                               const std::string &data_type) {
  try {
    json j = json::parse(json_response);

    // Simple JSONPath implementation (supports basic paths like $.field or
    // $.nested.field)
    json value = j;

    // Remove leading $. if present
    std::string path = jsonpath;
    if (path.substr(0, 2) == "$.") {
      path = path.substr(2);
    }

    // Split path by '.'
    std::istringstream iss(path);
    std::string token;
    while (std::getline(iss, token, '.')) {
      if (!token.empty()) {
        value = value[token];
      }
    }

    // Convert to appropriate type
    if (data_type == "string") {
      return value.get<std::string>();
    } else if (data_type == "int") {
      return value.get<int>();
    } else if (data_type == "bool") {
      return value.get<bool>();
    } else {
      // Default: double
      return value.get<double>();
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("[HTTP/REST] JSON extraction failed: " +
                                    std::string(e.what()));
    return 0.0; // Default value
  }
}

std::string HttpRestDriver::BuildRequestBody(const DataPoint &point,
                                             const DataValue &value) {
  // Use template if provided
  if (!request_body_template_.empty()) {
    std::string body = request_body_template_;

    // Replace placeholders
    // {value}, {name}, {id}, etc.
    std::string value_str;
    if (std::holds_alternative<double>(value)) {
      value_str = std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<int>(value)) {
      value_str = std::to_string(std::get<int>(value));
    } else if (std::holds_alternative<std::string>(value)) {
      value_str = "\"" + std::get<std::string>(value) + "\"";
    } else if (std::holds_alternative<bool>(value)) {
      value_str = std::get<bool>(value) ? "true" : "false";
    }

    size_t pos = body.find("{value}");
    if (pos != std::string::npos) {
      body.replace(pos, 7, value_str);
    }

    return body;
  }

  // Default: simple JSON
  json j;
  j["name"] = point.name;

  // Convert DataValue to appropriate JSON type
  if (std::holds_alternative<double>(value)) {
    j["value"] = std::get<double>(value);
  } else if (std::holds_alternative<int>(value)) {
    j["value"] = std::get<int>(value);
  } else if (std::holds_alternative<std::string>(value)) {
    j["value"] = std::get<std::string>(value);
  } else if (std::holds_alternative<bool>(value)) {
    j["value"] = std::get<bool>(value);
  }

  return j.dump();
}

void HttpRestDriver::SetError(const std::string &message,
                              Structs::ErrorCode code) {
  std::lock_guard<std::mutex> lock(error_mutex_);
  last_error_.code = code;
  last_error_.message = message;
  status_ = DriverStatus::DRIVER_ERROR;
  LogManager::getInstance().Error("[HTTP/REST] " + message);
}

void HttpRestDriver::UpdateStatistics(const std::string &operation,
                                      bool success, double duration_ms) {
  if (operation == "read") {
    driver_statistics_.total_reads.fetch_add(1);
    if (success) {
      driver_statistics_.successful_reads.fetch_add(1);
    } else {
      driver_statistics_.failed_reads.fetch_add(1);
    }
  } else if (operation == "write") {
    driver_statistics_.total_writes.fetch_add(1);
    if (success) {
      driver_statistics_.successful_writes.fetch_add(1);
    } else {
      driver_statistics_.failed_writes.fetch_add(1);
    }
  }

  (void)duration_ms; // Suppress warning
}

// =============================================================================
// CURL Callback
// =============================================================================

size_t HttpRestDriver::WriteCallback(void *contents, size_t size, size_t nmemb,
                                     void *userp) {
  size_t total_size = size * nmemb;
  std::string *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), total_size);
  return total_size;
}
} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Entry Point
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void RegisterPlugin() {
  // 1. DB에 프로토콜 정보 자동 등록 (없을 경우)
  try {
    auto &repo_factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();
    if (protocol_repo) {
      if (!protocol_repo->findByType("HTTP_REST").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("HTTP_REST");
        entity.setDisplayName("HTTP REST");
        entity.setCategory("web");
        entity.setDescription("HTTP REST API Protocol Driver");
        entity.setDefaultPort(80);
        protocol_repo->save(entity);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[HttpRestDriver] DB Registration failed: " << e.what()
              << std::endl;
  }

  // 2. 메모리 Factory에 드라이버 생성자 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "HTTP_REST",
      []() { return std::make_unique<PulseOne::Drivers::HttpRestDriver>(); });
  std::cout << "[HttpRestDriver] Plugin Registered Successfully" << std::endl;
}

// Legacy wrapper for static linking
void RegisterHttpDriver() { RegisterPlugin(); }
}
#endif
