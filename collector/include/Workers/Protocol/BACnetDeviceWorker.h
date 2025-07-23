/**
 * @file BACnetDeviceWorker.h
 * @brief BACnet UDP 디바이스 워커 클래스
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 * 
 * @details
 * BaseDeviceWorker → UdpBasedWorker → BACnetDeviceWorker 상속 구조
 * BACnet/IP 프로토콜 전용 디바이스 워커 구현
 */

#ifndef BACNET_DEVICE_WORKER_H
#define BACNET_DEVICE_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <condition_variable>

// BACnet Stack includes
extern "C" {
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#include "bacnet/datalink.h"
#include "bacnet/services.h"
#include "bacnet/cov.h"
#include "bacnet/whois.h"
#include "bacnet/iam.h"
#include "bacnet/tsm.h"
#include "bacnet/h_npdu.h"
#include "bacnet/address.h"
}

namespace PulseOne {
namespace Workers {

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id;                      ///< BACnet Device ID
    std::string device_name;                 ///< 디바이스 이름  
    std::string ip_address;                  ///< IP 주소
    uint16_t port = 47808;                   ///< UDP 포트
    uint32_t max_apdu_length = 480;          ///< 최대 APDU 길이
    bool segmentation_supported = false;     ///< 세그멘테이션 지원 여부
    uint16_t vendor_id = 0;                  ///< 벤더 ID
    std::string application_software_version; ///< 소프트웨어 버전
    std::chrono::system_clock::time_point last_seen; ///< 마지막 응답 시간
    
    /// 네트워크 정보
    BACNET_ADDRESS bacnet_address;           ///< BACnet 주소
    bool is_router = false;                  ///< 라우터 여부
    std::vector<uint16_t> network_list;      ///< 네트워크 목록 (라우터인 경우)
};

/**
 * @brief BACnet 객체 정보
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type;          ///< 객체 타입
    uint32_t object_instance;                ///< 객체 인스턴스
    std::string object_name;                 ///< 객체 이름
    std::string description;                 ///< 설명
    BACNET_PROPERTY_ID property_id;          ///< 속성 ID (주로 Present_Value)
    uint32_t array_index = BACNET_ARRAY_ALL; ///< 배열 인덱스
    
    /// 현재 값 정보
    BACNET_APPLICATION_DATA_VALUE present_value; ///< 현재 값
    Drivers::DataQuality quality = Drivers::DataQuality::GOOD; ///< 데이터 품질
    std::chrono::system_clock::time_point timestamp; ///< 타임스탬프
    
    /// COV 관련
    bool cov_subscribed = false;             ///< COV 구독 상태
    uint32_t cov_process_id = 0;             ///< COV 프로세스 ID
    uint32_t cov_lifetime = 3600;            ///< COV 수명 (초)
    std::chrono::system_clock::time_point cov_subscription_time; ///< 구독 시간
};

/**
 * @brief BACnet 설정 구조체
 */
struct BACnetConfig {
    /// 로컬 디바이스 설정
    uint32_t device_id = 260001;             ///< 로컬 Device ID
    std::string device_name = "PulseOne-Collector"; ///< 로컬 디바이스 이름
    std::string object_name = "PulseOne BACnet Collector"; ///< Object_Name
    uint16_t vendor_id = 999;                ///< 벤더 ID (임시)
    
    /// 네트워크 설정  
    std::string interface_name = "eth0";     ///< 네트워크 인터페이스
    uint16_t port = 47808;                   ///< UDP 포트
    bool broadcast_enabled = true;           ///< 브로드캐스트 허용
    
    /// 통신 설정
    uint32_t apdu_timeout = 6000;            ///< APDU 타임아웃 (ms)
    uint8_t apdu_retries = 3;                ///< APDU 재시도 횟수
    uint32_t response_timeout = 5000;        ///< 응답 타임아웃 (ms)
    
    /// 디스커버리 설정
    bool who_is_enabled = true;              ///< Who-Is 브로드캐스트 활성화
    uint32_t who_is_interval = 30000;        ///< Who-Is 간격 (ms)
    uint32_t discovery_timeout = 10000;      ///< 디스커버리 타임아웃 (ms)
    uint32_t device_object_scan_interval = 60000; ///< 디바이스 객체 스캔 간격 (ms)
    
    /// 스캔 설정
    uint32_t scan_interval = 5000;           ///< 기본 스캔 간격 (ms)
    uint16_t max_batch_size = 50;            ///< 배치 읽기 최대 크기
    uint16_t max_concurrent_requests = 10;   ///< 최대 동시 요청 수
    
