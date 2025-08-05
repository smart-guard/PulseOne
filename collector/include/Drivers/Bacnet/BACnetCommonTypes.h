// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCommonTypes.h
// 🔥 매크로 충돌 해결 + BACnetDeviceInfo 확장
// =============================================================================

#ifndef BACNET_COMMON_TYPES_H
#define BACNET_COMMON_TYPES_H

// ✅ 매크로 충돌 방지 - STL 먼저 인클루드
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <limits>

// ✅ BACnet 매크로와 STL 충돌 방지
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

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
    OBJECT_MULTI_STATE_INPUT = 13,
    OBJECT_MULTI_STATE_OUTPUT = 14,
    OBJECT_MULTI_STATE_VALUE = 19,
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

typedef struct {
    uint8_t net;
    uint8_t len;
    uint8_t adr[6];
} BACNET_ADDRESS;

#define BACNET_MAX_INSTANCE 4194303U
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// BACnet 객체 정보 구조체 (매크로 충돌 해결)
// =============================================================================

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type;    ///< 객체 타입
    uint32_t object_instance;          ///< 객체 인스턴스
    std::string object_name;           ///< 객체 이름
    std::string description;           ///< 설명
    std::string units;                 ///< 단위 정보
    
    bool out_of_service = false;       ///< 서비스 중단 상태
    std::string present_value;         ///< 현재 값 (문자열로 저장)
    
    // ✅ 매크로 충돌 해결: std::numeric_limits 사용
    double high_limit = (std::numeric_limits<double>::max)();
    double low_limit = (std::numeric_limits<double>::lowest)();
    
    BACnetObjectInfo() = default;
    
    /**
     * @brief 객체 식별자 문자열 생성
     */
    std::string GetObjectIdentifier() const {
        return std::to_string(static_cast<uint32_t>(object_type)) + ":" + 
               std::to_string(object_instance);
    }
};

// =============================================================================
// 🔥 확장된 BACnet 디바이스 정보 (모든 필요한 필드 포함)
// =============================================================================

/**
 * @brief BACnet 디바이스 정보 (완전한 버전)
 * @details BACnetDriver.h와 BACnetDiscoveryService에서 모두 사용 가능
 */
struct BACnetDeviceInfo {
    // ✅ 기본 디바이스 정보
    uint32_t device_id = 0;
    std::string device_name = "";
    
    // ✅ 네트워크 정보 (두 가지 방식 모두 지원)
    BACNET_ADDRESS address;              // BACnet 표준 주소 구조체
    std::string ip_address = "";         // 문자열 IP (편의용)
    uint16_t port = 47808;              // UDP 포트
    
    // ✅ BACnet 프로토콜 정보
    uint16_t max_apdu_length = 1476;
    uint8_t segmentation_support = 1;    // bool 대신 uint8_t (BACnet 표준)
    uint16_t vendor_id = 0;
    uint8_t protocol_version = 1;
    uint8_t protocol_revision = 14;
    
    // ✅ 상태 및 타이밍 정보
    std::chrono::system_clock::time_point last_seen;
    bool is_online = false;
    
    // ✅ 발견된 객체 목록
    std::vector<BACnetObjectInfo> objects;
    
    // ✅ 생성자들
    BACnetDeviceInfo() {
        last_seen = std::chrono::system_clock::now();
        // BACNET_ADDRESS 초기화
        address.net = 0;
        address.len = 0;
        for (int i = 0; i < 6; i++) {
            address.adr[i] = 0;
        }
    }
    
    BACnetDeviceInfo(uint32_t id, const std::string& name) 
        : BACnetDeviceInfo() {
        device_id = id;
        device_name = name;
    }
    
    // ✅ 유틸리티 메서드들
    
    /**
     * @brief IP 주소를 BACNET_ADDRESS로 변환
     */
    void SetIpAddress(const std::string& ip, uint16_t udp_port = 47808) {
        ip_address = ip;
        port = udp_port;
        
        // IP 문자열을 BACNET_ADDRESS로 변환
        unsigned int ip_parts[4];
        if (sscanf(ip.c_str(), "%u.%u.%u.%u", &ip_parts[0], &ip_parts[1], &ip_parts[2], &ip_parts[3]) == 4) {
            address.net = 0;
            address.len = 6;
            address.adr[0] = static_cast<uint8_t>(ip_parts[0]);
            address.adr[1] = static_cast<uint8_t>(ip_parts[1]);
            address.adr[2] = static_cast<uint8_t>(ip_parts[2]);
            address.adr[3] = static_cast<uint8_t>(ip_parts[3]);
            address.adr[4] = (udp_port >> 8) & 0xFF;
            address.adr[5] = udp_port & 0xFF;
        }
    }
    
