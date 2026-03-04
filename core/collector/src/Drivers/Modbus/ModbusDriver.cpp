// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// Modbus 드라이버 - 완전한 DriverConfig 매핑 버전 (하드코딩 제거 완료)
// =============================================================================

#include "Drivers/Modbus/ModbusDriver.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Drivers/Common/DriverFactory.h"
#include "Drivers/Modbus/ModbusConnectionPool.h"
#include "Drivers/Modbus/ModbusDiagnostics.h"
#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusPerformance.h"
#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

using namespace PulseOne::Platform;

namespace PulseOne {
namespace Drivers {

// 타입 별칭 정의
using ErrorInfo = PulseOne::Structs::ErrorInfo;
using TimestampedValue = PulseOne::Structs::TimestampedValue;

// =============================================================================
// 1:N 시리얼 포트 공유를 위한 정적 뮤텍스 레지스트리
// =============================================================================
std::mutex &ModbusDriver::GetSerialMutex(const std::string &endpoint) {
  static std::mutex registry_mutex;
  static std::map<std::string, std::unique_ptr<std::mutex>> mutex_registry;

  std::lock_guard<std::mutex> lock(registry_mutex);
  if (mutex_registry.find(endpoint) == mutex_registry.end()) {
    mutex_registry[endpoint] = std::make_unique<std::mutex>();
  }
  return *mutex_registry[endpoint];
}

// =============================================================================
// 생성자/소멸자
// =============================================================================

ModbusDriver::ModbusDriver()
    : modbus_ctx_(nullptr), driver_statistics_("MODBUS"), is_connected_(false),
      current_slave_id_(-1), diagnostics_(nullptr), connection_pool_(nullptr),
      failover_(nullptr), performance_(nullptr) {
  // LogManager 초기화
  logger_ = &LogManager::getInstance();

  // 드라이버 상태 초기화
  status_ = Enums::ConnectionStatus::DISCONNECTED;

  logger_->Info("ModbusDriver 생성됨: " + config_.name);
}

ModbusDriver::~ModbusDriver() {
  Disconnect();
  CleanupConnection();

  logger_->Info("ModbusDriver 소멸됨: " + config_.name);
}

// =============================================================================
// IProtocolDriver 인터페이스 구현 (Core 기능)
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig &config) {
  std::lock_guard<std::mutex> lock(connection_mutex_);

  config_ = config;

  // 🔥 DriverConfig에서 직접 설정 가져오기
  logger_->Info("🔧 Initializing ModbusDriver with DriverConfig data:");
  logger_->Info("  - Received Config Properties Size: " +
                std::to_string(config.properties.size()));
  logger_->Info("  - Stored Config_ Properties Size: " +
                std::to_string(config_.properties.size()));
  logger_->Info("  - Endpoint: " + config_.endpoint);
  logger_->Info("  - Timeout: " + std::to_string(config_.timeout_ms) + "ms");
  logger_->Info("  - Retry Count: " + std::to_string(config_.retry_count));
  logger_->Info("  - Polling Interval: " +
                std::to_string(config_.polling_interval_ms) + "ms");

  // 🔥 기본 Slave ID 설정 (properties에서)
  if (config_.properties.count("slave_id")) {
    try {
      current_slave_id_ = std::stoi(config_.properties.at("slave_id"));
      logger_->Info("  - Default Slave ID: " +
                    std::to_string(current_slave_id_));
    } catch (const std::exception &e) {
      logger_->Warn(
          "  - Invalid slave_id format: " + config_.properties.at("slave_id") +
          ". Using default: " + std::to_string(current_slave_id_));
    }
  }

  // 🔥 디버그 모드 설정 (properties에서)
  if (config_.properties.count("debug_enabled") &&
      config_.properties.at("debug_enabled") == "true") {
    logger_->Info("  - Debug mode requested");
  }

  // Modbus 컨텍스트 설정
  if (!SetupModbusConnection()) {
    SetError(Structs::ErrorCode::CONFIGURATION_ERROR,
             "Failed to setup Modbus connection");
    return false;
  }

  if (config_.properties.count("debug_enabled") &&
      config_.properties.at("debug_enabled") == "true") {
    modbus_set_debug(modbus_ctx_, TRUE);
    logger_->Info("  - Debug mode enabled on context");
  }

  // 🔥 DriverConfig의 timeout_ms 직접 사용
  uint32_t timeout_ms = config_.timeout_ms;
  uint32_t timeout_sec = timeout_ms / 1000;
  uint32_t timeout_usec = (timeout_ms % 1000) * 1000;

  modbus_set_response_timeout(modbus_ctx_, timeout_sec, timeout_usec);

  // 🔥 byte timeout도 DriverConfig 기반으로 설정
  uint32_t byte_timeout_ms = timeout_ms / 2; // 기본값: response timeout의 절반
  if (config_.properties.count("byte_timeout_ms")) {
    try {
      byte_timeout_ms = std::stoi(config_.properties.at("byte_timeout_ms"));
    } catch (...) {
    }
  }
  uint32_t byte_timeout_usec = (byte_timeout_ms % 1000) * 1000;
  modbus_set_byte_timeout(modbus_ctx_, 0, byte_timeout_usec);

  // ── parallel_connections 설정 → ConnectionPool 활성화 ──────────────────
  // 장치 설정에서 "parallel_connections" 읽기 (기본값 1 = 순차)
  size_t parallel_connections = 1;
  if (config_.properties.count("parallel_connections")) {
    try {
      int v = std::stoi(config_.properties.at("parallel_connections"));
      if (v > 0)
        parallel_connections = static_cast<size_t>(v);
    } catch (...) {
    }
  }

  if (parallel_connections > 1) {
    // 병렬 모드: ConnectionPool 생성 및 활성화
    if (!connection_pool_) {
      connection_pool_ = std::make_unique<ModbusConnectionPool>(this);
    }
    int conn_timeout_sec = static_cast<int>(config_.timeout_ms / 1000);
    if (conn_timeout_sec < 1)
      conn_timeout_sec = 5;

    bool pool_ok = connection_pool_->EnableConnectionPooling(
        parallel_connections, conn_timeout_sec);

    logger_->Info("🔗 ConnectionPool 활성화: parallel_connections=" +
                  std::to_string(parallel_connections) +
                  (pool_ok ? " ✅" : " ⚠️ 연결 실패 (폴링 시 재시도)"));
  } else {
    // 순차 모드: 풀 비활성화 (기본)
    logger_->Info("🔗 ConnectionPool: 순차 모드 (parallel_connections=1)");
  }

  logger_->Info("✅ ModbusDriver initialization completed");
  return true;
}

bool ModbusDriver::Connect() {
  std::lock_guard<std::mutex> lock(connection_mutex_);

  if (is_connected_.load()) {
    return true;
  }

  if (!modbus_ctx_) {
    SetError(Structs::ErrorCode::CONNECTION_FAILED,
             "Modbus context not initialized");
    return false;
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  // 🔥 DriverConfig의 retry_count 사용
  uint32_t max_retries = config_.retry_count;

  for (uint32_t attempt = 0; attempt <= max_retries; ++attempt) {
    int result = modbus_connect(modbus_ctx_);
    if (result != -1) {
      is_connected_.store(true);

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      UpdateStats(true, duration.count(), "connect");

      logger_->Info("✅ Modbus connection established to " + config_.endpoint +
                    " (attempt " + std::to_string(attempt + 1) + "/" +
                    std::to_string(max_retries + 1) + ")");
      return true;
    }

    // 마지막 시도가 아니면 재시도
    if (attempt < max_retries) {
      // 🔥 DriverConfig 기반 백오프 계산
      uint32_t retry_delay = 100 * (attempt + 1); // 기본 백오프
      if (config_.properties.count("retry_delay_ms")) {
        try {
          retry_delay = static_cast<uint32_t>(
              std::stoi(config_.properties.at("retry_delay_ms")) *
              static_cast<int>(attempt + 1));
        } catch (...) {
        } // 잘못된 값이면 기본 백오프 유지
      }

      logger_->Warn("Connection attempt " + std::to_string(attempt + 1) +
                    " failed, retrying in " + std::to_string(retry_delay) +
                    "ms...");
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
    }
  }

  // 모든 시도 실패
  auto error_msg = std::string("Connection failed after ") +
                   std::to_string(max_retries + 1) +
                   " attempts: " + modbus_strerror(errno);
  SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  UpdateStats(false, duration.count(), "connect");

  return false;
}

bool ModbusDriver::Disconnect() {
  std::lock_guard<std::mutex> lock(connection_mutex_);

  if (!is_connected_.load()) {
    return true;
  }

  if (modbus_ctx_) {
    modbus_close(modbus_ctx_);
  }

  is_connected_.store(false);
  logger_->Info("🔌 Modbus connection closed for " + config_.endpoint);
  return true;
}

bool ModbusDriver::IsConnected() const { return is_connected_.load(); }

bool ModbusDriver::ReadValues(const std::vector<Structs::DataPoint> &points,
                              std::vector<Structs::TimestampedValue> &values) {
  // 연결 풀링 활성화 시
  if (connection_pool_ && IsConnectionPoolingEnabled()) {
    return PerformReadWithConnectionPool(points, values);
  }
  return ReadValuesImpl(points, values);
}

bool ModbusDriver::ReadValuesImpl(
    const std::vector<Structs::DataPoint> &points,
    std::vector<Structs::TimestampedValue> &values) {
  // 🔍 1:N 시리얼 포트 공유 또는 동시 접근 제어를 위한 엔드포인트 잠금
  logger_->Debug("[ModbusDriver] Entering ReadValuesImpl for endpoint: " +
                 config_.endpoint +
                 " (Points: " + std::to_string(points.size()) + ")");
  std::lock_guard<std::mutex> port_lock(GetSerialMutex(config_.endpoint));
  logger_->Debug("[ModbusDriver] Acquired port_lock for endpoint: " +
                 config_.endpoint);

  values.clear();
  values.reserve(points.size());

  // 1. Group points by Slave ID and Register Type
  struct PointGroup {
    int slave_id;
    std::string func_type;
    std::vector<const Structs::DataPoint *> points;
  };

  std::map<std::pair<int, std::string>, PointGroup> groups;

  for (const auto &point : points) {
    int slave_id = current_slave_id_;
    if (point.protocol_params.count("slave_id")) {
      try {
        slave_id = std::stoi(point.protocol_params.at("slave_id"));
      } catch (...) {
      }
    }

    std::string func_type = point.data_type; // Default to data_type mapping
    if (point.protocol_params.count("function_code")) {
      try {
        int fc = std::stoi(point.protocol_params.at("function_code"));
        switch (fc) {
        case 1:
          func_type = "COIL";
          break;
        case 2:
          func_type = "DISCRETE_INPUT";
          break;
        case 3:
          func_type = "HOLDING_REGISTER";
          break;
        case 4:
          func_type = "INPUT_REGISTER";
          break;
        default:
          func_type = point.data_type;
          break;
        }
      } catch (...) {
        func_type = point.data_type;
      }
    } else {
      // [BUG FIX] Parse address_string prefix before falling back to heuristic.
      // Modbus 표준: CO:xxx=FC01(Coil), DI:xxx=FC02(Discrete),
      //              HR:xxx=FC03(Holding), IR:xxx=FC04(Input)
      if (!point.address_string.empty()) {
        const std::string &as = point.address_string;
        if (as.size() > 3) {
          std::string prefix = as.substr(0, 3);
          // Normalise to uppercase for robustness
          std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                         ::toupper);
          if (prefix == "CO:")
            func_type = "COIL";
          else if (prefix == "DI:")
            func_type = "DISCRETE_INPUT";
          else if (prefix == "HR:")
            func_type = "HOLDING_REGISTER";
          else if (prefix == "IR:")
            func_type = "INPUT_REGISTER";
          // else: keep data_type-based func_type
        }
      }
      // Final fallback: if still not a known coil type, default to HOLDING
      if (func_type != "COIL" && func_type != "DISCRETE_INPUT" &&
          func_type != "HOLDING_REGISTER" && func_type != "INPUT_REGISTER") {
        func_type = "HOLDING_REGISTER";
      }
    }

    groups[{slave_id, func_type}].slave_id = slave_id;
    groups[{slave_id, func_type}].func_type = func_type;
    groups[{slave_id, func_type}].points.push_back(&point);
  }

  bool any_success = false;

  // 2. Process each group
  for (auto &[key, group] : groups) {
    // Sort by address
    std::sort(group.points.begin(), group.points.end(),
              [](const Structs::DataPoint *a, const Structs::DataPoint *b) {
                return a->address < b->address;
              });

    logger_->Debug("[ModbusDriver] Processing Group - Slave: " +
                   std::to_string(group.slave_id) +
                   ", Func: " + group.func_type);
    SetSlaveId(group.slave_id);

    // Chunking
    size_t current_idx = 0;
    while (current_idx < group.points.size()) {
      // Modbus 주소는 0-65535 범위; uint32_t → uint16_t 절단 방지
      if (group.points[current_idx]->address > 0xFFFF) {
        current_idx++;
        continue; // 범위 초과 주소 skip
      }
      uint16_t start_addr =
          static_cast<uint16_t>(group.points[current_idx]->address);
      uint16_t current_addr = start_addr;
      uint16_t max_count = 120; // Safe Modbus limit

      std::vector<const Structs::DataPoint *> chunk_points;

      // Collect points for this chunk
      size_t next_idx = current_idx;
      uint16_t end_addr = start_addr;

      while (next_idx < group.points.size()) {
        const auto *p = group.points[next_idx];
        // Modbus addresses are 0-65535; clamp to avoid silent truncation
        if (p->address > 0xFFFF) {
          next_idx++;
          continue; // skip out-of-range addresses
        }
        uint16_t p_addr = static_cast<uint16_t>(p->address);

        // Determine register consumption
        uint16_t reg_count = 1;
        if (group.func_type == "HOLDING_REGISTER" ||
            group.func_type == "INPUT_REGISTER") {
          if (p->data_type == "FLOAT32" || p->data_type == "INT32" ||
              p->data_type == "UINT32")
            reg_count = 2;
        }

        // Check continuity and size limit
        if (p_addr > end_addr + 10)
          break; // Gap limit (optimize excessive gaps)
        if ((p_addr + reg_count) - start_addr > max_count)
          break;

        chunk_points.push_back(p);
        end_addr = std::max<uint16_t>(end_addr, p_addr + reg_count);
        next_idx++;
      }

      uint16_t total_count = end_addr - start_addr;
      if (total_count == 0)
        total_count = 1; // Minimal safety

      // Perform Read
      bool success = false;
      std::vector<uint16_t> reg_buffers;
      std::vector<uint8_t> coil_buffers;

      auto start_time = std::chrono::high_resolution_clock::now();

      if (group.func_type == "HOLDING_REGISTER") {
        success = ReadHoldingRegisters(group.slave_id, start_addr, total_count,
                                       reg_buffers);
        if (success) {
          std::string raw_values_str;
          for (size_t i = 0; i < reg_buffers.size(); ++i) {
            raw_values_str += std::to_string(reg_buffers[i]);
            if (i < reg_buffers.size() - 1) {
              raw_values_str += ", ";
            }
          }
          logger_->Info(
              "[ModbusDriver] Raw Holding Register values read for addr " +
              std::to_string(start_addr) + " (count: " +
              std::to_string(total_count) + "): [" + raw_values_str + "]");
        }
        driver_statistics_.IncrementProtocolCounter("register_reads");
      } else if (group.func_type == "INPUT_REGISTER") {
        success = ReadInputRegisters(group.slave_id, start_addr, total_count,
                                     reg_buffers);
        if (success) {
          std::string raw_values_str;
          for (size_t i = 0; i < reg_buffers.size(); ++i) {
            raw_values_str += std::to_string(reg_buffers[i]);
            if (i < reg_buffers.size() - 1) {
              raw_values_str += ", ";
            }
          }
          logger_->Info(
              "[ModbusDriver] Raw Input Register values read for addr " +
              std::to_string(start_addr) + " (count: " +
              std::to_string(total_count) + "): [" + raw_values_str + "]");
        }
        driver_statistics_.IncrementProtocolCounter("register_reads");
      } else if (group.func_type == "COIL") {
        success =
            ReadCoils(group.slave_id, start_addr, total_count, coil_buffers);
        driver_statistics_.IncrementProtocolCounter("coil_reads");
      } else if (group.func_type == "DISCRETE_INPUT") {
        success = ReadDiscreteInputs(group.slave_id, start_addr, total_count,
                                     coil_buffers);
        driver_statistics_.IncrementProtocolCounter("coil_reads");
      }

      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::high_resolution_clock::now() - start_time)
              .count();

      UpdateStats(success, duration, "read_batch");

      // Extract Values
      for (const auto *p : chunk_points) {
        Structs::TimestampedValue tv;
        try {
          tv.point_id = std::stoi(p->id);
        } catch (...) {
          tv.point_id =
              0; // Use 0 for points without numeric IDs (like keep-alive)
        }
        tv.source = p->name; // Set semantic name for Hybrid Strategy
        tv.timestamp = std::chrono::system_clock::now();

        if (success) {
          tv.quality = Structs::DataQuality::GOOD;
          uint16_t offset = p->address - start_addr;

          if (group.func_type == "HOLDING_REGISTER" ||
              group.func_type == "INPUT_REGISTER") {
            if (offset < reg_buffers.size()) {
              tv.value = ExtractValueFromBuffer(reg_buffers, offset, *p);
            } else {
              // Bug33 Fix: 버퍼 범위 초과 → GOOD 대신 BAD quality로 변경
              tv.quality = Structs::DataQuality::BAD;
            }
          } else { // Coils
            if (offset < coil_buffers.size()) {
              tv.value = (coil_buffers[offset] != 0);
            } else {
              tv.quality = Structs::DataQuality::BAD;
            }
          }

          std::string val_str;
          std::visit(
              [&val_str](auto &&v) {
                std::ostringstream oss;
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) {
                  oss << (v ? "true" : "false");
                } else {
                  oss << v;
                }
                val_str = oss.str();
              },
              tv.value);
          logger_->log("worker", PulseOne::Enums::LogLevel::DEBUG,
                       "[ModbusDriver] Read Point " + p->id + " (" + p->name +
                           ") Addr: " + std::to_string(p->address) +
                           " -> Value: " + val_str + " (Quality: GOOD)");

          any_success = true;
        } else {
          tv.quality = Structs::DataQuality::BAD;
          logger_->Warn("[ModbusDriver] Read Point " + p->id + " (" + p->name +
                        ") Addr: " + std::to_string(p->address) +
                        " -> FAILED (Quality: BAD)");
        }
        values.push_back(tv);
      }

      current_idx = next_idx;
    }
  }

  return any_success;
}

