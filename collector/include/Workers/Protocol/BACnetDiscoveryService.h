// =============================================================================
// include/Workers/Protocol/BACnetDiscoveryService.h 
// 누락된 메서드 선언 추가
// =============================================================================

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H


// 전방 선언
namespace PulseOne {
namespace Workers {
    class BACnetWorker;
}
}
//#include "Workers/Protocol/BACnetWorker.h"
#include "Database/Repositories/DeviceRepository.h" 
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Common/UnifiedCommonTypes.h"  // 🔥 수정: Structs.h 대신
#include <memory>
#include <mutex>
#include <chrono>

namespace PulseOne {
namespace Workers {

class BACnetDiscoveryService {
public:
    // =======================================================================
    // 통계 구조체
    // =======================================================================
    
    struct Statistics {
        size_t devices_processed = 0;
        size_t devices_saved = 0;
        size_t objects_processed = 0;
        size_t objects_saved = 0;
        size_t values_processed = 0;
        size_t values_saved = 0;
        size_t database_errors = 0;
        std::chrono::system_clock::time_point last_activity;
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    explicit BACnetDiscoveryService(
        std::shared_ptr<Database::Repositories::DeviceRepository> device_repo,
        std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo,
        std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo = nullptr
    );
    
    ~BACnetDiscoveryService();

    // =======================================================================
    // 공개 메서드들
    // =======================================================================
    
    bool RegisterToWorker(std::shared_ptr<BACnetWorker> worker);
    void UnregisterFromWorker();
    
    Statistics GetStatistics() const;
    bool IsActive() const;
    void ResetStatistics();

    // =======================================================================
    // 콜백 핸들러들 (public으로 변경)
    // =======================================================================
    
    void OnDeviceDiscovered(const Drivers::BACnetDeviceInfo& device);
    void OnObjectDiscovered(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    void OnValueChanged(const std::string& object_id, const TimestampedValue& value);

private:
    // =======================================================================
    // 데이터베이스 저장 메서드들
    // =======================================================================
    
    bool SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device);
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value);

    // =======================================================================
    // 🔥 추가: 누락된 헬퍼 메서드 선언들
    // =======================================================================
    
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    std::string GenerateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object);
    std::string ObjectTypeToString(int type);
    PulseOne::Enums::DataType DetermineDataType(int tag);  // 🔥 수정: 올바른 반환 타입
    std::string DataTypeToString(PulseOne::Enums::DataType type);  // 🔥 추가: 새 헬퍼 함수
    std::string ConvertDataValueToString(const PulseOne::Structs::DataValue& value);
    void HandleError(const std::string& context, const std::string& error);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // Repository 참조들
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repository_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repository_;
    
    // Worker 참조 (weak_ptr로 순환 참조 방지)
    std::weak_ptr<BACnetWorker> registered_worker_;
    
    // 상태 관리
    std::atomic<bool> is_active_;
    
    // 통계 (스레드 안전)
    mutable std::mutex stats_mutex_;
    Statistics statistics_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DISCOVERY_SERVICE_H