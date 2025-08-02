// =============================================================================
// collector/include/Drivers/Bacnet/BACnetCommonTypes.h
// 🔥 BACnet 전용 공통 타입 정의 - 기존 시스템과 통합
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
    OBJECT_MULTISTATE_INPUT = 13,
    OBJECT_MULTISTATE_OUTPUT = 14,
    OBJECT_MULTISTATE_VALUE = 19
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
    ERROR_CODE_TIMEOUT = 67
} BACNET_ERROR_CODE;

#define BACNET_ARRAY_ALL (-1)
#define BACNET_MAX_INSTANCE 0x3FFFFF
#define MAX_APDU 1476
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 🔥 BACnet 디바이스 정보
// =============================================================================

/**
 * @brief BACnet 디바이스 정보 구조체
 */
struct BACnetDeviceInfo {
    uint32_t device_id;                    // 디바이스 ID
    std::string device_name;               // 디바이스 이름
    std::string ip_address;                // IP 주소
    uint16_t port;                         // 포트 번호
    uint16_t max_apdu_length;              // 최대 APDU 길이
    bool segmentation_supported;           // 세그멘테이션 지원 여부
    uint16_t vendor_id;                    // 벤더 ID
    uint8_t protocol_version;              // 프로토콜 버전
    uint8_t protocol_revision;             // 프로토콜 리비전
    
    // 발견 정보
    std::chrono::system_clock::time_point discovery_time;
    std::chrono::system_clock::time_point last_seen;
    bool is_online;
    
    // 추가 정보
    std::string vendor_name;
    std::string model_name;
    std::string application_software_version;
    std::string description;
    
    BACnetDeviceInfo() 
        : device_id(0), port(47808), max_apdu_length(1476)
        , segmentation_supported(true), vendor_id(0)
        , protocol_version(1), protocol_revision(14)
        , is_online(false) {
        discovery_time = std::chrono::system_clock::now();
        last_seen = discovery_time;
    }
    
    /**
     * @brief 디바이스가 활성 상태인지 확인
     * @param timeout_seconds 타임아웃 시간 (기본: 300초)
     * @return 활성 상태면 true
     */
    bool IsActive(int timeout_seconds = 300) const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_seen);
        return elapsed.count() < timeout_seconds;
    }
    
    /**
     * @brief 마지막 접촉 시간 업데이트
     */
    void UpdateLastSeen() {
        last_seen = std::chrono::system_clock::now();
        is_online = true;
    }
};

// =============================================================================
// 🔥 BACnet 객체 정보
// =============================================================================

/**
 * @brief BACnet 객체 정보 구조체
 */
struct BACnetObjectInfo {
    uint32_t device_id;                    // 소속 디바이스 ID
    BACNET_OBJECT_TYPE object_type;        // 객체 타입
    uint32_t object_instance;              // 객체 인스턴스
    std::string object_name;               // 객체 이름
    std::string description;               // 객체 설명
    
    // 속성 정보
    std::vector<BACNET_PROPERTY_ID> supported_properties;
    
    // 메타데이터
    bool is_commandable;                   // 제어 가능 여부
    bool is_readable;                      // 읽기 가능 여부
    bool is_writable;                      // 쓰기 가능 여부
    std::string units;                     // 단위 정보
    
    BACnetObjectInfo()
        : device_id(0), object_type(OBJECT_ANALOG_INPUT), object_instance(0)
        , is_commandable(false), is_readable(true), is_writable(false) {}
    
    BACnetObjectInfo(uint32_t dev_id, BACNET_OBJECT_TYPE type, uint32_t instance)
        : device_id(dev_id), object_type(type), object_instance(instance)
        , is_commandable(false), is_readable(true), is_writable(false) {}
    
    /**
     * @brief 객체 식별자 문자열 반환
     * @return "Device123.AI.0" 형식의 문자열
     */
    std::string GetIdentifier() const {
        return "Device" + std::to_string(device_id) + "." + 
               ObjectTypeToString(object_type) + "." + 
               std::to_string(object_instance);
    }
    
