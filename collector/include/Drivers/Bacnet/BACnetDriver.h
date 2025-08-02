// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 실제 설치된 BACnet 헤더 구조 완전 적용
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
#include <cstring>  // memset을 위해 추가

// =============================================================================
// 🔥 JSON 라이브러리 (기존 패턴 100% 준수)
// =============================================================================
#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
// JSON 라이브러리가 없는 경우 기본 구현 사용 (다른 클래스들과 동일)
struct json {
    template<typename T> T get() const { return T{}; }
    bool contains(const std::string&) const { return false; }
    std::string dump() const { return "{}"; }
    static json parse(const std::string&) { return json{}; }
    static json object() { return json{}; }
    static json array() { return json{}; }
    json& operator[](const std::string& key) { return *this; }
    json& operator=(const std::string& value) { return *this; }
    json& operator=(bool value) { return *this; }
    json& operator=(int value) { return *this; }
    json& operator=(double value) { return *this; }
    json& operator=(const json& other) { return *this; }
    bool empty() const { return true; }
    void push_back(const json& item) {}
    template<typename T>
    T value(const std::string& key, const T& default_value) const { return default_value; }
};
#endif

// =============================================================================
// 🔥 실제 확인된 BACnet Stack 헤더들 include
// =============================================================================
#ifdef HAS_BACNET_STACK
extern "C" {
    // 🔥 핵심 기본 헤더들 (실제 존재 확인됨)
    #include <bacnet/config.h>        // BACnet 설정
    #include <bacnet/bacdef.h>        // BACnet 기본 정의
    #include <bacnet/bacenum.h>       // BACnet 열거형들
    #include <bacnet/bacerror.h>      // BACnet 에러
    #include <bacnet/bactext.h>       // BACnet 텍스트
    #include <bacnet/bacapp.h>        // BACnet 애플리케이션 데이터
    #include <bacnet/bacdcode.h>      // BACnet 인코딩/디코딩
    
    // 🔥 주소 및 네트워크 관련 (실제 존재 확인됨)
    #include <bacnet/bacaddr.h>       // BACnet 주소 (address.h 대신)
    #include <bacnet/npdu.h>          // 네트워크 레이어
    #include <bacnet/apdu.h>          // 애플리케이션 레이어
    
    // 🔥 데이터링크 레이어 (datalink 디렉토리 확인됨)
    #include <bacnet/datalink/datalink.h>  // 데이터링크 기본
    #include <bacnet/datalink/bip.h>       // BACnet/IP 데이터링크
    #include <bacnet/datalink/bvlc.h>      // BACnet Virtual Link Control
    
    // 🔥 트랜잭션 상태 머신 (basic 디렉토리 확인됨)
    #include <bacnet/basic/tsm/tsm.h>      // Transaction State Machine
    
    // 🔥 디바이스 객체 관리 (basic 디렉토리 확인됨)
    #include <bacnet/basic/object/device.h> // Device Object
    
    // 🔥 주소 관리 (실제 함수 위치 확인됨)
    #include <bacnet/basic/binding/address.h> // address_*, address_get_by_device
    
    // 🔥 APDU 핸들러 (실제 함수 위치 확인됨)
    #include <bacnet/basic/service/h_apdu.h>  // apdu_set_*_handler
    
    // 🔥 서비스 관련 헤더들 (실제 존재 확인됨)
    #include <bacnet/whois.h>         // Who-Is 서비스
    #include <bacnet/iam.h>           // I-Am 서비스
    #include <bacnet/rp.h>            // Read Property 서비스
    #include <bacnet/wp.h>            // Write Property 서비스
    #include <bacnet/cov.h>           // COV 서비스
    #include <bacnet/reject.h>        // Reject 서비스
    
    // 🔥 고급 서비스들 (실제 존재 확인됨)
    #include <bacnet/dcc.h>           // Device Communication Control
    #include <bacnet/datetime.h>      // 날짜/시간
    #include <bacnet/event.h>         // 이벤트 처리
    #include <bacnet/timesync.h>      // 시간 동기화
    
    // 🔥 기타 유틸리티들 (실제 존재 확인됨)
    #include <bacnet/bacint.h>        // BACnet 정수
    #include <bacnet/bacstr.h>        // BACnet 문자열
    #include <bacnet/bacreal.h>       // BACnet 실수
    #include <bacnet/property.h>      // 프로퍼티 관리
    #include <bacnet/proplist.h>      // 프로퍼티 리스트
    #include <bacnet/timestamp.h>     // 타임스탬프
    
    // 🔥 확장 서비스들 (실제 존재 확인됨)
    #include <bacnet/rpm.h>           // Read Property Multiple
    #include <bacnet/wpm.h>           // Write Property Multiple
    #include <bacnet/readrange.h>     // Read Range
}

