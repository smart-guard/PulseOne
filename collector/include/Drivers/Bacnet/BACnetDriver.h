// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 완성된 BACnet 드라이버 헤더 - 표준 DriverStatistics 사용
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

// BACnet Stack 조건부 인클루드 (실제 설치된 SourceForge 버전)
#ifdef HAS_BACNET_STACK
extern "C" {
    // 핵심 BACnet 헤더들 (실제 확인된 경로)
    #include <bacnet/bacdef.h>
    #include <bacnet/config.h>
    #include <bacnet/bactext.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacapp.h>
    #include <bacnet/version.h>
    
    // 네트워크 및 프로토콜 레이어
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    
    // 서비스 관련 (SourceForge 실제 구조)
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

// =============================================================================
// BACnet 특화 구조체들
// =============================================================================

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id = 0;
    std::string device_name = "";
    std::string ip_address = "";
    uint16_t port = 47808;
    uint32_t vendor_id = 0;
    uint16_t max_apdu_length = 1476;
    bool segmentation_supported = true;
    uint8_t protocol_version = 1;
    uint8_t protocol_revision = 14;
    std::chrono::system_clock::time_point last_seen;
    bool is_online = false;
};

/**
 * @brief BACnet 읽기 요청
 */
struct BACnetReadRequest {
    uint32_t device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    int32_t array_index = BACNET_ARRAY_ALL;
    std::string point_id;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
};

/**
 * @brief BACnet 쓰기 요청
 */
struct BACnetWriteRequest {
    uint32_t device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    PulseOne::Structs::DataValue value;
    int32_t array_index = BACNET_ARRAY_ALL;
    uint8_t priority = 16;
    std::string point_id;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
};

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
    // IProtocolDriver 인터페이스 구현
    // =============================================================================
    
    // 기본 라이프사이클
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Start() override;                                    // ✅ 새로 추가
    bool Stop() override;                                     // ✅ 새로 추가
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    // 데이터 읽기/쓰기
    bool ReadValues(
        const std::vector<PulseOne::Structs::DataPoint>& points,
        std::vector<PulseOne::Structs::TimestampedValue>& values
    ) override;
    
    bool WriteValue(
        const PulseOne::Structs::DataPoint& point,
        const PulseOne::Structs::DataValue& value
    ) override;
    
    bool WriteValues(
        const std::map<PulseOne::Structs::DataPoint, PulseOne::Structs::DataValue>& points_and_values
    ) override;
    
    // 상태 및 정보
    PulseOne::Enums::ProtocolType GetProtocolType() const override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    const PulseOne::Structs::DriverStatistics& GetStatistics() const override;  // ✅ 표준 구조 반환
    PulseOne::Structs::ErrorInfo GetLastError() const override;
    void SetError(PulseOne::Enums::ErrorCode code, const std::string& message) override;  // ✅ 새로 추가
    std::string GetDiagnosticInfo() const override;
    bool HealthCheck() override;

    // =============================================================================
    // BACnet 특화 기능들
    // =============================================================================
    
    /**
     * @brief 단일 BACnet 속성 읽기 (핵심 기능만)
     * @param device_id 대상 디바이스 ID
     * @param object_type BACnet 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param value 읽은 값이 저장될 변수
     * @return 성공 여부
     */
    bool ReadSingleProperty(uint32_t device_id, 
                           BACNET_OBJECT_TYPE object_type,
                           uint32_t object_instance, 
                           BACNET_PROPERTY_ID property_id,
                           PulseOne::Structs::DataValue& value);
    
    /**
     * @brief 단일 BACnet 속성 쓰기 (핵심 기능만)
     * @param device_id 대상 디바이스 ID
     * @param object_type BACnet 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param value 쓸 값
     * @param priority 쓰기 우선순위 (1-16)
     * @return 성공 여부
     */
    bool WriteSingleProperty(uint32_t device_id, 
                            BACNET_OBJECT_TYPE object_type,
                            uint32_t object_instance, 
                            BACNET_PROPERTY_ID property_id,
                            const PulseOne::Structs::DataValue& value,
                            uint8_t priority = 16);
    
    /**
     * @brief 싱글턴 인스턴스 접근
     */
    static BACnetDriver* GetInstance();

protected:
    // =============================================================================
    // 보호된 메서드들 (서브클래스 오버라이드 가능)
    // =============================================================================
    virtual bool DoStart();
    virtual bool DoStop();

