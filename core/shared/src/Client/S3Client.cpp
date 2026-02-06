/**
 * @file S3Client.cpp
 * @brief S3 클라이언트 구현 - AWS Signature V4 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 */

#include "Client/S3Client.h"
#include <algorithm>
#include <future>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <thread>

// 암호화 라이브러리 (OpenSSL)
#ifdef HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

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

S3Client::S3Client(const S3Config &config) : config_(config) {
  LOG_DEBUG("S3Client 생성자 시작");

  try {
    std::string endpoint_log =
        config_.endpoint.empty() ? "(empty)" : config_.endpoint;
    LOG_DEBUG("Endpoint: " + endpoint_log);
  } catch (...) {
    LOG_ERROR("Endpoint 로깅 실패");
  }

  if (!config_.endpoint.empty() && config_.endpoint.back() == '/') {
    config_.endpoint.pop_back();
  }

  if (!config_.isValid()) {
    // ✅ 구체적인 에러 메시지
    std::string error_detail = "Invalid S3 configuration: ";
    if (config_.access_key.empty())
      error_detail += "access_key is empty; ";
    if (config_.secret_key.empty())
      error_detail += "secret_key is empty; ";
    if (config_.bucket_name.empty())
      error_detail += "bucket_name is empty; ";

    LOG_ERROR(error_detail);
    return;
  }

  LOG_DEBUG("S3 config valid, creating HttpClient...");

  try {
    HttpRequestOptions http_options;
    http_options.timeout_sec = config_.upload_timeout_sec;
    http_options.connect_timeout_sec = config_.connect_timeout_sec;
    http_options.verify_ssl = config_.verify_ssl;
    http_options.user_agent = "PulseOne-S3Client/1.0";

    LOG_DEBUG("HttpRequestOptions prepared");

    // 방어적 생성
    http_client_ = std::make_unique<HttpClient>(config_.endpoint, http_options);

    LOG_DEBUG("HttpClient make_unique returned");

    // null 체크
    if (!http_client_) {
      LOG_ERROR("HttpClient creation returned nullptr");
      return;
    }

    LOG_DEBUG("S3Client initialized for bucket: " + config_.bucket_name);

  } catch (const std::exception &e) {
    LOG_ERROR("HttpClient creation exception: " + std::string(e.what()));
    http_client_ = nullptr;
  } catch (...) {
    LOG_ERROR("Unknown exception during HttpClient creation");
    http_client_ = nullptr;
  }
}

S3UploadResult
S3Client::upload(const std::string &object_key, const std::string &content,
                 const std::string &content_type,
                 const std::unordered_map<std::string, std::string> &metadata) {

  std::string actual_content_type =
      content_type.empty() ? config_.content_type : content_type;

  return executeUploadWithRetry(object_key, content, actual_content_type,
                                metadata);
}

S3UploadResult S3Client::uploadJson(
    const std::string &object_key, const std::string &json_content,
    const std::unordered_map<std::string, std::string> &metadata) {

  auto enhanced_metadata = metadata;
  enhanced_metadata["Content-Language"] = "ko-KR";
  enhanced_metadata["x-amz-meta-source"] = "PulseOne";
  enhanced_metadata["x-amz-meta-type"] = "json-data";

  return upload(object_key, json_content, "application/json",
                enhanced_metadata);
}

std::vector<S3UploadResult> S3Client::uploadBatch(
    const std::unordered_map<std::string, std::string> &files,
    const std::string &content_type,
    const std::unordered_map<std::string, std::string> &common_metadata) {

  std::vector<S3UploadResult> results;
  results.reserve(files.size());

  LOG_DEBUG("Starting batch upload: " + std::to_string(files.size()) +
            " files");

  // 병렬 업로드 (최대 5개 동시 업로드)
  const size_t max_concurrent = 5;
  std::vector<std::future<S3UploadResult>> futures;

  auto files_iter = files.begin();

  while (files_iter != files.end()) {
    // 현재 배치 준비
    futures.clear();
    futures.reserve(max_concurrent);

    for (size_t i = 0; i < max_concurrent && files_iter != files.end();
         ++i, ++files_iter) {
      const auto &[object_key, content] = *files_iter;

      futures.push_back(
          std::async(std::launch::async, [this, &object_key, &content,
                                          &content_type, &common_metadata]() {
            return upload(object_key, content, content_type, common_metadata);
          }));
    }

    // 결과 수집
    for (auto &future : futures) {
      results.push_back(future.get());
    }
  }

  size_t successful = std::count_if(
      results.begin(), results.end(),
      [](const S3UploadResult &result) { return result.success; });

  LOG_DEBUG("Batch upload completed: " + std::to_string(successful) + "/" +
            std::to_string(results.size()) + " successful");

  return results;
}

