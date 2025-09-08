// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// Modbus 드라이버 - 완전한 DriverConfig 매핑 버전 (하드코딩 제거 완료)
// =============================================================================

#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Modbus/ModbusDiagnostics.h"
#include "Drivers/Modbus/ModbusConnectionPool.h"
#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusPerformance.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Drivers {

// 타입 별칭 정의
using ProtocolType = PulseOne::Enums::ProtocolType;
using ErrorInfo = PulseOne::Structs::ErrorInfo;
using TimestampedValue = PulseOne::Structs::TimestampedValue;

// =============================================================================
// 생성자/소멸자
// =============================================================================

ModbusDriver::ModbusDriver()
    : modbus_ctx_(nullptr)
    , driver_statistics_("MODBUS")
    , is_connected_(false)
    , current_slave_id_(1)
    , diagnostics_(nullptr)
    , connection_pool_(nullptr)
    , failover_(nullptr)
    , performance_(nullptr)
{
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

bool ModbusDriver::Initialize(const DriverConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    config_ = config;
    
    // 🔥 DriverConfig에서 직접 설정 가져오기
    logger_->Info("🔧 Initializing ModbusDriver with DriverConfig data:");
    logger_->Info("  - Endpoint: " + config_.endpoint);
    logger_->Info("  - Timeout: " + std::to_string(config_.timeout_ms) + "ms");
    logger_->Info("  - Retry Count: " + std::to_string(config_.retry_count));
    logger_->Info("  - Polling Interval: " + std::to_string(config_.polling_interval_ms) + "ms");
    
    // Modbus 컨텍스트 설정
    if (!SetupModbusConnection()) {
        SetError(Structs::ErrorCode::CONFIGURATION_ERROR, "Failed to setup Modbus connection");
        return false;
    }
    
    // 🔥 DriverConfig의 timeout_ms 직접 사용
    uint32_t timeout_ms = config_.timeout_ms;
    uint32_t timeout_sec = timeout_ms / 1000;
    uint32_t timeout_usec = (timeout_ms % 1000) * 1000;
    
    modbus_set_response_timeout(modbus_ctx_, timeout_sec, timeout_usec);
    
    // 🔥 byte timeout도 DriverConfig 기반으로 설정
    uint32_t byte_timeout_ms = timeout_ms / 2; // 기본값: response timeout의 절반
    if (config_.properties.count("byte_timeout_ms")) {
        byte_timeout_ms = std::stoi(config_.properties.at("byte_timeout_ms"));
    }
    uint32_t byte_timeout_usec = (byte_timeout_ms % 1000) * 1000;
    modbus_set_byte_timeout(modbus_ctx_, 0, byte_timeout_usec);
    
    // 🔥 디버그 모드 설정 (properties에서)
    if (config_.properties.count("debug_enabled") && 
        config_.properties.at("debug_enabled") == "true") {
        modbus_set_debug(modbus_ctx_, TRUE);
        logger_->Info("  - Debug mode enabled");
    }
    
    // 🔥 기본 Slave ID 설정 (properties에서)
    if (config_.properties.count("slave_id")) {
        current_slave_id_ = std::stoi(config_.properties.at("slave_id"));
        logger_->Info("  - Default Slave ID: " + std::to_string(current_slave_id_));
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
        SetError(Structs::ErrorCode::CONNECTION_FAILED, "Modbus context not initialized");
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
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            UpdateStats(true, duration.count(), "connect");
            
            logger_->Info("✅ Modbus connection established to " + config_.endpoint + 
                         " (attempt " + std::to_string(attempt + 1) + "/" + std::to_string(max_retries + 1) + ")");
            return true;
        }
        
        // 마지막 시도가 아니면 재시도
        if (attempt < max_retries) {
            // 🔥 DriverConfig 기반 백오프 계산
            uint32_t retry_delay = 100 * (attempt + 1); // 기본 백오프
            if (config_.properties.count("retry_delay_ms")) {
                retry_delay = std::stoi(config_.properties.at("retry_delay_ms")) * (attempt + 1);
            }
            
            logger_->Warn("Connection attempt " + std::to_string(attempt + 1) + " failed, retrying in " + 
                         std::to_string(retry_delay) + "ms...");
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay));
        }
    }
    
    // 모든 시도 실패
    auto error_msg = std::string("Connection failed after ") + std::to_string(max_retries + 1) + 
                    " attempts: " + modbus_strerror(errno);
    SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
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

