// =============================================================================
// collector/include/Drivers/Bacnet/BACnetObjectMapper.h
// 🔥 BACnet 객체 매퍼 - 복잡한 객체 매핑 및 관리
// =============================================================================

#ifndef BACNET_OBJECT_MAPPER_H
#define BACNET_OBJECT_MAPPER_H


#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <functional>

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
}
#endif

namespace PulseOne {
namespace Drivers {

class BACnetDriver; // 전방 선언

/**
 * @brief BACnet 객체 매핑 관리자
 * 
 * 기능:
 * - DataPoint ↔ BACnet Object 매핑
 * - 복잡한 객체 구조 지원
 * - 객체 메타데이터 관리
 * - 동적 객체 발견 및 매핑
 * - 매핑 캐싱 및 최적화
 */
class BACnetObjectMapper {
public:
    // ==========================================================================
    // 타입 정의들
    // ==========================================================================
    
    /**
     * @brief 확장된 BACnet 객체 정보
     */
    struct ExtendedDataPoint : public DataPoint {
        // 추가 메타데이터
        std::string device_name;
        std::string object_description;
        std::string units;
        double scaling_factor = 1.0;
        double scaling_offset = 0.0;
        double min_value = 0.0;
        double max_value = 0.0;
        
        // 매핑 정보
        std::string mapping_key;                    // 고유 매핑 키
        std::string custom_identifier;              // 사용자 정의 식별자
        std::vector<BACNET_PROPERTY_ID> mapped_properties; // 매핑된 프로퍼티들
        uint32_t array_index = BACNET_ARRAY_ALL;
        
        // 상태 정보
        std::chrono::system_clock::time_point discovered_time;
        std::chrono::system_clock::time_point last_accessed;
        std::atomic<uint64_t> read_count{0};
        std::atomic<uint64_t> write_count{0};
        std::atomic<bool> is_active{true};
        
        ExtendedDataPoint();
        ExtendedDataPoint(const DataPoint& base);
        
        std::string GetFullIdentifier() const;
        bool IsStale(std::chrono::minutes max_age = std::chrono::minutes(60)) const;
    };
    
    /**
     * @brief 객체 매핑 규칙
     */
    struct MappingRule {
        std::string rule_name;
        std::function<bool(const ExtendedDataPoint&)> filter;
        std::function<std::string(const ExtendedDataPoint&)> identifier_generator;
        std::vector<BACNET_PROPERTY_ID> default_properties;
        bool auto_apply = true;
        
        MappingRule(const std::string& name);
    };
    
    /**
     * @brief 디바이스 객체 맵
     */
    struct DeviceObjectMap {
        uint32_t device_id;
        std::string device_name;
        std::map<std::string, ExtendedDataPoint> objects; // key = object_key
        std::chrono::system_clock::time_point last_discovery;
        std::atomic<bool> is_complete{false};
        std::atomic<uint32_t> total_objects{0};
        
        DeviceObjectMap(uint32_t id);
        std::string GetObjectKey(BACNET_OBJECT_TYPE type, uint32_t instance) const;
    };

public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    explicit BACnetObjectMapper(BACnetDriver* driver);
    ~BACnetObjectMapper() = default;
    
    // 복사/이동 방지
    BACnetObjectMapper(const BACnetObjectMapper&) = delete;
    BACnetObjectMapper& operator=(const BACnetObjectMapper&) = delete;
    
    // ==========================================================================
    // 🔥 객체 매핑 관리
    // ==========================================================================
    
    /**
     * @brief 단일 객체 매핑
     */
    bool MapObject(const std::string& custom_identifier,
                  uint32_t device_id,
                  BACNET_OBJECT_TYPE object_type,
                  uint32_t object_instance,
                  const std::vector<BACNET_PROPERTY_ID>& properties = {PROP_PRESENT_VALUE});
    
    /**
     * @brief DataPoint를 BACnet 객체로 매핑
     */
    bool MapDataPoint(const Structs::DataPoint& point);
    
    /**
     * @brief 디바이스의 모든 객체 자동 매핑
     */
    bool MapDeviceObjects(uint32_t device_id, 
                         const std::vector<MappingRule>& rules = {},
                         bool force_rediscovery = false);
    
    /**
     * @brief 매핑 제거
     */
    bool UnmapObject(const std::string& mapping_key);
    bool UnmapDevice(uint32_t device_id);
    
    // ==========================================================================
    // 🔥 객체 조회 및 변환
    // ==========================================================================
    
