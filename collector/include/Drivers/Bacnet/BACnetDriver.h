// =============================================================================
// include/Drivers/Bacnet/BACnetDriver.h
// 모든 컴파일 에러 해결 버전
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

// 🔥 수정: 올바른 헤더 경로들
#include "Drivers/Common/IProtocolDriver.h"
#include "Utils/LogManager.h"  // 직접 LogManager 사용
#include "Common/UnifiedCommonTypes.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>
#include <queue>

// 🔥 네임스페이스 별칭 (UnifiedCommonTypes.h와 일치)
namespace Utils = PulseOne::Utils;
namespace Constants = PulseOne::Constants;
namespace Structs = PulseOne::Structs;

// 🔥 BACnet 타입들 - 완전한 구조체 정의
extern "C" {
    // BACnet 주소 구조체 - 완전 정의
    typedef struct BACnet_Address {
        uint8_t mac_len;        // MAC 주소 길이 (0-6)
        uint8_t mac[6];         // MAC 주소
        uint16_t net;           // 네트워크 번호 (0 = 로컬)
        uint8_t len;           // 추가 정보 길이
        uint8_t adr[3];        // 추가 주소 정보
    } BACNET_ADDRESS;
    
    // BACnet 애플리케이션 데이터 값
    typedef struct BACnet_Application_Data_Value {
        uint8_t tag;           // 데이터 타입 태그
        union {
            bool Boolean;
            uint32_t Unsigned_Int;
            int32_t Signed_Int;
            float Real;
            double Double;
            struct {
                uint8_t length;
                char value[256];
            } Character_String;
            struct {
                uint32_t type;
                uint32_t instance;
            } Object_Id;
        } type;
    } BACNET_APPLICATION_DATA_VALUE;
    
    // BACnet 객체 타입
    typedef enum {
        OBJECT_ANALOG_INPUT = 0,
        OBJECT_ANALOG_OUTPUT = 1,
        OBJECT_ANALOG_VALUE = 2,
        OBJECT_BINARY_INPUT = 3,
        OBJECT_BINARY_OUTPUT = 4,
        OBJECT_BINARY_VALUE = 5,
        OBJECT_CALENDAR = 6,
        OBJECT_COMMAND = 7,
        OBJECT_DEVICE = 8,
        OBJECT_EVENT_ENROLLMENT = 9,
        OBJECT_FILE = 10,
        OBJECT_GROUP = 11,
        OBJECT_LOOP = 12,
        OBJECT_MULTI_STATE_INPUT = 13,
        OBJECT_MULTI_STATE_OUTPUT = 14,
        OBJECT_NOTIFICATION_CLASS = 15,
        OBJECT_PROGRAM = 16,
        OBJECT_SCHEDULE = 17,
        OBJECT_AVERAGING = 18,
        OBJECT_MULTI_STATE_VALUE = 19,
        OBJECT_TRENDLOG = 20,
        OBJECT_LIFE_SAFETY_POINT = 21,
        OBJECT_LIFE_SAFETY_ZONE = 22,
        MAX_BACNET_OBJECT_TYPE = 23
    } BACNET_OBJECT_TYPE;
    
    // BACnet 속성 ID
    typedef enum {
        PROP_OBJECT_IDENTIFIER = 75,
        PROP_OBJECT_NAME = 77,
        PROP_OBJECT_TYPE = 79,
        PROP_PRESENT_VALUE = 85,
        PROP_DESCRIPTION = 28,
        PROP_UNITS = 117,
        PROP_OBJECT_LIST = 76,
        PROP_MAX_APDU_LENGTH_ACCEPTED = 62,
        PROP_SEGMENTATION_SUPPORTED = 107,
        PROP_VENDOR_NAME = 121,
        PROP_VENDOR_IDENTIFIER = 120,
        PROP_MODEL_NAME = 70,
        PROP_FIRMWARE_REVISION = 44,
        PROP_APPLICATION_SOFTWARE_VERSION = 12,
        PROP_PROTOCOL_VERSION = 98,
        PROP_PROTOCOL_REVISION = 139,
        PROP_SYSTEM_STATUS = 112,
        PROP_DATABASE_REVISION = 155
    } BACNET_PROPERTY_ID;
    
    // BACnet 애플리케이션 태그
    typedef enum {
        BACNET_APPLICATION_TAG_NULL = 0,
        BACNET_APPLICATION_TAG_BOOLEAN = 1,
        BACNET_APPLICATION_TAG_UNSIGNED_INT = 2,
        BACNET_APPLICATION_TAG_SIGNED_INT = 3,
        BACNET_APPLICATION_TAG_REAL = 4,
        BACNET_APPLICATION_TAG_DOUBLE = 5,
        BACNET_APPLICATION_TAG_OCTET_STRING = 6,
        BACNET_APPLICATION_TAG_CHARACTER_STRING = 7,
        BACNET_APPLICATION_TAG_BIT_STRING = 8,
        BACNET_APPLICATION_TAG_ENUMERATED = 9,
        BACNET_APPLICATION_TAG_DATE = 10,
        BACNET_APPLICATION_TAG_TIME = 11,
        BACNET_APPLICATION_TAG_OBJECT_ID = 12,
        MAX_BACNET_APPLICATION_TAG = 13
    } BACNET_APPLICATION_TAG;
    
    // 🔥 BACnet 서비스 데이터 구조체들 (스텁)
    typedef struct {
        uint8_t dummy;  // 스텁용 더미 필드
    } BACNET_CONFIRMED_SERVICE_ACK_DATA;
    
    typedef struct {
        uint8_t dummy;  // 스텁용 더미 필드
    } BACNET_CONFIRMED_SERVICE_DATA;
    
    // 상수들
    static const uint32_t BACNET_ARRAY_ALL = 4294967295U;
    static const uint16_t BACNET_VENDOR_ID = 260;
    static const int MAX_MPDU = 1497;
}

