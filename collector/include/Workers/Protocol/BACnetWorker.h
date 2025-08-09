/**
 * @file BACnetWorker 최종 완성본 - Discovery 제거, 데이터 스캔만!
 * @brief BACnetWorker.h/.cpp 완전 수정본
 * @author PulseOne Development Team
 * @date 2025-08-09
 * @version 6.0.0 - 최종 완성
 * 
 * ✅ 올바른 역할:
 * 1. 설정된 DataPoint만 스캔
 * 2. 파이프라인으로 전송
 * 3. COV 처리
 * 
 * ❌ 제거된 잘못된 역할:
 * 1. Discovery (BACnetDiscoveryService가 담당)
 * 2. DB 저장 (DataProcessingService가 담당)
 */

// =============================================================================
// 📄 collector/include/Workers/Protocol/BACnetWorker.h - 수정본
// =============================================================================

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"                    
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetTypes.h"                                 
#include "Drivers/Bacnet/BACnetDriver.h"  
#include "Drivers/Bacnet/BACnetServiceManager.h"                  
#include "Common/DriverStatistics.h"                       
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
// 타입 별칭 정의
// =============================================================================

using DataPoint = PulseOne::Structs::DataPoint;              
using DeviceInfo = PulseOne::Structs::DeviceInfo;            
using TimestampedValue = PulseOne::Structs::TimestampedValue; 
using DataValue = PulseOne::Structs::DataValue;              

// =============================================================================
// BACnet 워커 통계
// =============================================================================

struct BACnetWorkerStats {
    std::atomic<uint64_t> polling_cycles{0};          
    std::atomic<uint64_t> read_operations{0};         
    std::atomic<uint64_t> write_operations{0};        
    std::atomic<uint64_t> failed_operations{0};       
    std::atomic<uint64_t> cov_notifications{0};       
    
    std::chrono::system_clock::time_point start_time;     
    std::chrono::system_clock::time_point last_reset;     
    
    void Reset() {
        polling_cycles = 0;
        read_operations = 0;
        write_operations = 0;
        failed_operations = 0;
        cov_notifications = 0;
        last_reset = std::chrono::system_clock::now();
    }
};

// =============================================================================
// 콜백 함수 타입들
// =============================================================================

using ValueChangedCallback = std::function<void(const std::string& object_id, const TimestampedValue&)>;

// =============================================================================
// BACnetWorker 클래스 - 데이터 스캔 전용
// =============================================================================

class BACnetWorker : public UdpBasedWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    explicit BACnetWorker(const DeviceInfo& device_info);
    virtual ~BACnetWorker();

    // =============================================================================
    // UdpBasedWorker 순수 가상 함수 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;
    bool ProcessReceivedPacket(const UdpPacket& packet) override;
    
    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    
    // =============================================================================
    // ✅ Worker의 진짜 기능들 - 데이터 스캔 + 파이프라인 전송
    // =============================================================================
    
    /**
     * @brief BACnet 객체 값들을 TimestampedValue로 변환 후 파이프라인 전송
     */
    bool SendBACnetDataToPipeline(const std::map<std::string, DataValue>& object_values,
                                 const std::string& context,
                                 uint32_t priority = 0);

    /**
     * @brief BACnet 단일 속성을 파이프라인 전송
     */
    bool SendBACnetPropertyToPipeline(const std::string& object_id,
                                     const DataValue& property_value,
                                     const std::string& object_name = "",
                                     uint32_t priority = 0);

    /**
     * @brief TimestampedValue 배열을 직접 파이프라인 전송 (로깅 포함)
     */
    bool SendValuesToPipelineWithLogging(const std::vector<TimestampedValue>& values,
                                        const std::string& context,
                                        uint32_t priority = 0);

    /**
     * @brief COV (Change of Value) 알림을 파이프라인 전송
     */
    bool SendCOVNotificationToPipeline(const std::string& object_id,
                                      const DataValue& new_value,
                                      const DataValue& previous_value = DataValue{});

    // =============================================================================
    // ✅ 설정 및 상태 관리
    // =============================================================================
    
    /**
     * @brief 외부에서 설정된 DataPoint들 로드 (BACnetDiscoveryService가 제공)
     */
    void LoadDataPointsFromConfiguration(const std::vector<DataPoint>& data_points);
    
    /**
     * @brief 현재 설정된 DataPoint들 조회
     */
    std::vector<DataPoint> GetConfiguredDataPoints() const;
    
    /**
     * @brief 통계 조회
     */
    std::string GetBACnetWorkerStats() const;
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 콜백 설정
     */
    void SetValueChangedCallback(ValueChangedCallback callback);
    
    /**
     * @brief BACnet Driver 직접 접근
     */
    Drivers::BACnetDriver* GetBACnetDriver() const {
        return bacnet_driver_.get();
    }
    bool WriteProperty(uint32_t device_id,
                      BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance,
                      BACNET_PROPERTY_ID property_id,
                      const DataValue& value,
                      uint8_t priority = BACNET_NO_PRIORITY);
    
    bool WriteObjectProperty(const std::string& object_id, 
                            const DataValue& value,
                            uint8_t priority = BACNET_NO_PRIORITY);
    
    bool WriteBACnetDataPoint(const std::string& point_id, const DataValue& value);    
    
private:
    // =============================================================================
    // 내부 구현 메서드들
    // =============================================================================
    
    bool ParseBACnetConfigFromDeviceInfo();
    PulseOne::Structs::DriverConfig CreateDriverConfigFromDeviceInfo();
    bool InitializeBACnetDriver();
    void ShutdownBACnetDriver();
    
    // ✅ 데이터 스캔 스레드 (하나만!)
    void DataScanThreadFunction();
    
    // ✅ 실제 데이터 스캔 로직
    bool PerformDataScan();
    bool ProcessDataPoints(const std::vector<DataPoint>& points);
    
    void UpdateWorkerStats(const std::string& operation, bool success);
    std::string CreateObjectId(const DataPoint& point) const;
    void SetupBACnetDriverCallbacks();
    
    // COV 처리
    void ProcessValueChangeForCOV(const std::string& object_id, 
                                 const TimestampedValue& new_value);
    bool IsValueChanged(const DataValue& previous, const DataValue& current);
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    BACnetWorkerStats worker_stats_;
    
    // ✅ 스레드 관리 (단순화)
    std::atomic<bool> thread_running_;
    std::unique_ptr<std::thread> data_scan_thread_;
    
    // ✅ 설정된 DataPoint들 (외부에서 로드됨)
    mutable std::mutex data_points_mutex_;
    std::vector<DataPoint> configured_data_points_;
    
    // 콜백 함수
    ValueChangedCallback on_value_changed_;

    // COV용 이전 값 저장
    std::map<std::string, DataValue> previous_values_;
    std::mutex previous_values_mutex_;

    std::shared_ptr<PulseOne::Drivers::BACnetServiceManager> bacnet_service_manager_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H