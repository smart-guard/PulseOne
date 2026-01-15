#include "Workers/Protocol/BleBeaconWorker.h"
#include "Logging/LogManager.h"
#include <chrono>

namespace PulseOne {
namespace Workers {

BleBeaconWorker::BleBeaconWorker(const Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info) {
    ble_driver_ = std::make_unique<PulseOne::Drivers::Ble::BleBeaconDriver>();
    // No need to set driver_ pointer manually if using unique_ptr and IProtocolDriver interface 
    // isn't stored in BaseDeviceWorker as a raw pointer in the header I saw.
    // Wait, BaseDeviceWorker header didn't show a 'driver_' member. 
    // It assumes derived classes handle their drivers or it was hidden in the truncated part.
    // Assuming derived class manages specifics.
}

BleBeaconWorker::~BleBeaconWorker() {
    Stop().wait();
}

std::future<bool> BleBeaconWorker::Start() {
    return std::async(std::launch::async, [this]() {
        if (is_running_) return true;
        
        if (!EstablishConnection()) {
            // Even if initial connection fails, we might want to start logic to retry?
            // BaseDeviceWorker logic usually handles retries if we use it, 
            // but here we have our own loop.
            // Let's allow start but loop will retry.
        }
        
        is_running_ = true;
        worker_thread_ = std::thread(&BleBeaconWorker::PollingThreadFunction, this);
        ChangeState(WorkerState::RUNNING);
        return true;
    });
}

std::future<bool> BleBeaconWorker::Stop() {
    return std::async(std::launch::async, [this]() {
        is_running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        CloseConnection();
        ChangeState(WorkerState::STOPPED);
        return true;
    });
}
bool BleBeaconWorker::EstablishConnection() {
    if (!ble_driver_) return false;
    
    Structs::DriverConfig config;
    config.properties["adapter"] = "default";
    
    if (!ble_driver_->Initialize(config)) {
        LogManager::getInstance().Error("Failed to initialize BLE driver");
        return false;
    }
    
    return ble_driver_->Connect();
}

bool BleBeaconWorker::CloseConnection() {
    if (!ble_driver_) return false;
    return ble_driver_->Disconnect();
}

bool BleBeaconWorker::CheckConnection() {
    if (!ble_driver_) return false;
    return ble_driver_->IsConnected();
}

void BleBeaconWorker::RegisterServices() {
    // No specific services for now
}

void BleBeaconWorker::PollingThreadFunction() {
    LogManager::getInstance().Info("BLE Worker Polling Thread Started: " + device_info_.id);
    
    while (is_running_) {
        if (!CheckConnection()) {
            if (!EstablishConnection()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
        }

        auto start_time = std::chrono::steady_clock::now();

        // 1. Get Points to Read
        std::vector<Structs::DataPoint> points_to_read;
        {
            std::lock_guard<std::mutex> lock(data_points_mutex_);
            for (const auto& point : data_points_) {
                if (point.is_enabled) {
                    points_to_read.push_back(point);
                }
            }
        }

        // 2. Read Values
        if (!points_to_read.empty()) {
            std::vector<Structs::TimestampedValue> values;
            if (ble_driver_->ReadValues(points_to_read, values)) {
                // LogManager::getInstance().Info("Read " + std::to_string(values.size()) + " values from driver."); 
                
                // 3. Update Points and Send to Pipeline
                std::vector<Structs::TimestampedValue> pipeline_values;
                // 3. Update Points
                {
                    std::lock_guard<std::mutex> lock(data_points_mutex_);
                    
                    // Assuming 1:1 mapping as driver processes the vector in order
                    for (size_t i = 0; i < points_to_read.size() && i < values.size(); ++i) {
                        const auto& read_point_def = points_to_read[i];
                        auto& val = values[i];
                        
                        // find matching point in actual data_points_ to update state
                        for (auto& point : data_points_) {
                            if (point.id == read_point_def.id) {
                                point.UpdateCurrentValue(val.value, val.quality);
                                pipeline_values.push_back(point.ToTimestampedValue());
                                
                                LogManager::getInstance().Info("[Worker] Pipeline Payload - DeviceID: " + device_info_.id + 
                                                              ", PointID: " + point.id + 
                                                              ", Value: " + PulseOne::Utils::DataVariantToString(point.current_value));
                                break;
                            }
                        }
                    }
                }
                
                // 4. Send to Pipeline (without lock to avoid deadlock with GetDataPoints)
                if (!pipeline_values.empty()) {
                    SendValuesToPipelineWithLogging(pipeline_values, "BLE_BEACON");
                } else {
                    LogManager::getInstance().Warn("Pipeline values empty after updates.");
                }
            } else {
                LogManager::getInstance().Warn("Driver ReadValues returned false.");
            }
        } else {
             LogManager::getInstance().Warn("No points to read (points_to_read empty). DataPoints count: " + std::to_string(data_points_.size()));
        }

        // Sleep for polling interval
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto sleep_duration = std::chrono::milliseconds(device_info_.polling_interval_ms) - elapsed;
        
        if (sleep_duration > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        } else {
             std::this_thread::yield();
        }
    }
}

} // namespace Workers
} // namespace PulseOne
