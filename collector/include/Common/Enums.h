#ifndef PULSEONE_COMMON_ENUMS_H
#define PULSEONE_COMMON_ENUMS_H

/**
 * @file Enums.h
 * @brief PulseOne 열거형 정의 (점검 기능 포함)
 * @author PulseOne Development Team
 * @date 2025-07-24
 */

#include <cstdint>

namespace PulseOne::Enums {
    
    /**
     * @brief 지원되는 통신 프로토콜 타입
     */
    enum class ProtocolType : uint8_t {
        UNKNOWN = 0,
        MODBUS_TCP = 1,
        MODBUS_RTU = 2,
        MQTT = 3,
        BACNET_IP = 4,
        BACNET_MSTP = 5,
        OPC_UA = 6,
        ETHERNET_IP = 7,
        CUSTOM = 99
    };
    
    /**
     * @brief 데이터 품질 상태 (현장 점검 상황 반영)
     */
    enum class DataQuality : uint8_t {
        GOOD = 0,                    // 정상 데이터
        BAD = 1,                     // 불량 데이터
        UNCERTAIN = 2,               // 불확실한 데이터
        NOT_CONNECTED = 3,           // 연결 끊김
        SCAN_DELAYED = 4,            // 🆕 스캔 지연 (오래된 데이터)
        UNDER_MAINTENANCE = 5,       // 🆕 점검 중 (신뢰할 수 없음)
        STALE_DATA = 6,              // 🆕 오래된 데이터 (30초 이상)
        VERY_STALE_DATA = 7,         // 🆕 매우 오래된 데이터 (5분 이상)
        MAINTENANCE_BLOCKED = 8,     // 🆕 점검으로 인한 차단
        ENGINEER_OVERRIDE = 9        // 🆕 엔지니어 수동 값 설정
    };
    
    /**
     * @brief 디바이스 연결 상태
     */
    enum class ConnectionStatus : uint8_t {
        DISCONNECTED = 0,
        CONNECTING = 1,
        CONNECTED = 2,
        RECONNECTING = 3,
        ERROR = 4,
        MAINTENANCE_MODE = 5         // 🆕 점검 모드
    };
    
    /**
     * @brief 점검 상태 (현장 엔지니어 작업 상태)
     */
    enum class MaintenanceStatus : uint8_t {
        NORMAL = 0,                  // 정상 운영
        MAINTENANCE_REQUESTED = 1,   // 점검 요청됨
        UNDER_MAINTENANCE = 2,       // 점검 중
        MAINTENANCE_COMPLETED = 3,   // 점검 완료
        EMERGENCY_STOP = 4,          // 비상 정지
        REMOTE_BLOCKED = 5           // 원격 제어 차단됨
    };
    
    /**
     * @brief 점검 타입
     */
    enum class MaintenanceType : uint8_t {
        SCHEDULED = 0,               // 예정된 점검
        EMERGENCY = 1,               // 긴급 점검
        ROUTINE = 2,                 // 정기 점검
        DIAGNOSTIC = 3,              // 진단 작업
        CALIBRATION = 4,             // 교정 작업
        REPAIR = 5                   // 수리 작업
    };
    
    /**
     * @brief 원격 제어 권한 상태
     */
    enum class RemoteControlStatus : uint8_t {
        ENABLED = 0,                 // 원격 제어 허용
        DISABLED = 1,                // 원격 제어 비활성화
        BLOCKED_BY_MAINTENANCE = 2,  // 점검으로 인한 차단
        BLOCKED_BY_ENGINEER = 3,     // 엔지니어가 차단
        EMERGENCY_BLOCKED = 4        // 비상 상황으로 차단
    };
    
    /**
     * @brief 에러 코드 (범주별 구분)
     */
    enum class ErrorCode : int32_t {
        // 성공
        SUCCESS = 0,
        
        // 연결 관련 에러 (100-199)
        CONNECTION_FAILED = 100,
        CONNECTION_TIMEOUT = 101,
        CONNECTION_LOST = 102,
        AUTHENTICATION_FAILED = 103,
        INVALID_ENDPOINT = 104,
        
        // 프로토콜 관련 에러 (200-299)
        PROTOCOL_ERROR = 200,
        INVALID_FRAME = 201,
        CHECKSUM_ERROR = 202,
        UNSUPPORTED_FUNCTION = 203,
        INVALID_ADDRESS = 204,
        