    /**
     * @brief 속성이 지원되는지 확인
     */
    bool SupportsProperty(BACNET_PROPERTY_ID property) const {
        return std::find(supported_properties.begin(), supported_properties.end(), property) 
               != supported_properties.end();
    }
    
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
            case OBJECT_DEVICE: return "DEVICE";
            case OBJECT_MULTISTATE_INPUT: return "MSI";
            case OBJECT_MULTISTATE_OUTPUT: return "MSO";
            case OBJECT_MULTISTATE_VALUE: return "MSV";
            default: return "UNKNOWN";
        }
    }
};

// =============================================================================
// 🔥 BACnet 특화 통계 구조체
// =============================================================================

/**
 * @brief BACnet 프로토콜 전용 통계
 */
struct BACnetStatistics {
    // 기본 작업 통계
    std::atomic<uint64_t> total_read_requests{0};
    std::atomic<uint64_t> successful_reads{0};
    std::atomic<uint64_t> failed_reads{0};
    std::atomic<uint64_t> total_write_requests{0};
    std::atomic<uint64_t> successful_writes{0};
    std::atomic<uint64_t> failed_writes{0};
    
    // 연결 통계
    std::atomic<uint64_t> connection_attempts{0};
    std::atomic<uint64_t> successful_connections{0};
    std::atomic<uint64_t> connection_failures{0};
    
    // BACnet 특화 통계
    std::atomic<uint64_t> who_is_sent{0};
    std::atomic<uint64_t> i_am_received{0};
    std::atomic<uint64_t> read_property_requests{0};
    std::atomic<uint64_t> write_property_requests{0};
    std::atomic<uint64_t> read_property_multiple_requests{0};
    std::atomic<uint64_t> write_property_multiple_requests{0};
    std::atomic<uint64_t> cov_subscriptions{0};
    std::atomic<uint64_t> cov_notifications_received{0};
    
    // 에러 통계
    std::atomic<uint64_t> error_count{0};
    std::atomic<uint64_t> timeout_errors{0};
    std::atomic<uint64_t> protocol_errors{0};
    std::atomic<uint64_t> communication_errors{0};
    std::atomic<uint64_t> device_errors{0};
    std::atomic<uint64_t> object_errors{0};
    std::atomic<uint64_t> property_errors{0};
    
    // 성능 지표
    std::atomic<uint64_t> total_response_time_ms{0};
    std::atomic<uint64_t> max_response_time_ms{0};
    std::atomic<uint64_t> min_response_time_ms{UINT64_MAX};
    
    // 네트워크 통계
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    
    // 디바이스 발견 통계
    std::atomic<uint64_t> devices_discovered{0};
    std::atomic<uint64_t> discovery_attempts{0};
    
    // 시간 정보
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;
    
    BACnetStatistics() {
        start_time = std::chrono::system_clock::now();
        last_update = start_time;
        min_response_time_ms.store(0); // 초기값 수정
    }
    
    /**
     * @brief 읽기 성공률 계산
     */
    double GetReadSuccessRate() const {
        uint64_t total = total_read_requests.load();
        return total > 0 ? (static_cast<double>(successful_reads.load()) / total) * 100.0 : 0.0;
    }
    
    /**
     * @brief 쓰기 성공률 계산
     */
    double GetWriteSuccessRate() const {
        uint64_t total = total_write_requests.load();
        return total > 0 ? (static_cast<double>(successful_writes.load()) / total) * 100.0 : 0.0;
    }
    
    /**
     * @brief 평균 응답 시간 계산
     */
    double GetAverageResponseTime() const {
        uint64_t total_ops = total_read_requests.load() + total_write_requests.load();
        uint64_t total_time = total_response_time_ms.load();
        return total_ops > 0 ? static_cast<double>(total_time) / total_ops : 0.0;
    }
    
