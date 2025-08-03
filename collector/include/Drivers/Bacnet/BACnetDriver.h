// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 완전 수정된 BACnet 드라이버 헤더 - 모든 컴파일 에러 해결
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/DriverStatistics.h"            // ✅ 표준 통계 구조 사용
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include "Common/DriverError.h"
#include "Utils/LogManager.h"

// ✅ BACnet 스택 조건부 인클루드 (깔끔한 구조)
#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>                // 기본 정의
    #include <bacnet/bacenum.h>               // 열거형들
    #include <bacnet/rp.h>                    // ReadProperty
    #include <bacnet/wp.h>                    // WriteProperty
    #include <bacnet/rpm.h>                   // ReadPropertyMultiple  
    #include <bacnet/wpm.h>                   // WritePropertyMultiple
    #include <bacnet/cov.h>                   // Change of Value
    #include <bacnet/iam.h>                   // I-Am
    #include <bacnet/whois.h>                 // Who-Is
    #include <bacnet/ihave.h>                 // I-Have
    #include <bacnet/whohas.h>                // Who-Has
    
    // 데이터링크 레이어 (계층적 구조)
    #include <bacnet/datalink/bip.h>          // BACnet/IP (실제 경로)
    
    // 기본 서비스 (계층적 구조)
    #include <bacnet/basic/tsm/tsm.h>         // Transaction State Machine
    #include <bacnet/basic/binding/address.h> // Address binding
    #include <bacnet/basic/object/device.h>   // Device object
    
    // 파일 서비스 (선택적)
    #ifdef BACNET_USE_FILE_SERVICES
    #include <bacnet/arf.h>                   // AtomicReadFile
    #include <bacnet/awf.h>                   // AtomicWriteFile
    #endif
    
    // 추가 유틸리티
    #include <bacnet/datetime.h>              // Date/Time handling
    #include <bacnet/reject.h>                // Reject handling
}
#else
    // 시뮬레이션을 위한 더미 정의들
    typedef enum {
        OBJECT_DEVICE = 8,
        OBJECT_ANALOG_INPUT = 0,
        OBJECT_ANALOG_OUTPUT = 1,
        OBJECT_ANALOG_VALUE = 2,
        OBJECT_BINARY_INPUT = 3,
        OBJECT_BINARY_OUTPUT = 4,
        OBJECT_BINARY_VALUE = 5,
        OBJECT_MULTI_STATE_INPUT = 13,
        OBJECT_MULTI_STATE_OUTPUT = 14,
        OBJECT_MULTI_STATE_VALUE = 19
    } BACNET_OBJECT_TYPE;
    
    typedef enum {
        PROP_PRESENT_VALUE = 85,
        PROP_OBJECT_NAME = 77,
        PROP_OBJECT_TYPE = 79,
        PROP_SYSTEM_STATUS = 112,
        PROP_VENDOR_NAME = 121,
        PROP_VENDOR_IDENTIFIER = 120,
        PROP_MODEL_NAME = 70,
        PROP_FIRMWARE_REVISION = 44,
        PROP_PROTOCOL_VERSION = 98,
        PROP_PROTOCOL_REVISION = 139
    } BACNET_PROPERTY_ID;
    
    typedef struct {
        uint8_t mac[6];
        uint16_t net;
        uint8_t mac_len;
    } BACNET_ADDRESS;
    
    #define BACNET_MAX_INSTANCE 0x3FFFFF
    #define BACNET_ARRAY_ALL 0xFFFFFFFF
    #define MAX_MPDU 1476
#endif

#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <condition_variable>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 전방 선언
// =============================================================================
class BACnetWorker;
class BACnetErrorMapper;

// ✅ BACnet 구조체들은 BACnetCommonTypes.h에서 정의됨
// 여기서는 using 선언만 사용
using BACnetDeviceInfo = PulseOne::Drivers::BACnetDeviceInfo;
using BACnetObjectInfo = PulseOne::Drivers::BACnetObjectInfo;
using BACnetReadRequest = PulseOne::Drivers::BACnetReadRequest;
using BACnetWriteRequest = PulseOne::Drivers::BACnetWriteRequest;

// =============================================================================
// BACnet 드라이버 클래스
// =============================================================================

/**
 * @brief BACnet 프로토콜 드라이버 (표준화된 통계 구조 사용)
 * @details GPL with exception 라이선스의 BACnet Stack 사용
 *          모든 표준 BACnet 서비스 지원 (ReadProperty, WriteProperty, COV 등)
 */
class BACnetDriver : public IProtocolDriver {
private:
    // 싱글턴 패턴 (BACnet 스택 전역 상태 관리용)
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 생성자 삭제 (싱글턴 패턴)
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;