bool ModbusDriver::WriteValue(const Structs::DataPoint &point,
                              const Structs::DataValue &value) {
  // 연결 풀링이 활성화된 경우 해당 방식 사용
  if (connection_pool_ && IsConnectionPoolingEnabled()) {
    return PerformWriteWithConnectionPool(point, value);
  }
  return WriteValueImpl(point, value);
}

bool ModbusDriver::WriteValueImpl(const Structs::DataPoint &point,
                                  const Structs::DataValue &value) {
  // 🔍 엔드포인트 잠금
  std::lock_guard<std::mutex> port_lock(GetSerialMutex(config_.endpoint));

  auto start_time = std::chrono::high_resolution_clock::now();

  // 포인트별 Slave ID
  int slave_id = current_slave_id_;
  if (point.protocol_params.count("slave_id")) {
    try {
      slave_id = std::stoi(point.protocol_params.at("slave_id"));
    } catch (...) {
    }
  }
  SetSlaveId(slave_id);

  // ── function_type 결정 ──────────────────────────────────────────────────
  std::string function_type = point.data_type;
  if (point.protocol_params.count("function_code")) {
    try {
      int fc = std::stoi(point.protocol_params.at("function_code"));
      switch (fc) {
      case 1:
      case 2:
      case 5:
        function_type = "COIL";
        break;
      case 3:
      case 4:
      case 6:
      case 16:
        function_type = "HOLDING_REGISTER";
        break;
      default:
        break;
      }
    } catch (...) {
    }
  } else {
    if (!point.address_string.empty() && point.address_string.size() > 3) {
      std::string prefix = point.address_string.substr(0, 3);
      std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
      if (prefix == "CO:" || prefix == "DI:")
        function_type = "COIL";
      else if (prefix == "HR:" || prefix == "IR:")
        function_type = "HOLDING_REGISTER";
    }
    if (function_type == "bool" || function_type == "BOOL")
      function_type = "COIL";
    else if (function_type != "COIL" && function_type != "HOLDING_REGISTER")
      function_type = "HOLDING_REGISTER";
  }

  bool success = false;

  // ── bit_index: MaskWriteRegister ───────────────────────────────────────
  if (point.protocol_params.count("bit_index") &&
      function_type == "HOLDING_REGISTER") {
    try {
      int bit_idx = std::stoi(point.protocol_params.at("bit_index"));
      if (bit_idx >= 0 && bit_idx < 16) {
        // 값을 bool로 변환 (DataVariantToDouble: int32_t/float/double 등 모두
        // 처리)
        bool is_on = (PulseOne::BasicTypes::DataVariantToDouble(value) != 0.0);

        uint16_t and_mask = static_cast<uint16_t>(~(1 << bit_idx));
        uint16_t or_mask = is_on ? static_cast<uint16_t>(1 << bit_idx) : 0;
        success = MaskWriteRegister(slave_id, point.address, and_mask, or_mask);

        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        UpdateStats(success, dur.count(), "write");
        RecordResponseTime(slave_id, dur.count());
        RecordSlaveRequest(slave_id, success, dur.count());
        return success;
      }
    } catch (...) {
    }
  }

  // ── HOLDING_REGISTER 쓰기 ─────────────────────────────────────────────
  if (function_type == "HOLDING_REGISTER") {

    // FLOAT32 / INT32 / UINT32 → 2 레지스터 (FC16)
    if (point.data_type == "FLOAT32" || point.data_type == "INT32" ||
        point.data_type == "UINT32") {

      // 32비트 값 조립 — 역스케일링 + DataVariantToDouble로 안전하게 추출
      // 예: user값=23.5, scaling_factor=0.1 → register값=235.0
      double user_val = PulseOne::BasicTypes::DataVariantToDouble(value);
      double register_val = user_val;
      if (point.scaling_factor != 0.0) {
        register_val = (user_val - point.scaling_offset) / point.scaling_factor;
      }

      uint32_t raw32 = 0;
      if (point.data_type == "FLOAT32") {
        float f = static_cast<float>(register_val);
        std::memcpy(&raw32, &f, 4);
      } else if (point.data_type == "INT32") {
        raw32 = static_cast<uint32_t>(static_cast<int32_t>(register_val));
      } else { // UINT32
        raw32 = static_cast<uint32_t>(register_val);
      }

      // byte_order 적용 (ConvertRegistersToValue와 동일 방식)
      std::string byte_order = "big_endian";
      if (config_.properties.count("byte_order"))
        byte_order = config_.properties.at("byte_order");
      if (point.protocol_params.count("byte_order"))
        byte_order = point.protocol_params.at("byte_order");
      bool swap_words =
          (byte_order == "swapped" || byte_order == "big_endian_swapped" ||
           byte_order == "little_endian");

      std::vector<uint16_t> regs(2);
      if (swap_words) {
        regs[0] = static_cast<uint16_t>(raw32 & 0xFFFF); // low word first
        regs[1] = static_cast<uint16_t>((raw32 >> 16) & 0xFFFF);
      } else {
        regs[0] =
            static_cast<uint16_t>((raw32 >> 16) & 0xFFFF); // high word first
        regs[1] = static_cast<uint16_t>(raw32 & 0xFFFF);
      }

      success = WriteHoldingRegisters(slave_id, point.address, regs);
      driver_statistics_.IncrementProtocolCounter("register_writes", 2);
      RecordRegisterAccess(point.address, false, true);

    } else {
      // 16비트 이하 타입 → 단일 레지스터 (FC06)
      uint16_t modbus_value = 0;
      if (!ConvertToModbusValue(value, point, modbus_value)) {
        SetError(Structs::ErrorCode::DATA_CONVERSION_ERROR,
                 "Failed to convert value to Modbus format");
        return false;
      }
      success = WriteHoldingRegister(slave_id, point.address, modbus_value);
      driver_statistics_.IncrementProtocolCounter("register_writes");
      RecordRegisterAccess(point.address, false, true);
    }

  } else if (function_type == "COIL") {
    // COIL 쓰기
    uint16_t modbus_value = 0;
    if (!ConvertToModbusValue(value, point, modbus_value)) {
      SetError(Structs::ErrorCode::DATA_CONVERSION_ERROR,
               "Failed to convert value to Modbus format");
      return false;
    }
    success = WriteCoil(slave_id, point.address, modbus_value != 0);
    driver_statistics_.IncrementProtocolCounter("coil_writes");
  } else {
    SetError(Structs::ErrorCode::UNSUPPORTED_FUNCTION,
             "Write operation not supported for this data type: " +
                 function_type);
    return false;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  UpdateStats(success, duration.count(), "write");
  RecordResponseTime(slave_id, duration.count());
  RecordSlaveRequest(slave_id, success, duration.count());

  return success;
}

const DriverStatistics &ModbusDriver::GetStatistics() const {
  return driver_statistics_;
}

void ModbusDriver::ResetStatistics() {
  driver_statistics_.total_reads.store(0);
  driver_statistics_.successful_reads.store(0);
  driver_statistics_.failed_reads.store(0);
  driver_statistics_.total_writes.store(0);
  driver_statistics_.successful_writes.store(0);
  driver_statistics_.failed_writes.store(0);
  driver_statistics_.average_response_time = std::chrono::milliseconds(0);
}

std::string ModbusDriver::GetProtocolType() const {
  if (config_.endpoint.find('/') != std::string::npos ||
      config_.endpoint.find("COM") != std::string::npos) {
    return "MODBUS_RTU";
  }
  return "MODBUS_TCP";
}

Enums::DriverStatus ModbusDriver::GetStatus() const {
  // driver_mutex_ 보호 하에 status_ 읽기
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  // 1. 먼저 모듈러스 컨텍스트 확인
  if (!modbus_ctx_) {
    return Enums::DriverStatus::UNINITIALIZED;
  }

  // 2. ConnectionStatus를 DriverStatus로 변환 (ERROR 매크로 회피)
  auto connection_status = status_; // driver_mutex_ 보호 하에 안전

  // 🔥 숫자 값으로 비교해서 Windows ERROR 매크로 회피
  auto status_value = static_cast<uint8_t>(connection_status);

  switch (status_value) {
  case 0:
    return Enums::DriverStatus::STOPPED;
  case 1:
    return Enums::DriverStatus::STARTING;
  case 2:
    if (IsConnected()) {
      return Enums::DriverStatus::RUNNING;
    } else {
      return Enums::DriverStatus::DRIVER_ERROR;
    }
  case 3:
    return Enums::DriverStatus::STARTING;
  case 4:
    return Enums::DriverStatus::STOPPING;
  case 5:
    return Enums::DriverStatus::DRIVER_ERROR;
  case 6:
    return Enums::DriverStatus::DRIVER_ERROR;
  case 7:
    return Enums::DriverStatus::MAINTENANCE;
  default:
    return Enums::DriverStatus::UNINITIALIZED;
  }
}

ErrorInfo ModbusDriver::GetLastError() const { return last_error_; }

// =============================================================================
// 기본 Modbus 통신 (Core 기능) - 에러 처리 강화
// =============================================================================

bool ModbusDriver::ReadHoldingRegisters(int slave_id, uint16_t start_addr,
                                        uint16_t count,
                                        std::vector<uint16_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  values.resize(count);
  logger_->Debug("[ModbusDriver] Calling modbus_read_registers - Slave: " +
                 std::to_string(slave_id) +
                 ", Addr: " + std::to_string(start_addr) +
                 ", Count: " + std::to_string(count));
  int result =
      modbus_read_registers(modbus_ctx_, start_addr, count, values.data());
  logger_->Debug("[ModbusDriver] modbus_read_registers result: " +
                 std::to_string(result));

  if (result == -1) {
    auto error_msg =
        std::string("Read holding registers failed: ") + modbus_strerror(errno);

    // errno에 따라 적절한 에러 코드 분류
    if (errno == ETIMEDOUT || errno == EAGAIN) {
      SetError(Structs::ErrorCode::IO_TIMEOUT, error_msg);
    } else if (errno == ECONNRESET || errno == ECONNABORTED || errno == EPIPE) {
      SetError(Structs::ErrorCode::CONNECTION_LOST, error_msg);
    } else if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
      SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
    } else if (errno == EINVAL || errno == ERANGE) {
      SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
    } else {
      SetError(Structs::ErrorCode::READ_FAILED, error_msg);
    }

    // Modbus 예외 코드 기록
    // libmodbus: Modbus exception는 MODBUS_ENOBASE(112)+exception_code로 저장됨
    // errno 1~8(EPERM~E2BIG)은 OS 에러이며 Modbus 예외가 아님 - 잘못된 분류
    // 제거됨

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("register_reads");
  return true;
}

bool ModbusDriver::ReadInputRegisters(int slave_id, uint16_t start_addr,
                                      uint16_t count,
                                      std::vector<uint16_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  values.resize(count);
  int result = modbus_read_input_registers(modbus_ctx_, start_addr, count,
                                           values.data());

  if (result == -1) {
    auto error_msg =
        std::string("Read input registers failed: ") + modbus_strerror(errno);

    if (errno == ETIMEDOUT || errno == EAGAIN) {
      SetError(Structs::ErrorCode::IO_TIMEOUT, error_msg);
    } else if (errno == ECONNRESET || errno == ECONNABORTED || errno == EPIPE) {
      SetError(Structs::ErrorCode::CONNECTION_LOST, error_msg);
    } else if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
      SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
    } else if (errno == EINVAL || errno == ERANGE) {
      SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
    } else {
      SetError(Structs::ErrorCode::READ_FAILED, error_msg);
    }
    // libmodbus: Modbus exception은 errno=MODBUS_ENOBASE+exc_code로 저장. errno
    // 1~8은 OS 에러이므로 제거됨

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("register_reads");
  return true;
}

bool ModbusDriver::ReadCoils(int slave_id, uint16_t start_addr, uint16_t count,
                             std::vector<uint8_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  values.resize(count);
  int result = modbus_read_bits(modbus_ctx_, start_addr, count, values.data());

  if (result == -1) {
    auto error_msg =
        std::string("Read coils failed: ") + modbus_strerror(errno);
    SetError(Structs::ErrorCode::READ_FAILED, error_msg);

    if (errno > 0 && errno < 255) {
      RecordExceptionCode(static_cast<uint8_t>(errno));
    }

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("coil_reads");
  return true;
}

bool ModbusDriver::ReadDiscreteInputs(int slave_id, uint16_t start_addr,
                                      uint16_t count,
                                      std::vector<uint8_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  values.resize(count);
  int result =
      modbus_read_input_bits(modbus_ctx_, start_addr, count, values.data());

  if (result == -1) {
    auto error_msg =
        std::string("Read discrete inputs failed: ") + modbus_strerror(errno);
    SetError(Structs::ErrorCode::READ_FAILED, error_msg);

    if (errno > 0 && errno < 255) {
      RecordExceptionCode(static_cast<uint8_t>(errno));
    }

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("coil_reads");
  return true;
}

bool ModbusDriver::WriteHoldingRegister(int slave_id, uint16_t address,
                                        uint16_t value) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  int result = modbus_write_register(modbus_ctx_, address, value);

  if (result == -1) {
    auto error_msg =
        std::string("Write holding register failed: ") + modbus_strerror(errno);

    if (errno == ETIMEDOUT || errno == EAGAIN) {
      SetError(Structs::ErrorCode::IO_TIMEOUT, error_msg);
    } else if (errno == ECONNRESET || errno == ECONNABORTED || errno == EPIPE) {
      SetError(Structs::ErrorCode::CONNECTION_LOST, error_msg);
    } else if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
      SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
    } else if (errno == EINVAL || errno == ERANGE) {
      SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
    } else {
      SetError(Structs::ErrorCode::WRITE_FAILED, error_msg);
    }
    // libmodbus: Modbus exception은 errno=MODBUS_ENOBASE+exc_code로 저장. errno
    // 1~8은 OS 에러이므로 제거됨

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("register_writes");
  return true;
}

bool ModbusDriver::WriteHoldingRegisters(int slave_id, uint16_t start_addr,
                                         const std::vector<uint16_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  int result = modbus_write_registers(modbus_ctx_, start_addr, values.size(),
                                      values.data());

  if (result == -1) {
    auto error_msg = std::string("Write holding registers failed: ") +
                     modbus_strerror(errno);

    if (errno == ETIMEDOUT || errno == EAGAIN) {
      SetError(Structs::ErrorCode::IO_TIMEOUT, error_msg);
    } else if (errno == ECONNRESET || errno == ECONNABORTED || errno == EPIPE) {
      SetError(Structs::ErrorCode::CONNECTION_LOST, error_msg);
    } else if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
      SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
    } else if (errno == EINVAL || errno == ERANGE) {
      SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
    } else {
      SetError(Structs::ErrorCode::WRITE_FAILED, error_msg);
    }
    // libmodbus: Modbus exception은 errno=MODBUS_ENOBASE+exc_code로 저장. errno
    // 1~8은 OS 에러이므로 제거됨

    return false;
  }

  // register_writes 카운터는 호출자(WriteValueImpl)에서 관리함 (중복 증가 방지)
  return true;
}

bool ModbusDriver::WriteCoil(int slave_id, uint16_t address, bool value) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  int result = modbus_write_bit(modbus_ctx_, address, value ? 1 : 0);

  if (result == -1) {
    auto error_msg =
        std::string("Write coil failed: ") + modbus_strerror(errno);
    SetError(Structs::ErrorCode::PROTOCOL_ERROR, error_msg);
    // libmodbus: Modbus exception은 errno=MODBUS_ENOBASE+exc_code로 저장. errno
    // 1~8은 OS 에러이므로 제거됨

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("coil_writes");
  return true;
}

bool ModbusDriver::WriteCoils(int slave_id, uint16_t start_addr,
                              const std::vector<uint8_t> &values) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  int result =
      modbus_write_bits(modbus_ctx_, start_addr, values.size(), values.data());

  if (result == -1) {
    auto error_msg =
        std::string("Write coils failed: ") + modbus_strerror(errno);
    SetError(Structs::ErrorCode::PROTOCOL_ERROR, error_msg);
    // libmodbus: Modbus exception은 errno=MODBUS_ENOBASE+exc_code로 저장. errno
    // 1~8은 OS 에러이므로 제거됨

    return false;
  }

  driver_statistics_.IncrementProtocolCounter("coil_writes", values.size());
  return true;
}

