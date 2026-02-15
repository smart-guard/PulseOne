/**
 * @file ModbusWorker.cpp
 * @brief Unified Modbus Worker Implementation - Handles both TCP and RTU
 */

#include "Workers/Protocol/ModbusWorker.h"
#include "Drivers/Common/DriverFactory.h" // Plugin System Factory
#include "Logging/LogManager.h"
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>
#include <sstream>

namespace PulseOne {
namespace Workers {

using namespace std::chrono;
using json = nlohmann::json;

ModbusWorker::ModbusWorker(const DeviceInfo &device_info)
    : BaseDeviceWorker(device_info), modbus_driver_(nullptr),
      polling_thread_running_(false) {
  LogMessage(LogLevel::INFO,
             "ModbusWorker created for device: " + device_info.name +
                 " (Protocol: " + device_info.protocol_type + ")");

  if (!ParseModbusConfig()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to parse Modbus configuration");
  }

  if (!InitializeModbusDriver()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to initialize ModbusDriver");
  }
}

ModbusWorker::~ModbusWorker() {
  Stop().get();
  LogMessage(LogLevel::INFO, "ModbusWorker destroyed");
}

std::future<bool> ModbusWorker::Start() {
  return std::async(std::launch::async, [this]() -> bool {
    if (GetState() == WorkerState::RUNNING) {
      return true;
    }

    LogMessage(LogLevel::INFO, "Starting ModbusWorker...");

    StartReconnectionThread();

    if (modbus_driver_ && !modbus_driver_->Start()) {
      LogMessage(LogLevel::LOG_ERROR, "Failed to start ModbusDriver");
      ChangeState(WorkerState::WORKER_ERROR);
      return false;
    }

    if (EstablishConnection()) {
      ChangeState(WorkerState::RUNNING);
    } else {
      ChangeState(WorkerState::RECONNECTING);
    }

    // 폴링 스레드 시작
    polling_thread_running_ = true;
    polling_thread_ = std::make_unique<std::thread>(
        &ModbusWorker::PollingThreadFunction, this);

    return true;
  });
}

std::future<bool> ModbusWorker::Stop() {
  return std::async(std::launch::async, [this]() -> bool {
    LogMessage(LogLevel::INFO, "Stopping ModbusWorker...");

    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
      polling_thread_->join();
    }

    if (modbus_driver_) {
      modbus_driver_->Stop();
      modbus_driver_->Disconnect();
    }

    CloseConnection();
    ChangeState(WorkerState::STOPPED);
    StopAllThreads();

    return true;
  });
}

bool ModbusWorker::EstablishConnection() {
  if (!modbus_driver_)
    return false;

  LogMessage(LogLevel::INFO, "Establishing Modbus connection...");
  bool connected = modbus_driver_->Connect();

  if (connected) {
    LogMessage(LogLevel::INFO, "Modbus connection established successfully");
    SetConnectionState(true);
  } else {
    auto last_error = modbus_driver_->GetLastError();
    LogMessage(LogLevel::WARN,
               "Failed to establish Modbus connection: " + last_error.message +
                   " (Details: " + last_error.details + ")");
    SetConnectionState(false);
  }

  return connected;
}

bool ModbusWorker::CloseConnection() {
  if (modbus_driver_) {
    modbus_driver_->Disconnect();
  }
  SetConnectionState(false);
  return true;
}

bool ModbusWorker::CheckConnection() {
  if (modbus_driver_) {
    return modbus_driver_->IsConnected();
  }
  return false;
}

bool ModbusWorker::SendKeepAlive() {
  if (!modbus_driver_ || !modbus_driver_->IsConnected())
    return false;

  try {
    // Read a simple register (e.g., address 0) as keep-alive
    std::vector<uint16_t> regs;
    PulseOne::Structs::DataPoint dp;
    dp.id = "0";    // Dummy ID for keep-alive
    dp.address = 0; // Standard keep-alive address
    dp.data_type = "UINT16";

    std::string s_id = modbus_config_.properties.count("slave_id")
                           ? modbus_config_.properties.at("slave_id")
                           : "1";
    dp.protocol_params["slave_id"] = s_id;
    dp.protocol_params["function_code"] = "3";

    LogMessage(LogLevel::DEBUG,
               "Keep-alive: Sending request (SlaveID=" + s_id + ")");

    std::vector<TimestampedValue> results;
    bool success = modbus_driver_->ReadValues({dp}, results);

    if (success) {
      LogMessage(LogLevel::DEBUG, "Keep-alive: Success");
    } else {
      LogMessage(LogLevel::WARN, "Keep-alive: Driver returned false");
    }
    return success;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR, "Keep-alive: EXCEPTION in SendKeepAlive: " +
                                        std::string(e.what()));
    throw; // Re-throw to be caught by HandleKeepAlive
  }
}

