// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 기존 BACnetDriver와 완전 호환되는 향상된 드라이버
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include "Drivers/Bacnet/BACnetServiceManager.h"
#include "Drivers/Bacnet/BACnetCOVManager.h"
#include "Drivers/Bacnet/BACnetObjectMapper.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/UnifiedCommonTypes.h"

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/config.h>
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/bactext.h>
    #include <bacnet/bacapp.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacaddr.h>
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    #include <bacnet/datalink/datalink.h>
    #include <bacnet/datalink/bip.h>
    #include <bacnet/basic/client/bac-rw.h>
    #include <bacnet/wp.h>
    #include <bacnet/basic/client/bac-discover.h>
    #include <bacnet/basic/client/bac-rw.h>
}
#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief 기존 호환성을 유지하는 향상된 BACnet 드라이버
 * 
 * 기존 BACnetWorker와 완전 호환되면서 새로운 고급 기능 추가:
 * - 모듈형 구조로 클래스 분리
 * - 통합된 통계 시스템
 * - 고급 BACnet 서비스
 * - COV 및 객체 매핑 지원
 */
class BACnetDriver : public IProtocolDriver {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 방지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;
    
    // ==========================================================================
    // IProtocolDriver 기본 인터페이스 (기존 호환성)
    // ==========================================================================
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
                   
    bool WriteValue(const Structs::DataPoint& point,
                   const Structs::DataValue& value) override;
                   
    Enums::ProtocolType GetProtocolType() const override {
        return Enums::ProtocolType::BACNET_IP;
    }
    
    Structs::DriverStatus GetStatus() const override {
        return status_.load();
    }
    
    Structs::ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    // ==========================================================================
    // 🔥 새로운 고급 기능들 (기존 API 확장)
    // ==========================================================================
    
    /**
     * @brief 향상된 디바이스 발견 (기존 API 확장)
     */
    int DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& devices, 
                       int timeout_ms = 3000);
    
    /**
     * @brief BACnet 객체 목록 조회
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief COV 구독 관리
     */
    bool SubscribeToCOV(uint32_t device_id, uint32_t object_instance, 
                       BACNET_OBJECT_TYPE object_type, uint32_t lifetime_seconds = 300);
    bool UnsubscribeFromCOV(uint32_t device_id, uint32_t object_instance, 
                           BACNET_OBJECT_TYPE object_type);
    
    /**
     * @brief 고급 BACnet 서비스들
     */
    bool ReadPropertyMultiple(uint32_t device_id, 
                             const std::vector<BACnetObjectInfo>& objects,
                             std::vector<TimestampedValue>& results);
                             
    bool WritePropertyMultiple(uint32_t device_id,
                              const std::map<BACnetObjectInfo, Structs::DataValue>& values);
    
    /**
     * @brief 복잡한 객체 매핑
     */
    bool MapComplexObject(const std::string& object_identifier,
                         uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                         uint32_t object_instance,
                         const std::vector<BACNET_PROPERTY_ID>& properties = {});
    
    /**
     * @brief BACnet 특화 통계 정보
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    std::string GetDiagnosticInfo() const override;
    bool HealthCheck() override;

protected:
    // ==========================================================================
    // IProtocolDriver 보호된 메서드
    // ==========================================================================
    bool DoStart() override;
    bool DoStop() override;

private:
    // ==========================================================================
    // 🔥 모듈화된 컴포넌트들
    // ==========================================================================
    
    // 에러 및 통계 관리
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    std::unique_ptr<BACnetStatisticsManager> statistics_manager_;
    
    // 고급 서비스 관리자들
    std::unique_ptr<BACnetServiceManager> service_manager_;
    std::unique_ptr<BACnetCOVManager> cov_manager_;
    std::unique_ptr<BACnetObjectMapper> object_mapper_;
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> status_{Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_stop_{false};
    
    // BACnet 네트워크 설정
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    
    // 스레드 관리
    std::thread network_thread_;
    std::mutex network_mutex_;
    std::condition_variable network_condition_;
    
    // 에러 상태 (기존 호환성)
    mutable std::mutex error_mutex_;
    mutable Structs::ErrorInfo last_error_;
    mutable DriverStatistics standard_statistics_cache_;
    
    // ==========================================================================
    // 핵심 내부 메서드들
    // ==========================================================================
    
    // BACnet 스택 관리
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    bool IsBACnetStackInitialized() const;
    
    // 네트워크 관리
    void NetworkThreadFunction();
    bool ProcessBACnetMessages();
    bool TestNetworkConnectivity();
    
    // 에러 처리 (기존 API 유지)
    void SetError(Enums::ErrorCode code, const std::string& message);
    void LogBACnetError(const std::string& message, 
                       uint8_t error_class = 0, uint8_t error_code = 0);
    
    // 유틸리티
    std::string CreateObjectKey(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                               uint32_t object_instance);
    
#ifdef HAS_BACNET_STACK
    // BACnet Stack 콜백 핸들러들 (정적)
    static void IAmHandler(uint8_t* service_request, uint16_t service_len, 
                          BACNET_ADDRESS* src);
    static void ReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void ErrorHandler(BACNET_ADDRESS* src, uint8_t invoke_id, 
                            BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    static void RejectHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason);
    static void AbortHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t abort_reason);
    static void COVNotificationHandler(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_DATA* service_data);
#endif
    
    // 정적 인스턴스 관리
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;
    
    // 친구 클래스 (모듈화된 컴포넌트들이 private 멤버 접근 가능)
    friend class BACnetServiceManager;
    friend class BACnetCOVManager;
    friend class BACnetObjectMapper;
    friend class BACnetStatisticsManager;
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H