bool ModbusDriver::MaskWriteRegister(int slave_id, uint16_t address,
                                     uint16_t and_mask, uint16_t or_mask) {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!EnsureConnection()) {
    return false;
  }

  SetSlaveId(slave_id);

  int result =
      modbus_mask_write_register(modbus_ctx_, address, and_mask, or_mask);

  if (result == -1) {
    auto error_msg =
        std::string("Mask write register failed: ") + modbus_strerror(errno);
    SetError(Structs::ErrorCode::PROTOCOL_ERROR, error_msg);
    return false;
  }

  driver_statistics_.IncrementProtocolCounter("register_writes");
  return true;
}

void ModbusDriver::SetSlaveId(int slave_id) {
  if (modbus_ctx_ && current_slave_id_ != slave_id) {
    modbus_set_slave(modbus_ctx_, slave_id);
    current_slave_id_ = slave_id;
  }
}

int ModbusDriver::GetSlaveId() const { return current_slave_id_; }

// =============================================================================
// 내부 메서드 (Core 기능)
// =============================================================================

bool ModbusDriver::SetupModbusConnection() {
  CleanupConnection();

  // 🔥 DriverConfig의 endpoint 분석하여 TCP/RTU 자동 판단
  if (config_.endpoint.find(':') != std::string::npos) {
    // TCP 방식 - IP:PORT 형태
    auto pos = config_.endpoint.find(':');
    std::string host = config_.endpoint.substr(0, pos);
    int port = 502; // default
    try {
      port = std::stoi(config_.endpoint.substr(pos + 1));
    } catch (...) {
      logger_->Warn("[SetupModbusConnection] Invalid port in endpoint: " +
                    config_.endpoint + " — using default 502");
    }

    // Hostname Resolution for Docker (libmodbus often requires IP)
    struct addrinfo hints, *res;
    int status;
    char ipstr[INET_ADDRSTRLEN];
    std::string resolved_host = host;

    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Force IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host.c_str(), NULL, &hints, &res)) == 0) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
      void *addr = &(ipv4->sin_addr);
      if (inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr)) {
        resolved_host = std::string(ipstr);
        if (host != resolved_host) {
          logger_->Info("RESOLVED Hostname: " + host + " -> " + resolved_host);
        }
      }
      freeaddrinfo(res);
    } else {
      logger_->Warn("Failed to resolve hostname: " + host + " (" +
#ifdef _WIN32
                    std::string(gai_strerrorA(status))
#else
                    std::string(gai_strerror(status))
#endif
                    + ")");
    }

    modbus_ctx_ = modbus_new_tcp(resolved_host.c_str(), port);
    logger_->Info("🔗 Setting up Modbus TCP connection to " + resolved_host +
                  ":" + std::to_string(port));
  } else {
    // RTU 방식 - 시리얼 포트
    // 🔥 RTU 설정은 properties에서 가져오기
    int baud_rate = 9600; // 기본값
    char parity = 'N';
    int data_bits = 8;
    int stop_bits = 1;

    if (config_.properties.count("baud_rate")) {
      try {
        baud_rate = std::stoi(config_.properties.at("baud_rate"));
      } catch (...) {
      }
    }
    if (config_.properties.count("parity")) {
      const auto &par_str = config_.properties.at("parity");
      if (!par_str.empty())
        parity = par_str[0]; // empty이면 기본값 'N' 유지
    }
    if (config_.properties.count("data_bits")) {
      try {
        data_bits = std::stoi(config_.properties.at("data_bits"));
      } catch (...) {
      }
    }
    if (config_.properties.count("stop_bits")) {
      try {
        stop_bits = std::stoi(config_.properties.at("stop_bits"));
      } catch (...) {
      }
    }

    modbus_ctx_ = modbus_new_rtu(config_.endpoint.c_str(), baud_rate, parity,
                                 data_bits, stop_bits);
    logger_->Info("🔗 Setting up Modbus RTU connection to " + config_.endpoint +
                  " (" + std::to_string(baud_rate) + " baud)");
  }

  bool success = modbus_ctx_ != nullptr;
  if (success && current_slave_id_ != -1) {
    modbus_set_slave(modbus_ctx_, current_slave_id_);
  }

  logger_->Info("SetupModbusConnection result: " +
                std::string(success ? "SUCCESS" : "FAILED"));
  return success;
}

