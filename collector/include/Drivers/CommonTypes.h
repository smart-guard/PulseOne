/**
 * @file CommonTypes.h
 * @brief 드라이버 시스템 공통 타입 및 상수 정의
 * @details 기존 PulseOne 코드와 호환되는 새로운 드라이버 시스템용 타입들
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 1.0.0
 */

#ifndef DRIVERS_COMMON_TYPES_H
#define DRIVERS_COMMON_TYPES_H

// 표준 라이브러리 (C++17 기준)
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <variant>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <future>

// 기존 PulseOne 헤더들과의 호환성을 위한 인클루드
// 실제 경로는 기존 코드 구조에 맞게 조정 필요
#include "Utils/LogLevels.h"  // 기존 로그 레벨 사용

namespace PulseOne {
namespace Drivers {

/**
 * @namespace Constants
 * @brief 드라이버 시스템 전역 상수들
 * @details C/C++ 개발자 효율성을 고려한 컴파일 타임 상수 정의
 */
namespace Constants {
    // 기본 통신 설정 상수
    constexpr int DEFAULT_POLLING_INTERVAL_MS = 1000;    ///< 기본 폴링 간격 (밀리초)
    constexpr int DEFAULT_TIMEOUT_MS = 5000;             ///< 기본 타임아웃 (밀리초)
    constexpr int DEFAULT_RETRY_COUNT = 3;               ///< 기본 재시도 횟수
    constexpr int DEFAULT_THREAD_POOL_SIZE = 4;          ///< 기본 스레드 풀 크기
    
    // 데이터 크기 제한 상수
    constexpr size_t MAX_DEVICE_NAME_LENGTH = 100;       ///< 디바이스 이름 최대 길이
    constexpr size_t MAX_ENDPOINT_LENGTH = 255;          ///< 엔드포인트 주소 최대 길이
    constexpr size_t MAX_ERROR_MESSAGE_LENGTH = 512;     ///< 에러 메시지 최대 길이
    constexpr size_t MAX_CONFIG_JSON_LENGTH = 4096;      ///< 설정 JSON 최대 길이
    
    // 프로토콜별 기본 포트 상수
    constexpr int DEFAULT_MODBUS_TCP_PORT = 502;         ///< Modbus TCP 기본 포트
    constexpr int DEFAULT_MQTT_PORT = 1883;              ///< MQTT 기본 포트
    constexpr int DEFAULT_MQTT_SSL_PORT = 8883;          ///< MQTT SSL 기본 포트
    constexpr int DEFAULT_BACNET_PORT = 47808;           ///< BACnet IP 기본 포트
    constexpr int DEFAULT_OPC_UA_PORT = 4840;            ///< OPC UA 기본 포트
    
    // 성능 및 메모리 관련 상수
    constexpr int MAX_CONCURRENT_CONNECTIONS = 1000;     ///< 최대 동시 연결 수
    constexpr int MAX_QUEUE_SIZE = 10000;                ///< 최대 큐 크기
    constexpr int HEARTBEAT_INTERVAL_MS = 30000;         ///< 하트비트 간격 (30초)
    constexpr int STATISTICS_UPDATE_INTERVAL_MS = 10000; ///< 통계 업데이트 간격 (10초)
    
