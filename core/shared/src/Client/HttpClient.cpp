/**
 * @file HttpClient.cpp
 * @brief HTTP 클라이언트 래퍼 구현 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 */

#include "Client/HttpClient.h"
#include <chrono>
#include <regex>
#include <sstream>

#ifdef HAS_SHARED_LIBS
#include "Logging/LogManager.h"
#define LOG_ERROR(msg) LogManager::getInstance().Error(msg)
#define LOG_DEBUG(msg) LogManager::getInstance().Debug(msg)
#else
#define LOG_ERROR(msg) // 로깅 비활성화
#define LOG_DEBUG(msg) // 로깅 비활성화
#endif

namespace PulseOne {
namespace Client {

#if HAS_CURL
bool HttpClient::curl_global_initialized_ = false;
std::mutex HttpClient::curl_global_mutex_;
#endif

HttpClient::HttpClient(const std::string &base_url,
                       const HttpRequestOptions &options)
    : base_url_(base_url), options_(options),
      library_type_(HttpLibraryType::NONE) {

  initializeHttpLibrary();

  // 기본 헤더 설정
  default_headers_["User-Agent"] = options_.user_agent;
  default_headers_["Accept"] = "application/json";
  default_headers_["Connection"] = "keep-alive";
}

HttpClient::~HttpClient() {
#if HAS_CURL
  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = nullptr;
  }
#endif
}

void HttpClient::initializeHttpLibrary() {
  LOG_DEBUG("HttpClient::initializeHttpLibrary() 시작");
  LOG_DEBUG("base_url: " + base_url_);

  // httplib 우선 사용
#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
  try {
    LOG_DEBUG("Trying httplib initialization...");

    if (!base_url_.empty()) {
      auto parsed = parseUrl(base_url_);
      std::string host = parsed["host"];
      int port = std::stoi(parsed.count("port")
                               ? parsed["port"]
                               : (parsed["scheme"] == "https" ? "443" : "80"));

      LOG_DEBUG("Parsed - scheme: " + parsed["scheme"] + ", host: " + host +
                ", port: " + std::to_string(port));

      if (parsed["scheme"] == "https") {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        httplib_client_ = std::make_unique<httplib::SSLClient>(host, port);
        LOG_DEBUG("SSLClient created");
#else
        LOG_ERROR("HTTPS requested but httplib has no SSL support. Falling "
                  "back to Curl if available.");
        // continue to try curl
#endif
      } else {
        httplib_client_ = std::make_unique<httplib::Client>(host, port);
        LOG_DEBUG("Client created");
      }

      if (httplib_client_) {
        httplib_client_->set_connection_timeout(options_.connect_timeout_sec);
        httplib_client_->set_read_timeout(options_.timeout_sec);

        // set_follow_redirects is not available in very old versions
#ifdef CPPHTTPLIB_FOLLOW_LOCATION_SUPPORT
        httplib_client_->set_follow_redirects(options_.follow_redirects);
#endif

        if (parsed["scheme"] == "https" && !options_.verify_ssl) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
          auto ssl_client =
              dynamic_cast<httplib::SSLClient *>(httplib_client_.get());
          if (ssl_client) {
            ssl_client->set_ca_cert_path("");
            LOG_DEBUG("SSL verification disabled");
          }
#endif
        }

        library_type_ = HttpLibraryType::HTTPLIB;
        LOG_DEBUG("HttpClient initialized with httplib successfully");
        return;
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to initialize httplib: " + std::string(e.what()));
  }
#else
  LOG_DEBUG("HAVE_HTTPLIB not defined or disabled");
#endif

  // httplib 실패 시 curl 사용
#if HAS_CURL
  try {
    LOG_DEBUG("Trying curl initialization...");

    {
      std::lock_guard<std::mutex> lock(curl_global_mutex_);
      if (!curl_global_initialized_) {
        LOG_DEBUG("Initializing curl globally...");
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_global_initialized_ = true;
      }
    }

    std::lock_guard<std::mutex> lock(http_mutex_);
    curl_handle_ = curl_easy_init();
    if (curl_handle_) {
      library_type_ = HttpLibraryType::CURL;
      LOG_DEBUG("HttpClient initialized with curl successfully");
      return;
    } else {
      LOG_ERROR("curl_easy_init() returned nullptr");
    }
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to initialize curl: " + std::string(e.what()));
  }
#else
  LOG_DEBUG("HAS_CURL not defined");
#endif

  LOG_ERROR("No HTTP library available - HAVE_HTTPLIB="
#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
            "1"
#else
            "0"
#endif
            ", HAS_CURL="
#if HAS_CURL
            "1"
#else
            "0"
#endif
  );

  library_type_ = HttpLibraryType::NONE;
}

HttpResponse
HttpClient::get(const std::string &path,
                const std::unordered_map<std::string, std::string> &headers) {
  return executeRequest("GET", path, "", "", headers);
}

HttpResponse
HttpClient::post(const std::string &path, const std::string &body,
                 const std::string &content_type,
                 const std::unordered_map<std::string, std::string> &headers) {
  return executeRequest("POST", path, body, content_type, headers);
}

HttpResponse
HttpClient::put(const std::string &path, const std::string &body,
                const std::string &content_type,
                const std::unordered_map<std::string, std::string> &headers) {
  return executeRequest("PUT", path, body, content_type, headers);
}

HttpResponse
HttpClient::del(const std::string &path,
                const std::unordered_map<std::string, std::string> &headers) {
  return executeRequest("DELETE", path, "", "", headers);
}

HttpResponse HttpClient::executeRequest(
    const std::string &method, const std::string &path, const std::string &body,
    const std::string &content_type,
    const std::unordered_map<std::string, std::string> &headers) {

  std::lock_guard<std::mutex> lock(http_mutex_);
  auto start_time = std::chrono::high_resolution_clock::now();

  HttpResponse response;

  switch (library_type_) {
#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
  case HttpLibraryType::HTTPLIB:
    response = executeWithHttplib(method, path, body, content_type, headers);
    break;
#endif
#if HAS_CURL
  case HttpLibraryType::CURL:
    response = executeWithCurl(method, path, body, content_type, headers);
    break;
#endif
  default:
    (void)method;
    (void)path;
    (void)body;
    (void)content_type;
    (void)headers;
    response.status_code = 0;
    response.error_message = "No HTTP library available";
    break;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  response.response_time_ms = duration.count();

  LOG_DEBUG("HTTP " + method + " " + path + " -> " +
            std::to_string(response.status_code) + " (" +
            std::to_string(response.response_time_ms) + "ms)");

  return response;
}

#if defined(HAVE_HTTPLIB) && HAVE_HTTPLIB
HttpResponse HttpClient::executeWithHttplib(
    const std::string &method, const std::string &path, const std::string &body,
    const std::string &content_type,
    const std::unordered_map<std::string, std::string> &headers) {

  HttpResponse response;

  try {
    if (!httplib_client_) {
      response.error_message = "httplib client not initialized";
      return response;
    }

    httplib::Headers req_headers;
    for (const auto &header : default_headers_) {
      req_headers.insert(header);
    }
    for (const auto &header : headers) {
      req_headers.insert(header);
    }

    if (!content_type.empty()) {
      req_headers.insert({"Content-Type", content_type});
    }

    if (!options_.bearer_token.empty()) {
      req_headers.insert({"Authorization", "Bearer " + options_.bearer_token});
    } else if (!options_.username.empty()) {
      std::string auth = options_.username + ":" + options_.password;
      req_headers.insert({"Authorization", "Basic " + auth});
    }

    httplib::Result result;

    if (method == "GET") {
      result = httplib_client_->Get(path.c_str(), req_headers);
    } else if (method == "POST") {
      result = httplib_client_->Post(path.c_str(), req_headers, body,
                                     content_type.c_str());
    } else if (method == "PUT") {
      result = httplib_client_->Put(path.c_str(), req_headers, body,
                                    content_type.c_str());
    } else if (method == "DELETE") {
      result = httplib_client_->Delete(path.c_str(), req_headers);
    }

    if (result) {
      response.status_code = result->status;
      response.body = result->body;

      for (const auto &header : result->headers) {
        response.headers[header.first] = header.second;
      }
    } else {
      response.status_code = 0;
      response.error_message = "HTTP request failed";
    }

  } catch (const std::exception &e) {
    response.status_code = 0;
    response.error_message = "httplib exception: " + std::string(e.what());
  }

  return response;
}
#endif

#if HAS_CURL
HttpResponse HttpClient::executeWithCurl(
    const std::string &method, const std::string &path, const std::string &body,
    const std::string &content_type,
    const std::unordered_map<std::string, std::string> &headers) {

  HttpResponse response;

  try {
    if (!curl_handle_) {
      response.error_message = "curl handle not initialized";
      LOG_ERROR("curl handle is nullptr");
      return response;
    }

    std::string full_url;
    // path가 http:// 또는 https://로 시작하면 절대 URL로 처리
    if (path.find("http://") == 0 || path.find("https://") == 0) {
      full_url = path; // path를 그대로 사용
    } else {
      // 상대 경로면 base_url과 결합
      full_url = base_url_;
      if (!path.empty() && path[0] != '/') {
        full_url += "/";
      }
      full_url += path;
    }

    LOG_DEBUG("curl request starting");
    LOG_DEBUG("  Method: " + method);
    LOG_DEBUG("  Full URL: " + full_url);
    LOG_DEBUG("  Body size: " + std::to_string(body.length()) + " bytes");

    curl_easy_setopt(curl_handle_, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, options_.timeout_sec);
    curl_easy_setopt(curl_handle_, CURLOPT_CONNECTTIMEOUT,
                     options_.connect_timeout_sec);
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION,
                     options_.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl_handle_, CURLOPT_MAXREDIRS, options_.max_redirects);

    LOG_DEBUG("  Timeout: " + std::to_string(options_.timeout_sec) + "s");
    LOG_DEBUG("  SSL verify: " +
              std::string(options_.verify_ssl ? "true" : "false"));

    if (!options_.verify_ssl) {
      curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, 0L);
      LOG_DEBUG("  SSL verification disabled");
    }

    curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, 1L);
    LOG_DEBUG("HttpClient::executeCurlRequest - Content-Type argument: [" +
              content_type + "]");

    std::string response_body;
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_body);

