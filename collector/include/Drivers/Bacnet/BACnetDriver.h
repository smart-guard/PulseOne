#ifndef PULSEONE_DRIVERS_BACNET_DRIVER_H
#define PULSEONE_DRIVERS_BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
#include "Database/DatabaseManager.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <map>
#include <deque>

// BACnet Stack includes
extern "C" {
#include "bacnet/basic/client/bac-data.h"  // ✅ 존재 확인됨
#include "bacnet/basic/client/bac-rw.h"    // ✅ Read/Write 함수들
#include "bacnet/basic/client/bac-discover.h" // ✅ Discovery 함수들
#include "bacnet/rp.h"                     // ✅ Read Property
#include "bacnet/wp.h"                     // ✅ Write Property
#include "bacnet/whois.h"                  // ✅ Who-Is
#include "bacnet/iam.h"                    // ✅ I-Am
#include "bacnet/cov.h"                    // ✅ COV
#include "bacnet/basic/object/device.h"    // ✅ Device 함수들
#include "bacnet/basic/services.h"         // ✅ Services
#include "bacnet/basic/sys/bacnet_stack_exports.h"  // ✅ 확인됨
#include "bacnet/bacdef.h"                 // ✅ 기본 정의들
#include "bacnet/bacenum.h"                // ✅ Enum 정의들
#include "bacnet/bacapp.h"                 // ✅ Application 데이터
#include "bacnet/bacstr.h"                 // ✅ 문자열 함수들
#include "bacnet/npdu.h"                   // ✅ Network PDU
#if __has_include("bacnet/bacenum.h")
#include "bacnet/bacenum.h"
#endif

#if __has_include("bacnet/bacapp.h")  
#include "bacnet/bacapp.h"
#endif

#if __has_include("bacnet/npdu.h")
#include "bacnet/npdu.h"
#endif

#if __has_include("bacnet/iam.h")
#include "bacnet/iam.h"
#endif

#if __has_include("bacnet/rp.h")
#include "bacnet/rp.h"
#endif

#if __has_include("bacnet/wp.h")
#include "bacnet/wp.h"
#endif

#if __has_include("bacnet/bacstr.h")
#include "bacnet/bacstr.h"
#elif __has_include("bacnet/characterstring.h")
#include "bacnet/characterstring.h"
#endif

#if __has_include("bacnet/datalink/datalink.h")
#include "bacnet/datalink/datalink.h"
#elif __has_include("bacnet/datalink.h")
#include "bacnet/datalink.h"
#endif

// 서비스 헤더들 (존재하는 경우)
#if __has_include("bacnet/basic/services.h")
#include "bacnet/basic/services.h"
#endif

#if __has_include("bacnet/basic/service/s_rp.h")
#include "bacnet/basic/service/s_rp.h"
#endif

#if __has_include("bacnet/basic/service/s_wp.h")
#include "bacnet/basic/service/s_wp.h"
#endif

#if __has_include("bacnet/basic/tsm/tsm.h")
#include "bacnet/basic/tsm/tsm.h"
#endif
}

namespace PulseOne {
namespace Drivers {

// 🔥 편의성을 위한 상수 alias 정의

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id;          ///< BACnet Device ID
    std::string device_name;     ///< 디바이스 이름
    std::string ip_address;      ///< IP 주소
    uint16_t port;               ///< UDP 포트 (기본 47808)
    uint32_t max_apdu_length;    ///< 최대 APDU 길이
    bool segmentation_supported; ///< 세그멘테이션 지원 여부
    std::chrono::system_clock::time_point last_seen; ///< 마지막 응답 시간
    
    // 🔥 기본값 설정을 통합 상수 사용
    BACnetDeviceInfo() 
        : device_id(Constants::BACNET_DEFAULT_DEVICE_INSTANCE)
        , port(Constants::BACNET_DEFAULT_PORT)
        , max_apdu_length(Constants::BACNET_MAX_APDU_LENGTH)
        , segmentation_supported(true)
        , last_seen(std::chrono::system_clock::now()) {}
};

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type;      ///< 객체 타입
    uint32_t object_instance;            ///< 객체 인스턴스
    BACNET_PROPERTY_ID property_id;      ///< 속성 ID
    uint32_t array_index;                ///< 배열 인덱스 (BACNET_ARRAY_ALL for scalar)
    std::string object_name;             ///< 객체 이름
    BACNET_APPLICATION_DATA_VALUE value; ///< 현재 값
    DataQuality quality;                 ///< 데이터 품질
    std::chrono::system_clock::time_point timestamp; ///< 타임스탬프
    
