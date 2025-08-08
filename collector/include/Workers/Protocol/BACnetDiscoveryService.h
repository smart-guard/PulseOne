/**
 * @file BACnetDiscoveryService.h 
 * @brief BACnet 발견 서비스 헤더 - 🔥 모든 문제 완전 해결
 * @author PulseOne Development Team
 * @date 2025-08-08
 */

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H

#include "Database/Repositories/DeviceRepository.h" 
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/DatabaseTypes.h"    
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Bacnet/BACnetTypes.h"  // 🔥 추가: BACNET_ADDRESS 정의
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>

// 🔥 전방 선언만 (include 하지 않음)
namespace PulseOne {
namespace Workers {
    class BACnetWorker;
}
}

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
    // 🔥 콜백 핸들러들 - 올바른 타입 사용
    // =======================================================================
    
    void OnDeviceDiscovered(const PulseOne::Structs::DeviceInfo& device);
    void OnObjectDiscovered(const PulseOne::Structs::DataPoint& object);
    void OnObjectDiscovered(uint32_t device_id, const std::vector<PulseOne::Structs::DataPoint>& objects);
    void OnValueChanged(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

private:
    // =======================================================================
    // 데이터베이스 저장 메서드들
    // =======================================================================
    
    bool SaveDiscoveredDeviceToDatabase(const PulseOne::Structs::DeviceInfo& device);
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<PulseOne::Structs::DataPoint>& objects);
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

    // =======================================================================
    // 유틸리티 함수들
    // =======================================================================
    
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    std::string GenerateDataPointId(uint32_t device_id, const PulseOne::Structs::DataPoint& object);
    std::string ObjectTypeToString(int type);
    PulseOne::Enums::DataType DetermineDataType(int type);
    std::string DataTypeToString(PulseOne::Enums::DataType type);
    std::string ConvertDataValueToString(const PulseOne::Structs::DataValue& value);
    double ConvertDataValueToDouble(const PulseOne::Structs::DataValue& value);
    void HandleError(const std::string& context, const std::string& error);
    int GuessObjectTypeFromName(const std::string& object_name);
    std::string BACnetAddressToString(const BACNET_ADDRESS& address);

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