bool ModbusWorker::WriteDataPoint(const std::string &point_id,
                                  const DataValue &value) {
  if (!modbus_driver_)
    return false;

  auto point_opt = FindDataPointById(point_id);
  if (!point_opt.has_value()) {
    LogMessage(LogLevel::WARN,
               "Write requested for unknown point ID: " + point_id);
    return false;
  }

  // We delegate the entire writing logic to the driver via the generic
  // interface. The driver is responsible for interpreting protocol_params
  // (slave_id, function_code, etc.) and performing the correct Modbus write
  // function (Coil, Register, etc.).

  // Note: Advanced features like MaskWrite or split-byte writing that were
  // previously here should ideally be handled by the driver based on DataPoint
  // configuration (e.g. bit_index). If the generic WriteValue doesn't support
  // them yet, they won't work, but basic writes will.

  bool success = modbus_driver_->WriteValue(point_opt.value(), value);

  if (success) {
    LogMessage(LogLevel::INFO,
               "Successfully wrote value to point: " + point_id);
  } else {
    LogMessage(LogLevel::WARN, "Failed to write value to point: " + point_id);
  }

  return success;
}

bool ModbusWorker::ParseModbusConfig() {
  modbus_config_.device_id = device_info_.name;
  modbus_config_.endpoint = device_info_.endpoint;
  modbus_config_.timeout_ms = device_info_.timeout_ms;
  modbus_config_.retry_count = device_info_.retry_count;
  modbus_config_.properties = device_info_.driver_config.properties;

  // Add protocol specific endpoint info if not present
  std::string p_type = device_info_.protocol_type;
  std::transform(p_type.begin(), p_type.end(), p_type.begin(), ::tolower);

  if (device_info_.protocol_type == "MODBUS_TCP" || p_type == "tcp" ||
      p_type == "modbus_tcp") {
    modbus_config_.protocol = "MODBUS_TCP";
    modbus_config_.properties["protocol_type"] = "MODBUS_TCP";
  } else {
    modbus_config_.protocol = "MODBUS_RTU";
    modbus_config_.properties["protocol_type"] = "MODBUS_RTU";
    modbus_config_.properties["serial_port"] = device_info_.endpoint;
  }

  return true;
}

bool ModbusWorker::InitializeModbusDriver() {
  // Factory를 통해 ModbusDriver 생성 (Plugin)
  // device_info_.protocol_type은 "MODBUS_TCP" 또는 "MODBUS_RTU"여야 함
  std::string driver_key = device_info_.protocol_type;
  modbus_driver_ =
      PulseOne::Drivers::DriverFactory::GetInstance().CreateDriver(driver_key);

  // Check if created
  if (!modbus_driver_) {
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to create ModbusDriver instance via Factory. Key: " +
                   driver_key);
    // Fallback: If "MODBUS" was passed but not found, try "MODBUS_TCP" as
    // default? No, strict mode is better.
    return false;
  }

  // Configure (Note: ModbusDriver parses properties internally)
  PulseOne::Structs::DriverConfig config;
  config.endpoint = modbus_config_.endpoint;
  config.properties = modbus_config_.properties;
  config.timeout_ms = modbus_config_.timeout_ms;

  if (!modbus_driver_->Initialize(config)) {
    LogMessage(LogLevel::LOG_ERROR, "ModbusDriver initialization failed");
    return false;
  }

  // SetupDriverCallbacks(); // Not using callbacks for now
  return true;
}

