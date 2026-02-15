/**
 * @file HttpRestWorker.cpp
 * @brief HTTP/REST Protocol Worker Implementation
 */

#include "Workers/Protocol/HttpRestWorker.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

namespace PulseOne {
namespace Workers {

using namespace std::chrono;
using json = nlohmann::json;

HttpRestWorker::HttpRestWorker(const DeviceInfo &device_info)
    : BaseDeviceWorker(device_info), http_driver_(nullptr),
      polling_thread_running_(false) {
  LogMessage(LogLevel::INFO,
             "HttpRestWorker created for device: " + device_info.name);

  if (!ParseHttpRestConfig()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to parse HTTP/REST configuration");
  }

  if (!InitializeHttpRestDriver()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to initialize HttpRestDriver");
  }
}

HttpRestWorker::~HttpRestWorker() {
  Stop().get();
  LogMessage(LogLevel::INFO, "HttpRestWorker destroyed");
}

std::future<bool> HttpRestWorker::Start() {
  return std::async(std::launch::async, [this]() -> bool {
    if (GetState() == WorkerState::RUNNING) {
      return true;
    }

    LogMessage(LogLevel::INFO, "Starting HttpRestWorker...");

    StartReconnectionThread();

    if (http_driver_ && !http_driver_->Start()) {
      LogMessage(LogLevel::LOG_ERROR, "Failed to start HttpRestDriver");
      ChangeState(WorkerState::WORKER_ERROR);
      return false;
    }

    if (EstablishConnection()) {
      ChangeState(WorkerState::RUNNING);
    } else {
      ChangeState(WorkerState::RECONNECTING);
    }

    // Start polling thread
    polling_thread_running_ = true;
    polling_thread_ = std::make_unique<std::thread>(
        &HttpRestWorker::PollingThreadFunction, this);

    return true;
  });
}

std::future<bool> HttpRestWorker::Stop() {
  return std::async(std::launch::async, [this]() -> bool {
    LogMessage(LogLevel::INFO, "Stopping HttpRestWorker...");

    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
      polling_thread_->join();
    }

    if (http_driver_) {
      http_driver_->Stop();
      http_driver_->Disconnect();
    }

    CloseConnection();
    ChangeState(WorkerState::STOPPED);
    StopAllThreads();

    return true;
  });
}

bool HttpRestWorker::EstablishConnection() {
  if (!http_driver_)
    return false;

  LogMessage(LogLevel::INFO, "Establishing HTTP/REST connection...");
  bool connected = http_driver_->Connect();

  if (connected) {
    LogMessage(LogLevel::INFO, "HTTP/REST connection established successfully");
    SetConnectionState(true);
  } else {
    LogMessage(LogLevel::WARN, "Failed to establish HTTP/REST connection");
    SetConnectionState(false);
  }

  return connected;
}

bool HttpRestWorker::CloseConnection() {
  if (http_driver_) {
    http_driver_->Disconnect();
  }
  SetConnectionState(false);
  return true;
}

bool HttpRestWorker::CheckConnection() {
  if (http_driver_) {
    return http_driver_->IsConnected();
  }
  return false;
}

bool HttpRestWorker::SendKeepAlive() {
  // HTTP/REST doesn't need keep-alive
  return true;
}

bool HttpRestWorker::WriteDataPoint(const std::string &point_id,
                                    const DataValue &value) {
  if (!http_driver_)
    return false;

  // Find data point
  for (const auto &point : data_points_) {
    if (point.id == point_id) {
      return http_driver_->WriteValue(point, value);
    }
  }

  LogMessage(LogLevel::WARN,
             "Write requested for unknown point ID: " + point_id);
  return false;
}

bool HttpRestWorker::ParseHttpRestConfig() {
  http_config_.endpoint = device_info_.endpoint;
  http_config_.protocol = "HTTP_REST";

  // Copy properties from device_info
  http_config_.properties = device_info_.properties;

  // Set defaults if not provided
  if (!http_config_.properties.count("http_method")) {
    http_config_.properties["http_method"] = "GET";
  }
  if (!http_config_.properties.count("timeout_ms")) {
    http_config_.properties["timeout_ms"] = "10000";
  }
  if (!http_config_.properties.count("retry_count")) {
    http_config_.properties["retry_count"] = "3";
  }

  return true;
}

bool HttpRestWorker::InitializeHttpRestDriver() {
  try {
    // Use DriverFactory to create driver
    http_driver_ = PulseOne::Drivers::DriverFactory::GetInstance().CreateDriver(
        "HTTP_REST");

    if (!http_driver_) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Failed to create HTTP/REST driver from factory");
      return false;
    }

    if (!http_driver_->Initialize(http_config_)) {
      LogMessage(LogLevel::LOG_ERROR, "Failed to initialize HTTP/REST driver");
      return false;
    }

    LogMessage(LogLevel::INFO, "HTTP/REST driver initialized successfully");
    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception during HTTP/REST driver initialization: " +
                   std::string(e.what()));
    return false;
  }
}

void HttpRestWorker::PollingThreadFunction() {
  LogMessage(LogLevel::INFO, "HTTP/REST polling thread started");

  while (polling_thread_running_) {
    if (GetState() != WorkerState::RUNNING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }

    if (!CheckConnection()) {
      HandleConnectionError("HTTP/REST connection lost");
      continue;
    }

    try {
      // Read all data points
      std::vector<PulseOne::Structs::TimestampedValue> values;
      bool success = http_driver_->ReadValues(data_points_, values);

      if (success && !values.empty()) {
        // Send to pipeline
        SendDataToPipeline(values);
      } else if (!success) {
        HandleConnectionError("HTTP/REST read values failed");
        continue;
      }

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Exception in polling thread: " + std::string(e.what()));
    }

    // Sleep for polling interval
    uint32_t interval = device_info_.polling_interval_ms > 0
                            ? device_info_.polling_interval_ms
                            : 5000;
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }

  LogMessage(LogLevel::INFO, "HTTP/REST polling thread stopped");
}

} // namespace Workers
} // namespace PulseOne
