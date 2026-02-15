#ifndef PULSEONE_WORKERS_PROTOCOL_GENERIC_DEVICE_WORKER_H
#define PULSEONE_WORKERS_PROTOCOL_GENERIC_DEVICE_WORKER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace PulseOne {
namespace Workers {
namespace Protocol {

/**
 * @brief Generic Worker that wraps any IProtocolDriver plugin.
 */
class GenericDeviceWorker : public BaseDeviceWorker {
public:
  GenericDeviceWorker(const PulseOne::Structs::DeviceInfo &device_info);
  virtual ~GenericDeviceWorker();

  // BaseDeviceWorker pure virtual overrides
  std::future<bool> Start() override;
  std::future<bool> Stop() override;
  bool EstablishConnection() override;
  bool CloseConnection() override;
  bool CheckConnection() override;

  // Write Interface
  bool WriteDataPoint(const std::string &point_id,
                      const PulseOne::Structs::DataValue &value) override;

protected:
  // Helpers
  void SetupDriver();
  bool PerformRead();
  bool PerformWrite(const PulseOne::Structs::DataPoint &point,
                    const PulseOne::Structs::DataValue &value);

private:
  void PollingThreadMain();
  std::string internal_protocol_name_;
  std::unique_ptr<PulseOne::Drivers::IProtocolDriver> driver_;
  std::unique_ptr<std::thread> polling_thread_;
  std::atomic<bool> polling_thread_running_{false};
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // PULSEONE_WORKERS_PROTOCOL_GENERIC_DEVICE_WORKER_H
