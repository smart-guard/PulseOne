// collector/include/Common/Enums.h
#ifndef PULSEONE_COMMON_ENUMS_H
#define PULSEONE_COMMON_ENUMS_H

/**
 * @file Enums.h
 * @brief PulseOne 통합 열거형 정의 (중복 제거)
 * @author PulseOne Development Team
 * @date 2025-08-04
 * @details
 * 🎯 목적: 기존 프로젝트의 모든 중복 열거형들을 하나로 통합
 * 📋 기존 코드 분석 결과:
 * - Utils/LogLevels.h의 LogLevel 통합
 * - 기존 Common/Enums.h의 내용 확장 및 정리
 * - 중복된 에러 코드, 상태값들 통합
 * - 새로운 점검 관련 열거형 추가
 */

#include <cstdint>
#include <string>

namespace PulseOne {
namespace Enums {

    // =========================================================================
    // 🔥 프로토콜 관련 열거형들 (기존 여러 정의 통합)
    // =========================================================================
    
    /**
     * @brief 프로토콜 타입 (모든 중복 정의 통합)
     * @details Modbus, MQTT, BACnet 등 지원하는 모든 프로토콜
     */
    enum class ProtocolType : uint8_t {
        UNKNOWN = 0,
        MODBUS_TCP = 1,
        MODBUS_RTU = 2,
        MODBUS_ASCII = 3,      // 향후 확장
        MQTT = 4,
        MQTT_5 = 5,            // MQTT 5.0 명시적 지원
        BACNET = 6,
        BACNET_IP = 6,
        BACNET_MSTP = 7,
        OPC_UA = 8,            // 향후 확장
        ETHERNET_IP = 9,       // 향후 확장
        PROFINET = 10,         // 향후 확장
        HTTP_REST = 11,        // 향후 확장
        COAP = 12,             // 향후 확장
        CUSTOM = 254,          // 사용자 정의 프로토콜
        MAX_PROTOCOLS = 255
    };

    // =========================================================================
    // 🔥 로그 관련 열거형들 (기존 Utils/LogLevels.h 통합)
    // =========================================================================
    
    /**
     * @brief 로그 레벨 (기존 LogLevels.h와 100% 호환)
     * @details 기존 LogManager와 완전 호환되도록 설계
     */
    enum class LogLevel : uint8_t {
        TRACE = 0,     // 가장 상세한 디버그 정보
        DEBUG = 1,     // 디버그 정보
        DEBUG_LEVEL = 1, // DEBUG 와 같음 호환성 때문에 DEBUG_LEVEL 이름을 쓰는것 뿐임
        INFO = 2,      // 일반 정보 (기본값)
        WARN = 3,      // 경고
        ERROR = 4,     // 에러
        FATAL = 5,     // 치명적 에러
        MAINTENANCE = 6, // 🆕 점검 관련 로그
        OFF = 255      // 로그 비활성화
    };
    
    /**
     * @brief 드라이버 로그 카테고리 (세분화된 로그 분류)
     * @details 프로토콜 드라이버에서 사용하는 로그 카테고리
     */
    enum class DriverLogCategory : uint8_t {
        GENERAL = 0,           // 일반적인 드라이버 로그
        CONNECTION = 1,        // 연결 관련 로그
        COMMUNICATION = 2,     // 통신 관련 로그
        DATA_PROCESSING = 3,   // 데이터 처리 관련 로그
        ERROR_HANDLING = 4,    // 에러 처리 관련 로그
        PERFORMANCE = 5,       // 성능 관련 로그
        SECURITY = 6,          // 보안 관련 로그
        PROTOCOL_SPECIFIC = 7, // 프로토콜별 특수 로그
        DIAGNOSTICS = 8,       // 진단 정보 로그
        MAINTENANCE = 9        // 🆕 점검 관련 로그
    };

    // =========================================================================
    // 🔥 상태 및 연결 관련 열거형들 (중복 제거)
    // =========================================================================
    
    /**
     * @brief 연결 상태 (모든 프로토콜 공통)
     * @details 기존 여러 ConnectionState, ConnectionStatus 통합
     */
    enum class ConnectionStatus : uint8_t {
        DISCONNECTED = 0,      // 연결 해제됨
        CONNECTING = 1,        // 연결 중
        CONNECTED = 2,         // 연결됨
        RECONNECTING = 3,      // 재연결 중
        DISCONNECTING = 4,     // 연결 해제 중
        ERROR = 5,             // 연결 에러
        TIMEOUT = 6,           // 연결 타임아웃
        MAINTENANCE = 7,       // 🆕 점검 모드 (연결 차단)
    };
    