S3UploadResult S3Client::executeUploadWithRetry(
    const std::string &object_key, const std::string &content,
    const std::string &content_type,
    const std::unordered_map<std::string, std::string> &metadata) {

  S3UploadResult result;

  // ✅ http_client_ null 체크 추가
  if (!http_client_) {
    result.success = false;
    result.error_message = "HttpClient not initialized";
    LOG_ERROR("S3 upload failed: HttpClient is null");
    return result;
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int attempt = 0; attempt <= config_.max_retries; ++attempt) {
    try {
      if (attempt > 0) {
        LOG_DEBUG("S3 upload retry attempt " + std::to_string(attempt) +
                  " for: " + object_key);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.retry_delay_ms * attempt));
      }

      // AWS Signature V4 인증 헤더 생성
      auto timestamp = std::chrono::system_clock::now();
      std::unordered_map<std::string, std::string> headers;

      // endpoint may contain https:// and path, extract only the hostname for
      // Host header and path for signing
      std::string endpoint_path = "";
      std::string host = config_.endpoint;
      size_t protocol_pos = host.find("://");
      if (protocol_pos != std::string::npos) {
        host = host.substr(protocol_pos + 3);
      }
      size_t path_pos = host.find("/");
      if (path_pos != std::string::npos) {
        endpoint_path = host.substr(path_pos);
        if (!endpoint_path.empty() && endpoint_path.back() == '/') {
          endpoint_path.pop_back();
        }
        host = host.substr(0, path_pos);
      }
      headers["host"] = host;
      LogManager::getInstance().Info("[v3.2.0 Debug] S3 Host: " + host +
                                     ", Path: " + endpoint_path +
                                     ", Bucket: " + config_.bucket_name);

      const std::string standard_content_type = "application/json";
      headers["content-type"] = standard_content_type;
      headers["x-amz-date"] = formatTimestamp(timestamp);
      headers["x-amz-content-sha256"] = sha256Hash(content);

      LogManager::getInstance().Info(
          "[v3.2.0 Debug] S3 Final Headers to Sign:");
      for (const auto &h : headers) {
        LogManager::getInstance().Info("  " + h.first + ": " + h.second);
      }

      // Storage Class 설정
      if (!config_.storage_class.empty()) {
        headers["x-amz-storage-class"] = config_.storage_class;
      }

      // 메타데이터 헤더 추가
      for (const auto &meta : metadata) {
        if (meta.first.find("x-amz-meta-") == 0) {
          headers[meta.first] = meta.second;
        } else {
          headers["x-amz-meta-" + meta.first] = meta.second;
        }
      }

      // 사용자 정의 헤더 추가 (x-api-key 등)
      for (const auto &custom : config_.custom_headers) {
        headers[custom.first] = custom.second;
      }

      // Authorization 헤더 생성
      // Canonical URI must include the full path sent to the server
      std::string uri = endpoint_path + "/" + object_key;
      if (!config_.use_virtual_host_style) {
        uri = endpoint_path + "/" + config_.bucket_name + "/" + object_key;
      }

      // Normalize slashes in uri
      uri = std::regex_replace(uri, std::regex("//+"), "/");
      LogManager::getInstance().Info("[v3.2.0 Debug] S3 Final URI: [" + uri +
                                     "]");

      // ✅ 헤더 정규화 (소문자로 변환 및 공백 제거)
      std::unordered_map<std::string, std::string> normalized_headers;
      for (const auto &h : headers) {
        std::string key = h.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        std::string val = h.second;
        // Trim whitespace
        val.erase(0, val.find_first_not_of(" \t\n\r"));
        val.erase(val.find_last_not_of(" \t\n\r") + 1);

        normalized_headers[key] = val;
      }

      std::string auth_header = createSignatureV4(
          "PUT", uri, {}, normalized_headers, content, timestamp);
      headers["Authorization"] = auth_header;

      // S3 URL 생성
      std::string s3_url = buildS3Url(object_key);

      // HTTP PUT 요청 실행
      // ✅ S3 전송을 위한 헤더 맵 업데이트 (인증 정보 포함)
      std::unordered_map<std::string, std::string> final_headers =
          normalized_headers;
      final_headers["authorization"] = auth_header;

      http_client_->setBaseUrl(""); // 전체 URL 사용
      auto http_response = http_client_->put(
          s3_url, content, standard_content_type, final_headers);

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);

      result.upload_time_ms = duration.count();
      result.upload_time = timestamp;
      result.file_size = content.length();
      result.status_code = http_response.status_code;

      if (http_response.isSuccess()) {
        result.success = true;
        result.file_url = s3_url;

        // ETag 추출
        auto etag_it = http_response.headers.find("ETag");
        if (etag_it != http_response.headers.end()) {
          result.etag = etag_it->second;
          // 따옴표 제거
          if (result.etag.front() == '"' && result.etag.back() == '"') {
            result.etag = result.etag.substr(1, result.etag.length() - 2);
          }
        }

        LOG_DEBUG("S3 upload successful: " + object_key +
                  " (ETag: " + result.etag + ")");
        break;

      } else {
        std::string clean_body = http_response.body;

        // HTML 응답인 경우 제목(title)만 추출하여 노이즈 제거
        if (clean_body.find("<html") != std::string::npos ||
            clean_body.find("<HTML") != std::string::npos) {
          std::regex title_regex("<title>(.*?)</title>",
                                 std::regex_constants::icase);
          std::smatch match;
          if (std::regex_search(clean_body, match, title_regex) &&
              match.size() > 1) {
            clean_body = "[HTML] " + match.str(1);
          } else {
            clean_body = "HTML Error Response (Check logs)";
          }
        }

        // 너무 긴 에러 메시지 절삭 (최대 100자)
        if (clean_body.length() > 100) {
          clean_body = clean_body.substr(0, 100) + "...";
        }

        result.error_message = "HTTP " +
                               std::to_string(http_response.status_code) +
                               ": " + clean_body;

        // 재시도하지 않을 오류들 (4xx 클라이언트 오류)
        if (http_response.isClientError()) {
          LOG_ERROR("S3 upload client error (no retry): " +
                    result.error_message);
          break;
        }
      }

    } catch (const std::exception &e) {
      result.error_message = "S3 upload exception: " + std::string(e.what());
    }

    if (attempt == config_.max_retries) {
      LOG_ERROR("S3 upload failed after " +
                std::to_string(config_.max_retries + 1) +
                " attempts: " + result.error_message);
    }
  }

  return result;
}

