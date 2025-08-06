// collector/include/Workers/Protocol/ModbusRtuWorker.h
#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

/**
 * @file ModbusRtuWorker.h - 기존 구조 기반 최종 수정
 * @brief Modbus RTU 워커 클래스 헤더 (타입 충돌만 해결)
 * @author PulseOne Development Team
 * @date 2025-08-06
 * 
 * 🔥 최소 수정사항:
 * - PulseOne::Drivers::ModbusConfig → PulseOne::Structs::DriverConfig
 * - 기존 멤버 변수명과 구조 100% 유지
 * - 기존 메서드 시그니처 유지
 */

// 기본 시스템 헤더들 (기존과 동일)
#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <future>

// PulseOne 헤더들 (기존과 동일)
#include "Workers/Base/SerialBasedWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Common/Structs.h"              // ✅ DriverConfig 포함

// ✅ 올바른 네임스페이스 - 중첩 없음
namespace PulseOne {
namespace Workers {

// 🔥 타입 별칭들 (기존 코드와 호환)
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using WorkerState = PulseOne::Enums::WorkerState;
using LogLevel = PulseOne::Enums::LogLevel;
using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;

/**
 * @brief Modbus RTU 폴링 그룹 (기존과 동일)
 */
struct ModbusRtuPollingGroup {
    uint32_t group_id;                               ///< 그룹 ID
    std::string group_name;                          ///< 그룹 이름
    int slave_id;                                    ///< 슬레이브 ID
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 레지스터 개수
    uint32_t polling_interval_ms;                    ///< 폴링 주기 (밀리초)
    bool enabled;                                    ///< 활성화 여부
    
    std::vector<PulseOne::DataPoint> data_points;    ///< 이 그룹에 속한 데이터 포인트들
    
    // 실행 시간 추적
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusRtuPollingGroup() 
        : group_id(0), group_name(""), slave_id(1)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus RTU 워커 클래스 (기존 구조 100% 유지)
 * @details SerialBasedWorker 기반의 Modbus RTU 프로토콜 구현
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    /**
     * @brief 생성자 (🔥 기존 시그니처 유지!)
     */
    explicit ModbusRtuWorker(const PulseOne::DeviceInfo& device_info);
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusRtuWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현 (기존과 동일)
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // SerialBasedWorker 인터페이스 구현 (RTU 특화)
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // =============================================================================
    // 설정 API (🔥 타입만 수정, 메서드명 유지!)
    // =============================================================================
    
    /**
     * @brief RTU 설정 구성 (🔥 타입 변경: DriverConfig 사용!)
     */
    void ConfigureModbusRtu(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief 현재 설정 반환 (🔥 타입 변경: DriverConfig 반환!)
     */
    const PulseOne::Structs::DriverConfig& GetModbusConfig() const { return modbus_config_; }
    
    // 편의 메서드들 (기존과 동일하되 내부 구현만 변경)
    void SetSlaveId(int slave_id);
    void SetResponseTimeout(int timeout_ms);

    // =============================================================================
    // RTU 특화 슬레이브 관리 (기존과 동일)
    // =============================================================================
    
    bool AddSlave(int slave_id, const std::string& device_name = "");
    bool RemoveSlave(int slave_id);
    std::shared_ptr<DeviceInfo> GetSlaveInfo(int slave_id) const;
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

    // =============================================================================
    // 폴링 그룹 관리 (기존과 동일)
    // =============================================================================
    
    uint32_t AddPollingGroup(const std::string& group_name,
                            int slave_id,
                            ModbusRegisterType register_type,
                            uint16_t start_address,
                            uint16_t register_count,
                            int polling_interval_ms = 1000);
    
    bool RemovePollingGroup(uint32_t group_id);
    bool EnablePollingGroup(uint32_t group_id, bool enabled);
    bool AddDataPointToGroup(uint32_t group_id, const PulseOne::DataPoint& data_point);

    // =============================================================================
    // 데이터 읽기/쓰기 (기존과 동일)
    // =============================================================================
    
    bool ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                             uint16_t register_count, std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_address, 
                           uint16_t register_count, std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_address, 
                   uint16_t coil_count, std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                           uint16_t input_count, std::vector<bool>& values);
    bool WriteSingleRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    bool WriteMultipleRegisters(int slave_id, uint16_t start_address, 
                               const std::vector<uint16_t>& values);
    bool WriteMultipleCoils(int slave_id, uint16_t start_address, 
                           const std::vector<bool>& values);

    // =============================================================================
    // 상태 조회 API (기존과 동일)
    // =============================================================================
    
    std::string GetModbusStats() const;
    std::string GetSerialBusStatus() const;
    std::string GetSlaveStatusList() const;
    std::string GetPollingGroupStatus() const;

    int GetSlaveId() const;
    int GetBaudRate() const;
    char GetParity() const;
    int GetDataBits() const;
    int GetStopBits() const;
    int GetFrameDelay() const;
    int GetResponseTimeout() const;
    int GetByteTimeout() const;
    int GetMaxRetries() const;    

protected:
    // =============================================================================
    // 🔥 멤버 변수들 (기존 변수명 유지하되 타입만 변경!)
    // =============================================================================
    
    // ModbusDriver 인스턴스 (기존과 동일)
    std::shared_ptr<PulseOne::Drivers::ModbusDriver> modbus_driver_;
    
    // 🔥 핵심 변경: 타입만 변경, 변수명은 기존 유지!
    PulseOne::Structs::DriverConfig modbus_config_;   ///< 기존 변수명 유지
    
    // 기존 멤버 변수들 (그대로 유지)
    std::atomic<uint32_t> next_group_id_;
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // 시리얼 버스 액세스 제어 (RTU 고유)
    mutable std::mutex bus_mutex_;
    
    // RTU 특화: 슬레이브 관리
    std::map<int, std::shared_ptr<DeviceInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    
    // 폴링 그룹 관리
    std::map<uint32_t, ModbusRtuPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;

    // =============================================================================
    // 헬퍼 메서드들 (기존과 동일)
    // =============================================================================
    
    void PollingWorkerThread();
    bool ProcessPollingGroup(ModbusRtuPollingGroup& group);
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    int CheckSlaveStatus(int slave_id);
    
    void LockBus();    // RTU 고유
    void UnlockBus();  // RTU 고유
    void LogRtuMessage(PulseOne::Enums::LogLevel level, const std::string& message);
    std::vector<PulseOne::Structs::DataPoint> CreateDataPoints(int slave_id, 
                                                               ModbusRegisterType register_type,
                                                               uint16_t start_address, 
                                                               uint16_t count);
    
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                               const std::string& key, 
                               const std::string& default_value = "");

private:
    // =============================================================================
    // 설정 및 초기화 메서드들 (기존과 동일)
    // =============================================================================
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
};

// =============================================================================
// ✅ 인라인 구현 (편의 메서드들)
// =============================================================================

inline void ModbusRtuWorker::SetSlaveId(int slave_id) {
    // 🔥 DriverConfig.properties 사용으로 변경
    modbus_config_.properties["slave_id"] = std::to_string(slave_id);
}

inline void ModbusRtuWorker::SetResponseTimeout(int timeout_ms) {
    // 🔥 DriverConfig.properties 사용으로 변경
    modbus_config_.properties["response_timeout_ms"] = std::to_string(timeout_ms);
}

} // namespace Workers  
} // namespace PulseOne

#endif // MODBUS_RTU_WORKER_H