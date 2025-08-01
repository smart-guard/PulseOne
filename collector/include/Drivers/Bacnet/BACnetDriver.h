// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 실제 존재하는 BACnet 헤더들로 수정
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/UnifiedCommonTypes.h"
#include <memory>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <future>

// 🔥 실제 존재하는 BACnet Stack 헤더들만 include
#ifdef HAS_BACNET_STACK
extern "C" {
    // 🔥 핵심 헤더들 (실제 존재함)
    #include <bacnet/config.h>        // BACnet 설정
    #include <bacnet/bacdef.h>        // BACnet 기본 정의
    #include <bacnet/bacenum.h>       // BACnet 열거형들
    #include <bacnet/bacerror.h>      // BACnet 에러
    #include <bacnet/bactext.h>       // BACnet 텍스트
    #include <bacnet/bacapp.h>        // BACnet 애플리케이션 데이터
    #include <bacnet/bacdcode.h>      // BACnet 인코딩/디코딩
    #include <bacnet/address.h>       // BACnet 주소
    #include <bacnet/npdu.h>          // 네트워크 레이어
    #include <bacnet/apdu.h>          // 애플리케이션 레이어
    #include <bacnet/datalink.h>      // 데이터링크 레이어
    #include <bacnet/tsm.h>           // 트랜잭션 상태 머신
    #include <bacnet/device.h>        // 디바이스 객체
    
    // 🔥 서비스 관련 헤더들
    #include <bacnet/whois.h>         // Who-Is 서비스
    #include <bacnet/iam.h>           // I-Am 서비스
    #include <bacnet/rp.h>            // Read Property 서비스
    #include <bacnet/wp.h>            // Write Property 서비스
    #include <bacnet/cov.h>           // COV 서비스
    #include <bacnet/reject.h>        // Reject 서비스
    
    // 🔥 핸들러 관련 헤더들
    #include <bacnet/h_apdu.h>        // APDU 핸들러
    #include <bacnet/h_rp.h>          // Read Property 핸들러
    #include <bacnet/h_wp.h>          // Write Property 핸들러
    #include <bacnet/h_iam.h>         // I-Am 핸들러
    #include <bacnet/h_whois.h>       // Who-Is 핸들러
    
    // 🔥 Basic 서비스들
    #include <bacnet/basic/services.h>    // 기본 서비스들
    #include <bacnet/services.h>          // 서비스 정의
    
    // 🔥 데이터링크 관련
    #include <bacnet/datalink/bip.h>      // BACnet/IP
    #include <bacnet/datalink/datalink.h> // 데이터링크 인터페이스
}
#endif

// 네임스페이스 별칭 (기존 패턴 유지)
namespace Utils = PulseOne::Utils;
namespace Constants = PulseOne::Constants;
namespace Structs = PulseOne::Structs;
namespace Enums = PulseOne::Enums;

namespace PulseOne {
namespace Drivers {

// 🔥 전역 BACnet 버퍼 선언 (외부에서 정의)
#ifdef HAS_BACNET_STACK
extern uint8_t Rx_Buf[MAX_MPDU];
extern uint8_t Tx_Buf[MAX_MPDU];
#endif

/**
 * @brief BACnet 디바이스 정보 구조체 (BACnetWorker 호환)
 */
struct BACnetDeviceInfo {
    uint32_t device_id = 0;
    std::string device_name = "";
    std::string ip_address = "";
    uint16_t port = 47808;
    uint32_t max_apdu_length = 1476;
    bool segmentation_supported = false;
    std::string vendor_name = "";
    uint16_t vendor_id = 0;
    std::string model_name = "";
    std::string firmware_version = "";
    std::string location = "";
    std::string description = "";
    std::chrono::system_clock::time_point last_seen;
    
    bool IsValid() const {
#ifdef HAS_BACNET_STACK
        return (device_id > 0 && device_id <= BACNET_MAX_INSTANCE && 
                !ip_address.empty() && port > 0);
#else
        return (device_id > 0 && !ip_address.empty() && port > 0);
#endif
    }
};

/**
 * @brief BACnet 객체 정보 구조체
 */
struct BACnetObjectInfo {
#ifdef HAS_BACNET_STACK
    BACNET_OBJECT_TYPE object_type = MAX_BACNET_OBJECT_TYPE;
    BACNET_PROPERTY_ID property_id = MAX_BACNET_PROPERTY_ID;
    BACNET_APPLICATION_DATA_VALUE value;
    uint32_t array_index = BACNET_ARRAY_ALL;
#else
    int object_type = 0;
    int property_id = 0;
    int value = 0;
    uint32_t array_index = 0;
#endif
    uint32_t object_instance = 0;
    std::string object_name = "";
    bool writable = false;
    std::string units = "";
    std::chrono::system_clock::time_point timestamp;
    PulseOne::Enums::DataQuality quality = PulseOne::Enums::DataQuality::GOOD;  
    
