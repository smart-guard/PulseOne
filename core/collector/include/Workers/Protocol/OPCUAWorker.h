/**
 * @file OPCUAWorker.h
 * @brief OPC UA Protocol Worker
 * @author PulseOne Development Team
 * @date 2026-01-07
 */

#ifndef OPCUA_WORKER_H
#define OPCUA_WORKER_H

#include "Drivers/Common/DriverFactory.h"
#include "Drivers/Common/IProtocolDriver.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include <atomic>
#include <memory>
#include <thread>

namespace PulseOne {
namespace Workers {

class OPCUAWorker : public BaseDeviceWorker {
public:
  explicit OPCUAWorker(const PulseOne::Structs::DeviceInfo &device_info);
  virtual ~OPCUAWorker();

  // BaseDeviceWorker Interface Implementation
  std::future<bool> Start() override;
  std::future<bool> Stop() override;
  bool EstablishConnection() override;
  bool CloseConnection() override;
  bool CheckConnection() override;

  // Write Interface override
  bool WriteDataPoint(const std::string &point_id,
                      const PulseOne::Structs::DataValue &value) override;

  // Node Browsing override
  std::vector<PulseOne::Structs::DataPoint> DiscoverDataPoints() override;

protected:
  void PollingThreadFunction();

private:
  std::unique_ptr<PulseOne::Drivers::IProtocolDriver> opcua_driver_;
  std::atomic<bool> polling_thread_running_;
  std::unique_ptr<std::thread> polling_thread_;
};

} // namespace Workers
} // namespace PulseOne

#endif // OPCUA_WORKER_H