bool S3Client::exists(const std::string &object_key) {
  try {
    auto info = getObjectInfo(object_key);
    return info.has_value();
  } catch (...) {
    return false;
  }
}

std::optional<S3Object> S3Client::getObjectInfo(const std::string &object_key) {
  try {
    // HEAD 요청으로 객체 정보 조회
    auto timestamp = std::chrono::system_clock::now();

    std::unordered_map<std::string, std::string> headers;
    headers["Host"] =
        config_.endpoint.find("://") != std::string::npos
            ? config_.endpoint.substr(config_.endpoint.find("://") + 3)
            : config_.endpoint;
    std::string uri = "/" + object_key;
    if (!config_.use_virtual_host_style) {
      uri = "/" + config_.bucket_name + "/" + object_key;
    }
    std::string auth_header =
        createSignatureV4("HEAD", uri, {}, headers, "", timestamp);
    headers["Authorization"] = auth_header;

    std::string s3_url = buildS3Url(object_key);

    // HTTP HEAD 요청 (GET을 사용해서 헤더만 확인)
    http_client_->setBaseUrl("");
    auto response = http_client_->get(s3_url, headers);

    if (response.isSuccess()) {
      S3Object obj;
      obj.key = object_key;

      // ETag 추출
      auto etag_it = response.headers.find("ETag");
      if (etag_it != response.headers.end()) {
        obj.etag = etag_it->second;
      }

      // Content-Length 추출
      auto length_it = response.headers.find("Content-Length");
      if (length_it != response.headers.end()) {
        obj.size = std::stoull(length_it->second);
      }

      // Last-Modified 추출 및 파싱 (간단 구현)
      auto modified_it = response.headers.find("Last-Modified");
      if (modified_it != response.headers.end()) {
        // 실제로는 HTTP 날짜 파싱이 필요하지만 여기서는 현재 시간 사용
        obj.last_modified = std::chrono::system_clock::now();
      }

      return obj;
    }

  } catch (const std::exception &e) {
    LOG_ERROR("Failed to get S3 object info: " + std::string(e.what()));
  }

  return std::nullopt;
}

