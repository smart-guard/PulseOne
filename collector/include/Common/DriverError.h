/**
 * @file DriverError.h - 올바른 순서로 재구성
 * @brief enum 정의를 먼저, 구조체는 나중에
 */

#ifndef PULSEONE_DRIVER_ERROR_H
#define PULSEONE_DRIVER_ERROR_H

/**
 * @file DriverError.h
 * @brief PulseOne 드라이버 하이브리드 에러 시스템
 * @author PulseOne Development Team
 * @date 2025-08-02
 * @version 1.0.0
 */

#include "Common/BasicTypes.h"
#include <string>
#include <chrono>
#include <map>
#include <sstream>

#ifdef HAS_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Structs {

// =============================================================================
// 🔥 1. 먼저 enum들을 정의 (ErrorInfo에서 사용하기 전에!)
// =============================================================================

/**
 * @brief 기존 시스템 호환용 ErrorCode
 * @details Common/Enums.h의 ErrorCode와 완전히 동일
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
    INVALID_PARAMETER = 506,
    
    // 드라이버 시스템용 추가 에러들
    WARNING_DATA_STALE = 1000,
    WARNING_PARTIAL_SUCCESS = 1001,
    FATAL_INTERNAL_ERROR = 2000,
    FATAL_MEMORY_EXHAUSTED = 2001
};

/**
 * @brief 드라이버 전용 통합 에러 코드
 * @details 모든 프로토콜의 에러를 PulseOne 표준으로 분류
 */
enum class DriverErrorCode : uint32_t {
    // ===== 기본 성공/실패 =====
    SUCCESS = 0,
    
    // ===== 연결 관련 에러 (1-19) =====
    CONNECTION_FAILED = 1,
    CONNECTION_LOST = 2,
    CONNECTION_TIMEOUT = 3,
    CONNECTION_REFUSED = 4,
    NETWORK_UNREACHABLE = 5,
    HOST_NOT_FOUND = 6,
    PORT_UNAVAILABLE = 7,
    SSL_HANDSHAKE_FAILED = 8,
    AUTHENTICATION_FAILED = 9,
    AUTHORIZATION_FAILED = 10,
    
    // ===== 통신 관련 에러 (20-39) =====
    TIMEOUT = 20,
    COMMUNICATION_ERROR = 21,
    PROTOCOL_ERROR = 22,
    DATA_FORMAT_ERROR = 23,
    CHECKSUM_ERROR = 24,
    FRAME_ERROR = 25,
    BUFFER_OVERFLOW = 26,
    BUFFER_UNDERFLOW = 27,
    SEQUENCE_ERROR = 28,
    
    // ===== 요청/응답 관련 에러 (40-59) =====
    REQUEST_REJECTED = 40,
    REQUEST_TIMEOUT = 41,
    INVALID_REQUEST = 42,
    UNSUPPORTED_FUNCTION = 43,
    OPERATION_ABORTED = 44,
    OPERATION_CANCELLED = 45,
    RESPONSE_TOO_LARGE = 46,
    RESPONSE_MALFORMED = 47,
    
    // ===== 데이터 관련 에러 (60-79) =====
    INVALID_PARAMETER = 60,
    PARAMETER_OUT_OF_RANGE = 61,
    VALUE_OUT_OF_RANGE = 62,
    DATA_TYPE_MISMATCH = 63,
    DATA_CONVERSION_ERROR = 64,
    DATA_VALIDATION_ERROR = 65,
    ENCODING_ERROR = 66,
    DECODING_ERROR = 67,
    
    // ===== 리소스 관련 에러 (80-99) =====
    RESOURCE_UNAVAILABLE = 80,
    RESOURCE_BUSY = 81,
    MEMORY_ALLOCATION_FAILED = 82,
    FILE_NOT_FOUND = 83,
    PERMISSION_DENIED = 84,
    DISK_FULL = 85,
    HANDLE_INVALID = 86,
    
    // ===== 디바이스/객체 관련 에러 (100-119) =====
    DEVICE_NOT_FOUND = 100,
    DEVICE_NOT_RESPONDING = 101,
    DEVICE_BUSY = 102,
    DEVICE_ERROR = 103,
    OBJECT_NOT_FOUND = 104,
    PROPERTY_NOT_FOUND = 105,
    SERVICE_NOT_SUPPORTED = 106,
    ACCESS_DENIED = 107,
    
