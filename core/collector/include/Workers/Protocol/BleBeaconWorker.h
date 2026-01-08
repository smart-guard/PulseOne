#ifndef WORKERS_PROTOCOL_BLE_BEACON_WORKER_H
#define WORKERS_PROTOCOL_BLE_BEACON_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Ble/BleBeaconDriver.h"

namespace PulseOne {
namespace Workers {

class BleBeaconWorker : public BaseDeviceWorker {
public:
    explicit BleBeaconWorker(const PulseOne::Structs::DeviceInfo& device_info);
    virtual ~BleBeaconWorker() = default;

    // BaseDeviceWorker Interface Implementation
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;

protected:
    void PollingThreadFunction();

private:
    std::unique_ptr<PulseOne::Drivers::Ble::BleBeaconDriver> ble_driver_;
    std::atomic<bool> is_started_{false};
    std::atomic<bool> polling_thread_running_{false};
    std::unique_ptr<std::thread> polling_thread_;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_BLE_BEACON_WORKER_H
