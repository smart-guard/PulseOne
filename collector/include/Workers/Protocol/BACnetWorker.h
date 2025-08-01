/**
 * @file BACnetWorker.h
 * @brief BACnet 프로토콜 워커 클래스
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet/IP 프로토콜 특화 기능을 제공합니다.
 * BACnetDriver를 사용하여 실제 BACnet 통신을 수행합니다.
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <map>
#include <memory>

namespace PulseOne {
namespace Workers {

/**
 * @brief BACnet 워커 설정 구조체
 */
struct BACnetWorkerConfig {
    uint32_t device_id = 260001;             ///< 로컬 Device ID
    uint16_t port = 47808;                   ///< UDP 포트
    uint32_t discovery_interval = 30000;     ///< 디스커버리 간격 (ms)
    uint32_t polling_interval = 5000;        ///< 폴링 간격 (ms)
    bool auto_discovery = true;              ///< 자동 디스커버리 활성화
    uint32_t apdu_timeout = 6000;            ///< APDU 타임아웃 (ms)
    uint8_t apdu_retries = 3;                ///< APDU 재시도 횟수
    
    // 🔥 BACnetWorker.cpp에서 요구하는 필드들 추가
    uint32_t max_retries = 3;                ///< 최대 재시도 횟수 (apdu_retries와 동일)
    std::string target_ip = "";              ///< 타겟 IP 주소
    uint16_t target_port = 47808;            ///< 타겟 포트
    uint32_t device_instance = 260001;       ///< 디바이스 인스턴스
    uint16_t max_apdu_length = 1476;         ///< 최대 APDU 길이
};

/**
 * @brief BACnet 워커 통계 정보
 */
struct BACnetWorkerStatistics {
    std::atomic<uint64_t> discovery_attempts{0};   ///< 디스커버리 시도 수
    std::atomic<uint64_t> devices_discovered{0};   ///< 발견된 디바이스 수
    std::atomic<uint64_t> polling_cycles{0};       ///< 폴링 사이클 수
    std::atomic<uint64_t> read_operations{0};      ///< 읽기 작업 수
    std::atomic<uint64_t> write_operations{0};     ///< 쓰기 작업 수
    std::atomic<uint64_t> successful_reads{0};     ///< 성공한 읽기 수
    std::atomic<uint64_t> successful_writes{0};    ///< 성공한 쓰기 수
    std::atomic<uint64_t> failed_operations{0};    ///< 실패한 작업 수
    
    std::chrono::system_clock::time_point start_time;     ///< 통계 시작 시간
    std::chrono::system_clock::time_point last_reset;     ///< 마지막 리셋 시간
};

/**
 * @class BACnetWorker
 * @brief BACnet/IP 프로토콜 워커
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet/IP 프로토콜에 특화된 기능을 제공합니다.
 * BACnetDriver를 내부적으로 사용하여 실제 BACnet 통신을 수행합니다.
 * - BACnet 디바이스 자동 발견
 * - 주기적 데이터 폴링
 * - 비동기 읽기/쓰기 작업
 * - 통계 및 상태 모니터링
 */
class BACnetWorker : public UdpBasedWorker {

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
    explicit BACnetWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~BACnetWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;

    // =============================================================================
    // BACnet 공개 인터페이스
    // =============================================================================
    
    /**
     * @brief BACnet 워커 설정
     * @param config BACnet 워커 설정
     */
    void ConfigureBACnetWorker(const BACnetWorkerConfig& config);
    
    /**
     * @brief BACnet 워커 통계 정보 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetBACnetWorkerStats() const;
    
    /**
     * @brief BACnet 워커 통계 리셋
     */
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 발견된 디바이스 목록 조회
     * @return JSON 형태의 디바이스 목록
     */
    std::string GetDiscoveredDevices() const;

    // =============================================================================
    // 콜백 인터페이스 (외부 서비스와 연동)
    // =============================================================================
    
    /**
     * @brief 디바이스 발견 콜백 타입
     * @param device 발견된 디바이스 정보
     */
    using DeviceDiscoveredCallback = std::function<void(const Drivers::BACnetDeviceInfo& device)>;
    
    /**
     * @brief 객체 발견 콜백 타입
     * @param device_id 디바이스 ID
     * @param objects 발견된 객체들
     */
    using ObjectDiscoveredCallback = std::function<void(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects)>;
    
    /**
     * @brief 값 변경 콜백 타입
     * @param object_id 객체 식별자 (device_id:object_type:instance)
     * @param value 변경된 값
     */
    using ValueChangedCallback = std::function<void(const std::string& object_id, const PulseOne::TimestampedValue& value)>;
    