    // ===== 설정/상태 관련 에러 (120-139) =====
    CONFIGURATION_ERROR = 120,
    INVALID_STATE = 121,
    STATE_TRANSITION_ERROR = 122,
    VERSION_MISMATCH = 123,
    FEATURE_NOT_SUPPORTED = 124,
    FEATURE_DISABLED = 125,
    
    // ===== 시스템 관련 에러 (140-159) =====
    SYSTEM_ERROR = 140,
    HARDWARE_ERROR = 141,
    SOFTWARE_ERROR = 142,
    DRIVER_ERROR = 143,
    LIBRARY_ERROR = 144,
    
    // ===== 보안 관련 에러 (160-179) =====
    SECURITY_ERROR = 160,
    CERTIFICATE_ERROR = 161,
    CERTIFICATE_EXPIRED = 162,
    CERTIFICATE_INVALID = 163,
    ENCRYPTION_ERROR = 164,
    DECRYPTION_ERROR = 165,
    
    // ===== 기타 =====
    UNKNOWN_ERROR = 999
};

// =============================================================================
// 🔥 2. 이제 ErrorInfo 구조체 정의 (enum들이 이미 정의된 후)
// =============================================================================

/**
 * @brief 하이브리드 에러 정보 구조체
 * @details 기존 ErrorCode + 새로운 DriverErrorCode 모두 지원
 */
struct ErrorInfo {
    // 🔥 멤버 변수 (선언 순서와 초기화 순서 일치)
    ErrorCode code = ErrorCode::SUCCESS;
    std::string message = "";
    std::string details = "";                    // DriverLogger 호환성
    BasicTypes::Timestamp occurred_at = std::chrono::system_clock::now();
    
    // 새로운 드라이버 시스템 확장
    DriverErrorCode driver_code = DriverErrorCode::SUCCESS;
    std::string protocol = "";
    int native_error_code = 0;
    std::string native_error_name = "";
    std::string context = "";
    std::map<std::string, std::string> additional_info;
    
    // ===== 생성자들 =====
    ErrorInfo() = default;
    
    // 기존 방식 (완전 호환)
    ErrorInfo(ErrorCode error_code, const std::string& error_message)
        : code(error_code), message(error_message), details(error_message) {
        driver_code = LegacyToDriverCode(error_code);
    }
    
    // details 포함 생성자 (DriverLogger 호환)
    ErrorInfo(ErrorCode error_code, const std::string& error_message, const std::string& error_details)
        : code(error_code), message(error_message), details(error_details) {
        driver_code = LegacyToDriverCode(error_code);
    }
    
    // 새로운 방식 (멤버 초기화 순서 수정)
    ErrorInfo(DriverErrorCode driver_error_code, const std::string& error_message, 
              const std::string& protocol_name = "", int native_code = 0)
        : code(DriverToLegacyCode(driver_error_code))
        , message(error_message)
        , details(error_message)
        , driver_code(driver_error_code)
        , protocol(protocol_name)
        , native_error_code(native_code) {
    }
    
    // ===== 유틸리티 메서드들 =====
    bool IsSuccess() const { return code == ErrorCode::SUCCESS; }
    bool IsFailure() const { return code != ErrorCode::SUCCESS; }
    
