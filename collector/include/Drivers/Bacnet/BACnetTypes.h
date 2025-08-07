// =============================================================================
// collector/include/Drivers/Bacnet/BACnetTypes.h
// BACnet 타입 정의 및 상수 - 완전 복원
// =============================================================================

#ifndef BACNET_TYPES_H
#define BACNET_TYPES_H

#include <cstdint>

// =============================================================================
// BACnet 기본 타입 정의
// =============================================================================

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
}

// 실제 BACnet 스택이 있을 때는 원본 타입 사용
using BACnetObjectType = BACNET_OBJECT_TYPE;
using BACnetPropertyId = BACNET_PROPERTY_ID;
using BACnetErrorCode = BACNET_ERROR_CODE;
using BACnetObjectId = BACNET_OBJECT_ID;

#else

// =============================================================================
// BACnet 스택이 없을 때의 타입 정의 (완전한 열거형으로 정의)
// =============================================================================

/**
 * @brief BACnet 객체 타입 열거형
 */
enum BACnetObjectType : uint16_t {
    // 표준 객체 타입들
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_ANALOG_OUTPUT = 1,
    OBJECT_ANALOG_VALUE = 2,
    OBJECT_BINARY_INPUT = 3,
    OBJECT_BINARY_OUTPUT = 4,
    OBJECT_BINARY_VALUE = 5,
    OBJECT_CALENDAR = 6,
    OBJECT_COMMAND = 7,
    OBJECT_DEVICE = 8,
    OBJECT_EVENT_ENROLLMENT = 9,
    OBJECT_FILE = 10,
    OBJECT_GROUP = 11,
    OBJECT_LOOP = 12,
    OBJECT_MULTI_STATE_INPUT = 13,
    OBJECT_MULTI_STATE_OUTPUT = 14,
    OBJECT_NOTIFICATION_CLASS = 15,
    OBJECT_PROGRAM = 16,
    OBJECT_SCHEDULE = 17,
    OBJECT_AVERAGING = 18,
    OBJECT_MULTI_STATE_VALUE = 19,
    OBJECT_TRENDLOG = 20,
    OBJECT_LIFE_SAFETY_POINT = 21,
    OBJECT_LIFE_SAFETY_ZONE = 22,
    OBJECT_ACCUMULATOR = 23,
    OBJECT_PULSE_CONVERTER = 24,
    OBJECT_EVENT_LOG = 25,
    OBJECT_GLOBAL_GROUP = 26,
    OBJECT_TREND_LOG_MULTIPLE = 27,
    OBJECT_LOAD_CONTROL = 28,
    OBJECT_STRUCTURED_VIEW = 29,
    OBJECT_ACCESS_DOOR = 30,
    // 더 많은 타입들...
    MAX_BACNET_OBJECT_TYPE = 1023
};

/**
 * @brief BACnet 프로퍼티 ID 열거형
 */
