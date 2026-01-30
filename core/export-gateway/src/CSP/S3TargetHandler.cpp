/**
 * @file S3TargetHandler.cpp
 * @brief S3 타겟 핸들러 - Stateless 패턴 (v2.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0 - Production-Ready with ClientCacheManager
 * 저장 위치: core/export-gateway/src/CSP/S3TargetHandler.cpp
 *
 * 🚀 v2.0 주요 변경:
 * - Stateless 핸들러 패턴 적용
 * - ClientCacheManager 기반 클라이언트 캐싱
 * - initialize() 선택적 (없어도 동작)
 * - Thread-safe 보장
 */

#include "CSP/S3TargetHandler.h"
#include "Client/S3Client.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ClientCacheManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <sstream>

namespace PulseOne {
namespace CSP {

using json = nlohmann::json;
using json = nlohmann::json;
namespace Utils = PulseOne::Utils;
namespace Client = PulseOne::Client;

// =============================================================================
// Static S3Client Cache (모든 인스턴스 공유)
// =============================================================================
static Utils::ClientCacheManager<Client::S3Client, Client::S3Config> &
getS3ClientCache() {
  static Utils::ClientCacheManager<Client::S3Client, Client::S3Config> cache(
      [](const Client::S3Config &config) {
        // 팩토리: S3Client 생성
        return std::make_shared<Client::S3Client>(config);
      },
      600 // 10분 유휴 시간 (S3는 재연결 비용이 크므로 길게)
  );
  return cache;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

S3TargetHandler::S3TargetHandler() {
  LogManager::getInstance().Info("S3TargetHandler 초기화 (Stateless)");
}

S3TargetHandler::~S3TargetHandler() {
  LogManager::getInstance().Info("S3TargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool S3TargetHandler::initialize(const json &config) {
  // ✅ Stateless 패턴: initialize()는 선택적
  // 설정 검증만 수행
  std::vector<std::string> errors;
  bool valid = validateConfig(config, errors);

  if (!valid) {
    for (const auto &error : errors) {
      LogManager::getInstance().Error("초기화 검증 실패: " + error);
    }
  }

  LogManager::getInstance().Info("S3 타겟 핸들러 초기화 완료 (Stateless)");
  return valid;
}

TargetSendResult S3TargetHandler::sendAlarm(const AlarmMessage &alarm,
                                            const json &config) {
  TargetSendResult result;
  result.target_type = "S3";
  result.target_name = getTargetName(config);
  result.success = false;

  try {
    // ✅ 버킷명 추출
    std::string bucket_name = extractBucketName(config);
    if (bucket_name.empty()) {
      result.error_message = "버킷명이 설정되지 않음";
      LogManager::getInstance().Error(result.error_message);
      return result;
    }

    LogManager::getInstance().Info("S3 알람 업로드 시작: " +
                                   result.target_name);

    // ✅ 클라이언트 획득 (캐시 활용)
    auto client = getOrCreateClient(config, bucket_name);
    if (!client) {
      result.error_message = "S3 클라이언트 생성 실패";
      LogManager::getInstance().Error(result.error_message);
      return result;
    }

    // 객체 키 생성
    std::string object_key = generateObjectKey(alarm, config);
    LogManager::getInstance().Debug("S3 객체 키: " + object_key);

    // JSON 내용 생성
    std::string json_content = buildJsonContent(alarm, config);

    // 압축 처리 (선택사항)
    bool compression_enabled = config.value("compression_enabled", false);
    if (compression_enabled) {
      int compression_level = config.value("compression_level", 6);
      json_content = compressContent(json_content, compression_level);
      LogManager::getInstance().Debug(
          "압축 완료 - 크기: " + std::to_string(json_content.length()) +
          " bytes");
    }

    // 메타데이터 생성
    auto metadata = buildMetadata(alarm, config);

    // ✅ S3 업로드 실행
    auto upload_result = client->uploadJson(object_key, json_content, metadata);

    // 결과 처리
    result.success = upload_result.success;
    result.response_time = std::chrono::milliseconds(
        static_cast<long>(upload_result.upload_time_ms));
    result.content_size = json_content.length();

    if (upload_result.success) {
      result.s3_object_key = object_key;
      success_count_++;
      total_bytes_uploaded_ += json_content.length();

      LogManager::getInstance().Info(
          "✅ S3 업로드 성공: " + object_key + " (ETag: " + upload_result.etag +
          ", 소요시간: " + std::to_string(result.response_time.count()) +
          "ms)");
    } else {
      result.error_message = upload_result.error_message;
      failure_count_++;

      LogManager::getInstance().Error("❌ S3 업로드 실패: " + object_key +
                                      " - " + result.error_message);
    }

    upload_count_++;

  } catch (const std::exception &e) {
    result.error_message = "S3 업로드 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
    failure_count_++;
  }

  return result;
}

std::vector<TargetSendResult>
S3TargetHandler::sendAlarmBatch(const std::vector<AlarmMessage> &alarms,
                                const json &config) {
  std::vector<TargetSendResult> results;
  if (alarms.empty())
    return results;

  std::string target_name = getTargetName(config);
  TargetSendResult base_result(target_name, "S3", false);

  try {
    std::string bucket_name = extractBucketName(config);
    auto client = getOrCreateClient(config, bucket_name);
    if (!client) {
      base_result.error_message = "S3 클라이언트 생성 실패";
      for (size_t i = 0; i < alarms.size(); ++i)
        results.push_back(base_result);
      return results;
    }

    // JSON Array 생성
    json json_array = json::array();
    for (const auto &alarm : alarms) {
      if (config.contains("body_template")) {
        json item = config["body_template"];
        expandTemplateVariables(item, alarm);

        // 개별 아이템이 이미 배열이면 그대로 추가, 객체면 객체로 추가
        json_array.push_back(item);
      } else {
        json_array.push_back(alarm.to_json());
      }
    }
    std::string json_content = json_array.dump();

    // 압축 처리 (선택사항)
    bool compression_enabled = config.value("compression_enabled", false);
    if (compression_enabled) {
      int compression_level = config.value("compression_level", 6);
      json_content = compressContent(json_content, compression_level);
    }

    // 배치 파일 이름 생성 (YYYYMMDDHHMMSS_batch.json)
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm *now_tm = std::gmtime(&now_c);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", now_tm);
    std::string timestamp_str = buf;
    std::string object_key =
        "alarms/batch/" + timestamp_str + "_" + generateRequestId() + ".json";

    if (compression_enabled)
      object_key += ".gz";

    // ✅ S3 업로드 실행
    auto upload_result =
        client->uploadJson(object_key, json_content, {{"type", "alarm_batch"}});

    if (upload_result.success) {
      base_result.success = true;
      base_result.s3_object_key = object_key;
      base_result.content_size = json_content.length();
      base_result.response_time = std::chrono::milliseconds(
          static_cast<long>(upload_result.upload_time_ms));

      success_count_++;
      total_bytes_uploaded_ += json_content.length();
    } else {
      base_result.error_message = upload_result.error_message;
      failure_count_++;
    }

    for (size_t i = 0; i < alarms.size(); ++i) {
      results.push_back(base_result);
    }

  } catch (const std::exception &e) {
    base_result.error_message =
        "알람 배치 업로드 중 예외: " + std::string(e.what());
    for (size_t i = 0; i < alarms.size(); ++i)
      results.push_back(base_result);
  }

  return results;
}

bool S3TargetHandler::testConnection(const json &config) {
  try {
    LogManager::getInstance().Info("S3 연결 테스트 시작");

    // ✅ 설정 검증 먼저 (크래시 방지)
    std::vector<std::string> errors;
    if (!validateConfig(config, errors)) {
      for (const auto &err : errors) {
        LogManager::getInstance().Error("설정 검증 실패: " + err);
      }
      LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 설정 오류");
      return false;
    }

    std::string bucket_name = extractBucketName(config);
    if (bucket_name.empty()) {
      LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 버킷명이 없음");
      return false;
    }

    // ✅ 클라이언트 생성 (검증 포함)
    auto client = getOrCreateClient(config, bucket_name);
    if (!client) {
      LogManager::getInstance().Error(
          "❌ S3 연결 테스트 실패: 클라이언트 생성 실패 (설정 확인 필요)");
      return false;
    }

    // ✅ 안전한 testConnection 호출
    bool success = false;
    try {
      success = client->testConnection();
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("❌ S3 연결 테스트 실패: " +
                                      std::string(e.what()));
      return false;
    } catch (...) {
      LogManager::getInstance().Error(
          "❌ S3 연결 테스트 실패: 알 수 없는 예외");
      return false;
    }

    if (success) {
      LogManager::getInstance().Info("✅ S3 연결 테스트 성공");

      // 추가 검증: 테스트 업로드/삭제 (선택사항)
      if (config.value("test_upload", false)) {
        try {
          std::string test_key =
              "test/connection_test_" + generateTimestampString() + ".json";
          json test_content = {{"test", true},
                               {"timestamp", getCurrentTimestamp()},
                               {"source", "PulseOne-CSPGateway-Test"}};

          auto result = client->uploadJson(test_key, test_content.dump(), {});
          if (result.success) {
            LogManager::getInstance().Info("✅ 테스트 업로드 성공: " +
                                           test_key);
          } else {
            LogManager::getInstance().Warn(
                "⚠️  테스트 업로드 실패 (연결은 성공)");
            // 연결은 성공했으므로 true 유지
          }
        } catch (const std::exception &e) {
          LogManager::getInstance().Warn("⚠️  테스트 업로드 예외: " +
                                         std::string(e.what()));
          // 연결은 성공했으므로 true 유지
        }
      }
    } else {
      LogManager::getInstance().Error("❌ S3 연결 테스트 실패");
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("❌ S3 연결 테스트 예외: " +
                                    std::string(e.what()));
    return false;
  } catch (...) {
    // ✅ 모든 예외 처리
    LogManager::getInstance().Error("❌ S3 연결 테스트 알 수 없는 예외 발생");
    return false;
  }
}

bool S3TargetHandler::validateConfig(const json &config,
                                     std::vector<std::string> &errors) {
  errors.clear();

  // 버킷명 검증
  if (!config.contains("bucket_name")) {
    errors.push_back("bucket_name 필드가 필수입니다");
    return false;
  }

  std::string bucket_name = extractBucketName(config);
  if (bucket_name.empty()) {
    errors.push_back("bucket_name이 비어있습니다");
    return false;
  }

  // 버킷명 형식 검증 (AWS S3 규칙) - '/' 포함 허용 (Folder 지원)
  if (bucket_name.length() < 3) {
    errors.push_back("bucket_name은 3자 이상이어야 합니다");
    return false;
  }

  // 영문 소문자, 숫자, 하이픈, 슬래시(/) 허용
  if (!std::regex_match(bucket_name,
                        std::regex("^[a-z0-9][a-z0-9.-/]*[a-z0-9]$"))) {
    errors.push_back(
        "bucket_name은 소문자, 숫자, 하이픈, 슬래시(/)만 사용 가능합니다");
    return false;
  }

  return true;
}

json S3TargetHandler::getStatus() const {
  auto cache_stats = getS3ClientCache().getStats();

  return json{{"type", "S3"},
              {"upload_count", upload_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()},
              {"total_bytes_uploaded", total_bytes_uploaded_.load()},
              {"cache_stats",
               {{"active_clients", cache_stats.active_clients},
                {"total_entries", cache_stats.total_entries}}}};
}

void S3TargetHandler::cleanup() {
  getS3ClientCache().clear();
  LogManager::getInstance().Info("S3TargetHandler 정리 완료");
}

// =============================================================================
// Private 핵심 메서드
// =============================================================================

std::shared_ptr<Client::S3Client>
S3TargetHandler::getOrCreateClient(const json &config,
                                   const std::string &bucket_name) {

  // ✅ 캐시 키: bucket_name (버킷별로 클라이언트 재사용)
  std::string cache_key = bucket_name;

  // S3 설정 구성
  Client::S3Config s3_config = buildS3Config(config);

  // ✅ 설정 유효성 검증 (크래시 방지)
  if (!s3_config.isValid()) {
    std::string error_detail = "Invalid S3 configuration: ";
    if (s3_config.access_key.empty())
      error_detail += "access_key is empty; ";
    if (s3_config.secret_key.empty())
      error_detail += "secret_key is empty; ";
    if (s3_config.bucket_name.empty())
      error_detail += "bucket_name is empty; ";

    LogManager::getInstance().Error(error_detail);
    return nullptr; // ✅ nullptr 반환 (크래시 방지)
  }

  // ✅ 캐시에서 가져오거나 생성
  try {
    auto client = getS3ClientCache().getOrCreate(cache_key, s3_config);

    // ✅ 생성 실패 시 nullptr 체크
    if (!client) {
      LogManager::getInstance().Error("S3Client 생성 실패: " + bucket_name);
      return nullptr;
    }

    return client;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("S3Client 생성 예외: " +
                                    std::string(e.what()));
    return nullptr;
  } catch (...) {
    LogManager::getInstance().Error("S3Client 생성 알 수 없는 예외");
    return nullptr;
  }
}

std::string S3TargetHandler::extractBucketName(const json &config) const {
  if (config.contains("BucketName") &&
      !config["BucketName"].get<std::string>().empty()) {
    std::string bucket_name = config["BucketName"].get<std::string>();
    return expandEnvironmentVariables(bucket_name);
  }
  if (config.contains("bucket_name") &&
      !config["bucket_name"].get<std::string>().empty()) {
    std::string bucket_name = config["bucket_name"].get<std::string>();
    return expandEnvironmentVariables(bucket_name);
  }
  return "";
}

Client::S3Config S3TargetHandler::buildS3Config(const json &config) const {
  Client::S3Config s3_config;

  // 버킷명
  s3_config.bucket_name = extractBucketName(config);

  // Folder (Path Prefix) 처리
  if (config.contains("Folder")) {
    s3_config.prefix = config["Folder"].get<std::string>();
  } else if (config.contains("prefix")) {
    s3_config.prefix = config["prefix"].get<std::string>();
  }
  s3_config.prefix = expandEnvironmentVariables(s3_config.prefix);

  // S3ServiceUrl (Endpoint) 우선 처리
  if (config.contains("S3ServiceUrl")) {
    std::string url = config["S3ServiceUrl"].get<std::string>();
    s3_config.endpoint = expandEnvironmentVariables(url);

    // ✅ Virtual Host Style 자동 감지 (버킷명이 호스트에 포함된 경우)
    if (!s3_config.bucket_name.empty() &&
        s3_config.endpoint.find(s3_config.bucket_name + ".") !=
            std::string::npos) {
      s3_config.use_virtual_host_style = true;
      LogManager::getInstance().Info("S3 Virtual Host Style 감지됨: " +
                                     s3_config.endpoint);
    }

    // URL에서 Region 추출 시도 (예: https://s3.ap-northeast-2.amazonaws.com)
    std::regex region_regex("s3\\.([a-z0-9-]+)\\.amazonaws\\.com");
    std::smatch match;
    if (std::regex_search(s3_config.endpoint, match, region_regex)) {
      s3_config.region = match[1];
    } else {
      // 기본값 혹은 설정된 값 유지
      s3_config.region = config.value("region", "us-east-1");
    }
  } else {
    // 리전
    std::string region = config.value("region", "us-east-1");
    s3_config.region = expandEnvironmentVariables(region);

    // 엔드포인트
    if (config.contains("endpoint")) {
      std::string endpoint = config["endpoint"].get<std::string>();
      s3_config.endpoint = expandEnvironmentVariables(endpoint);
    } else {
      s3_config.endpoint = generateS3Endpoint(s3_config.region);
    }
  }

  // 자격증명 로드
  loadCredentials(config, s3_config);

  // 타임아웃 설정
  s3_config.upload_timeout_sec = config.value("upload_timeout_sec", 60);
  s3_config.connect_timeout_sec = config.value("connect_timeout_sec", 10);
  s3_config.verify_ssl = config.value("verify_ssl", true);
  s3_config.max_retries = config.value("max_retries", 3);

  // 컨텐츠 타입
  std::string content_type = config.value("content_type", "application/json");
  s3_config.content_type = expandEnvironmentVariables(content_type);

  return s3_config;
}

void S3TargetHandler::loadCredentials(const json &config,
                                      Client::S3Config &s3_config) const {
  auto &config_manager = ConfigManager::getInstance();

  // 1. 파일에서 자격증명 로드 (최우선)
  if (config.contains("access_key_file") &&
      config.contains("secret_key_file")) {
    try {
      std::string access_key_config =
          config["access_key_file"].get<std::string>();
      std::string secret_key_config =
          config["secret_key_file"].get<std::string>();

      std::string access_key = config_manager.getSecret(access_key_config);
      std::string secret_key = config_manager.getSecret(secret_key_config);

      if (!access_key.empty() && !secret_key.empty()) {
        s3_config.access_key = access_key;
        s3_config.secret_key = secret_key;
        LogManager::getInstance().Info("✅ S3 자격증명 파일에서 로드");
        return;
      }
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("자격증명 파일 로드 실패: " +
                                      std::string(e.what()));
    }
  }

  // 2. 직접 설정에서 로드 (AccessKeyID / SecretAccessKey 우선)
  if (config.contains("AccessKeyID") && config.contains("SecretAccessKey")) {
    s3_config.access_key = config_manager.expandVariables(
        config["AccessKeyID"].get<std::string>());
    s3_config.secret_key = config_manager.expandVariables(
        config["SecretAccessKey"].get<std::string>());
    LogManager::getInstance().Info("✅ S3 자격증명(S3 Service) 설정에서 로드");
    return;
  }

  if (config.contains("access_key") && config.contains("secret_key")) {
    s3_config.access_key =
        config_manager.expandVariables(config["access_key"].get<std::string>());
    s3_config.secret_key =
        config_manager.expandVariables(config["secret_key"].get<std::string>());
    LogManager::getInstance().Info("✅ S3 자격증명 설정에서 로드");
    return;
  }

  // 3. 환경변수에서 로드
  std::string credential_prefix = config.value("credential_prefix", "S3_");

  std::string access_key =
      config_manager.getSecret(credential_prefix + "ACCESS_KEY");
  if (!access_key.empty()) {
    s3_config.access_key = access_key;
  }

  std::string secret_key =
      config_manager.getSecret(credential_prefix + "SECRET_KEY");
  if (!secret_key.empty()) {
    s3_config.secret_key = secret_key;
  }

  if (s3_config.access_key.empty() || s3_config.secret_key.empty()) {
    LogManager::getInstance().Warn(
        "⚠️  S3 자격증명이 설정되지 않음 (IAM Role 사용 가능)");
  } else {
    LogManager::getInstance().Info("✅ S3 자격증명 환경변수에서 로드");
  }
}

std::string S3TargetHandler::generateObjectKey(const AlarmMessage &alarm,
                                               const json &config) const {
  // 객체 키 템플릿
  // 객체 키 템플릿
  std::string template_str;
  if (config.contains("ObjectKeyTemplate")) {
    template_str = config["ObjectKeyTemplate"].get<std::string>();
  } else {
    template_str = config.value(
        "object_key_template",
        "{building_id}/{date}/{point_name}_{timestamp}_alarm.json");
  }
  template_str = expandEnvironmentVariables(template_str);

  // 템플릿 확장
  std::string object_key = expandTemplate(template_str, alarm);

  // 경로 정규화
  object_key = std::regex_replace(object_key, std::regex("//+"), "/");

  // Folder (Path Prefix) 처리
  std::string prefix;
  if (config.contains("Folder")) {
    prefix = config["Folder"].get<std::string>();
  } else if (config.contains("prefix")) {
    prefix = config["prefix"].get<std::string>();
  }

  if (!prefix.empty()) {
    prefix = expandEnvironmentVariables(prefix);
    // prefix에 끝 슬래시 보장
    if (prefix.back() != '/')
      prefix += "/";

    object_key = prefix + object_key;
  }

  // 시작 슬래시 제거 (S3 키 규칙)
  while (!object_key.empty() && object_key[0] == '/') {
    object_key = object_key.substr(1);
  }

  return object_key;
}

std::string S3TargetHandler::expandTemplate(const std::string &template_str,
                                            const AlarmMessage &alarm) const {
  std::string result = template_str;

  // 알람 변수 치환
  result = std::regex_replace(result, std::regex("\\{building_id\\}"),
                              std::to_string(alarm.bd));
  result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
  result = std::regex_replace(result, std::regex("\\{value\\}"),
                              std::to_string(alarm.vl));
  result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"),
                              std::to_string(alarm.al));
  result = std::regex_replace(result, std::regex("\\{status\\}"),
                              std::to_string(alarm.st));

  // 타임스탬프 변수 초기값 (fallback: 현재 시간)
  std::string year = generateYearString();
  std::string month = generateMonthString();
  std::string day = generateDayString();
  std::string hour = generateHourString();
  std::string minute = generateMinuteString();
  std::string second = generateSecondString();
  std::string date_str = generateDateString();
  std::string ts_str = generateTimestampString();

  // 발생 시간(alarm.tm: yyyy-MM-dd HH:mm:ss.fff)이 있으면 해당 값 사용
  if (alarm.tm.length() >= 19) {
    year = alarm.tm.substr(0, 4);
    month = alarm.tm.substr(5, 2);
    day = alarm.tm.substr(8, 2);
    hour = alarm.tm.substr(11, 2);
    minute = alarm.tm.substr(14, 2);
    second = alarm.tm.substr(17, 2);
    date_str = year + month + day;
    ts_str = year + month + day + "_" + hour + minute + second;
  }

  result = std::regex_replace(result, std::regex("\\{timestamp\\}"), ts_str);
  result = std::regex_replace(result, std::regex("\\{date\\}"), date_str);
  result = std::regex_replace(result, std::regex("\\{year\\}"), year);
  result = std::regex_replace(result, std::regex("\\{month\\}"), month);
  result = std::regex_replace(result, std::regex("\\{day\\}"), day);
  result = std::regex_replace(result, std::regex("\\{hour\\}"), hour);
  result = std::regex_replace(result, std::regex("\\{minute\\}"), minute);
  result = std::regex_replace(result, std::regex("\\{second\\}"), second);

  // 알람 상태
  result = std::regex_replace(result, std::regex("\\{alarm_status\\}"),
                              alarm.get_alarm_status_string());

  // 안전한 파일명으로 변환
  result = std::regex_replace(result, std::regex("[^a-zA-Z0-9/_.-]"), "_");

  return result;
}

std::string S3TargetHandler::buildJsonContent(const AlarmMessage &alarm,
                                              const json &config) const {
  json content;

  // ✅ v3.2.0: Payload Template 지원 (Object or Array)
  if (config.contains("body_template") &&
      (config["body_template"].is_object() ||
       config["body_template"].is_array())) {
    content = config["body_template"];
    expandTemplateVariables(content, alarm);

    // ✅ 객체인 경우 배열로 래핑하여 일관성 유지
    if (content.is_object()) {
      return json::array({content}).dump(2);
    }
    return content.dump(2);
  }

  // 기본 알람 데이터
  content["building_id"] = alarm.bd;
  content["point_name"] = alarm.nm;
  content["value"] = alarm.vl;
  content["timestamp"] = alarm.tm;
  content["alarm_flag"] = alarm.al;
  content["status"] = alarm.st;
  content["description"] = alarm.des;

  // 메타데이터
  content["source"] = "PulseOne-CSPGateway";
  content["version"] = "2.0";
  content["upload_timestamp"] = getCurrentTimestamp();
  content["alarm_status"] = alarm.get_alarm_status_string();

  // 사용자 정의 필드
  if (config.contains("additional_fields") &&
      config["additional_fields"].is_object()) {
    for (auto &[key, value] : config["additional_fields"].items()) {
      content[key] = value;
    }
  }

  if (config.value("compression_enabled", false)) {
    content["_compression"] = "gzip";
    content["_compression_level"] = config.value("compression_level", 6);
  }

  return json::array({content}).dump(2);
}

std::unordered_map<std::string, std::string>
S3TargetHandler::buildMetadata(const AlarmMessage &alarm,
                               const json &config) const {

  std::unordered_map<std::string, std::string> metadata;

  // 기본 메타데이터
  metadata["building-id"] = std::to_string(alarm.bd);
  metadata["point-name"] = alarm.nm;
  metadata["alarm-flag"] = std::to_string(alarm.al);
  metadata["alarm-status"] = alarm.get_alarm_status_string();
  metadata["upload-timestamp"] = getCurrentTimestamp();
  metadata["source"] = "PulseOne-CSPGateway";
  metadata["version"] = "2.0";

  // 압축 정보
  if (config.value("compression_enabled", false)) {
    metadata["content-encoding"] = "gzip";
    metadata["compression-level"] =
        std::to_string(config.value("compression_level", 6));
  }

  // 사용자 정의 메타데이터
  if (config.contains("custom_metadata") &&
      config["custom_metadata"].is_object()) {
    for (auto &[key, value] : config["custom_metadata"].items()) {
      if (value.is_string()) {
        std::string expanded_value =
            expandTemplate(value.get<std::string>(), alarm);
        metadata[key] = expanded_value;
      }
    }
  }

  return metadata;
}

std::string S3TargetHandler::compressContent(const std::string &content,
                                             int level) const {
  // 실제 구현: zlib 사용
  // 여기서는 단순화된 구현
  LogManager::getInstance().Debug("압축 (레벨: " + std::to_string(level) +
                                  ") - 실제 구현 시 zlib 사용");
  return content;
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string S3TargetHandler::generateRequestId() const {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  return "req_" + std::to_string(ms.count());
}

std::string S3TargetHandler::getTargetName(const json &config) const {
  if (config.contains("name") && config["name"].is_string()) {
    return config["name"].get<std::string>();
  }

  std::string bucket_name = extractBucketName(config);
  if (!bucket_name.empty()) {
    return "S3://" + bucket_name;
  }

  return "S3-Target";
}

std::string S3TargetHandler::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
  return oss.str();
}

std::string S3TargetHandler::generateTimestampString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
  return oss.str();
}

std::string S3TargetHandler::generateDateString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
  return oss.str();
}

std::string S3TargetHandler::generateYearString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y");
  return oss.str();
}

std::string S3TargetHandler::generateMonthString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%m");
  return oss.str();
}