    /**
     * @brief 기존 ErrorCode → DriverErrorCode 변환
     */
    static DriverErrorCode LegacyToDriverCode(ErrorCode legacy_code) {
        switch (legacy_code) {
            case ErrorCode::SUCCESS: return DriverErrorCode::SUCCESS;
            case ErrorCode::CONNECTION_FAILED: return DriverErrorCode::CONNECTION_FAILED;
            case ErrorCode::CONNECTION_TIMEOUT: return DriverErrorCode::CONNECTION_TIMEOUT;
            case ErrorCode::CONNECTION_LOST: return DriverErrorCode::CONNECTION_LOST;
            case ErrorCode::PROTOCOL_ERROR: return DriverErrorCode::PROTOCOL_ERROR;
            case ErrorCode::DATA_FORMAT_ERROR: return DriverErrorCode::DATA_FORMAT_ERROR;
            case ErrorCode::MAINTENANCE_ACTIVE: return DriverErrorCode::DEVICE_BUSY;
            case ErrorCode::REMOTE_CONTROL_BLOCKED: return DriverErrorCode::ACCESS_DENIED;
            case ErrorCode::INSUFFICIENT_PERMISSION: return DriverErrorCode::PERMISSION_DENIED;
            case ErrorCode::RESOURCE_EXHAUSTED: return DriverErrorCode::RESOURCE_UNAVAILABLE;
            case ErrorCode::INTERNAL_ERROR: return DriverErrorCode::SYSTEM_ERROR;
            case ErrorCode::INVALID_PARAMETER: return DriverErrorCode::INVALID_PARAMETER;
            case ErrorCode::DEVICE_NOT_FOUND: return DriverErrorCode::DEVICE_NOT_FOUND;
            case ErrorCode::DEVICE_ERROR: return DriverErrorCode::DEVICE_ERROR;
            case ErrorCode::CHECKSUM_ERROR: return DriverErrorCode::CHECKSUM_ERROR;
            case ErrorCode::UNSUPPORTED_FUNCTION: return DriverErrorCode::UNSUPPORTED_FUNCTION;
            case ErrorCode::AUTHENTICATION_FAILED: return DriverErrorCode::AUTHENTICATION_FAILED;
            case ErrorCode::CONFIGURATION_ERROR: return DriverErrorCode::CONFIGURATION_ERROR;
            default: return DriverErrorCode::UNKNOWN_ERROR;
        }
    }
    
    /**
     * @brief DriverErrorCode → 기존 ErrorCode 변환
     */
    static ErrorCode DriverToLegacyCode(DriverErrorCode driver_code) {
        switch (driver_code) {
            case DriverErrorCode::SUCCESS: return ErrorCode::SUCCESS;
            case DriverErrorCode::CONNECTION_FAILED: return ErrorCode::CONNECTION_FAILED;
            case DriverErrorCode::CONNECTION_TIMEOUT: return ErrorCode::CONNECTION_TIMEOUT;
            case DriverErrorCode::CONNECTION_LOST: return ErrorCode::CONNECTION_LOST;
            case DriverErrorCode::PROTOCOL_ERROR: return ErrorCode::PROTOCOL_ERROR;
            case DriverErrorCode::DATA_FORMAT_ERROR: return ErrorCode::DATA_FORMAT_ERROR;
            case DriverErrorCode::DEVICE_BUSY: return ErrorCode::MAINTENANCE_ACTIVE;
            case DriverErrorCode::ACCESS_DENIED: return ErrorCode::REMOTE_CONTROL_BLOCKED;
            case DriverErrorCode::PERMISSION_DENIED: return ErrorCode::INSUFFICIENT_PERMISSION;
            case DriverErrorCode::RESOURCE_UNAVAILABLE: return ErrorCode::RESOURCE_EXHAUSTED;
            case DriverErrorCode::SYSTEM_ERROR: return ErrorCode::INTERNAL_ERROR;
            case DriverErrorCode::INVALID_PARAMETER: return ErrorCode::INVALID_PARAMETER;
            case DriverErrorCode::DEVICE_NOT_FOUND: return ErrorCode::DEVICE_NOT_FOUND;
            case DriverErrorCode::DEVICE_ERROR: return ErrorCode::DEVICE_ERROR;
            case DriverErrorCode::CHECKSUM_ERROR: return ErrorCode::CHECKSUM_ERROR;
            case DriverErrorCode::UNSUPPORTED_FUNCTION: return ErrorCode::UNSUPPORTED_FUNCTION;
            case DriverErrorCode::AUTHENTICATION_FAILED: return ErrorCode::AUTHENTICATION_FAILED;
            case DriverErrorCode::CONFIGURATION_ERROR: return ErrorCode::CONFIGURATION_ERROR;
            default: return ErrorCode::INTERNAL_ERROR;
        }
    }
    
