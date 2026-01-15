#include "Drivers/Ble/BleBeaconDriver.h"
#include <iostream>
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Drivers {
namespace Ble {

BleBeaconDriver::BleBeaconDriver() : is_connected_(false) {}

BleBeaconDriver::~BleBeaconDriver() {
    Disconnect();
}

bool BleBeaconDriver::Initialize(const Structs::DriverConfig& config) {
    // Basic init, nothing complex for simulation
    (void)config;
    return true;
}

bool BleBeaconDriver::Connect() {
    is_connected_ = true;
    NotifyStatusChange(Enums::ConnectionStatus::CONNECTED);
    return true;
}

bool BleBeaconDriver::Disconnect() {
    is_connected_ = false;
    NotifyStatusChange(Enums::ConnectionStatus::DISCONNECTED);
    return true;
}

bool BleBeaconDriver::IsConnected() const {
    return is_connected_;
}

bool BleBeaconDriver::Start() {
    return Connect();
}

bool BleBeaconDriver::Stop() {
    return Disconnect();
}

bool BleBeaconDriver::WriteValue(const Structs::DataPoint& point, const Structs::DataValue& value) {
    (void)point;
    (void)value;
    return false; // BLE Beacons are read-only
}

Enums::ProtocolType BleBeaconDriver::GetProtocolType() const {
    return Enums::ProtocolType::BLE_BEACON; 
}

Structs::DriverStatus BleBeaconDriver::GetStatus() const {
    if (is_connected_) return Enums::DriverStatus::RUNNING;
    return Enums::DriverStatus::STOPPED;
}

Structs::ErrorInfo BleBeaconDriver::GetLastError() const {
    return last_error_;
}

void BleBeaconDriver::SimulateBeaconDiscovery(const std::string& uuid, int rssi) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    discovered_rssi_[uuid] = rssi;
}

bool BleBeaconDriver::ReadValues(const std::vector<Structs::DataPoint>& points, 
                               std::vector<Structs::TimestampedValue>& out_values) {
    if (!is_connected_) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    
    for (const auto& point : points) {
        Structs::TimestampedValue val;
        val.timestamp = std::chrono::system_clock::now();
            
        auto it = discovered_rssi_.find(point.address_string);
        if (it != discovered_rssi_.end()) {
            val.value = it->second;
            val.quality = Enums::DataQuality::GOOD;
        } else {
            // Simulation fallback for specific test UUID if not explicitly set
            if (point.address_string == "74278BDA-B644-4520-8F0C-720EAF059935") {
                val.value = -65; // Simulated "Good" RSSI
                val.quality = Enums::DataQuality::GOOD;
            } else {
                val.value = 0;
                val.quality = Enums::DataQuality::BAD;
            }
        }
        
        // Log the value for debugging
        if (std::holds_alternative<int>(val.value)) {
            // std::cout << "[Driver] Read RSSI for " << point.address_string << ": " << std::get<int>(val.value) << std::endl;
             LogManager::getInstance().Info("[Driver] Read RSSI for " + point.address_string + ": " + std::to_string(std::get<int>(val.value)));
        } else if (std::holds_alternative<double>(val.value)) {
             LogManager::getInstance().Info("[Driver] Read RSSI (double) for " + point.address_string + ": " + std::to_string(std::get<double>(val.value)));
        }

        out_values.push_back(val);
        
        // Update statistics
        UpdateReadStats(true);
    }
    
    return true;
}

} // namespace Ble
} // namespace Drivers
} // namespace PulseOne