        // 데이터 관련 에러 (300-399)
        DATA_FORMAT_ERROR = 300,
        DATA_OUT_OF_RANGE = 301,
        DATA_TYPE_MISMATCH = 302,
        DATA_CONVERSION_ERROR = 303,
        
        // 점검 관련 에러 (400-499)
        MAINTENANCE_ACTIVE = 400,
        REMOTE_CONTROL_BLOCKED = 401,
        INSUFFICIENT_PERMISSION = 402,
        ENGINEER_OVERRIDE_ACTIVE = 403,
        MAINTENANCE_LOCK_FAILED = 404,
        
        // 시스템 관련 에러 (500-599)
        RESOURCE_EXHAUSTED = 500,
        INTERNAL_ERROR = 501,
        CONFIGURATION_ERROR = 502,
        DRIVER_NOT_FOUND = 503,
        DEVICE_NOT_FOUND = 504,
        DEVICE_ERROR = 505,
        INVALID_PARAMETER = 504,
        
        // 🆕 드라이버 시스템용 추가 에러들
        WARNING_DATA_STALE = 1000,
        WARNING_PARTIAL_SUCCESS = 1001,
        FATAL_INTERNAL_ERROR = 2000,
        FATAL_MEMORY_EXHAUSTED = 2001
    };
    
    /**
     * @brief Worker 상태
     */
    enum class WorkerStatus : uint8_t {
        STOPPED = 0,
        STARTING = 1,
        RUNNING = 2,
        STOPPING = 3,
        ERROR = 4,
        MAINTENANCE = 5              // 🆕 점검 모드
    };
    
    /**
     * @brief 로그 레벨 (기존 LogLevels.h와 완전 호환)
     * @note DEBUG 매크로 충돌 방지를 위해 순서 변경
     */
    enum class LogLevel : uint8_t {
        TRACE = 0,
        DEBUG_LEVEL = 1,     // DEBUG 매크로 충돌 방지
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5,
        MAINTENANCE = 6      // 🆕 점검 관련 로그
    };
    
    /**
     * @brief 드라이버 로그 카테고리 (DriverLogger.h에서 이동)
     */
    enum class DriverLogCategory : uint8_t {
        GENERAL = 0,        // 일반적인 드라이버 로그
        CONNECTION = 1,     // 연결 관련 로그
        COMMUNICATION = 2,  // 통신 관련 로그
        DATA_PROCESSING = 3, // 데이터 처리 관련 로그
        ERROR_HANDLING = 4, // 에러 처리 관련 로그
        PERFORMANCE = 5,    // 성능 관련 로그
        SECURITY = 6,       // 보안 관련 로그
        PROTOCOL_SPECIFIC = 7, // 프로토콜별 특수 로그
        DIAGNOSTICS = 8     // 진단 정보 로그
    };
    
    /**
     * @brief 디바이스 타입
     */
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
        PRESSURE_TRANSMITTER = 10
    };
    
    /**
     * @brief 데이터 포인트 접근 타입
     */
    enum class AccessType : uint8_t {
        READ_ONLY = 0,
        WRITE_ONLY = 1,
        READ_WRITE = 2,
        READ_WRITE_MAINTENANCE_ONLY = 3  // 🆕 점검 시에만 쓰기 가능
    };
        
    /**
     * @brief 데이터 타입 열거형 (프로토콜 드라이버에서 사용)
     */
    enum class DataType : uint8_t {
        UNKNOWN = 0,        // 알 수 없는 타입
        BOOL = 1,          // boolean
        INT8 = 2,          // 8bit 정수
        UINT8 = 3,         // 8bit 부호없는 정수  
        INT16 = 4,         // 16bit 정수
        UINT16 = 5,        // 16bit 부호없는 정수
        INT32 = 6,         // 32bit 정수
        UINT32 = 7,        // 32bit 부호없는 정수
        INT64 = 8,         // 64bit 정수 (향후 확장)
        UINT64 = 9,        // 64bit 부호없는 정수 (향후 확장)
        FLOAT32 = 10,      // 32bit 부동소수점 (기존 테스트 코드 호환)
        FLOAT64 = 11,      // 64bit 부동소수점 (DOUBLE 대신)
        STRING = 12,       // 문자열
        BINARY = 13,       // 바이너리 데이터
        DATETIME = 14,     // 날짜/시간 (향후 확장)
        JSON = 15          // JSON 데이터 (MQTT용)
    };
    
} // namespace PulseOne::Enums

#endif // PULSEONE_COMMON_ENUMS_H