    bool IsValid() const {
#ifdef HAS_BACNET_STACK
        return (object_type < MAX_BACNET_OBJECT_TYPE && 
                object_instance <= BACNET_MAX_INSTANCE &&
                property_id < MAX_BACNET_PROPERTY_ID);
#else
        return (object_instance > 0);
#endif
    }
};

/**
 * @brief 실제 BACnet Stack 연동 드라이버
 * @note BACnetWorker와 완전 호환, 실제 BACnet/IP 통신 지원
 */
class BACnetDriver : public IProtocolDriver {
public:
    BACnetDriver();
    ~BACnetDriver() override;
    
    // =======================================================================
    // IProtocolDriver 인터페이스 구현 (기존 패턴 유지)
    // =======================================================================
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    
    bool WriteValue(const Structs::DataPoint& point, 
                   const Structs::DataValue& value) override;
    
    Enums::ProtocolType GetProtocolType() const override;
    
    // 🔥 수정: DriverState -> DriverStatus로 변경
    Structs::DriverStatus GetStatus() const override;
    
    Structs::ErrorInfo GetLastError() const override;
        
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;

    // =======================================================================
    // BACnet 특화 메서드들 (BACnetWorker 호환)
    // =======================================================================
    
    /**
     * @brief BACnet 디바이스 검색 (Who-Is 브로드캐스트)
     * @param discovered_devices [out] 발견된 디바이스 맵
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 발견된 디바이스 수
     */
    int DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& discovered_devices, 
                       int timeout_ms = 5000);
    
    /**
     * @brief 특정 BACnet 프로퍼티 읽기
     */
    bool ReadBACnetProperty(uint32_t device_id,
#ifdef HAS_BACNET_STACK
                           BACNET_OBJECT_TYPE obj_type, 
                           uint32_t obj_instance, 
                           BACNET_PROPERTY_ID prop_id, 
                           TimestampedValue& result,
                           uint32_t array_index = BACNET_ARRAY_ALL);
#else
                           int obj_type, 
                           uint32_t obj_instance, 
                           int prop_id, 
                           TimestampedValue& result,
                           uint32_t array_index = 0);
#endif
    
    /**
     * @brief 특정 BACnet 프로퍼티 쓰기
     */
    bool WriteBACnetProperty(uint32_t device_id,
#ifdef HAS_BACNET_STACK
                            BACNET_OBJECT_TYPE obj_type,
                            uint32_t obj_instance,
                            BACNET_PROPERTY_ID prop_id,
                            const Structs::DataValue& value,
                            uint8_t priority = 0,
                            uint32_t array_index = BACNET_ARRAY_ALL);
#else
                            int obj_type,
                            uint32_t obj_instance,
                            int prop_id,
                            const Structs::DataValue& value,
                            uint8_t priority = 0,
                            uint32_t array_index = 0);
#endif
    
    /**
     * @brief 디바이스 객체 목록 읽기
     */
    bool ReadDeviceObjectList(uint32_t device_id, 
                             std::vector<BACnetObjectInfo>& objects);

    bool SendWhoIs();
    std::map<uint32_t, BACnetDeviceInfo> GetDiscoveredDevices() const;
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief 자세한 드라이버 상태 정보 반환 (BACnet 특화)
     */
    Structs::DriverState GetDetailedStatus() const;

