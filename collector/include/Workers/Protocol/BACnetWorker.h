/**
 * @file BACnetWorker.h
 * @brief BACnet 프로토콜 워커 클래스 - 🔥 모든 에러 수정 완료본
 * @author PulseOne Development Team
 * @date 2025-08-08
 * @version 5.0.0
 * 
 * 🔥 수정사항:
 * 1. 콜백 타입 불일치 완전 해결
 * 2. 누락된 메서드 선언 추가
 * 3. 스레드 함수명 통일
 * 4. 필드명 실제 구조와 정확히 일치
 */

#ifndef BACNET_WORKER_H
#define BACNET_WORKER_H

#include "Workers/Base/UdpBasedWorker.h"                    
#include "Common/Structs.h"                                 
#include "Drivers/Bacnet/BACnetDriver.h"                    
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
// 🔥 타입 별칭 정의 - 실제 프로젝트 구조 사용
// =============================================================================

using DataPoint = PulseOne::Structs::DataPoint;              
using DeviceInfo = PulseOne::Structs::DeviceInfo;            
using TimestampedValue = PulseOne::Structs::TimestampedValue; 
using DataValue = PulseOne::Structs::DataValue;              

// =============================================================================
// 🔥 BACnet 헬퍼 함수들 - 실제 필드명 사용
// =============================================================================

/**
 * @brief BACnet 객체를 DataPoint로 생성하는 헬퍼 함수
 */
inline DataPoint CreateBACnetDataPoint(
    uint32_t device_id,
    uint16_t object_type, 
    uint32_t object_instance,
    const std::string& object_name = "",
    const std::string& description = "",
    const std::string& units = "") {
    
    DataPoint point;
    
    // ✅ 실제 필드명 사용
    point.device_id = std::to_string(device_id);              
    point.address = static_cast<int>(object_instance);        
    point.name = object_name.empty() ? 
                 ("Object_" + std::to_string(object_instance)) : object_name;
    point.description = description;
    point.unit = units;
    point.is_enabled = true;  // ✅ enabled → is_enabled
    
    // BACnet 객체 타입에 따른 데이터 타입 결정
    switch (object_type) {
        case 0:  case 1:  case 2:   // AI, AO, AV
            point.data_type = "float";
            break;
        case 3:  case 4:  case 5:   // BI, BO, BV
            point.data_type = "bool";
            break;
        case 13: case 14: case 19:  // MI, MO, MV
            point.data_type = "int";
            break;
        default:
            point.data_type = "string";
            break;
    }
    
    // ✅ BACnet 특화 정보를 protocol_params에 저장 (not properties)
    point.protocol_params["bacnet_object_type"] = std::to_string(object_type);
    point.protocol_params["bacnet_device_id"] = std::to_string(device_id);
    point.protocol_params["bacnet_instance"] = std::to_string(object_instance);
    
    // 고유 ID 생성
    point.id = std::to_string(device_id) + ":" + 
               std::to_string(object_type) + ":" + 
               std::to_string(object_instance);
               
    return point;
}

/**
 * @brief DataPoint에서 BACnet 정보 추출 - 실제 필드명 사용
 */