    // Modbus 프로토콜 제한 상수
    constexpr int MODBUS_MAX_READ_REGISTERS = 125;       ///< Modbus 최대 레지스터 읽기 수
    constexpr int MODBUS_MAX_READ_COILS = 2000;          ///< Modbus 최대 코일 읽기 수
    constexpr int MODBUS_MAX_WRITE_REGISTERS = 123;      ///< Modbus 최대 레지스터 쓰기 수
    constexpr int MODBUS_MAX_WRITE_COILS = 1968;         ///< Modbus 최대 코일 쓰기 수
}

/**
 * @brief 지원하는 프로토콜 타입 열거형
 * @details 타입 안전성을 보장하는 scoped enum 사용
 */
enum class ProtocolType : uint8_t {
    UNKNOWN = 0,        ///< 알 수 없는 프로토콜
    MODBUS_TCP,         ///< Modbus TCP/IP
    MODBUS_RTU,         ///< Modbus RTU (시리얼)
    MQTT,               ///< MQTT 프로토콜
    BACNET_IP,          ///< BACnet over IP
    OPC_UA,             ///< OPC Unified Architecture
    ETHERNET_IP,        ///< EtherNet/IP
    PROFINET,           ///< PROFINET
    CUSTOM              ///< 사용자 정의 프로토콜
};

/**
 * @brief 데이터 타입 열거형
 * @details 산업용 프로토콜에서 일반적으로 사용되는 데이터 타입들
 */
enum class DataType : uint8_t {
    UNKNOWN = 0,        ///< 알 수 없는 타입
    BOOL,               ///< 불린 (1비트)
    INT8,               ///< 8비트 부호 있는 정수
    UINT8,              ///< 8비트 부호 없는 정수
    INT16,              ///< 16비트 부호 있는 정수 (리틀 엔디안)
    UINT16,             ///< 16비트 부호 없는 정수 (리틀 엔디안)
    INT32,              ///< 32비트 부호 있는 정수 (리틀 엔디안)
    UINT32,             ///< 32비트 부호 없는 정수 (리틀 엔디안)
    INT64,              ///< 64비트 부호 있는 정수 (리틀 엔디안)
    UINT64,             ///< 64비트 부호 없는 정수 (리틀 엔디안)
    FLOAT32,            ///< 32비트 부동소수점 (IEEE 754)
    FLOAT64,            ///< 64비트 부동소수점 (IEEE 754)
    STRING,             ///< UTF-8 문자열
    BINARY,             ///< 바이너리 데이터
    TIMESTAMP           ///< 타임스탬프 (ISO 8601)
};

/**
 * @brief 연결 상태 열거형
 * @details 디바이스 연결 상태를 명확히 구분
 */
enum class ConnectionStatus : uint8_t {
    DISCONNECTED = 0,   ///< 연결 해제됨
    CONNECTING,         ///< 연결 시도 중
    CONNECTED,          ///< 정상 연결됨
    RECONNECTING,       ///< 재연결 시도 중
    ERROR,              ///< 연결 오류 상태
    MAINTENANCE,        ///< 유지보수 모드
    TIMEOUT,            ///< 연결 타임아웃
    UNAUTHORIZED        ///< 인증 실패
};

/**
 * @brief 드라이버 상태 열거형
 * @details 드라이버 생명주기 상태 관리
 */
enum class DriverStatus : uint8_t {
    UNINITIALIZED = 0,  ///< 초기화되지 않음
    INITIALIZING,       ///< 초기화 중
    INITIALIZED,        ///< 초기화 완료
    STARTING,           ///< 시작 중
    RUNNING,            ///< 정상 동작 중
    PAUSING,            ///< 일시정지 중
    PAUSED,             ///< 일시정지됨
    STOPPING,           ///< 정지 중
    STOPPED,            ///< 정지됨
    ERROR,              ///< 오류 상태
    CRASHED             ///< 크래시 상태
};

/**
 * @brief 데이터 품질 열거형
 * @details OPC UA 표준을 참고한 데이터 품질 정의
 */
enum class DataQuality : uint8_t {
    GOOD = 0,           ///< 정상 데이터
    BAD,                ///< 불량 데이터
    UNCERTAIN,          ///< 불확실한 데이터
    NOT_CONNECTED,      ///< 연결되지 않음
    DEVICE_FAILURE,     ///< 디바이스 장애
    SENSOR_FAILURE,     ///< 센서 장애
    COMMUNICATION_FAILURE, ///< 통신 장애
    OUT_OF_SERVICE,     ///< 서비스 중단
    MAINTENANCE         ///< 유지보수 중
};

/**
 * @brief 알람 우선순위 열거형
 * @details 산업용 시스템의 일반적인 알람 분류
 */
enum class AlarmPriority : uint8_t {
    LOW = 0,            ///< 낮은 우선순위 (정보성)
    MEDIUM,             ///< 중간 우선순위 (주의)
    HIGH,               ///< 높은 우선순위 (경고)
    CRITICAL,           ///< 치명적 우선순위 (위험)
    EMERGENCY           ///< 비상 우선순위 (즉시 대응 필요)
};

/**
 * @brief 에러 코드 열거형
 * @details 시스템 전체에서 사용할 표준 에러 코드
 */
enum class ErrorCode : int32_t {
    SUCCESS = 0,                    ///< 성공
    
