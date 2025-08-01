// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 실제 BACnet Stack 라이브러리 연동 구현
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

// BACnet Stack 라이브러리 헤더들 (조건부 include)
#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/config.h>
    #include <bacnet/bactext.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/iam.h>
    #include <bacnet/whois.h>
    #include <bacnet/rp.h>
    #include <bacnet/wp.h>
    #include <bacnet/tsm.h>
    #include <bacnet/address.h>
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    #include <bacnet/device.h>
    #include <bacnet/net.h>
    #include <bacnet/datalink.h>
    #include <bacnet/handlers.h>
    #include <bacnet/client.h>
    #include <bacnet/txbuf.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
}
#endif

// 네임스페이스 별칭 (기존 패턴 유지)
namespace Utils = PulseOne::Utils;
namespace Constants = PulseOne::Constants;
namespace Structs = PulseOne::Structs;
namespace Enums = PulseOne::Enums;

namespace PulseOne {
namespace Drivers {

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
    
    bool IsValid() const {
        return (device_id > 0 && device_id <= BACNET_MAX_INSTANCE && 
                !ip_address.empty() && port > 0);
    }
};

/**
 * @brief BACnet 객체 정보 구조체
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type = MAX_BACNET_OBJECT_TYPE;
    uint32_t object_instance = 0;
    std::string object_name = "";
    BACNET_PROPERTY_ID property_id = MAX_BACNET_PROPERTY_ID;
    uint32_t array_index = BACNET_ARRAY_ALL;
    bool writable = false;
    std::string units = "";
    BACNET_APPLICATION_DATA_VALUE value;
    
    bool IsValid() const {
        return (object_type < MAX_BACNET_OBJECT_TYPE && 
                object_instance <= BACNET_MAX_INSTANCE &&
                property_id < MAX_BACNET_PROPERTY_ID);
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
     * @param device_id 대상 디바이스 ID
     * @param obj_type 객체 타입
     * @param obj_instance 객체 인스턴스
     * @param prop_id 프로퍼티 ID
     * @param result [out] 읽은 값과 타임스탬프
     * @param array_index 배열 인덱스 (기본: ALL)
     * @return 성공 여부
     */
    bool ReadBACnetProperty(uint32_t device_id,
                           BACNET_OBJECT_TYPE obj_type, 
                           uint32_t obj_instance, 
                           BACNET_PROPERTY_ID prop_id, 
                           TimestampedValue& result,
                           uint32_t array_index = BACNET_ARRAY_ALL);
    
    /**
     * @brief 특정 BACnet 프로퍼티 쓰기
     * @param device_id 대상 디바이스 ID
     * @param obj_type 객체 타입
     * @param obj_instance 객체 인스턴스
     * @param prop_id 프로퍼티 ID
     * @param value 쓸 값
     * @param priority 우선순위 (1-16, 0=없음)
     * @param array_index 배열 인덱스 (기본: ALL)
     * @return 성공 여부
     */
    bool WriteBACnetProperty(uint32_t device_id,
                            BACNET_OBJECT_TYPE obj_type,
                            uint32_t obj_instance,
                            BACNET_PROPERTY_ID prop_id,
                            const Structs::DataValue& value,
                            uint8_t priority = 0,
                            uint32_t array_index = BACNET_ARRAY_ALL);
    
    /**
     * @brief 디바이스 객체 목록 읽기
     * @param device_id 대상 디바이스 ID
     * @param objects [out] 객체 정보 벡터
     * @return 성공 여부
     */
    bool ReadDeviceObjectList(uint32_t device_id, 
                             std::vector<BACnetObjectInfo>& objects);

private:
    // =======================================================================
    // 멤버 변수들 (기존 패턴 유지)
    // =======================================================================
    DriverConfig config_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_stop_{false};
    Structs::ErrorInfo last_error_;
    mutable DriverStatistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // BACnet 특화 변수들
    uint32_t local_device_id_ = 389999;  // 기본 디바이스 ID
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
        BACNET_CONFIRMED_SERVICE service_choice;
        uint32_t target_device_id;
        BACNET_APPLICATION_DATA_VALUE* result_value = nullptr;
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
    uint8_t SendWhoIsRequest(uint32_t low_limit = 0, uint32_t high_limit = BACNET_MAX_INSTANCE);
    uint8_t SendReadPropertyRequest(uint32_t device_id,
                                   BACNET_OBJECT_TYPE obj_type,
                                   uint32_t obj_instance,
                                   BACNET_PROPERTY_ID prop_id,
                                   uint32_t array_index = BACNET_ARRAY_ALL);
    
    uint8_t SendWritePropertyRequest(uint32_t device_id,
                                    BACNET_OBJECT_TYPE obj_type,
                                    uint32_t obj_instance,
                                    BACNET_PROPERTY_ID prop_id,
                                    const BACNET_APPLICATION_DATA_VALUE& value,
                                    uint8_t priority = 0,
                                    uint32_t array_index = BACNET_ARRAY_ALL);
    
    // =======================================================================
    // 콜백 핸들러 등록
    // =======================================================================
    void RegisterBACnetHandlers();
    
    // =======================================================================
    // 정적 콜백 함수들 (BACnet Stack에서 호출)
    // =======================================================================
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
                                 uint8_t abort_reason);
                                 
    static void StaticRejectHandler(BACNET_ADDRESS* src,
                                  uint8_t invoke_id,
                                  uint8_t reject_reason);
    
    // =======================================================================
    // 실제 핸들러 구현 (인스턴스 메서드)
    // =======================================================================
    void HandleIAm(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
    void HandleReadPropertyAck(uint8_t* service_request, uint16_t service_len,
                              BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    void HandleWritePropertyAck(uint8_t* service_request, uint16_t service_len,
                               BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    
    void HandleError(BACNET_ADDRESS* src, uint8_t invoke_id,
                    BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    void HandleAbort(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t abort_reason);
    void HandleReject(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason);
    
    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    void SetError(Enums::ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    
    Structs::DataValue ConvertBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    bool ConvertToBACnetValue(const Structs::DataValue& pulse_value, 
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    BACNET_OBJECT_TYPE StringToObjectType(const std::string& type_str);
    BACNET_PROPERTY_ID StringToPropertyId(const std::string& prop_str);
    
    std::string ObjectTypeToString(BACNET_OBJECT_TYPE obj_type);
    std::string PropertyIdToString(BACNET_PROPERTY_ID prop_id);
    
    bool WaitForResponse(uint8_t invoke_id, int timeout_ms = 5000);
    void CompleteRequest(uint8_t invoke_id, bool success);
    
    // =======================================================================
    // 정적 인스턴스 포인터 (콜백에서 사용)
    // =======================================================================
    static BACnetDriver* instance_;  // 현재 활성 인스턴스
    static std::mutex instance_mutex_;
};

} // namespace PulseOne::Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H