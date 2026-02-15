// collector/include/Common/Enums.h
#ifndef PULSEONE_COMMON_ENUMS_H
#define PULSEONE_COMMON_ENUMS_H

#include <cstdint>
#include <string>

// =============================================================================
// Windows 매크로 충돌 방지 - 반드시 enum 정의 전에!
// =============================================================================
#ifdef _WIN32
// Windows.h의 ERROR 매크로 제거
#ifdef ERROR
#undef ERROR
#endif

// Windows.h의 다른 충돌 가능한 매크로들 제거
#ifdef FATAL
#undef FATAL
#endif

#ifdef MAINTENANCE
#undef MAINTENANCE
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

// Windows 전용 정의
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

// 🔥 NEW: Linux/system headers might define INFO, DEBUG, or WARN as macros
// 이것은 Windows 뿐만 아니라 Linux 환경에서도 충돌을 일으킬 수 있습니다.
#ifdef INFO
#undef INFO
#endif

#ifdef DEBUG
#undef DEBUG
#endif

#ifdef WARN
#undef WARN
#endif

namespace PulseOne {
namespace Enums {

// =========================================================================
// 로그 레벨 - Windows 호환
// =========================================================================
enum class LogLevel : uint8_t {
  TRACE = 0,
  DEBUG = 1,
  DEBUG_LEVEL = 1,
  INFO = 2,
  WARN = 3,
  LOG_ERROR = 4, // 🔥 ERROR → LOG_ERROR로 변경 (Windows 매크로 충돌 완전 방지)
  LOG_FATAL = 5, // Windows FATAL 매크로와 충돌 방지됨
  MAINTENANCE = 6,
  OFF = 255
};

// =========================================================================
// 드라이버 로그 카테고리
// =========================================================================
enum class DriverLogCategory : uint8_t {
  GENERAL = 0,
  CONNECTION = 1,
  COMMUNICATION = 2,
  DATA_PROCESSING = 3,
  ERROR_HANDLING = 4,
  PERFORMANCE = 5,
  SECURITY = 6,
  PROTOCOL_SPECIFIC = 7,
  DIAGNOSTICS = 8,
  MAINTENANCE = 9
};

// =========================================================================
// 연결 상태 - Windows 호환
// =========================================================================
enum class ConnectionStatus : uint8_t {
  DISCONNECTED = 0,
  CONNECTING = 1,
  CONNECTED = 2,
  RECONNECTING = 3,
  DISCONNECTING = 4,
  ERROR = 5, // Windows ERROR 매크로와 충돌 방지됨
  TIMEOUT = 6,
  MAINTENANCE = 7
};

// =========================================================================
// 드라이버 상태 - Windows 호환
// =========================================================================
enum class DriverStatus : uint8_t {
  UNINITIALIZED = 0,
  INITIALIZING = 1,
  INITIALIZED = 2,
  STARTING = 3,
  RUNNING = 4,
  PAUSING = 5,
  PAUSED = 6,
  STOPPING = 7,
  STOPPED = 8,
  DRIVER_ERROR = 9, // Windows ERROR 매크로와 충돌 방지됨
  CRASHED = 10,
  MAINTENANCE = 11
};

// =========================================================================
// 데이터 품질
// =========================================================================
enum class DataQuality : uint8_t {
  UNKNOWN = 0,
  GOOD = 1,
  BAD = 2,
  UNCERTAIN = 3,
  STALE = 4,
  MAINTENANCE = 5,
  SIMULATED = 6,
  MANUAL = 7,
  NOT_CONNECTED = 8,
  TIMEOUT = 9,
  SCAN_DELAYED = 10,
  UNDER_MAINTENANCE = 11,
  STALE_DATA = 12,
  VERY_STALE_DATA = 13,
  MAINTENANCE_BLOCKED = 14,
  ENGINEER_OVERRIDE = 15
};

// =========================================================================
// 데이터 타입
// =========================================================================
enum class DataType : uint8_t {
  UNKNOWN = 0,
  BOOL = 1,
  INT8 = 2,
  UINT8 = 3,
  INT16 = 4,
  UINT16 = 5,
  INT32 = 6,
  UINT32 = 7,
  INT64 = 8,
  UINT64 = 9,
  FLOAT32 = 10,
  FLOAT64 = 11,
  STRING = 12,
  BINARY = 13,
  DATETIME = 14,
  JSON = 15,
  ARRAY = 16,
  OBJECT = 17
};

// =========================================================================
// 접근 타입
// =========================================================================
enum class AccessType : uint8_t {
  READ_ONLY = 0,
  WRITE_ONLY = 1,
  READ_WRITE = 2,
  READ_WRITE_MAINTENANCE_ONLY = 3
};

// =========================================================================
// 에러 코드 - Windows 호환
// =========================================================================
enum class ErrorCode : uint16_t {
  SUCCESS = 0,
  UNKNOWN_ERROR = 1,

