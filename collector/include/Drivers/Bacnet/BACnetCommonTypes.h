// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCommonTypes.h (수정 버전)
// BACnet 특화 통계 및 타입 정의 추가
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
// 🔥 BACnet 설정 구조체 정의 (누락된 타입)
// =============================================================================

/**
 * @brief BACnet 드라이버 설정
 */
struct BACnetDriverConfig {
    // 기본 네트워크 설정
    std::string local_ip = "0.0.0.0";
    uint16_t local_port = 47808;
    uint32_t local_device_id = 12345;
    std::string device_name = "PulseOne-Collector";
    
    // Discovery 설정
    bool enable_discovery = true;
    uint32_t discovery_timeout_ms = 5000;
    uint16_t discovery_port = 47808;
    bool auto_add_discovered_devices = false;
    
    // 읽기/쓰기 설정
    uint32_t read_timeout_ms = 3000;
    uint32_t write_timeout_ms = 5000;
    uint8_t max_segments = 0;
    uint8_t max_apdu = 50;
    
    // 재시도 설정
    uint8_t max_retry_count = 3;
    uint32_t retry_delay_ms = 1000;
    
    // 성능 설정
    uint32_t max_concurrent_requests = 10;
    uint32_t request_queue_size = 100;
    bool enable_caching = true;
    uint32_t cache_ttl_ms = 30000;
    
    // 로깅 설정
    bool enable_packet_logging = false;
    bool enable_performance_logging = true;
    std::string log_level = "INFO";
};

// =============================================================================
// 🔥 BACnet 특화 통계 구조체 정의 (핵심!)
// =============================================================================

/**
 * @brief BACnet 프로토콜 특화 통계
 * @details DriverStatistics를 상속하여 BACnet 특화 기능 추가
 */
struct BACnetStatistics {
    // 🔥 공통 통계는 DriverStatistics 포함
    PulseOne::Structs::DriverStatistics base_statistics;
    
    // ==========================================================================
    // BACnet 프로토콜 특화 통계
    // ==========================================================================
    
    // 🔥 BACnet 서비스별 통계
    std::atomic<uint64_t> read_property_requests{0};
    std::atomic<uint64_t> read_property_multiple_requests{0};
    std::atomic<uint64_t> write_property_requests{0};
    std::atomic<uint64_t> write_property_multiple_requests{0};
    std::atomic<uint64_t> device_communication_control_requests{0};
    std::atomic<uint64_t> reinitialize_device_requests{0};
    std::atomic<uint64_t> who_is_requests{0};
    std::atomic<uint64_t> i_am_responses{0};
    
    // 🔥 BACnet 에러별 통계
    std::atomic<uint64_t> unknown_object_errors{0};
    std::atomic<uint64_t> unknown_property_errors{0};
    std::atomic<uint64_t> value_out_of_range_errors{0};
    std::atomic<uint64_t> write_access_denied_errors{0};
    std::atomic<uint64_t> device_busy_errors{0};
    std::atomic<uint64_t> communication_disabled_errors{0};
    std::atomic<uint64_t> timeout_errors{0};
    std::atomic<uint64_t> segment_errors{0};
    std::atomic<uint64_t> abort_errors{0};
    std::atomic<uint64_t> reject_errors{0};
    
    // 🔥 네트워크 레벨 통계
    std::atomic<uint64_t> broadcast_messages_sent{0};
    std::atomic<uint64_t> broadcast_messages_received{0};
    std::atomic<uint64_t> unicast_messages_sent{0};
    std::atomic<uint64_t> unicast_messages_received{0};
    std::atomic<uint64_t> malformed_packets{0};
    std::atomic<uint64_t> duplicate_packets{0};
    
    // 🔥 Discovery 관련 통계
    std::atomic<uint64_t> devices_discovered{0};
    std::atomic<uint64_t> objects_discovered{0};
    std::atomic<uint64_t> discovery_scans_completed{0};
    std::atomic<uint64_t> discovery_timeouts{0};
    
    // 🔥 세그멘테이션 통계
    std::atomic<uint64_t> segmented_requests_sent{0};
    std::atomic<uint64_t> segmented_responses_received{0};
    std::atomic<uint64_t> segment_ack_sent{0};
    std::atomic<uint64_t> segment_nak_sent{0};
    
    // 🔥 캐시 통계
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> cache_invalidations{0};
    
    // ==========================================================================
    // 생성자 및 유틸리티 메서드
    // ==========================================================================
    
    BACnetStatistics() = default;
    