std::vector<S3Object> S3Client::listObjects(const std::string &prefix,
                                            int max_keys) {
  std::vector<S3Object> objects;

  // unused parameter 경고 방지
  (void)prefix;
  (void)max_keys;

  try {
    // ListObjects API 호출 (간단 구현 - 실제로는 XML 파싱 필요)
    LOG_DEBUG("S3 listObjects not fully implemented");

  } catch (const std::exception &e) {
    LOG_ERROR("Failed to list S3 objects: " + std::string(e.what()));
  }

  return objects;
}

void S3Client::updateConfig(const S3Config &new_config) {
  config_ = new_config;

  // HTTP 클라이언트 재초기화
  HttpRequestOptions http_options;
  http_options.timeout_sec = config_.upload_timeout_sec;
  http_options.connect_timeout_sec = config_.connect_timeout_sec;
  http_options.verify_ssl = config_.verify_ssl;

  http_client_ = std::make_unique<HttpClient>(config_.endpoint, http_options);
}

bool S3Client::testConnection() {
  try {
    // 간단한 ListBucket 요청으로 연결 테스트
    auto timestamp = std::chrono::system_clock::now();

    std::unordered_map<std::string, std::string> headers;
    headers["Host"] =
        config_.endpoint.find("://") != std::string::npos
            ? config_.endpoint.substr(config_.endpoint.find("://") + 3)
            : config_.endpoint;
    std::string uri = "/";
    if (!config_.use_virtual_host_style) {
      uri = "/" + config_.bucket_name + "/";
    }
    std::string auth_header =
        createSignatureV4("GET", uri, {}, headers, "", timestamp);
    headers["Authorization"] = auth_header;

    std::string bucket_url = config_.endpoint + "/" + config_.bucket_name;

    http_client_->setBaseUrl("");
    auto response = http_client_->get(bucket_url, headers);

    return response.status_code > 0; // 연결이라도 되면 성공

  } catch (...) {
    return false;
  }
}

std::string
S3Client::generateTimestampFileName(const std::string &prefix,
                                    const std::string &extension) const {

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::ostringstream filename;
  filename << prefix << "_"
           << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << "_"
           << std::setfill('0') << std::setw(3) << ms.count() << "."
           << extension;

  return filename.str();
}