void ModbusWorker::PollingThreadFunction() {
  LogMessage(LogLevel::INFO, "Modbus Polling thread started (Optimized)");

  while (polling_thread_running_) {
    auto start_time = system_clock::now();

    if (GetState() == WorkerState::RUNNING && modbus_driver_ &&
        modbus_driver_->IsConnected()) {
      std::vector<DataPoint> points_to_read;
      {
        std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
        // Collect enabled points
        for (const auto &dp : data_points_) {
          if (dp.is_enabled)
            points_to_read.push_back(dp);
        }
      }

      if (!points_to_read.empty()) {
        std::vector<TimestampedValue> results;
        // Driver handles optimization (grouping/batching)
        bool success = modbus_driver_->ReadValues(points_to_read, results);

        if (success) {
          // Send to pipeline
          if (!results.empty()) {
            SendValuesToPipelineWithLogging(results, "Polling", 0);
          }
          UpdateCommunicationResult(
              true, "", 0,
              duration_cast<milliseconds>(system_clock::now() - start_time));
        } else {
          std::string err = "Failed to read values";
          UpdateCommunicationResult(
              false, err, -1,
              duration_cast<milliseconds>(system_clock::now() - start_time));

          // 🔥 연결 실패 시 BaseDeviceWorker의 재연결 로직 트리거
          HandleConnectionError("Modbus polling failed: " + err);
        }
      }
    } else {
      // Reconnection logic is handled by ReconnectionThread or
      // EstablishConnection
    }

    // Simple sleep for interval (Using device default or min interval)
    // Ideally should respect per-point interval, but for simplification we poll
    // all at lowest interval or loop.
    int interval = device_info_.polling_interval_ms > 0
                       ? device_info_.polling_interval_ms
                       : 1000;
    std::this_thread::sleep_for(milliseconds(interval));
  }

  LogMessage(LogLevel::INFO, "Modbus Polling thread stopped");
}

bool ModbusWorker::ParseModbusAddress(const DataPoint &data_point,
                                      uint8_t &slave_id,
                                      ModbusRegisterType &register_type,
                                      uint16_t &address) {
  slave_id = static_cast<uint8_t>(
      std::stoi(GetPropertyValue(data_point.protocol_params, "slave_id",
                                 modbus_config_.properties.count("slave_id")
                                     ? modbus_config_.properties.at("slave_id")
                                     : "1")));

  // 1. Try address_string first (e.g., "HR:100", "IR:5", "DI:10", "CO:1")
  if (!data_point.address_string.empty()) {
    std::string s = data_point.address_string;
    size_t colon_pos = s.find(':');
    if (colon_pos != std::string::npos) {
      std::string type = s.substr(0, colon_pos);
      std::string addr_str = s.substr(colon_pos + 1);

      try {
        uint16_t addr = static_cast<uint16_t>(std::stoi(addr_str));
        if (type == "HR") {
          register_type = ModbusRegisterType::HOLDING_REGISTER;
          address = addr;
          return true;
        } else if (type == "IR") {
          register_type = ModbusRegisterType::INPUT_REGISTER;
          address = addr;
          return true;
        } else if (type == "DI") {
          register_type = ModbusRegisterType::DISCRETE_INPUT;
          address = addr;
          return true;
        } else if (type == "CO") {
          register_type = ModbusRegisterType::COIL;
          address = addr;
          return true;
        }
      } catch (...) {
        // Fall through to legacy if parsing fails
      }
    }
  }

  // 2. Legacy numeric address fallback (40001, 30001, etc.)
  uint32_t raw_address = data_point.address;
  if (raw_address >= 40001) {
    register_type = ModbusRegisterType::HOLDING_REGISTER;
    address = static_cast<uint16_t>(raw_address - 40001);
  } else if (raw_address >= 30001) {
    register_type = ModbusRegisterType::INPUT_REGISTER;
    address = static_cast<uint16_t>(raw_address - 30001);
  } else if (raw_address >= 10001) {
    register_type = ModbusRegisterType::DISCRETE_INPUT;
    address = static_cast<uint16_t>(raw_address - 10001);
  } else {
    register_type = ModbusRegisterType::COIL;
    address = static_cast<uint16_t>(raw_address - 1);
  }
  return true;
}

uint16_t ModbusWorker::GetDataPointRegisterCount(const DataPoint &point) const {
  if (point.data_type == "FLOAT32" || point.data_type == "INT32" ||
      point.data_type == "UINT32")
    return 2;
  return 1;
}