bool ModbusDriver::IsConnected() const {
    return is_connected_.load();
}

bool ModbusDriver::ReadValues(const std::vector<Structs::DataPoint>& points, 
                              std::vector<Structs::TimestampedValue>& values) {
    // 연결 풀링이 활성화된 경우 해당 방식 사용
    if (connection_pool_ && IsConnectionPoolingEnabled()) {
        return PerformReadWithConnectionPool(points, values);
    }
    
    values.clear();
    values.reserve(points.size());
    
    for (const auto& point : points) {
        std::vector<uint16_t> raw_values;
        bool success = false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 🔥 포인트별 Slave ID 가져오기 (protocol_params 또는 기본값)
        int slave_id = current_slave_id_; // DriverConfig에서 설정된 기본값
        if (point.protocol_params.count("slave_id")) {
            slave_id = std::stoi(point.protocol_params.at("slave_id"));
        }
        SetSlaveId(slave_id);
        
        // 🔥 포인트별 function_code 확인 (data_type 대신 사용)
        std::string function_type = point.data_type; // 기본값
        if (point.protocol_params.count("function_code")) {
            int func_code = std::stoi(point.protocol_params.at("function_code"));
            switch (func_code) {
                case 1: function_type = "COIL"; break;
                case 2: function_type = "DISCRETE_INPUT"; break;
                case 3: function_type = "HOLDING_REGISTER"; break;
                case 4: function_type = "INPUT_REGISTER"; break;
                default: function_type = point.data_type; break;
            }
        }
        
        // 데이터 타입에 따라 읽기
        if (function_type == "HOLDING_REGISTER") {
            success = ReadHoldingRegisters(slave_id, point.address, 1, raw_values);
            driver_statistics_.IncrementProtocolCounter("register_reads");
            RecordRegisterAccess(point.address, true, false);
        } else if (function_type == "INPUT_REGISTER") {
            success = ReadInputRegisters(slave_id, point.address, 1, raw_values);
            driver_statistics_.IncrementProtocolCounter("register_reads");
            RecordRegisterAccess(point.address, true, false);
        } else if (function_type == "COIL" || function_type == "DISCRETE_INPUT") {
            std::vector<uint8_t> coil_values;
            if (function_type == "COIL") {
                success = ReadCoils(slave_id, point.address, 1, coil_values);
            } else {
                success = ReadDiscreteInputs(slave_id, point.address, 1, coil_values);
            }
            
            if (success && !coil_values.empty()) {
                raw_values.push_back(coil_values[0] ? 1 : 0);
            }
            driver_statistics_.IncrementProtocolCounter("coil_reads");
        } else {
            // 알 수 없는 타입은 기본적으로 HOLDING_REGISTER로 시도
            success = ReadHoldingRegisters(slave_id, point.address, 1, raw_values);
            driver_statistics_.IncrementProtocolCounter("register_reads");
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        UpdateStats(success, duration.count(), "read");
        RecordResponseTime(slave_id, duration.count());
        RecordSlaveRequest(slave_id, success, duration.count());
        
        if (success && !raw_values.empty()) {
            Structs::TimestampedValue timestamped_value;
            timestamped_value.value = ConvertModbusValue(point, raw_values[0]);
            timestamped_value.timestamp = std::chrono::system_clock::now();
            timestamped_value.quality = Structs::DataQuality::GOOD;
            
            values.push_back(timestamped_value);
        } else {
            // 실패한 경우에도 BAD 품질로 값 추가
            Structs::TimestampedValue timestamped_value;
            timestamped_value.value = Structs::DataValue{}; // 빈 variant
            timestamped_value.timestamp = std::chrono::system_clock::now();
            timestamped_value.quality = Structs::DataQuality::BAD;
            
            values.push_back(timestamped_value);
        }
    }
    
    return !values.empty();
}

bool ModbusDriver::WriteValue(const Structs::DataPoint& point, 
                              const Structs::DataValue& value) {
    // 연결 풀링이 활성화된 경우 해당 방식 사용
    if (connection_pool_ && IsConnectionPoolingEnabled()) {
        return PerformWriteWithConnectionPool(point, value);
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 🔥 포인트별 Slave ID 가져오기
    int slave_id = current_slave_id_; // DriverConfig에서 설정된 기본값
    if (point.protocol_params.count("slave_id")) {
        slave_id = std::stoi(point.protocol_params.at("slave_id"));
    }
    SetSlaveId(slave_id);
    
    bool success = false;
    uint16_t modbus_value;
    
    // 데이터 변환
    if (!ConvertToModbusValue(value, point, modbus_value)) {
        SetError(Structs::ErrorCode::DATA_CONVERSION_ERROR, "Failed to convert value to Modbus format");
        return false;
    }
    
    // 🔥 포인트별 function_code 확인
    std::string function_type = point.data_type; // 기본값
    if (point.protocol_params.count("function_code")) {
        int func_code = std::stoi(point.protocol_params.at("function_code"));
        switch (func_code) {
            case 5: function_type = "COIL"; break;
            case 6: 
            case 16: function_type = "HOLDING_REGISTER"; break;
            default: function_type = point.data_type; break;
        }
    }
    
    // 데이터 타입에 따라 쓰기
    if (function_type == "HOLDING_REGISTER") {
        success = WriteHoldingRegister(slave_id, point.address, modbus_value);
        driver_statistics_.IncrementProtocolCounter("register_writes");
        RecordRegisterAccess(point.address, false, true);
    } else if (function_type == "COIL") {
        success = WriteCoil(slave_id, point.address, modbus_value != 0);
        driver_statistics_.IncrementProtocolCounter("coil_writes");
    } else {
        SetError(Structs::ErrorCode::UNSUPPORTED_FUNCTION, 
                "Write operation not supported for this data type: " + function_type);
        return false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    UpdateStats(success, duration.count(), "write");
    RecordResponseTime(slave_id, duration.count());
    RecordSlaveRequest(slave_id, success, duration.count());
    
    return success;
}

const DriverStatistics& ModbusDriver::GetStatistics() const {
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

ProtocolType ModbusDriver::GetProtocolType() const {
    return ProtocolType::MODBUS_TCP;
}

Enums::DriverStatus ModbusDriver::GetStatus() const {
    // 1. 먼저 모듈러스 컨텍스트 확인
    if (!modbus_ctx_) {
        return Enums::DriverStatus::UNINITIALIZED;
    }
    
    // 2. ConnectionStatus를 DriverStatus로 변환 (ERROR 매크로 회피)
    auto connection_status = status_;  // 직접 접근 (atomic 아님)
    
    // 🔥 숫자 값으로 비교해서 Windows ERROR 매크로 회피
    auto status_value = static_cast<uint8_t>(connection_status);
    
    switch (status_value) {
        case 0:  // DISCONNECTED = 0
            return Enums::DriverStatus::STOPPED;
            
        case 1:  // CONNECTING = 1
            return Enums::DriverStatus::STARTING;
            
        case 2:  // CONNECTED = 2
            // 연결되어 있으면 실제 연결 상태도 확인
            if (IsConnected()) {
                return Enums::DriverStatus::RUNNING;
            } else {
                return Enums::DriverStatus::DRIVER_ERROR;  // 연결 상태 불일치
            }
            
        case 3:  // RECONNECTING = 3
            return Enums::DriverStatus::STARTING;
            
        case 4:  // DISCONNECTING = 4
            return Enums::DriverStatus::STOPPING;
            
        case 5:  // ERROR = 5 (숫자로 비교해서 매크로 충돌 회피)
            return Enums::DriverStatus::DRIVER_ERROR;
            
        case 6:  // TIMEOUT = 6
            return Enums::DriverStatus::DRIVER_ERROR;
            
        case 7:  // MAINTENANCE = 7
            return Enums::DriverStatus::MAINTENANCE;
            
        default:
            return Enums::DriverStatus::UNINITIALIZED;
    }
}

ErrorInfo ModbusDriver::GetLastError() const {
    return last_error_;
}

// =============================================================================
// 기본 Modbus 통신 (Core 기능) - 에러 처리 강화
// =============================================================================

bool ModbusDriver::ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                                         std::vector<uint16_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    values.resize(count);
    int result = modbus_read_registers(modbus_ctx_, start_addr, count, values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Read holding registers failed: ") + modbus_strerror(errno);
        
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
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("register_reads");
    return true;
}

bool ModbusDriver::ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                                      std::vector<uint16_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    values.resize(count);
    int result = modbus_read_input_registers(modbus_ctx_, start_addr, count, values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Read input registers failed: ") + modbus_strerror(errno);
        
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
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("register_reads");
    return true;
}

bool ModbusDriver::ReadCoils(int slave_id, uint16_t start_addr, uint16_t count, 
                             std::vector<uint8_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    values.resize(count);
    int result = modbus_read_bits(modbus_ctx_, start_addr, count, values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Read coils failed: ") + modbus_strerror(errno);
        SetError(Structs::ErrorCode::READ_FAILED, error_msg);
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("coil_reads");
    return true;
}

bool ModbusDriver::ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, 
                                      std::vector<uint8_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    values.resize(count);
    int result = modbus_read_input_bits(modbus_ctx_, start_addr, count, values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Read discrete inputs failed: ") + modbus_strerror(errno);
        SetError(Structs::ErrorCode::READ_FAILED, error_msg);
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("coil_reads");
    return true;
}

bool ModbusDriver::WriteHoldingRegister(int slave_id, uint16_t address, uint16_t value) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    int result = modbus_write_register(modbus_ctx_, address, value);
    
    if (result == -1) {
        auto error_msg = std::string("Write holding register failed: ") + modbus_strerror(errno);
        
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
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("register_writes");
    return true;
}

bool ModbusDriver::WriteHoldingRegisters(int slave_id, uint16_t start_addr, 
                                         const std::vector<uint16_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    int result = modbus_write_registers(modbus_ctx_, start_addr, values.size(), values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Write holding registers failed: ") + modbus_strerror(errno);
        
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
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("register_writes", values.size());
    return true;
}

bool ModbusDriver::WriteCoil(int slave_id, uint16_t address, bool value) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    int result = modbus_write_bit(modbus_ctx_, address, value ? 1 : 0);
    
    if (result == -1) {
        auto error_msg = std::string("Write coil failed: ") + modbus_strerror(errno);
        SetError(Structs::ErrorCode::PROTOCOL_ERROR, error_msg);
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("coil_writes");
    return true;
}

bool ModbusDriver::WriteCoils(int slave_id, uint16_t start_addr, 
                              const std::vector<uint8_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    int result = modbus_write_bits(modbus_ctx_, start_addr, values.size(), values.data());
    
    if (result == -1) {
        auto error_msg = std::string("Write coils failed: ") + modbus_strerror(errno);
        SetError(Structs::ErrorCode::PROTOCOL_ERROR, error_msg);
        
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            if (errno >= 1 && errno <= 8) {
                SetError(Structs::ErrorCode::PROTOCOL_ERROR, 
                        "Modbus exception code " + std::to_string(errno) + ": " + error_msg);
            }
        }
        
        return false;
    }
    
    driver_statistics_.IncrementProtocolCounter("coil_writes", values.size());
    return true;
}

void ModbusDriver::SetSlaveId(int slave_id) {
    if (modbus_ctx_ && current_slave_id_ != slave_id) {
        modbus_set_slave(modbus_ctx_, slave_id);
        current_slave_id_ = slave_id;
    }
}

int ModbusDriver::GetSlaveId() const {
    return current_slave_id_;
}

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
        int port = std::stoi(config_.endpoint.substr(pos + 1));
        
        modbus_ctx_ = modbus_new_tcp(host.c_str(), port);
        logger_->Info("🔗 Setting up Modbus TCP connection to " + host + ":" + std::to_string(port));
    } else {
        // RTU 방식 - 시리얼 포트
        // 🔥 RTU 설정은 properties에서 가져오기
        int baud_rate = 9600; // 기본값
        char parity = 'N';
        int data_bits = 8;
        int stop_bits = 1;
        
        if (config_.properties.count("baud_rate")) {
            baud_rate = std::stoi(config_.properties.at("baud_rate"));
        }
        if (config_.properties.count("parity")) {
            parity = config_.properties.at("parity")[0];
        }
        if (config_.properties.count("data_bits")) {
            data_bits = std::stoi(config_.properties.at("data_bits"));
        }
        if (config_.properties.count("stop_bits")) {
            stop_bits = std::stoi(config_.properties.at("stop_bits"));
        }
        
        modbus_ctx_ = modbus_new_rtu(config_.endpoint.c_str(), baud_rate, parity, data_bits, stop_bits);
        logger_->Info("🔗 Setting up Modbus RTU connection to " + config_.endpoint + 
                     " (" + std::to_string(baud_rate) + " baud)");
    }
    
    return modbus_ctx_ != nullptr;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1))); // 백오프
        
        if (Connect()) {
            return true;
        }
    }
    return false;
}