inline bool GetBACnetInfoFromDataPoint(const DataPoint& point, 
                                      uint32_t& device_id,
                                      uint16_t& object_type, 
                                      uint32_t& object_instance) {
    try {
        // ✅ protocol_params에서 추출 (not properties)
        auto it_dev = point.protocol_params.find("bacnet_device_id");
        auto it_type = point.protocol_params.find("bacnet_object_type");
        auto it_inst = point.protocol_params.find("bacnet_instance");
        
        if (it_dev != point.protocol_params.end() && 
            it_type != point.protocol_params.end() && 
            it_inst != point.protocol_params.end()) {
            
            device_id = std::stoul(it_dev->second);
            object_type = static_cast<uint16_t>(std::stoul(it_type->second));
            object_instance = std::stoul(it_inst->second);
            return true;
        }
        
        // 대안: 기본 필드 사용
        device_id = std::stoul(point.device_id);
        object_instance = static_cast<uint32_t>(point.address);
        object_type = 0; // 기본값
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

/**
 * @brief BACnet 객체 타입 이름 반환
 */
inline std::string GetBACnetObjectTypeName(uint16_t object_type) {
    switch (object_type) {
        case 0: return "ANALOG_INPUT";
        case 1: return "ANALOG_OUTPUT";
        case 2: return "ANALOG_VALUE";
        case 3: return "BINARY_INPUT";
        case 4: return "BINARY_OUTPUT";
        case 5: return "BINARY_VALUE";
        case 8: return "DEVICE";
        case 13: return "MULTI_STATE_INPUT";
        case 14: return "MULTI_STATE_OUTPUT";
        case 19: return "MULTI_STATE_VALUE";
        default: return "UNKNOWN_" + std::to_string(object_type);
    }
}

// =============================================================================
// 🔥 BACnet 워커 통계 (필수 최소한만)
// =============================================================================

struct BACnetWorkerStats {
    std::atomic<uint64_t> discovery_attempts{0};      
    std::atomic<uint64_t> devices_discovered{0};      
    std::atomic<uint64_t> polling_cycles{0};          
    std::atomic<uint64_t> read_operations{0};         
    std::atomic<uint64_t> write_operations{0};        
    std::atomic<uint64_t> failed_operations{0};       
    std::atomic<uint64_t> cov_notifications{0};       
    
    std::chrono::system_clock::time_point start_time;     
    std::chrono::system_clock::time_point last_reset;     
    
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

// =============================================================================
// 🔥 콜백 함수 타입들 - 1:1 구조 (자신의 디바이스용)
// =============================================================================

using ObjectDiscoveredCallback = std::function<void(const DataPoint&)>;  // 자신의 객체 발견
using ValueChangedCallback = std::function<void(const std::string& object_id, const TimestampedValue&)>;

// =============================================================================
// 🔥 BACnetWorker 클래스 - 실제 프로젝트 구조 준수
// =============================================================================

/**
 * @class BACnetWorker
 * @brief BACnet 프로토콜 워커 클래스 (실제 프로젝트 구조 완전 준수)
 */
class BACnetWorker : public UdpBasedWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    explicit BACnetWorker(const DeviceInfo& device_info);
    virtual ~BACnetWorker();

    // =============================================================================
    // 🔥 UdpBasedWorker 순수 가상 함수 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;
    bool ProcessReceivedPacket(const UdpPacket& packet) override;
    
    // =============================================================================
    // 🔥 BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    
    // =============================================================================
    // BACnet 공개 기능들 - 1:1 구조 (1 Worker = 1 Device)
    // =============================================================================
    
    std::string GetBACnetWorkerStats() const;
    void ResetBACnetWorkerStats();
    
    /**
     * @brief 자신의 디바이스 정보 반환 (1:1 구조)
     */
    const DeviceInfo& GetDeviceInfo() const { return device_info_; }
    
    /**
     * @brief 자신의 BACnet 객체들 조회 (1:1 구조)
     */
    std::vector<DataPoint> GetDiscoveredObjects() const;
    
    /**
     * @brief 자신의 디바이스 JSON 정보
     */
    std::string GetDeviceInfoAsJson() const;
    
    /**
     * @brief Discovery 제어
     */
    bool StartObjectDiscovery();
    void StopObjectDiscovery();
    
    /**
     * @brief 콜백 설정 (자신의 디바이스용) - 🔥 올바른 타입 사용
     */
    void SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback);
    void SetValueChangedCallback(ValueChangedCallback callback);
    
    Drivers::BACnetDriver* GetBACnetDriver() const {
        return bacnet_driver_.get();
    }    
    
private:
    // =============================================================================
    // 🔥 내부 구현 메서드들 - DeviceInfo 기반
    // =============================================================================
    
    bool ParseBACnetConfigFromDeviceInfo();
    PulseOne::Structs::DriverConfig CreateDriverConfigFromDeviceInfo();
    bool InitializeBACnetDriver();
    void ShutdownBACnetDriver();
    
    // 🔥 수정: 스레드 함수명 통일
    void ObjectDiscoveryThreadFunction();  // 객체 발견 스레드 (자신의 디바이스)
    void PollingThreadFunction();
    
    bool PerformObjectDiscovery();         // 자신의 객체들 발견
    bool PerformPolling();
    
    // 🔥 추가: 누락된 메서드 선언들
    bool ProcessDataPoints(const std::vector<DataPoint>& points);
    bool ProcessBACnetDataPoints(const std::vector<DataPoint>& bacnet_points);
    bool DiscoverMyObjects(std::vector<DataPoint>& data_points);  // 자신의 객체들 발견
    
    void UpdateWorkerStats(const std::string& operation, bool success);
    std::string CreateObjectId(const DataPoint& point) const;
    
    // =============================================================================
    // 멤버 변수들 - 표준 구조체만 사용
    // =============================================================================
    
    std::unique_ptr<Drivers::BACnetDriver> bacnet_driver_;
    BACnetWorkerStats worker_stats_;
    
    std::atomic<bool> threads_running_;
    std::unique_ptr<std::thread> object_discovery_thread_;  // 객체 발견 스레드
    std::unique_ptr<std::thread> polling_thread_;
    
    // 🔥 1:1 구조: 자신의 객체들만 관리
    mutable std::mutex objects_mutex_;
    std::vector<DataPoint> my_objects_;                      // 자신의 BACnet 객체들
    
    // 콜백 함수들 (1:1 구조)
    ObjectDiscoveredCallback on_object_discovered_;
    ValueChangedCallback on_value_changed_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_WORKER_H