private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 설정
    PulseOne::Structs::DriverConfig config_;
    uint32_t local_device_id_ = 1234;
    std::string target_ip_ = "";
    uint16_t target_port_ = 47808;
    uint32_t max_apdu_length_ = 1476;
    bool segmentation_support_ = true;
    
    // 상태 관리
    mutable std::atomic<PulseOne::Structs::DriverStatus> status_{PulseOne::Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_stop_{false};
    PulseOne::Structs::ErrorInfo last_error_;
    
    // ✅ 표준 통계 구조 직접 사용 (중복 제거)
    mutable PulseOne::Structs::DriverStatistics driver_statistics_;
    
    // BACnet 스택 관련
    std::atomic<bool> is_bacnet_initialized_{false};
    int socket_fd_ = -1;
    std::atomic<bool> network_thread_running_{false};
    std::thread network_thread_;
    
    // 워커 및 헬퍼 클래스들 (핵심 기능만)
    std::unique_ptr<BACnetWorker> worker_;
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    
    // 🔥 고급 기능들은 별도 클래스로 분리 (필요시 주입)
    // std::unique_ptr<BACnetServiceManager> service_manager_;      // COV, 알람, 스케줄링
    // std::unique_ptr<BACnetDiscoveryManager> discovery_manager_;  // 디바이스/객체 검색
    // std::unique_ptr<BACnetObjectMapper> object_mapper_;          // 객체 매핑
    // std::unique_ptr<BACnetNetworkManager> network_manager_;      // 네트워크 관리
    
    // 동기화
    mutable std::mutex driver_mutex_;
    mutable std::mutex statistics_mutex_;
    mutable std::mutex network_mutex_;
    std::condition_variable network_cv_;
    
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    // 설정 및 초기화
    void ParseDriverConfig(const PulseOne::Structs::DriverConfig& config);
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    bool SetupLocalDevice();
    void InitializeBACnetStatistics();                      // ✅ BACnet 특화 통계 초기화
    
    // 네트워크 관리
    bool StartNetworkThread();
    void StopNetworkThread();
    void NetworkThreadFunction();
    void ProcessIncomingMessages();
    
    // 읽기/쓰기 헬퍼들
    bool ReadSingleValue(const PulseOne::Structs::DataPoint& point, 
                        PulseOne::Structs::TimestampedValue& value);
    bool WriteSingleValue(const PulseOne::Structs::DataPoint& point, 
                         const PulseOne::Structs::DataValue& value);
    
    // BACnet 프로토콜 레벨 메서드들
    uint8_t SendReadPropertyRequest(uint32_t device_id,
                                   BACNET_OBJECT_TYPE object_type,
                                   uint32_t object_instance,
                                   BACNET_PROPERTY_ID property_id);
    
    uint8_t SendWritePropertyRequest(uint32_t device_id,
                                    BACNET_OBJECT_TYPE object_type,
                                    uint32_t object_instance,
                                    BACNET_PROPERTY_ID property_id,
                                    const PulseOne::Structs::DataValue& value,
                                    int32_t array_index = BACNET_ARRAY_ALL,
                                    uint8_t priority = 16);
    
    // Discovery 헬퍼들
    void SendWhoIsRequest();
    void ProcessIAmResponse(uint32_t device_id, const BACNET_ADDRESS& address);
    
    // ✅ 표준 통계 업데이트 메서드들
    void UpdateReadStatistics(bool success, std::chrono::milliseconds duration);
    void UpdateWriteStatistics(bool success, std::chrono::milliseconds duration);
    void UpdateErrorStatistics(const std::string& error_type);
    void UpdateConnectionStatistics(bool connected);
    
    // 유틸리티 메서드들
    std::string BACnetValueToString(const PulseOne::Structs::DataValue& value) const;
    PulseOne::Structs::DataValue ParseBACnetValue(const std::string& str, BACNET_OBJECT_TYPE type) const;
    bool IsValidBACnetAddress(const std::string& address) const;
    BACNET_OBJECT_TYPE StringToObjectType(const std::string& type_str) const;
    BACNET_PROPERTY_ID StringToPropertyID(const std::string& prop_str) const;
    std::string ObjectTypeToString(BACNET_OBJECT_TYPE type) const;
    std::string PropertyIDToString(BACNET_PROPERTY_ID prop) const;
    
    // 주소 관리
    bool ResolveDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address);
    void CacheDeviceAddress(uint32_t device_id, const BACNET_ADDRESS& address);
    
    // 에러 변환
    PulseOne::Structs::ErrorInfo ConvertBACnetError(int bacnet_error_class, 
                                                   int bacnet_error_code, 
                                                   const std::string& context = "") const;
};

// =============================================================================
// 인라인 메서드 구현
// =============================================================================

inline PulseOne::Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return PulseOne::Enums::ProtocolType::BACNET;
}

inline PulseOne::Structs::DriverStatus BACnetDriver::GetStatus() const {
    return status_.load();
}

inline PulseOne::Structs::ErrorInfo BACnetDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(driver_mutex_);
    return last_error_;
}

inline BACnetDriver* BACnetDriver::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    return instance_;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H