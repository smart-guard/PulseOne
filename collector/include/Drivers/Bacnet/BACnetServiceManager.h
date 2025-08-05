// =============================================================================
// collector/include/Drivers/Bacnet/BACnetServiceManager.h
// 🔥 BACnet 고급 서비스 관리자 - RPM, WPM, Object Services
// =============================================================================

#ifndef BACNET_SERVICE_MANAGER_H
#define BACNET_SERVICE_MANAGER_H

#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <future>
#include <atomic>

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
    #include <bacnet/bacaddr.h>
}
#endif

namespace PulseOne {
namespace Drivers {

class BACnetDriver; // 전방 선언

/**
 * @brief BACnet 고급 서비스 관리자
 * 
 * 다음 고급 BACnet 서비스들을 관리:
 * - Read Property Multiple (RPM)
 * - Write Property Multiple (WPM) 
 * - Create/Delete Object
 * - Device Discovery 최적화
 * - 배치 처리 최적화
 */
class BACnetServiceManager {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    explicit BACnetServiceManager(BACnetDriver* driver);
    ~BACnetServiceManager();
    
    // 복사/이동 방지
    BACnetServiceManager(const BACnetServiceManager&) = delete;
    BACnetServiceManager& operator=(const BACnetServiceManager&) = delete;
    
    // ==========================================================================
    // 🔥 고급 읽기 서비스
    // ==========================================================================
    
    /**
     * @brief Read Property Multiple 서비스
     * @param device_id 대상 디바이스 ID
     * @param objects 읽을 객체 목록
     * @param results 결과 저장소
     * @param timeout_ms 타임아웃 (기본: 5000ms)
     * @return 성공 여부
     */
    bool ReadPropertyMultiple(uint32_t device_id,
                             const std::vector<DataPoint>& objects,
                             std::vector<TimestampedValue>& results,
                             uint32_t timeout_ms = 5000);
    
    /**
     * @brief 최적화된 배치 읽기 (자동 RPM 그룹핑)
     * @param device_id 대상 디바이스 ID
     * @param points DataPoint 목록 (자동으로 BACnet 객체로 변환)
     * @param results 결과 저장소
     * @return 성공 여부
     */
    bool BatchRead(uint32_t device_id,
                   const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& results);
    
    /**
     * @brief 단일 프로퍼티 읽기 (기본 Read Property)
     */
    bool ReadProperty(uint32_t device_id,
                     BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance,
                     BACNET_PROPERTY_ID property_id,
                     TimestampedValue& result,
                     uint32_t array_index = BACNET_ARRAY_ALL);
    
    // ==========================================================================
    // 🔥 고급 쓰기 서비스
    // ==========================================================================
    
    /**
     * @brief Write Property Multiple 서비스
     */
    bool WritePropertyMultiple(uint32_t device_id,
                              const std::map<DataPoint, Structs::DataValue>& values,
                              uint32_t timeout_ms = 5000);
    
    /**
     * @brief 최적화된 배치 쓰기
     */
    bool BatchWrite(uint32_t device_id,
                    const std::vector<std::pair<Structs::DataPoint, Structs::DataValue>>& point_values);
    
    /**
     * @brief 단일 프로퍼티 쓰기
     */
    bool WriteProperty(uint32_t device_id,
                      BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance,
                      BACNET_PROPERTY_ID property_id,
                      const Structs::DataValue& value,
                      uint8_t priority = BACNET_NO_PRIORITY,
                      uint32_t array_index = BACNET_ARRAY_ALL);
    
    // ==========================================================================
    // 🔥 객체 관리 서비스
    // ==========================================================================
    
    /**
     * @brief 객체 생성
     */
    bool CreateObject(uint32_t device_id,
                     BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance,
                     const std::map<BACNET_PROPERTY_ID, Structs::DataValue>& initial_properties = {});
    
    /**
     * @brief 객체 삭제
     */
    bool DeleteObject(uint32_t device_id,
                     BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance);
    
    /**
     * @brief 디바이스 객체 목록 조회
     */
    std::vector<DataPoint> GetDeviceObjects(uint32_t device_id,
                                                  BACNET_OBJECT_TYPE filter_type = OBJECT_PROPRIETARY_MIN);
    
    // ==========================================================================
    // 🔥 디바이스 발견 최적화
    // ==========================================================================
    
    /**
     * @brief 향상된 디바이스 발견
     */
    int DiscoverDevices(std::map<uint32_t, DeviceInfo>& devices,
                       uint32_t low_limit = 0,
                       uint32_t high_limit = 4194303,
                       uint32_t timeout_ms = 3000);
    
    /**
     * @brief 특정 디바이스 정보 조회
     */
    bool GetDeviceInfo(uint32_t device_id, DeviceInfo& device_info);
    
    /**
     * @brief 디바이스 온라인 상태 확인
     */
    bool PingDevice(uint32_t device_id, uint32_t timeout_ms = 1000);
    
    // ==========================================================================
    // 서비스 상태 및 통계
    // ==========================================================================
    
    /**
     * @brief 서비스 성능 통계
     */
    struct ServiceStatistics {
        std::atomic<uint64_t> rpm_requests{0};
        std::atomic<uint64_t> rpm_success{0};
        std::atomic<uint64_t> wpm_requests{0};
        std::atomic<uint64_t> wpm_success{0};
        std::atomic<uint64_t> object_creates{0};
        std::atomic<uint64_t> object_deletes{0};
        std::atomic<double> avg_rpm_time_ms{0.0};
        std::atomic<double> avg_wpm_time_ms{0.0};
        std::atomic<uint32_t> max_objects_per_rpm{0};
        