DataValue
ModbusWorker::ConvertRegistersToValue(const std::vector<uint16_t> &registers,
                                      const DataPoint &point) const {
  if (registers.empty())
    return 0.0;

  // Bit Index support (0-15)
  std::string bit_idx_str =
      GetPropertyValue(point.protocol_params, "bit_index", "");
  if (!bit_idx_str.empty()) {
    try {
      int bit_idx = std::stoi(bit_idx_str);
      if (bit_idx >= 0 && bit_idx < 16) {
        return ((registers[0] >> bit_idx) & 0x01) != 0;
      }
    } catch (const std::exception &e) {
      // Ignore invalid bit_index
    }
  }

  // Byte selection support
  if (point.data_type == "INT8_H" || point.data_type == "UINT8_H") {
    uint8_t val = static_cast<uint8_t>((registers[0] >> 8) & 0xFF);
    if (point.data_type == "INT8_H")
      return static_cast<double>(static_cast<int8_t>(val));
    return static_cast<double>(val);
  } else if (point.data_type == "INT8_L" || point.data_type == "UINT8_L") {
    uint8_t val = static_cast<uint8_t>(registers[0] & 0xFF);
    if (point.data_type == "INT8_L")
      return static_cast<double>(static_cast<int8_t>(val));
    return static_cast<double>(val);
  }

  std::string byte_order =
      GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian");
  if (point.protocol_params.count("byte_order"))
    byte_order = point.protocol_params.at("byte_order");

  if ((point.data_type == "FLOAT32" || point.data_type == "INT32" ||
       point.data_type == "UINT32") &&
      registers.size() >= 2) {
    uint32_t combined;
    if (byte_order == "swapped" || byte_order == "big_endian_swapped" ||
        byte_order == "little_endian") {
      combined = (static_cast<uint32_t>(registers[1]) << 16) | registers[0];
    } else {
      combined = (static_cast<uint32_t>(registers[0]) << 16) | registers[1];
    }

    if (point.data_type == "FLOAT32") {
      float f;
      std::memcpy(&f, &combined, sizeof(f));
      return static_cast<double>(f);
    } else if (point.data_type == "INT32")
      return static_cast<double>(static_cast<int32_t>(combined));
    else
      return static_cast<double>(combined);
  }

  // Signed INT16 check
  if (point.data_type == "INT16") {
    return static_cast<double>(static_cast<int16_t>(registers[0]));
  }

  return static_cast<double>(registers[0]);
}

std::vector<uint16_t>
ModbusWorker::ConvertValueToRegisters(const DataValue &value,
                                      const DataPoint &point) const {
  std::vector<uint16_t> regs;
  std::string byte_order =
      GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian");
  if (point.protocol_params.count("byte_order"))
    byte_order = point.protocol_params.at("byte_order");

  if (point.data_type == "FLOAT32" || point.data_type == "INT32" ||
      point.data_type == "UINT32") {
    uint32_t combined = 0;
    if (point.data_type == "FLOAT32") {
      float f =
          static_cast<float>(PulseOne::BasicTypes::DataVariantToDouble(value));
      std::memcpy(&combined, &f, sizeof(combined));
    } else if (point.data_type == "INT32") {
      combined = static_cast<uint32_t>(static_cast<int32_t>(
          PulseOne::BasicTypes::DataVariantToDouble(value)));
    } else {
      combined = static_cast<uint32_t>(
          PulseOne::BasicTypes::DataVariantToDouble(value));
    }

    if (byte_order == "swapped" || byte_order == "big_endian_swapped" ||
        byte_order == "little_endian") {
      regs.push_back(static_cast<uint16_t>(combined & 0xFFFF));
      regs.push_back(static_cast<uint16_t>((combined >> 16) & 0xFFFF));
    } else {
      regs.push_back(static_cast<uint16_t>((combined >> 16) & 0xFFFF));
      regs.push_back(static_cast<uint16_t>(combined & 0xFFFF));
    }
  } else {
    regs.push_back(static_cast<uint16_t>(
        PulseOne::BasicTypes::DataVariantToDouble(value)));
  }
  return regs;
}

std::string ModbusWorker::GetPropertyValue(
    const std::map<std::string, std::string> &properties,
    const std::string &key, const std::string &default_value) const {
  auto it = properties.find(key);
  return (it != properties.end()) ? it->second : default_value;
}

std::optional<DataPoint>
ModbusWorker::FindDataPointById(const std::string &point_id) {
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
  for (const auto &dp : data_points_) {
    if (dp.id == point_id)
      return dp;
  }
  return std::nullopt;
}

} // namespace Workers
} // namespace PulseOne
