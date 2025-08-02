// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCommonTypes.h
// 🔥 BACnet 전용 공통 타입 정의 - 컴파일 에러 수정 버전
// =============================================================================

#ifndef BACNET_COMMON_TYPES_H
#define BACNET_COMMON_TYPES_H

#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>

// BACnet 스택이 있는 경우 타입 포함
#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
}
#else
// 스택이 없는 경우 필요한 타입들을 더미로 정의
typedef enum {
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_ANALOG_OUTPUT = 1,
    OBJECT_ANALOG_VALUE = 2,
    OBJECT_BINARY_INPUT = 3,
    OBJECT_BINARY_OUTPUT = 4,
    OBJECT_BINARY_VALUE = 5,
    OBJECT_DEVICE = 8,
    OBJECT_MULTI_STATE_INPUT = 13,      // 🔥 올바른 이름 사용
    OBJECT_MULTI_STATE_OUTPUT = 14,     // 🔥 올바른 이름 사용
    OBJECT_MULTI_STATE_VALUE = 19,      // 🔥 올바른 이름 사용
    OBJECT_PROPRIETARY_MIN = 128
} BACNET_OBJECT_TYPE;

typedef enum {
    PROP_PRESENT_VALUE = 85,
    PROP_OUT_OF_SERVICE = 81,
    PROP_OBJECT_NAME = 77,
    PROP_OBJECT_LIST = 76,
    PROP_DESCRIPTION = 28,
    PROP_UNITS = 117,
    PROP_HIGH_LIMIT = 45,
    PROP_LOW_LIMIT = 59,
    PROP_PRIORITY_ARRAY = 87
} BACNET_PROPERTY_ID;

typedef enum {
    ERROR_CLASS_DEVICE = 0,
    ERROR_CLASS_OBJECT = 1,
    ERROR_CLASS_PROPERTY = 2,
    ERROR_CLASS_RESOURCES = 3,
    ERROR_CLASS_SECURITY = 4,
    ERROR_CLASS_SERVICES = 5,
    ERROR_CLASS_VT = 6,
    ERROR_CLASS_COMMUNICATION = 7
} BACNET_ERROR_CLASS;

typedef enum {
    ERROR_CODE_UNKNOWN_OBJECT = 31,
    ERROR_CODE_UNKNOWN_PROPERTY = 32,
    ERROR_CODE_VALUE_OUT_OF_RANGE = 37,
    ERROR_CODE_WRITE_ACCESS_DENIED = 40,
    ERROR_CODE_DEVICE_BUSY = 3,
    ERROR_CODE_COMMUNICATION_DISABLED = 83,
    ERROR_CODE_TIMEOUT = 6
} BACNET_ERROR_CODE;

typedef struct {
    uint8_t net;
    uint8_t len;
    uint8_t adr[6];
} BACNET_ADDRESS;

typedef struct {
    uint32_t type;
    uint32_t instance;
} BACNET_OBJECT_ID;

#define BACNET_MAX_INSTANCE 4194303U
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 🔥 성능 스냅샷 구조체 (BACnetStatisticsManager용)
// =============================================================================

/**
 * @brief 성능 측정 스냅샷
 */
struct PerformanceSnapshot {
    std::chrono::system_clock::time_point timestamp;  ///< 측정 시점
    uint64_t total_operations = 0;                     ///< 총 작업 수
    uint64_t successful_operations = 0;                ///< 성공한 작업 수
    uint64_t failed_operations = 0;                    ///< 실패한 작업 수
    double current_ops_per_second = 0.0;               ///< 현재 OPS
    double average_response_time_ms = 0.0;             ///< 평균 응답 시간 (ms)
    double error_rate_percent = 0.0;                   ///< 에러율 (%)
    uint64_t total_bytes_sent = 0;                     ///< 총 송신 바이트
    uint64_t total_bytes_received = 0;                 ///< 총 수신 바이트
    
    /**
     * @brief 기본 생성자
     */
    PerformanceSnapshot() : timestamp(std::chrono::system_clock::now()) {}
    
    /**
     * @brief JSON 형태로 직렬화
     */
    std::string ToJson() const {
        std::string json = "{\n";
        json += "  \"timestamp\": " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count()) + ",\n";
        json += "  \"total_operations\": " + std::to_string(total_operations) + ",\n";
        json += "  \"successful_operations\": " + std::to_string(successful_operations) + ",\n";
        json += "  \"failed_operations\": " + std::to_string(failed_operations) + ",\n";
        json += "  \"current_ops_per_second\": " + std::to_string(current_ops_per_second) + ",\n";
        json += "  \"average_response_time_ms\": " + std::to_string(average_response_time_ms) + ",\n";
        json += "  \"error_rate_percent\": " + std::to_string(error_rate_percent) + ",\n";
        json += "  \"total_bytes_sent\": " + std::to_string(total_bytes_sent) + ",\n";
        json += "  \"total_bytes_received\": " + std::to_string(total_bytes_received) + "\n";
        json += "}";
        return json;
    }
};