namespace PulseOne::Drivers {

// =============================================================================
// BACnet 특화 구조체들 - 수정된 버전
// =============================================================================

/**
 * @brief BACnet 디바이스 정보 - 수정된 구조체
 */
struct BACnetDeviceInfo {
    uint32_t device_id;                    ///< BACnet 디바이스 ID
    std::string device_name;               ///< 디바이스 이름
    uint16_t vendor_id;                    ///< 벤더 ID
    std::string vendor_name;               ///< 벤더 이름
    std::string model_name;                ///< 모델 이름
    std::string firmware_revision;         ///< 펌웨어 버전
    
    // 🔥 수정: 실제 사용되는 필드들로 변경
    std::string ip_address;                ///< IP 주소 문자열
    uint16_t port;                         ///< 포트 번호
    uint32_t max_apdu_length;              ///< 최대 APDU 길이
    bool segmentation_supported;           ///< 세그멘테이션 지원
    
    BACNET_ADDRESS address;                ///< 네트워크 주소 (이제 완전한 타입)
    std::chrono::steady_clock::time_point discovered_time; ///< 발견 시간
    std::chrono::system_clock::time_point last_seen;       ///< 마지막 응답 시간
    
    BACnetDeviceInfo() : device_id(0), vendor_id(0), port(47808), 
                        max_apdu_length(1476), segmentation_supported(true) {}
};

/**
 * @brief BACnet 객체 정보 - 수정된 구조체
 */
struct BACnetObjectInfo {
    uint32_t device_id;                    ///< 소속 디바이스 ID (구조체 첫 필드로 이동)
    BACNET_OBJECT_TYPE object_type;        ///< 객체 타입
    uint32_t object_instance;              ///< 객체 인스턴스
    std::string object_name;               ///< 객체 이름
    std::string description;               ///< 설명
    std::string units;                     ///< 단위
    bool writable;                         ///< 쓰기 가능 여부
    
    // 🔥 추가: DiscoveryService에서 사용하는 필드들
    BACNET_PROPERTY_ID property_id;        ///< 속성 ID
    uint32_t array_index;                  ///< 배열 인덱스
    DataQuality quality;                   ///< 데이터 품질
    std::chrono::system_clock::time_point timestamp; ///< 타임스탬프
    BACNET_APPLICATION_DATA_VALUE value;   ///< 현재 값
    
    BACnetObjectInfo() : device_id(0), object_type(OBJECT_ANALOG_INPUT), 
                        object_instance(0), writable(false),
                        property_id(PROP_PRESENT_VALUE), array_index(BACNET_ARRAY_ALL),
                        quality(DataQuality::GOOD) {}
    