    /**
     * @brief 드라이버 상태 (모든 드라이버 공통)
     * @details 기존 여러 DriverStatus 정의 통합
     */
    enum class DriverStatus : uint8_t {
        UNINITIALIZED = 0,     // 초기화 안됨
        INITIALIZING = 1,      // 초기화 중
        INITIALIZED = 2,       // 초기화 완료
        STARTING = 3,          // 시작 중
        RUNNING = 4,           // 실행 중
        PAUSING = 5,           // 일시정지 중
        PAUSED = 6,            // 일시정지됨
        STOPPING = 7,          // 정지 중
        STOPPED = 8,           // 정지됨
        ERROR = 9,             // 에러 상태
        CRASHED = 10,          // 크래시됨
        MAINTENANCE = 11       // 🆕 점검 모드
    };

    // =========================================================================
    // 🔥 데이터 관련 열거형들
    // =========================================================================
    
    /**
     * @brief 데이터 품질 상태 (확장)
     * @details Utils.h 함수들과 완전 호환되도록 확장
     */
    enum class DataQuality : uint8_t {
        // 기본 품질 상태 (0-7)
        UNKNOWN = 0,                    // 알 수 없음
        GOOD = 1,                       // 정상
        BAD = 2,                        // 불량
        UNCERTAIN = 3,                  // 불확실
        STALE = 4,                      // 오래된 데이터
        MAINTENANCE = 5,                // 점검 중
        SIMULATED = 6,                  // 시뮬레이션 데이터
        MANUAL = 7,                     // 수동 입력 데이터
        
        // 확장 품질 상태 (8-15) - Utils.h 함수 호환
        NOT_CONNECTED = 8,              // 연결 안됨
        TIMEOUT = 9,                    // 타임아웃
        SCAN_DELAYED = 10,              // 스캔 지연
        UNDER_MAINTENANCE = 11,         // 점검 중 (상세)
        STALE_DATA = 12,                // 부실한 데이터
        VERY_STALE_DATA = 13,           // 매우 부실한 데이터
        MAINTENANCE_BLOCKED = 14,       // 점검으로 차단됨
        ENGINEER_OVERRIDE = 15          // 엔지니어 오버라이드
    };
    
    /**
     * @brief 데이터 타입 (모든 프로토콜 공통)
     * @details 기존 여러 DataType 정의 통합
     */
    enum class DataType : uint8_t {
        UNKNOWN = 0,           // 알 수 없는 타입
        BOOL = 1,             // 불린 (1비트)
        INT8 = 2,             // 8비트 정수
        UINT8 = 3,            // 8비트 부호없는 정수
        INT16 = 4,            // 16비트 정수 (Modbus 주요 타입)
        UINT16 = 5,           // 16비트 부호없는 정수
        INT32 = 6,            // 32비트 정수
        UINT32 = 7,           // 32비트 부호없는 정수
        INT64 = 8,            // 64비트 정수
        UINT64 = 9,           // 64비트 부호없는 정수
        FLOAT32 = 10,         // 32비트 부동소수점
        FLOAT64 = 11,         // 64비트 부동소수점 (DOUBLE)
        STRING = 12,          // 문자열 (MQTT JSON, BACnet 문자열)
        BINARY = 13,          // 바이너리 데이터
        DATETIME = 14,        // 날짜/시간
        JSON = 15,            // JSON 객체 (MQTT 특화)
        ARRAY = 16,           // 배열 (향후 확장)
        OBJECT = 17           // 객체 (향후 확장)
    };
    
    /**
     * @brief 접근 타입 (읽기/쓰기 권한)
     */
    enum class AccessType : uint8_t {
        READ_ONLY = 0,        // 읽기 전용
        WRITE_ONLY = 1,       // 쓰기 전용  
        READ_WRITE = 2,       // 읽기/쓰기
        READ_WRITE_MAINTENANCE_ONLY = 3  // 🆕 점검 시에만 쓰기 가능
    };

    // =========================================================================
    // 🔥 에러 및 결과 관련 열거형들 (중복 제거)
    // =========================================================================
    
    /**
     * @brief 통합 에러 코드 (모든 중복 ErrorCode 통합)
     * @details Modbus, MQTT, BACnet 공통 에러 코드
     */
    enum class ErrorCode : uint16_t {
        // 공통 성공/실패
        SUCCESS = 0,
        UNKNOWN_ERROR = 1,
        
        // 연결 관련 에러 (1-99)
        CONNECTION_FAILED = 10,
        CONNECTION_TIMEOUT = 11,
        CONNECTION_REFUSED = 12,
        CONNECTION_LOST = 13,
        AUTHENTICATION_FAILED = 14,
        AUTHORIZATION_FAILED = 15,
        
        // 통신 관련 에러 (100-199)
        TIMEOUT = 100,
        PROTOCOL_ERROR = 101,
        INVALID_REQUEST = 102,
        INVALID_RESPONSE = 103,
        CHECKSUM_ERROR = 104,
        FRAME_ERROR = 105,
        