    curl_easy_setopt(curl_handle_, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, &response.headers);

    if (method == "POST") {
      curl_easy_setopt(curl_handle_, CURLOPT_POST, 1L);
      if (!body.empty()) {
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, body.length());
      }
    } else if (method == "PUT") {
      curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, "PUT");
      if (!body.empty()) {
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE, body.length());
      }
    } else if (method == "DELETE") {
      curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    struct curl_slist *header_list = nullptr;
    std::unordered_map<std::string, std::string> merged_headers;

    // Helper to add header with lowercase key
    auto add_normalized = [&](const std::string &k, const std::string &v) {
      std::string lower_k = k;
      std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(),
                     ::tolower);
      merged_headers[lower_k] = v;
    };

    for (const auto &header : default_headers_) {
      add_normalized(header.first, header.second);
    }

    for (const auto &header : headers) {
      add_normalized(header.first, header.second);
    }

    if (!content_type.empty()) {
      add_normalized("content-type", content_type);
    }

    if (!options_.bearer_token.empty()) {
      add_normalized("authorization", "Bearer " + options_.bearer_token);
    }

    for (const auto &header : merged_headers) {
      std::string header_str = header.first + ": " + header.second;
      header_list = curl_slist_append(header_list, header_str.c_str());
      // [SECURITY] Mask sensitive headers
      std::string log_value = header.second;
      std::string lower_key = header.first;
      std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                     ::tolower);

      if (lower_key.find("key") != std::string::npos ||
          lower_key.find("auth") != std::string::npos ||
          lower_key.find("token") != std::string::npos ||
          lower_key.find("secret") != std::string::npos) {
        log_value = "********";
      }

      LOG_DEBUG("  Header: " + header.first + ": " + log_value);
    }

    if (!options_.username.empty() && options_.bearer_token.empty()) {
      curl_easy_setopt(curl_handle_, CURLOPT_USERNAME,
                       options_.username.c_str());
      curl_easy_setopt(curl_handle_, CURLOPT_PASSWORD,
                       options_.password.c_str());
      LOG_DEBUG("  Auth: Basic");
    }

    if (header_list) {
      curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
    }

    LOG_DEBUG("Executing curl_easy_perform...");

    CURLcode res = curl_easy_perform(curl_handle_);

    LOG_DEBUG("curl_easy_perform completed");
    LOG_DEBUG("  CURLcode: " + std::to_string(res) + " (" +
              std::string(curl_easy_strerror(res)) + ")");

    if (res == CURLE_OK) {
      long status_code;
      curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &status_code);
      response.status_code = static_cast<int>(status_code);
      response.body = response_body;

      double total_time = 0;
      curl_easy_getinfo(curl_handle_, CURLINFO_TOTAL_TIME, &total_time);

      LOG_DEBUG("Request successful");
      LOG_DEBUG("  HTTP Status: " + std::to_string(status_code));
      LOG_DEBUG("  Response size: " + std::to_string(response_body.length()) +
                " bytes");
      LOG_DEBUG("  Total time: " + std::to_string(total_time) + "s");

    } else {
      response.status_code = 0;
      response.error_message =
          "curl error: " + std::string(curl_easy_strerror(res));

      LOG_ERROR("curl request failed");
      LOG_ERROR("  Error code: " + std::to_string(res));
      LOG_ERROR("  Error message: " + std::string(curl_easy_strerror(res)));

      if (res == CURLE_COULDNT_RESOLVE_HOST) {
        LOG_ERROR("  DNS resolution failed - check hostname");
      } else if (res == CURLE_COULDNT_CONNECT) {
        LOG_ERROR("  Connection failed - check network/firewall");
      } else if (res == CURLE_OPERATION_TIMEDOUT) {
        LOG_ERROR("  Operation timeout");
      } else if (res == CURLE_SSL_CONNECT_ERROR) {
        LOG_ERROR("  SSL connection error");
      } else if (res == CURLE_URL_MALFORMAT) {
        LOG_ERROR("  Malformed URL: " + full_url);
      }
    }

    if (header_list) {
      curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, nullptr);
      curl_slist_free_all(header_list);
    }

    // 다음 요청을 위해 필드 초기화 (상태 전이 방지)
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, nullptr);
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, nullptr);

  } catch (const std::exception &e) {
    response.status_code = 0;
    response.error_message = "curl exception: " + std::string(e.what());
    LOG_ERROR("Exception in executeWithCurl: " + std::string(e.what()));
  }

  return response;
}