void ModbusDriver::CleanupConnection() {
  if (modbus_ctx_) {
    modbus_close(modbus_ctx_);
    modbus_free(modbus_ctx_);
    modbus_ctx_ = nullptr;
  }
}

bool ModbusDriver::EnsureConnection() {
  if (!IsConnected()) {
    return Connect();
  }
  return true;
}

bool ModbusDriver::ReconnectWithRetry(int max_retries) {
  for (int i = 0; i < max_retries; ++i) {
    Disconnect();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100 * (i + 1))); // 백오프

    if (Connect()) {
      return true;
    }
  }
  return false;
}

void ModbusDriver::UpdateStats(bool success, double response_time_ms,
                               const std::string &operation) {
  if (operation == "read") {
    driver_statistics_.total_reads.fetch_add(1);
    if (success) {
      driver_statistics_.successful_reads.fetch_add(1);
    } else {
      driver_statistics_.failed_reads.fetch_add(1);
    }
  } else if (operation == "write") {
    driver_statistics_.total_writes.fetch_add(1);
    if (success) {
      driver_statistics_.successful_writes.fetch_add(1);
    } else {
      driver_statistics_.failed_writes.fetch_add(1);
    }
  }

  // 응답 시간 업데이트 (간단한 이동 평균)
  auto current_avg = driver_statistics_.average_response_time.count();
  auto new_avg = static_cast<long>((current_avg + response_time_ms) / 2.0);
  driver_statistics_.average_response_time = std::chrono::milliseconds(new_avg);
}