    // 일반적인 에러 (1-99)
    UNKNOWN_ERROR = 1,              ///< 알 수 없는 에러
    INVALID_PARAMETER = 2,          ///< 잘못된 매개변수
    NULL_POINTER = 3,               ///< 널 포인터 접근
    OUT_OF_MEMORY = 4,              ///< 메모리 부족
    NOT_IMPLEMENTED = 5,            ///< 구현되지 않음
    ACCESS_DENIED = 6,              ///< 접근 거부
    RESOURCE_BUSY = 7,              ///< 리소스 사용 중
    
    // 연결 관련 에러 (100-199)
    CONNECTION_FAILED = 100,        ///< 연결 실패
    CONNECTION_TIMEOUT = 101,       ///< 연결 타임아웃
    CONNECTION_REFUSED = 102,       ///< 연결 거부
    CONNECTION_LOST = 103,          ///< 연결 끊김
    AUTHENTICATION_FAILED = 104,    ///< 인증 실패
    AUTHORIZATION_FAILED = 105,     ///< 권한 부족
    
    // 프로토콜 관련 에러 (200-299)
    PROTOCOL_ERROR = 200,           ///< 프로토콜 에러
    INVALID_MESSAGE_FORMAT = 201,   ///< 잘못된 메시지 형식
    UNSUPPORTED_FUNCTION = 202,     ///< 지원하지 않는 기능
    CHECKSUM_ERROR = 203,           ///< 체크섬 에러
    SEQUENCE_ERROR = 204,           ///< 시퀀스 에러
    
    // 디바이스 관련 에러 (300-399)
    DEVICE_NOT_FOUND = 300,         ///< 디바이스를 찾을 수 없음
    DEVICE_BUSY = 301,              ///< 디바이스 사용 중
    DEVICE_ERROR = 302,             ///< 디바이스 에러
    REGISTER_NOT_FOUND = 303,       ///< 레지스터를 찾을 수 없음
    INVALID_ADDRESS = 304,          ///< 잘못된 주소
    
    // 데이터 관련 에러 (400-499)
    DATA_TYPE_MISMATCH = 400,       ///< 데이터 타입 불일치
    DATA_OUT_OF_RANGE = 401,        ///< 데이터 범위 초과
    DATA_CORRUPTION = 402,          ///< 데이터 손상
    BUFFER_OVERFLOW = 403,          ///< 버퍼 오버플로우
    BUFFER_UNDERFLOW = 404,         ///< 버퍼 언더플로우
    
