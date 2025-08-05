#ifndef BACNET_COMMON_TYPES_H
#define BACNET_COMMON_TYPES_H


#include "Common/Structs.h"  // 🔥 통합 DeviceInfo, DataPoint 사용

namespace PulseOne {
namespace Drivers {

// 🔥 개별 구조체는 삭제! 통합 DeviceInfo와 DataPoint만 사용
// 대신 properties 키들과 유틸리티 함수들만 제공

namespace BACnetKeys {
    // DeviceInfo.properties에서 사용할 키들
    constexpr const char* DEVICE_ID = "bacnet_device_id";
    constexpr const char* VENDOR_ID = "vendor_id";
    constexpr const char* MAX_APDU_LENGTH = "max_apdu_length";
    constexpr const char* SEGMENTATION_SUPPORT = "segmentation_support";
    constexpr const char* PROTOCOL_VERSION = "protocol_version";
    constexpr const char* MODEL_NAME = "model_name";
    constexpr const char* FIRMWARE_REVISION = "firmware_revision";
    
    // DataPoint.properties에서 사용할 키들
    constexpr const char* OBJECT_TYPE = "bacnet_object_type";
    constexpr const char* OBJECT_INSTANCE = "bacnet_object_instance";
    constexpr const char* PROPERTY_ID = "bacnet_property_id";
}

namespace BACnetUtils {
    
    /**
     * @brief 통합 DeviceInfo에서 BACnet Device ID 추출
     */
    inline uint32_t GetBACnetDeviceId(const Structs::DeviceInfo& device_info) {
        auto it = device_info.properties.find(BACnetKeys::DEVICE_ID);
        return (it != device_info.properties.end()) ? std::stoul(it->second) : 0;
    }
    
    /**
     * @brief 통합 DeviceInfo에 BACnet Device ID 설정
     */
    inline void SetBACnetDeviceId(Structs::DeviceInfo& device_info, uint32_t device_id) {
        device_info.properties[BACnetKeys::DEVICE_ID] = std::to_string(device_id);
    }
    
    /**
     * @brief endpoint에서 포트 추출
     */
    inline uint16_t GetPortFromEndpoint(const std::string& endpoint) {
        size_t colon_pos = endpoint.find(':');
        if (colon_pos != std::string::npos) {
            return static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
        }
        return 47808; // 기본 BACnet 포트
    }
    
    /**
     * @brief endpoint에서 IP 추출
     */
    inline std::string GetIpFromEndpoint(const std::string& endpoint) {
        size_t colon_pos = endpoint.find(':');
        if (colon_pos != std::string::npos) {
            return endpoint.substr(0, colon_pos);
        }
        return endpoint;
    }
    
    /**
     * @brief BACNET_ADDRESS 변환 유틸리티
     */
    inline BACNET_ADDRESS EndpointToBACnetAddress(const std::string& endpoint) {
        BACNET_ADDRESS address = {0};
        std::string ip = GetIpFromEndpoint(endpoint);
        uint16_t port = GetPortFromEndpoint(endpoint);
        
        unsigned int ip_parts[4];
        if (sscanf(ip.c_str(), "%u.%u.%u.%u", &ip_parts[0], &ip_parts[1], &ip_parts[2], &ip_parts[3]) == 4) {
            address.net = 0;
            address.len = 6;
            address.adr[0] = static_cast<uint8_t>(ip_parts[0]);
            address.adr[1] = static_cast<uint8_t>(ip_parts[1]);
            address.adr[2] = static_cast<uint8_t>(ip_parts[2]);
            address.adr[3] = static_cast<uint8_t>(ip_parts[3]);
            address.adr[4] = (port >> 8) & 0xFF;
            address.adr[5] = port & 0xFF;
        }
        return address;
    }
}

} // namespace Drivers  
} // namespace PulseOne

#endif