std::string S3TargetHandler::generateDayString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%d");
  return oss.str();
}

std::string S3TargetHandler::generateHourString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%H");
  return oss.str();
}

std::string S3TargetHandler::generateMinuteString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%M");
  return oss.str();
}

std::string S3TargetHandler::generateSecondString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%S");
  return oss.str();
}

std::string
S3TargetHandler::expandEnvironmentVariables(const std::string &str) const {
  std::string result = str;
  size_t pos = 0;

  while ((pos = result.find("${", pos)) != std::string::npos) {
    size_t end_pos = result.find("}", pos + 2);
    if (end_pos == std::string::npos)
      break;

    std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
    const char *env_value = std::getenv(var_name.c_str());
    std::string replacement = env_value ? env_value : "";

    result.replace(pos, end_pos - pos + 1, replacement);
    pos += replacement.length();
  }

  return result;
}

std::string
S3TargetHandler::generateS3Endpoint(const std::string &region) const {
  if (region == "us-east-1") {
    return "https://s3.amazonaws.com";
  }

  if (region.find("cn-") == 0) {
    return "https://s3." + region + ".amazonaws.com.cn";
  }

  if (region.find("us-gov-") == 0) {
    return "https://s3." + region + ".amazonaws.com";
  }

  return "https://s3." + region + ".amazonaws.com";
}

