// =============================================================================
// core/shared/src/Data/RedisDataTypes.cpp
// Redis 데이터 타입 구현
// =============================================================================

#include "Data/RedisDataTypes.h"

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// CurrentValue 구현
// =============================================================================

nlohmann::json CurrentValue::toJson() const {
    nlohmann::json j;
    j["point_id"] = point_id;
    j["device_id"] = device_id;
    j["device_name"] = device_name;
    j["point_name"] = point_name;
    j["value"] = value;
    j["timestamp"] = timestamp;
    j["quality"] = quality;
    j["data_type"] = data_type;
    j["unit"] = unit;
    j["changed"] = changed;
    return j;
}

std::optional<CurrentValue> CurrentValue::fromJson(const nlohmann::json& j) {
    try {
        CurrentValue cv;
        cv.point_id = j.value("point_id", 0);
        cv.device_id = j.value("device_id", "");
        cv.device_name = j.value("device_name", "");
        cv.point_name = j.value("point_name", "");
        cv.value = j.value("value", "");
        cv.timestamp = j.value("timestamp", 0L);
        cv.quality = j.value("quality", "");
        cv.data_type = j.value("data_type", "");
        cv.unit = j.value("unit", "");
        cv.changed = j.value("changed", false);
        return cv;
    } catch (...) {
        return std::nullopt;
    }
}

// =============================================================================
// VirtualPointValue 구현
// =============================================================================

nlohmann::json VirtualPointValue::toJson() const {
    nlohmann::json j;
    j["point_id"] = point_id;
    j["value"] = value;
    j["timestamp"] = timestamp;
    j["quality"] = quality;
    j["is_virtual"] = is_virtual;
    j["source"] = source;
    return j;
}

std::optional<VirtualPointValue> VirtualPointValue::fromJson(const nlohmann::json& j) {
    try {
        VirtualPointValue vp;
        vp.point_id = j.value("point_id", 0);
        vp.value = j.value("value", "");
        vp.timestamp = j.value("timestamp", 0L);
        vp.quality = j.value("quality", "");
        vp.is_virtual = j.value("is_virtual", true);
        vp.source = j.value("source", "");
        return vp;
    } catch (...) {
        return std::nullopt;
    }
}

// =============================================================================
// ActiveAlarm 구현
// =============================================================================

nlohmann::json ActiveAlarm::toJson() const {
    nlohmann::json j;
    j["tenant_id"] = tenant_id;
    j["rule_id"] = rule_id;
    j["device_id"] = device_id;
    j["point_id"] = point_id;
    j["point_name"] = point_name;
    j["alarm_name"] = alarm_name;
    j["value"] = value;
    j["severity"] = severity;
    j["state"] = state;
    j["message"] = message;
    j["timestamp"] = timestamp;
    j["triggered_at"] = triggered_at;
    j["acknowledged"] = acknowledged;
    j["acknowledged_by"] = acknowledged_by;
    j["acknowledged_at"] = acknowledged_at;
    return j;
}

std::optional<ActiveAlarm> ActiveAlarm::fromJson(const nlohmann::json& j) {
    try {
        ActiveAlarm alarm;
        alarm.tenant_id = j.value("tenant_id", 0);
        alarm.rule_id = j.value("rule_id", 0);
        alarm.device_id = j.value("device_id", "");
        alarm.point_id = j.value("point_id", 0);
        alarm.point_name = j.value("point_name", "");
        alarm.alarm_name = j.value("alarm_name", "");
        alarm.value = j.value("value", "");
        alarm.severity = j.value("severity", "");
        alarm.state = j.value("state", "");
        alarm.message = j.value("message", "");
        alarm.timestamp = j.value("timestamp", 0L);
        alarm.triggered_at = j.value("triggered_at", "");
        alarm.acknowledged = j.value("acknowledged", false);
        alarm.acknowledged_by = j.value("acknowledged_by", "");
        alarm.acknowledged_at = j.value("acknowledged_at", "");
        return alarm;
    } catch (...) {
        return std::nullopt;
    }
}

// =============================================================================
// ExportLogEntry 구현
// =============================================================================

nlohmann::json ExportLogEntry::toJson() const {
    nlohmann::json j;
    j["log_type"] = log_type;
    j["service_id"] = service_id;
    j["target_id"] = target_id;
    j["point_id"] = point_id;
    j["source_value"] = source_value;
    j["converted_value"] = converted_value;
    j["status"] = status;
    j["error_message"] = error_message;
    j["processing_time_ms"] = processing_time_ms;
    j["timestamp"] = timestamp;
    return j;
}

// =============================================================================
// ServiceStatus 구현
// =============================================================================

nlohmann::json ServiceStatus::toJson() const {
    nlohmann::json j;
    j["service_id"] = service_id;
    j["service_type"] = service_type;
    j["service_name"] = service_name;
    j["is_running"] = is_running;
    j["active_connections"] = active_connections;
    j["total_requests"] = total_requests;
    j["successful_requests"] = successful_requests;
    j["failed_requests"] = failed_requests;
    j["last_request_at"] = last_request_at;
    return j;
}

// =============================================================================
// RedisKeyBuilder 구현
// =============================================================================

std::string RedisKeyBuilder::DevicePointKey(int device_num, const std::string& point_name) {
    return "device:" + std::to_string(device_num) + ":" + point_name;
}

std::string RedisKeyBuilder::PointLatestKey(int point_id) {
    return "point:" + std::to_string(point_id) + ":latest";
}

std::string RedisKeyBuilder::VirtualPointKey(int point_id) {
    return "virtualpoint:" + std::to_string(point_id);
}

std::string RedisKeyBuilder::ActiveAlarmKey(int rule_id) {
    return "alarm:active:" + std::to_string(rule_id);
}

std::string RedisKeyBuilder::AlarmCountKey() {
    return "alarms:count:today";
}

std::string RedisKeyBuilder::ExportLogKey(int target_id, int64_t timestamp) {
    return "export:log:" + std::to_string(target_id) + ":" + std::to_string(timestamp);
}

std::string RedisKeyBuilder::ServiceStatusKey(int service_id) {
    return "service:status:" + std::to_string(service_id);
}

std::string RedisKeyBuilder::ExportCounterKey(const std::string& metric_name) {
    return "export:counter:" + metric_name;
}

std::string RedisKeyBuilder::DeviceAllPointsPattern(int device_num) {
    return "device:" + std::to_string(device_num) + ":*";
}

} // namespace Data
} // namespace Shared
} // namespace PulseOne