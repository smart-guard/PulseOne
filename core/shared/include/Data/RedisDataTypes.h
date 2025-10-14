// =============================================================================
// core/shared/include/Data/RedisDataTypes.h
// Redis 데이터 타입 정의 (공통)
// =============================================================================

#ifndef REDIS_DATA_TYPES_H
#define REDIS_DATA_TYPES_H

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// CurrentValue - device:{num}:{name}, point:{id}:latest
// =============================================================================
struct CurrentValue {
    int point_id;
    std::string device_id;
    std::string device_name;
    std::string point_name;
    std::string value;
    int64_t timestamp;
    std::string quality;
    std::string data_type;
    std::string unit;
    bool changed;
    
    nlohmann::json toJson() const;
    static std::optional<CurrentValue> fromJson(const nlohmann::json& j);
};

// =============================================================================
// VirtualPointValue - virtualpoint:{id}
// =============================================================================
struct VirtualPointValue {
    int point_id;
    std::string value;
    int64_t timestamp;
    std::string quality;
    bool is_virtual;
    std::string source;
    
    nlohmann::json toJson() const;
    static std::optional<VirtualPointValue> fromJson(const nlohmann::json& j);
};

// =============================================================================
// ActiveAlarm - alarm:active:{rule_id}
// =============================================================================
struct ActiveAlarm {
    int tenant_id;
    int rule_id;
    std::string device_id;
    int point_id;
    std::string point_name;
    std::string alarm_name;
    std::string value;
    std::string severity;
    std::string state;
    std::string message;
    int64_t timestamp;
    std::string triggered_at;
    bool acknowledged;
    std::string acknowledged_by;
    std::string acknowledged_at;
    
    nlohmann::json toJson() const;
    static std::optional<ActiveAlarm> fromJson(const nlohmann::json& j);
};

// =============================================================================
// BatchReadResult
// =============================================================================
struct BatchReadResult {
    std::vector<CurrentValue> values;
    std::vector<int> failed_point_ids;
    int total_requested;
    int successful;
    int failed;
    double elapsed_ms;
    
    BatchReadResult() : total_requested(0), successful(0), failed(0), elapsed_ms(0.0) {}
};

// =============================================================================
// ExportLogEntry
// =============================================================================
struct ExportLogEntry {
    std::string log_type;
    int service_id;
    int target_id;
    int point_id;
    std::string source_value;
    std::string converted_value;
    std::string status;
    std::string error_message;
    int processing_time_ms;
    int64_t timestamp;
    
    nlohmann::json toJson() const;
};

// =============================================================================
// ServiceStatus
// =============================================================================
struct ServiceStatus {
    int service_id;
    std::string service_type;
    std::string service_name;
    bool is_running;
    int active_connections;
    int total_requests;
    int successful_requests;
    int failed_requests;
    int64_t last_request_at;
    
    nlohmann::json toJson() const;
};

// =============================================================================
// RedisKeyBuilder - 키 생성 헬퍼
// =============================================================================
class RedisKeyBuilder {
public:
    static std::string DevicePointKey(int device_num, const std::string& point_name);
    static std::string PointLatestKey(int point_id);
    static std::string VirtualPointKey(int point_id);
    static std::string ActiveAlarmKey(int rule_id);
    static std::string AlarmCountKey();
    static std::string ExportLogKey(int target_id, int64_t timestamp);
    static std::string ServiceStatusKey(int service_id);
    static std::string ExportCounterKey(const std::string& metric_name);
    static std::string DeviceAllPointsPattern(int device_num);
};

// =============================================================================
// PubSubChannels - 채널 이름
// =============================================================================
class PubSubChannels {
public:
    static constexpr const char* ALARMS_ALL = "alarms:all";
    static constexpr const char* ALARMS_CRITICAL = "alarms:critical";
    static constexpr const char* ALARMS_HIGH = "alarms:high";
};

} // namespace Data
} // namespace Shared
} // namespace PulseOne

#endif // REDIS_DATA_TYPES_H