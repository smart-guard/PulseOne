/**
 * @file S3TargetHandler.cpp
 * @brief S3 Target Handler implementation - PulseOne::Gateway::Target
 * @author PulseOne Development Team
 * @date 2026-02-06
 */

#include "Gateway/Target/S3TargetHandler.h"
#include "Client/S3Client.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Logging/LogManager.h"
#include "Security/SecretManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ClientCacheManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

namespace PulseOne {
namespace Gateway {
namespace Target {

// Static Client Cache for S3
static Utils::ClientCacheManager<Client::S3Client, Client::S3Config> &
getS3ClientCache() {
  static Utils::ClientCacheManager<Client::S3Client, Client::S3Config> cache(
      [](const Client::S3Config &config) {
        return std::make_shared<Client::S3Client>(config);
      },
      100);
  return cache;
}

S3TargetHandler::S3TargetHandler() {
  LogManager::getInstance().Info(
      "S3TargetHandler initialized (Gateway::Target)");
}

S3TargetHandler::~S3TargetHandler() {
  LogManager::getInstance().Info("S3TargetHandler destroyed");
}

bool S3TargetHandler::initialize(const json &config) {
  std::vector<std::string> errors;
  return validateConfig(config, errors);
}

TargetSendResult
S3TargetHandler::sendAlarm(const PulseOne::Gateway::Model::AlarmMessage &alarm,
                           const json &config) {
  TargetSendResult result;
  result.target_type = "S3";
  result.target_name = getTargetName(config);

  try {
    std::string bucket = extractBucketName(config);
    auto client = getOrCreateClient(config, bucket);
    if (!client) {
      result.error_message = "Failed to create S3 client";
      return result;
    }

    std::string key = generateObjectKey(alarm, config);
    std::string content = buildJsonContent(alarm, config);
    auto metadata = buildMetadata(alarm, config);

    auto upload_res =
        client->upload(key, content, "application/json", metadata);
    result.success = upload_res.isSuccess();
    result.sent_payload = content;
    result.response_body =
        upload_res.error_message; // S3 usually has error message for failures
    if (result.success) {
      upload_count_++;
      success_count_++;
      total_bytes_uploaded_ += content.length();
    } else {
      result.error_message = "S3 upload failed: " + upload_res.error_message;
      failure_count_++;
    }
  } catch (const std::exception &e) {
    result.error_message = std::string("S3 exception: ") + e.what();
    failure_count_++;
  }

  return result;
}

TargetSendResult S3TargetHandler::sendFile(const std::string &local_path,
                                           const json &config) {
  TargetSendResult result;
  // File upload logic...
  return result;
}

std::vector<TargetSendResult> S3TargetHandler::sendAlarmBatch(
    const std::vector<PulseOne::Gateway::Model::AlarmMessage> &alarms,
    const json &config) {
  return ITargetHandler::sendAlarmBatch(alarms, config);
}

std::vector<TargetSendResult> S3TargetHandler::sendValueBatch(
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &config) {
  std::vector<TargetSendResult> results;
  // Batch upload logic...
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
  std::string bucket_name = extractBucketName(config);
  if (bucket_name.empty()) {
    errors.push_back(
        "bucket_name 또는 BucketName 필드가 필수이며 비어있을 수 없습니다");
    return false;
  }

  // 버킷명 형식 검증 (AWS S3 규칙) - '/' 포함 허용 (Folder 지원)
  if (bucket_name.length() < 3) {
    errors.push_back("bucket_name은 3자 이상이어야 합니다");
    return false;
  }

  // 영문 소문자, 숫자, 하이픈(-), 점(.), 슬래시(/) 허용
  if (!std::regex_match(bucket_name,
                        std::regex("^[a-z0-9][a-z0-9.\\-/]*[a-z0-9]$"))) {
    errors.push_back(
        "bucket_name은 소문자, 숫자, 하이픈, 슬래시(/)만 사용 가능합니다");
    return false;
  }

  return true;
}

void S3TargetHandler::cleanup() { getS3ClientCache().clear(); }

json S3TargetHandler::getStatus() const {
  return json{{"type", "S3"},
              {"upload_count", upload_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()}};
}

// Private implementations...
std::shared_ptr<PulseOne::Client::S3Client>
S3TargetHandler::getOrCreateClient(const json &config,
                                   const std::string &bucket_name) {
  PulseOne::Client::S3Config s3_config;
  s3_config.endpoint = config.value(
      "endpoint", config.value("S3ServiceUrl", s3_config.endpoint));
  s3_config.region = config.value("region", s3_config.region);

  // 시크릿 해석 (ConfigManager 활용)
  auto &cfg = ConfigManager::getInstance();
  std::string access_key =
      config.value("access_key", config.value("AccessKeyID", ""));
  std::string secret_key =
      config.value("secret_key", config.value("SecretAccessKey", ""));

  s3_config.access_key = cfg.expandVariables(access_key);
  s3_config.secret_key = cfg.expandVariables(secret_key);
  s3_config.bucket_name = bucket_name;
  s3_config.prefix = config.value("prefix", "");
  s3_config.use_ssl = config.value("use_ssl", true);
  s3_config.verify_ssl = config.value("verify_ssl", true);

  return getS3ClientCache().getOrCreate(bucket_name, s3_config);
}

std::string S3TargetHandler::extractBucketName(const json &config) const {
  if (config.contains("bucket"))
    return config["bucket"];
  if (config.contains("bucket_name"))
    return config["bucket_name"];
  if (config.contains("BucketName"))
    return config["BucketName"];
  return "";
}

std::string S3TargetHandler::generateObjectKey(
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  // 객체 키 템플릿
  std::string template_str;
  if (config.contains("ObjectKeyTemplate")) {
    template_str = config["ObjectKeyTemplate"].get<std::string>();
  } else {
    template_str = config.value("object_key_template",
                                "{building_id}/{date}/{timestamp}.json");
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

std::string S3TargetHandler::expandTemplate(
    const std::string &template_str,
    const PulseOne::Gateway::Model::AlarmMessage &alarm) const {
  std::string result = template_str;

  // 알람 변수 치환
  result = std::regex_replace(result, std::regex("\\{building_id\\}"),
                              std::to_string(alarm.site_id));
  result = std::regex_replace(result, std::regex("\\{point_name\\}"),
                              alarm.point_name);
  result = std::regex_replace(result, std::regex("\\{value\\}"),
                              std::to_string(alarm.measured_value));
  result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"),
                              std::to_string(alarm.alarm_level));
  result = std::regex_replace(result, std::regex("\\{status\\}"),
                              std::to_string(alarm.status_code));

  // 타임스탬프 변수 초기값 (fallback: 현재 시간)
  std::string year = generateYearString();
  std::string month = generateMonthString();
  std::string day = generateDayString();
  std::string hour = generateHourString();
  std::string minute = generateMinuteString();
  std::string second = generateSecondString();
  std::string date_str = generateDateString();
  std::string ts_str = generateTimestampString();

  // 발생 시간(alarm.timestamp: yyyy-MM-dd HH:mm:ss.fff)이 있으면 해당 값 사용
  if (alarm.timestamp.length() >= 19) {
    year = alarm.timestamp.substr(0, 4);
    month = alarm.timestamp.substr(5, 2);
    day = alarm.timestamp.substr(8, 2);
    hour = alarm.timestamp.substr(11, 2);
    minute = alarm.timestamp.substr(14, 2);
    second = alarm.timestamp.substr(17, 2);
    date_str = year + month + day;
    // [Mod] User Request: No underscore in timestamp (YYYYMMDDHHMMSS)
    ts_str = year + month + day + hour + minute + second;
  }

  result = std::regex_replace(result, std::regex("\\{timestamp\\}"), ts_str);
  result = std::regex_replace(result, std::regex("\\{date\\}"), date_str);
  // [New] Support {site_id} as alias for building_id
  result = std::regex_replace(result, std::regex("\\{site_id\\}"),
                              std::to_string(alarm.site_id));
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

void S3TargetHandler::expandTemplateVariables(
    json &template_json, const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  auto &transformer = PulseOne::Transform::PayloadTransformer::getInstance();

  std::string target_field_name = "";
  std::string target_description = "";
  std::string converted_value = "";

  // ✅ v3.2.1: 타겟 설정(config)의 field_mappings에서 현재 포인트의 매핑 정보를
  // 찾음
  if (config.contains("field_mappings") &&
      config["field_mappings"].is_array()) {
    for (const auto &m : config["field_mappings"]) {
      if (m.contains("point_id") && m["point_id"] == alarm.point_id) {
        target_field_name = m.value("target_field", "");
        break;
      }
    }
  }

  auto context = transformer.createContext(alarm, target_field_name,
                                           target_description, converted_value);
  template_json = transformer.transform(template_json, context);
  LogManager::getInstance().Info("[TRACE-TRANSFORM-S3] Final Alarm Payload: " +
                                 template_json.dump());
}

void S3TargetHandler::expandTemplateVariables(
    json &template_json, const PulseOne::Gateway::Model::ValueMessage &value,
    const json &config) const {
  auto &transformer = PulseOne::Transform::PayloadTransformer::getInstance();

  std::string target_field_name = "";

  if (config.contains("field_mappings") &&
      config["field_mappings"].is_array()) {
    for (const auto &m : config["field_mappings"]) {
      if (m.contains("point_id") && m["point_id"] == value.point_id) {
        target_field_name = m.value("target_field", "");
        break;
      }
    }
  }

  auto context = transformer.createContext(value, target_field_name);
  template_json = transformer.transform(template_json, context);
  LogManager::getInstance().Info("[TRACE-TRANSFORM-S3] Final Value Payload: " +
                                 template_json.dump());
}

std::string S3TargetHandler::buildJsonContent(
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  json content;

  // ✅ v3.2.0: Payload Template 지원 (Object or Array)
  if (config.contains("body_template") &&
      (config["body_template"].is_object() ||
       config["body_template"].is_array())) {
    content = config["body_template"];
    expandTemplateVariables(content, alarm, config);

    // ✅ 객체인 경우 배열로 래핑하여 일관성 유지
    if (content.is_object()) {
      return json::array({content}).dump(2);
    }
    return content.dump(2);
  }

  // 기본 알람 데이터
  content["building_id"] = alarm.site_id;
  content["point_name"] = alarm.point_name;
  content["value"] = alarm.measured_value;
  content["timestamp"] = alarm.timestamp;
  content["alarm_flag"] = alarm.alarm_level;
  content["status"] = alarm.status_code;
  content["description"] = alarm.description;

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

  return content.dump(2);
}

std::unordered_map<std::string, std::string> S3TargetHandler::buildMetadata(
    const PulseOne::Gateway::Model::AlarmMessage &alarm,
    const json &config) const {
  std::unordered_map<std::string, std::string> metadata;
  metadata["building_id"] = std::to_string(alarm.site_id);
  metadata["point_name"] = alarm.point_name;
  metadata["timestamp"] = alarm.timestamp;
  return metadata;
}

std::string S3TargetHandler::compressContent(const std::string &content,
                                             int level) const {
  return content;
}

std::string S3TargetHandler::getTargetName(const json &config) const {
  return config.value("name", "S3_Target");
}
std::string S3TargetHandler::getCurrentTimestamp() const {
  return PulseOne::Gateway::Model::AlarmMessage::current_time_to_csharp_format(
      true);
}
std::string S3TargetHandler::generateRequestId() const { return "req_s3"; }

REGISTER_TARGET_HANDLER("S3", S3TargetHandler);

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

} // namespace Target
} // namespace Gateway
} // namespace PulseOne