    /**
     * @brief 간단한 에러 요약
     */
    std::string GetSummary() const {
        if (IsSuccess()) return "Success";
        return "[" + std::to_string(static_cast<int>(code)) + "] " + message;
    }
    
    /**
     * @brief 상세 에러 정보 텍스트
     */
    std::string GetDetailedInfo() const {
        std::ostringstream oss;
        
        oss << "=== Error Details ===\n";
        oss << "Legacy Error Code: " << static_cast<int>(code) << "\n";
        oss << "Driver Error Code: " << static_cast<int>(driver_code) << "\n";
        oss << "Error Message: " << message << "\n";
        
        if (!details.empty() && details != message) {
            oss << "Error Details: " << details << "\n";
        }
        
        if (!protocol.empty()) {
            oss << "Protocol: " << protocol << "\n";
        }
        
        if (native_error_code != 0) {
            oss << "Native Error Code: " << native_error_code << "\n";
            if (!native_error_name.empty()) {
                oss << "Native Error Name: " << native_error_name << "\n";
            }
        }
        
        if (!context.empty()) {
            oss << "Context: " << context << "\n";
        }
        
        auto time_t = std::chrono::system_clock::to_time_t(occurred_at);
        oss << "Occurred At: " << std::ctime(&time_t);
        
        if (!additional_info.empty()) {
            oss << "Additional Info:\n";
            for (const auto& [key, value] : additional_info) {
                oss << "  " << key << ": " << value << "\n";
            }
        }
        
        return oss.str();
    }
    
    /**
     * @brief JSON 형태로 에러 정보 출력
     */
    std::string ToJsonString() const {
#ifdef HAS_NLOHMANN_JSON
        json error_json;
        
        // 기존 시스템 정보
        error_json["legacy_error_code"] = static_cast<int>(code);
        error_json["legacy_error_name"] = LegacyErrorCodeToString(code);
        
        // 새로운 시스템 정보
        error_json["driver_error_code"] = static_cast<int>(driver_code);
        error_json["driver_error_name"] = DriverErrorCodeToString(driver_code);
        
        error_json["message"] = message;
        error_json["is_success"] = IsSuccess();
        
        if (!details.empty()) {
            error_json["details"] = details;
        }
        
        if (!protocol.empty()) {
            error_json["protocol"] = protocol;
        }
        
        if (native_error_code != 0) {
            error_json["native_error_code"] = native_error_code;
            if (!native_error_name.empty()) {
                error_json["native_error_name"] = native_error_name;
            }
        }
        
        if (!context.empty()) {
            error_json["context"] = context;
        }
        
        error_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            occurred_at.time_since_epoch()).count();
        
        if (!additional_info.empty()) {
            error_json["additional_info"] = additional_info;
        }
        
        return error_json.dump(2);
#else
        return "{\"message\":\"JSON library not available\"}";
#endif
    }
    
private:
    std::string LegacyErrorCodeToString(ErrorCode code) const {
        switch (code) {
            case ErrorCode::SUCCESS: return "SUCCESS";
            case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
            case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
            case ErrorCode::CONNECTION_LOST: return "CONNECTION_LOST";
            case ErrorCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
            case ErrorCode::DATA_FORMAT_ERROR: return "DATA_FORMAT_ERROR";
            case ErrorCode::MAINTENANCE_ACTIVE: return "MAINTENANCE_ACTIVE";
            case ErrorCode::REMOTE_CONTROL_BLOCKED: return "REMOTE_CONTROL_BLOCKED";
            case ErrorCode::INSUFFICIENT_PERMISSION: return "INSUFFICIENT_PERMISSION";
            case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
            case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
            case ErrorCode::INVALID_PARAMETER: return "INVALID_PARAMETER";
            case ErrorCode::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
            case ErrorCode::DEVICE_ERROR: return "DEVICE_ERROR";
            default: return "LEGACY_ERROR_" + std::to_string(static_cast<int>(code));
        }
    }
    
    std::string DriverErrorCodeToString(DriverErrorCode code) const {
        switch (code) {
            case DriverErrorCode::SUCCESS: return "SUCCESS";
            case DriverErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
            case DriverErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
            case DriverErrorCode::CONNECTION_LOST: return "CONNECTION_LOST";
            case DriverErrorCode::TIMEOUT: return "TIMEOUT";
            case DriverErrorCode::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
            case DriverErrorCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
            case DriverErrorCode::DATA_FORMAT_ERROR: return "DATA_FORMAT_ERROR";
            case DriverErrorCode::CHECKSUM_ERROR: return "CHECKSUM_ERROR";
            case DriverErrorCode::DEVICE_BUSY: return "DEVICE_BUSY";
            case DriverErrorCode::ACCESS_DENIED: return "ACCESS_DENIED";
            case DriverErrorCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
            case DriverErrorCode::RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
            case DriverErrorCode::SYSTEM_ERROR: return "SYSTEM_ERROR";
            case DriverErrorCode::INVALID_PARAMETER: return "INVALID_PARAMETER";
            case DriverErrorCode::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
            case DriverErrorCode::DEVICE_ERROR: return "DEVICE_ERROR";
            default: return "DRIVER_ERROR_" + std::to_string(static_cast<int>(code));
        }
    }
};