void ModbusDriver::SetError(Structs::ErrorCode code,
                            const std::string &message) {
  last_error_.code = code;
  last_error_.message = message;

  // 🔥 연결 단절/실패에만 재연결 유도
  // READ_FAILED/WRITE_FAILED는 일시적 오류일 수 있으므로 포함하지 않음
  // (Modbus 예외 코드 응답도 READ_FAILED로 분류되기 때문)
  if (code == Structs::ErrorCode::CONNECTION_LOST ||
      code == Structs::ErrorCode::CONNECTION_FAILED) {

    if (is_connected_.load()) {
      is_connected_.store(false);
      logger_->Warn("ModbusDriver: Connection flagged as LOST due to error: " +
                    message);
    }
  }
}

Structs::DataValue
ModbusDriver::ConvertModbusValue(const Structs::DataPoint &point,
                                 uint16_t raw_value) const {
  Structs::DataValue result;

  // 🔥 DataPoint의 scaling_factor와 scaling_offset 활용
  double scaled_value = static_cast<double>(raw_value) * point.scaling_factor +
                        point.scaling_offset;

  // data_type 문자열을 기반으로 변환
  if (point.data_type == "INT16") {
    result = static_cast<int16_t>(scaled_value);
  } else if (point.data_type == "UINT16") {
    result = static_cast<uint16_t>(scaled_value);
  } else if (point.data_type == "FLOAT") {
    result = static_cast<float>(scaled_value);
  } else if (point.data_type == "DOUBLE") {
    result = scaled_value;
  } else if (point.data_type == "BOOL" || point.data_type == "COIL" ||
             point.data_type == "bool") {
    result = (raw_value != 0);
  } else {
    // 기본: double로 반환 (DataVariant에 unsigned int 타입 없음)
    result = scaled_value;
  }

  return result;
}

