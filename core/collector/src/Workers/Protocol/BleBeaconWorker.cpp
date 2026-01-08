#include "Workers/Protocol/BleBeaconWorker.h"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Workers {

BleBeaconWorker::BleBeaconWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info) {
    ble_driver_ = std::make_unique<PulseOne::Drivers::Ble::BleBeaconDriver>();
}

std::future<bool> BleBeaconWorker::Start() {
    return std::async(std::launch::async, [this]() {
        if (is_started_) return true;

        // Initialize Driver with Config
        PulseOne::Structs::DriverConfig config;
        config.endpoint = device_info_.endpoint;
        config.properties = device_info_.properties;
        
        // Pass port/ip if available in properties (optional)
        // config.port = ...

        if (!ble_driver_->Initialize(config)) {
            LogManager::getInstance().Error("[BleBeaconWorker] Initialize failed");
            return false;
        }
        
        // BLE Driver Start = Start Scanning
        if (!ble_driver_->Start()) {
            LogManager::getInstance().Error("[BleBeaconWorker] Driver Start failed");
            return false;
        }
        
        current_state_ = WorkerState::RUNNING;
        is_started_ = true;
        
        // Start Polling Thread
        if (!polling_thread_running_) {
            polling_thread_running_ = true;
            polling_thread_ = std::make_unique<std::thread>(&BleBeaconWorker::PollingThreadFunction, this);
        }
        
        LogManager::getInstance().Info("[BleBeaconWorker] Started");
        return true;
    });
}

std::future<bool> BleBeaconWorker::Stop() {
    return std::async(std::launch::async, [this]() {
        if (!is_started_) return true;
        
        // Stop Polling Thread
        polling_thread_running_ = false;
        if (polling_thread_ && polling_thread_->joinable()) {
            polling_thread_->join();
            polling_thread_.reset();
        }

        ble_driver_->Stop();
        
        current_state_ = WorkerState::STOPPED;
        is_started_ = false;
        
        LogManager::getInstance().Info("[BleBeaconWorker] Stopped");
        return true;
    });
}

void BleBeaconWorker::PollingThreadFunction() {
    LogManager::getInstance().Info("[BleBeaconWorker] Polling thread started");
    
    while (polling_thread_running_) {
        // BLE Driver caches data, so we poll the cache
        std::vector<PulseOne::Structs::DataPoint> points_to_read;
        {
            std::lock_guard<std::mutex> lock(data_points_mutex_);
            points_to_read = data_points_;
        }
        
        if (!points_to_read.empty()) {
            std::vector<PulseOne::Structs::TimestampedValue> values;
            if (ble_driver_->ReadValues(points_to_read, values)) {
                 SendDataToPipeline(values);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(GetPollingInterval()));
    }
}

bool BleBeaconWorker::EstablishConnection() {
    return ble_driver_->Connect();
}

bool BleBeaconWorker::CloseConnection() {
    return ble_driver_->Disconnect();
}

bool BleBeaconWorker::CheckConnection() {
    return ble_driver_->IsConnected();
}

} // namespace Workers
} // namespace PulseOne