    /**
     * @brief 매핑된 객체 조회
     */
    bool GetMappedObject(const std::string& mapping_key, ExtendedDataPoint& object_info) const;
    
    /**
     * @brief DataPoint를 BACnet 객체로 변환
     */
    bool DataPointToBACnetObject(const Structs::DataPoint& point, ExtendedDataPoint& object_info) const;
    
    /**
     * @brief BACnet 객체를 DataPoint로 변환
     */
    bool BACnetObjectToDataPoint(const ExtendedDataPoint& object_info, Structs::DataPoint& point) const;
    
    /**
     * @brief 디바이스의 모든 매핑된 객체 조회
     */
    std::vector<ExtendedDataPoint> GetDeviceMappedObjects(uint32_t device_id) const;
    
    /**
     * @brief 모든 매핑된 객체 조회
     */
    std::vector<ExtendedDataPoint> GetAllMappedObjects() const;
    
    /**
     * @brief 매핑 키로 객체 검색
     */
    std::vector<ExtendedDataPoint> SearchObjects(const std::string& search_pattern) const;
    
    // ==========================================================================
    // 🔥 객체 발견 및 분석
    // ==========================================================================
    
    /**
     * @brief 디바이스 객체 발견
     */
    bool DiscoverDeviceObjects(uint32_t device_id, 
                              std::vector<DataPoint>& discovered_objects,
                              bool include_device_object = false);
    
    /**
     * @brief 객체 상세 정보 조회
     */
    bool GetObjectDetails(uint32_t device_id,
                         BACNET_OBJECT_TYPE object_type,
                         uint32_t object_instance,
                         ExtendedDataPoint& object_info);
    
    /**
     * @brief 객체 프로퍼티 목록 조회
     */
    std::vector<BACNET_PROPERTY_ID> GetObjectProperties(uint32_t device_id,
                                                       BACNET_OBJECT_TYPE object_type,
                                                       uint32_t object_instance);
    
    // ==========================================================================
    // 🔥 매핑 규칙 관리
    // ==========================================================================
    
    /**
     * @brief 매핑 규칙 추가
     */
    void AddMappingRule(const MappingRule& rule);
    
    /**
     * @brief 매핑 규칙 제거
     */
    bool RemoveMappingRule(const std::string& rule_name);
    
    /**
     * @brief 기본 매핑 규칙들 로드
     */
    void LoadDefaultMappingRules();
    
    /**
     * @brief 매핑 규칙 적용
     */
    bool ApplyMappingRules(uint32_t device_id, const std::vector<DataPoint>& objects);
    
    // ==========================================================================
    // 🔥 매핑 통계 및 상태
    // ==========================================================================
    
    /**
     * @brief 매핑 통계
     */
    struct MappingStatistics {
        std::atomic<uint32_t> total_mapped_objects{0};
        std::atomic<uint32_t> active_mappings{0};
        std::atomic<uint32_t> cached_devices{0};
        std::atomic<uint64_t> successful_discoveries{0};
        std::atomic<uint64_t> failed_discoveries{0};
        std::atomic<uint64_t> object_reads{0};
        std::atomic<uint64_t> object_writes{0};
        std::atomic<uint64_t> cache_hits{0};
        std::atomic<uint64_t> cache_misses{0};
        
        void Reset();
    };
    
    const MappingStatistics& GetMappingStatistics() const { return mapping_stats_; }
    
    /**
     * @brief 매핑 상태 보고서
     */
    std::string GetMappingReport() const;
    
    /**
     * @brief 매핑 건강성 체크
     */
    bool HealthCheck();
    
    // ==========================================================================
    // 캐시 및 최적화
    // ==========================================================================
    
    /**
     * @brief 캐시 정리
     */
    void CleanupCache(std::chrono::minutes max_age = std::chrono::minutes(60));
    
    /**
     * @brief 캐시 통계
     */
    struct CacheStatistics {
        size_t total_entries;
        size_t active_devices;
        size_t stale_entries;
        double hit_rate;
        std::chrono::system_clock::time_point last_cleanup;
    };
    
    CacheStatistics GetCacheStatistics() const;
    
    /**
     * @brief 매핑 데이터 내보내기/가져오기
     */
    std::string ExportMappings() const;
    bool ImportMappings(const std::string& mapping_data);

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    BACnetDriver* driver_;                    // 부모 드라이버
    MappingStatistics mapping_stats_;         // 매핑 통계
    
