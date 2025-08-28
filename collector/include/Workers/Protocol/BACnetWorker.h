/**
 * @file BACnetWorker.h - 컴파일 에러 완전 수정
 * @brief extra qualification 및 타입 불일치 해결
 */

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
     * @brief COV (Change of Value) 알림을 파이프라인 전송
     */
    bool SendCOVNotificationToPipeline(const std::string& object_id,
                                      const DataValue& new_value,
                                      const DataValue& previous_value = DataValue{});
    
    // 🔥 문제 1 해결: extra qualification 제거
    PulseOne::Structs::DataPoint* FindDataPointByObjectId(const std::string& object_id);
    
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
    /**
     * @brief 데이터 포인트에 값 쓰기 (통합 인터페이스)
     * @param point_id 데이터 포인트 ID
     * @param value 쓸 값 (DataValue variant)
     * @return 성공 시 true
     */
    virtual bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;
    
    /**
     * @brief 아날로그 출력 제어 (범용 제어 인터페이스)
     * @param output_id 출력 ID (point_id 또는 BACnet 객체 ID)
     * @param value 출력 값 (0.0-100.0% 또는 원시값)
     * @return 성공 시 true
     */
    virtual bool WriteAnalogOutput(const std::string& output_id, double value) override;
    
    /**
     * @brief 디지털 출력 제어 (범용 제어 인터페이스)
     * @param output_id 출력 ID (point_id 또는 BACnet 객체 ID)
     * @param value 출력 값 (true/false)
     * @return 성공 시 true
     */
    virtual bool WriteDigitalOutput(const std::string& output_id, bool value) override;
    
    /**
     * @brief 세트포인트 설정 (아날로그 출력의 별칭)
     * @param setpoint_id 세트포인트 ID
     * @param value 설정값
     * @return 성공 시 true
     */
    virtual bool WriteSetpoint(const std::string& setpoint_id, double value) override;
    
    // =============================================================================
    // 범용 장비 제어 인터페이스 (BACnet 특화)
    // =============================================================================
    
    /**
     * @brief 디지털 장비 제어 (팬, 펌프, 댐퍼, 밸브 등)
     * @param device_id BACnet 장비 ID
     * @param enable 장비 활성화/비활성화
     * @return 성공 시 true
     */
    virtual bool ControlDigitalDevice(const std::string& device_id, bool enable) override;
    
    /**
     * @brief 아날로그 장비 제어 (VAV, VFD, 아날로그 밸브 등)
     * @param device_id BACnet 장비 ID
     * @param value 제어값 (일반적으로 0.0-100.0%)
     * @return 성공 시 true
     */
    virtual bool ControlAnalogDevice(const std::string& device_id, double value) override;
    
    // =============================================================================
    // BACnet 특화 편의 래퍼 함수들 (인라인 구현)
    // =============================================================================
    
    /**
     * @brief VAV (Variable Air Volume) 댐퍼 제어
     * @param vav_id VAV 유닛 ID
     * @param position 댐퍼 위치 (0.0-100.0%)
     * @return 성공 시 true
     */
    inline bool ControlVAV(const std::string& vav_id, double position) {
        return ControlAnalogDevice(vav_id, position);
    }
    
    /**
     * @brief AHU (Air Handling Unit) 팬 제어
     * @param ahu_id AHU 유닛 ID
     * @param enable 팬 시작/정지
     * @return 성공 시 true
     */
    inline bool ControlAHU(const std::string& ahu_id, bool enable) {
        return ControlDigitalDevice(ahu_id, enable);
    }
    
    /**
     * @brief 냉각밸브 제어
     * @param valve_id 냉각밸브 ID
     * @param position 밸브 위치 (0.0-100.0%)
     * @return 성공 시 true
     */
    inline bool ControlChilledWaterValve(const std::string& valve_id, double position) {
        return ControlAnalogDevice(valve_id, position);
    }
    
    /**
     * @brief 가열밸브 제어
     * @param valve_id 가열밸브 ID
     * @param position 밸브 위치 (0.0-100.0%)
     * @return 성공 시 true
     */
    inline bool ControlHeatingValve(const std::string& valve_id, double position) {
        return ControlAnalogDevice(valve_id, position);
    }
    
    /**
     * @brief 온도 설정점 변경
     * @param zone_id 존 ID (또는 컨트롤러 ID)
     * @param temperature 설정 온도 (°C)
     * @return 성공 시 true
     */
    inline bool SetTemperatureSetpoint(const std::string& zone_id, double temperature) {
        return WriteSetpoint(zone_id, temperature);
    }
    
    /**
     * @brief 습도 설정점 변경
     * @param zone_id 존 ID (또는 컨트롤러 ID)
     * @param humidity 설정 습도 (%RH)
     * @return 성공 시 true
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
    bool WriteDataPointValue(const std::string& point_id, const DataValue& value);
    bool ParseBACnetObjectId(const std::string& object_id, uint32_t& device_id, 
                            BACNET_OBJECT_TYPE& object_type, uint32_t& object_instance);
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);
    void LogWriteOperation(const std::string& object_id, const DataValue& value,
                          const std::string& property_name, bool success);    
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

    // 🔥 문제 2 해결: COV용 이전 값 저장 - 키를 std::string으로 변경
    std::map<std::string, DataValue> previous_values_;
    std::mutex previous_values_mutex_;

    std::shared_ptr<PulseOne::Drivers::BACnetServiceManager> bacnet_service_manager_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H