    /// COV 설정
    bool cov_subscription = false;           ///< COV 구독 사용
    uint32_t cov_lifetime = 3600;            ///< COV 구독 수명 (초)
    bool auto_cov_subscription = true;       ///< 자동 COV 구독
    
    /// 로깅 설정
    bool packet_logging = false;             ///< 패킷 로깅
    uint32_t max_packet_history = 1000;      ///< 최대 패킷 히스토리
    bool debug_enabled = false;              ///< 디버그 모드
};

/**
 * @brief BACnet 요청 타입
 */
enum class BACnetRequestType {
    READ_PROPERTY,
    WRITE_PROPERTY,
    READ_PROPERTY_MULTIPLE,
    WHO_IS,
    I_AM,
    COV_NOTIFICATION,
    SUBSCRIBE_COV,
    UNSUBSCRIBE_COV,
    DEVICE_COMMUNICATION_CONTROL,
    REINITIALIZE_DEVICE
};

/**
 * @brief BACnet 요청 정보
 */
struct BACnetRequest {
    BACnetRequestType request_type;          ///< 요청 타입
    uint32_t target_device_id;               ///< 대상 디바이스 ID
    BACNET_OBJECT_TYPE object_type;          ///< 객체 타입
    uint32_t object_instance;                ///< 객체 인스턴스
    BACNET_PROPERTY_ID property_id;          ///< 속성 ID
    uint32_t array_index = BACNET_ARRAY_ALL; ///< 배열 인덱스
    BACNET_APPLICATION_DATA_VALUE value;     ///< 값 (쓰기용)
    
    /// 요청 메타데이터
    uint8_t invoke_id;                       ///< Invoke ID
    std::chrono::system_clock::time_point request_time; ///< 요청 시간
    uint32_t timeout_ms = 5000;              ///< 타임아웃
    uint8_t retries = 0;                     ///< 재시도 횟수
    
    /// 응답 정보
    bool response_received = false;          ///< 응답 수신 여부
    bool success = false;                    ///< 성공 여부
    std::string error_message;               ///< 에러 메시지
    BACNET_APPLICATION_DATA_VALUE response_value; ///< 응답 값
};

/**
 * @class BACnetDeviceWorker
 * @brief BACnet/IP 프로토콜 전용 디바이스 워커
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet/IP 특화 기능을 구현합니다.
 * - BACnet 디바이스 디스커버리 (Who-Is/I-Am)
 * - 객체 및 속성 읽기/쓰기
 * - COV (Change of Value) 구독
 * - 배치 처리 및 성능 최적화
 * - BACnet 프로토콜 스택 통합
 */
class BACnetDeviceWorker : public UdpBasedWorker {

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    explicit BACnetDeviceWorker(
        const Drivers::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );
    
    /**
     * @brief 소멸자
     */
    ~BACnetDeviceWorker() override;
    
    // 복사/이동 방지
    BACnetDeviceWorker(const BACnetDeviceWorker&) = delete;
    BACnetDeviceWorker& operator=(const BACnetDeviceWorker&) = delete;
    BACnetDeviceWorker(BACnetDeviceWorker&&) = delete;
    BACnetDeviceWorker& operator=(BACnetDeviceWorker&&) = delete;

    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현
    // =============================================================================
    
    /**
     * @brief 워커 시작
     * @return Future<bool> 시작 결과
     */
    std::future<bool> Start() override;
    
    /**
     * @brief 워커 정지
     * @return Future<bool> 정지 결과
     */
    std::future<bool> Stop() override;

    // =============================================================================
    // BACnet 특화 공개 인터페이스
    // =============================================================================
    
    /**
     * @brief BACnet 디바이스 디스커버리 실행
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 발견된 디바이스 수
     */
    size_t DiscoverDevices(uint32_t timeout_ms = 10000);
    
    /**
     * @brief 발견된 디바이스 목록 조회
     * @return 디바이스 목록
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     * @param device_id 디바이스 ID
     * @return 객체 목록
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief COV 구독 관리
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param subscribe true=구독, false=구독해제
     * @param lifetime COV 수명 (초)
     * @return 성공 시 true
     */
    bool ManageCOVSubscription(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                               uint32_t object_instance, bool subscribe, 
                               uint32_t lifetime = 3600);

protected:
    // =============================================================================
    // UdpBasedWorker 순수 가상 함수 구현 (BACnet 특화)
    // =============================================================================
    
