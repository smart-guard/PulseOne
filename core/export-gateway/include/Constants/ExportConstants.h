/**
 * @file ExportConstants.h
 * @brief 수출 게이트웨이 전용 상수 정의
 * @author PulseOne Development Team
 * @date 2026-01-31
 */

#ifndef EXPORT_CONSTANTS_H
#define EXPORT_CONSTANTS_H

#include <string>

namespace PulseOne {
namespace Constants {
namespace Export {

// =============================================================================
// Redis Keys & Channels
// =============================================================================
namespace Redis {
const std::string CHANNEL_ALARMS_PROCESSED = "alarms:processed";
const std::string CHANNEL_CMD_GATEWAY_RESULT = "cmd:gateway:result";
const std::string KEY_GATEWAY_STATUS_PREFIX = "gateway:status:";
const std::string KEY_LAST_SEEN = "lastSeen";
const std::string STATUS_ONLINE = "online";
const std::string STATUS_OFFLINE = "offline";

// Event Subscriber Patterns
const std::string PATTERN_SCHEDULE_EXECUTE = "schedule:execute:*";
const std::string CHANNEL_SCHEDULE_RELOAD = "schedule:reload";
const std::string CHANNEL_CONFIG_RELOAD = "config:reload";
const std::string CHANNEL_TARGET_RELOAD = "target:reload";
const std::string PATTERN_CMD = "cmd:*";
const std::string CHANNEL_ALARMS_ALL = "alarms:all";
} // namespace Redis

// =============================================================================
// Configuration Keys & Defaults
// =============================================================================
namespace Config {
// Database
const std::string DB_PATH_DEFAULT = "/app/data/db/pulseone.db";

// Redis
const std::string REDIS_HOST_DEFAULT = "localhost";
const int REDIS_PORT_DEFAULT = 6379;

// Alarm Processing
const int ALARM_WORKER_THREADS_DEFAULT = 4;
const int ALARM_MAX_QUEUE_SIZE_DEFAULT = 10000;
const int EXPORT_TIMEOUT_DEFAULT = 30;

// Schedule
const int SCHEDULE_CHECK_INTERVAL_DEFAULT = 10;
const int SCHEDULE_RELOAD_INTERVAL_DEFAULT = 60;
const int SCHEDULE_BATCH_SIZE_DEFAULT = 100;
} // namespace Config

// =============================================================================
// Target Types
// =============================================================================
namespace TargetType {
const std::string HTTP = "HTTP";
const std::string HTTPS = "HTTPS";
const std::string MQTT = "MQTT";
const std::string MQTTS = "MQTTS";
const std::string FILE = "FILE";
const std::string S3 = "S3";
} // namespace TargetType

// =============================================================================
// HTTP Constants
// =============================================================================
namespace Http {
const std::string METHOD_GET = "GET";
const std::string METHOD_POST = "POST";
const std::string METHOD_PUT = "PUT";
const std::string METHOD_PATCH = "PATCH";

const std::string HEADER_CONTENT_TYPE = "Content-Type";
const std::string HEADER_ACCEPT = "Accept";
const std::string HEADER_USER_AGENT = "User-Agent";
const std::string HEADER_AUTHORIZATION = "Authorization";
const std::string HEADER_X_REQUEST_ID = "X-Request-ID";
const std::string HEADER_X_TIMESTAMP = "X-Timestamp";

const std::string CONTENT_TYPE_JSON = "application/json";
const std::string CONTENT_TYPE_JSON_UTF8 = "application/json; charset=utf-8";
} // namespace Http

// =============================================================================
// Command Constants
// =============================================================================
namespace Command {
const std::string MANUAL_EXPORT = "manual_export";
const std::string MANUAL_EXPORT_RESULT = "manual_export_result";
} // namespace Command

// =============================================================================
// JSON Key Constants
// =============================================================================
namespace JsonKeys {
const std::string COMMAND = "command";
const std::string PAYLOAD = "payload";
const std::string TARGET = "target";
const std::string TARGET_NAME = "target_name";
const std::string TARGET_ID = "target_id";
const std::string SUCCESS = "success";
const std::string ERROR_MSG = "error";
const std::string COMMAND_ID = "command_id";
const std::string ALL_TARGETS = "ALL";
} // namespace JsonKeys

// =============================================================================
// Configuration Keys (JSON)
// =============================================================================
namespace ConfigKeys {
const std::string MAX_QUEUE_SIZE = "max_queue_size";
const std::string BATCH_SIZE = "batch_size";
const std::string FLUSH_INTERVAL = "flush_interval_ms";
const std::string RETRY_COUNT = "retry_count";
const std::string EXPORT_MODE = "export_mode";
const std::string BODY_TEMPLATE = "body_template";
const std::string FIELD_MAPPINGS = "field_mappings";
const std::string TARGET_FIELD = "target_field";
const std::string POINT_ID = "point_id";
const std::string SITE_ID = "site_id";
const std::string FAILURE_THRESHOLD = "failure_threshold";
const std::string SITE_MAPPING = "site_mapping";
} // namespace ConfigKeys

// =============================================================================
// Export Modes
// =============================================================================
namespace ExportMode {
const std::string ALARM = "alarm";
const std::string VALUE = "value";
const std::string BATCH = "batch";
const std::string EVENT = "EVENT";
} // namespace ExportMode

} // namespace Export
} // namespace Constants
} // namespace PulseOne

#endif // EXPORT_CONSTANTS_H
