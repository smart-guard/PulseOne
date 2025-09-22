/**
 * @file BACnetWorker.h - 최적화 및 버그 수정 완성본
 * @brief Windows/Linux 크로스 플랫폼 + 성능 최적화 + 메모리 관리
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

// =============================================================================
// 플랫폼 호환성 헤더 (가장 먼저!)
// =============================================================================
#include "Platform/PlatformCompat.h"

// =============================================================================
// 기본 헤더들
// =============================================================================
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <optional>

// =============================================================================
// PulseOne 헤더들 (순서 중요!)
// =============================================================================
#include "Workers/Base/UdpBasedWorker.h"                    
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetTypes.h"                                 
#include "Drivers/Bacnet/BACnetDriver.h"  // 독립객체 헤더
#include "Drivers/Bacnet/BACnetServiceManager.h"                  
#include "Common/DriverStatistics.h"

namespace PulseOne {
namespace Workers {

// =============================================================================
// 타입 별칭 정의 (충돌 방지)
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
// BACnetWorker 클래스 - 독립 BACnetDriver 사용 + 최적화
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
    // 데이터 스캔 + 파이프라인 전송 기능들
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
     * @brief COV (Change of Value) 알림을 파이프라인 전송
     */
    bool SendCOVNotificationToPipeline(const std::string& object_id,
                                      const DataValue& new_value,
                                      const DataValue& previous_value = DataValue{});
    
    /**
     * @brief DataPoint ID로 객체 검색 (호환성 유지)
     */
    PulseOne::Structs::DataPoint* FindDataPointByObjectId(const std::string& object_id);
    
    /**
     * @brief 최적화된 DataPoint 검색 (새로 추가)
     */
    PulseOne::Structs::DataPoint* FindDataPointByObjectIdOptimized(const std::string& object_id);
    
    // =============================================================================
    // 설정 및 상태 관리
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
     * @brief 통계 조회 (메모리 사용량 정보 포함)
     */
    std::string GetBACnetWorkerStats() const;
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 콜백 설정
     */
    void SetValueChangedCallback(ValueChangedCallback callback);
    
    /**
     * @brief BACnet Driver 직접 접근 (독립객체)
     */
    PulseOne::Drivers::BACnetDriver* GetBACnetDriver() const {
        return bacnet_driver_.get();
    }
    
    // =============================================================================
    // 메모리 최적화 기능들 (새로 추가)
    // =============================================================================
    
    /**
     * @brief DataPoint 인덱스 재구성 (성능 최적화)
     */
    void RebuildDataPointIndex();
    
    /**
     * @brief 이전값 캐시 정리 (메모리 최적화)
     */
    void CleanupPreviousValues();
    
    // =============================================================================
    // BACnet 제어 기능들
    // =============================================================================
    
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
    
    // =============================================================================
    // BaseDeviceWorker Write 인터페이스 구현
    // =============================================================================
    
    /**
     * @brief 데이터 포인트에 값 쓰기 (통합 인터페이스)
     */
    virtual bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;
    
    /**
     * @brief 아날로그 출력 제어 (범용 제어 인터페이스)
     */
    virtual bool WriteAnalogOutput(const std::string& output_id, double value) override;
    
    /**
     * @brief 디지털 출력 제어 (범용 제어 인터페이스)
     */
    virtual bool WriteDigitalOutput(const std::string& output_id, bool value) override;
    
    /**
     * @brief 세트포인트 설정 (아날로그 출력의 별칭)
     */
    virtual bool WriteSetpoint(const std::string& setpoint_id, double value) override;
    
    // =============================================================================
    // 범용 장비 제어 인터페이스 (BACnet 특화)
    // =============================================================================
    
    /**
     * @brief 디지털 장비 제어 (팬, 펌프, 댐퍼, 밸브 등)
     */
    virtual bool ControlDigitalDevice(const std::string& device_id, bool enable) override;
    
    /**
     * @brief 아날로그 장비 제어 (VAV, VFD, 아날로그 밸브 등)
     */
    virtual bool ControlAnalogDevice(const std::string& device_id, double value) override;
    
    // =============================================================================
    // BACnet 특화 편의 래퍼 함수들 (인라인 구현)
    // =============================================================================
    
    /**
     * @brief VAV (Variable Air Volume) 댐퍼 제어
     */
    inline bool ControlVAV(const std::string& vav_id, double position) {
        return ControlAnalogDevice(vav_id, position);
    }
    
    /**
     * @brief AHU (Air Handling Unit) 팬 제어
     */
    inline bool ControlAHU(const std::string& ahu_id, bool enable) {
        return ControlDigitalDevice(ahu_id, enable);
    }
    
    /**
     * @brief 냉각밸브 제어
     */
    inline bool ControlChilledWaterValve(const std::string& valve_id, double position) {
        return ControlAnalogDevice(valve_id, position);
    }
    
    /**
     * @brief 가열밸브 제어
     */
    inline bool ControlHeatingValve(const std::string& valve_id, double position) {
        return ControlAnalogDevice(valve_id, position);
    }
    
    /**
     * @brief 온도 설정점 변경
     */
    inline bool SetTemperatureSetpoint(const std::string& zone_id, double temperature) {
        return WriteSetpoint(zone_id, temperature);
    }
    
    /**
     * @brief 습도 설정점 변경
     */
    inline bool SetHumiditySetpoint(const std::string& zone_id, double humidity) {
        return WriteSetpoint(zone_id, humidity);
    }    
    