    /**
     * @brief BACnet 프로토콜 연결 수립
     * @details UDP 소켓이 이미 준비된 상태에서 BACnet Driver를 통해 스택 초기화
     * @return 성공 시 true
     */
    bool EstablishProtocolConnection() override;
    
    /**
     * @brief BACnet 프로토콜 연결 해제
     * @details BACnet Driver를 통한 스택 정리 후 UDP 소켓 해제
     * @return 성공 시 true
     */
    bool CloseProtocolConnection() override;
    
    /**
     * @brief BACnet 프로토콜 연결 상태 확인
     * @details BACnet Driver를 통한 스택 상태 검증
     * @return 연결 상태
     */
    bool CheckProtocolConnection() override;
    
    /**
     * @brief BACnet Keep-alive 전송
     * @details BACnet Driver를 통한 Who-Is 브로드캐스트로 연결 상태 확인
     * @return 성공 시 true
     */
    bool SendProtocolKeepAlive() override;
    
    /**
     * @brief 수신된 BACnet 패킷 처리
     * @details UDP 패킷을 BACnet Driver에 전달하여 프로토콜 처리
     * @param packet 수신된 UDP 패킷
     * @return 성공 시 true
     */
    bool ProcessReceivedPacket(const UdpPacket& packet) override;

private:
    // =============================================================================
    // BACnet 전용 멤버 변수
    // =============================================================================
    
    /// BACnet 설정
    BACnetConfig bacnet_config_;
    
    /// BACnet 드라이버 (순수 통신 담당)
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    
    /// 발견된 디바이스 맵 (Device ID → 디바이스 정보)
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    /// 디바이스별 객체 캐시 (Device ID → 객체 목록)
    std::map<uint32_t, std::vector<BACnetObjectInfo>> device_objects_cache_;
    
    /// 미완료 요청 맵 (Invoke ID → 요청 정보)
    std::map<uint8_t, BACnetRequest> pending_requests_;
    
    /// COV 구독 목록
    std::set<std::tuple<uint32_t, BACNET_OBJECT_TYPE, uint32_t>> cov_subscriptions_;
    
    /// 뮤텍스들
    mutable std::mutex devices_mutex_;
    mutable std::mutex objects_cache_mutex_;
    mutable std::mutex requests_mutex_;
    mutable std::mutex cov_mutex_;
    
    /// 디스커버리 스레드
    std::unique_ptr<std::thread> discovery_thread_;
    std::atomic<bool> discovery_thread_running_;
    
    /// 스캔 스레드
    std::unique_ptr<std::thread> scan_thread_;
    std::atomic<bool> scan_thread_running_;
    
    /// 현재 Invoke ID (요청 식별용)
    std::atomic<uint8_t> current_invoke_id_;

    // =============================================================================
    // BACnet 내부 메서드
    // =============================================================================
    
    /**
     * @brief BACnet 설정 파싱
     * @details device_info의 protocol_config에서 BACnet 설정 추출
     * @return 성공 시 true
     */
    bool ParseBACnetConfig();
    
    /**
     * @brief BACnet 드라이버 초기화
     * @details BACnet Driver 생성 및 설정
     * @return 성공 시 true
     */
    bool InitializeBACnetDriver();
    
    /**
     * @brief BACnet 드라이버 정리
     * @details BACnet Driver 정리 및 해제
     */
    void ShutdownBACnetDriver();
    
    /**
     * @brief 디스커버리 스레드 함수
     */
    void DiscoveryThreadFunction();
    
    /**
     * @brief 스캔 스레드 함수
     */
    void ScanThreadFunction();
    
    /**
     * @brief Who-Is 브로드캐스트 전송 (Driver 위임)
     * @param low_limit 디바이스 ID 하한 (0=제한없음)
     * @param high_limit 디바이스 ID 상한 (0=제한없음)  
     * @return 성공 시 true
     */
    bool SendWhoIs(uint32_t low_limit = 0, uint32_t high_limit = 0);
    
    /**
     * @brief I-Am 응답 처리 (Driver에서 콜백으로 호출)
     * @param device_id 디바이스 ID
     * @param max_apdu 최대 APDU 길이
     * @param segmentation 세그멘테이션 지원
     * @param vendor_id 벤더 ID
     * @param src_address 송신자 주소
     */
    void ProcessIAmResponse(uint32_t device_id, uint32_t max_apdu,
                           BACNET_SEGMENTATION segmentation, uint16_t vendor_id,
                           const BACNET_ADDRESS& src_address);
    