// =============================================================================
// 🔥 BACnet 상수 및 매크로 정의 보완
// =============================================================================

// MAX_MPDU 정의 확인 및 보완
#ifndef MAX_MPDU
    #ifdef MAX_APDU
        #define MAX_MPDU MAX_APDU
    #else
        #define MAX_MPDU 1476  // 표준 BACnet/IP MTU
    #endif
#endif

// BACNET_ARRAY_ALL 정의 확인
#ifndef BACNET_ARRAY_ALL
    #define BACNET_ARRAY_ALL 0xFFFFFFFF
#endif

// MAX_CHARACTER_STRING_BYTES 정의 확인
#ifndef MAX_CHARACTER_STRING_BYTES
    #define MAX_CHARACTER_STRING_BYTES 256
#endif

#else
    // =============================================================================
    // 🔥 HAS_BACNET_STACK이 정의되지 않은 경우 - 스텁 정의들
    // =============================================================================
    
    typedef uint32_t BACNET_OBJECT_TYPE;
    typedef uint32_t BACNET_PROPERTY_ID;
    typedef uint32_t BACNET_CONFIRMED_SERVICE;
    typedef uint32_t BACNET_APPLICATION_TAG;
    typedef uint32_t BACNET_ERROR_CLASS;
    typedef uint32_t BACNET_ERROR_CODE;
    
    typedef struct {
        uint16_t net;
        uint8_t mac[6];
        uint8_t mac_len;
        uint8_t len;
    } BACNET_ADDRESS;
    
    typedef struct {
        uint8_t tag;
        union {
            bool Boolean;
            uint32_t Unsigned_Int;
            int32_t Signed_Int;
            float Real;
            double Double;
            struct {
                uint8_t value[256];
                uint8_t length;
                uint8_t encoding;
            } Character_String;
        } type;
        
        // C++에서 사용하기 위한 기본 생성자
        #ifdef __cplusplus
        BACNET_APPLICATION_DATA_VALUE() : tag(0) {
            memset(&type, 0, sizeof(type));
        }
        #endif
    } BACNET_APPLICATION_DATA_VALUE;
    
    typedef struct {
        uint8_t invoke_id;
        uint8_t service_choice;
    } BACNET_CONFIRMED_SERVICE_ACK_DATA;
    
    typedef struct {
        BACNET_OBJECT_TYPE object_type;
        uint32_t object_instance;
        BACNET_PROPERTY_ID object_property;
        uint32_t array_index;
        uint8_t application_data[256];
        int application_data_len;
    } BACNET_READ_PROPERTY_DATA;
    
    typedef struct {
        BACNET_OBJECT_TYPE object_type;
        uint32_t object_instance;
        BACNET_PROPERTY_ID object_property;
        uint32_t array_index;
        uint8_t priority;
        uint8_t application_data[256];
        int application_data_len;
    } BACNET_WRITE_PROPERTY_DATA;
    
    // 스텁 상수들
    #define MAX_MPDU 1476
    #define BACNET_ARRAY_ALL 0xFFFFFFFF
    #define MAX_CHARACTER_STRING_BYTES 256
    
    // 객체 타입들
    #define OBJECT_ANALOG_INPUT 0
    #define OBJECT_ANALOG_OUTPUT 1
    #define OBJECT_ANALOG_VALUE 2
    #define OBJECT_BINARY_INPUT 3
    #define OBJECT_BINARY_OUTPUT 4
    #define OBJECT_BINARY_VALUE 5
    #define OBJECT_DEVICE 8
    
    // 프로퍼티 ID들
    #define PROP_PRESENT_VALUE 85
    #define PROP_OBJECT_NAME 77
    #define PROP_OBJECT_TYPE 79
    #define PROP_DESCRIPTION 28
    #define PROP_OBJECT_LIST 76
    #define PROP_UNITS 117
    
    // 서비스 타입들
    #define SERVICE_CONFIRMED_READ_PROPERTY 12
    #define SERVICE_CONFIRMED_WRITE_PROPERTY 15
    #define SERVICE_UNCONFIRMED_I_AM 0
    
    // 애플리케이션 태그들
    #define BACNET_APPLICATION_TAG_BOOLEAN 1
    #define BACNET_APPLICATION_TAG_UNSIGNED_INT 2
    #define BACNET_APPLICATION_TAG_SIGNED_INT 3
    #define BACNET_APPLICATION_TAG_REAL 4
    #define BACNET_APPLICATION_TAG_DOUBLE 5
    #define BACNET_APPLICATION_TAG_CHARACTER_STRING 7
    
    // 스텁 함수 선언들
    inline bool address_own_device_id_set(uint32_t) { return true; }
    inline void address_init() {}
    inline bool bip_init(char*) { return true; }
    inline bool address_get_by_device(uint32_t, uint32_t*, BACNET_ADDRESS*) { return false; }
    inline uint8_t tsm_next_free_invokeID() { return 1; }
    inline int rp_encode_apdu(uint8_t*, uint8_t, BACNET_READ_PROPERTY_DATA*) { return 10; }
    inline int wp_encode_apdu(uint8_t*, uint8_t, BACNET_WRITE_PROPERTY_DATA*) { return 10; }
    inline int whois_encode_apdu(uint8_t*, uint32_t, uint32_t) { return 10; }
    inline bool bvlc_send_pdu(BACNET_ADDRESS*, uint8_t*, uint8_t*, int) { return true; }
    inline int bacapp_encode_application_data(uint8_t*, BACNET_APPLICATION_DATA_VALUE*) { return 4; }
    inline int bacapp_decode_application_data(uint8_t*, int, BACNET_APPLICATION_DATA_VALUE*) { return 4; }
    inline bool characterstring_ansi_copy(char*, size_t, void*) { return true; }
    inline void characterstring_init_ansi(void*, const char*) {}
    inline bool iam_decode_service_request(uint8_t*, uint32_t*, unsigned*, int*, uint16_t*) { return true; }
    inline bool rp_ack_decode_service_request(uint8_t*, uint16_t, BACNET_READ_PROPERTY_DATA*) { return true; }
    inline void apdu_set_unconfirmed_handler(uint8_t, void(*)(uint8_t*, uint16_t, BACNET_ADDRESS*)) {}
    inline void apdu_set_confirmed_ack_handler(uint8_t, void(*)(uint8_t*, uint16_t, BACNET_ADDRESS*, BACNET_CONFIRMED_SERVICE_ACK_DATA*)) {}
    inline void apdu_set_error_handler(uint8_t, void(*)(BACNET_ADDRESS*, uint8_t, BACNET_ERROR_CLASS, BACNET_ERROR_CODE)) {}
    inline void apdu_set_abort_handler(void(*)(BACNET_ADDRESS*, uint8_t, uint8_t, bool)) {}
    inline void apdu_set_reject_handler(void(*)(BACNET_ADDRESS*, uint8_t, uint8_t)) {}
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
    bool online = false;
    
    bool IsValid() const {
        return (device_id > 0 && device_id <= 4194303 && 
                !ip_address.empty() && port > 0);
    }
};

