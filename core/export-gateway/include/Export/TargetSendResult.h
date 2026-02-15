/**
 * @file TargetSendResult.h
 * @brief 타겟 전송 결과 타입 정의
 */

#ifndef EXPORT_TARGET_SEND_RESULT_H
#define EXPORT_TARGET_SEND_RESULT_H

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

namespace PulseOne {

using json = nlohmann::json;

namespace Export {

struct TargetSendResult {
public:
  bool success = false;
  bool skipped = false;
  std::string error_message = "";
  std::chrono::milliseconds response_time{0};
  size_t content_size = 0;
  int retry_count = 0;

  int target_id = 0;
  std::string target_name = "";
  std::string target_type = "";
  std::string sent_payload = "";

  int status_code = 0;
  std::string response_body = "";

  std::string file_path = "";
  std::string s3_object_key = "";
  std::string mqtt_topic = "";

  std::chrono::system_clock::time_point timestamp =
      std::chrono::system_clock::now();

  TargetSendResult() = default;

  TargetSendResult(const std::string &name, const std::string &type,
                   bool result)
      : success(result), target_name(name), target_type(type) {}

  bool isHttpSuccess() const { return status_code >= 200 && status_code < 300; }
  bool isClientError() const { return status_code >= 400 && status_code < 500; }
  bool isServerError() const { return status_code >= 500 && status_code < 600; }

  json toJson() const {
    return json{{"success", success},
                {"error_message", error_message},
                {"response_time_ms", response_time.count()},
                {"content_size", content_size},
                {"retry_count", retry_count},
                {"target_name", target_name},
                {"target_type", target_type},
                {"status_code", status_code},
                {"response_body", response_body},
                {"file_path", file_path},
                {"s3_object_key", s3_object_key},
                {"mqtt_topic", mqtt_topic}};
  }
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_TARGET_SEND_RESULT_H