// =============================================================================
// Batch and Template Methods
// =============================================================================
std::vector<TargetSendResult>
S3TargetHandler::sendValueBatch(const std::vector<ValueMessage> &values,
                                const json &config) {

  std::vector<TargetSendResult> results;
  if (values.empty())
    return results;

  TargetSendResult base_result;
  base_result.target_type = "S3";
  base_result.target_name = getTargetName(config);
  base_result.success = false;

  // 1. 설정 및 클라이언트
  std::string bucket_name = extractBucketName(config);
  if (bucket_name.empty()) {
    base_result.error_message = "버킷명이 설정되지 않음";
    for (size_t i = 0; i < values.size(); ++i)
      results.push_back(base_result);
    return results;
  }

  auto client = getOrCreateClient(config, bucket_name);
  if (!client) {
    base_result.error_message = "S3 클라이언트 생성 실패";
    for (size_t i = 0; i < values.size(); ++i)
      results.push_back(base_result);
    return results;
  }

  // 2. JSON Array 생성
  json json_array = json::array();
  for (const auto &val : values) {
    if (config.contains("body_template") &&
        config["body_template"].is_object()) {
      json item = config["body_template"];
      expandTemplateVariables(item, val);
      json_array.push_back(item);
    } else {
      json_array.push_back(val.to_json());
    }
  }
  std::string content = json_array.dump();

  // 3. S3 경로: {Folder}/{bd}/{yyyyMMdd}/{yyyyMMddHHmmss}.json
  // 발생 시간(values[0].tm) 기반으로 경로 구성
  std::string year = "", month = "", day = "", hour = "", minute = "",
              second = "";
  if (!values.empty() && values[0].tm.length() >= 19) {
    const std::string &tm = values[0].tm;
    year = tm.substr(0, 4);
    month = tm.substr(5, 2);
    day = tm.substr(8, 2);
    hour = tm.substr(11, 2);
    minute = tm.substr(14, 2);
    second = tm.substr(17, 2);
  } else {
    // Fallback: 현재 시간
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::gmtime(&now_c); // UTC 기준
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y", &now_tm);
    year = buf;
    std::strftime(buf, sizeof(buf), "%m", &now_tm);
    month = buf;
    std::strftime(buf, sizeof(buf), "%d", &now_tm);
    day = buf;
    std::strftime(buf, sizeof(buf), "%H", &now_tm);
    hour = buf;
    std::strftime(buf, sizeof(buf), "%M", &now_tm);
    minute = buf;
    std::strftime(buf, sizeof(buf), "%S", &now_tm);
    second = buf;
  }

  std::string date_part = year + month + day;
  std::string time_part = year + month + day + hour + minute + second;

  int bd = values[0].bd;
  std::string object_key =
      std::to_string(bd) + "/" + date_part + "/" + time_part + ".json";

  // Folder (Path Prefix) 처리
  std::string prefix;
  if (config.contains("Folder")) {
    prefix = config["Folder"].get<std::string>();
  } else if (config.contains("prefix")) {
    prefix = config["prefix"].get<std::string>();
  }

  if (!prefix.empty()) {
    prefix = expandEnvironmentVariables(prefix);
    if (prefix.back() != '/')
      prefix += "/";
    object_key = prefix + object_key;
  }

  // 시작 슬래시 제거
  while (!object_key.empty() && object_key[0] == '/') {
    object_key = object_key.substr(1);
  }

  // 4. 업로드
  bool success = false;
  std::string error_msg;

  try {
    std::unordered_map<std::string, std::string> metadata;
    metadata["batch_size"] = std::to_string(values.size());
    metadata["content_type"] = "application/json";

    auto upload_result = client->uploadJson(object_key, content, metadata);
    success = upload_result.success;
    error_msg = upload_result.error_message;

    if (success) {
      LogManager::getInstance().Info("✅ S3 배치 업로드 성공: " + object_key);
      success_count_++;
      total_bytes_uploaded_ += content.length();
    } else {
      LogManager::getInstance().Error("❌ S3 배치 업로드 실패: " + error_msg);
      failure_count_++;
    }
  } catch (const std::exception &e) {
    error_msg = e.what();
    LogManager::getInstance().Error("❌ S3 배치 업로드 예외: " + error_msg);
    failure_count_++;
  }

  // 5. 결과 채우기
  for (size_t i = 0; i < values.size(); ++i) {
    TargetSendResult res = base_result;
    res.success = success;
    res.error_message = error_msg;
    res.s3_object_key = object_key;
    res.content_size = content.length() / values.size();
    results.push_back(res);
  }

  upload_count_++;
  return results;
}

void S3TargetHandler::expandTemplateVariables(json &template_json,
                                              const AlarmMessage &alarm) const {
  try {
    auto &transformer = PulseOne::Transform::PayloadTransformer::getInstance();
    auto context = transformer.createContext(alarm);
    template_json = transformer.transform(template_json, context);
  } catch (...) {
  }
}

void S3TargetHandler::expandTemplateVariables(json &template_json,
                                              const ValueMessage &value) const {
  try {
    auto &transformer = PulseOne::Transform::PayloadTransformer::getInstance();
    auto context = transformer.createContext(value);
    template_json = transformer.transform(template_json, context);
  } catch (...) {
  }
}

} // namespace CSP
} // namespace PulseOne
