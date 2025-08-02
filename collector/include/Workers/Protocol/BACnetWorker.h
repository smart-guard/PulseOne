/**
 * @file BACnetWorker.h
 * @brief BACnet 프로토콜 워커 클래스 - 컴파일 에러 수정 버전
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 * 
 * @details
 * UdpBasedWorker를 상속받아 BACnet 프로토콜에 특화된 기능을 제공합니다.
 * 🔥 namespace 및 클래스 정의 완전 수정
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"                    // ✅ 부모 클래스
#include "Drivers/Bacnet/BACnetCommonTypes.h"               // ✅ BACnet 타입들
#include "Drivers/Bacnet/BACnetDriver.h"                    // ✅ BACnet 드라이버
#include "Drivers/Bacnet/BACnetStatisticsManager.h"         // ✅ 통계 관리자
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
using BACnetWorkerConfig = Drivers::BACnetDriverConfig;        // 워커 설정
using BACnetWorkerStats = Drivers::BACnetWorkerStats;          // 워커 통계
using BACnetDeviceInfo = Drivers::BACnetDeviceInfo;            // 디바이스 정보  
using BACnetObjectInfo = Drivers::BACnetObjectInfo;            // 객체 정보
using PerformanceSnapshot = Drivers::PerformanceSnapshot;      // 성능 스냅샷

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
 * - 자동 디바이스 발견
 * - 주기적 데이터 폴링
 * - COV(Change of Value) 지원
 * - 실시간 통계 및 모니터링
 */
class BACnetWorker : public UdpBasedWorker {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    BACnetWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client);
    
    /**
     * @brief 소멸자
     */
    virtual ~BACnetWorker();
    
    // 복사/이동 방지
    BACnetWorker(const BACnetWorker&) = delete;
    BACnetWorker& operator=(const BACnetWorker&) = delete;
    BACnetWorker(BACnetWorker&&) = delete;
    BACnetWorker& operator=(BACnetWorker&&) = delete;

    // ==========================================================================
    // BaseDeviceWorker 인터페이스 구현
    // ==========================================================================
    
    /**
     * @brief 워커 시작
     * @return 시작 결과를 담은 future
     */
    std::future<bool> Start() override;
    
    /**
     * @brief 워커 정지
     * @return 정지 결과를 담은 future
     */
    std::future<bool> Stop() override;

    // ==========================================================================
    // BACnet 워커 설정 관리
    // ==========================================================================
    
    /**
     * @brief BACnet 워커 설정
     * @param config 설정 구조체
     */
    void ConfigureBACnetWorker(const BACnetWorkerConfig& config);
    
    /**
     * @brief BACnet 워커 통계 조회
     * @return JSON 형태의 통계 문자열
     */
    std::string GetBACnetWorkerStats() const;
    
    /**
     * @brief BACnet 워커 통계 리셋
     */
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 발견된 디바이스 목록 JSON 조회
     * @return JSON 형태의 디바이스 목록
     */
    std::string GetDiscoveredDevicesAsJson() const;

    // ==========================================================================
    // 콜백 함수 설정
    // ==========================================================================
    
    /**
     * @brief 디바이스 발견 콜백 설정
     * @param callback 콜백 함수
     */
    void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback);
    
    /**
     * @brief 객체 발견 콜백 설정
     * @param callback 콜백 함수
     */
    void SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback);
    
    /**
     * @brief 값 변경 콜백 설정
     * @param callback 콜백 함수
     */
    void SetValueChangedCallback(ValueChangedCallback callback);

    // ==========================================================================
    // 발견된 정보 조회
    // ==========================================================================
    
    /**
     * @brief 발견된 디바이스 목록 조회
     * @return 디바이스 정보 벡터
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     * @param device_id 디바이스 ID
     * @return 객체 정보 벡터
     */
    std::vector<BACnetObjectInfo> GetDiscoveredObjects(uint32_t device_id) const;

protected:
    // ==========================================================================
    // UdpBasedWorker 순수 가상 함수 구현
    // ==========================================================================
    
    /**
     * @brief 프로토콜 연결 수립
     * @return 성공 시 true
     */
    bool EstablishProtocolConnection() override;
    
    /**
     * @brief 프로토콜 연결 해제
     * @return 성공 시 true
     */
    bool CloseProtocolConnection() override;
    
    /**
     * @brief 프로토콜 연결 상태 확인
     * @return 연결 상태
     */
    bool CheckProtocolConnection() override;
    
    /**
     * @brief 프로토콜 Keep-alive 전송
     * @return 성공 시 true
     */
    bool SendProtocolKeepAlive() override;
    
    /**
     * @brief 수신된 UDP 패킷 처리
     * @param packet UDP 패킷
     * @return 처리 성공 시 true
     */
    bool ProcessReceivedPacket(const UdpPacket& packet) override;
    
    /**
     * @brief 데이터 포인트 처리 (올바른 시그니처로 수정)
     * @param points 데이터 포인트 벡터
     * @return 처리 성공 시 true
     */
    bool ProcessDataPoints(const std::vector<PulseOne::DataPoint>& points) override;

private:
    // ==========================================================================
    // 내부 메서드들
    // ==========================================================================
    
    /**
     * @brief BACnet 워커 설정 파싱
     */
    bool ParseBACnetWorkerConfig();
    
    /**
     * @brief BACnet 드라이버 초기화
     */
    bool InitializeBACnetDriver();
    
    /**
     * @brief BACnet 드라이버 정리
     */
    void ShutdownBACnetDriver();
    
    /**
     * @brief 드라이버 설정 생성
     */
    PulseOne::Structs::DriverConfig CreateDriverConfig();
    
    /**
     * @brief 발견 스레드 함수
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
     * @brief 워커 통계 업데이트
     */
    void UpdateWorkerStats(const std::string& operation, bool success);
    
    /**
     * @brief 객체 ID 생성
     */
    std::string CreateObjectId(uint32_t device_id, const BACnetObjectInfo& object_info) const;

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // BACnet 드라이버
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    
    // 설정 및 통계
    BACnetWorkerConfig worker_config_;
    BACnetWorkerStats worker_stats_;
    
    // 스레드 관리
    std::atomic<bool> threads_running_;
    std::unique_ptr<std::thread> discovery_thread_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // 발견된 정보 저장
    mutable std::mutex devices_mutex_;                                          // 디바이스 목록 보호
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;                   // 발견된 디바이스들
    std::map<uint32_t, std::vector<BACnetObjectInfo>> discovered_objects_;      // 발견된 객체들
    std::map<std::string, PulseOne::Structs::TimestampedValue> current_values_; // 현재 값들
    
    // 콜백 함수들
    DeviceDiscoveredCallback on_device_discovered_;
    ObjectDiscoveredCallback on_object_discovered_;
    ValueChangedCallback on_value_changed_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H