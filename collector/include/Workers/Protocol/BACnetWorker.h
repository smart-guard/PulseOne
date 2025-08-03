/**
 * @file BACnetWorker.h
 * @brief BACnet 프로토콜 워커 클래스 - 🔥 잘못된 include 수정
 * @author PulseOne Development Team
 * @date 2025-08-03
 * @version 1.0.0
 * 
 * 🔥 주요 수정사항:
 * 1. BACnetStatisticsManager.h → 표준 DriverStatistics 사용
 * 2. 표준 PulseOne 아키텍처 준수
 * 3. include 경로 정리
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet 프로토콜에 특화된 기능을 제공합니다.
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"                    // ✅ 부모 클래스
#include "Drivers/Bacnet/BACnetCommonTypes.h"               // ✅ BACnet 타입들
#include "Drivers/Bacnet/BACnetDriver.h"                    // ✅ BACnet 드라이버
#include "Common/DriverStatistics.h"                       // ✅ 표준 통계 구조 (BACnetStatisticsManager 대신)
#include "Common/UnifiedCommonTypes.h"                      // ✅ 공통 타입들
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
// =============================================================================

// BACnet 구조체들은 BACnetCommonTypes.h에서 정의됨
using BACnetDeviceInfo = Drivers::BACnetDeviceInfo;            // 디바이스 정보  
using BACnetObjectInfo = Drivers::BACnetObjectInfo;            // 객체 정보

/**
 * @brief BACnet 워커 설정 구조체
 */
struct BACnetWorkerConfig {
    // 디스커버리 설정
    bool enable_discovery = true;
    std::chrono::seconds discovery_interval{300};      // 5분마다 디스커버리
    uint32_t discovery_low_limit = 0;
    uint32_t discovery_high_limit = 4194303;          // BACNET_MAX_INSTANCE
    
    // 폴링 설정
    std::chrono::milliseconds polling_interval{1000};  // 1초마다 폴링
    std::chrono::milliseconds read_timeout{5000};      // 읽기 타임아웃 5초
    bool enable_cov = true;                            // COV 구독 사용
    
    // 성능 설정
    size_t max_concurrent_reads = 10;
    size_t max_devices = 100;
    bool enable_bulk_read = true;                      // ReadPropertyMultiple 사용
    
    // 네트워크 설정
    std::string local_ip = "0.0.0.0";
    uint16_t local_port = 47808;
    uint16_t max_apdu_length = 1476;
    
    BACnetWorkerConfig() = default;
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
using DeviceDiscoveredCallback = std::function<void(const BACnetDeviceInfo&)>;
using ObjectDiscoveredCallback = std::function<void(uint32_t device_id, const BACnetObjectInfo&)>;
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
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> DoWork() override;
    bool Initialize() override;
    void Shutdown() override;
    void ProcessReceivedData(const std::vector<uint8_t>& data, const std::string& source_address) override;
    
    // =============================================================================
    // BACnet 특화 기능들
    // =============================================================================
    
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
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 스캔
     */
    bool ScanDeviceObjects(uint32_t device_id);
    
    /**
     * @brief COV 구독
     */
    bool SubscribeCOV(uint32_t device_id, uint32_t object_type, uint32_t object_instance);
    
    /**
     * @brief 단일 속성 읽기
     */
    bool ReadProperty(uint32_t device_id, uint32_t object_type, uint32_t object_instance, 
                     uint32_t property_id, PulseOne::Structs::DataValue& value);
    
    /**
     * @brief 단일 속성 쓰기
     */
    bool WriteProperty(uint32_t device_id, uint32_t object_type, uint32_t object_instance,
                      uint32_t property_id, const PulseOne::Structs::DataValue& value, uint8_t priority = 16);

    // =============================================================================
    // 콜백 설정
    // =============================================================================
    
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    void SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback);
    void SetValueChangedCallback(ValueChangedCallback callback);

    // =============================================================================
    // 상태 및 통계 조회
    // =============================================================================
    
    /**
     * @brief BACnet 워커 통계 조회
     */
    BACnetWorkerStats GetWorkerStatistics() const;
    
    /**
     * @brief 워커 설정 조회
     */
    BACnetWorkerConfig GetWorkerConfig() const { return worker_config_; }
    
    /**
     * @brief 워커 설정 업데이트
     */
    bool UpdateWorkerConfig(const BACnetWorkerConfig& config);

protected:
    // =============================================================================
    // 내부 구현 메서드들
    // =============================================================================
    
    /**
     * @brief BACnet 워커 설정 파싱
     */
    bool ParseBACnetWorkerConfig();
    
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
     * @brief BACnet 패킷 처리
     */
    void ProcessBACnetPacket(const std::vector<uint8_t>& packet, const std::string& source_ip);

private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // BACnet 드라이버
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    
    // 설정 및 통계
    BACnetWorkerConfig worker_config_;
    mutable BACnetWorkerStats worker_stats_;
    mutable std::mutex stats_mutex_;
    
    // 발견된 디바이스들
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    mutable std::mutex devices_mutex_;
    
    // 스레드 관리
    std::atomic<bool> threads_running_;
    std::unique_ptr<std::thread> discovery_thread_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // 콜백들
    DeviceDiscoveredCallback device_discovered_callback_;
    ObjectDiscoveredCallback object_discovered_callback_;
    ValueChangedCallback value_changed_callback_;
    std::mutex callback_mutex_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H