    // =============================================================================
    // IProtocolDriver 인터페이스 구현 (완전한 시그니처)
    // =============================================================================
    
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points,
                   std::vector<PulseOne::Structs::TimestampedValue>& values) override;
    
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
                   const PulseOne::Structs::DataValue& value) override;
    
    // ✅ 누락된 필수 메서드들 추가
    bool Start() override;
    bool Stop() override;
    
    PulseOne::Enums::ProtocolType GetProtocolType() const override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override;
    
    // ✅ SetError 메서드 (override 제거 - IProtocolDriver에 없음)
    void SetError(PulseOne::Enums::ErrorCode code, const std::string& message);
    
    // ✅ 통계 관련 (표준 DriverStatistics 사용)
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;

    // =============================================================================
    // BACnet 특화 메서드들
    // =============================================================================
    
    /**
     * @brief BACnet 디바이스 발견 (Who-Is 브로드캐스트)
     */
    bool DiscoverDevices(uint32_t low_limit = 0, uint32_t high_limit = BACNET_MAX_INSTANCE);
    
    /**
     * @brief 특정 디바이스의 객체 목록 읽기
     */
    bool ReadObjectList(uint32_t device_id, std::vector<BACnetObjectInfo>& objects);
    
    /**
     * @brief 단일 속성 읽기
     */
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     PulseOne::Structs::DataValue& value);
    
    /**
     * @brief 단일 속성 쓰기
     */
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      const PulseOne::Structs::DataValue& value, uint8_t priority = 16);
    
    /**
     * @brief COV 구독
     */
    bool SubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, uint32_t lifetime_seconds = 3600);
    
    /**
     * @brief COV 구독 해제
     */
    bool UnsubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                       uint32_t object_instance);

    // =============================================================================
    // 콜백 및 이벤트 핸들링
    // =============================================================================
    
    /**
     * @brief 디바이스 발견 콜백 등록
     */
    void SetDeviceDiscoveredCallback(std::function<void(const BACnetDeviceInfo&)> callback);
    
    /**
     * @brief COV 알림 콜백 등록
     */
    void SetCOVNotificationCallback(std::function<void(const std::string&, const PulseOne::Structs::TimestampedValue&)> callback);

    // =============================================================================
    // 상태 조회 및 설정
    // =============================================================================
    
    /**
     * @brief 로컬 디바이스 ID 설정
     */
    void SetLocalDeviceId(uint32_t device_id) { local_device_id_ = device_id; }
    
    /**
     * @brief 로컬 디바이스 ID 조회
     */
    uint32_t GetLocalDeviceId() const { return local_device_id_; }
    
    /**
     * @brief 발견된 디바이스 목록 조회
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 싱글턴 인스턴스 조회 (BACnet 스택 전역 상태용)
     */
    static BACnetDriver* GetInstance();

protected:
    // =============================================================================
    // 내부 구현 메서드들
    // =============================================================================
    
    /**
     * @brief 설정 파싱
     */
    void ParseDriverConfig(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief BACnet 스택 초기화
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 스택 정리
     */
    void CleanupBACnetStack();
    
    /**
     * @brief 네트워크 처리 스레드
     */
    void NetworkProcessingThread();
    
    /**
     * @brief BACnet 특화 통계 초기화
     */
    void InitializeBACnetStatistics();
    
    /**
     * @brief 에러 처리
     */
    void HandleBACnetError(uint8_t error_class, uint8_t error_code, const std::string& context);

    // =============================================================================
    // 멤버 변수들
    // =============================================================================

private:
    // ✅ 표준 통계 구조 (DriverStatistics 사용)
    mutable DriverStatistics driver_statistics_;
    
    // 기본 설정
    PulseOne::Structs::DriverConfig config_;
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    uint16_t max_apdu_length_;
    bool segmentation_support_;
    
    // 상태 관리
    std::atomic<PulseOne::Structs::DriverStatus> status_{PulseOne::Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> should_stop_{false};
    PulseOne::Structs::ErrorInfo last_error_;
    
    // 네트워크
    int socket_fd_;
    std::thread network_thread_;
    std::mutex network_mutex_;
    
    // 발견된 디바이스들
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    mutable std::mutex devices_mutex_;
    
    // 헬퍼 클래스들
    std::unique_ptr<BACnetWorker> worker_;
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    
    // 콜백들
    std::function<void(const BACnetDeviceInfo&)> device_discovered_callback_;
    std::function<void(const std::string&, const PulseOne::Structs::TimestampedValue&)> cov_notification_callback_;
    std::mutex callback_mutex_;
};

// =============================================================================
// 인라인 메서드 구현
// =============================================================================

inline PulseOne::Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return PulseOne::Enums::ProtocolType::BACNET_IP;  // ✅ 수정: BACNET → BACNET_IP
}

inline PulseOne::Structs::DriverStatus BACnetDriver::GetStatus() const {
    return status_.load();
}

inline PulseOne::Structs::ErrorInfo BACnetDriver::GetLastError() const {
    return last_error_;
}

inline BACnetDriver* BACnetDriver::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    return instance_;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H