    /**
     * @brief 초당 작업 수 계산
     */
    double GetOperationsPerSecond() const {
        auto now = std::chrono::system_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        uint64_t total_ops = total_read_requests.load() + total_write_requests.load();
        return runtime.count() > 0 ? static_cast<double>(total_ops) / runtime.count() : 0.0;
    }
    
    /**
     * @brief 에러율 계산 (백분율)
     */
    double GetErrorRate() const {
        uint64_t total_ops = total_read_requests.load() + total_write_requests.load();
        return total_ops > 0 ? (static_cast<double>(error_count.load()) / total_ops) * 100.0 : 0.0;
    }
};

// =============================================================================
// 🔥 성능 지표 구조체들
// =============================================================================

/**
 * @brief 실시간 성능 지표
 */
struct PerformanceMetrics {
    double current_ops_per_second;         // 현재 초당 작업 수
    double avg_response_time_ms;           // 평균 응답 시간
    double success_rate_percent;           // 성공률 (%)
    double error_rate_percent;             // 에러율 (%)
    uint64_t active_connections;           // 활성 연결 수
    uint64_t pending_requests;             // 대기 중인 요청 수
    
    // 최근 성능 (최근 1분간)
    double recent_ops_per_second;
    double recent_avg_response_time_ms;
    double recent_success_rate_percent;
    
    std::chrono::system_clock::time_point measurement_time;
    
    PerformanceMetrics() 
        : current_ops_per_second(0.0), avg_response_time_ms(0.0)
        , success_rate_percent(0.0), error_rate_percent(0.0)
        , active_connections(0), pending_requests(0)
        , recent_ops_per_second(0.0), recent_avg_response_time_ms(0.0)
        , recent_success_rate_percent(0.0) {
        measurement_time = std::chrono::system_clock::now();
    }
};

/**
 * @brief COV 구독 정보
 */
struct COVSubscriptionInfo {
    uint32_t device_id;
    uint32_t object_instance;
    BACNET_OBJECT_TYPE object_type;
    uint32_t subscriber_process_id;
    uint32_t lifetime_seconds;
    bool confirmed_notifications;
    
    std::chrono::system_clock::time_point subscription_time;
    std::chrono::system_clock::time_point expiry_time;
    std::chrono::system_clock::time_point last_notification;
    
    std::atomic<uint64_t> notifications_received{0};
    std::atomic<bool> is_active{false};
    
    COVSubscriptionInfo()
        : device_id(0), object_instance(0), object_type(OBJECT_ANALOG_INPUT)
        , subscriber_process_id(0), lifetime_seconds(300), confirmed_notifications(false) {
        subscription_time = std::chrono::system_clock::now();
        expiry_time = subscription_time + std::chrono::seconds(lifetime_seconds);
        last_notification = subscription_time;
    }
    
    /**
     * @brief 구독이 만료되었는지 확인
     */
    bool IsExpired() const {
        return std::chrono::system_clock::now() > expiry_time;
    }
    
    /**
     * @brief 남은 시간 계산
     */
    std::chrono::seconds GetRemainingTime() const {
        auto now = std::chrono::system_clock::now();
        if (now >= expiry_time) {
            return std::chrono::seconds(0);
        }
        return std::chrono::duration_cast<std::chrono::seconds>(expiry_time - now);
    }
};

// =============================================================================
// 🔥 BACnet 설정 구조체들
// =============================================================================

/**
 * @brief BACnet 드라이버 설정
 */
struct BACnetDriverConfig {
    // 기본 네트워크 설정
    uint32_t local_device_id;
    std::string local_ip;
    uint16_t local_port;
    std::string target_ip;
    uint16_t target_port;
    
    // BACnet 설정
    uint16_t max_apdu_length;
    bool segmentation_supported;
    uint32_t timeout_ms;
    uint32_t retry_count;
    
    // 고급 설정
    bool enable_cov;
    bool enable_rpm; // Read Property Multiple
    bool enable_wpm; // Write Property Multiple
    bool auto_device_discovery;
    uint32_t discovery_interval_seconds;
    
