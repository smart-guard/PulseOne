/**
 * @file PayloadTransformer.cpp
 * @brief 템플릿 기반 Payload 변환기 구현
 * @version 2.0.0
 * 저장 위치: core/export-gateway/src/Transform/PayloadTransformer.cpp
 */

#include "Transform/PayloadTransformer.h"
#include "Logging/LogManager.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace PulseOne {
namespace Transform {

PayloadTransformer::PayloadTransformer() {
    LogManager::getInstance().Debug("PayloadTransformer 초기화");
}

// =============================================================================
// 메인 변환
// =============================================================================

json PayloadTransformer::transform(const json& template_json, const TransformContext& context) {
    try {
        json result = template_json;
        auto variables = buildVariableMap(context);
        expandJsonRecursive(result, variables);
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("PayloadTransformer::transform 실패: " + std::string(e.what()));
        return json::object();
    }
}

std::string PayloadTransformer::transformString(const std::string& template_str, const TransformContext& context) {
    std::string result = template_str;
    auto variables = buildVariableMap(context);
    
    for (const auto& [key, value] : variables) {
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
    return json{
        {"controlpoint", "{{target_field_name}}"},
        {"description", "{{target_description}}"},
        {"value", "{{converted_value}}"},
        {"time", "{{timestamp_iso8601}}"},
        {"status", "{{alarm_status}}"}
    };
}

json PayloadTransformer::getHDCDefaultTemplate() {
    return json{
        {"building_id", "{{building_id}}"},
        {"point_id", "{{target_field_name}}"},
        {"data", json{
            {"value", "{{converted_value}}"},
            {"timestamp", "{{timestamp_unix_ms}}"}
        }},
        {"metadata", json{
            {"description", "{{target_description}}"},
            {"alarm_status", "{{alarm_status}}"}
        }}
    };
}

json PayloadTransformer::getBEMSDefaultTemplate() {
    return json{
        {"buildingId", "{{building_id}}"},
        {"sensorName", "{{target_field_name}}"},
        {"sensorValue", "{{converted_value}}"},
        {"timestamp", "{{timestamp_iso8601}}"},
        {"alarmLevel", "{{alarm_status}}"}
    };
}

json PayloadTransformer::getGenericDefaultTemplate() {
    return json{
        {"building_id", "{{building_id}}"},
        {"point_name", "{{point_name}}"},
        {"value", "{{value}}"},
        {"converted_value", "{{converted_value}}"},
        {"timestamp", "{{timestamp_iso8601}}"},
        {"alarm_status", "{{alarm_status}}"},
        {"source", "PulseOne-ExportGateway"}
    };
}

json PayloadTransformer::getDefaultTemplateForSystem(const std::string& system_type) {
    if (system_type == "insite") return getInsiteDefaultTemplate();
    else if (system_type == "hdc") return getHDCDefaultTemplate();
    else if (system_type == "bems") return getBEMSDefaultTemplate();
    else return getGenericDefaultTemplate();
}

// =============================================================================
// 컨텍스트 생성
// =============================================================================

TransformContext PayloadTransformer::createContext(
    const AlarmMessage& alarm,
    const std::string& target_field_name,
    const std::string& target_description,
    const std::string& converted_value) {
    
    TransformContext context;
    context.alarm = alarm;
    context.target_field_name = target_field_name;
    context.target_description = target_description;
    context.converted_value = converted_value;
    context.timestamp_iso8601 = toISO8601(alarm.tm);
    context.timestamp_unix_ms = toUnixTimestampMs(alarm.tm);
    context.alarm_status = getAlarmStatusString(alarm.al, alarm.st);
    return context;
}

// =============================================================================
// 변수 맵 생성
// =============================================================================

std::map<std::string, std::string> PayloadTransformer::buildVariableMap(const TransformContext& context) {
    std::map<std::string, std::string> vars;
    
    // AlarmMessage 필드
    vars["building_id"] = std::to_string(context.alarm.bd);
    vars["point_name"] = context.alarm.nm;
    vars["value"] = std::to_string(context.alarm.vl);
    vars["timestamp"] = context.alarm.tm;
    vars["alarm_flag"] = std::to_string(context.alarm.al);
    vars["status"] = std::to_string(context.alarm.st);
    vars["description"] = context.alarm.des;
    
    // 매핑 필드
    vars["target_field_name"] = context.target_field_name;
    vars["target_description"] = context.target_description;
    vars["converted_value"] = context.converted_value;
    
    // 계산 필드
    vars["timestamp_iso8601"] = context.timestamp_iso8601;
    vars["timestamp_unix_ms"] = std::to_string(context.timestamp_unix_ms);
    vars["alarm_status"] = context.alarm_status;
    
    // 커스텀 변수
    for (const auto& [key, value] : context.custom_vars) {
        vars[key] = value;
    }
    
    // 함수형 변수
    for (const auto& [name, func] : custom_variable_funcs_) {
        try {
            vars[name] = func(context);
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("커스텀 변수 '" + name + "' 실패: " + std::string(e.what()));
        }
    }
    
    return vars;
}

// =============================================================================
// JSON 재귀 치환
// =============================================================================

void PayloadTransformer::expandJsonRecursive(json& obj, const std::map<std::string, std::string>& variables) {
    if (obj.is_string()) {
        std::string str = obj.get<std::string>();
        
        for (const auto& [key, value] : variables) {
            std::string pattern = "{{" + key + "}}";
            size_t pos = 0;
            while ((pos = str.find(pattern, pos)) != std::string::npos) {
                str.replace(pos, pattern.length(), value);
                pos += value.length();
            }
        }
        
        // 숫자 변환 시도
        if (!str.empty() && (std::isdigit(str[0]) || str[0] == '-')) {
            try {
                if (str.find('.') != std::string::npos) {
                    obj = std::stod(str);
                } else {
                    obj = std::stoll(str);
                }
                return;
            } catch (...) {}
        }
        obj = str;
        
    } else if (obj.is_object()) {
        for (auto& [key, value] : obj.items()) {
            expandJsonRecursive(value, variables);
        }
    } else if (obj.is_array()) {
        for (auto& item : obj) {
            expandJsonRecursive(item, variables);
        }
    }
}

// =============================================================================
// 헬퍼
// =============================================================================

std::string PayloadTransformer::toISO8601(const std::string& alarm_tm) {
    if (alarm_tm.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
    
    if (alarm_tm.find('T') != std::string::npos) return alarm_tm;
    
    std::string iso = alarm_tm;
    size_t space_pos = iso.find(' ');
    if (space_pos != std::string::npos) {
        iso[space_pos] = 'T';
        if (iso.back() != 'Z') iso += "Z";
    }
    return iso;
}

int64_t PayloadTransformer::toUnixTimestampMs(const std::string& alarm_tm) {
    if (alarm_tm.empty()) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
    
    try {
        std::tm tm = {};
        std::istringstream ss(alarm_tm);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
    } catch (...) {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
}

std::string PayloadTransformer::getAlarmStatusString(int al, int st) {
    if (al == 1) {
        switch (st) {
            case 0: return "NORMAL";
            case 1: return "WARNING";
            case 2: return "CRITICAL";
            case 3: return "ACTIVE";
            default: return "ACTIVE";
        }
    }
    return "CLEARED";
}

void PayloadTransformer::registerCustomVariable(
    const std::string& name,
    std::function<std::string(const TransformContext&)> value_func) {
    custom_variable_funcs_[name] = value_func;
}

} // namespace Transform
} // namespace PulseOne