    /**
     * @brief 디바이스 발견 콜백 등록
     * @param callback 콜백 함수
     */
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    
    /**
     * @brief 객체 발견 콜백 등록
     * @param callback 콜백 함수
     */
    void SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback);
    
    /**
     * @brief 값 변경 콜백 등록
     * @param callback 콜백 함수
     */
    void SetValueChangedCallback(ValueChangedCallback callback);
    
    /**
     * @brief 발견된 디바이스 목록 반환 (외부 접근용)
     * @return 디바이스 정보 벡터
     */
    std::vector<Drivers::BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 목록 반환
     * @param device_id 디바이스 ID
     * @return 객체 정보 벡터
     */
    std::vector<Drivers::BACnetObjectInfo> GetDiscoveredObjects(uint32_t device_id) const;    

protected:
    // =============================================================================
    // UdpBasedWorker 순수 가상 함수 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;
    bool ProcessReceivedPacket(const UdpPacket& packet) override;

    // =============================================================================
    // BACnet 워커 핵심 기능
    // =============================================================================
    
    /**
     * @brief BACnet 드라이버 초기화
     * @return 성공 시 true
     */
    bool InitializeBACnetDriver();
    
    /**
     * @brief BACnet 드라이버 종료
     */
    void ShutdownBACnetDriver();
    
    /**
     * @brief 데이터 포인트를 BACnet 읽기 작업으로 변환
     * @param points 데이터 포인트 목록
     * @return 성공 시 true
     */
    bool ProcessDataPoints(const std::vector<PulseOne::DataPoint>& points);

    // =============================================================================
    // 멤버 변수 (protected)
    // =============================================================================
    
    /// BACnet 워커 설정
    BACnetWorkerConfig worker_config_;
    
    /// BACnet 워커 통계
    mutable BACnetWorkerStatistics worker_stats_;
    
    /// BACnet 드라이버 인스턴스
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    
    /// 워커 스레드들
    std::unique_ptr<std::thread> discovery_thread_;
    std::unique_ptr<std::thread> polling_thread_;
    
    /// 스레드 실행 플래그
    std::atomic<bool> threads_running_;
    
    /// 발견된 디바이스 정보 (Device ID -> Device Info)
    std::map<uint32_t, Drivers::BACnetDeviceInfo> discovered_devices_;
    
    /// 디바이스 맵 뮤텍스
    mutable std::mutex devices_mutex_;

 private:
    // =============================================================================
    // 내부 메서드 (이미 있는 것들은 그대로 두고 누락된 것만 추가)
    // =============================================================================
    
    /**
     * @brief BACnet 워커 설정 파싱
     * @details device_info의 config_json에서 BACnet 워커 설정 추출
     * @return 성공 시 true
     */
    bool ParseBACnetWorkerConfig();  // ✅ 이미 선언되어 있음
    
    /**
     * @brief 디스커버리 스레드 함수
     */
    void DiscoveryThreadFunction();  // ❌ 추가 필요
    
    /**
     * @brief 폴링 스레드 함수
     */
    void PollingThreadFunction(); 
    
    /**
     * @brief BACnet 드라이버 설정 생성
     * @return 드라이버 설정
     */
    PulseOne::Structs::DriverConfig CreateDriverConfig();
    
    /**
     * @brief Discovery 비즈니스 로직 수행
     * @return 성공 시 true
     */
    bool PerformDiscovery();
    
    /**
     * @brief 통계 업데이트
     * @param operation 작업 타입
     * @param success 성공 여부
     */
    void UpdateWorkerStats(const std::string& operation, bool success);  // ❌ 추가 필요
    /**
     * @brief Polling 비즈니스 로직 수행
     * @return 성공 시 true
     */
    bool PerformPolling();

    // =============================================================================
    // 콜백 함수들
    // =============================================================================
    DeviceDiscoveredCallback on_device_discovered_;
    ObjectDiscoveredCallback on_object_discovered_;
    ValueChangedCallback on_value_changed_;
    
    // =============================================================================
    // 발견된 객체 저장 맵 (추가)
    // =============================================================================
    /// 디바이스별 객체 정보 (Device ID -> Objects)
    std::map<uint32_t, std::vector<Drivers::BACnetObjectInfo>> discovered_objects_;
    
    /// 객체별 현재 값 (Object ID -> Value)
    std::map<std::string, PulseOne::TimestampedValue> current_values_;
    
    // =============================================================================
    // 헬퍼 메서드들
    // =============================================================================
    
    /**
     * @brief 객체 ID 생성 (device_id:object_type:instance)
     * @param device_id 디바이스 ID
     * @param object_info 객체 정보
     * @return 객체 식별자 문자열
     */
    std::string CreateObjectId(uint32_t device_id, const Drivers::BACnetObjectInfo& object_info) const;    

};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H