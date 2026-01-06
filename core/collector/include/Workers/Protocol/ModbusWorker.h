/**
 * @file ModbusWorker.h
 * @brief Unified Modbus Worker class - Handles both TCP and RTU
 * @author PulseOne Development Team
 * @date 2026-01-05
 */

#ifndef MODBUS_WORKER_H
#define MODBUS_WORKER_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>

#include "Workers/Base/BaseDeviceWorker.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Common/IProtocolDriver.h"

namespace PulseOne {
namespace Workers {

using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;
using DataQuality = PulseOne::Enums::DataQuality;

/**
 * @brief Modbus Polling Group
 */
struct ModbusPollingGroup {
    uint32_t group_id;
    uint8_t slave_id;
    ModbusRegisterType register_type;
    uint16_t start_address;
    uint16_t register_count;
    uint32_t polling_interval_ms;
    bool enabled;
    
    std::vector<DataPoint> data_points;
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusPollingGroup() 
        : group_id(0), slave_id(1)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Unified Modbus Worker class
 */
class ModbusWorker : public BaseDeviceWorker {
public:
    explicit ModbusWorker(const DeviceInfo& device_info);
    virtual ~ModbusWorker();

    // BaseDeviceWorker Interface Implementation
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;
    bool SendKeepAlive() override;

    // Write Interface
    bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;

protected:
    // Internal methods
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
    
    void PollingThreadFunction();
    bool ProcessPollingGroup(const ModbusPollingGroup& group);
    size_t CreatePollingGroupsFromDataPoints(const std::vector<DataPoint>& data_points);
    
    // Address and Type helpers
    bool ParseModbusAddress(const DataPoint& data_point, uint8_t& slave_id, 
                           ModbusRegisterType& register_type, uint16_t& address);
    
    // 32-bit and Endianness Helpers
    uint16_t GetDataPointRegisterCount(const DataPoint& point) const;
    DataValue ConvertRegistersToValue(const std::vector<uint16_t>& registers, const DataPoint& point) const;
    std::vector<uint16_t> ConvertValueToRegisters(const DataValue& value, const DataPoint& point) const;
    
    // Helper Methods
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                               const std::string& key, 
                               const std::string& default_value = "") const;
    
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);

    // Pipeline communication
    bool SendReadResultToPipeline(const std::vector<uint16_t>& values, uint16_t start_address,
                                 ModbusRegisterType register_type, int slave_id,
                                 const std::vector<DataPoint>& group_points);
    bool SendDiscreteReadResultToPipeline(const std::vector<bool>& values, uint16_t start_address,
                                       ModbusRegisterType register_type, int slave_id);

private:
    std::unique_ptr<PulseOne::Drivers::IProtocolDriver> modbus_driver_;
    PulseOne::Structs::DriverConfig modbus_config_;
    
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
    // uint32_t next_group_id_; // Removed
    
    // std::map<uint32_t, ModbusPollingGroup> polling_groups_; // Removed
    mutable std::mutex polling_groups_mutex_; // Keep for now or remove? Remove.
};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_WORKER_H
