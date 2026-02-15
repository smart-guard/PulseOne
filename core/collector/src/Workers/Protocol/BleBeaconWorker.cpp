#include "Workers/Protocol/BleBeaconWorker.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <chrono>

namespace PulseOne {
namespace Workers {

BleBeaconWorker::BleBeaconWorker(const Structs::DeviceInfo &device_info)
    : BaseDeviceWorker(device_info) {
  ble_driver_ =
      Drivers::DriverFactory::GetInstance().CreateDriver("BLE_BEACON");
  if (!ble_driver_) {
    LogManager::getInstance().Error(
        "[BleBeaconWorker] Failed to create BLE driver via factory");
  }
}

BleBeaconWorker::~BleBeaconWorker() { Stop().wait(); }

std::future<bool> BleBeaconWorker::Start() {
  return std::async(std::launch::async, [this]() {
    if (is_running_)
      return true;

    StartReconnectionThread();

    if (!EstablishConnection()) {
      LogManager::getInstance().Warn(
          "Initial BLE connection failed. Will retry via base engine.");
      ChangeState(WorkerState::RECONNECTING);
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
  if (!ble_driver_)
    return false;

  Structs::DriverConfig config;
  // Map Endpoint (e.g., "hci0" or "default")
  config.endpoint = device_info_.endpoint;
  if (config.endpoint.empty())
    config.endpoint = "default";

  // Pass all properties (includes scan_interval_ms, etc.)
  config.properties = device_info_.properties;

  // Explicitly set 'adapter' property if not present but endpoint is set
  if (config.properties.find("adapter") == config.properties.end()) {
    config.properties["adapter"] = config.endpoint;
  }

  if (!ble_driver_->Initialize(config)) {
    LogManager::getInstance().Error("Failed to initialize BLE driver");
    return false;
  }

  return ble_driver_->Connect();
}

bool BleBeaconWorker::CloseConnection() {
  if (!ble_driver_)
    return false;
  return ble_driver_->Disconnect();
}

bool BleBeaconWorker::CheckConnection() {
  if (!ble_driver_)
    return false;
  return ble_driver_->IsConnected();
}

void BleBeaconWorker::RegisterServices() {
  // No specific services for now
}

void BleBeaconWorker::PollingThreadFunction() {
  LogManager::getInstance().Info("BLE Worker Polling Thread Started: " +
                                 device_info_.id);

  while (is_running_) {
    if (!CheckConnection()) {
      HandleConnectionError("BLE connection lost or not established");
      continue;
    }

    auto start_time = std::chrono::steady_clock::now();

    // 1. Get Points to Read
    std::vector<Structs::DataPoint> points_to_read;
    {
      std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
      for (const auto &point : data_points_) {
        if (point.is_enabled) {
          points_to_read.push_back(point);
        }
      }
    }

    // 2. Read Values
    if (!points_to_read.empty()) {
      std::vector<Structs::TimestampedValue> values;
      if (ble_driver_->ReadValues(points_to_read, values)) {
        // LogManager::getInstance().Info("Read " +
        // std::to_string(values.size()) + " values from driver.");

        // 3. Update Points and Send to Pipeline
        std::vector<Structs::TimestampedValue> pipeline_values;
        // 3. Update Points
        {
          std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);

          // Assuming 1:1 mapping as driver processes the vector in order
          for (size_t i = 0; i < points_to_read.size() && i < values.size();
               ++i) {
            const auto &read_point_def = points_to_read[i];
            auto &val = values[i];

            // find matching point in actual data_points_ to update state
            for (auto &point : data_points_) {
              if (point.id == read_point_def.id) {
                point.UpdateCurrentValue(val.value, val.quality);
                pipeline_values.push_back(point.ToTimestampedValue());

                LogManager::getInstance().Info(
                    "[Worker] Pipeline Payload - DeviceID: " + device_info_.id +
                    ", PointID: " + point.id + ", Value: " +
                    PulseOne::Utils::DataVariantToString(point.current_value));
                break;
              }
            }
          }
        }

        // 4. Send to Pipeline (without lock to avoid deadlock with
        // GetDataPoints)
        if (!pipeline_values.empty()) {
          SendValuesToPipelineWithLogging(pipeline_values, "BLE_BEACON");
        } else {
          LogManager::getInstance().Warn(
              "Pipeline values empty after updates.");
        }
      } else {
        LogManager::getInstance().Warn("Driver ReadValues returned false.");
      }
    } else {
      LogManager::getInstance().Warn(
          "No points to read (points_to_read empty). DataPoints count: " +
          std::to_string(data_points_.size()));
    }

    // Sleep for polling interval
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    auto sleep_duration =
        std::chrono::milliseconds(device_info_.polling_interval_ms) - elapsed;

    if (sleep_duration > std::chrono::milliseconds(0)) {
      std::this_thread::sleep_for(sleep_duration);
    } else {
      std::this_thread::yield();
    }
  }
}

} // namespace Workers
} // namespace PulseOne
