#ifndef BLE_BEACON_DRIVER_H
#define BLE_BEACON_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/Enums.h"
#include "Logging/LogManager.h"
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace PulseOne {
namespace Drivers {
namespace Ble {

struct BeaconInfo {
    std::string uuid;
    int rssi;
    int tx_power;
    std::chrono::system_clock::time_point last_seen;
};

class BleBeaconDriver : public IProtocolDriver {
public:
    BleBeaconDriver();
    virtual ~BleBeaconDriver();

    // IProtocolDriver implementation
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    bool Start() override { return Connect(); }
    bool Stop() override { return Disconnect(); }
    
    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points, 
              std::vector<PulseOne::Structs::TimestampedValue>& values) override;
              
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
               const PulseOne::Structs::DataValue& value) override;

    PulseOne::Enums::ProtocolType GetProtocolType() const override { return PulseOne::Enums::ProtocolType::BLE_BEACON; }
    PulseOne::Enums::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override { return last_error_; }

    // Not supported
    bool Subscribe(const std::string& topic, int qos = 0) override { return false; }
    bool Unsubscribe(const std::string& topic) override { return false; }

    // Simulation helper for testing
    void SimulateBeaconDiscovery(const std::string& uuid, int rssi);

private:
    void ScanningLoop();
    void CleanUpStaleBeacons();

    bool is_initialized_ = false;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> stop_scanning_{false};
    
    std::string adapter_name_;
    int scan_window_ms_ = 100;
    int stale_timeout_ms_ = 10000; // 10 seconds

    std::thread scanning_thread_;
    mutable std::mutex beacons_mutex_;
    std::map<std::string, BeaconInfo> beacons_; // Key: UUID
};

} // namespace Ble
} // namespace Drivers
} // namespace PulseOne

#endif // BLE_BEACON_DRIVER_H