bool ModbusDriver::ConvertToModbusValue(const Structs::DataValue &value,
                                        const Structs::DataPoint &point,
                                        uint16_t &modbus_value) const {
  // 🔥 역스케일링 적용 (scaling_factor=0 방지)
  auto apply_reverse_scaling = [&](double val) -> uint16_t {
    double reverse_scaled = val;
    if (point.scaling_factor != 0.0) {
      reverse_scaled = (val - point.scaling_offset) / point.scaling_factor;
    }
    return static_cast<uint16_t>(
        std::max(0.0, std::min(65535.0, reverse_scaled)));
  };

  // data_type 문자열을 기반으로 변환
  // DataVariant = std::variant<bool, int16_t, uint16_t, int32_t, uint32_t,
  //   int64_t, uint64_t, float, double, std::string>
  // DataVariantToDouble로 safe 변환 후 역스케일링 적용
  if (point.data_type == "INT16" || point.data_type == "UINT16") {
    modbus_value =
        apply_reverse_scaling(PulseOne::BasicTypes::DataVariantToDouble(value));
  } else if (point.data_type == "FLOAT") {
    // FLOAT(단정밀도)는 float로 꺼내고, 없으면 double 경유
    float float_val;
    if (std::holds_alternative<float>(value))
      float_val = std::get<float>(value);
    else
      float_val =
          static_cast<float>(PulseOne::BasicTypes::DataVariantToDouble(value));
    modbus_value = apply_reverse_scaling(float_val);
  } else if (point.data_type == "BOOL" || point.data_type == "COIL" ||
             point.data_type == "bool") {
    modbus_value =
        (PulseOne::BasicTypes::DataVariantToDouble(value) != 0.0) ? 1 : 0;
  } else {
    return false;
  }

  return true;
}

// =============================================================================
// Start/Stop 메서드 구현
// =============================================================================