        void Reset();
    };
    
    const ServiceStatistics& GetServiceStatistics() const { return service_stats_; }
    void ResetServiceStatistics();
    
private:
    // ==========================================================================
    // 내부 구조체들
    // ==========================================================================
    
    struct PendingRequest {
        uint8_t invoke_id;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point timeout_time;
        std::string service_type;
        std::promise<bool> promise;
        std::vector<TimestampedValue>* results_ptr = nullptr;
        
        PendingRequest(uint8_t id, const std::string& type, uint32_t timeout_ms);
    };
    
    struct RPMGroup {
        std::vector<DataPoint> objects;
        std::vector<size_t> original_indices; // 원본 배열에서의 인덱스
        size_t estimated_size_bytes;
        
        RPMGroup();
    };
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    BACnetDriver* driver_;                    // 부모 드라이버
    ServiceStatistics service_stats_;         // 서비스 통계
    
    // 요청 관리
    std::mutex requests_mutex_;
    std::map<uint8_t, std::unique_ptr<PendingRequest>> pending_requests_;
    uint8_t next_invoke_id_{1};
    
    // 성능 최적화
    std::mutex optimization_mutex_;
    std::map<uint32_t, DeviceInfo> device_cache_;
    std::chrono::steady_clock::time_point last_cache_cleanup_;
    
    // RPM 최적화 설정
    static constexpr size_t MAX_RPM_OBJECTS = 50;        // RPM당 최대 객체 수
    static constexpr size_t MAX_APDU_SIZE = 1476;        // 최대 APDU 크기
    static constexpr size_t ESTIMATED_OVERHEAD = 20;     // 객체당 예상 오버헤드
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    // 요청 관리
    uint8_t GetNextInvokeId();
    void RegisterRequest(std::unique_ptr<PendingRequest> request);
    bool CompleteRequest(uint8_t invoke_id, bool success);
    std::shared_future<bool> GetRequestFuture(uint8_t invoke_id);
    void TimeoutRequests();
    
    // RPM 최적화
    std::vector<RPMGroup> OptimizeRPMGroups(const std::vector<DataPoint>& objects);
    size_t EstimateObjectSize(const DataPoint& object);
    bool CanGroupObjects(const DataPoint& obj1, const DataPoint& obj2);
    
    // WPM 최적화
    std::vector<std::map<DataPoint, Structs::DataValue>> 
        OptimizeWPMGroups(const std::map<DataPoint, Structs::DataValue>& values);
    
    // 데이터 변환
    DataPoint DataPointToBACnetObject(const Structs::DataPoint& point);
    TimestampedValue BACnetValueToTimestampedValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    bool DataValueToBACnetValue(const Structs::DataValue& data_value, 
                               BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    // 에러 처리
    void LogServiceError(const std::string& service_name, const std::string& error_message);
    void UpdateServiceStatistics(const std::string& service_type, bool success, double duration_ms);
    
    // 캐시 관리
    void UpdateDeviceCache(uint32_t device_id, const DeviceInfo& device_info);
    bool GetCachedDeviceInfo(uint32_t device_id, DeviceInfo& device_info);
    void CleanupDeviceCache();
    
#ifdef HAS_BACNET_STACK
    // BACnet 스택 헬퍼들
    bool SendRPMRequest(uint32_t device_id, const std::vector<DataPoint>& objects, uint8_t invoke_id);
    bool SendWPMRequest(uint32_t device_id, const std::map<DataPoint, Structs::DataValue>& values, uint8_t invoke_id);
    bool ParseRPMResponse(const uint8_t* service_data, uint16_t service_len, 
                         const std::vector<DataPoint>& expected_objects,
                         std::vector<TimestampedValue>& results);
    bool ParseWPMResponse(const uint8_t* service_data, uint16_t service_len);
    
    // 주소 관리
    bool GetDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address);
    void CacheDeviceAddress(uint32_t device_id, const BACNET_ADDRESS& address);
#endif
    
    // 친구 클래스
    friend class BACnetDriver;
};

// =============================================================================
// 🔥 인라인 구현들 (성능 최적화)
// =============================================================================

inline BACnetServiceManager::PendingRequest::PendingRequest(uint8_t id, const std::string& type, uint32_t timeout_ms)
    : invoke_id(id)
    , start_time(std::chrono::steady_clock::now())
    , timeout_time(start_time + std::chrono::milliseconds(timeout_ms))
    , service_type(type) {
}

inline BACnetServiceManager::RPMGroup::RPMGroup() 
    : estimated_size_bytes(0) {
    objects.reserve(MAX_RPM_OBJECTS);
    original_indices.reserve(MAX_RPM_OBJECTS);
}

inline void BACnetServiceManager::ServiceStatistics::Reset() {
    rpm_requests = 0;
    rpm_success = 0;
    wpm_requests = 0;
    wpm_success = 0;
    object_creates = 0;
    object_deletes = 0;
    avg_rpm_time_ms = 0.0;
    avg_wpm_time_ms = 0.0;
    max_objects_per_rpm = 0;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_SERVICE_MANAGER_H