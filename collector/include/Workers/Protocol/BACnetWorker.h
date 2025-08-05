/**
 * @file BACnetWorker.h
 * @brief BACnet 프로토콜 워커 클래스 - 🔥 UdpBasedWorker 인터페이스 완전 준수
 * @author PulseOne Development Team
 * @date 2025-08-03
 * @version 1.0.0
 * 
 * 🔥 주요 수정사항:
 * 1. UdpBasedWorker 순수 가상 함수들만 override로 선언
 * 2. 잘못된 BaseDeviceWorker 메서드들 제거 (Start/Stop 등)
 * 3. 헤더와 구현 파일 완전 일치
 * 4. 표준 DriverStatistics 사용
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"                    // ✅ 부모 클래스
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetDriver.h"                    // ✅ BACnet 드라이버
#include "Common/DriverStatistics.h"                       // ✅ 표준 통계 구조                     // ✅ 공통 타입들
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <map>
#include <functional>
#include <chrono>

namespace PulseOne {
namespace Workers {

// =============================================================================
// 🔥 타입 별칭 정의 (BACnetCommonTypes.h에서 가져온 타입들)
#include "Common/Structs.h"
// =============================================================================

// BACnet 구조체들은 BACnetCommonTypes.h에서 정의됨
#include "Common/Structs.h"
using DeviceInfo = Drivers::DeviceInfo;            // 디바이스 정보  
using DataPoint = Drivers::DataPoint;            // 객체 정보

/**
 * @brief BACnet 워커 설정 구조체
 */
struct BACnetWorkerConfig {
    // BACnet 장치 설정
    uint32_t local_device_id = 260001;                        // 로컬 디바이스 ID
    uint16_t target_port = 47808;                             // BACnet 표준 포트
    uint32_t timeout_ms = 5000;                               // 타임아웃 (밀리초)
    uint8_t retry_count = 3;                                  // 재시도 횟수
    
    // 디스커버리 설정
    bool auto_device_discovery = true;                        // 자동 디바이스 발견
    uint32_t discovery_interval_seconds = 300;               // 5분마다 디스커버리
    uint32_t discovery_low_limit = 0;                        // 발견 범위 시작
    uint32_t discovery_high_limit = 4194303;                 // 발견 범위 끝
    
    // 폴링 설정
    uint32_t polling_interval_ms = 1000;                     // 1초마다 폴링
    bool enable_cov = true;                                  // COV 구독 사용
    bool enable_bulk_read = true;                            // ReadPropertyMultiple 사용
    
    // 디버그 설정
    bool verbose_logging = false;                            // 상세 로깅
    
    // 성능 설정
    size_t max_concurrent_reads = 10;                        // 최대 동시 읽기
    size_t max_devices = 100;                               // 최대 디바이스 수
    uint16_t max_apdu_length = 1476;                        // 최대 APDU 길이
    
    /**
     * @brief 설정 유효성 검사
     */
    bool Validate() const {
        return (local_device_id > 0 && local_device_id <= 4194303) &&
               (target_port > 0) &&
               (timeout_ms >= 1000 && timeout_ms <= 60000) &&
               (retry_count >= 1 && retry_count <= 10) &&
               (discovery_interval_seconds >= 60) &&
               (polling_interval_ms >= 100);
    }
};

/**
 * @brief BACnet 워커 통계
 */
struct BACnetWorkerStats {
    std::atomic<uint64_t> discovery_attempts{0};      ///< 발견 시도 횟수
    std::atomic<uint64_t> devices_discovered{0};      ///< 발견된 디바이스 수
    std::atomic<uint64_t> polling_cycles{0};          ///< 폴링 사이클 수
    std::atomic<uint64_t> read_operations{0};         ///< 읽기 작업 수
    std::atomic<uint64_t> write_operations{0};        ///< 쓰기 작업 수
    std::atomic<uint64_t> failed_operations{0};       ///< 실패한 작업 수
    std::atomic<uint64_t> cov_notifications{0};       ///< COV 알림 수
    
    std::chrono::system_clock::time_point start_time;     ///< 시작 시간
    std::chrono::system_clock::time_point last_reset;     ///< 마지막 리셋 시간
    
    /**
     * @brief 통계 리셋
     */
    void Reset() {
        discovery_attempts = 0;
        devices_discovered = 0;
        polling_cycles = 0;
        read_operations = 0;
        write_operations = 0;
        failed_operations = 0;
        cov_notifications = 0;
        last_reset = std::chrono::system_clock::now();
    }
};

// 콜백 함수 타입들 (BACnetCommonTypes.h의 구조체 사용)
#include "Common/Structs.h"
using DeviceDiscoveredCallback = std::function<void(const DeviceInfo&)>;
using ObjectDiscoveredCallback = std::function<void(uint32_t device_id, const DataPoint&)>;
using ValueChangedCallback = std::function<void(const std::string& object_id, const PulseOne::Structs::TimestampedValue&)>;

// =============================================================================
// 🔥 BACnetWorker 클래스 정의
// =============================================================================