  // 연결 관련
  CONNECTION_FAILED = 10,
  CONNECTION_TIMEOUT = 11,
  CONNECTION_REFUSED = 12,
  CONNECTION_LOST = 13,
  AUTHENTICATION_FAILED = 14,
  AUTHORIZATION_FAILED = 15,

  // 통신 관련
  TIMEOUT = 100,
  PROTOCOL_ERROR = 101,
  INVALID_REQUEST = 102,
  INVALID_RESPONSE = 103,
  CHECKSUM_ERROR = 104,
  INVALID_ENDPOINT = 105,
  FRAME_ERROR = 106,

  // 데이터 관련
  INVALID_DATA = 200,
  DATA_TYPE_MISMATCH = 201,
  DATA_OUT_OF_RANGE = 202,
  DATA_FORMAT_ERROR = 203,
  UNSUPPORTED_FUNCTION = 204,
  DATA_STALE = 205,

  // 디바이스 관련
  DEVICE_NOT_RESPONDING = 300,
  DEVICE_BUSY = 301,
  DEVICE_ERROR = 302,
  DEVICE_NOT_FOUND = 303,
  DEVICE_OFFLINE = 304,

  // 설정 관련
  INVALID_CONFIGURATION = 400,
  MISSING_CONFIGURATION = 401,
  CONFIGURATION_ERROR = 402,
  INVALID_PARAMETER = 403,

  // 시스템 관련
  MEMORY_ERROR = 500,
  RESOURCE_EXHAUSTED = 501,
  INTERNAL_ERROR = 502,
  FILE_ERROR = 503,

  // 점검 관련
  MAINTENANCE_ACTIVE = 600,
  MAINTENANCE_PERMISSION_DENIED = 601,
  MAINTENANCE_TIMEOUT = 602,
  REMOTE_CONTROL_BLOCKED = 603,
  INSUFFICIENT_PERMISSION = 604,

