//=============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// BACnet 프로토콜 드라이버 헤더 - inline 함수 중복 제거
//=============================================================================

#ifndef PULSEONE_DRIVERS_BACNET_DRIVER_H
#define PULSEONE_DRIVERS_BACNET_DRIVER_H

// =============================================================================
// 필수 헤더 포함
// =============================================================================
#include "Drivers/Common/IProtocolDriver.h"
#include "Common/BasicTypes.h"           // UUID, Timestamp 등
#include "Common/Enums.h"                // ProtocolType, ConnectionStatus 등  
#include "Common/Structs.h"              // DeviceInfo, DataPoint 등
#include "Common/DriverStatistics.h"     // DriverStatistics
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Utils/LogManager.h"

#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <future>
#include <chrono>

// BACnet 스택 조건부 포함
#ifdef HAS_BACNET_STACK
extern "C" {
    // 핵심 BACnet 헤더들 (실제 확인된 경로)
    #include <bacnet/bacdef.h>
    #include <bacnet/config.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacapp.h>
    
    // 네트워크 및 프로토콜 레이어
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    
    // 서비스 관련 (실제 확인된 파일들)
    #include <bacnet/rp.h>                    // ReadProperty
    #include <bacnet/wp.h>                    // WriteProperty
    #include <bacnet/rpm.h>                   // ReadPropertyMultiple  
    #include <bacnet/wpm.h>                   // WritePropertyMultiple
    #include <bacnet/cov.h>                   // Change of Value
    #include <bacnet/iam.h>                   // I-Am
    #include <bacnet/whois.h>                 // Who-Is
    
    // 데이터링크 레이어 (계층적 구조 - 실제 경로!)
    #include <bacnet/datalink/bip.h>          // BACnet/IP
    
    // 기본 서비스 (계층적 구조 - 실제 경로!)
    #include <bacnet/basic/tsm/tsm.h>         // Transaction State Machine
    #include <bacnet/basic/binding/address.h> // Address binding
    #include <bacnet/basic/object/device.h>   // Device object
}
#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 프로토콜 드라이버
 * @details 표준 DriverStatistics 사용, IProtocolDriver 완전 구현
 */
class BACnetDriver : public IProtocolDriver {
private:
    // 싱글턴 패턴
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 금지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;

    // =============================================================================
    // IProtocolDriver 인터페이스 구현 (모든 함수 선언만)
    // =============================================================================
    
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool Start() override;
    bool Stop() override;
    
    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points,
                   std::vector<PulseOne::Structs::TimestampedValue>& values) override;
    
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
                   const PulseOne::Structs::DataValue& value) override;
    
    PulseOne::Enums::ProtocolType GetProtocolType() const override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override;
    
    const PulseOne::Structs::DriverStatistics& GetStatistics() const override;
    void SetError(PulseOne::Enums::ErrorCode code, const std::string& message);
    
    // =============================================================================
    // BACnet 특화 공개 메서드들
    // =============================================================================
    
    /**
     * @brief 싱글톤 인스턴스 반환
     */
    static BACnetDriver& GetInstance();
    
    /**
     * @brief BACnet 디바이스 검색
     */
    std::vector<DeviceInfo> DiscoverDevices(uint32_t timeout_ms = 5000);
    
    /**
     * @brief COV (Change of Value) 구독
     */
    bool SubscribeCOV(uint32_t device_id, BACnetObjectType object_type, 
                     uint32_t object_instance, uint32_t property_id);
    
    /**
     * @brief COV 구독 해제
     */
    bool UnsubscribeCOV(uint32_t device_id, BACnetObjectType object_type, 
                       uint32_t object_instance, uint32_t property_id);

private:
    // =============================================================================
    // 상태 관리 멤버 변수들 (선언 순서 수정)
    // =============================================================================
    mutable PulseOne::Structs::DriverStatistics driver_statistics_;
    std::atomic<PulseOne::Structs::DriverStatus> status_;
    std::atomic<bool> is_connected_;                    // ✅ 위치 수정
    std::atomic<bool> should_stop_;
    mutable std::mutex error_mutex_;
    PulseOne::Structs::ErrorInfo last_error_;
    
    // =============================================================================
    // BACnet 특화 설정들
    // =============================================================================
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    uint32_t max_apdu_length_;
    bool segmentation_support_;
    
    // =============================================================================
    // 네트워킹 관련
    // =============================================================================
    int socket_fd_;                                     // ✅ 위치 수정
    std::thread network_thread_;
    
    // =============================================================================
    // BACnet 특화 비공개 메서드들 (선언만)
    // =============================================================================
    
    /**
     * @brief 드라이버 설정 파싱
     */
    void ParseDriverConfig(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief BACnet 스택 초기화
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 특화 통계 초기화
     */
    void InitializeBACnetStatistics();
    
    /**
     * @brief UDP 소켓 생성
     */
    bool CreateSocket();
    
    /**
     * @brief 소켓 해제
     */
    void CloseSocket();
    
    /**
     * @brief 단일 속성 읽기
     */
    bool ReadSingleProperty(const PulseOne::Structs::DataPoint& point, 
                           PulseOne::Structs::TimestampedValue& value);
    
    /**
     * @brief 단일 속성 쓰기
     */
    bool WriteSingleProperty(const PulseOne::Structs::DataPoint& point, 
                           const PulseOne::Structs::DataValue& value);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_BACNET_DRIVER_H