private:
    // =======================================================================
    // 멤버 변수들 (기존 패턴 유지)
    // =======================================================================
    DriverConfig config_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_stop_{false};
    
    // 현재 드라이버 상태
    std::atomic<Structs::DriverStatus> current_status_{Structs::DriverStatus::UNINITIALIZED};
    
    Structs::ErrorInfo last_error_;
    mutable DriverStatistics statistics_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex devices_mutex_;
 
    void* bacnet_ctx_{nullptr};

    // BACnet 특화 변수들
    uint32_t local_device_id_ = 389999;
    std::string target_ip_ = "127.0.0.1";
    uint16_t target_port_ = 47808;
    
    // 스레드 관리
    std::unique_ptr<std::thread> network_thread_;
    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    
    // 응답 대기 관리
    struct PendingRequest {
        uint8_t invoke_id;
        std::promise<bool> promise;
        std::chrono::system_clock::time_point timeout;
#ifdef HAS_BACNET_STACK
        BACNET_CONFIRMED_SERVICE service_choice;
        BACNET_APPLICATION_DATA_VALUE* result_value = nullptr;
#else
        int service_choice;
        void* result_value = nullptr;
#endif
        uint32_t target_device_id;
    };
    
    std::map<uint8_t, std::unique_ptr<PendingRequest>> pending_requests_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    // =======================================================================
    // BACnet Stack 초기화 및 관리
    // =======================================================================
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    void NetworkThreadFunction();
    void ProcessBACnetMessages();
    
    // =======================================================================
    // BACnet 서비스 헬퍼 메서드들
    // =======================================================================
    uint8_t SendWhoIsRequest(uint32_t low_limit = 0, uint32_t high_limit = 0);
    uint8_t SendReadPropertyRequest(uint32_t device_id,
#ifdef HAS_BACNET_STACK
                                   BACNET_OBJECT_TYPE obj_type,
                                   uint32_t obj_instance,
                                   BACNET_PROPERTY_ID prop_id,
                                   uint32_t array_index = BACNET_ARRAY_ALL);
#else
                                   int obj_type,
                                   uint32_t obj_instance,
                                   int prop_id,
                                   uint32_t array_index = 0);
#endif
    
    uint8_t SendWritePropertyRequest(uint32_t device_id,
#ifdef HAS_BACNET_STACK
                                    BACNET_OBJECT_TYPE obj_type,
                                    uint32_t obj_instance,
                                    BACNET_PROPERTY_ID prop_id,
                                    const BACNET_APPLICATION_DATA_VALUE& value,
                                    uint8_t priority = 0,
                                    uint32_t array_index = BACNET_ARRAY_ALL);
#else
                                    int obj_type,
                                    uint32_t obj_instance,
                                    int prop_id,
                                    int value,
                                    uint8_t priority = 0,
                                    uint32_t array_index = 0);
#endif
    
    // =======================================================================
    // 콜백 핸들러 등록
    // =======================================================================
    void RegisterBACnetHandlers();
    
    // =======================================================================
    // 정적 콜백 함수들 (BACnet Stack에서 호출)
    // =======================================================================
#ifdef HAS_BACNET_STACK
    static void StaticIAmHandler(uint8_t* service_request, uint16_t service_len, 
                                BACNET_ADDRESS* src);
    
    static void StaticReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                           BACNET_ADDRESS* src,
                                           BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    
    static void StaticWritePropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                            BACNET_ADDRESS* src,
                                            BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    
    static void StaticErrorHandler(BACNET_ADDRESS* src,
                                 uint8_t invoke_id,
                                 BACNET_ERROR_CLASS error_class,
                                 BACNET_ERROR_CODE error_code);
                                 
    static void StaticAbortHandler(BACNET_ADDRESS* src,
                                 uint8_t invoke_id,
                                 uint8_t abort_reason,
                                 bool server); 
                                 
    static void StaticRejectHandler(BACNET_ADDRESS* src,
                                  uint8_t invoke_id,
                                  uint8_t reject_reason);
#endif
    
    // =======================================================================
    // 실제 핸들러 구현 (인스턴스 메서드)
    // =======================================================================
#ifdef HAS_BACNET_STACK
    void HandleIAm(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
    void HandleReadPropertyAck(uint8_t* service_request, uint16_t service_len,
                              BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    void HandleWritePropertyAck(uint8_t* service_request, uint16_t service_len,
                               BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    
    void HandleError(BACNET_ADDRESS* src, uint8_t invoke_id,
                    BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    void HandleAbort(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t abort_reason);
    void HandleReject(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason);
#endif
    
    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    void SetError(Enums::ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    
#ifdef HAS_BACNET_STACK
    Structs::DataValue ConvertBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    bool ConvertToBACnetValue(const Structs::DataValue& pulse_value, 
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    BACNET_OBJECT_TYPE StringToObjectType(const std::string& type_str);
    BACNET_PROPERTY_ID StringToPropertyId(const std::string& prop_str);
    
    std::string ObjectTypeToString(BACNET_OBJECT_TYPE obj_type);
    std::string PropertyIdToString(BACNET_PROPERTY_ID prop_id);
#endif
    
    bool WaitForResponse(uint8_t invoke_id, int timeout_ms = 5000);
    void CompleteRequest(uint8_t invoke_id, bool success);
    
    // =======================================================================
    // 정적 인스턴스 포인터 (콜백에서 사용)
    // =======================================================================
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;
};

} // namespace PulseOne::Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H