bool ModbusDriver::Start() {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (is_started_) {
    if (logger_) {
      logger_->Debug("ModbusDriver already started for endpoint: " +
                     config_.endpoint);
    }
    return true;
  }

  if (logger_) {
    logger_->Info("🚀 Starting ModbusDriver for endpoint: " + config_.endpoint);
  }

  try {
    // 연결 상태 확인 및 연결 시도
    if (!IsConnected()) {
      if (!Connect()) {
        if (logger_) {
          logger_->Error("Failed to connect during ModbusDriver::Start()");
        }
        return false;
      }
    }

    // 시작 상태로 변경
    is_started_ = true;
    status_ = PulseOne::Enums::ConnectionStatus::CONNECTED;

    // 통계 시작 시간 기록
    driver_statistics_.start_time = std::chrono::system_clock::now();

    if (logger_) {
      logger_->Info("✅ ModbusDriver started successfully");
    }

    return true;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("Exception in ModbusDriver::Start(): " +
                     std::string(e.what()));
    }
    return false;
  }
}

bool ModbusDriver::Stop() {
  std::lock_guard<std::recursive_mutex> lock(driver_mutex_);

  if (!is_started_) {
    if (logger_) {
      logger_->Debug("ModbusDriver already stopped for endpoint: " +
                     config_.endpoint);
    }
    return true;
  }

  if (logger_) {
    logger_->Info("🛑 Stopping ModbusDriver for endpoint: " + config_.endpoint);
  }

  try {
    // 연결 해제
    if (IsConnected()) {
      Disconnect();
    }

    // 중지 상태로 변경
    is_started_ = false;
    status_ = PulseOne::Enums::ConnectionStatus::DISCONNECTED;

    if (logger_) {
      logger_->Info("✅ ModbusDriver stopped successfully");
    }

    return true;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("Exception in ModbusDriver::Stop(): " +
                     std::string(e.what()));
    }
    return false;
  }
}

// =============================================================================
// 고급 기능 메서드들 (기존과 동일하게 유지)
// =============================================================================

bool ModbusDriver::EnableDiagnostics(bool packet_logging, bool console_output) {
  if (!diagnostics_) {
    diagnostics_ = std::make_unique<ModbusDiagnostics>(this);
    if (!diagnostics_) {
      return false;
    }
  }

  diagnostics_->EnablePacketLogging(packet_logging);
  diagnostics_->EnableConsoleOutput(console_output);
  return true;
}

void ModbusDriver::DisableDiagnostics() { diagnostics_.reset(); }

bool ModbusDriver::IsDiagnosticsEnabled() const {
  return diagnostics_ != nullptr;
}

std::string ModbusDriver::GetDiagnosticsJSON() const {
  if (diagnostics_) {
    return diagnostics_->GetDiagnosticsJSON();
  }

  // 기본 정보만 반환
  std::ostringstream oss;
  oss << "{"
      << "\"diagnostics\":\"disabled\","
      << "\"basic_stats\":{"
      << "\"total_reads\":" << driver_statistics_.total_reads.load() << ","
      << "\"successful_reads\":" << driver_statistics_.successful_reads.load()
      << ","
      << "\"failed_reads\":" << driver_statistics_.failed_reads.load() << ","
      << "\"total_writes\":" << driver_statistics_.total_writes.load() << ","
      << "\"successful_writes\":" << driver_statistics_.successful_writes.load()
      << ","
      << "\"failed_writes\":" << driver_statistics_.failed_writes.load()
      << "}}";

  return oss.str();
}

std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
  if (diagnostics_) {
    return diagnostics_->GetDiagnostics();
  }

  return {{"status", "disabled"}};
}

std::vector<uint64_t> ModbusDriver::GetResponseTimeHistogram() const {
  if (diagnostics_) {
    return diagnostics_->GetResponseTimeHistogram();
  }

  return {};
}

double ModbusDriver::GetCrcErrorRate() const {
  if (diagnostics_) {
    return diagnostics_->GetCrcErrorRate();
  }

  return 0.0;
}

// =============================================================================
// 연결 풀링 관련 메서드들
// =============================================================================

bool ModbusDriver::EnableConnectionPooling(size_t pool_size,
                                           int timeout_seconds) {
  if (!connection_pool_) {
    connection_pool_ = std::make_unique<ModbusConnectionPool>(this);
    if (!connection_pool_) {
      return false;
    }
  }

  return connection_pool_->EnableConnectionPooling(pool_size, timeout_seconds);
}

void ModbusDriver::DisableConnectionPooling() { connection_pool_.reset(); }

bool ModbusDriver::IsConnectionPoolingEnabled() const {
  return connection_pool_ != nullptr && connection_pool_->IsEnabled();
}

bool ModbusDriver::EnableAutoScaling(double load_threshold,
                                     size_t max_connections) {
  if (!connection_pool_) {
    EnableConnectionPooling(); // 먼저 풀링 활성화
  }

  return connection_pool_->EnableAutoScaling(load_threshold, max_connections);
}

void ModbusDriver::DisableAutoScaling() {
  if (connection_pool_) {
    connection_pool_->DisableAutoScaling();
  }
}

ConnectionPoolStats ModbusDriver::GetConnectionPoolStats() const {
  if (connection_pool_ && IsConnectionPoolingEnabled()) {
    return connection_pool_->GetConnectionPoolStats();
  }

  return ConnectionPoolStats{};
}

// =============================================================================
// 고급 기능 델리게이트 메서드들
// =============================================================================

void ModbusDriver::RecordExceptionCode(uint8_t exception_code) {
  driver_statistics_.IncrementProtocolCounter("exception_codes");

  if (diagnostics_) {
    diagnostics_->RecordExceptionCode(exception_code);
  }
}

void ModbusDriver::RecordCrcCheck(bool crc_valid) {
  if (!crc_valid) {
    driver_statistics_.IncrementProtocolCounter("crc_errors");
  }

  if (diagnostics_) {
    diagnostics_->RecordCrcCheck(crc_valid);
  }
}

void ModbusDriver::RecordResponseTime(int slave_id, uint32_t response_time_ms) {
  if (diagnostics_) {
    diagnostics_->RecordResponseTime(slave_id, response_time_ms);
  }
}

void ModbusDriver::RecordRegisterAccess(uint16_t address, bool is_read,
                                        bool is_write) {
  if (diagnostics_) {
    diagnostics_->RecordRegisterAccess(address, is_read, is_write);
  }
}

void ModbusDriver::RecordSlaveRequest(int slave_id, bool success,
                                      uint32_t response_time_ms) {
  if (diagnostics_) {
    diagnostics_->RecordSlaveRequest(slave_id, success, response_time_ms);
  }
}

bool ModbusDriver::PerformReadWithConnectionPool(
    const std::vector<Structs::DataPoint> &points,
    std::vector<Structs::TimestampedValue> &values) {
  if (connection_pool_) {
    return connection_pool_->PerformBatchRead(points, values);
  }

  // 풀이 없으면 일반 방식으로 fallback
  return ReadValuesImpl(points, values);
}

bool ModbusDriver::PerformWriteWithConnectionPool(
    const Structs::DataPoint &point, const Structs::DataValue &value) {
  if (connection_pool_) {
    return connection_pool_->PerformWrite(point, value);
  }

  // 풀이 없으면 일반 방식으로 fallback
  return WriteValueImpl(point, value);
}

bool ModbusDriver::SwitchToBackupEndpoint() {
  if (failover_) {
    return failover_->SwitchToBackup();
  }

  return false;
}

bool ModbusDriver::CheckPrimaryEndpointRecovery() {
  if (failover_) {
    return failover_->CheckPrimaryRecovery();
  }

  return false;
}

// =============================================================================
// 나머지 고급 기능 스텁들 (향후 구현 예정)
// =============================================================================