  // 프로토콜 특화
  MODBUS_EXCEPTION = 1000,
  MQTT_PUBLISH_FAILED = 1100,
  BACNET_SERVICE_ERROR = 1200
};

// =========================================================================
// 점검 상태
// =========================================================================
enum class MaintenanceStatus : uint8_t {
  NORMAL = 0,
  SCHEDULED = 1,
  IN_PROGRESS = 2,
  PAUSED = 3,
  COMPLETED = 4,
  CANCELLED = 5,
  EMERGENCY = 6
};

// =========================================================================
// 점검 타입
// =========================================================================
enum class MaintenanceType : uint8_t {
  ROUTINE = 0,
  PREVENTIVE = 1,
  CORRECTIVE = 2,
  EMERGENCY = 3,
  CALIBRATION = 4,
  FIRMWARE_UPDATE = 5,
  CONFIGURATION = 6
};

// =========================================================================
// 디바이스 타입
// =========================================================================
enum class DeviceType : uint8_t {
  GENERIC = 0,
  PLC = 1,
  HMI = 2,
  SENSOR = 3,
  ACTUATOR = 4,
  DRIVE = 5,
  GATEWAY = 6,
  ENERGY_METER = 7,
  TEMPERATURE_CONTROLLER = 8,
  FLOW_METER = 9,
  PRESSURE_TRANSMITTER = 10,
  VALVE = 11,
  PUMP = 12,
  MOTOR = 13,
  ROBOT = 14,
  CAMERA = 15,
  SCALE = 16,
  BARCODE_READER = 17,
  RFID_READER = 18
};

// =========================================================================
// 우선순위
// =========================================================================
enum class Priority : uint8_t {
  LOWEST = 0,
  LOW = 1,
  NORMAL = 2,
  HIGH = 3,
  HIGHEST = 4,
  CRITICAL = 5
};

// =========================================================================
// 알람 레벨
// =========================================================================
enum class AlarmLevel : uint8_t {
  INFO = 0,
  WARNING = 1,
  MINOR = 2,
  MAJOR = 3,
  CRITICAL = 4,
  EMERGENCY = 5
};

// =========================================================================
// 접근 모드 (WorkerFactory 호환)
// =========================================================================
enum class AccessMode : uint8_t { read = 0, write = 1, read_write = 2 };

// =========================================================================
// Modbus 레지스터 타입
// =========================================================================
enum class ModbusRegisterType : uint8_t {
  COIL = 0,
  DISCRETE_INPUT = 1,
  HOLDING_REGISTER = 2,
  INPUT_REGISTER = 3
};

// =========================================================================
// 디바이스 상태 - Windows 호환
// =========================================================================
enum class DeviceStatus : uint8_t {
  ONLINE = 0,
  OFFLINE = 1,
  MAINTENANCE = 2,
  DEVICE_ERROR = 3, // Windows ERROR 매크로와 충돌 방지됨
  WARNING = 4
};

// =========================================================================
// 문자열 변환 함수들 (인라인) - Windows 호환
// =========================================================================

inline std::string DeviceStatusToString(DeviceStatus status) {
  switch (status) {
  case DeviceStatus::ONLINE:
    return "ONLINE";
  case DeviceStatus::OFFLINE:
    return "OFFLINE";
  case DeviceStatus::MAINTENANCE:
    return "MAINTENANCE";
  case DeviceStatus::DEVICE_ERROR:
    return "ERROR";
  case DeviceStatus::WARNING:
    return "WARNING";
  default:
    return "UNKNOWN";
  }
}

inline DeviceStatus StringToDeviceStatus(const std::string &status_str) {
  if (status_str == "ONLINE")
    return DeviceStatus::ONLINE;
  if (status_str == "OFFLINE")
    return DeviceStatus::OFFLINE;
  if (status_str == "MAINTENANCE")
    return DeviceStatus::MAINTENANCE;
  if (status_str == "ERROR")
    return DeviceStatus::DEVICE_ERROR;
  if (status_str == "WARNING")
    return DeviceStatus::WARNING;
  return DeviceStatus::OFFLINE;
}

inline std::string LogLevelToString(LogLevel level) {
  switch (level) {
  case LogLevel::TRACE:
    return "TRACE";
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARN:
    return "WARN";
  case LogLevel::LOG_ERROR:
    return "ERROR";
  case LogLevel::LOG_FATAL:
    return "FATAL";
  case LogLevel::MAINTENANCE:
    return "MAINTENANCE";
  case LogLevel::OFF:
    return "OFF";
  default:
    return "UNKNOWN";
  }
}

inline std::string ConnectionStatusToString(ConnectionStatus status) {
  switch (status) {
  case ConnectionStatus::DISCONNECTED:
    return "DISCONNECTED";
  case ConnectionStatus::CONNECTING:
    return "CONNECTING";
  case ConnectionStatus::CONNECTED:
    return "CONNECTED";
  case ConnectionStatus::RECONNECTING:
    return "RECONNECTING";
  case ConnectionStatus::DISCONNECTING:
    return "DISCONNECTING";
  case ConnectionStatus::ERROR:
    return "ERROR";
  case ConnectionStatus::TIMEOUT:
    return "TIMEOUT";
  case ConnectionStatus::MAINTENANCE:
    return "MAINTENANCE";
  default:
    return "UNKNOWN";
  }
}

inline std::string ErrorCodeToString(ErrorCode code) {
  switch (code) {
  case ErrorCode::SUCCESS:
    return "SUCCESS";
  case ErrorCode::UNKNOWN_ERROR:
    return "UNKNOWN_ERROR";
  case ErrorCode::CONNECTION_FAILED:
    return "CONNECTION_FAILED";
  case ErrorCode::CONNECTION_TIMEOUT:
    return "CONNECTION_TIMEOUT";
  case ErrorCode::DEVICE_NOT_RESPONDING:
    return "DEVICE_NOT_RESPONDING";
  case ErrorCode::INVALID_DATA:
    return "INVALID_DATA";
  case ErrorCode::MODBUS_EXCEPTION:
    return "MODBUS_EXCEPTION";
  case ErrorCode::MQTT_PUBLISH_FAILED:
    return "MQTT_PUBLISH_FAILED";
  case ErrorCode::BACNET_SERVICE_ERROR:
    return "BACNET_SERVICE_ERROR";
  default:
    return "UNKNOWN_ERROR";
  }
}

// =========================================================================
// 🔥 DriverStatus 문자열 변환 헬퍼 함수
// =========================================================================

/**
 * @brief DriverStatus → 문자열 변환
 */
inline std::string DriverStatusToString(DriverStatus status) {
  switch (status) {
  case DriverStatus::UNINITIALIZED:
    return "UNINITIALIZED";
  case DriverStatus::INITIALIZING:
    return "INITIALIZING";
  case DriverStatus::INITIALIZED:
    return "INITIALIZED";
  case DriverStatus::STARTING:
    return "STARTING";
  case DriverStatus::RUNNING:
    return "RUNNING";
  case DriverStatus::PAUSING:
    return "PAUSING";
  case DriverStatus::PAUSED:
    return "PAUSED";
  case DriverStatus::STOPPING:
    return "STOPPING";
  case DriverStatus::STOPPED:
    return "STOPPED";
  case DriverStatus::DRIVER_ERROR:
    return "DRIVER_ERROR"; // 🔥 변경된 이름
  case DriverStatus::CRASHED:
    return "CRASHED";
  case DriverStatus::MAINTENANCE:
    return "MAINTENANCE";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief 문자열 → DriverStatus 변환
 */
inline DriverStatus StringToDriverStatus(const std::string &status_str) {
  if (status_str == "UNINITIALIZED")
    return DriverStatus::UNINITIALIZED;
  if (status_str == "INITIALIZING")
    return DriverStatus::INITIALIZING;
  if (status_str == "INITIALIZED")
    return DriverStatus::INITIALIZED;
  if (status_str == "STARTING")
    return DriverStatus::STARTING;
  if (status_str == "RUNNING")
    return DriverStatus::RUNNING;
  if (status_str == "PAUSING")
    return DriverStatus::PAUSING;
  if (status_str == "PAUSED")
    return DriverStatus::PAUSED;
  if (status_str == "STOPPING")
    return DriverStatus::STOPPING;
  if (status_str == "STOPPED")
    return DriverStatus::STOPPED;
  if (status_str == "DRIVER_ERROR" || status_str == "ERROR")
    return DriverStatus::DRIVER_ERROR; // 🔥 기존 호환성
  if (status_str == "CRASHED")
    return DriverStatus::CRASHED;
  if (status_str == "MAINTENANCE")
    return DriverStatus::MAINTENANCE;
  return DriverStatus::UNINITIALIZED;
}

// =========================================================================
// 🔥 기존 코드 호환성을 위한 상수 정의
// =========================================================================
namespace DriverStatusCompat {
constexpr uint8_t UNINITIALIZED = 0;
constexpr uint8_t INITIALIZING = 1;
constexpr uint8_t INITIALIZED = 2;
constexpr uint8_t STARTING = 3;
constexpr uint8_t RUNNING = 4;
constexpr uint8_t PAUSING = 5;
constexpr uint8_t PAUSED = 6;
constexpr uint8_t STOPPING = 7;
constexpr uint8_t STOPPED = 8;
constexpr uint8_t DRIVER_ERROR = 9; // 🔥 ERROR → DRIVER_ERROR
constexpr uint8_t CRASHED = 10;
constexpr uint8_t MAINTENANCE = 11;
} // namespace DriverStatusCompat

} // namespace Enums
} // namespace PulseOne

#endif // PULSEONE_COMMON_ENUMS_H