enum BACnetPropertyId : uint32_t {
    PROP_ACKED_TRANSITIONS = 0,
    PROP_ACK_REQUIRED = 1,
    PROP_ACTION = 2,
    PROP_ACTION_TEXT = 3,
    PROP_ACTIVE_TEXT = 4,
    PROP_ACTIVE_VT_SESSIONS = 5,
    PROP_ALARM_VALUE = 6,
    PROP_ALARM_VALUES = 7,
    PROP_ALL = 8,
    PROP_ALL_WRITES_SUCCESSFUL = 9,
    PROP_APDU_SEGMENT_TIMEOUT = 10,
    PROP_APDU_TIMEOUT = 11,
    PROP_APPLICATION_SOFTWARE_VERSION = 12,
    PROP_ARCHIVE = 13,
    PROP_BIAS = 14,
    PROP_CHANGE_OF_STATE_COUNT = 15,
    PROP_CHANGE_OF_STATE_TIME = 16,
    PROP_NOTIFICATION_CLASS = 17,
    PROP_CONTROLLED_VARIABLE_REFERENCE = 19,
    PROP_CONTROLLED_VARIABLE_UNITS = 20,
    PROP_CONTROLLED_VARIABLE_VALUE = 21,
    PROP_COV_INCREMENT = 22,
    PROP_DATE_LIST = 23,
    PROP_DAYLIGHT_SAVINGS_STATUS = 24,
    PROP_DEADBAND = 25,
    PROP_DERIVATIVE_CONSTANT = 26,
    PROP_DERIVATIVE_CONSTANT_UNITS = 27,
    PROP_DESCRIPTION = 28,
    PROP_DESCRIPTION_OF_HALT = 29,
    PROP_DEVICE_ADDRESS_BINDING = 30,
    PROP_DEVICE_TYPE = 31,
    PROP_EFFECTIVE_PERIOD = 32,
    PROP_ELAPSED_ACTIVE_TIME = 33,
    PROP_ERROR_LIMIT = 34,
    PROP_EVENT_ENABLE = 35,
    PROP_EVENT_STATE = 36,
    PROP_EVENT_TYPE = 37,
    PROP_EXCEPTION_SCHEDULE = 38,
    PROP_FAULT_VALUES = 39,
    PROP_FEEDBACK_VALUE = 40,
    PROP_FILE_ACCESS_METHOD = 41,
    PROP_FILE_SIZE = 42,
    PROP_FILE_TYPE = 43,
    PROP_FIRMWARE_REVISION = 44,
    PROP_HIGH_LIMIT = 45,
    PROP_INACTIVE_TEXT = 46,
    PROP_IN_PROCESS = 47,
    PROP_INSTANCE_OF = 48,
    PROP_INTEGRAL_CONSTANT = 49,
    PROP_INTEGRAL_CONSTANT_UNITS = 50,
    PROP_ISSUE_CONFIRMED_NOTIFICATIONS = 51,
    PROP_LIMIT_ENABLE = 52,
    PROP_LIST_OF_GROUP_MEMBERS = 53,
    PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES = 54,
    PROP_LIST_OF_SESSION_KEYS = 55,
    PROP_LOCAL_DATE = 56,
    PROP_LOCAL_TIME = 57,
    PROP_LOCATION = 58,
    PROP_LOW_LIMIT = 59,
    PROP_MANIPULATED_VARIABLE_REFERENCE = 60,
    PROP_MAXIMUM_OUTPUT = 61,
    PROP_MAX_APDU_LENGTH_ACCEPTED = 62,
    PROP_MAX_INFO_FRAMES = 63,
    PROP_MAX_MASTER = 64,
    PROP_MAX_PRES_VALUE = 65,
    PROP_MINIMUM_OFF_TIME = 66,
    PROP_MINIMUM_ON_TIME = 67,
    PROP_MINIMUM_OUTPUT = 68,
    PROP_MIN_PRES_VALUE = 69,
    PROP_MODEL_NAME = 70,
    PROP_MODIFICATION_DATE = 71,
    PROP_NOTIFY_TYPE = 72,
    PROP_NUMBER_OF_APDU_RETRIES = 73,
    PROP_NUMBER_OF_STATES = 74,
    PROP_OBJECT_IDENTIFIER = 75,
    PROP_OBJECT_LIST = 76,
    PROP_OBJECT_NAME = 77,
    PROP_OBJECT_PROPERTY_REFERENCE = 78,
    PROP_OBJECT_TYPE = 79,
    PROP_OPTIONAL = 80,
    PROP_OUT_OF_SERVICE = 81,
    PROP_OUTPUT_UNITS = 82,
    PROP_EVENT_PARAMETERS = 83,
    PROP_POLARITY = 84,
    PROP_PRESENT_VALUE = 85,        // 가장 중요한 프로퍼티!
    PROP_PRIORITY = 86,
    PROP_PRIORITY_ARRAY = 87,
    PROP_PRIORITY_FOR_WRITING = 88,
    PROP_PROCESS_IDENTIFIER = 89,
    PROP_PROGRAM_CHANGE = 90,
    PROP_PROGRAM_LOCATION = 91,
    PROP_PROGRAM_STATE = 92,
    PROP_PROPORTIONAL_CONSTANT = 93,
    PROP_PROPORTIONAL_CONSTANT_UNITS = 94,
    PROP_PROTOCOL_CONFORMANCE_CLASS = 95,
    PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED = 96,
    PROP_PROTOCOL_SERVICES_SUPPORTED = 97,
    PROP_PROTOCOL_VERSION = 98,
    PROP_READ_ONLY = 99,
    PROP_REASON_FOR_HALT = 100,
    PROP_RECIPIENT = 101,
    PROP_RECIPIENT_LIST = 102,
    PROP_RELIABILITY = 103,
    PROP_RELINQUISH_DEFAULT = 104,
    PROP_REQUIRED = 105,
    PROP_RESOLUTION = 106,
    PROP_SEGMENTATION_SUPPORTED = 107,
    PROP_SETPOINT = 108,
    PROP_SETPOINT_REFERENCE = 109,
    PROP_STATE_TEXT = 110,
    PROP_STATUS_FLAGS = 111,
    PROP_SYSTEM_STATUS = 112,
    PROP_TIME_DELAY = 113,
    PROP_TIME_OF_ACTIVE_TIME_RESET = 114,
    PROP_TIME_OF_STATE_COUNT_RESET = 115,
    PROP_TIME_SYNCHRONIZATION_RECIPIENTS = 116,
    PROP_UNITS = 117,
    PROP_UPDATE_INTERVAL = 118,
    PROP_UTC_OFFSET = 119,
    PROP_VENDOR_IDENTIFIER = 120,
    PROP_VENDOR_NAME = 121,
    PROP_VT_CLASSES_SUPPORTED = 122,
    PROP_WEEKLY_SCHEDULE = 123,
    PROP_ATTEMPTED_SAMPLES = 124,
    PROP_AVERAGE_VALUE = 125,
    PROP_BUFFER_SIZE = 126,
    PROP_CLIENT_COV_INCREMENT = 127,
    PROP_COV_RESUBSCRIPTION_INTERVAL = 128,
    PROP_CURRENT_NOTIFY_TIME = 129,
    PROP_EVENT_TIME_STAMPS = 130,
    // 더 많은 프로퍼티들...
    MAX_BACNET_PROPERTY_ID = 4194303
};

