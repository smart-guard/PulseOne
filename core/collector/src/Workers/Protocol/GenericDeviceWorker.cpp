#include "Workers/Protocol/GenericDeviceWorker.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Workers {
namespace Protocol {

GenericDeviceWorker::GenericDeviceWorker(
    const PulseOne::Structs::DeviceInfo &device_info)
    : BaseDeviceWorker(device_info) {
  internal_protocol_name_ = device_info.protocol_type;
  LogManager::getInstance().Info("[GenericWorker] Created for protocol: " +
                                 internal_protocol_name_);
  SetupDriver();
}

GenericDeviceWorker::~GenericDeviceWorker() {
  Stop().get();
  LogManager::getInstance().Info("[GenericWorker] Destroyed");
}

std::future<bool> GenericDeviceWorker::Start() {
  return std::async(std::launch::async, [this]() -> bool {
    if (current_state_ == WorkerState::RUNNING)
      return true;

    LogManager::getInstance().Info("[GenericWorker] Starting worker...");

    StartReconnectionThread();

    if (driver_ && !driver_->Start()) {
      LogManager::getInstance().Error("[GenericWorker] Driver start failed");
      return false;
    }

    if (EstablishConnection()) {
      current_state_ = WorkerState::RUNNING;
    } else {
      current_state_ = WorkerState::RECONNECTING;
    }

    polling_thread_running_ = true;
    polling_thread_ = std::make_unique<std::thread>(
        &GenericDeviceWorker::PollingThreadMain, this);

    return true;
  });
}

std::future<bool> GenericDeviceWorker::Stop() {
  return std::async(std::launch::async, [this]() -> bool {
    LogManager::getInstance().Info("[GenericWorker] Stopping worker...");

    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
      polling_thread_->join();
    }

    if (driver_) {
      driver_->Stop();
      driver_->Disconnect();
    }

    CloseConnection();
    current_state_ = WorkerState::STOPPED;
    StopAllThreads();

    return true;
  });
}

bool GenericDeviceWorker::EstablishConnection() {
  if (!driver_)
    return false;

  bool connected = driver_->Connect();
  is_connected_ = connected;
  return connected;
}

bool GenericDeviceWorker::CloseConnection() {
  if (driver_) {
    driver_->Disconnect();
  }
  is_connected_ = false;
  return true;
}

bool GenericDeviceWorker::CheckConnection() {
  if (!driver_)
    return false;
  bool connected = driver_->IsConnected();
  is_connected_ = connected;
  return connected;
}

bool GenericDeviceWorker::WriteDataPoint(
    const std::string &point_id, const PulseOne::Structs::DataValue &value) {
  if (!driver_ || !driver_->IsConnected())
    return false;

  // point_id를 통해 data_points_에서 해당 포인트를 찾음
  auto it =
      std::find_if(data_points_.begin(), data_points_.end(),
                   [&point_id](const auto &p) { return p.id == point_id; });

  if (it == data_points_.end()) {
    LogManager::getInstance().Error("[GenericWorker] Write failed: Point ID " +
                                    point_id + " not found");
    return false;
  }

  return driver_->WriteValue(*it, value);
}

void GenericDeviceWorker::PollingThreadMain() {
  LogManager::getInstance().Info("[GenericWorker] Polling thread started for " +
                                 internal_protocol_name_);

  while (polling_thread_running_) {
    if (current_state_ == WorkerState::RUNNING) {
      if (!PerformRead()) {
        LogManager::getInstance().Warn(
            "[GenericWorker] PerformRead failed for " +
            internal_protocol_name_);
      }
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(device_info_.polling_interval_ms));
  }

  LogManager::getInstance().Info(
      "[GenericWorker] Polling thread finished for " + internal_protocol_name_);
}

void GenericDeviceWorker::SetupDriver() {
  // Dynamically request driver from Factory using string
  driver_ = Drivers::DriverFactory::GetInstance().CreateDriver(
      internal_protocol_name_);

  if (!driver_) {
    LogManager::getInstance().Error(
        "[GenericWorker] Failed to create driver for: " +
        internal_protocol_name_);
    return;
  }

  if (!driver_->Initialize(device_info_.driver_config)) {
    LogManager::getInstance().Error(
        "[GenericWorker] Driver initialization failed for: " +
        internal_protocol_name_);
  } else {
    LogManager::getInstance().Info("[GenericWorker] Driver initialized for: " +
                                   internal_protocol_name_);
  }
}

bool GenericDeviceWorker::PerformRead() {
  if (!driver_ || !driver_->IsConnected())
    return false;

  std::vector<PulseOne::Structs::TimestampedValue> received_values;
  if (driver_->ReadValues(data_points_, received_values)) {
    SendDataToPipeline(received_values);
    return true;
  }
  return false;
}
bool GenericDeviceWorker::PerformWrite(
    const PulseOne::Structs::DataPoint &point,
    const PulseOne::Structs::DataValue &value) {
  if (!driver_ || !driver_->IsConnected())
    return false;
  return driver_->WriteValue(point, value);
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne
