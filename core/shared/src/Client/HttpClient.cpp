/**
 * @file HttpClient.cpp
 * @brief HTTP 클라이언트 래퍼 구현 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 */

#include "Client/HttpClient.h"
#include <chrono>
#include <sstream>
#include <regex>

#ifdef HAS_SHARED_LIBS
    #include "Utils/LogManager.h"
    #define LOG_ERROR(msg) LogManager::getInstance().Error(msg)
    #define LOG_DEBUG(msg) LogManager::getInstance().Debug(msg)
#else
    #define LOG_ERROR(msg) // 로깅 비활성화
    #define LOG_DEBUG(msg) // 로깅 비활성화
#endif

namespace PulseOne {
namespace Client {

#ifdef HAS_CURL
bool HttpClient::curl_global_initialized_ = false;
#endif

HttpClient::HttpClient(const std::string& base_url, const HttpRequestOptions& options)
    : base_url_(base_url), options_(options), library_type_(HttpLibraryType::NONE) {
    
    initializeHttpLibrary();
    
    // 기본 헤더 설정
    default_headers_["User-Agent"] = options_.user_agent;
    default_headers_["Accept"] = "application/json";
    default_headers_["Connection"] = "keep-alive";
}

HttpClient::~HttpClient() {
#ifdef HAS_CURL
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
#ifdef HAVE_HTTPLIB
    try {
        LOG_DEBUG("Trying httplib initialization...");
        
        if (!base_url_.empty()) {
            auto parsed = parseUrl(base_url_);
            std::string host = parsed["host"];
            int port = std::stoi(parsed.count("port") ? parsed["port"] : 
                                (parsed["scheme"] == "https" ? "443" : "80"));
            
            LOG_DEBUG("Parsed - scheme: " + parsed["scheme"] + ", host: " + host + ", port: " + std::to_string(port));
            
            if (parsed["scheme"] == "https") {
                httplib_client_ = std::make_unique<httplib::SSLClient>(host, port);
                LOG_DEBUG("SSLClient created");
            } else {
                httplib_client_ = std::make_unique<httplib::Client>(host, port);
                LOG_DEBUG("Client created");
            }
            
            httplib_client_->set_connection_timeout(options_.connect_timeout_sec);
            httplib_client_->set_read_timeout(options_.timeout_sec);
            httplib_client_->set_follow_redirects(options_.follow_redirects);
            
            if (parsed["scheme"] == "https" && !options_.verify_ssl) {
                auto ssl_client = dynamic_cast<httplib::SSLClient*>(httplib_client_.get());
                if (ssl_client) {
                    ssl_client->set_ca_cert_path("");
                    LOG_DEBUG("SSL verification disabled");
                }
            }
        } else {
            httplib_client_ = std::make_unique<httplib::Client>();
            LOG_DEBUG("Empty base_url - default client created");
        }
        
        library_type_ = HttpLibraryType::HTTPLIB;
        LOG_DEBUG("HttpClient initialized with httplib successfully");
        return;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize httplib: " + std::string(e.what()));
    }
#else
    LOG_DEBUG("HAVE_HTTPLIB not defined");
#endif

    // httplib 실패 시 curl 사용
#ifdef HAS_CURL
    try {
        LOG_DEBUG("Trying curl initialization...");
        
        if (!curl_global_initialized_) {
            LOG_DEBUG("Initializing curl globally...");
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_global_initialized_ = true;
        }
        
        curl_handle_ = curl_easy_init();
        if (curl_handle_) {
            library_type_ = HttpLibraryType::CURL;
            LOG_DEBUG("HttpClient initialized with curl successfully");
            return;
        } else {
            LOG_ERROR("curl_easy_init() returned nullptr");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize curl: " + std::string(e.what()));
    }
#else
    LOG_DEBUG("HAS_CURL not defined");
#endif

    LOG_ERROR("No HTTP library available - HAVE_HTTPLIB=" 
#ifdef HAVE_HTTPLIB
              "1"
#else
              "0"
#endif
              ", HAS_CURL="
#ifdef HAS_CURL
              "1"
#else
              "0"
#endif
    );
    
    library_type_ = HttpLibraryType::NONE;
}

HttpResponse HttpClient::get(const std::string& path,
                            const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("GET", path, "", "", headers);
}

HttpResponse HttpClient::post(const std::string& path,
                             const std::string& body,
                             const std::string& content_type,
                             const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("POST", path, body, content_type, headers);
}

HttpResponse HttpClient::put(const std::string& path,
                            const std::string& body,
                            const std::string& content_type,
                            const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("PUT", path, body, content_type, headers);
}

HttpResponse HttpClient::del(const std::string& path,
                            const std::unordered_map<std::string, std::string>& headers) {
    return executeRequest("DELETE", path, "", "", headers);
}

HttpResponse HttpClient::executeRequest(const std::string& method,
                                       const std::string& path,
                                       const std::string& body,
                                       const std::string& content_type,
                                       const std::unordered_map<std::string, std::string>& headers) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    HttpResponse response;
    
    switch (library_type_) {
#ifdef HAVE_HTTPLIB
        case HttpLibraryType::HTTPLIB:
            response = executeWithHttplib(method, path, body, content_type, headers);
            break;
#endif
#ifdef HAS_CURL
        case HttpLibraryType::CURL:
            response = executeWithCurl(method, path, body, content_type, headers);
            break;
#endif
        default:
            // 매개변수 unused 경고 방지
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
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    response.response_time_ms = duration.count();
    
    LOG_DEBUG("HTTP " + method + " " + path + " -> " + std::to_string(response.status_code) + 
              " (" + std::to_string(response.response_time_ms) + "ms)");
    
    return response;
}

#ifdef HAVE_HTTPLIB
HttpResponse HttpClient::executeWithHttplib(const std::string& method,
                                           const std::string& path,
                                           const std::string& body,
                                           const std::string& content_type,
                                           const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse response;
    
    try {
        if (!httplib_client_) {
            response.error_message = "httplib client not initialized";
            return response;
        }
        
        // 헤더 병합
        httplib::Headers req_headers;
        for (const auto& header : default_headers_) {
            req_headers.insert(header);
        }
        for (const auto& header : headers) {
            req_headers.insert(header);
        }
        
        if (!content_type.empty()) {
            req_headers.insert({"Content-Type", content_type});
        }
        
        // 인증 헤더 추가
        if (!options_.bearer_token.empty()) {
            req_headers.insert({"Authorization", "Bearer " + options_.bearer_token});
        } else if (!options_.username.empty()) {
            std::string auth = options_.username + ":" + options_.password;
            // Base64 인코딩 필요 (간단 구현)
            req_headers.insert({"Authorization", "Basic " + auth}); // 실제로는 base64 인코딩 필요
        }
        
        httplib::Result result;
        
        if (method == "GET") {
            result = httplib_client_->Get(path.c_str(), req_headers);
        } else if (method == "POST") {
            result = httplib_client_->Post(path.c_str(), req_headers, body, content_type.c_str());
        } else if (method == "PUT") {
            result = httplib_client_->Put(path.c_str(), req_headers, body, content_type.c_str());
        } else if (method == "DELETE") {
            result = httplib_client_->Delete(path.c_str(), req_headers);
        }
        
        if (result) {
            response.status_code = result->status;
            response.body = result->body;
            
            for (const auto& header : result->headers) {
                response.headers[header.first] = header.second;
            }
        } else {
            response.status_code = 0;
            response.error_message = "HTTP request failed";
        }
        
    } catch (const std::exception& e) {
        response.status_code = 0;
        response.error_message = "httplib exception: " + std::string(e.what());
    }
    
    return response;
}
#endif

#ifdef HAS_CURL
HttpResponse HttpClient::executeWithCurl(const std::string& method,
                                        const std::string& path,
                                        const std::string& body,
                                        const std::string& content_type,
                                        const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse response;
    
    try {
        if (!curl_handle_) {
            response.error_message = "curl handle not initialized";
            LOG_ERROR("❌ curl handle is nullptr");
            return response;
        }
        
        // URL 구성
        std::string full_url = base_url_;
        if (!path.empty() && path[0] != '/') {
            full_url += "/";
        }
        full_url += path;
        
        // 🔥 상세 로그 추가 1: URL 확인
        LOG_DEBUG("🌐 curl PUT request starting");
        LOG_DEBUG("  Method: " + method);
        LOG_DEBUG("  Base URL: " + base_url_);
        LOG_DEBUG("  Path: " + path);
        LOG_DEBUG("  Full URL: " + full_url);
        LOG_DEBUG("  Body size: " + std::to_string(body.length()) + " bytes");
        
        // curl 옵션 설정
        curl_easy_setopt(curl_handle_, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, options_.timeout_sec);
        curl_easy_setopt(curl_handle_, CURLOPT_CONNECTTIMEOUT, options_.connect_timeout_sec);
        curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, options_.follow_redirects ? 1L : 0L);
        curl_easy_setopt(curl_handle_, CURLOPT_MAXREDIRS, options_.max_redirects);
        
        // 🔥 상세 로그 추가 2: curl 옵션 확인
        LOG_DEBUG("  Timeout: " + std::to_string(options_.timeout_sec) + "s");
        LOG_DEBUG("  Connect timeout: " + std::to_string(options_.connect_timeout_sec) + "s");
        LOG_DEBUG("  SSL verify: " + std::string(options_.verify_ssl ? "true" : "false"));
        
        if (!options_.verify_ssl) {
            curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, 0L);
            LOG_DEBUG("  ⚠️  SSL verification disabled");
        }
        
        // 🔥 상세 로그 추가 3: verbose 모드 활성화 (디버깅용)
        curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, 1L);
        