bool ModbusDriver::EnableFailover(int failure_threshold,
                                  int recovery_check_interval) {
  (void)failure_threshold;
  (void)recovery_check_interval;
  return false;
}

void ModbusDriver::DisableFailover() { failover_.reset(); }

bool ModbusDriver::IsFailoverEnabled() const { return failover_ != nullptr; }

bool ModbusDriver::AddBackupEndpoint(const std::string &endpoint) {
  if (failover_) {
    return failover_->AddBackupEndpoint(endpoint);
  }
  return false;
}

void ModbusDriver::RemoveBackupEndpoint(const std::string &endpoint) {
  if (failover_) {
    failover_->RemoveBackupEndpoint(endpoint);
  }
}

std::vector<std::string> ModbusDriver::GetActiveEndpoints() const {
  if (failover_) {
    return failover_->GetActiveEndpoints();
  }
  return {config_.endpoint};
}

bool ModbusDriver::EnablePerformanceMode() { return false; }

void ModbusDriver::DisablePerformanceMode() { performance_.reset(); }

bool ModbusDriver::IsPerformanceModeEnabled() const {
  return performance_ != nullptr;
}

void ModbusDriver::SetReadBatchSize(size_t batch_size) {
  if (performance_) {
    performance_->SetReadBatchSize(batch_size);
  }
}

void ModbusDriver::SetWriteBatchSize(size_t batch_size) {
  if (performance_) {
    performance_->SetWriteBatchSize(batch_size);
  }
}

int ModbusDriver::TestConnectionQuality() {
  if (performance_) {
    return performance_->TestConnectionQuality();
  }
  return -1;
}

bool ModbusDriver::StartRealtimeMonitoring(int interval_seconds) {
  if (performance_) {
    return performance_->StartRealtimeMonitoring(interval_seconds);
  }
  return false;
}

void ModbusDriver::StopRealtimeMonitoring() {
  if (performance_) {
    performance_->StopRealtimeMonitoring();
  }
}

// Core Modbus methods - REMOVED (implemented earlier)
// ErrorInfo ModbusDriver::GetLastError() const { return ErrorInfo(); }
// ... duplicate stubs removed ...

Structs::DataValue
ModbusDriver::ExtractValueFromBuffer(const std::vector<uint16_t> &buffer,
                                     size_t offset,
                                     const Structs::DataPoint &point) {
  if (offset >= buffer.size())
    return Structs::DataValue{};

  // Byte Order
  std::string byte_order = "big_endian";
  if (config_.properties.count("byte_order"))
    byte_order = config_.properties.at("byte_order");
  if (point.protocol_params.count("byte_order"))
    byte_order = point.protocol_params.at("byte_order");

  // 2-Register Types (32-bit)
  if (point.data_type == "FLOAT32" || point.data_type == "INT32" ||
      point.data_type == "UINT32") {
    if (offset + 1 >= buffer.size())
      return Structs::DataValue{};

    uint32_t combined;
    // Handle swap
    if (byte_order == "swapped" || byte_order == "big_endian_swapped" ||
        byte_order == "little_endian") {
      combined =
          (static_cast<uint32_t>(buffer[offset + 1]) << 16) | buffer[offset];
    } else {
      combined =
          (static_cast<uint32_t>(buffer[offset]) << 16) | buffer[offset + 1];
    }

    if (point.data_type == "FLOAT32") {
      float f;
      std::memcpy(&f, &combined, sizeof(f));
      double raw = static_cast<double>(f);
      return raw * point.scaling_factor + point.scaling_offset;
    } else if (point.data_type == "INT32") {
      double raw = static_cast<double>(static_cast<int32_t>(combined));
      return raw * point.scaling_factor + point.scaling_offset;
    } else { // UINT32
      double raw = static_cast<double>(combined);
      return raw * point.scaling_factor + point.scaling_offset;
    }
  }
  uint16_t reg = buffer[offset];

  // 🔥 Bit Range 추출: bit_start~bit_end 범위를 하나의 정수값으로 추출
  // 예: bit_start=0, bit_end=3 → 하위 4비트 → 0~15 값
  if (point.protocol_params.count("bit_start") &&
      point.protocol_params.count("bit_end")) {
    try {
      int bs = std::stoi(point.protocol_params.at("bit_start"));
      int be = std::stoi(point.protocol_params.at("bit_end"));
      if (bs >= 0 && be < 16 && bs <= be) {
        // 마스크 생성: bs~be 비트만 추출
        uint16_t mask = static_cast<uint16_t>(((1 << (be - bs + 1)) - 1) << bs);
        uint16_t extracted = (reg & mask) >> bs;
        double val = static_cast<double>(extracted);
        return val * point.scaling_factor + point.scaling_offset;
      }
    } catch (...) {
    }
  }

  // 🔥 Bit Index 추출: 단일 비트 → bool / 0 or 1
  if (point.protocol_params.count("bit_index")) {
    try {
      int bit_idx = std::stoi(point.protocol_params.at("bit_index"));
      if (bit_idx >= 0 && bit_idx < 16) {
        bool bit_val = (reg & (1 << bit_idx)) != 0;
        if (point.data_type == "BOOL" || point.data_type == "COIL" ||
            point.data_type == "bool") {
          return bit_val;
        } else {
          double numeric_val = bit_val ? 1.0 : 0.0;
          return numeric_val * point.scaling_factor + point.scaling_offset;
        }
      }
    } catch (...) {
    }
  }

  double scaled_value;
  // INT16 check
  if (point.data_type == "INT16") {
    scaled_value = static_cast<double>(static_cast<int16_t>(reg));
  } else if (point.data_type == "BOOL" || point.data_type == "COIL" ||
             point.data_type == "bool") {
    return (reg != 0);
  } else {
    scaled_value = static_cast<double>(reg);
  }

  // 🔥 Apply Scaling
  return scaled_value * point.scaling_factor + point.scaling_offset;
}

// Advanced feature stubs - REMOVED (implemented in helper classes)

} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Entry Point
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void RegisterPlugin() {
  // 1. DB에 프로토콜 정보 자동 등록 (없을 경우)
  try {
    auto &repo_factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();
    if (protocol_repo) {
      if (!protocol_repo->findByType("MODBUS_TCP").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("MODBUS_TCP");
        entity.setDisplayName("Modbus TCP");
        entity.setCategory("industrial");
        entity.setDescription("Modbus over TCP/IP Protocol Driver");
        entity.setDefaultPort(502);
        protocol_repo->save(entity);
      }
      if (!protocol_repo->findByType("MODBUS_RTU").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("MODBUS_RTU");
        entity.setDisplayName("Modbus RTU");
        entity.setCategory("industrial");
        entity.setDescription("Modbus Serial (RTU/ASCII) Protocol Driver");
        entity.setUsesSerial(true);
        protocol_repo->save(entity);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[ModbusDriver] DB Registration failed: " << e.what()
              << std::endl;
  }

  // 2. 메모리 Factory에 드라이버 생성자 등록
  // 일반 MODBUS 드라이버 등록 (설정에 따라 TCP/RTU 결정)
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "MODBUS",
      []() { return std::make_unique<PulseOne::Drivers::ModbusDriver>(); });

  // TCP 드라이버 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "MODBUS_TCP",
      []() { return std::make_unique<PulseOne::Drivers::ModbusDriver>(); });

  // RTU 드라이버 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver(
      "MODBUS_RTU",
      []() { return std::make_unique<PulseOne::Drivers::ModbusDriver>(); });
}

// Legacy wrapper for static linking
void RegisterModbusDriver() { RegisterPlugin(); }
}
#endif