std::string S3Client::createSignatureV4(
    const std::string &method, const std::string &uri,
    const std::unordered_map<std::string, std::string> &query_params,
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &payload,
    const std::chrono::system_clock::time_point &timestamp) {

  // 1. Canonical Request 생성
  std::string canonical_request =
      createCanonicalRequest(method, uri, query_params, headers, payload);
  LogManager::getInstance().Info("[v3.2.0 Debug] S3 Canonical Request:\n[\n" +
                                 canonical_request + "\n]");

  // 2. String to Sign 생성
  std::string string_to_sign = createStringToSign(canonical_request, timestamp);
  LogManager::getInstance().Info("[v3.2.0 Debug] S3 String-to-Sign:\n[\n" +
                                 string_to_sign + "\n]");

  LogManager::getInstance().Info(
      "[v3.2.0 Debug] S3 Credential Scope: " + formatDate(timestamp) + "/" +
      config_.region + "/" + config_.service_name + "/aws4_request");

  // 3. Signing Key 생성 (Raw Bytes)
  std::string signing_key = createSigningKey(timestamp);

  // 4. Signature 계산 (Signing Key는 binary, string_to_sign은 string)
  std::string signature = hmacSha256(signing_key, string_to_sign);

  // 5. Authorization 헤더 생성
  std::string date_str = formatDate(timestamp);
  std::string credential = config_.access_key + "/" + date_str + "/" +
                           config_.region + "/" + config_.service_name +
                           "/aws4_request";

  // Signed Headers 생성
  std::vector<std::string> header_keys;
  for (const auto &header : headers) {
    std::string key = header.first;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    header_keys.push_back(key);
  }
  std::sort(header_keys.begin(), header_keys.end());

  std::string signed_headers;
  for (size_t i = 0; i < header_keys.size(); ++i) {
    if (i > 0)
      signed_headers += ";";
    signed_headers += header_keys[i];
  }

  std::ostringstream auth_header;
  auth_header << "AWS4-HMAC-SHA256 "
              << "Credential=" << credential << ","
              << "SignedHeaders=" << signed_headers << ","
              << "Signature=" << signature;

  return auth_header.str();
}

std::string S3Client::createCanonicalRequest(
    const std::string &method, const std::string &uri,
    const std::unordered_map<std::string, std::string> &query_params,
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &payload) {

  std::ostringstream canonical;

  // 1. HTTP Method
  canonical << method << "\n";

  // 2. Canonical URI
  // AWS S3 requires single encoding for path segments, preserving slashes.
  // We also normalize consecutive slashes to single slashes.
  std::string normalized_uri = uri;
  normalized_uri = std::regex_replace(normalized_uri, std::regex("//+"), "/");
  std::string encoded_uri =
      urlEncode(normalized_uri, false); // S3 path should NOT encode slashes
  LogManager::getInstance().Info("[v3.2.0 Debug] Canonical URI: [" +
                                 encoded_uri + "]");
  canonical << encoded_uri << "\n";

  // 3. Canonical Query String
  if (query_params.empty()) {
    canonical << "\n";
  } else {
    std::vector<std::pair<std::string, std::string>> sorted_params(
        query_params.begin(), query_params.end());
    std::sort(sorted_params.begin(), sorted_params.end());

    for (size_t i = 0; i < sorted_params.size(); ++i) {
      if (i > 0)
        canonical << "&";
      canonical << urlEncode(sorted_params[i].first) << "="
                << urlEncode(sorted_params[i].second);
    }
    canonical << "\n";
  }

  // 4. Canonical Headers
  std::vector<std::pair<std::string, std::string>> sorted_headers;
  for (const auto &header : headers) {
    std::string key = header.first;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    sorted_headers.emplace_back(key, header.second);
  }
  std::sort(sorted_headers.begin(), sorted_headers.end());

  for (const auto &header : sorted_headers) {
    canonical << header.first << ":" << header.second << "\n";
  }
  canonical << "\n";

  // 5. Signed Headers
  for (size_t i = 0; i < sorted_headers.size(); ++i) {
    if (i > 0)
      canonical << ";";
    canonical << sorted_headers[i].first;
  }
  canonical << "\n";

  // 6. Hashed Payload
  canonical << sha256Hash(payload);

  return canonical.str();
}

std::string S3Client::createStringToSign(
    const std::string &canonical_request,
    const std::chrono::system_clock::time_point &timestamp) {

  std::ostringstream string_to_sign;

  string_to_sign << "AWS4-HMAC-SHA256\n";
  string_to_sign << formatTimestamp(timestamp) << "\n";
  string_to_sign << formatDate(timestamp) << "/" << config_.region << "/"
                 << config_.service_name << "/aws4_request\n";
  string_to_sign << sha256Hash(canonical_request);

  return string_to_sign.str();
}

