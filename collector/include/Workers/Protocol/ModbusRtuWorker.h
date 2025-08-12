// =============================================================================
// collector/include/Workers/Protocol/ModbusRtuWorker.h - 완성본
// =============================================================================

#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

/**
 * @file ModbusRtuWorker.h - 실제 ModbusDriver 연동 완성본
 * @brief Modbus RTU 워커 클래스 (실제 libmodbus 기반)
 * @author PulseOne Development Team
 * @date 2025-08-08
 * @version 7.0.0 - 실제 ModbusDriver 연동 완성
 */

// 시스템 헤더들
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>

// PulseOne 헤더들
#include "Workers/Base/SerialBasedWorker.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Modbus/ModbusDriver.h"  // 🔥 실제 ModbusDriver 포함
namespace PulseOne {
namespace Workers {

// 타입 별칭
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;

/**
 * @brief Modbus RTU 폴링 그룹
 */
struct ModbusRtuPollingGroup {
    uint32_t group_id;
    std::string group_name;
    int slave_id;
    ModbusRegisterType register_type;
    uint16_t start_address;
    uint16_t register_count;
    uint32_t polling_interval_ms;
    bool enabled;
    
    std::vector<DataPoint> data_points;
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusRtuPollingGroup() 
        : group_id(0), slave_id(1)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus RTU 워커 클래스 - 실제 ModbusDriver 사용
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    explicit ModbusRtuWorker(const DeviceInfo& device_info);
    virtual ~ModbusRtuWorker();

    // BaseDeviceWorker 인터페이스
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // SerialBasedWorker 인터페이스
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // 설정 API
    void ConfigureModbusRtu(const PulseOne::Structs::DriverConfig& config);
    const PulseOne::Structs::DriverConfig& GetModbusConfig() const { return modbus_config_; }

    // RTU 특화 슬레이브 관리
    bool AddSlave(int slave_id, const std::string& device_name = "");
    bool RemoveSlave(int slave_id);
    std::shared_ptr<DeviceInfo> GetSlaveInfo(int slave_id) const;
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

    // 폴링 그룹 관리
    uint32_t AddPollingGroup(const std::string& group_name, int slave_id,
                            ModbusRegisterType register_type, uint16_t start_address,
                            uint16_t register_count, int polling_interval_ms = 1000);
    bool RemovePollingGroup(uint32_t group_id);
    bool EnablePollingGroup(uint32_t group_id, bool enabled);
    bool AddDataPointToGroup(uint32_t group_id, const DataPoint& data_point);

    // 데이터 읽기/쓰기 (실제 ModbusDriver 기반)
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
    bool SendModbusRtuDataToPipeline(const std::vector<uint16_t>& raw_values, 
                                    uint16_t start_address,
                                    const std::string& register_type,
                                    uint32_t priority = 0);
    bool SendModbusRtuBoolDataToPipeline(const std::vector<bool>& raw_values,
                                        uint16_t start_address,
                                        const std::string& register_type,
                                        uint32_t priority = 0);

    // 상태 조회
    std::string GetModbusStats() const;
    std::string GetSerialBusStatus() const;
    std::string GetSlaveStatusList() const;

    // 설정 접근자
    int GetSlaveId() const;
    int GetBaudRate() const;
    char GetParity() const;
    int GetDataBits() const;
    int GetStopBits() const;
    PulseOne::Drivers::ModbusDriver* GetModbusDriver() const {
        return modbus_driver_.get();
    }
private:
    // 🔥 실제 ModbusDriver 인스턴스
    std::unique_ptr<PulseOne::Drivers::ModbusDriver> modbus_driver_;
    
    // 설정 및 상태
    PulseOne::Structs::DriverConfig modbus_config_;
    std::atomic<uint32_t> next_group_id_;
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // RTU 특화: 버스 접근 제어
    mutable std::mutex bus_mutex_;
    
    // 슬레이브 및 폴링 그룹 관리
    std::map<int, std::shared_ptr<DeviceInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    std::map<uint32_t, ModbusRtuPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;

    // 내부 메서드들
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
    void PollingWorkerThread();
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    int CheckSlaveStatus(int slave_id);
    void LockBus();
    void UnlockBus();
    
    // 유틸리티
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                               const std::string& key, 
                               const std::string& default_value = "") const;
};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_RTU_WORKER_H

// =============================================================================
// collector/src/Workers/Protocol/ModbusRtuWorker.cpp - 완성본
// =============================================================================