    /**
     * @brief 공통 통계에 대한 참조 반환
     */
    const PulseOne::Structs::DriverStatistics& GetBaseStatistics() const {
        return base_statistics;
    }
    
    PulseOne::Structs::DriverStatistics& GetBaseStatistics() {
        return base_statistics;
    }
    
    /**
     * @brief BACnet 특화 통계 초기화
     */
    void ResetBACnetStatistics() {
        // BACnet 서비스별 통계 초기화
        read_property_requests = 0;
        read_property_multiple_requests = 0;
        write_property_requests = 0;
        write_property_multiple_requests = 0;
        device_communication_control_requests = 0;
        reinitialize_device_requests = 0;
        who_is_requests = 0;
        i_am_responses = 0;
        
        // BACnet 에러별 통계 초기화
        unknown_object_errors = 0;
        unknown_property_errors = 0;
        value_out_of_range_errors = 0;
        write_access_denied_errors = 0;
        device_busy_errors = 0;
        communication_disabled_errors = 0;
        timeout_errors = 0;
        segment_errors = 0;
        abort_errors = 0;
        reject_errors = 0;
        
        // 네트워크 레벨 통계 초기화
        broadcast_messages_sent = 0;
        broadcast_messages_received = 0;
        unicast_messages_sent = 0;
        unicast_messages_received = 0;
        malformed_packets = 0;
        duplicate_packets = 0;
        
        // Discovery 관련 통계 초기화
        devices_discovered = 0;
        objects_discovered = 0;
        discovery_scans_completed = 0;
        discovery_timeouts = 0;
        
        // 세그멘테이션 통계 초기화
        segmented_requests_sent = 0;
        segmented_responses_received = 0;
        segment_ack_sent = 0;
        segment_nak_sent = 0;
        
        // 캐시 통계 초기화
        cache_hits = 0;
        cache_misses = 0;
        cache_invalidations = 0;
    }
    
    /**
     * @brief 전체 통계 초기화 (공통 + BACnet 특화)
     */
    void Reset() {
        base_statistics.ResetStatistics();
        ResetBACnetStatistics();
    }
    
    /**
     * @brief BACnet 에러율 계산
     */
    double GetBACnetErrorRate() const {
        uint64_t total_bacnet_errors = unknown_object_errors.load() + 
                                     unknown_property_errors.load() +
                                     value_out_of_range_errors.load() +
                                     write_access_denied_errors.load() +
                                     device_busy_errors.load() +
                                     communication_disabled_errors.load() +
                                     timeout_errors.load() +
                                     segment_errors.load() +
                                     abort_errors.load() +
                                     reject_errors.load();
        
        uint64_t total_operations = base_statistics.total_operations.load();
        if (total_operations == 0) return 0.0;
        
        return (static_cast<double>(total_bacnet_errors) / total_operations) * 100.0;
    }
    
    /**
     * @brief 캐시 적중률 계산
     */
    double GetCacheHitRate() const {
        uint64_t total_cache_accesses = cache_hits.load() + cache_misses.load();
        if (total_cache_accesses == 0) return 0.0;
        
        return (static_cast<double>(cache_hits.load()) / total_cache_accesses) * 100.0;
    }
};

// =============================================================================
// 🔥 성능 메트릭스 구조체 정의 (중복 해결)
// =============================================================================

/**
 * @brief BACnet 성능 메트릭스 (BACnetStatisticsManager에서 사용)
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

// =============================================================================
// BACnet 객체 정보 구조체 (units 필드 추가)
// =============================================================================

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type;    ///< 객체 타입
    uint32_t object_instance;          ///< 객체 인스턴스
    std::string object_name;           ///< 객체 이름
    std::string description;           ///< 설명
    
    // 🔥 누락되었던 필드 추가
    std::string units;                 ///< 단위 정보
    
    bool out_of_service = false;       ///< 서비스 중단 상태
    std::string present_value;         ///< 현재 값 (문자열로 저장)
    double high_limit = std::numeric_limits<double>::max();
    double low_limit = std::numeric_limits<double>::lowest();
    
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
// 기타 BACnet 관련 구조체들
// =============================================================================

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id;
    std::string device_name;
    BACNET_ADDRESS address;
    uint16_t max_apdu_length;
    uint8_t segmentation_support;
    uint16_t vendor_id;
    std::vector<BACnetObjectInfo> objects;
    
    BACnetDeviceInfo() = default;
    BACnetDeviceInfo(uint32_t id, const std::string& name) 
        : device_id(id), device_name(name) {}
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COMMON_TYPES_H