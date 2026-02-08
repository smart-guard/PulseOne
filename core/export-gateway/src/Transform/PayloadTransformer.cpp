/**
 * @file PayloadTransformer.cpp
 * @brief 템플릿 기반 Payload 변환기 구현
 * @version 2.0.0
 * 저장 위치: core/export-gateway/src/Transform/PayloadTransformer.cpp
 */

#include "Transform/PayloadTransformer.h"
#include "Logging/LogManager.h"
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

namespace PulseOne {
namespace Transform {

PayloadTransformer::PayloadTransformer() {
  // noise log removed
}

// =============================================================================
// 메인 변환
// =============================================================================

json PayloadTransformer::transform(const json &template_json,
                                   const TransformContext &context) {
  try {
    json result = template_json;
    auto variables = buildVariableMap(context);
    expandJsonRecursive(result, variables);
    return result;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("PayloadTransformer::transform 실패: " +
                                    std::string(e.what()));
    return json::object();
  }
}

std::string
PayloadTransformer::transformString(const std::string &template_str,
                                    const TransformContext &context) {
  std::string result = template_str;
  auto variables = buildVariableMap(context);

  for (const auto &[key, value] : variables) {
    std::string pattern = "{{" + key + "}}";
    size_t pos = 0;
    while ((pos = result.find(pattern, pos)) != std::string::npos) {
      result.replace(pos, pattern.length(), value);
      pos += value.length();
    }
  }
  return result;
}

// =============================================================================
// 시스템별 기본 템플릿
// =============================================================================

json PayloadTransformer::getInsiteDefaultTemplate() {
  return json{{"site_id", "{{site_id}}"},
              {"target_key", "{{target_key}}"},
              {"measured_value", "{{measured_value}}"},
              {"timestamp", "{{timestamp}}"},
              {"status_code", "{{status_code}}"}};
}

json PayloadTransformer::getHDCDefaultTemplate() {
  return json{{"building_id", "{{building_id}}"},
              {"point_id", "{{target_field_name}}"},
              {"data", json{{"value", "{{converted_value}}"},
                            {"timestamp", "{{timestamp_unix_ms}}"}}},
              {"metadata", json{{"description", "{{target_description}}"},
                                {"alarm_status", "{{alarm_status}}"}}}};
}

json PayloadTransformer::getBEMSDefaultTemplate() {
  return json{{"buildingId", "{{building_id}}"},
              {"sensorName", "{{target_field_name}}"},
              {"sensorValue", "{{converted_value}}"},
              {"timestamp", "{{timestamp_iso8601}}"},
              {"alarmLevel", "{{alarm_status}}"}};
}

json PayloadTransformer::getGenericDefaultTemplate() {
  return json{{"site_id", "{{site_id}}"},
              {"type", "{{type}}"},
              {"target_key", "{{target_key}}"},
              {"measured_value", "{{measured_value}}"},
              {"timestamp", "{{timestamp}}"},
              {"status_code", "{{status_code}}"},
              {"alarm_level", "{{alarm_level}}"},
              {"target_description", "{{target_description}}"},
              {"mi", "{{mi}}"},
              {"mx", "{{mx}}"},
              {"il", "{{il}}"},
              {"xl", "{{xl}}"}};
}

json PayloadTransformer::getDefaultTemplateForSystem(
    const std::string &system_type) {
  if (system_type == "insite")
    return getInsiteDefaultTemplate();
  else if (system_type == "hdc")
    return getHDCDefaultTemplate();
  else if (system_type == "bems")
    return getBEMSDefaultTemplate();
  else
    return getGenericDefaultTemplate();
}

// =============================================================================
// 컨텍스트 생성
// =============================================================================

TransformContext PayloadTransformer::createContext(
    const AlarmMessage &alarm, const std::string &target_field_name,
    const std::string &target_description, const std::string &converted_value) {

  TransformContext context;
  context.building_id = alarm.site_id;
  context.point_name = alarm.point_name;
  context.value = std::to_string(alarm.measured_value);
  context.timestamp = alarm.timestamp;
  context.status = alarm.status_code;
  context.type = alarm.data_type;

  context.alarm_flag = alarm.alarm_level;
  context.description = alarm.description;

  context.target_field_name = target_field_name;
  context.target_description = target_description;
  context.converted_value = converted_value;

  context.timestamp_iso8601 = toISO8601(alarm.timestamp);
  context.timestamp_unix_ms = toUnixTimestampMs(alarm.timestamp);
  context.alarm_status =
      getAlarmStatusString(alarm.alarm_level, alarm.status_code);
  context.original_point_name = alarm.original_name;

  LogManager::getInstance().Info(
      "[TRACE-TRANSFORM] Value Context created for " + context.point_name +
      " (Value: " + context.value + ")");

  // [v3.2.1] True Zero-Assumption Harvesting: 모든 추가 정보를 토큰화
  if (!alarm.extra_info.is_null() && alarm.extra_info.is_object()) {
    for (auto it = alarm.extra_info.begin(); it != alarm.extra_info.end();
         ++it) {
      // 이미 정의된 기본 변수들과 충돌하지 않는 경우에만 수집
      context.custom_vars[it.key()] = it.value().is_string()
                                          ? it.value().get<std::string>()
                                          : it.value().dump();
    }
  }

  return context;
}

TransformContext PayloadTransformer::createContext(
    const ValueMessage &value, const std::string &target_field_name,
    const std::string &target_description, const std::string &converted_value) {

  TransformContext context;
  context.building_id = value.site_id;
  context.point_name = value.point_name;
  context.value = value.measured_value;
  context.timestamp = value.timestamp;
  context.status = value.status_code;
  context.type = value.data_type;

  context.alarm_flag = 0; // 주기 데이터는 알람 아님

  context.target_field_name = target_field_name;
  context.target_description = target_description;
  context.converted_value = converted_value;

  context.timestamp_iso8601 = toISO8601(value.timestamp);
  context.timestamp_unix_ms = toUnixTimestampMs(value.timestamp);

  LogManager::getInstance().Info(
      "[TRACE-TRANSFORM] Value Context created for " + context.point_name +
      " (Value: " + context.value + ")");

  // [v3.0.0] Zero-Assumption Harvesting for Periodic Data
  if (!value.extra_info.is_null() && value.extra_info.is_object()) {
    for (auto it = value.extra_info.begin(); it != value.extra_info.end();
         ++it) {
      if (it.value().is_primitive()) {
        context.custom_vars[it.key()] = it.value().is_string()
                                            ? it.value().get<std::string>()
                                            : it.value().dump();
      } else {
        context.custom_vars[it.key()] = it.value().dump();
      }
    }
  }

  context.alarm_status = "NORMAL";

  return context;
}

// =============================================================================
// 변수 맵 생성
// =============================================================================

std::map<std::string, std::string>
PayloadTransformer::buildVariableMap(const TransformContext &context) {
  std::map<std::string, std::string> vars;

  // 기본 필드
  vars["building_id"] = std::to_string(context.building_id);
  vars["point_name"] = context.point_name;
  vars["value"] = context.value;
  vars["timestamp"] = context.timestamp;
  vars["status"] = std::to_string(context.status);
  vars["type"] = context.type;

  // 알람 필드
  vars["alarm_flag"] = std::to_string(context.alarm_flag);
  vars["description"] = context.description;

  // 매핑 필드
  vars["target_field_name"] = context.target_field_name;
  vars["target_description"] = context.target_description;
  vars["converted_value"] = context.converted_value;

  // 계산 필드
  vars["timestamp_iso8601"] = context.timestamp_iso8601;
  vars["timestamp_unix_ms"] = std::to_string(context.timestamp_unix_ms);
  vars["alarm_status"] = context.alarm_status;

  // 신규 변수 및 별칭
  vars["original_name"] = context.point_name;
  vars["site_id"] = std::to_string(context.building_id);

  // ✅ 인간 친화적 별칭 (Alias) 추가 (v3.2.0 Agnostic Standard)
  vars["target_key"] = context.target_field_name.empty()
                           ? context.point_name
                           : context.target_field_name;
  vars["mapping_name"] = vars["target_key"];
  vars["measured_value"] =
      context.converted_value.empty() ? context.value : context.converted_value;
  vars["point_value"] = vars["measured_value"];
  vars["status_code"] = std::to_string(context.status);
  vars["alarm_level"] = std::to_string(context.alarm_flag);
  vars["timestamp"] = context.timestamp_iso8601;

  vars["target_description"] = context.target_description.empty()
                                   ? context.description
                                   : context.target_description;

  // Metadata & Intelligence
  vars["site_name"] = context.custom_vars.count("site_name")
                          ? context.custom_vars.at("site_name")
                          : "";
  vars["device_name"] = context.custom_vars.count("device_name")
                            ? context.custom_vars.at("device_name")
                            : "";
  vars["is_control"] = context.custom_vars.count("is_control")
                           ? context.custom_vars.at("is_control")
                           : "0";
  vars["is_writable"] = vars["is_control"];
  vars["data_type"] = vars["type"];

  // [v3.2.0] Legacy Aliases - KEEP for template compatibility
  vars["bd"] = vars["site_id"];
  vars["nm"] = vars["target_key"];
  vars["vl"] = vars["measured_value"];
  vars["tm"] = context.timestamp;
  vars["st"] = vars["status_code"];
  vars["al"] = vars["alarm_level"];
  vars["des"] = vars["target_description"];
  vars["ty"] = vars["data_type"];

  // Internal short keys (il, xl, mi, mx) - required by some templates
  vars["il"] =
      context.custom_vars.count("il") ? context.custom_vars.at("il") : "-";
  vars["xl"] =
      context.custom_vars.count("xl") ? context.custom_vars.at("xl") : "1";
  vars["mi"] =
      context.custom_vars.count("mi") ? context.custom_vars.at("mi") : "[0]";
  vars["mx"] =
      context.custom_vars.count("mx") ? context.custom_vars.at("mx") : "[100]";

  // [v3.0.0] Zero-Assumption Token Pool (Automatically turn all custom_vars
  // into tokens)
  for (const auto &[key, value] : context.custom_vars) {
    if (!vars.count(key)) {
      vars[key] = value;
    }
    // Also support double-brace format explicitly in the map for
    // legacy/specific template cases Note: expandJsonRecursive already handles
    // {{key}}, so this is for string-based transformString calls
    vars["{{" + key + "}}"] = value;
    vars["{" + key + "}"] = value;
  }

  // 함수형 변수
  for (const auto &[name, func] : custom_variable_funcs_) {
    try {
      vars[name] = func(context);
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("커스텀 변수 '" + name +
                                     "' 실패: " + std::string(e.what()));
    }
  }

  return vars;
}

// =============================================================================
// JSON 재귀 치환
// =============================================================================

void PayloadTransformer::expandJsonRecursive(
    json &obj, const std::map<std::string, std::string> &variables) {
  if (obj.is_string()) {
    std::string str = obj.get<std::string>();
    // 1. 일반 변수 치환 ({var} 및 {{var}} 지원)
    for (const auto &[key, value] : variables) {
      // {{var}}
      std::string pattern2 = "{{" + key + "}}";
      size_t pos = 0;
      while ((pos = str.find(pattern2, pos)) != std::string::npos) {
        str.replace(pos, pattern2.length(), value);
        pos += value.length();
      }

      // {var}
      std::string pattern1 = "{" + key + "}";
      pos = 0;
      while ((pos = str.find(pattern1, pos)) != std::string::npos) {
        str.replace(pos, pattern1.length(), value);
        pos += value.length();
      }
    }

    // 2. 매핑 변수 치환 ({map:key:true_val:false_val} 또는
    // {{map:key:true_val:false_val}}) 예: {map:al:발생:복구},
    // {map:st:제어가능:제어불가}, {map:ty:DIGITAL:ANALOG}
    std::regex map_regex("\\{+map:([^:]+):([^:]*):([^}]+)\\}+");
    std::smatch match;
    std::string search_str = str;
    std::string result_str;
    auto words_begin =
        std::sregex_iterator(search_str.begin(), search_str.end(), map_regex);
    auto words_end = std::sregex_iterator();

    size_t last_pos = 0;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
      std::smatch match = *i;
      result_str += search_str.substr(last_pos, match.position() - last_pos);

      std::string key = match[1].str();
      std::string true_val = match[2].str();
      std::string false_val = match[3].str();

      bool condition = false;
      if (variables.count(key)) {
        std::string val = variables.at(key);
        // "1", "true", "ALARM", "DIGITAL" 등을 참으로 간주
        if (key == "ty" || key == "type") {
          condition = (val == "bit" || val == "bool" || val == "DIGITAL");
        } else {
          condition = (val == "1" || val == "true" || val == "active" ||
                       val == "ALARM");
        }
      }

      result_str += condition ? true_val : false_val;
      last_pos = match.position() + match.length();
    }
    result_str += search_str.substr(last_pos);
    str = result_str;

    // 만약 전체 문자열이 "{{var}}" 형태이고, 그 변수값이 JSON 배열/객체
    // 형태라면 문자열이 아닌 원본 타입으로 치환 (예: "[0]" -> [0])
    if (!str.empty() && str.front() == '[' && str.back() == ']') {
      try {
        json parsed = json::parse(str);
        obj = parsed;
        return;
      } catch (...) {
      }
    }

    // 숫자 변환 시도 (완전한 숫자인 경우만 처리)
    if (!str.empty() &&
        (std::isdigit(str[0]) || str[0] == '-' || str[0] == '.')) {
      try {
        size_t processed = 0;
        if (str.find('.') != std::string::npos) {
          double val = std::stod(str, &processed);
          if (processed == str.length()) {
            obj = val;
            return;
          }
        } else {
          long long val = std::stoll(str, &processed);
          if (processed == str.length()) {
            obj = val;
            return;
          }
        }
      } catch (...) {
      }
    }
    obj = str;

  } else if (obj.is_object()) {
    json new_obj = json::object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      std::string key = it.key();

      // ✅ Key Expansion 지원
      for (const auto &[v_key, v_value] : variables) {
        std::string pattern2 = "{{" + v_key + "}}";
        size_t pos = 0;
        while ((pos = key.find(pattern2, pos)) != std::string::npos) {
          key.replace(pos, pattern2.length(), v_value);
          pos += v_value.length();
        }

        std::string pattern1 = "{" + v_key + "}";
        pos = 0;
        while ((pos = key.find(pattern1, pos)) != std::string::npos) {
          key.replace(pos, pattern1.length(), v_value);
          pos += v_value.length();
        }
      }

      json value = it.value();
      expandJsonRecursive(value, variables);
      new_obj[key] = value;
    }
    obj = new_obj;
  } else if (obj.is_array()) {
    for (auto &item : obj) {
      expandJsonRecursive(item, variables);
    }
  }
}