private:
    // =============================================================================
    // 내부 구현 메서드들
    // =============================================================================
    
    bool ParseBACnetConfigFromDeviceInfo();
    PulseOne::Structs::DriverConfig CreateDriverConfigFromDeviceInfo();
    bool InitializeBACnetDriver();
    void ShutdownBACnetDriver();
    
    // 데이터 스캔 스레드
    void DataScanThreadFunction();
    
    // 실제 데이터 스캔 로직
    bool PerformDataScan();
    bool ProcessDataPoints(const std::vector<DataPoint>& points);
    
    void UpdateWorkerStats(const std::string& operation, bool success);
    std::string CreateObjectId(const DataPoint& point) const;
    void SetupBACnetDriverCallbacks();
    
    // COV 처리
    void ProcessValueChangeForCOV(const std::string& object_id, 
                                 const TimestampedValue& new_value);
    bool IsValueChanged(const DataValue& previous, const DataValue& current);
    
    // 제어 관련
    bool WriteDataPointValue(const std::string& point_id, const DataValue& value);
    bool ParseBACnetObjectId(const std::string& object_id, uint32_t& device_id, 
                            BACNET_OBJECT_TYPE& object_type, uint32_t& object_instance);
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);
    void LogWriteOperation(const std::string& object_id, const DataValue& value,
                          const std::string& property_name, bool success);    
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 독립 BACnetDriver 객체 (싱글톤 아님!)
    std::unique_ptr<PulseOne::Drivers::BACnetDriver> bacnet_driver_;
    
    // BACnet 서비스 매니저
    std::shared_ptr<PulseOne::Drivers::BACnetServiceManager> bacnet_service_manager_;
    
    // 워커 통계
    BACnetWorkerStats worker_stats_;
    
    // 스레드 관리
    std::atomic<bool> thread_running_;
    std::unique_ptr<std::thread> data_scan_thread_;
    
    // 메모리 정리 카운터 (새로 추가)
    std::atomic<uint32_t> cleanup_timer_;
    
    // 설정된 DataPoint들 (외부에서 로드됨)
    mutable std::mutex data_points_mutex_;
    std::vector<DataPoint> configured_data_points_;
    
    // 성능 최적화: DataPoint 빠른 검색을 위한 인덱스 (새로 추가)
    std::unordered_map<std::string, PulseOne::Structs::DataPoint*> datapoint_index_;
    
    // 콜백 함수
    ValueChangedCallback on_value_changed_;

    // COV용 이전 값 저장 (스레드 안전하게 보호됨)
    std::map<std::string, DataValue> previous_values_;
    mutable std::mutex previous_values_mutex_;  // 스레드 안전성 강화
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H