        // 응답 콜백 설정
        std::string response_body;
        curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response_body);
        
        // 헤더 콜백 설정
        curl_easy_setopt(curl_handle_, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
        curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, &response.headers);
        
        // HTTP 메서드 설정
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
        
        // 헤더 설정
        struct curl_slist* header_list = nullptr;
        
        // 기본 헤더 추가
        for (const auto& header : default_headers_) {
            std::string header_str = header.first + ": " + header.second;
            header_list = curl_slist_append(header_list, header_str.c_str());
            LOG_DEBUG("  Header: " + header_str);
        }
        
        // 요청 헤더 추가
        for (const auto& header : headers) {
            std::string header_str = header.first + ": " + header.second;
            header_list = curl_slist_append(header_list, header_str.c_str());
            LOG_DEBUG("  Header: " + header_str);
        }
        
        if (!content_type.empty()) {
            std::string content_type_header = "Content-Type: " + content_type;
            header_list = curl_slist_append(header_list, content_type_header.c_str());
            LOG_DEBUG("  Header: " + content_type_header);
        }
        
        // 인증 헤더
        if (!options_.bearer_token.empty()) {
            std::string auth_header = "Authorization: Bearer " + options_.bearer_token;
            header_list = curl_slist_append(header_list, auth_header.c_str());
            LOG_DEBUG("  Header: Authorization: Bearer ***");
        } else if (!options_.username.empty()) {
            curl_easy_setopt(curl_handle_, CURLOPT_USERNAME, options_.username.c_str());
            curl_easy_setopt(curl_handle_, CURLOPT_PASSWORD, options_.password.c_str());
            LOG_DEBUG("  Auth: Basic (username set)");
        }
        
