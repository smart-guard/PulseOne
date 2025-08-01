// =============================================================================
// BACnetDiscoveryService.h - 헤더 파일
// 파일 위치: collector/include/Workers/Protocol/BACnetDiscoveryService.h
// =============================================================================

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H

/**
 * @file BACnetDiscoveryService.h
 * @brief BACnet 디바이스/객체 발견을 데이터베이스에 저장하는 서비스
 * @author PulseOne Development Team
 * @date 2025-01-25
 * @version 1.0.0
 * 
 * @details
 * BACnetWorker의 콜백을 받아서 발견된 디바이스와 객체를 데이터베이스에 저장합니다.
 * - 순수 데이터베이스 저장 담당
 * - BACnetWorker와 느슨한 결합
 * - 에러 처리 및 복구 포함
 */

#include "Workers/Protocol/BACnetWorker.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include <memory>
#include <string>
#include <vector>

namespace PulseOne {
namespace Workers {  // Services → Workers로 변경

/**
 * @brief BACnet 발견 서비스 통계
 */
struct BACnetDiscoveryStats {
    uint64_t devices_processed = 0;
    uint64_t devices_saved = 0;
    uint64_t objects_processed = 0;
    uint64_t objects_saved = 0;
    uint64_t values_processed = 0;
    uint64_t values_saved = 0;
    uint64_t database_errors = 0;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_activity;
    
    BACnetDiscoveryStats() {
        start_time = std::chrono::system_clock::now();
        last_activity = start_time;
    }
};

/**
 * @class BACnetDiscoveryService
 * @brief BACnet 발견 데이터의 데이터베이스 저장 서비스
 * 
 * @details
 * BACnetWorker로부터 콜백을 통해 발견된 디바이스, 객체, 값 변경 정보를 받아서
 * 적절한 데이터베이스 테이블에 저장합니다.
 */
class BACnetDiscoveryService {
public:
    /**
     * @brief 생성자
     * @param device_repo 디바이스 리포지토리
     * @param datapoint_repo 데이터포인트 리포지토리
     * @param current_value_repo 현재값 리포지토리
     */
    explicit BACnetDiscoveryService(
        std::shared_ptr<Database::Repositories::DeviceRepository> device_repo,
        std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo,
        std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    ~BACnetDiscoveryService();
    
    // =============================================================================
    // BACnetWorker 연동
    // =============================================================================
    
    /**
     * @brief BACnetWorker와 연동하여 콜백 등록
     * @param worker BACnet 워커 인스턴스
     * @return 성공 시 true
     */
    bool RegisterWithWorker(std::shared_ptr<Workers::BACnetWorker> worker);
    
    /**
     * @brief 워커 연동 해제
     */
    void UnregisterFromWorker();
    
    // =============================================================================
    // 통계 및 상태
    // =============================================================================
    
    /**
     * @brief 서비스 통계 반환
     * @return 통계 정보
     */
    BACnetDiscoveryStats GetStatistics() const;
    
    /**
     * @brief 통계 리셋
     */
    void ResetStatistics();
    
    /**
     * @brief 서비스 활성화 상태
     * @return 활성화된 경우 true
     */
    bool IsActive() const;

private:
    // =============================================================================
    // 콜백 핸들러들
    // =============================================================================
    
    /**
     * @brief 디바이스 발견 콜백 핸들러
     * @param device 발견된 디바이스 정보
     */
    void OnDeviceDiscovered(const Drivers::BACnetDeviceInfo& device);
    
    /**
     * @brief 객체 발견 콜백 핸들러
     * @param device_id 디바이스 ID
     * @param objects 발견된 객체들
     */
    void OnObjectDiscovered(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    
    /**
     * @brief 값 변경 콜백 핸들러
     * @param object_id 객체 식별자
     * @param value 변경된 값
     */
    void OnValueChanged(const std::string& object_id, const PulseOne::TimestampedValue& value);
    
    // =============================================================================
    // 데이터베이스 저장 로직
    // =============================================================================
    
    /**
     * @brief 발견된 디바이스를 데이터베이스에 저장
     * @param device 디바이스 정보
     * @return 성공 시 true
     */
    bool SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device);
    
    /**
     * @brief 발견된 객체들을 데이터베이스에 저장
     * @param device_id 디바이스 ID
     * @param objects 객체들
     * @return 성공 시 true
     */
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects);
    
    /**
     * @brief 현재값을 데이터베이스에 업데이트
     * @param object_id 객체 식별자
     * @param value 값
     * @return 성공 시 true
     */
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const PulseOne::TimestampedValue& value);
    
    // =============================================================================
    // 헬퍼 메서드들
    // =============================================================================
    
    /**
     * @brief BACnet 디바이스 ID로 데이터베이스 디바이스 ID 찾기
     * @param bacnet_device_id BACnet 디바이스 ID
     * @return 데이터베이스 디바이스 ID (없으면 -1)
     */
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    
    /**
     * @brief 데이터포인트 ID 생성
     * @param device_id 디바이스 ID
     * @param object BACnet 객체 정보
     * @return 데이터포인트 ID
     */
    std::string CreateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object);
    
    /**
     * @brief BACnet 객체 타입을 문자열로 변환
     * @param type 객체 타입
     * @return 타입 문자열
     */
    std::string ObjectTypeToString(BACNET_OBJECT_TYPE type);
    
    /**
     * @brief 에러 처리 및 로깅
     * @param operation 작업 이름
     * @param error_message 에러 메시지
     */
    void HandleError(const std::string& operation, const std::string& error_message);
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    /// Repository들
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repository_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repository_;
    
    /// 연동된 워커
    std::weak_ptr<Workers::BACnetWorker> registered_worker_;
    
    /// 서비스 상태
    mutable std::mutex service_mutex_;
    bool is_active_;
    
    /// 통계
    mutable BACnetDiscoveryStats statistics_;
    mutable std::mutex stats_mutex_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DISCOVERY_SERVICE_H