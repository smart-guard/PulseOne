/**
 * @file ModbusWorker.h
 * @brief Unified Modbus Worker class - Handles both TCP and RTU
 * @author PulseOne Development Team
 * @date 2026-01-05
 */

#ifndef MODBUS_WORKER_H
#define MODBUS_WORKER_H

#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Drivers/Common/IProtocolDriver.h"
#include "Workers/Base/BaseDeviceWorker.h"

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
      : group_id(0), slave_id(1),
        register_type(ModbusRegisterType::HOLDING_REGISTER), start_address(0),
        register_count(1), polling_interval_ms(1000), enabled(true),
        last_poll_time(std::chrono::system_clock::now()),
        next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Unified Modbus Worker class
 */
class ModbusWorker : public BaseDeviceWorker {
public:
  explicit ModbusWorker(const DeviceInfo &device_info);
  virtual ~ModbusWorker();

  // BaseDeviceWorker Interface Implementation
  std::future<bool> Start() override;
  std::future<bool> Stop() override;
  bool EstablishConnection() override;
  bool CloseConnection() override;
  bool CheckConnection() override;
  bool SendKeepAlive() override;

  // Write Interface
  bool WriteDataPoint(const std::string &point_id,
                      const DataValue &value) override;

  // Test Helper
  const PulseOne::Structs::DriverConfig &GetDriverConfig() const {
    return modbus_config_;
  }

protected:
  // Internal methods
  bool ParseModbusConfig();
  bool InitializeModbusDriver();
  void SetupDriverCallbacks();

  void PollingThreadFunction();
  bool ProcessPollingGroup(const ModbusPollingGroup &group);
  size_t
  CreatePollingGroupsFromDataPoints(const std::vector<DataPoint> &data_points);

  // Address and Type helpers
  bool ParseModbusAddress(const DataPoint &data_point, uint8_t &slave_id,
                          ModbusRegisterType &register_type, uint16_t &address);

  // 32-bit and Endianness Helpers
  uint16_t GetDataPointRegisterCount(const DataPoint &point) const;
  DataValue ConvertRegistersToValue(const std::vector<uint16_t> &registers,
                                    const DataPoint &point) const;
  std::vector<uint16_t> ConvertValueToRegisters(const DataValue &value,
                                                const DataPoint &point) const;

  // Helper Methods
  std::string
  GetPropertyValue(const std::map<std::string, std::string> &properties,
                   const std::string &key,
                   const std::string &default_value = "") const;

  std::optional<DataPoint> FindDataPointById(const std::string &point_id);

  // Pipeline communication
  bool SendReadResultToPipeline(const std::vector<uint16_t> &values,
                                uint16_t start_address,
                                ModbusRegisterType register_type, int slave_id,
                                const std::vector<DataPoint> &group_points);
  bool SendDiscreteReadResultToPipeline(const std::vector<bool> &values,
                                        uint16_t start_address,
                                        ModbusRegisterType register_type,
                                        int slave_id);

private:
  std::unique_ptr<PulseOne::Drivers::IProtocolDriver> modbus_driver_;
  PulseOne::Structs::DriverConfig modbus_config_;

  std::atomic<bool> polling_thread_running_;
  std::unique_ptr<std::thread> polling_thread_;

  // ==========================================================================
  // 이중 폴링 그룹
  // fast_points_: polling_interval_ms > 0 AND < device_interval_ms 포인트
  // slow_points_: 그 외 (device_interval_ms 기준 N번에 1번 읽음)
  // ==========================================================================
  std::vector<DataPoint> fast_points_; // 개별 주기 설정된 포인트 (고속)
  std::vector<DataPoint> slow_points_; // 장치 기본 주기 포인트 (저속)
  mutable std::mutex polling_groups_mutex_;
  uint64_t slow_loop_counter_ = 0; // 저속 그룹 루프 카운터

  void BuildPollingGroups(const std::vector<DataPoint> &points);

  // 포인트 런타임 재로드 시 fast/slow 그룹도 재분류
  void ReloadDataPoints(
      const std::vector<PulseOne::Structs::DataPoint> &new_points) override;
};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_WORKER_H
