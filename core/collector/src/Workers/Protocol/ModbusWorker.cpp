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

    // 포인트별 폴링 그룹 분류 (고속/저속)
    {
      std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
      BuildPollingGroups(data_points_);
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

// =============================================================================
// BuildPollingGroups — data_points_를 두 그룹으로 분류
//
// 분류 기준:
//   fast_points_: polling_interval_ms가 설정되어 있고 (> 0),
//                 장치 기본 주기보다 짧은 포인트
//                 → DB 기본값(1000ms)이 device_interval과 같으면 slow로 분류
//
//   slow_points_: 그 외 (장치 기본 주기로 폴링)
// =============================================================================
void ModbusWorker::BuildPollingGroups(const std::vector<DataPoint> &points) {
  std::lock_guard<std::mutex> grp_lock(polling_groups_mutex_);
  fast_points_.clear();
  slow_points_.clear();

  uint32_t device_interval = static_cast<uint32_t>(
      device_info_.polling_interval_ms > 0 ? device_info_.polling_interval_ms
                                           : 1000);

  for (const auto &dp : points) {
    if (!dp.is_enabled)
      continue;

    // 포인트에 유효한 개별 주기가 있고, 장치 기본 주기보다 짧을 때만 fast
    if (dp.polling_interval_ms > 0 &&
        dp.polling_interval_ms < device_interval) {
      fast_points_.push_back(dp);
    } else {
      slow_points_.push_back(dp);
    }
  }

  LogMessage(
      LogLevel::INFO,
      "[DualPolling] 분류 완료: 고속(fast)=" +
          std::to_string(fast_points_.size()) + "포인트, 저속(slow)=" +
          std::to_string(slow_points_.size()) + "포인트" +
          (fast_points_.empty() ? " [fast 없음 → 단일 기본 주기로 운영]" : ""));
}

// =============================================================================
// ReloadDataPoints — 런타임 포인트 재로드 시 fast/slow 그룹도 재분류
//
// BaseDeviceWorker::ReloadDataPoints는 data_points_만 교체하므로,
// ModbusWorker에서 반드시 override하여 BuildPollingGroups를 재호출해야 함.
// 재호출하지 않으면 fast_points_ / slow_points_가 이전 포인트 기준으로 남아
// 잘못된 폴링 주기가 계속 적용됨.
// =============================================================================
void ModbusWorker::ReloadDataPoints(const std::vector<DataPoint> &new_points) {
  // 1) BaseDeviceWorker: data_points_ 교체 + 락 처리
  BaseDeviceWorker::ReloadDataPoints(new_points);

  // 2) fast/slow 그룹 재분류 (data_points_mutex_ 보유 상태에서 호출)
  {
    std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
    BuildPollingGroups(data_points_);
  }

  // 로그: polling_groups_mutex_ 보호
  {
    std::lock_guard<std::mutex> grp_lock(polling_groups_mutex_);
    LogMessage(LogLevel::INFO,
               "[DualPolling] ReloadDataPoints: fast=" +
                   std::to_string(fast_points_.size()) +
                   ", slow=" + std::to_string(slow_points_.size()));
  }
}

// =============================================================================
// PollingThreadFunction — 이중 폴링 루프
//
// 매 루프:
//   1) fast_points_ 전체 읽기   (polling_interval_ms 기준 elapsed 체크)
//   2) slow_loop_counter_가 slow_ratio에 도달하면 slow_points_ 읽기
//
// fast_interval  = 고속 포인트 중 가장 짧은 polling_interval_ms (없으면 장치
// 기본값) slow_interval  = 장치 기본 polling_interval_ms (없으면 5000ms)
// slow_ratio     = slow_interval / fast_interval  (정수 반올림)
// =============================================================================
void ModbusWorker::PollingThreadFunction() {
  LogMessage(LogLevel::INFO, "Modbus Polling thread started (Dual-Speed)");

  // 장치 기본 폴링 주기
  int device_interval_ms = device_info_.polling_interval_ms > 0
                               ? device_info_.polling_interval_ms
                               : 1000;

  slow_loop_counter_ = 0;

  while (polling_thread_running_) {
    auto loop_start = system_clock::now();

    // ── 루프마다 fast/slow 상태 최신으로 읽기 (ReloadDataPoints 대응) ──────
    uint32_t fast_interval_ms = static_cast<uint32_t>(device_interval_ms);
    bool has_fast_group = false;
    uint64_t slow_ratio = 1;
    {
      std::lock_guard<std::mutex> grp_lock(polling_groups_mutex_);
      has_fast_group = !fast_points_.empty();
      if (has_fast_group) {
        for (const auto &fp : fast_points_) {
          if (fp.polling_interval_ms > 0 &&
              fp.polling_interval_ms < fast_interval_ms)
            fast_interval_ms = fp.polling_interval_ms;
        }
        slow_ratio =
            static_cast<uint64_t>(device_interval_ms) / fast_interval_ms;
        if (slow_ratio < 1)
          slow_ratio = 1;
      }
      // has_fast_group == false: slow_ratio=1, fast_interval_ms=device_interval
    }

    if (GetState() == WorkerState::RUNNING && modbus_driver_ &&
        modbus_driver_->IsConnected()) {

      // ── [1] fast 그룹 읽기 (fast 포인트가 있을 때만)
      if (has_fast_group) {
        std::vector<DataPoint> fast_to_read;
        {
          std::lock_guard<std::mutex> grp_lock(polling_groups_mutex_);
          for (const auto &fp : fast_points_) {
            if (fp.is_enabled)
              fast_to_read.push_back(fp);
          }
        }
        if (!fast_to_read.empty()) {
          std::vector<TimestampedValue> results;
          bool success = modbus_driver_->ReadValues(fast_to_read, results);
          if (success && !results.empty()) {
            SendValuesToPipelineWithLogging(results, "FastPolling", 0);
            UpdateCommunicationResult(
                true, "", 0,
                duration_cast<milliseconds>(system_clock::now() - loop_start));
          } else if (!success) {
            HandleConnectionError("Fast-group Modbus polling failed");
          }
        }
      }

      // ── [2] slow 그룹 읽기 (slow_ratio 루프마다 1회)
      ++slow_loop_counter_;
      if (slow_loop_counter_ >= slow_ratio) {
        slow_loop_counter_ = 0;

        std::vector<DataPoint> slow_to_read;
        {
          std::lock_guard<std::mutex> grp_lock(polling_groups_mutex_);
          for (const auto &sp : slow_points_) {
            if (sp.is_enabled)
              slow_to_read.push_back(sp);
          }
        }

        // slow도 fast도 없으면 → 전체 data_points_ 폴링 (기존 동작 유지)
        if (slow_to_read.empty() && !has_fast_group) {
          std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
          for (const auto &dp : data_points_) {
            if (dp.is_enabled)
              slow_to_read.push_back(dp);
          }
        }

        if (!slow_to_read.empty()) {
          auto slow_start = system_clock::now();
          std::vector<TimestampedValue> results;
          bool success = modbus_driver_->ReadValues(slow_to_read, results);
          if (success && !results.empty()) {
            SendValuesToPipelineWithLogging(results, "SlowPolling", 0);
            LogMessage(LogLevel::DEBUG,
                       "[DualPolling] Slow 읽기: " +
                           std::to_string(results.size()) + "포인트, " +
                           std::to_string(duration_cast<milliseconds>(
                                              system_clock::now() - slow_start)
                                              .count()) +
                           "ms");
          } else if (!success) {
            HandleConnectionError("Slow-group Modbus polling failed");
          }
        }
      }

    } else {
      // 연결 안 된 상태 — ReconnectionThread가 처리
    }

    // ── 루프 주기 맞추기
    auto elapsed =
        duration_cast<milliseconds>(system_clock::now() - loop_start);
    auto sleep_ms = milliseconds(fast_interval_ms) - elapsed;
    if (sleep_ms.count() > 0) {
      std::this_thread::sleep_for(sleep_ms);
    }
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
    // 소주소(1~9999): 기본값을 Holding Register(FC3)로 설정
    // (이전: Coil(FC1). 산업 현장에서 HR이 압도적으로 많이 사용됨)
    // Coil이 필요하면 UI에서 CO 선택 → address_string="CO:N" 으로 명시적 지정
    register_type = ModbusRegisterType::HOLDING_REGISTER;
    address = static_cast<uint16_t>(raw_address > 0 ? raw_address - 1 : 0);
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

  // Bit Index support (0-15): single bit extraction
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

  // Bit Range support (bit_start ~ bit_end): multi-bit extraction
  // e.g. bit_start=0, bit_end=3  → extracts bits [3:0] as integer (0~15)
  std::string bit_start_str =
      GetPropertyValue(point.protocol_params, "bit_start", "");
  std::string bit_end_str =
      GetPropertyValue(point.protocol_params, "bit_end", "");
  if (!bit_start_str.empty() && !bit_end_str.empty()) {
    try {
      int bit_start = std::stoi(bit_start_str);
      int bit_end = std::stoi(bit_end_str);
      if (bit_start >= 0 && bit_end < 16 && bit_start <= bit_end) {
        int bit_count = bit_end - bit_start + 1;
        uint16_t mask = static_cast<uint16_t>((1u << bit_count) - 1u);
        uint16_t extracted = (registers[0] >> bit_start) & mask;
        return static_cast<double>(extracted);
      }
    } catch (const std::exception &e) {
      // Ignore invalid bit_start/bit_end
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