        if (header_list) {
            curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
        }
        
        // 🔥 상세 로그 추가 4: 요청 실행 전
        LOG_DEBUG("🚀 Executing curl_easy_perform...");
        
        // 요청 실행
        CURLcode res = curl_easy_perform(curl_handle_);
        
        // 🔥 상세 로그 추가 5: 응답 상세 정보
        LOG_DEBUG("📥 curl_easy_perform completed");
        LOG_DEBUG("  CURLcode: " + std::to_string(res) + " (" + std::string(curl_easy_strerror(res)) + ")");
        
        if (res == CURLE_OK) {
            long status_code;
            curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &status_code);
            response.status_code = static_cast<int>(status_code);
            response.body = response_body;
            
            // 추가 정보 로깅
            double total_time = 0;
            curl_easy_getinfo(curl_handle_, CURLINFO_TOTAL_TIME, &total_time);
            
            long redirect_count = 0;
            curl_easy_getinfo(curl_handle_, CURLINFO_REDIRECT_COUNT, &redirect_count);
            
            char* effective_url = nullptr;
            curl_easy_getinfo(curl_handle_, CURLINFO_EFFECTIVE_URL, &effective_url);
            
            LOG_DEBUG("✅ Request successful");
            LOG_DEBUG("  HTTP Status: " + std::to_string(status_code));
            LOG_DEBUG("  Response size: " + std::to_string(response_body.length()) + " bytes");
            LOG_DEBUG("  Total time: " + std::to_string(total_time) + "s");
            LOG_DEBUG("  Redirects: " + std::to_string(redirect_count));
            if (effective_url) {
                LOG_DEBUG("  Effective URL: " + std::string(effective_url));
            }
            
        } else {
            response.status_code = 0;
            response.error_message = "curl error: " + std::string(curl_easy_strerror(res));
            
            // 🔥 상세 로그 추가 6: 에러 상세 분석
            LOG_ERROR("❌ curl request failed");
            LOG_ERROR("  Error code: " + std::to_string(res));
            LOG_ERROR("  Error message: " + std::string(curl_easy_strerror(res)));
            
            // 에러별 상세 분석
            if (res == CURLE_COULDNT_RESOLVE_HOST) {
                LOG_ERROR("  🔍 DNS resolution failed - check hostname");
            } else if (res == CURLE_COULDNT_CONNECT) {
                LOG_ERROR("  🔍 Connection failed - check network/firewall");
            } else if (res == CURLE_OPERATION_TIMEDOUT) {
                LOG_ERROR("  🔍 Operation timeout - check timeout settings");
            } else if (res == CURLE_SSL_CONNECT_ERROR) {
                LOG_ERROR("  🔍 SSL connection error - check SSL settings");
            } else if (res == CURLE_URL_MALFORMAT) {
                LOG_ERROR("  🔍 Malformed URL: " + full_url);
            }
        }
        
        // 정리
        if (header_list) {
            curl_slist_free_all(header_list);
        }
        
    } catch (const std::exception& e) {
        response.status_code = 0;
        response.error_message = "curl exception: " + std::string(e.what());
        LOG_ERROR("❌ Exception in executeWithCurl: " + std::string(e.what()));
    }
    
    return response;
}
#endif

