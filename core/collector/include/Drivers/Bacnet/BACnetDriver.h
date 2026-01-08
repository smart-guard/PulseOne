//=============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// BACnet 프로토콜 드라이버 헤더 - 독립객체 + Windows/Linux 완전 호환 (컴파일 에러 수정)
//=============================================================================

#ifndef PULSEONE_DRIVERS_BACNET_DRIVER_H
#define PULSEONE_DRIVERS_BACNET_DRIVER_H

// =============================================================================
// 필수 헤더 포함
// =============================================================================
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <future>
#include <chrono>

// Windows 매크로 충돌 방지
#ifdef min
#undef min
#endif
#ifdef max  
#undef max
#endif
#ifdef ERROR
#undef ERROR
#endif

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/BasicTypes.h"           // UniqueId, Timestamp 등
#include "Common/Enums.h"                // ProtocolType, ConnectionStatus 등  
#include "Common/Structs.h"              // DeviceInfo, DataPoint 등
#include "Common/DriverStatistics.h"     // DriverStatistics
#include "Drivers/Bacnet/BACnetTypes.h"
#include "Logging/LogManager.h"

// BACnet 스택 조건부 포함
#if HAS_BACNET_STACK
extern "C" {
    // 핵심 BACnet 헤더들
    #include <bacnet/bacdef.h>
    #include <bacnet/config.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacapp.h>
    
    // 네트워크 및 프로토콜 레이어
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    
    // 서비스 관련
    #include <bacnet/rp.h>                    // ReadProperty
    #include <bacnet/wp.h>                    // WriteProperty
    #include <bacnet/rpm.h>                   // ReadPropertyMultiple  
    #include <bacnet/wpm.h>                   // WritePropertyMultiple
    #include <bacnet/cov.h>                   // Change of Value
    #include <bacnet/iam.h>                   // I-Am
    #include <bacnet/whois.h>                 // Who-Is
    
    // 데이터링크 레이어
    #include <bacnet/datalink/bip.h>          // BACnet/IP
    
    // 기본 서비스
    #include <bacnet/basic/tsm/tsm.h>         // Transaction State Machine
    #include <bacnet/basic/binding/address.h> // Address binding
    #include <bacnet/basic/object/device.h>   // Device object
}

// 매크로 충돌 재방지
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef ERROR
#undef ERROR
#endif

#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 프로토콜 드라이버 (독립 객체)
 * @details 
 * - 싱글턴 제거, 독립 객체로 변경
 * - Windows/Linux 크로스 플랫폼 지원
 * - 표준 DriverStatistics 사용
 * - IProtocolDriver 완전 구현
 * - 멀티 디바이스 지원
 */
class BACnetDriver : public IProtocolDriver {
public:
    // =============================================================================
    // 생성자 및 소멸자 (일반 객체)
    // =============================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 금지 (리소스 관리)
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;

    // =============================================================================
    // IProtocolDriver 인터페이스 구현
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
    void ResetStatistics() override;
    
    void SetConnectedForTesting(bool connected) { is_connected_.store(connected); }
    
    // =============================================================================
    
    /**
     * @brief BACnet 디바이스 검색
     * @param timeout_ms 타임아웃 (기본: 5000ms)
     * @return 발견된 디바이스 목록
     */
    std::vector<PulseOne::Structs::DeviceInfo> DiscoverDevices(uint32_t timeout_ms = 5000);
    
    /**
     * @brief COV (Change of Value) 구독
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 프로퍼티 ID
     * @return 성공 여부
     */
    bool SubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id);
    
    /**
     * @brief COV 구독 해제
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 프로퍼티 ID
     * @return 성공 여부
     */
    bool UnsubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                       uint32_t object_instance, BACNET_PROPERTY_ID property_id);
    
    /**
     * @brief 에러 정보 설정
     * @param code 에러 코드
     * @param message 에러 메시지
     */
    void SetError(PulseOne::Enums::ErrorCode code, const std::string& message);

private:
    // =============================================================================
    // 상태 관리 멤버 변수들
    // =============================================================================
    mutable PulseOne::Structs::DriverStatistics driver_statistics_;
    mutable PulseOne::Structs::DriverConfig config_;
    std::atomic<PulseOne::Structs::DriverStatus> status_;
    std::atomic<bool> is_connected_;
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
    // 네트워킹 관련 (플랫폼별)
    // =============================================================================
    int socket_fd_;
    std::thread network_thread_;
    
    // =============================================================================
    // BACnet 특화 비공개 메서드들
    // =============================================================================
    
    /**
     * @brief 드라이버 설정 파싱
     * @param config 드라이버 설정
     */
    void ParseDriverConfig(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief BACnet 스택 초기화
     * @return 성공 여부
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 특화 통계 초기화
     */
    void InitializeBACnetStatistics();
    
    /**
     * @brief UDP 소켓 생성 (플랫폼별)
     * @return 성공 여부
     */
    bool CreateSocket();
    
    /**
     * @brief 소켓 해제 (플랫폼별)
     */
    void CloseSocket();
    
    /**
     * @brief 단일 속성 읽기
     * @param point 데이터 포인트
     * @param value 결과 값
     * @return 성공 여부
     */
    bool ReadSingleProperty(const PulseOne::Structs::DataPoint& point, 
                           PulseOne::Structs::TimestampedValue& value);
    
    /**
     * @brief 단일 속성 쓰기
     * @param point 데이터 포인트
     * @param value 쓸 값
     * @return 성공 여부
     */
    bool WriteSingleProperty(const PulseOne::Structs::DataPoint& point, 
                           const PulseOne::Structs::DataValue& value);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_BACNET_DRIVER_H