    // 매핑 저장소
    mutable std::mutex mappings_mutex_;
    std::map<std::string, ExtendedDataPoint> object_mappings_;  // mapping_key -> object_info
    std::map<uint32_t, DeviceObjectMap> device_maps_;                  // device_id -> device_map
    
    // 매핑 규칙
    std::mutex rules_mutex_;
    std::vector<MappingRule> mapping_rules_;
    
    // 캐시 관리
    std::mutex cache_mutex_;
    std::map<std::string, std::chrono::system_clock::time_point> discovery_cache_;
    std::map<std::string, std::vector<BACNET_PROPERTY_ID>> property_cache_;
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    // 키 생성
    std::string CreateMappingKey(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                                uint32_t object_instance) const;
    std::string CreateCacheKey(uint32_t device_id, const std::string& operation) const;
    
    // 객체 정보 수집
    bool ReadObjectName(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                       uint32_t object_instance, std::string& name);
    bool ReadObjectDescription(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                              uint32_t object_instance, std::string& description);
    bool ReadObjectUnits(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                        uint32_t object_instance, std::string& units);
    
    // 데이터 변환
    std::string BACnetObjectTypeToString(BACNET_OBJECT_TYPE object_type) const;
    std::string BACnetPropertyIdToString(BACNET_PROPERTY_ID property_id) const;
    BACNET_OBJECT_TYPE StringToBACnetObjectType(const std::string& type_str) const;
    BACNET_PROPERTY_ID StringToBACnetPropertyId(const std::string& property_str) const;
    
    // 매핑 유효성 검사
    bool ValidateMappingKey(const std::string& mapping_key) const;
    bool ValidateObjectInfo(const ExtendedDataPoint& object_info) const;
    
    // 통계 업데이트
    void UpdateMappingStatistics(const std::string& operation, bool success);
    void RecordObjectAccess(const std::string& mapping_key, bool is_write);
    
    // 에러 처리
    void LogMappingError(const std::string& operation, const std::string& error_message);
    
    // 캐시 헬퍼
    bool IsCacheValid(const std::string& cache_key, std::chrono::minutes max_age) const;
    void UpdateCache(const std::string& cache_key);
    
    // 기본 매핑 규칙들
    MappingRule CreateAnalogInputRule();
    MappingRule CreateAnalogOutputRule();
    MappingRule CreateBinaryInputRule();
    MappingRule CreateBinaryOutputRule();
    MappingRule CreateMultiStateRule();
    
    // 친구 클래스
    friend class BACnetDriver;
    friend class BACnetServiceManager;
};

// =============================================================================
// 🔥 인라인 구현들
// =============================================================================

inline BACnetObjectMapper::ExtendedDataPoint::ExtendedDataPoint() {
    discovered_time = std::chrono::system_clock::now();
    last_accessed = discovered_time;
}

inline BACnetObjectMapper::ExtendedDataPoint::ExtendedDataPoint(const DataPoint& base)
    : DataPoint(base) {
    discovered_time = std::chrono::system_clock::now();
    last_accessed = discovered_time;
}

inline std::string BACnetObjectMapper::ExtendedDataPoint::GetFullIdentifier() const {
    if (!custom_identifier.empty()) {
        return custom_identifier;
    }
    return device_name + "_" + object_name + "_" + std::to_string(object_instance);
}

inline bool BACnetObjectMapper::ExtendedDataPoint::IsStale(std::chrono::minutes max_age) const {
    auto now = std::chrono::system_clock::now();
    return (now - last_accessed) > max_age;
}

inline BACnetObjectMapper::MappingRule::MappingRule(const std::string& name) 
    : rule_name(name) {
    default_properties.push_back(PROP_PRESENT_VALUE);
}

inline BACnetObjectMapper::DeviceObjectMap::DeviceObjectMap(uint32_t id) 
    : device_id(id) {
    last_discovery = std::chrono::system_clock::now();
}

inline std::string BACnetObjectMapper::DeviceObjectMap::GetObjectKey(BACNET_OBJECT_TYPE type, uint32_t instance) const {
    return std::to_string(device_id) + "_" + std::to_string(type) + "_" + std::to_string(instance);
}

inline void BACnetObjectMapper::MappingStatistics::Reset() {
    total_mapped_objects = 0;
    active_mappings = 0;
    cached_devices = 0;
    successful_discoveries = 0;
    failed_discoveries = 0;
    object_reads = 0;
    object_writes = 0;
    cache_hits = 0;
    cache_misses = 0;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_OBJECT_MAPPER_H