/**
 * @brief BACnet 객체 정보 구조체
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type = OBJECT_ANALOG_INPUT;
    uint32_t object_instance = 0;
    std::string object_name = "";
    std::string description = "";
    
    // 🔥 BACnetDiscoveryService에서 사용되는 추가 필드들
    BACNET_PROPERTY_ID property_id = PROP_PRESENT_VALUE;
    BACNET_APPLICATION_DATA_VALUE value;
    uint32_t array_index = BACNET_ARRAY_ALL;
    bool writable = false;
    std::string units = "";
    std::chrono::system_clock::time_point timestamp;
    Enums::DataQuality quality = Enums::DataQuality::GOOD;
    
    bool IsValid() const {
        return (object_instance > 0 && object_instance <= 4194303);
    }
};

/**
 * @brief 펜딩 중인 BACnet 요청 구조체
 */
struct PendingRequest {
    uint8_t invoke_id = 0;
    std::chrono::system_clock::time_point timeout;
    uint8_t service_choice = 0;
    uint32_t target_device_id = 0;
    BACNET_APPLICATION_DATA_VALUE* result_value = nullptr;
    std::promise<bool> promise;
};

/**
 * @brief BACnet Driver 클래스
 * 
 * PulseOne 아키텍처에 완전히 통합된 BACnet/IP 프로토콜 드라이버
 * 실제 BACnet 네트워크 통신을 지원하며, 스텁 모드도 제공
 */