// =============================================================================
// BACnet 객체 정보 구조체
// =============================================================================

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type;    ///< 객체 타입
    uint32_t object_instance;          ///< 객체 인스턴스
    std::string object_name;           ///< 객체 이름
    std::string description;           ///< 설명
    bool out_of_service;               ///< 서비스 중단 여부
    
    /**
     * @brief 객체 타입을 문자열로 변환
     */
    static std::string ObjectTypeToString(BACNET_OBJECT_TYPE type) {
        switch (type) {
            case OBJECT_ANALOG_INPUT: return "AI";
            case OBJECT_ANALOG_OUTPUT: return "AO";
            case OBJECT_ANALOG_VALUE: return "AV";
            case OBJECT_BINARY_INPUT: return "BI";
            case OBJECT_BINARY_OUTPUT: return "BO";
            case OBJECT_BINARY_VALUE: return "BV";
            case OBJECT_DEVICE: return "DEV";
            case OBJECT_MULTI_STATE_INPUT: return "MSI";     // 🔥 올바른 상수 사용
            case OBJECT_MULTI_STATE_OUTPUT: return "MSO";    // 🔥 올바른 상수 사용
            case OBJECT_MULTI_STATE_VALUE: return "MSV";     // 🔥 올바른 상수 사용
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 기본 생성자
     */
    BACnetObjectInfo() 
        : object_type(OBJECT_ANALOG_INPUT), object_instance(0), out_of_service(false) {}
    
    /**
     * @brief 완전한 생성자
     */
    BACnetObjectInfo(BACNET_OBJECT_TYPE type, uint32_t instance, 
                     const std::string& name = "", const std::string& desc = "")
        : object_type(type), object_instance(instance), object_name(name), 
          description(desc), out_of_service(false) {}
          
    /**
     * @brief 객체 ID 문자열 생성
     */
    std::string GetObjectId() const {
        return ObjectTypeToString(object_type) + "." + std::to_string(object_instance);
    }
};

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id;                ///< 디바이스 ID
    std::string device_name;           ///< 디바이스 이름
    std::string vendor_name;           ///< 벤더 이름
    std::string model_name;            ///< 모델 이름
    std::string firmware_revision;     ///< 펌웨어 버전
    std::string description;           ///< 설명
    std::string ip_address;            ///< IP 주소
    uint16_t port;                     ///< 포트 번호
    uint32_t max_apdu_length;          ///< 최대 APDU 길이
    bool segmentation_supported;       ///< 분할 지원 여부
    std::chrono::system_clock::time_point last_seen;  ///< 마지막 확인 시간
    std::vector<BACnetObjectInfo> objects;  ///< 객체 목록
    
    /**
     * @brief 기본 생성자
     */
    BACnetDeviceInfo() 
        : device_id(0), port(47808), max_apdu_length(1476), 
          segmentation_supported(false), last_seen(std::chrono::system_clock::now()) {}
          
    /**
     * @brief 완전한 생성자
     */
    BACnetDeviceInfo(uint32_t id, const std::string& name, const std::string& ip)
        : device_id(id), device_name(name), ip_address(ip), port(47808), 
          max_apdu_length(1476), segmentation_supported(false), 
          last_seen(std::chrono::system_clock::now()) {}
};

// =============================================================================
// 유틸리티 함수들 (인라인으로 구현)
// =============================================================================

/**
 * @brief 문자열을 BACnet 객체 타입으로 변환
 */
inline BACNET_OBJECT_TYPE StringToBACnetObjectType(const std::string& str) {
    if (str == "AI") return OBJECT_ANALOG_INPUT;
    if (str == "AO") return OBJECT_ANALOG_OUTPUT;
    if (str == "AV") return OBJECT_ANALOG_VALUE;
    if (str == "BI") return OBJECT_BINARY_INPUT;
    if (str == "BO") return OBJECT_BINARY_OUTPUT;
    if (str == "BV") return OBJECT_BINARY_VALUE;
    if (str == "DEV") return OBJECT_DEVICE;
    if (str == "MSI") return OBJECT_MULTI_STATE_INPUT;      // 🔥 올바른 상수 사용
    if (str == "MSO") return OBJECT_MULTI_STATE_OUTPUT;     // 🔥 올바른 상수 사용
    if (str == "MSV") return OBJECT_MULTI_STATE_VALUE;      // 🔥 올바른 상수 사용
    return OBJECT_ANALOG_INPUT; // 기본값
}

/**
 * @brief BACnet 객체 타입을 문자열로 변환
 */
inline std::string BACnetObjectTypeToString(BACNET_OBJECT_TYPE type) {
    return BACnetObjectInfo::ObjectTypeToString(type);
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COMMON_TYPES_H