size_t HttpClient::curlWriteCallback(void *contents, size_t size, size_t nmemb,
                                     std::string *response) {
  size_t total_size = size * nmemb;
  response->append(static_cast<char *>(contents), total_size);
  return total_size;
}

size_t HttpClient::curlHeaderCallback(
    char *buffer, size_t size, size_t nitems,
    std::unordered_map<std::string, std::string> *headers) {
  size_t total_size = size * nitems;
  std::string header_line(buffer, total_size);

  size_t colon_pos = header_line.find(':');
  if (colon_pos != std::string::npos) {
    std::string key = header_line.substr(0, colon_pos);
    std::string value = header_line.substr(colon_pos + 1);

    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t\r\n") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);

    if (!key.empty() && !value.empty()) {
      (*headers)[key] = value;
    }
  }

  return total_size;
}
#endif

void HttpClient::setBaseUrl(const std::string &base_url) {
  base_url_ = base_url;
  initializeHttpLibrary();
}

void HttpClient::setOptions(const HttpRequestOptions &options) {
  options_ = options;
  default_headers_["User-Agent"] = options_.user_agent;
}

void HttpClient::setDefaultHeaders(
    const std::unordered_map<std::string, std::string> &headers) {
  for (const auto &header : headers) {
    default_headers_[header.first] = header.second;
  }
}

