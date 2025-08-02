// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCommonTypes.h
// 🔥 BACnet 공통 타입 정의 - 기존 시스템과 호환
// =============================================================================

#ifndef BACNET_COMMON_TYPES_H
#define BACNET_COMMON_TYPES_H

#include "Common/UnifiedCommonTypes.h"
#include <chrono>
#include <string>
#include <vector>
#include <atomic>

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
}
#else
// BACnet 스택이 없을 때의 더미 정의
typedef enum {
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_ANALOG_OUTPUT = 1,
    OBJECT_ANALOG_VALUE = 2,
    OBJECT_BINARY_INPUT = 3,
    OBJECT_BINARY_OUTPUT = 4,
    OBJECT_BINARY_VALUE = 5,
    OBJECT_DEVICE = 8,
    OBJECT_MULTI_STATE_INPUT = 13,
    OBJECT_MULTI_STATE_OUTPUT = 14,
    OBJECT_MULTI_STATE_VALUE = 19,
    OBJECT_PROPRIETARY_MIN = 128
} BACNET_OBJECT_TYPE;

typedef enum {
    PROP_PRESENT_VALUE = 85,
    PROP_OBJECT_NAME = 77,
    PROP_OBJECT_TYPE = 79,
    PROP_OBJECT_IDENTIFIER = 75,
    PROP_DESCRIPTION = 28,
    PROP_UNITS = 117,
    PROP_STATUS_FLAGS = 111,
    PROP_RELIABILITY = 103,
    PROP_OUT_OF_SERVICE = 81,
    PROP_PRIORITY_ARRAY = 87,
    PROP_RELINQUISH_DEFAULT = 104,
    PROP_OBJECT_LIST = 76
} BACNET_PROPERTY_ID;

#define BACNET_ARRAY_ALL 0xFFFFFFFF
#define BACNET_NO_PRIORITY 0
#define SEGMENTATION_NONE 0
#define SEGMENTATION_BOTH 3
#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id = 0;
    std::string device_name;
    uint32_t max_apdu_length = 1476;
    bool segmentation_supported = false;
    uint16_t vendor_id = 0;
    std::string ip_address;
    uint16_t port = 47808;
    std::chrono::system_clock::time_point last_seen;
    bool online = false;
    
    // 추가 정보
    std::string vendor_name;
    std::string model_name;
    std::string firmware_revision;
    std::string application_software_version;
    uint16_t protocol_version = 1;
    uint16_t protocol_revision = 22;
    
    BACnetDeviceInfo() {
        last_seen = std::chrono::system_clock::now();
    }
    
    bool IsOnline() const {
        auto now = std::chrono::system_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_seen);
        return online && (time_diff.count() < 5); // 5분 이내에 응답한 경우 온라인으로 간주
    }
    
    std::string GetEndpoint() const {
        return ip_address + ":" + std::to_string(port);
    }
};

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type = OBJECT_ANALOG_INPUT;
    uint32_t object_instance = 0;
    std::string object_name;
    std::string description;
    BACNET_PROPERTY_ID property_id = PROP_PRESENT_VALUE;
    uint32_t array_index = BACNET_ARRAY_ALL;
    
    // 상태 정보
    std::chrono::system_clock::time_point timestamp;
    Enums::DataQuality quality = Enums::DataQuality::GOOD;
    
    BACnetObjectInfo() {
        timestamp = std::chrono::system_clock::now();
    }
    
    BACnetObjectInfo(BACNET_OBJECT_TYPE type, uint32_t instance, 
                    const std::string& name = "", const std::string& desc = "",
                    BACNET_PROPERTY_ID prop = PROP_PRESENT_VALUE)
        : object_type(type), object_instance(instance), object_name(name), 
          description(desc), property_id(prop) {
        timestamp = std::chrono::system_clock::now();
    }
    
    std::string GetObjectKey() const {
        return std::to_string(object_type) + "_" + std::to_string(object_instance);
    }
    
    std::string GetFullIdentifier() const {
        if (!object_name.empty()) {
            return object_name + " (" + GetObjectKey() + ")";
        }
        return GetObjectKey();
    }
    
    // 비교 연산자 (맵에서 키로 사용하기 위해)
    bool operator<(const BACnetObjectInfo& other) const {
        if (object_type != other.object_type) {
            return object_type < other.object_type;
        }
        if (object_instance != other.object_instance) {
            return object_instance < other.object_instance;
        }
        return property_id < other.property_id;
    }
    
    bool operator==(const BACnetObjectInfo& other) const {
        return object_type == other.object_type && 
               object_instance == other.object_instance &&
               property_id == other.property_id;
    }
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COMMON_TYPES_H