// =============================================================================
// 헬퍼
// =============================================================================

std::string PayloadTransformer::toISO8601(const std::string &alarm_tm) {
  int64_t ms = toUnixTimestampMs(alarm_tm);
  std::time_t t = static_cast<std::time_t>(ms / 1000);

  std::ostringstream oss;
  oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");

  // 밀리초 추가 (.SSS)
  int64_t fractional = ms % 1000;
  oss << "." << std::setfill('0') << std::setw(3) << fractional;

  return oss.str();
}

int64_t PayloadTransformer::toUnixTimestampMs(const std::string &alarm_tm) {
  if (alarm_tm.empty()) {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
  }

  try {
    std::tm tm = {};
    std::istringstream ss(alarm_tm);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    // DB의 시간은 KST 문자열이므로 mktime(local) 사용 (TZ=Asia/Seoul 대응)
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               time_point.time_since_epoch())
        .count();
  } catch (...) {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
  }
}

std::string PayloadTransformer::getAlarmStatusString(int al, int st) {
  if (al == 1) {
    switch (st) {
    case 0:
      return "NORMAL";
    case 1:
      return "WARNING";
    case 2:
      return "CRITICAL";
    case 3:
      return "ACTIVE";
    default:
      return "ACTIVE";
    }
  }
  return "CLEARED";
}

void PayloadTransformer::registerCustomVariable(
    const std::string &name,
    std::function<std::string(const TransformContext &)> value_func) {
  custom_variable_funcs_[name] = value_func;
}

} // namespace Transform
} // namespace PulseOne