    // 시스템 관련 에러 (500-599)
    SYSTEM_ERROR = 500,             ///< 시스템 에러
    FILE_NOT_FOUND = 501,           ///< 파일을 찾을 수 없음
    FILE_ACCESS_ERROR = 502,        ///< 파일 접근 에러
    CONFIGURATION_ERROR = 503,      ///< 설정 에러
    THREAD_ERROR = 504,             ///< 스레드 에러
    MUTEX_ERROR = 505               ///< 뮤텍스 에러
};

// =============================================================================
// 공통 타입 정의 (성능과 메모리 효율성 고려)
// =============================================================================

/**
 * @brief 고해상도 타임스탬프 타입
 * @details 마이크로초 단위까지 지원하는 시간 표현
 */
using Timestamp = std::chrono::system_clock::time_point;

/**
 * @brief UUID 타입 (128비트 고유 식별자)
 * @details 문자열 기반이지만, 나중에 바이너리 최적화 가능
 */
using UUID = std::string;

/**
 * @brief 다형적 데이터 값 타입
 * @details std::variant를 사용하여 타입 안전하고 메모리 효율적인 값 저장
 */
using DataValue = std::variant<
    std::monostate,          ///< 빈 값 (NULL)
    bool,                    ///< 불린 값
    int8_t,                  ///< 8비트 정수
    uint8_t,                 ///< 8비트 부호없는 정수
    int16_t,                 ///< 16비트 정수
    uint16_t,                ///< 16비트 부호없는 정수
    int32_t,                 ///< 32비트 정수
    uint32_t,                ///< 32비트 부호없는 정수
    int64_t,                 ///< 64비트 정수
    uint64_t,                ///< 64비트 부호없는 정수
    float,                   ///< 32비트 부동소수점
    double,                  ///< 64비트 부동소수점
    std::string,             ///< 문자열
    std::vector<uint8_t>     ///< 바이너리 데이터
>;

// =============================================================================
// 핵심 데이터 구조체들 (RAII 패턴 적용)
// =============================================================================

/**
 * @brief 에러 정보 구조체
 * @details 에러 발생 시 상세 정보를 담는 구조체
 */
struct ErrorInfo {
    ErrorCode code;                           ///< 에러 코드
    std::string message;                      ///< 에러 메시지
    std::string context;                      ///< 에러 발생 컨텍스트
    Timestamp occurred_at;                    ///< 에러 발생 시간
    std::string file;                         ///< 에러 발생 파일 (디버그용)
    int line;                                 ///< 에러 발생 라인 (디버그용)
    
    /**
     * @brief 기본 생성자
     */
    ErrorInfo() 
        : code(ErrorCode::SUCCESS)
        , occurred_at(std::chrono::system_clock::now())
        , line(0) {}
    
    /**
     * @brief 매개변수 생성자
     * @param error_code 에러 코드
     * @param error_message 에러 메시지
     * @param error_context 에러 컨텍스트 (선택사항)
     */
    ErrorInfo(ErrorCode error_code, const std::string& error_message, 
              const std::string& error_context = "")
        : code(error_code)
        , message(error_message)
        , context(error_context)
        , occurred_at(std::chrono::system_clock::now())
        , line(0) {}
    
    /**
     * @brief 성공 여부 확인
     * @return 성공하면 true, 실패하면 false
     */
    bool IsSuccess() const noexcept {
        return code == ErrorCode::SUCCESS;
    }
    
    /**
     * @brief 에러 정보를 JSON 문자열로 변환
     * @return JSON 형태의 에러 정보
     */
    std::string ToJson() const;
};

/**
 * @brief 타임스탬프가 포함된 데이터 값
 * @details 실시간 데이터 수집에서 사용하는 핵심 데이터 구조
 */
struct TimestampedValue {
    DataValue value;                          ///< 실제 데이터 값
    DataQuality quality;                      ///< 데이터 품질
    Timestamp timestamp;                      ///< 데이터 수집 시간
    UUID source_id;                           ///< 데이터 소스 ID (선택사항)
    
    /**
     * @brief 기본 생성자
     */
    TimestampedValue()
        : quality(DataQuality::GOOD)
        , timestamp(std::chrono::system_clock::now()) {}
    
    /**
     * @brief 매개변수 생성자
     * @param val 데이터 값
     * @param qual 데이터 품질 (기본값: GOOD)
     */
    TimestampedValue(const DataValue& val, DataQuality qual = DataQuality::GOOD)
        : value(val)
        , quality(qual)
        , timestamp(std::chrono::system_clock::now()) {}
    
    /**
     * @brief 유효한 데이터인지 확인
     * @return 유효하면 true, 아니면 false
     */
    bool IsValid() const noexcept {
        return quality == DataQuality::GOOD || quality == DataQuality::UNCERTAIN;
    }
};

/**
 * @brief 디바이스 정보 구조체
 * @details 디바이스의 기본 정보와 통신 설정을 담는 구조체
 */
struct DeviceInfo {
    UUID id;                                  ///< 디바이스 고유 ID
    std::string name;                         ///< 디바이스 이름
    std::string description;                  ///< 디바이스 설명
    ProtocolType protocol;                    ///< 사용할 프로토콜
    std::string endpoint;                     ///< 연결 엔드포인트 (IP:Port, 시리얼 포트 등)
    std::string config_json;                  ///< 프로토콜별 설정 (JSON 형태)
    