/**
 * @class BACnetWorker
 * @brief BACnet 프로토콜 워커 클래스
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet 프로토콜에 특화된 기능을 제공합니다.
 * 
 * 주요 기능:
 * - BACnet 디바이스 자동 발견 (Who-Is/I-Am)
 * - 실시간 데이터 폴링 (ReadProperty)
 * - COV(Change of Value) 구독
 * - 데이터 쓰기 (WriteProperty)
 * - 표준 DriverStatistics 통계 시스템 사용
 */
class BACnetWorker : public UdpBasedWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    /**
     * @brief BACnet 워커 생성자
     */
    explicit BACnetWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~BACnetWorker();

    // =============================================================================
    // 🔥 UdpBasedWorker 순수 가상 함수 구현 (OVERRIDE)
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (BACnet 드라이버 초기화)
     */
    bool EstablishProtocolConnection() override;
    
    /**
     * @brief 프로토콜별 연결 해제 (BACnet 드라이버 정리)
     */
    bool CloseProtocolConnection() override;
    
    /**
     * @brief 프로토콜별 연결 상태 확인
     */
    bool CheckProtocolConnection() override;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (Who-Is 브로드캐스트)
     */
    bool SendProtocolKeepAlive() override;
    
    /**
     * @brief 수신된 UDP 패킷 처리 (BACnet 메시지 파싱)
     */
    bool ProcessReceivedPacket(const UdpPacket& packet) override;
    
    // =============================================================================
    // 🔥 BaseDeviceWorker 순수 가상 함수 구현 (Start/Stop만 존재)
    // =============================================================================
    
    /**
     * @brief 워커 시작 (BaseDeviceWorker 인터페이스)
     */
    std::future<bool> Start() override;
    
    /**
     * @brief 워커 정지 (BaseDeviceWorker 인터페이스)
     */
    std::future<bool> Stop() override;
    
    // =============================================================================
    // BACnet 특화 공개 기능들
    // =============================================================================
    
    /**
     * @brief BACnet 워커 설정
     */
    void ConfigureBACnetWorker(const BACnetWorkerConfig& config);
    
    /**
     * @brief BACnet 워커 통계 조회
     */
    std::string GetBACnetWorkerStats() const;
    
    /**
     * @brief BACnet 워커 통계 리셋
     */
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 발견된 디바이스 목록 JSON 형태로 조회
     */
    std::string GetDiscoveredDevicesAsJson() const;
    
    /**
     * @brief BACnet 디바이스 발견 시작
     */
    bool StartDiscovery();
    
    /**
     * @brief BACnet 디바이스 발견 중지
     */
    void StopDiscovery();
    
    /**
     * @brief 발견된 디바이스 목록 조회
     */
    std::vector<DeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     */
    std::vector<DataPoint> GetDiscoveredObjects(uint32_t device_id) const;
    
    /**
     * @brief 디바이스 발견 콜백 설정
     */
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    
    /**
     * @brief 객체 발견 콜백 설정
     */
    void SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback);
    
    /**
     * @brief 값 변경 콜백 설정
     */
    void SetValueChangedCallback(ValueChangedCallback callback);
    
private:
    // =============================================================================
    // 🔥 내부 구현 메서드들 (헤더에 선언, 구현에서 정의)
    // =============================================================================
    
    /**
     * @brief BACnet 워커 설정 파싱
     */
    bool ParseBACnetWorkerConfig();
    
    /**
     * @brief BACnet 드라이버 설정 생성
     */
    PulseOne::Structs::DriverConfig CreateDriverConfig();
    
    /**
     * @brief BACnet 드라이버 초기화
     */
    bool InitializeBACnetDriver();
    
    /**
     * @brief BACnet 드라이버 종료
     */
    void ShutdownBACnetDriver();
    
    /**
     * @brief 디스커버리 스레드 함수
     */
    void DiscoveryThreadFunction();
    
    /**
     * @brief 폴링 스레드 함수
     */
    void PollingThreadFunction();
    
    /**
     * @brief 디바이스 발견 수행
     */
    bool PerformDiscovery();
    
    /**
     * @brief 데이터 폴링 수행
     */
    bool PerformPolling();
    
    /**
     * @brief 데이터 포인트 처리
     */
    bool ProcessDataPoints(const std::vector<PulseOne::DataPoint>& points);
    
    /**
     * @brief 통계 업데이트
     */
    void UpdateWorkerStats(const std::string& operation, bool success);
    
    /**
     * @brief 객체 ID 생성 (UUID 지원)
     */
    std::string CreateObjectId(const std::string& device_id, const DataPoint& object_info) const;
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // BACnet 드라이버
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    
    // 설정 및 통계
    BACnetWorkerConfig worker_config_;
    BACnetWorkerStats worker_stats_;
    
    // 스레드 관리
    std::atomic<bool> threads_running_;
    std::unique_ptr<std::thread> discovery_thread_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // 발견된 디바이스 관리
    mutable std::mutex devices_mutex_;
    std::map<uint32_t, DeviceInfo> discovered_devices_;
    
    // 콜백 함수들
    DeviceDiscoveredCallback on_device_discovered_;
    ObjectDiscoveredCallback on_object_discovered_;
    ValueChangedCallback on_value_changed_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H