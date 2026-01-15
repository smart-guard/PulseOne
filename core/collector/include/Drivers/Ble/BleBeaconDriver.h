#ifndef BLE_BEACON_DRIVER_H
#define BLE_BEACON_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include <map>
#include <mutex>
#include <vector>
#include <string>

namespace PulseOne {
namespace Drivers {
namespace Ble {

class BleBeaconDriver : public PulseOne::Drivers::IProtocolDriver {
public:
    BleBeaconDriver();
    virtual ~BleBeaconDriver();

    // IProtocolDriver implementation
    bool Initialize(const Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    // Generic ReadValues (uses cached scan results)
    bool ReadValues(const std::vector<Structs::DataPoint>& points, 
                   std::vector<Structs::TimestampedValue>& out_values) override;

    // Write is not supported for Beacons
    // IProtocolDriver implementation continued
    bool Start() override;
    bool Stop() override;
    
    // Single point write (required by interface)
    bool WriteValue(const Structs::DataPoint& point, const Structs::DataValue& value) override;

    // Getters
    Enums::ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    Structs::ErrorInfo GetLastError() const override;

    // Simulation Helper
    void SimulateBeaconDiscovery(const std::string& uuid, int rssi);

private:
    bool is_connected_;
    std::mutex data_mutex_;
    std::map<std::string, int> discovered_rssi_; // UUID -> RSSI
};

} // namespace Ble
} // namespace Drivers
} // namespace PulseOne

#endif // BLE_BEACON_DRIVER_H