    // 통신 설정
    int polling_interval_ms;                  ///< 폴링 간격 (밀리초)
    int timeout_ms;                           ///< 타임아웃 (밀리초)
    int retry_count;                          ///< 재시도 횟수
    bool auto_reconnect;                      ///< 자동 재연결 여부
    int reconnect_delay_ms;                   ///< 재연결 지연 시간 (밀리초)
    
    // 상태 정보
    bool is_enabled;                          ///< 활성화 여부
    ConnectionStatus status;                  ///< 현재 연결 상태
    Timestamp last_communication;             ///< 마지막 통신 시간
    
    /**
     * @brief 기본 생성자
     */
    DeviceInfo()
        : protocol(ProtocolType::UNKNOWN)
        , polling_interval_ms(Constants::DEFAULT_POLLING_INTERVAL_MS)
        , timeout_ms(Constants::DEFAULT_TIMEOUT_MS)
        , retry_count(Constants::DEFAULT_RETRY_COUNT)
        , auto_reconnect(true)
        , reconnect_delay_ms(5000)
        , is_enabled(true)
        , status(ConnectionStatus::DISCONNECTED)
        , last_communication(std::chrono::system_clock::now()) {}
};

/**
 * @brief 데이터 포인트 구조체
 * @details 디바이스 내부의 개별 데이터 포인트 정보
 */
struct DataPoint {
    UUID id;                                  ///< 데이터 포인트 고유 ID
    UUID device_id;                           ///< 소속 디바이스 ID
    std::string name;                         ///< 데이터 포인트 이름
    std::string description;                  ///< 데이터 포인트 설명
    
    // 주소 정보
    int address;                              ///< 프로토콜별 주소 (레지스터, 태그 등)
    std::string address_string;               ///< 문자열 형태 주소 (선택사항)
    
    // 데이터 타입 및 변환
    DataType data_type;                       ///< 데이터 타입
    std::string unit;                         ///< 단위 (예: "°C", "bar", "rpm")
    double scaling_factor;                    ///< 스케일링 팩터
    double scaling_offset;                    ///< 스케일링 오프셋
    double min_value;                         ///< 최소값 (유효성 검사용)
    double max_value;                         ///< 최대값 (유효성 검사용)
    
    // 설정
    bool is_enabled;                          ///< 활성화 여부
    bool is_writable;                         ///< 쓰기 가능 여부
    int scan_rate_ms;                         ///< 개별 스캔 속도 (0이면 디바이스 기본값 사용)
    double deadband;                          ///< 데드밴드 (변화량 임계값)
    
    // 메타데이터
    std::map<std::string, std::string> tags;  ///< 사용자 정의 태그들
    std::string config_json;                  ///< 추가 설정 (JSON 형태)
    
    /**
     * @brief 기본 생성자
     */
    DataPoint()
        : address(0)
        , data_type(DataType::UNKNOWN)
        , scaling_factor(1.0)
        , scaling_offset(0.0)
        , min_value(std::numeric_limits<double>::lowest())
        , max_value(std::numeric_limits<double>::max())
        , is_enabled(true)
        , is_writable(false)
        , scan_rate_ms(0)
        , deadband(0.0) {}
    
    /**
     * @brief 비교 연산자 (STL 컨테이너 사용을 위해)
     * @param other 비교할 데이터 포인트
     * @return ID 기준 비교 결과
     */
    bool operator<(const DataPoint& other) const {
        return id < other.id;
    }
    