    /**
     * @brief BACNET_ADDRESS에서 IP 주소 추출
     */
    std::string GetIpAddress() const {
        if (!ip_address.empty()) {
            return ip_address;
        }
        
        if (address.len >= 6) {
            return std::to_string(address.adr[0]) + "." +
                   std::to_string(address.adr[1]) + "." +
                   std::to_string(address.adr[2]) + "." +
                   std::to_string(address.adr[3]);
        }
        
        return "";
    }
    
    /**
     * @brief UDP 포트 추출
     */
    uint16_t GetPort() const {
        if (port != 47808) {
            return port;
        }
        
        if (address.len >= 6) {
            return (static_cast<uint16_t>(address.adr[4]) << 8) | address.adr[5];
        }
        
        return 47808;
    }
    
    /**
     * @brief 엔드포인트 문자열 생성 (ip:port 형식)
     */
    std::string GetEndpoint() const {
        return GetIpAddress() + ":" + std::to_string(GetPort());
    }
    
    /**
     * @brief 디바이스가 최근에 응답했는지 확인
     */
    bool IsRecentlyActive(std::chrono::seconds timeout = std::chrono::seconds(300)) const {
        auto now = std::chrono::system_clock::now();
        return (now - last_seen) < timeout;
    }
    
    /**
     * @brief 객체 추가
     */
    void AddObject(const BACnetObjectInfo& object) {
        // 중복 방지
        for (const auto& existing : objects) {
            if (existing.object_type == object.object_type && 
                existing.object_instance == object.object_instance) {
                return; // 이미 존재함
            }
        }
        objects.push_back(object);
    }
    
    /**
     * @brief 특정 타입의 객체 개수
     */
    size_t GetObjectCount(BACNET_OBJECT_TYPE type = OBJECT_PROPRIETARY_MIN) const {
        if (type == OBJECT_PROPRIETARY_MIN) {
            return objects.size(); // 모든 객체
        }
        
        size_t count = 0;
        for (const auto& obj : objects) {
            if (obj.object_type == type) {
                count++;
            }
        }
        return count;
    }
};

// =============================================================================
// 기타 BACnet 관련 구조체들
// =============================================================================

/**
 * @brief BACnet 읽기 요청
 */
struct BACnetReadRequest {
    uint32_t device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    int32_t array_index = -1;  // BACNET_ARRAY_ALL 대신
    std::string point_id;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
};

/**
 * @brief BACnet 쓰기 요청
 */
struct BACnetWriteRequest {
    uint32_t device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    PulseOne::Structs::DataValue value;
    int32_t array_index = -1;  // BACNET_ARRAY_ALL 대신
    uint8_t priority = 16;
    std::string point_id;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
};

/**
 * @brief BACnet 에러 정보
 */
struct BACnetErrorInfo {
    uint8_t error_class = 0;
    uint8_t error_code = 0;
    std::string error_message = "";
    std::string context = "";
    std::chrono::system_clock::time_point timestamp;
    
    BACnetErrorInfo() {
        timestamp = std::chrono::system_clock::now();
    }
    
    bool IsError() const {
        return error_class != 0 || error_code != 0;
    }
};

/**
 * @brief BACnet 성능 메트릭
 */
struct PerformanceMetrics {
    std::chrono::system_clock::time_point measurement_time;
    double success_rate_percent = 0.0;
    double error_rate_percent = 0.0;
    double avg_response_time_ms = 0.0;
    double current_ops_per_second = 0.0;
    uint64_t active_connections = 0;
    uint64_t peak_ops_per_second = 0;
    uint64_t total_memory_usage_bytes = 0;
    double cpu_usage_percent = 0.0;
    
    PerformanceMetrics() : measurement_time(std::chrono::system_clock::now()) {}
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COMMON_TYPES_H