    // 🔥 기본값 설정
    BACnetObjectInfo() 
        : object_type(OBJECT_ANALOG_INPUT)
        , object_instance(0)
        , property_id(PROP_PRESENT_VALUE)
        , array_index(BACNET_ARRAY_ALL)
        , quality(DataQuality::GOOD)
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief BACnet 설정 구조체 (🔥 통합 상수 적용)
 */
struct BACnetConfig {
    uint32_t device_id = Constants::BACNET_DEFAULT_DEVICE_INSTANCE;        ///< 로컬 Device ID
    std::string interface_name = Constants::DEFAULT_INTERFACE_NAME;        ///< 네트워크 인터페이스
    uint16_t port = Constants::BACNET_DEFAULT_PORT;                       ///< UDP 포트
    uint32_t apdu_timeout = Constants::BACNET_DEFAULT_APDU_TIMEOUT_MS;    ///< APDU 타임아웃 (ms)
    uint8_t apdu_retries = Constants::BACNET_DEFAULT_APDU_RETRIES;        ///< APDU 재시도 횟수
    bool who_is_enabled = true;                                ///< Who-Is 브로드캐스트 활성화
    uint32_t who_is_interval = Constants::BACNET_WHO_IS_INTERVAL_MS;      ///< Who-Is 간격 (ms)
    uint32_t scan_interval = Constants::BACNET_SCAN_INTERVAL_MS;          ///< 스캔 간격 (ms)
    bool cov_subscription = false;                             ///< COV 구독 사용
    uint32_t cov_lifetime = Constants::BACNET_COV_LIFETIME_SECONDS;       ///< COV 구독 수명 (초)
    uint16_t max_apdu_length = Constants::BACNET_MAX_APDU_LENGTH;         ///< 최대 APDU 길이
    uint8_t segmentation = Constants::BACNET_SEGMENTED_BOTH;              ///< 세그멘테이션 지원
};

/**
 * @brief BACnet 패킷 로그 구조체 (진단용)
 */
struct BACnetPacketLog {
    std::string direction;        // "TX" or "RX"
    std::chrono::system_clock::time_point timestamp;
    uint32_t device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    bool success;
    std::string error_message;
    double response_time_ms;
    std::string decoded_value;    // 엔지니어 친화적 값
    std::string raw_data;         // 원시 APDU 데이터
    
    // 🔥 기본값 설정
    BACnetPacketLog() 
        : direction("Unknown")
        , timestamp(std::chrono::system_clock::now())
        , device_id(0)
        , object_type(OBJECT_ANALOG_INPUT)
        , object_instance(0)
        , property_id(PROP_PRESENT_VALUE)
        , success(false)
        , response_time_ms(0.0) {}
};
/**
 * @brief BACnet 프로토콜 드라이버
 * 
 * BACnet/IP 프로토콜을 사용하여 BACnet 디바이스와 통신합니다.
 * Device Discovery, Property 읽기/쓰기, COV(Change of Value) 구독을 지원합니다.
 */
class BACnetDriver : public IProtocolDriver {
public:
    /**
     * @brief 생성자
     */
    BACnetDriver();
    
    /**
     * @brief 소멸자
     */
    virtual ~BACnetDriver();
    
    // =============================================================================
    // IProtocolDriver 인터페이스 구현
    // =============================================================================
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    bool WriteValue(const Structs::DataPoint& point, const Structs::DataValue& value) override;
    
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    
    // 비동기 인터페이스
    std::future<std::vector<TimestampedValue>> ReadValuesAsync(
        const std::vector<Structs::DataPoint>& points, int timeout_ms = 0) override;
    std::future<bool> WriteValueAsync(
        const Structs::DataPoint& point, const Structs::DataValue& value, int priority = 16) override;
    
    // =============================================================================
    // BACnet 특화 기능
    // =============================================================================
    
    /**
     * @brief Who-Is 요청 전송 (디바이스 검색)
     * @param low_device_id 검색 시작 디바이스 ID (선택적)
     * @param high_device_id 검색 종료 디바이스 ID (선택적)
     * @return 성공 시 true
     */
    bool SendWhoIs(uint32_t low_device_id = 0, uint32_t high_device_id = BACNET_MAX_INSTANCE);
    
    /**
     * @brief 발견된 디바이스 목록 반환
     * @return 디바이스 정보 벡터
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 디바이스의 객체 목록 조회
     * @param device_id 대상 디바이스 ID
     * @return 객체 정보 벡터
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief COV 구독
     * @param device_id 대상 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param lifetime 구독 수명 (초)
     * @return 성공 시 true
     */
    bool SubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, uint32_t lifetime = 3600);
    
    /**
     * @brief COV 구독 해제
     * @param device_id 대상 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @return 성공 시 true
     */
    bool UnsubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                        uint32_t object_instance);
    
    bool FindDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address);
    bool CollectIAmResponses();
    bool WaitForReadResponse(uint8_t invoke_id, BACNET_APPLICATION_DATA_VALUE& value, 
                                      uint32_t timeout_ms);
    bool WaitForWriteResponse(uint8_t invoke_id, uint32_t timeout_ms);



