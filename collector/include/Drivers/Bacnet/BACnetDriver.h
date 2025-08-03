// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h (핵심 부분 수정)
// 컴파일 에러 해결: 인터페이스 시그니처 맞추기
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include "Common/UnifiedCommonTypes.h"
#include <memory>
#include <mutex>

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 프로토콜 드라이버
 */
class BACnetDriver : public IProtocolDriver {
public:
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // =============================================================================
    // 🔥 IProtocolDriver 인터페이스 구현 (시그니처 정확히 맞추기)
    // =============================================================================
    
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    // 🔥 수정: ReadData -> ReadValues로 시그니처 변경
    bool ReadValues(
        const std::vector<PulseOne::Structs::DataPoint>& points,
        std::vector<PulseOne::Structs::TimestampedValue>& values
    ) override;
    
    // 🔥 수정: WriteData -> WriteValue로 시그니처 변경  
    bool WriteValue(
        const PulseOne::Structs::DataPoint& point,
        const PulseOne::Structs::DataValue& value
    ) override;
    
    // 🔥 수정: WriteValues 시그니처를 IProtocolDriver와 일치시키기 (map 사용)
    bool WriteValues(const std::map<PulseOne::Structs::DataPoint, PulseOne::Structs::DataValue>& points_and_values) override;
    
    PulseOne::Enums::ProtocolType GetProtocolType() const override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    
    // 🔥 수정: ErrorInfo로 타입 변경 (IProtocolDriver 인터페이스와 일치)
    PulseOne::Structs::ErrorInfo GetLastError() const override;
    
    const PulseOne::Structs::DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    // 🔥 수정: DiscoverDevices 시그니처를 IProtocolDriver와 일치시키기
    std::vector<std::string> DiscoverDevices() override;
    
    // =============================================================================
    // BACnet 특화 메서드들
    // =============================================================================
    
    /**
     * @brief BACnet 특화 통계 반환
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    /**
     * @brief BACnet 디바이스 Discovery 실행
     */
    std::vector<BACnetDeviceInfo> DiscoverBACnetDevices(uint32_t timeout_ms = 5000);
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     */
    std::vector<BACnetObjectInfo> DiscoverDeviceObjects(uint32_t device_id);
    
    /**
     * @brief BACnet 설정 업데이트
     */
    bool UpdateBACnetConfig(const BACnetDriverConfig& config);
    
private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 🔥 수정: 올바른 타입 사용
    BACnetDriverConfig config_;
    
    // BACnet 스택 관련
    bool bacnet_initialized_;
    uint32_t local_device_id_;
    
    // 통계 관리
    std::unique_ptr<BACnetStatisticsManager> statistics_manager_;
    mutable PulseOne::Structs::DriverStatistics driver_statistics_;
    
    // 에러 정보
    // 🔥 수정: ErrorInfo로 타입 변경
    mutable PulseOne::Structs::ErrorInfo last_error_;
    
    // 동기화
    mutable std::mutex driver_mutex_;
    mutable std::mutex statistics_mutex_;
    
    // 상태
    PulseOne::Structs::DriverStatus current_status_;
    std::atomic<bool> is_connected_;
    
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    bool SetupLocalDevice();
    
    // 읽기/쓰기 헬퍼들
    bool ReadSingleProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                           uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                           PulseOne::Structs::DataValue& value);
    
    bool WriteSingleProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                            uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                            const PulseOne::Structs::DataValue& value);
    
    // Discovery 헬퍼들
    void SendWhoIsRequest();
    void ProcessIAmResponse(uint32_t device_id, const BACNET_ADDRESS& address);
    
    // 통계 업데이트 헬퍼들
    void UpdateReadStatistics(bool success, std::chrono::milliseconds duration);
    void UpdateWriteStatistics(bool success, std::chrono::milliseconds duration);
    void UpdateErrorStatistics(BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    
    // 유틸리티 메서드들
    std::string BACnetValueToString(const PulseOne::Structs::DataValue& value);
    PulseOne::Structs::DataValue ParseBACnetValue(const std::string& str, BACNET_OBJECT_TYPE type);
    bool IsValidBACnetAddress(const std::string& address) const;
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H