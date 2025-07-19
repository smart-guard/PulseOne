// =============================================================================
// collector/include/Drivers/BACnetDriver.h
// BACnet 프로토콜 드라이버 클래스
// =============================================================================

#ifndef PULSEONE_DRIVERS_BACNET_DRIVER_H
#define PULSEONE_DRIVERS_BACNET_DRIVER_H

#include "IProtocolDriver.h"
#include "DriverLogger.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

// BACnet Stack includes
extern "C" {
#include "bacnet/basic/client/bac-rp.h"
#include "bacnet/basic/client/bac-wp.h"
#include "bacnet/basic/client/bac-data.h"
#include "bacnet/basic/sys/sbuf.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/npdu/h_npdu.h"
#include "bacnet/bacnet_stack_exports.h"
}

namespace PulseOne {
namespace Drivers {

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
};

/**
 * @brief BACnet 설정 구조체
 */
struct BACnetConfig {
    uint32_t device_id = 260001;        ///< 로컬 Device ID
    std::string interface_name = "eth0"; ///< 네트워크 인터페이스
    uint16_t port = 47808;               ///< UDP 포트
    uint32_t apdu_timeout = 6000;        ///< APDU 타임아웃 (ms)
    uint8_t apdu_retries = 3;            ///< APDU 재시도 횟수
    bool who_is_enabled = true;          ///< Who-Is 브로드캐스트 활성화
    uint32_t who_is_interval = 30000;    ///< Who-Is 간격 (ms)
    uint32_t scan_interval = 5000;       ///< 스캔 간격 (ms)
    bool cov_subscription = false;       ///< COV 구독 사용
    uint32_t cov_lifetime = 3600;        ///< COV 구독 수명 (초)
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
    
    // IProtocolDriver 인터페이스 구현
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    bool WriteValue(const DataPoint& point, const DataValue& value) override;
    
    ProtocolType GetProtocolType() const override;
    DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    DriverStatistics GetStatistics() const override;
    
    // 비동기 인터페이스
    std::future<std::vector<TimestampedValue>> ReadValuesAsync(
        const std::vector<DataPoint>& points) override;
    std::future<bool> WriteValueAsync(
        const DataPoint& point, const DataValue& value) override;
    
    // BACnet 특화 기능
    
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

                           // ✅ 새로 추가: 진단 메소드들
    bool EnableDiagnostics(DatabaseManager& db_manager, 
                          bool packet_log = true, bool console = false);
    void DisableDiagnostics();
    std::string GetDiagnosticsJSON() const;
    std::string GetPointName(uint32_t device_id, BACNET_OBJECT_TYPE type, 
                           uint32_t instance) const;

private:
    // 내부 상태
    std::atomic<bool> initialized_;
    std::atomic<bool> connected_;
    std::atomic<DriverStatus> status_;
    ErrorInfo last_error_;
    
    // 설정
    BACnetConfig config_;
    DriverConfig driver_config_;
    
    // 로깅
    std::unique_ptr<DriverLogger> logger_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    DriverStatistics statistics_;
    
    // 스레드 관리
    std::thread worker_thread_;
    std::thread discovery_thread_;
    std::atomic<bool> stop_threads_;
    
    // 디바이스 관리
    mutable std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    // 데이터 포인트 관리
    mutable std::mutex points_mutex_;
    std::map<std::string, BACnetObjectInfo> object_cache_;
    
    // 요청 큐 관리
    mutable std::mutex request_mutex_;
    std::condition_variable request_cv_;
    std::queue<std::function<void()>> request_queue_;
    
    // BACnet 스택 관리
    bool stack_initialized_;
    
    // 내부 메서드
    
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
     * @brief I-Am 핸들러
     * @param service_request 서비스 요청
     * @param service_len 서비스 길이
     * @param src 소스 주소
     */
    static void IAmHandler(uint8_t* service_request, uint16_t service_len,
                          BACNET_ADDRESS* src);
    
    /**
     * @brief COV 알림 핸들러
     * @param service_request 서비스 요청
     * @param service_len 서비스 길이
     * @param src 소스 주소
     */
    static void COVNotificationHandler(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src);
    
    /**
     * @brief DataValue를 BACNET_APPLICATION_DATA_VALUE로 변환
     * @param data_value 입력 DataValue
     * @param data_type 데이터 타입
     * @param bacnet_value 출력 BACnet 값
     * @return 성공 시 true
     */
    bool ConvertToBACnetValue(const DataValue& data_value, DataType data_type,
                             BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    /**
     * @brief BACNET_APPLICATION_DATA_VALUE를 DataValue로 변환
     * @param bacnet_value 입력 BACnet 값
     * @param data_value 출력 DataValue
     * @return 성공 시 true
     */
    bool ConvertFromBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value,
                               DataValue& data_value);
    
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
    bool ParseDataPoint(const DataPoint& point, uint32_t& device_id,
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

    // ✅ 새로 추가: 진단 관련 멤버들
    bool diagnostics_enabled_;
    bool packet_logging_enabled_;
    bool console_output_enabled_;
    
    LogManager* log_manager_;
    DatabaseManager* db_manager_;
    
    struct BACnetDataPointInfo {
        std::string name;
        std::string description;
        std::string unit;
        double scaling_factor;
        double scaling_offset;
    };
    
    struct BACnetPacketLog {
        std::string direction;        // "TX" or "RX"
        Timestamp timestamp;
        uint32_t device_id;
        BACNET_OBJECT_TYPE object_type;
        uint32_t object_instance;
        BACNET_PROPERTY_ID property_id;
        bool success;
        std::string error_message;
        double response_time_ms;
        std::string decoded_value;    // 엔지니어 친화적 값
        std::string raw_data;         // 원시 APDU 데이터
    };
    
    mutable std::mutex diagnostics_mutex_;
    mutable std::mutex points_mutex_;
    mutable std::mutex packet_log_mutex_;
    
    std::map<std::string, BACnetDataPointInfo> point_info_map_;
    std::deque<BACnetPacketLog> packet_history_;
    std::string device_name_;
    
    // 진단 헬퍼 메소드들
    void LogBACnetPacket(const std::string& direction, uint32_t device_id,
                        BACNET_OBJECT_TYPE type, uint32_t instance,
                        BACNET_PROPERTY_ID prop, bool success,
                        const std::string& error = "",
                        double response_time_ms = 0.0);
    
    std::string FormatBACnetValue(uint32_t device_id, BACNET_OBJECT_TYPE type,
                                 uint32_t instance, BACNET_PROPERTY_ID prop,
                                 const BACNET_APPLICATION_DATA_VALUE& value) const;
    
    std::string FormatPacketForConsole(const BACnetPacketLog& log) const;
    bool LoadBACnetPointsFromDB();
    
    // 정적 멤버 (전역 인스턴스)
    static BACnetDriver* instance_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_BACNET_DRIVER_H