/**
 * @brief BACnet 에러 코드
 */
enum BACnetErrorCode : uint8_t {
    ERROR_CODE_OTHER = 0,
    ERROR_CODE_AUTHENTICATION_FAILED = 1,
    ERROR_CODE_CONFIGURATION_IN_PROGRESS = 2,
    ERROR_CODE_DEVICE_BUSY = 3,
    ERROR_CODE_DYNAMIC_CREATION_NOT_SUPPORTED = 4,
    ERROR_CODE_FILE_ACCESS_DENIED = 5,
    ERROR_CODE_INCOMPATIBLE_SECURITY_LEVELS = 6,
    ERROR_CODE_INCONSISTENT_PARAMETERS = 7,
    ERROR_CODE_INCONSISTENT_SELECTION_CRITERION = 8,
    ERROR_CODE_INVALID_DATA_TYPE = 9,
    ERROR_CODE_INVALID_FILE_ACCESS_METHOD = 10,
    ERROR_CODE_INVALID_FILE_START_POSITION = 11,
    ERROR_CODE_INVALID_OPERATOR_NAME = 12,
    ERROR_CODE_INVALID_PARAMETER_DATA_TYPE = 13,
    ERROR_CODE_INVALID_TIME_STAMP = 14,
    ERROR_CODE_KEY_GENERATION_ERROR = 15,
    ERROR_CODE_MISSING_REQUIRED_PARAMETER = 16,
    ERROR_CODE_NO_OBJECTS_OF_SPECIFIED_TYPE = 17,
    ERROR_CODE_NO_SPACE_FOR_OBJECT = 18,
    ERROR_CODE_NO_SPACE_TO_ADD_LIST_ELEMENT = 19,
    ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY = 20,
    ERROR_CODE_NO_VT_SESSIONS_AVAILABLE = 21,
    ERROR_CODE_PROPERTY_IS_NOT_A_LIST = 22,
    ERROR_CODE_OBJECT_DELETION_NOT_PERMITTED = 23,
    ERROR_CODE_OBJECT_IDENTIFIER_ALREADY_EXISTS = 24,
    ERROR_CODE_OPERATIONAL_PROBLEM = 25,
    ERROR_CODE_PASSWORD_FAILURE = 26,
    ERROR_CODE_READ_ACCESS_DENIED = 27,
    ERROR_CODE_SECURITY_NOT_SUPPORTED = 28,
    ERROR_CODE_SERVICE_REQUEST_DENIED = 29,
    ERROR_CODE_TIMEOUT = 30,
    ERROR_CODE_UNKNOWN_OBJECT = 31,
    ERROR_CODE_UNKNOWN_PROPERTY = 32,
    ERROR_CODE_UNSUPPORTED_OBJECT_TYPE = 35,
    ERROR_CODE_VALUE_OUT_OF_RANGE = 37,
    ERROR_CODE_VT_SESSION_ALREADY_CLOSED = 38,
    ERROR_CODE_VT_SESSION_TERMINATION_FAILURE = 39,
    ERROR_CODE_WRITE_ACCESS_DENIED = 40,
    ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED = 41,
    ERROR_CODE_INVALID_ARRAY_INDEX = 42,
    ERROR_CODE_COV_SUBSCRIPTION_FAILED = 43,
    ERROR_CODE_NOT_COV_PROPERTY = 44,
    ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED = 45,
    ERROR_CODE_INVALID_CONFIGURATION_DATA = 46,
    ERROR_CODE_DATATYPE_NOT_SUPPORTED = 47,
    ERROR_CODE_DUPLICATE_NAME = 48,
    ERROR_CODE_DUPLICATE_OBJECT_ID = 49,
    ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY = 50,
    ERROR_CODE_ABORT_BUFFER_OVERFLOW = 51,
    ERROR_CODE_ABORT_INVALID_APDU_IN_THIS_STATE = 52,
    ERROR_CODE_ABORT_PREEMPTED_BY_HIGHER_PRIORITY_TASK = 53,
    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED = 54,
    ERROR_CODE_ABORT_PROPRIETARY = 55,
    ERROR_CODE_ABORT_OTHER = 56,
    ERROR_CODE_INVALID_TAG = 57,
    ERROR_CODE_NETWORK_DOWN = 58,
    ERROR_CODE_REJECT_BUFFER_OVERFLOW = 59,
    ERROR_CODE_REJECT_INCONSISTENT_PARAMETERS = 60,
    ERROR_CODE_REJECT_INVALID_PARAMETER_DATA_TYPE = 61,
    ERROR_CODE_REJECT_INVALID_TAG = 62,
    ERROR_CODE_REJECT_MISSING_REQUIRED_PARAMETER = 63,
    ERROR_CODE_REJECT_PARAMETER_OUT_OF_RANGE = 64,
    ERROR_CODE_REJECT_TOO_MANY_ARGUMENTS = 65,
    ERROR_CODE_REJECT_UNDEFINED_ENUMERATION = 66,
    ERROR_CODE_REJECT_UNRECOGNIZED_SERVICE = 67,
    ERROR_CODE_REJECT_PROPRIETARY = 68,
    ERROR_CODE_REJECT_OTHER = 69,
    MAX_BACNET_ERROR_CODE = 255
};

