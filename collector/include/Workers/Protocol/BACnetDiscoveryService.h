// =============================================================================
// include/Workers/Protocol/BACnetDiscoveryService.h 
// 🔥 누락된 메서드 선언 완전 추가
// =============================================================================

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H

// 전방 선언
namespace PulseOne {
namespace Workers {
    class BACnetWorker;
}
}

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Database/Repositories/DeviceRepository.h" 
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/DatabaseTypes.h"    // 🔥 QueryCondition 포함
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
    void OnValueChanged(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

private:
    // =======================================================================
    // 데이터베이스 저장 메서드들
    // =======================================================================
    
    bool SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device);
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

    // =======================================================================
    // 🔥 누락된 유틸리티 함수 선언들 추가
    // =======================================================================
    
    /**
     * @brief 데이터베이스에서 BACnet 디바이스 ID로 실제 디바이스 ID 찾기
     */
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    
    /**
     * @brief 데이터포인트 ID 생성
     */
    std::string GenerateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object);
    
    /**
     * @brief 객체 타입을 문자열로 변환
     */
    std::string ObjectTypeToString(int type);
    
    /**
     * @brief BACnet 객체 타입으로부터 데이터 타입 결정
     */
    PulseOne::Enums::DataType DetermineDataType(int type);
    
    /**
     * @brief 데이터 타입을 문자열로 변환
     */
    std::string DataTypeToString(PulseOne::Enums::DataType type);
    
    /**
     * @brief 데이터 값을 문자열로 변환
     */
    std::string ConvertDataValueToString(const PulseOne::Structs::DataValue& value);
    
    /**
     * @brief BACnet 주소를 IP 문자열로 변환
     */
    std::string BACnetAddressToString(const BACNET_ADDRESS& address);
    
    /**
     * @brief DataValue를 double로 변환 (CurrentValueEntity용)
     */
    double ConvertDataValueToDouble(const PulseOne::Structs::DataValue& value);
    
    /**
     * @brief 에러 처리
     */
    void HandleError(const std::string& context, const std::string& error);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // Repository들
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repository_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repository_;
    
    // 워커 연결
    std::weak_ptr<BACnetWorker> registered_worker_;
    std::atomic<bool> is_active_;
    
    // 통계 및 동기화
    mutable Statistics statistics_;
    mutable std::mutex stats_mutex_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DISCOVERY_SERVICE_H