void HttpClient::setBearerToken(const std::string &token) {
  options_.bearer_token = token;
}

void HttpClient::setBasicAuth(const std::string &username,
                              const std::string &password) {
  options_.username = username;
  options_.password = password;
}

bool HttpClient::testConnection(const std::string &test_path) {
  try {
    auto response = get(test_path);
    return response.status_code > 0;
  } catch (...) {
    return false;
  }
}

std::string HttpClient::getHttpLibrary() const {
  switch (library_type_) {
  case HttpLibraryType::HTTPLIB:
    return "httplib";
  case HttpLibraryType::CURL:
    return "curl";
  default:
    return "none";
  }
}

void HttpClient::setSSLVerification(bool verify) {
  options_.verify_ssl = verify;
}

std::unordered_map<std::string, std::string>
HttpClient::parseUrl(const std::string &url) {
  std::unordered_map<std::string, std::string> components;

  std::regex url_regex(R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
  std::smatch matches;

  if (std::regex_match(url, matches, url_regex)) {
    components["scheme"] = matches[1].str();
    components["host"] = matches[2].str();
    if (matches[3].matched) {
      components["port"] = matches[3].str();
    }
    if (matches[4].matched) {
      components["path"] = matches[4].str();
    }
  }

  return components;
}

} // namespace Client
} // namespace PulseOne