void ModbusDriver::UpdateStats(bool success, double response_time_ms, const std::string& operation) {
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

void ModbusDriver::SetError(Structs::ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
}

Structs::DataValue ModbusDriver::ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const {
    Structs::DataValue result;
    
    // 🔥 DataPoint의 scaling_factor와 scaling_offset 활용
    double scaled_value = static_cast<double>(raw_value) * point.scaling_factor + point.scaling_offset;
    
    // data_type 문자열을 기반으로 변환
    if (point.data_type == "INT16") {
        result = static_cast<int16_t>(scaled_value);
    } else if (point.data_type == "UINT16") {
        result = static_cast<uint16_t>(scaled_value);
    } else if (point.data_type == "FLOAT") {
        result = static_cast<float>(scaled_value);
    } else if (point.data_type == "DOUBLE") {
        result = scaled_value;
    } else if (point.data_type == "BOOL" || point.data_type == "COIL") {
        result = (raw_value != 0);
    } else {
        // 기본값으로 스케일링된 unsigned int 사용
        result = static_cast<unsigned int>(scaled_value);
    }
    
    return result;
}

bool ModbusDriver::ConvertToModbusValue(const Structs::DataValue& value, const Structs::DataPoint& point, uint16_t& modbus_value) const {
    // 🔥 역스케일링 적용
    auto apply_reverse_scaling = [&](double val) -> uint16_t {
        double reverse_scaled = (val - point.scaling_offset) / point.scaling_factor;
        return static_cast<uint16_t>(std::max(0.0, std::min(65535.0, reverse_scaled)));
    };
    
    // data_type 문자열을 기반으로 변환
    if (point.data_type == "INT16") {
        if (std::holds_alternative<int16_t>(value)) {
            modbus_value = apply_reverse_scaling(std::get<int16_t>(value));
        } else if (std::holds_alternative<int>(value)) {
            modbus_value = apply_reverse_scaling(std::get<int>(value));
        } else {
            return false;
        }
    } else if (point.data_type == "UINT16") {
        if (std::holds_alternative<unsigned int>(value)) {
            modbus_value = apply_reverse_scaling(std::get<unsigned int>(value));
        } else if (std::holds_alternative<int>(value)) {
            modbus_value = apply_reverse_scaling(std::get<int>(value));
        } else {
            return false;
        }
    } else if (point.data_type == "FLOAT") {
        float float_val = 0.0f;
        if (std::holds_alternative<float>(value)) {
            float_val = std::get<float>(value);
        } else if (std::holds_alternative<double>(value)) {
            float_val = static_cast<float>(std::get<double>(value));
        } else {
            return false;
        }
        modbus_value = apply_reverse_scaling(float_val);
    } else if (point.data_type == "BOOL" || point.data_type == "COIL") {
        if (std::holds_alternative<bool>(value)) {
            modbus_value = std::get<bool>(value) ? 1 : 0;
        } else {
            return false;
        }
    } else {
        return false;
    }
    
    return true;
}

// =============================================================================
// Start/Stop 메서드 구현
// =============================================================================

bool ModbusDriver::Start() {
    std::lock_guard<std::mutex> lock(driver_mutex_);
    
    if (is_started_) {
        if (logger_) {
            logger_->Debug("ModbusDriver already started for endpoint: " + config_.endpoint);
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Exception in ModbusDriver::Start(): " + std::string(e.what()));
        }
        return false;
    }
}

bool ModbusDriver::Stop() {
    std::lock_guard<std::mutex> lock(driver_mutex_);
    
    if (!is_started_) {
        if (logger_) {
            logger_->Debug("ModbusDriver already stopped for endpoint: " + config_.endpoint);
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Exception in ModbusDriver::Stop(): " + std::string(e.what()));
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

void ModbusDriver::DisableDiagnostics() {
    diagnostics_.reset();
}

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
        << "\"successful_reads\":" << driver_statistics_.successful_reads.load() << ","
        << "\"failed_reads\":" << driver_statistics_.failed_reads.load() << ","
        << "\"total_writes\":" << driver_statistics_.total_writes.load() << ","
        << "\"successful_writes\":" << driver_statistics_.successful_writes.load() << ","
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

bool ModbusDriver::EnableConnectionPooling(size_t pool_size, int timeout_seconds) {
    if (!connection_pool_) {
        connection_pool_ = std::make_unique<ModbusConnectionPool>(this);
        if (!connection_pool_) {
            return false;
        }
    }
    
    return connection_pool_->EnableConnectionPooling(pool_size, timeout_seconds);
}

void ModbusDriver::DisableConnectionPooling() {
    connection_pool_.reset();
}

bool ModbusDriver::IsConnectionPoolingEnabled() const {
    return connection_pool_ != nullptr && connection_pool_->IsEnabled();
}

bool ModbusDriver::EnableAutoScaling(double load_threshold, size_t max_connections) {
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

void ModbusDriver::RecordRegisterAccess(uint16_t address, bool is_read, bool is_write) {
    if (diagnostics_) {
        diagnostics_->RecordRegisterAccess(address, is_read, is_write);
    }
}

void ModbusDriver::RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms) {
    if (diagnostics_) {
        diagnostics_->RecordSlaveRequest(slave_id, success, response_time_ms);
    }
}

bool ModbusDriver::PerformReadWithConnectionPool(const std::vector<Structs::DataPoint>& points,
                                                 std::vector<Structs::TimestampedValue>& values) {
    if (connection_pool_) {
        return connection_pool_->PerformBatchRead(points, values);
    }
    
    // 풀이 없으면 일반 방식으로 fallback
    return ReadValues(points, values);
}

bool ModbusDriver::PerformWriteWithConnectionPool(const Structs::DataPoint& point,
                                                  const Structs::DataValue& value) {
    if (connection_pool_) {
        return connection_pool_->PerformWrite(point, value);
    }
    
    // 풀이 없으면 일반 방식으로 fallback
    return WriteValue(point, value);
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

bool ModbusDriver::EnableFailover(int failure_threshold, int recovery_check_interval) {
    (void)failure_threshold;
    (void)recovery_check_interval;
    return false;
}

void ModbusDriver::DisableFailover() {
    failover_.reset();
}

bool ModbusDriver::IsFailoverEnabled() const {
    return failover_ != nullptr;
}

bool ModbusDriver::AddBackupEndpoint(const std::string& endpoint) {
    if (failover_) {
        return failover_->AddBackupEndpoint(endpoint);
    }
    return false;
}

void ModbusDriver::RemoveBackupEndpoint(const std::string& endpoint) {
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

bool ModbusDriver::EnablePerformanceMode() {
    return false;
}

void ModbusDriver::DisablePerformanceMode() {
    performance_.reset();
}

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

} // namespace Drivers
} // namespace PulseOne