/**
 * @file PayloadTransformer.h
 * @brief 템플릿 기반 Payload 변환기
 * @version 2.0.0
 * 저장 위치: core/export-gateway/include/Transform/PayloadTransformer.h
 */

#ifndef PAYLOAD_TRANSFORMER_H
#define PAYLOAD_TRANSFORMER_H

#include "CSP/AlarmMessage.h"
#include "Export/ExportTypes.h"
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace PulseOne {

using json = nlohmann::json;

namespace Transform {

using CSP::AlarmMessage;
using CSP::ValueMessage;

// =============================================================================
// 변수 컨텍스트
// =============================================================================

struct TransformContext {
  // 공통 필드
  int building_id = 0;
  std::string point_name;
  std::string original_point_name;
  std::string value;
  std::string timestamp;
  int status = 0;
  std::string type = "dbl";

  // 알람 전용 필드
  int alarm_flag = 0;
  std::string description;

  // 매핑 필드
  std::string target_field_name;
  std::string target_description;
  std::string converted_value;

  // 계산된 필드 (buildVariableMap에서 사용)
  std::string timestamp_iso8601;
  int64_t timestamp_unix_ms = 0;
  std::string alarm_status;

  std::map<std::string, std::string> custom_vars;
};

// =============================================================================
// PayloadTransformer (싱글톤)
// =============================================================================

class PayloadTransformer {
public:
  static PayloadTransformer &getInstance() {
    static PayloadTransformer instance;
    return instance;
  }

  PayloadTransformer(const PayloadTransformer &) = delete;
  PayloadTransformer &operator=(const PayloadTransformer &) = delete;

  // 메인 변환
  json transform(const json &template_json, const TransformContext &context);
  std::string transformString(const std::string &template_str,
                              const TransformContext &context);

  // [v3.0.0] Unified Payload Builder (Refactored from S3TargetHandler)
  json buildPayload(const AlarmMessage &alarm, const json &config);
  json buildPayload(const ValueMessage &value, const json &config);

  // 시스템별 기본 템플릿
  json getInsiteDefaultTemplate();
  json getHDCDefaultTemplate();
  json getBEMSDefaultTemplate();
  json getGenericDefaultTemplate();
  json getDefaultTemplateForSystem(const std::string &system_type);

  // 컨텍스트 생성 (알람)
  TransformContext createContext(const AlarmMessage &alarm,
                                 const std::string &target_field_name = "",
                                 const std::string &target_description = "",
                                 const std::string &converted_value = "");

  // 컨텍스트 생성 (주기적 데이터)
  TransformContext createContext(const ValueMessage &value,
                                 const std::string &target_field_name = "",
                                 const std::string &target_description = "",
                                 const std::string &converted_value = "");

  // 변수 관리
  std::map<std::string, std::string>
  buildVariableMap(const TransformContext &context);
  void expandJsonRecursive(json &obj,
                           const std::map<std::string, std::string> &variables,
                           const std::string &current_key = "");

  // 헬퍼
  std::string toISO8601(const std::string &alarm_tm);
  int64_t toUnixTimestampMs(const std::string &alarm_tm);
  std::string getAlarmStatusString(int al, int st);

  // 사용자 정의 변수
  void registerCustomVariable(
      const std::string &name,
      std::function<std::string(const TransformContext &)> value_func);

private:
  PayloadTransformer();
  ~PayloadTransformer() = default;

  std::map<std::string, std::function<std::string(const TransformContext &)>>
      custom_variable_funcs_;
};

} // namespace Transform
} // namespace PulseOne

#endif // PAYLOAD_TRANSFORMER_H