        // 데이터 관련 에러 (200-299)
        INVALID_DATA = 200,
        DATA_TYPE_MISMATCH = 201,
        DATA_OUT_OF_RANGE = 202,
        DATA_FORMAT_ERROR = 203,
        DATA_STALE = 204,
        
        // 디바이스 관련 에러 (300-399)
        DEVICE_NOT_RESPONDING = 300,
        DEVICE_BUSY = 301,
        DEVICE_ERROR = 302,
        DEVICE_NOT_FOUND = 303,
        DEVICE_OFFLINE = 304,
        
        // 설정 관련 에러 (400-499)
        INVALID_CONFIGURATION = 400,
        MISSING_CONFIGURATION = 401,
        CONFIGURATION_ERROR = 402,
        
        // 시스템 관련 에러 (500-599)
        MEMORY_ERROR = 500,
        RESOURCE_EXHAUSTED = 501,
        INTERNAL_ERROR = 502,
        FILE_ERROR = 503,
        
        // 🆕 점검 관련 에러 (600-699)
        MAINTENANCE_ACTIVE = 600,
        MAINTENANCE_PERMISSION_DENIED = 601,
        MAINTENANCE_TIMEOUT = 602,
        REMOTE_CONTROL_BLOCKED = 603,
        INSUFFICIENT_PERMISSION = 604,
        
        // 프로토콜 특화 에러 (1000+)
        MODBUS_EXCEPTION = 1000,
        MQTT_PUBLISH_FAILED = 1100,
        BACNET_SERVICE_ERROR = 1200
    };

    // =========================================================================
    // 🔥 점검 및 유지보수 관련 열거형들 (신규)
    // =========================================================================
    
    /**
     * @brief 점검 상태 (Maintenance Status)
     * @details 시스템/디바이스의 점검 상태 관리
     */
    enum class MaintenanceStatus : uint8_t {
        NORMAL = 0,            // 정상 운영
        SCHEDULED = 1,         // 점검 예정
        IN_PROGRESS = 2,       // 점검 중
        PAUSED = 3,           // 점검 일시중지
        COMPLETED = 4,        // 점검 완료
        CANCELLED = 5,        // 점검 취소
        EMERGENCY = 6         // 긴급 점검
    };
    
    /**
     * @brief 점검 타입
     */
    enum class MaintenanceType : uint8_t {
        ROUTINE = 0,          // 정기 점검
        PREVENTIVE = 1,       // 예방 점검
        CORRECTIVE = 2,       // 수정 점검
        EMERGENCY = 3,        // 긴급 점검
        CALIBRATION = 4,      // 캘리브레이션
        FIRMWARE_UPDATE = 5,  // 펌웨어 업데이트
        CONFIGURATION = 6     // 설정 변경
    };

    // =========================================================================
    // 🔥 디바이스 관련 열거형들
    // =========================================================================
    
    /**
     * @brief 디바이스 타입
     * @details 산업 현장에서 사용하는 다양한 디바이스 분류
     */
    enum class DeviceType : uint8_t {
        GENERIC = 0,          // 일반 디바이스
        PLC = 1,             // PLC (Programmable Logic Controller)
        HMI = 2,             // HMI (Human Machine Interface)
        SENSOR = 3,          // 센서 
        ACTUATOR = 4,        // 액추에이터
        DRIVE = 5,           // 인버터/드라이브
        GATEWAY = 6,         // 게이트웨이
        ENERGY_METER = 7,    // 전력량계
        TEMPERATURE_CONTROLLER = 8,  // 온도 조절기
        FLOW_METER = 9,      // 유량계
        PRESSURE_TRANSMITTER = 10,   // 압력 전송기
        VALVE = 11,          // 밸브
        PUMP = 12,           // 펌프
        MOTOR = 13,          // 모터
        ROBOT = 14,          // 로봇
        CAMERA = 15,         // 비전 카메라
        SCALE = 16,          // 저울
        BARCODE_READER = 17, // 바코드 리더
        RFID_READER = 18     // RFID 리더
    };

    // =========================================================================
    // 🔥 우선순위 및 레벨 관련 열거형들
    // =========================================================================
    
    /**
     * @brief 작업 우선순위
     * @details 비동기 작업, 알람 등의 우선순위 관리
     */
    enum class Priority : uint8_t {
        LOWEST = 0,
        LOW = 1,
        NORMAL = 2,
        HIGH = 3,
        HIGHEST = 4,
        CRITICAL = 5
    };
    
    /**
     * @brief 알람 레벨
     * @details 알람의 심각도 분류
     */
    enum class AlarmLevel : uint8_t {
        INFO = 0,            // 정보성 알람
        WARNING = 1,         // 경고 알람
        MINOR = 2,           // 경미한 알람
        MAJOR = 3,           // 중요한 알람
        CRITICAL = 4,        // 치명적 알람
        EMERGENCY = 5        // 비상 알람
    };

} // namespace Enums
} // namespace PulseOne

#endif // PULSEONE_COMMON_ENUMS_H