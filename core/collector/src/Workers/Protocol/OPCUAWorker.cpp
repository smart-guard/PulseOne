/**
 * @file OPCUAWorker.cpp
 * @brief OPC UA Protocol Worker Implementation
 */

#include "Workers/Protocol/OPCUAWorker.h"
#include "Logging/LogManager.h"
#include <future>

namespace PulseOne {
namespace Workers {

OPCUAWorker::OPCUAWorker(const PulseOne::Structs::DeviceInfo &device_info)
    : BaseDeviceWorker(device_info), polling_thread_running_(false) {
  opcua_driver_ =
      PulseOne::Drivers::DriverFactory::GetInstance().CreateDriver("OPC_UA");
  if (!opcua_driver_) {
    LogManager::getInstance().Error(
        "[OPCUAWorker] Failed to create OPC UA driver via factory. Plugin "
        "might not be loaded.");
  }
}

OPCUAWorker::~OPCUAWorker() { Stop().wait(); }

std::future<bool> OPCUAWorker::Start() {
  return std::async(std::launch::async, [this]() {
    LogMessage(PulseOne::Enums::LogLevel::INFO, "Starting OPCUAWorker...");

    // 1. Establish Connection
    StartReconnectionThread();

    if (!EstablishConnection()) {
      LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
                 "Failed to establish initial connection");
      ChangeState(WorkerState::RECONNECTING); // Let base thread handle it
      return true;                            // Start process so it can retry
    }

    // 2. Start Polling Thread
    if (!polling_thread_running_) {
      polling_thread_running_ = true;
      polling_thread_ = std::make_unique<std::thread>(
          &OPCUAWorker::PollingThreadFunction, this);
    }

    ChangeState(WorkerState::RUNNING);
    return true;
  });
}

std::future<bool> OPCUAWorker::Stop() {
  return std::async(std::launch::async, [this]() {
    LogMessage(PulseOne::Enums::LogLevel::INFO, "Stopping OPCUAWorker...");

    ChangeState(WorkerState::STOPPING);
    polling_thread_running_ = false;

    if (polling_thread_ && polling_thread_->joinable()) {
      polling_thread_->join();
      polling_thread_.reset();
    }

    CloseConnection();
    ChangeState(WorkerState::STOPPED);
    return true;
  });
}

bool OPCUAWorker::EstablishConnection() {
  if (!opcua_driver_)
    return false;

  // Configure Driver
  PulseOne::Structs::DriverConfig config = GetDeviceInfo().driver_config;
  // Ensure endpoint from device info is used if not in config properties
  if (config.endpoint.empty()) {
    config.endpoint = GetEndpoint();
  }

  if (!opcua_driver_->Initialize(config)) {
    LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
               "Failed to initialize OPC UA Driver");
    return false;
  }

  if (opcua_driver_->Connect()) {
    SetConnectionState(true);
    LogMessage(PulseOne::Enums::LogLevel::INFO, "Connected to OPC UA Server");
    return true;
  } else {
    SetConnectionState(false);
    LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
               "Failed to connect to OPC UA Server");
    return false;
  }
}

bool OPCUAWorker::CloseConnection() {
  if (opcua_driver_) {
    opcua_driver_->Disconnect();
  }
  SetConnectionState(false);
  return true;
}

bool OPCUAWorker::CheckConnection() {
  if (!opcua_driver_)
    return false;
  bool connected = opcua_driver_->IsConnected();
  SetConnectionState(connected);
  return connected;
}

bool OPCUAWorker::WriteDataPoint(const std::string &point_id,
                                 const PulseOne::Structs::DataValue &value) {
  if (!opcua_driver_)
    return false;

  if (!CheckConnection()) {
    LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
               "Cannot write: Driver disconnected");
    return false;
  }

  // Find DataPoint internally if needed, but Driver's WriteValue takes
  // DataPoint struct. We construct a temporary DataPoint with the ID/Address
  // info we have? The driver usually needs AddressString. Ideally we should
  // lookup the DataPoint from our list to get the AddressString.

  std::string address_string;
  {
    std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
    for (const auto &dp : data_points_) {
      if (dp.id == point_id) {
        address_string = dp.address_string;
        break;
      }
    }
  }

  if (address_string.empty()) {
    LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
               "DataPoint not found for write: " + point_id);
    return false;
  }

  PulseOne::Structs::DataPoint point;
  point.id = point_id;
  point.address_string = address_string;

  if (opcua_driver_->WriteValue(point, value)) {
    LogMessage(PulseOne::Enums::LogLevel::INFO,
               "Write successful to " + point_id);
    return true;
  } else {
    LogMessage(PulseOne::Enums::LogLevel::LOG_ERROR,
               "Write failed to " + point_id);
    return false;
  }
}

std::vector<PulseOne::Structs::DataPoint> OPCUAWorker::DiscoverDataPoints() {
  if (!opcua_driver_) {
    return {};
  }
  return opcua_driver_->DiscoverPoints();
}

void OPCUAWorker::PollingThreadFunction() {
  LogMessage(PulseOne::Enums::LogLevel::INFO, "Polling thread started");

  while (polling_thread_running_) {
    auto start_time = std::chrono::steady_clock::now();

    if (CheckConnection()) {
      std::vector<PulseOne::Structs::DataPoint> points_to_read;
      {
        std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
        points_to_read = data_points_;
      }

      if (!points_to_read.empty()) {
        std::vector<PulseOne::Structs::TimestampedValue> values;
        if (opcua_driver_->ReadValues(points_to_read, values)) {
          // Send to Pipeline
          SendDataToPipeline(values);
        } else {
          LogMessage(PulseOne::Enums::LogLevel::WARN, "ReadValues failed");
          // Identify specific connection error?
          if (!opcua_driver_->IsConnected()) {
            SetConnectionState(false);
          }
        }
      }
    } else {
      // Connection lost, trigger unified reconnection logic
      LogManager::getInstance().Warn(
          "OPC UA connection lost, triggering unified reconnection.");
      HandleConnectionError("OPC UA connection lost");
      std::this_thread::sleep_for(std::chrono::seconds(
          1)); // Wait a bit before next check/reconnection attempt
    }

    // Sleep for polling interval
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    auto interval = std::chrono::milliseconds(GetPollingInterval());

    if (interval > elapsed) {
      std::this_thread::sleep_for(interval - elapsed);
    } else {
      // Polling took longer than interval, yield minimal time
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  LogMessage(PulseOne::Enums::LogLevel::INFO, "Polling thread stopped");
}

} // namespace Workers
} // namespace PulseOne