// =============================================================================
// BACnet 상수들
// =============================================================================

// 배열 인덱스 상수
constexpr uint32_t BACNET_ARRAY_ALL = 0xFFFFFFFF;
constexpr uint32_t BACNET_NO_PRIORITY = 0;

// 최대값들
constexpr uint32_t BACNET_MAX_INSTANCE = 4194303;
constexpr uint16_t BACNET_MAX_DEVICE_ID = 4194303;

// 기본 포트
constexpr uint16_t BACNET_DEFAULT_PORT = 47808;

// 타임아웃
constexpr uint32_t BACNET_DEFAULT_TIMEOUT_MS = 5000;

#endif // HAS_BACNET_STACK

// =============================================================================
// 공통 유틸리티 함수들 (BACnet 스택 유무와 관계없이 사용)
// =============================================================================

namespace PulseOne {
namespace Drivers {
namespace BACnet {

/**
 * @brief BACnet 객체 타입을 문자열로 변환
 */
inline std::string ObjectTypeToString(BACnetObjectType type) {
    switch (type) {
        case OBJECT_ANALOG_INPUT: return "Analog Input";
        case OBJECT_ANALOG_OUTPUT: return "Analog Output";
        case OBJECT_ANALOG_VALUE: return "Analog Value";
        case OBJECT_BINARY_INPUT: return "Binary Input";
        case OBJECT_BINARY_OUTPUT: return "Binary Output";
        case OBJECT_BINARY_VALUE: return "Binary Value";
        case OBJECT_DEVICE: return "Device";
        case OBJECT_MULTI_STATE_INPUT: return "Multistate Input";      // 🔥 수정됨
        case OBJECT_MULTI_STATE_OUTPUT: return "Multistate Output";    // 🔥 수정됨
        case OBJECT_MULTI_STATE_VALUE: return "Multistate Value";      // 🔥 이걸로 수정 (중복 제거)
        case OBJECT_TRENDLOG: return "Trend Log";
        case OBJECT_ACCUMULATOR: return "Accumulator";
        case OBJECT_CALENDAR: return "Calendar";
        case OBJECT_SCHEDULE: return "Schedule";
        case OBJECT_COMMAND: return "Command";
        case OBJECT_EVENT_ENROLLMENT: return "Event Enrollment";
        case OBJECT_FILE: return "File";
        case OBJECT_GROUP: return "Group";
        case OBJECT_LOOP: return "Loop";
        case OBJECT_NOTIFICATION_CLASS: return "Notification Class";   // 🔥 올바른 값 유지
        case OBJECT_PROGRAM: return "Program";
        default: return "Unknown (" + std::to_string(static_cast<int>(type)) + ")";
    }
}
/**
 * @brief 문자열을 BACnet 객체 타입으로 변환
 */
inline BACnetObjectType StringToObjectType(const std::string& type_str) {
    std::string upper_str = type_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "ANALOG_INPUT" || upper_str == "AI") return OBJECT_ANALOG_INPUT;
    if (upper_str == "ANALOG_OUTPUT" || upper_str == "AO") return OBJECT_ANALOG_OUTPUT;
    if (upper_str == "ANALOG_VALUE" || upper_str == "AV") return OBJECT_ANALOG_VALUE;
    if (upper_str == "BINARY_INPUT" || upper_str == "BI") return OBJECT_BINARY_INPUT;
    if (upper_str == "BINARY_OUTPUT" || upper_str == "BO") return OBJECT_BINARY_OUTPUT;
    if (upper_str == "BINARY_VALUE" || upper_str == "BV") return OBJECT_BINARY_VALUE;
    if (upper_str == "DEVICE") return OBJECT_DEVICE;
    if (upper_str == "MULTISTATE_INPUT" || upper_str == "MSI") return OBJECT_MULTI_STATE_INPUT;
    if (upper_str == "MULTISTATE_OUTPUT" || upper_str == "MSO") return OBJECT_MULTI_STATE_OUTPUT;
    if (upper_str == "MULTISTATE_VALUE" || upper_str == "MSV") return OBJECT_MULTI_STATE_VALUE;
    
    // 숫자로 파싱 시도
    try {
        int type_num = std::stoi(type_str);
        return static_cast<BACnetObjectType>(type_num);
    } catch (...) {
        return OBJECT_ANALOG_INPUT; // 기본값
    }
}

/**
 * @brief BACnet 프로퍼티 ID를 문자열로 변환
 */
inline std::string PropertyIdToString(BACnetPropertyId prop_id) {
    switch (prop_id) {
        case PROP_PRESENT_VALUE: return "Present Value";
        case PROP_DESCRIPTION: return "Description";
        case PROP_OBJECT_NAME: return "Object Name";
        case PROP_OBJECT_TYPE: return "Object Type";
        case PROP_OBJECT_IDENTIFIER: return "Object Identifier";
        case PROP_STATUS_FLAGS: return "Status Flags";
        case PROP_EVENT_STATE: return "Event State";
        case PROP_OUT_OF_SERVICE: return "Out Of Service";
        case PROP_UNITS: return "Units";
        case PROP_HIGH_LIMIT: return "High Limit";
        case PROP_LOW_LIMIT: return "Low Limit";
        case PROP_DEADBAND: return "Deadband";
        case PROP_COV_INCREMENT: return "COV Increment";
        case PROP_POLARITY: return "Polarity";
        case PROP_ACTIVE_TEXT: return "Active Text";
        case PROP_INACTIVE_TEXT: return "Inactive Text";
        case PROP_STATE_TEXT: return "State Text";
        case PROP_NUMBER_OF_STATES: return "Number Of States";
        case PROP_RELINQUISH_DEFAULT: return "Relinquish Default";
        case PROP_PRIORITY_ARRAY: return "Priority Array";
        default: return "Property " + std::to_string(static_cast<uint32_t>(prop_id));
    }
}

/**
 * @brief 문자열을 BACnet 프로퍼티 ID로 변환
 */
inline BACnetPropertyId StringToPropertyId(const std::string& prop_str) {
    std::string upper_str = prop_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "PRESENT_VALUE" || upper_str == "PV") return PROP_PRESENT_VALUE;
    if (upper_str == "DESCRIPTION") return PROP_DESCRIPTION;
    if (upper_str == "OBJECT_NAME") return PROP_OBJECT_NAME;
    if (upper_str == "OBJECT_TYPE") return PROP_OBJECT_TYPE;
    if (upper_str == "OBJECT_IDENTIFIER") return PROP_OBJECT_IDENTIFIER;
    if (upper_str == "STATUS_FLAGS") return PROP_STATUS_FLAGS;
    if (upper_str == "EVENT_STATE") return PROP_EVENT_STATE;
    if (upper_str == "OUT_OF_SERVICE") return PROP_OUT_OF_SERVICE;
    if (upper_str == "UNITS") return PROP_UNITS;
    if (upper_str == "HIGH_LIMIT") return PROP_HIGH_LIMIT;
    if (upper_str == "LOW_LIMIT") return PROP_LOW_LIMIT;
    