    /**
     * @brief 속성 읽기 요청 (Driver 위임)
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param array_index 배열 인덱스
     * @param value 읽은 값 (출력)
     * @return 성공 시 true
     */
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     uint32_t array_index, BACNET_APPLICATION_DATA_VALUE& value);
    
    /**
     * @brief 속성 쓰기 요청 (Driver 위임)
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param array_index 배열 인덱스
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      uint32_t array_index, const BACNET_APPLICATION_DATA_VALUE& value);
    
    /**
     * @brief COV 알림 처리 (Driver에서 콜백으로 호출)
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_values 속성 값 목록
     */
    void ProcessCOVNotification(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                               uint32_t object_instance,
                               const std::vector<BACNET_PROPERTY_VALUE>& property_values);
    
    /**
     * @brief 폴링 그룹 처리
     * @details 설정된 데이터 포인트들을 그룹화하여 효율적 읽기
     * @param group 폴링 그룹
     * @return 성공 시 true
     */
    bool ProcessPollingGroup(const std::vector<Drivers::DataPoint>& data_points);
    
    /**
     * @brief 데이터 포인트를 BACnet 객체로 변환
     * @param data_point 데이터 포인트
     * @param bacnet_object BACnet 객체 정보 (출력)
     * @return 성공 시 true
     */
    bool DataPointToBACnetObject(const Drivers::DataPoint& data_point,
                                BACnetObjectInfo& bacnet_object);
    
    /**
     * @brief BACnet 객체를 데이터 값으로 변환
     * @param bacnet_object BACnet 객체 정보
     * @param timestamped_value 타임스탬프 값 (출력)
     * @return 성공 시 true
     */
    bool BACnetObjectToTimestampedValue(const BACnetObjectInfo& bacnet_object,
                                       Drivers::TimestampedValue& timestamped_value);
    
    /**
     * @brief 다음 Invoke ID 생성
     * @return 새로운 Invoke ID
     */
    uint8_t GetNextInvokeId();
    
    /**
     * @brief 요청 타임아웃 체크
     */
    void CheckRequestTimeouts();
    
    /**
     * @brief 캐시 정리
     */
    void CleanupCache();
    
    /**
     * @brief BACnet 주소를 IP 주소로 변환
     * @param bacnet_addr BACnet 주소
     * @param ip_str IP 주소 문자열 (출력)
     * @param port 포트 (출력)
     * @return 성공 시 true
     */
    bool BACnetAddressToIP(const BACNET_ADDRESS& bacnet_addr, 
                          std::string& ip_str, uint16_t& port);
    
    /**
     * @brief IP 주소를 BACnet 주소로 변환
     * @param ip_str IP 주소 문자열
     * @param port 포트
     * @param bacnet_addr BACnet 주소 (출력)
     * @return 성공 시 true
     */
    bool IPToBACnetAddress(const std::string& ip_str, uint16_t port,
                          BACNET_ADDRESS& bacnet_addr);

    /**
     * @brief BACnet Driver 콜백 설정
     * @details Driver에서 Worker로의 콜백 함수들 등록
     */
    void SetupDriverCallbacks();
    
    // =============================================================================
    // BACnet Driver 콜백 메서드들 (static)
    // =============================================================================
    
    /**
     * @brief I-Am 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param device_id 디바이스 ID
     * @param max_apdu 최대 APDU
     * @param segmentation 세그멘테이션
     * @param vendor_id 벤더 ID
     * @param address BACnet 주소
     */
    static void OnIAmReceived(void* worker_ptr, uint32_t device_id, uint32_t max_apdu,
                             BACNET_SEGMENTATION segmentation, uint16_t vendor_id,
                             const BACNET_ADDRESS& address);
    
    /**
     * @brief COV 알림 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_values 속성 값 목록
     */
    static void OnCOVNotification(void* worker_ptr, uint32_t device_id,
                                 BACNET_OBJECT_TYPE object_type, uint32_t object_instance,
                                 const std::vector<BACNET_PROPERTY_VALUE>& property_values);
    
    /**
     * @brief 에러 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param error_class 에러 클래스
     * @param error_code 에러 코드
     * @param error_message 에러 메시지
     */
    static void OnBACnetError(void* worker_ptr, BACNET_ERROR_CLASS error_class,
                             BACNET_ERROR_CODE error_code, const std::string& error_message);
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DEVICE_WORKER_H