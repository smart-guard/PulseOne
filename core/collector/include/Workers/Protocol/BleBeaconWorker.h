#pragma once

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Ble/BleBeaconDriver.h"
#include <memory>

namespace PulseOne {
namespace Workers {

class BleBeaconWorker : public BaseDeviceWorker {
public:
    explicit BleBeaconWorker(const Structs::DeviceInfo& device_info);
    virtual ~BleBeaconWorker();

    // BaseDeviceWorker implementation
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;

    // Worker specific
    void PollingThreadFunction();
    void RegisterServices(); // Not an override

    // Helper for testing
    Drivers::Ble::BleBeaconDriver* GetDriver() { return ble_driver_.get(); }

private:
   std::unique_ptr<Drivers::Ble::BleBeaconDriver> ble_driver_;
   
   // Thread management
   std::atomic<bool> is_running_{false};
   std::thread worker_thread_;
};

} // namespace Workers
} // namespace PulseOne