private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 기본 상태
    std::atomic<bool> initialized_;
    std::atomic<bool> connected_;
    std::atomic<Structs::DriverStatus> status_;
    
    // 설정
    DriverConfig driver_config_;
    BACnetConfig config_;
    
    // 로깅 및 에러
    std::unique_ptr<DriverLogger> logger_;
    ErrorInfo last_error_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    DriverStatistics statistics_;
    
    // 스레드 관리
    std::atomic<bool> stop_threads_;
    std::thread worker_thread_;
    std::thread discovery_thread_;
    std::condition_variable request_cv_;
    std::mutex request_mutex_;
    
    // 디바이스 관리
    mutable std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    // 진단 및 패킷 로깅
    mutable std::mutex packet_log_mutex_;
    std::deque<BACnetPacketLog> packet_history_;

    std::map<uint32_t, BACNET_ADDRESS> device_addresses_;
    
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    /**
     * @brief BACnet 설정 파싱
     * @param config_str 설정 문자열
     */
    void ParseBACnetConfig(const std::string& config_str);
    
    /**
     * @brief BACnet 스택 초기화
     * @return 성공 시 true
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 스택 종료
     */
    void ShutdownBACnetStack();
    
    /**
     * @brief 워커 스레드 함수
     */
    void WorkerThread();
    
    /**
     * @brief 디스커버리 스레드 함수
     */
    void DiscoveryThread();
    
    /**
     * @brief 단일 속성 읽기
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param array_index 배열 인덱스
     * @param value 출력 값
     * @return 성공 시 true
     */
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     uint32_t array_index, BACNET_APPLICATION_DATA_VALUE& value);
    
    /**
     * @brief 단일 속성 쓰기
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param array_index 배열 인덱스
     * @param value 입력 값
     * @return 성공 시 true
     */
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      uint32_t array_index, const BACNET_APPLICATION_DATA_VALUE& value);
    
    /**
     * @brief Structs::DataValue를 BACNET_APPLICATION_DATA_VALUE로 변환
     * @param data_value 입력 Structs::DataValue
     * @param data_type 데이터 타입
     * @param bacnet_value 출력 BACnet 값
     * @return 성공 시 true
     */
    bool ConvertToBACnetValue(const Structs::DataValue& data_value,
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);    
    /**
     * @brief BACNET_APPLICATION_DATA_VALUE를 Structs::DataValue로 변환
     * @param bacnet_value 입력 BACnet 값
     * @param data_value 출력 Structs::DataValue
     * @return 성공 시 true
     */
    bool ConvertFromBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value,
                               Structs::DataValue& data_value);
    
    /**
     * @brief 데이터 포인트에서 BACnet 객체 정보 파싱
     * @param point 데이터 포인트
     * @param device_id 출력 디바이스 ID
     * @param object_type 출력 객체 타입
     * @param object_instance 출력 객체 인스턴스
     * @param property_id 출력 속성 ID
     * @param array_index 출력 배열 인덱스
     * @return 성공 시 true
     */
    bool ParseDataPoint(const Structs::DataPoint& point, uint32_t& device_id,
                       BACNET_OBJECT_TYPE& object_type, uint32_t& object_instance,
                       BACNET_PROPERTY_ID& property_id, uint32_t& array_index);
    
    /**
     * @brief 에러 설정
     * @param code 에러 코드
     * @param message 에러 메시지
     */
    void SetError(ErrorCode code, const std::string& message);
    
    /**
     * @brief 통계 업데이트
     * @param operation 작업 타입
     * @param success 성공 여부
     * @param duration 소요 시간
     */
    void UpdateStatistics(const std::string& operation, bool success,
                         std::chrono::milliseconds duration);
    
    /**
     * @brief 객체 타입 이름 반환
     * @param type 객체 타입
     * @return 타입 이름 문자열
     */
    std::string GetObjectTypeName(BACNET_OBJECT_TYPE type) const;
    
    /**
     * @brief 속성 이름 반환
     * @param prop 속성 ID
     * @return 속성 이름 문자열
     */
    std::string GetPropertyName(BACNET_PROPERTY_ID prop) const;
    
    /**
     * @brief 패킷 히스토리 정리
     */
    void TrimPacketHistory();
    
    /**
     * @brief 패킷 로그를 파일 형태로 포맷팅
     * @param log 패킷 로그
     * @return 포맷된 문자열
     */
    std::string FormatPacketForFile(const BACnetPacketLog& log) const;
    uint8_t GetNextInvokeID();
        std::vector<BACnetObjectInfo> ScanStandardObjects(uint32_t device_id);
    void ScanObjectInstances(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                            std::vector<BACnetObjectInfo>& objects);
    std::vector<BACnetObjectInfo> ParseObjectList(uint32_t device_id,
                                                 const BACNET_APPLICATION_DATA_VALUE& object_list);
    void EnrichObjectProperties(std::vector<BACnetObjectInfo>& objects);

};

} // namespace Drivers
} // namespace PulseOne


#endif 