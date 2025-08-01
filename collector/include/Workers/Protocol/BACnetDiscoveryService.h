/**
 * @file BACnetDiscoveryService.h
 * @brief BACnet 발견 서비스 헤더 - BACnet 헤더 의존성 제거
 * @author PulseOne Development Team
 * @date 2025-08-01
 */

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H

#include "Workers/Protocol/BACnetWorker.h"
#include "Database/Repositories/DeviceRepository.h" 
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Common/Structs.h"
#include <memory>
#include <mutex>
#include <chrono>

namespace PulseOne {
namespace Workers {

/**
 * @brief BACnet 디바이스/객체 발견을 데이터베이스에 저장하는 서비스
 * 
 * 🎯 핵심 기능:
 * - BACnetWorker의 콜백을 등록하여 발견 이벤트 수신
 * - 발견된 디바이스를 devices 테이블에 저장
 * - 발견된 객체를 data_points 테이블에 저장  
 * - 값 변경을 current_values 테이블에 저장
 */
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
    // 서비스 제어
    // =======================================================================
    
    /**
     * @brief BACnetWorker에 콜백 등록
     */
    bool RegisterToWorker(std::shared_ptr<BACnetWorker> worker);
    
    /**
     * @brief BACnetWorker에서 콜백 해제
     */
    void UnregisterFromWorker();
    
    /**
     * @brief 서비스 활성 상태 확인
     */
    bool IsActive() const;

    // =======================================================================
    // 통계 조회
    // =======================================================================
    
    Statistics GetStatistics() const;
    void ResetStatistics();

private:
    // =======================================================================
    // 콜백 핸들러들 (BACnetWorker에서 호출됨)
    // =======================================================================
    
    void OnDeviceDiscovered(const Drivers::BACnetDeviceInfo& device);
    void OnObjectDiscovered(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    void OnValueChanged(const std::string& object_id, const TimestampedValue& value);

    // =======================================================================
    // 데이터베이스 저장 로직
    // =======================================================================
    
    bool SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device);
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value);

    // =======================================================================
    // 헬퍼 메서드들 - int 타입으로 간소화
    // =======================================================================
    
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    std::string GenerateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object);
    std::string ObjectTypeToString(int object_type);  // ✅ int로 변경
    std::string DetermineDataType(int tag);            // ✅ int로 변경
    std::string ConvertDataValueToString(const PulseOne::Structs::DataValue& value);
    void HandleError(const std::string& context, const std::string& error);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // Repository 인스턴스들
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repository_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repository_;
    
    // 등록된 워커 (약한 참조로 순환 참조 방지)
    std::weak_ptr<BACnetWorker> registered_worker_;
    
    // 서비스 상태
    bool is_active_;
    
    // 통계 (스레드 안전)
    mutable std::mutex stats_mutex_;
    Statistics statistics_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DISCOVERY_SERVICE_H