    // 고유 식별자 생성
    std::string GetId() const {
        return std::to_string(device_id) + "." + 
               std::to_string(object_type) + "." + 
               std::to_string(object_instance);
    }
};

/**
 * @brief BACnet 드라이버 설정
 */
struct BACnetDriverConfig {
    // 네트워크 설정
    std::string interface = "eth0";        ///< 네트워크 인터페이스
    uint16_t port = 47808;                 ///< BACnet/IP 포트 (기본: 47808)
    uint32_t device_instance = 1234;       ///< 로컬 디바이스 인스턴스
    std::string device_name = "PulseOne BACnet Client"; ///< 로컬 디바이스 이름
    
    // 타이밍 설정  
    uint32_t discovery_interval = 30000;   ///< 디스커버리 간격 (ms)
    uint32_t polling_interval = 5000;      ///< 폴링 간격 (ms)
    uint32_t request_timeout = 3000;       ///< 요청 타임아웃 (ms)
    uint32_t retry_count = 3;              ///< 재시도 횟수
    
    // 기능 설정
    bool who_is_enabled = true;            ///< Who-Is 브로드캐스트 활성화
    bool cov_enabled = false;              ///< COV 구독 활성화 (추후 구현)
    bool object_discovery_enabled = true;  ///< 객체 자동 발견 활성화
    uint32_t max_devices = 100;            ///< 최대 디바이스 수
    uint32_t max_objects_per_device = 1000; ///< 디바이스당 최대 객체 수
};

/**
 * @brief BACnet 드라이버 통계
 */
struct BACnetDriverStatistics {
    std::atomic<uint64_t> devices_discovered{0};      ///< 발견된 디바이스 수
    std::atomic<uint64_t> objects_discovered{0};      ///< 발견된 객체 수
    std::atomic<uint64_t> read_requests{0};           ///< 읽기 요청 수
    std::atomic<uint64_t> write_requests{0};          ///< 쓰기 요청 수
    std::atomic<uint64_t> successful_reads{0};        ///< 성공한 읽기 수
    std::atomic<uint64_t> successful_writes{0};       ///< 성공한 쓰기 수
    std::atomic<uint64_t> timeout_errors{0};          ///< 타임아웃 에러 수
    std::atomic<uint64_t> network_errors{0};          ///< 네트워크 에러 수
    std::atomic<uint64_t> who_is_sent{0};             ///< 전송한 Who-Is 수
    std::atomic<uint64_t> i_am_received{0};           ///< 수신한 I-Am 수
    
    std::chrono::steady_clock::time_point start_time; ///< 시작 시간
    std::chrono::steady_clock::time_point last_activity; ///< 마지막 활동 시간
};

// =============================================================================
// BACnetDriver 클래스 - 수정된 버전
// =============================================================================

/**
 * @brief BACnet 프로토콜 드라이버
 */
class BACnetDriver : public IProtocolDriver {
public:
    // ==========================================================================
    // 생성자 / 소멸자 - 수정된 시그니처
    // ==========================================================================
    
    /**
     * @brief 기본 생성자 (원본과 일치)
     */
    BACnetDriver();
    
    /**
     * @brief 소멸자
     */
    ~BACnetDriver() override;
    
    // ==========================================================================
    // IProtocolDriver 구현 - 수정된 시그니처
    // ==========================================================================
    
    // 🔥 수정: Initialize 시그니처 변경 (DriverConfig 매개변수)
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    
    bool WriteValue(const Structs::DataPoint& point, 
                   const Structs::DataValue& value) override;
    
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    
    // 비동기 인터페이스
    std::future<std::vector<TimestampedValue>> ReadValuesAsync(
        const std::vector<Structs::DataPoint>& points, int timeout_ms = 5000) override;
    
    std::future<bool> WriteValueAsync(const Structs::DataPoint& point, 
                                     const Structs::DataValue& value, 
                                     int priority = 16) override;
    
    // ==========================================================================
    // BACnet 특화 기능
    // ==========================================================================
    
    /**
     * @brief WHO-IS 브로드캐스트 전송
     */
    bool SendWhoIs(uint32_t low_device_id = 0, uint32_t high_device_id = 4194303);
    
    /**
     * @brief 발견된 디바이스 목록 조회
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief BACnet 통계 정보 조회
     */
    const BACnetDriverStatistics& GetBACnetStatistics() const;

private:
    // ==========================================================================
    // 내부 구현 메서드
    // ==========================================================================
    
    void ParseBACnetConfig(const std::string& config_str);
    bool InitializeBACnetStack();
    void ShutdownBACnetStack();
    void WorkerThread();
    void DiscoveryThread();
    