// =============================================================================
// 🔥 3. 프로토콜별 에러 변환 헬퍼들
// =============================================================================

/**
 * @brief Modbus 에러 변환 헬퍼
 */
class ModbusErrorConverter {
public:
    static ErrorInfo ConvertModbusError(int modbus_error, const std::string& context = "") {
        DriverErrorCode driver_code;
        std::string error_message;
        std::string native_name;
        
        switch (modbus_error) {
            case 0:
                driver_code = DriverErrorCode::SUCCESS;
                error_message = "Modbus operation successful";
                native_name = "MODBUS_SUCCESS";
                break;
            case 1:
                driver_code = DriverErrorCode::UNSUPPORTED_FUNCTION;
                error_message = "Modbus illegal function code";
                native_name = "ILLEGAL_FUNCTION";
                break;
            case 2:
                driver_code = DriverErrorCode::INVALID_PARAMETER;
                error_message = "Modbus illegal data address";
                native_name = "ILLEGAL_DATA_ADDRESS";
                break;
            case 5:
                driver_code = DriverErrorCode::DEVICE_BUSY;
                error_message = "Modbus slave acknowledge (busy)";
                native_name = "ACKNOWLEDGE";
                break;
            case -1:
                driver_code = DriverErrorCode::CONNECTION_FAILED;
                error_message = "Modbus connection failed";
                native_name = "CONNECTION_ERROR";
                break;
            case -2:
                driver_code = DriverErrorCode::TIMEOUT;
                error_message = "Modbus operation timeout";
                native_name = "TIMEOUT";
                break;
            case -3:
                driver_code = DriverErrorCode::CHECKSUM_ERROR;
                error_message = "Modbus CRC error";
                native_name = "CRC_ERROR";
                break;
            default:
                driver_code = DriverErrorCode::UNKNOWN_ERROR;
                error_message = "Unknown Modbus error: " + std::to_string(modbus_error);
                native_name = "UNKNOWN";
                break;
        }
        
        ErrorInfo error(driver_code, error_message, "MODBUS", modbus_error);
        error.native_error_name = native_name;
        error.context = context;
        return error;
    }
};

/**
 * @brief MQTT 에러 변환 헬퍼
 */
class MqttErrorConverter {
public:
    static ErrorInfo ConvertMqttError(int mqtt_error, const std::string& context = "") {
        // 구현 생략 (필요시 추가)
        ErrorInfo error(DriverErrorCode::UNKNOWN_ERROR, "MQTT error: " + std::to_string(mqtt_error), "MQTT", mqtt_error);
        error.context = context;
        return error;
    }
};

/**
 * @brief BACnet 에러 변환 헬퍼
 */
class BACnetErrorConverter {
public:
    static ErrorInfo ConvertBACnetError(int bacnet_error, const std::string& context = "") {
        // 구현 생략 (필요시 추가)
        ErrorInfo error(DriverErrorCode::UNKNOWN_ERROR, "BACnet error: " + std::to_string(bacnet_error), "BACNET", bacnet_error);
        error.context = context;
        return error;
    }
};

} // namespace Structs
} // namespace PulseOne

#endif // PULSEONE_DRIVER_ERROR_H