    // 숫자로 파싱 시도
    try {
        uint32_t prop_num = std::stoul(prop_str);
        return static_cast<BACnetPropertyId>(prop_num);
    } catch (...) {
        return PROP_PRESENT_VALUE; // 기본값
    }
}

/**
 * @brief BACnet 객체 ID 생성
 */
inline uint32_t CreateObjectId(BACnetObjectType type, uint32_t instance) {
    return (static_cast<uint32_t>(type) << 22) | (instance & 0x3FFFFF);
}

/**
 * @brief BACnet 객체 ID에서 타입 추출
 */
inline BACnetObjectType ExtractObjectType(uint32_t object_id) {
    return static_cast<BACnetObjectType>((object_id >> 22) & 0x3FF);
}

/**
 * @brief BACnet 객체 ID에서 인스턴스 추출
 */
inline uint32_t ExtractObjectInstance(uint32_t object_id) {
    return object_id & 0x3FFFFF;
}

} // namespace BACnet
} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_TYPES_H

// =============================================================================
// BACnetDriver.h와 BACnetObjectMapper.h에 추가할 include 문
// =============================================================================

/*
BACnetDriver.h와 BACnetObjectMapper.h 파일 상단에 이것을 추가:

#include "Drivers/Bacnet/BACnetTypes.h"

그러면 BACnetObjectType, BACnetPropertyId가 정의되어 컴파일 에러가 해결됩니다.
*/