    // 성능 설정
    uint32_t max_concurrent_requests;
    uint32_t request_queue_size;
    bool enable_request_batching;
    uint32_t batch_size;
    
    // 디버그 설정
    bool verbose_logging;
    bool enable_statistics;
    std::string log_level;
    
    BACnetDriverConfig()
        : local_device_id(1234), local_port(47808), target_port(47808)
        , max_apdu_length(1476), segmentation_supported(true)
        , timeout_ms(5000), retry_count(3)
        , enable_cov(false), enable_rpm(true), enable_wpm(true)
        , auto_device_discovery(true), discovery_interval_seconds(300)
        , max_concurrent_requests(10), request_queue_size(100)
        , enable_request_batching(true), batch_size(10)
        , verbose_logging(false), enable_statistics(true), log_level("INFO") {}
    
    /**
     * @brief 설정 유효성 검사
     */
    bool Validate() const {
        if (local_device_id == 0 || local_device_id > BACNET_MAX_INSTANCE) {
            return false;
        }
        if (local_port == 0 || target_port == 0) {
            return false;
        }
        if (max_apdu_length < 50 || max_apdu_length > 1476) {
            return false;
        }
        if (timeout_ms == 0 || timeout_ms > 60000) {
            return false;
        }
        if (retry_count > 10) {
            return false;
        }
        return true;
    }
};

// =============================================================================
// 🔥 유틸리티 함수들
// =============================================================================

/**
 * @brief BACnet 객체 타입을 문자열로 변환
 */
inline std::string BACnetObjectTypeToString(BACNET_OBJECT_TYPE type) {
    return BACnetObjectInfo::ObjectTypeToString(type);
}

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
    if (str == "DEVICE") return OBJECT_DEVICE;
    if (str == "MSI") return OBJECT_MULTISTATE_INPUT;
    if (str == "MSO") return OBJECT_MULTISTATE_OUTPUT;
    if (str == "MSV") return OBJECT_MULTISTATE_VALUE;
    return OBJECT_ANALOG_INPUT; // 기본값
}

/**
 * @brief BACnet 속성 ID를 문자열로 변환
 */
inline std::string BACnetPropertyToString(BACNET_PROPERTY_ID prop) {
    switch (prop) {
        case PROP_PRESENT_VALUE: return "PV";
        case PROP_OUT_OF_SERVICE: return "OOS";
        case PROP_OBJECT_NAME: return "NAME";
        case PROP_OBJECT_LIST: return "OBJECT_LIST";
        case PROP_DESCRIPTION: return "DESCRIPTION";
        case PROP_UNITS: return "UNITS";
        case PROP_HIGH_LIMIT: return "HIGH_LIMIT";
        case PROP_LOW_LIMIT: return "LOW_LIMIT";
        case PROP_PRIORITY_ARRAY: return "PRIORITY_ARRAY";
        default: return "PROP_" + std::to_string(static_cast<int>(prop));
    }
}

/**
 * @brief 문자열을 BACnet 속성 ID로 변환
 */
inline BACNET_PROPERTY_ID StringToBACnetProperty(const std::string& str) {
    if (str == "PV") return PROP_PRESENT_VALUE;
    if (str == "OOS") return PROP_OUT_OF_SERVICE;
    if (str == "NAME") return PROP_OBJECT_NAME;
    if (str == "OBJECT_LIST") return PROP_OBJECT_LIST;
    if (str == "DESCRIPTION") return PROP_DESCRIPTION;
    if (str == "UNITS") return PROP_UNITS;
    if (str == "HIGH_LIMIT") return PROP_HIGH_LIMIT;
    if (str == "LOW_LIMIT") return PROP_LOW_LIMIT;
    if (str == "PRIORITY_ARRAY") return PROP_PRIORITY_ARRAY;
    
    // 숫자로 시도
    try {
        return static_cast<BACNET_PROPERTY_ID>(std::stoi(str));
    } catch (...) {
        return PROP_PRESENT_VALUE; // 기본값
    }
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_COMMON_TYPES_H