    /**
     * @brief 등등 연산자
     * @param other 비교할 데이터 포인트
     * @return ID가 같으면 true
     */
    bool operator==(const DataPoint& other) const {
        return id == other.id;
    }
};

// =============================================================================
// 유틸리티 함수들 (인라인으로 성능 최적화)
// =============================================================================

/**
 * @brief 프로토콜 타입을 문자열로 변환
 * @param type 프로토콜 타입
 * @return 문자열 표현
 */
inline std::string ProtocolTypeToString(ProtocolType type) noexcept {
    switch (type) {
        case ProtocolType::MODBUS_TCP:    return "modbus_tcp";
        case ProtocolType::MODBUS_RTU:    return "modbus_rtu";
        case ProtocolType::MQTT:          return "mqtt";
        case ProtocolType::BACNET_IP:     return "bacnet_ip";
        case ProtocolType::OPC_UA:        return "opc_ua";
        case ProtocolType::ETHERNET_IP:   return "ethernet_ip";
        case ProtocolType::PROFINET:      return "profinet";
        case ProtocolType::CUSTOM:        return "custom";
        default:                          return "unknown";
    }
}

/**
 * @brief 문자열을 프로토콜 타입으로 변환
 * @param str 문자열
 * @return 프로토콜 타입
 */
inline ProtocolType StringToProtocolType(const std::string& str) noexcept {
    if (str == "modbus_tcp")    return ProtocolType::MODBUS_TCP;
    if (str == "modbus_rtu")    return ProtocolType::MODBUS_RTU;
    if (str == "mqtt")          return ProtocolType::MQTT;
    if (str == "bacnet_ip")     return ProtocolType::BACNET_IP;
    if (str == "opc_ua")        return ProtocolType::OPC_UA;
    if (str == "ethernet_ip")   return ProtocolType::ETHERNET_IP;
    if (str == "profinet")      return ProtocolType::PROFINET;
    if (str == "custom")        return ProtocolType::CUSTOM;
    return ProtocolType::UNKNOWN;
}

/**
 * @brief 데이터 타입을 문자열로 변환
 * @param type 데이터 타입
 * @return 문자열 표현
 */
inline std::string DataTypeToString(DataType type) noexcept {
    switch (type) {
        case DataType::BOOL:        return "bool";
        case DataType::INT8:        return "int8";
        case DataType::UINT8:       return "uint8";
        case DataType::INT16:       return "int16";
        case DataType::UINT16:      return "uint16";
        case DataType::INT32:       return "int32";
        case DataType::UINT32:      return "uint32";
        case DataType::INT64:       return "int64";
        case DataType::UINT64:      return "uint64";
        case DataType::FLOAT32:     return "float32";
        case DataType::FLOAT64:     return "float64";
        case DataType::STRING:      return "string";
        case DataType::BINARY:      return "binary";
        case DataType::TIMESTAMP:   return "timestamp";
        default:                    return "unknown";
    }
}

/**
 * @brief 연결 상태를 문자열로 변환
 * @param status 연결 상태
 * @return 문자열 표현
 */
inline std::string ConnectionStatusToString(ConnectionStatus status) noexcept {
    switch (status) {
        case ConnectionStatus::DISCONNECTED:   return "disconnected";
        case ConnectionStatus::CONNECTING:     return "connecting";
        case ConnectionStatus::CONNECTED:      return "connected";
        case ConnectionStatus::RECONNECTING:   return "reconnecting";
        case ConnectionStatus::ERROR:          return "error";
        case ConnectionStatus::MAINTENANCE:    return "maintenance";
        case ConnectionStatus::TIMEOUT:        return "timeout";
        case ConnectionStatus::UNAUTHORIZED:   return "unauthorized";
        default:                               return "unknown";
    }
}

/**
 * @brief 타임스탬프를 ISO 8601 문자열로 변환
 * @param ts 타임스탬프
 * @return ISO 8601 형식 문자열 (예: "2025-01-17T10:30:45.123Z")
 */
inline std::string TimestampToISOString(const Timestamp& ts) {
    auto time_t = std::chrono::system_clock::to_time_t(ts);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        ts.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

/**
 * @brief 간단한 UUID 생성 함수
 * @details 실제 운영에서는 더 강력한 UUID 라이브러리 사용 권장
 * @return UUID 문자열
 */
inline UUID GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

} // namespace Drivers
} // namespace PulseOne

#endif // DRIVERS_COMMON_TYPES_H