std::string S3Client::createSigningKey(
    const std::chrono::system_clock::time_point &timestamp) {
  std::string date_str = formatDate(timestamp);

  std::string k_date = hmacSha256Raw("AWS4" + config_.secret_key, date_str);
  std::string k_region = hmacSha256Raw(k_date, config_.region);
  std::string k_service = hmacSha256Raw(k_region, config_.service_name);
  std::string k_signing = hmacSha256Raw(k_service, "aws4_request");

  return k_signing;
}

std::string S3Client::urlEncode(const std::string &str,
                                bool encode_slash) const {
  std::ostringstream encoded;
  encoded << std::hex << std::uppercase;

  for (char c : str) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded << c;
    } else if (c == '/' && !encode_slash) {
      encoded << c;
    } else {
      encoded << '%' << std::setfill('0') << std::setw(2)
              << static_cast<unsigned char>(c);
    }
  }

  return encoded.str();
}

std::string S3Client::sha256Hash(const std::string &data) const {
#ifdef HAS_OPENSSL
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(data.c_str()), data.length(),
         hash);

  std::ostringstream hex_stream;
  hex_stream << std::hex << std::setfill('0');
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    hex_stream << std::setw(2) << static_cast<unsigned>(hash[i]);
  }

  return hex_stream.str();
#else
  // unused parameter 경고 방지
  (void)data;
  // OpenSSL이 없는 경우 간단한 해시 (실제 운영에서는 사용 불가)
  LOG_ERROR("SHA256 requires OpenSSL");
  return "dummy_hash";
#endif
}

std::string S3Client::hmacSha256(const std::string &key,
                                 const std::string &data) const {
#ifdef HAS_OPENSSL
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int result_len;

  HMAC(EVP_sha256(), key.c_str(), key.length(),
       reinterpret_cast<const unsigned char *>(data.c_str()), data.length(),
       result, &result_len);

  std::ostringstream hex_stream;
  hex_stream << std::hex << std::setfill('0');
  for (unsigned int i = 0; i < result_len; ++i) {
    hex_stream << std::setw(2) << static_cast<unsigned>(result[i]);
  }

  return hex_stream.str();
#else
  (void)key;
  (void)data;
  return "dummy_hmac";
#endif
}

std::string S3Client::hmacSha256Raw(const std::string &key,
                                    const std::string &data) const {
#ifdef HAS_OPENSSL
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int result_len;

  HMAC(EVP_sha256(), key.c_str(), key.length(),
       reinterpret_cast<const unsigned char *>(data.c_str()), data.length(),
       result, &result_len);

  return std::string(reinterpret_cast<char *>(result), result_len);
#else
  (void)key;
  (void)data;
  return "dummy_hmac_raw";
#endif
}

std::string S3Client::formatTimestamp(
    const std::chrono::system_clock::time_point &timestamp) const {
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::ostringstream formatted;
  formatted << std::put_time(std::gmtime(&time_t), "%Y%m%dT%H%M%SZ");
  return formatted.str();
}

std::string S3Client::formatDate(
    const std::chrono::system_clock::time_point &timestamp) const {
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::ostringstream formatted;
  formatted << std::put_time(std::gmtime(&time_t), "%Y%m%d");
  return formatted.str();
}

std::string S3Client::buildS3Url(const std::string &object_key) const {
  std::string base_url = config_.endpoint;
  if (!base_url.empty() && base_url.back() == '/') {
    base_url.pop_back();
  }

  std::string url;
  if (config_.use_virtual_host_style) {
    // Virtual Host Style: https://bucket.s3.region.amazonaws.com/object
    url = base_url + "/" + urlEncode(object_key, false);
  } else {
    // Path Style: https://s3.region.amazonaws.com/bucket/object
    url = base_url + "/" + config_.bucket_name + "/" +
          urlEncode(object_key, false);
  }

  // Normalize consecutive slashes (except the one after the protocol)
  size_t proto_end = url.find("://");
  if (proto_end != std::string::npos) {
    std::string proto = url.substr(0, proto_end + 3);
    std::string path_part = url.substr(proto_end + 3);
    path_part = std::regex_replace(path_part, std::regex("//+"), "/");
    url = proto + path_part;
  }

  return url;
}

} // namespace Client
} // namespace PulseOne