void HttpClient::setBaseUrl(const std::string& base_url) {
    base_url_ = base_url;
    initializeHttpLibrary(); // 재초기화
}

void HttpClient::setOptions(const HttpRequestOptions& options) {
    options_ = options;
    default_headers_["User-Agent"] = options_.user_agent;
}

void HttpClient::setDefaultHeaders(const std::unordered_map<std::string, std::string>& headers) {
    for (const auto& header : headers) {
        default_headers_[header.first] = header.second;
    }
}

void HttpClient::setBearerToken(const std::string& token) {
    options_.bearer_token = token;
}

void HttpClient::setBasicAuth(const std::string& username, const std::string& password) {
    options_.username = username;
    options_.password = password;
}

bool HttpClient::testConnection(const std::string& test_path) {
    try {
        auto response = get(test_path);
        return response.status_code > 0; // 연결이라도 되면 성공
    } catch (...) {
        return false;
    }
}

std::string HttpClient::getHttpLibrary() const {
    switch (library_type_) {
        case HttpLibraryType::HTTPLIB: return "httplib";
        case HttpLibraryType::CURL: return "curl";
        default: return "none";
    }
}

void HttpClient::setSSLVerification(bool verify) {
    options_.verify_ssl = verify;
}

std::unordered_map<std::string, std::string> HttpClient::parseUrl(const std::string& url) {
    std::unordered_map<std::string, std::string> components;
    
    // 간단한 URL 파싱 (정규식 사용)
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