class BACnetDriver : public IProtocolDriver {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 방지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;
    
    // =============================================================================
    // IProtocolDriver 인터페이스 구현
    // =============================================================================
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
    
    // =============================================================================
    // BACnet 특화 메서드들
    // =============================================================================
    
    // 디바이스 발견
    bool SendWhoIs();
    std::map<uint32_t, BACnetDeviceInfo> GetDiscoveredDevices() const;
    int DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& discovered_devices, 
                       int timeout_ms = 3000);
    
    // 객체 관리
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    // 프로퍼티 읽기/쓰기 (BACnet 특화)
    bool ReadBACnetProperty(uint32_t device_id,
                           BACNET_OBJECT_TYPE obj_type, 
                           uint32_t obj_instance, 
                           BACNET_PROPERTY_ID prop_id, 
                           TimestampedValue& result,
                           uint32_t array_index = BACNET_ARRAY_ALL);
                           
    bool WriteBACnetProperty(uint32_t device_id,
                            BACNET_OBJECT_TYPE obj_type,
                            uint32_t obj_instance,
                            BACNET_PROPERTY_ID prop_id,
                            const Structs::DataValue& value,
                            uint8_t priority = 0,
                            uint32_t array_index = BACNET_ARRAY_ALL);

private:
    // =============================================================================
    // BACnet Stack 관리
    // =============================================================================
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    void RegisterBACnetHandlers();
    void NetworkThreadFunction();
    void ProcessBACnetMessages();
    
    // =============================================================================
    // BACnet 요청 전송
    // =============================================================================
    uint8_t SendWhoIsRequest(uint32_t low_limit = 0, uint32_t high_limit = 4194303);
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
    
    // =============================================================================
    // 데이터 변환
    // =============================================================================
    bool ConvertToBACnetValue(const Structs::DataValue& pulse_value, 
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    Structs::DataValue ConvertBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    // =============================================================================
    // 유틸리티 메서드들
    // =============================================================================
    void SetError(Enums::ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    void CompleteRequest(uint8_t invoke_id, bool success);
    bool WaitForResponse(uint8_t invoke_id, int timeout_ms);
    bool ReadDeviceObjectList(uint32_t device_id, std::vector<BACnetObjectInfo>& objects);
    
    // =============================================================================
    // 정적 콜백 핸들러들 (BACnet Stack 콜백용)
    // =============================================================================
    static void StaticIAmHandler(uint8_t* service_request, uint16_t service_len, 
                                 BACNET_ADDRESS* src);
    static void StaticReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                           BACNET_ADDRESS* src,
                                           BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void StaticWritePropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                            BACNET_ADDRESS* src,
                                            BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void StaticErrorHandler(BACNET_ADDRESS* src, uint8_t invoke_id,
                                  BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    static void StaticAbortHandler(BACNET_ADDRESS* src, uint8_t invoke_id,
                                  uint8_t abort_reason, bool server);
    static void StaticRejectHandler(BACNET_ADDRESS* src, uint8_t invoke_id,
                                   uint8_t reject_reason);
    
    // =============================================================================
    // 인스턴스 핸들러들
    // =============================================================================
    void HandleIAm(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
    void HandleReadPropertyAck(uint8_t* service_request, uint16_t service_len,
                              BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    void HandleWritePropertyAck(uint8_t* service_request, uint16_t service_len,
                               BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    void HandleError(BACNET_ADDRESS* src, uint8_t invoke_id,
                    BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    void HandleAbort(BACNET_ADDRESS* src, uint8_t invoke_id,
                    uint8_t abort_reason, bool server);
    void HandleReject(BACNET_ADDRESS* src, uint8_t invoke_id,
                     uint8_t reject_reason);

private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 설정
    DriverConfig config_;
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> current_status_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> should_stop_;
    
    // 에러 정보
    Structs::ErrorInfo last_error_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    DriverStatistics statistics_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> network_thread_;
    
    // 요청/응답 관리
    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::map<uint8_t, std::unique_ptr<PendingRequest>> pending_requests_;
    
    // 발견된 디바이스들
    mutable std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    // 정적 인스턴스 (콜백을 위한)
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H