    // BACnet 통신 메서드들
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     uint32_t array_index, BACNET_APPLICATION_DATA_VALUE& value);
    
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      uint32_t array_index, const BACNET_APPLICATION_DATA_VALUE& value);
    
    // 헬퍼 메서드들
    bool FindDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address);
    bool ParseDataPoint(const Structs::DataPoint& point,
                       uint32_t& device_id, BACNET_OBJECT_TYPE& object_type,
                       uint32_t& object_instance, BACNET_PROPERTY_ID& property_id,
                       uint32_t& array_index);
    
    bool ConvertToBACnetValue(const Structs::DataValue& data_value,
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);    
    bool ConvertFromBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value,
                               Structs::DataValue& data_value);
    
    // 객체 발견 헬퍼들
    std::vector<BACnetObjectInfo> ScanStandardObjects(uint32_t device_id);
    void ScanObjectInstances(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                            std::vector<BACnetObjectInfo>& objects);
    std::vector<BACnetObjectInfo> ParseObjectList(uint32_t device_id,
                                                  const BACNET_APPLICATION_DATA_VALUE& object_list);
    void EnrichObjectProperties(std::vector<BACnetObjectInfo>& objects);
    std::string GetObjectTypeName(BACNET_OBJECT_TYPE type) const;
    std::string GetPropertyName(BACNET_PROPERTY_ID prop) const;
    
    // 에러 및 통계 처리
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(const std::string& operation, bool success,
                         std::chrono::milliseconds duration);
    
    // 더미 구현용 헬퍼들 (원본 호환성)
    bool CollectIAmResponses();
    bool WaitForReadResponse(uint8_t invoke_id, BACNET_APPLICATION_DATA_VALUE& value, 
                            uint32_t timeout_ms);
    bool WaitForWriteResponse(uint8_t invoke_id, uint32_t timeout_ms);
    uint8_t GetNextInvokeID();
    
    // ==========================================================================
    // BACnet 핸들러 함수들 (정적) - 수정된 시그니처
    // ==========================================================================
    
    static void handler_i_am(uint8_t* service_request, uint16_t service_len,
                            BACNET_ADDRESS* src);
    static void handler_who_is(uint8_t* service_request, uint16_t service_len,
                              BACNET_ADDRESS* src);
    static void handler_read_property_ack(uint8_t* service_request, 
                                         uint16_t service_len, BACNET_ADDRESS* src,
                                         BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void handler_abort(BACNET_ADDRESS* src, uint8_t invoke_id, 
                             uint8_t abort_reason, bool server);
    static void handler_reject(BACNET_ADDRESS* src, uint8_t invoke_id, 
                              uint8_t reject_reason);
    static void handler_read_property(uint8_t* service_request, uint16_t service_len,
                                     BACNET_ADDRESS* src, 
                                     BACNET_CONFIRMED_SERVICE_DATA* service_data);
    static void handler_write_property(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src, 
                                      BACNET_CONFIRMED_SERVICE_DATA* service_data);
    
    // ==========================================================================
    // 멤버 변수 - 수정된 타입들
    // ==========================================================================
    
    // 🔥 수정: LogManager 타입 직접 사용
    std::unique_ptr<DriverLogger> logger_;  // Utils::LogManager 대신 DriverLogger 사용
    BACnetDriverConfig config_;
    BACnetDriverStatistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // 기존 드라이버 설정 (원본 호환성)
    DriverConfig driver_config_;
    
    // 상태 관리
    std::atomic<bool> initialized_;
    std::atomic<bool> connected_;
    std::atomic<bool> stop_threads_;
    std::atomic<Structs::DriverStatus> status_;
    ErrorInfo last_error_;
    
    // 스레드 관리
    std::thread worker_thread_;
    std::thread discovery_thread_;
    std::condition_variable request_cv_;
    std::mutex request_mutex_;
    
    // 데이터 저장
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    std::map<uint32_t, std::vector<BACnetObjectInfo>> device_objects_;
    std::map<std::string, TimestampedValue> object_values_;
    std::map<uint32_t, BACNET_ADDRESS> device_addresses_;  // 원본 호환성
    
    mutable std::mutex devices_mutex_;
    mutable std::mutex objects_mutex_;
    mutable std::mutex values_mutex_;
    
    // BACnet Stack 관련
    static BACnetDriver* instance_;        ///< 싱글톤 인스턴스 (핸들러용)
    static uint8_t Handler_Transmit_Buffer[MAX_MPDU]; ///< 전송 버퍼
};

} // namespace PulseOne::Drivers

#endif // BACNET_DRIVER_H