// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// Modbus 드라이버 Facade 패턴 구현 (Core 기능 + 선택적 고급 기능)
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
    , is_connected_(false)
    , current_slave_id_(1)
    , driver_statistics_("MODBUS")
    , diagnostics_(nullptr)
    , connection_pool_(nullptr)
    , failover_(nullptr)
    , performance_(nullptr)
{
    // Core 통계 초기화 - 프로토콜 특화 카운터는 나중에 동적으로 추가
    // driver_statistics_.IncrementProtocolCounter() 메서드로 필요시 자동 생성됨
}

ModbusDriver::~ModbusDriver() {
    Disconnect();
    CleanupConnection();
}

// =============================================================================
// IProtocolDriver 인터페이스 구현 (Core 기능)
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    config_ = config;
    
    // Modbus 컨텍스트 설정
    if (!SetupModbusConnection()) {
        SetError(Structs::ErrorCode::CONFIGURATION_ERROR, "Failed to setup Modbus connection");
        return false;
    }
    
    // 연결 타임아웃 설정
    modbus_set_response_timeout(modbus_ctx_, 1, 0); // 1초
    modbus_set_byte_timeout(modbus_ctx_, 0, 500000); // 500ms
    
    // 디버그 모드 설정 (설정에 debug_enabled가 있다면)
    auto debug_it = config_.properties.find("debug_enabled");
    if (debug_it != config_.properties.end() && debug_it->second == "true") {
        modbus_set_debug(modbus_ctx_, TRUE);
    }
    
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
    
    int result = modbus_connect(modbus_ctx_);
    if (result == -1) {
        auto error_msg = std::string("Connection failed: ") + modbus_strerror(errno);
        SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStats(false, duration.count(), "connect");
        
        return false;
    }
    
    is_connected_.store(true);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    UpdateStats(true, duration.count(), "connect");
    
    return true;
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
    
    // 기본 방식으로 읽기
    values.clear();
    values.reserve(points.size());
    
    for (const auto& point : points) {
        std::vector<uint16_t> raw_values;
        bool success = false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Slave ID 설정 (properties에서 가져오기)
        int slave_id = 1; // 기본값
        auto slave_it = config_.properties.find("slave_id");
        if (slave_it != config_.properties.end()) {
            slave_id = std::stoi(slave_it->second);
        }
        SetSlaveId(slave_id);
        
        // 데이터 타입에 따라 읽기 (문자열로 저장됨)
        std::string data_type_str = point.data_type;
        if (data_type_str == "HOLDING_REGISTER") {
            success = ReadHoldingRegisters(slave_id, point.address, 1, raw_values);
            driver_statistics_.IncrementProtocolCounter("register_reads");
            RecordRegisterAccess(point.address, true, false);
        } else if (data_type_str == "INPUT_REGISTER") {
            success = ReadInputRegisters(slave_id, point.address, 1, raw_values);
            driver_statistics_.IncrementProtocolCounter("register_reads");
            RecordRegisterAccess(point.address, true, false);
        } else if (data_type_str == "COIL" || data_type_str == "DISCRETE_INPUT") {
            std::vector<uint8_t> coil_values;
            if (data_type_str == "COIL") {
                success = ReadCoils(slave_id, point.address, 1, coil_values);
            } else {
                success = ReadDiscreteInputs(slave_id, point.address, 1, coil_values);
            }
            
            if (success && !coil_values.empty()) {
                raw_values.push_back(coil_values[0] ? 1 : 0);
            }
            driver_statistics_.IncrementProtocolCounter("coil_reads");
        } else {
            success = false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        UpdateStats(success, duration.count(), "read");
        RecordResponseTime(slave_id, duration.count());
        RecordSlaveRequest(slave_id, success, duration.count());
        
        if (success && !raw_values.empty()) {
            Structs::TimestampedValue timestamped_value;
            // DataPoint 전체를 복사하지 않고 필요한 정보만 설정
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
    
    // Slave ID 설정 (properties에서 가져오기)
    int slave_id = 1; // 기본값
    auto slave_it = config_.properties.find("slave_id");
    if (slave_it != config_.properties.end()) {
        slave_id = std::stoi(slave_it->second);
    }
    SetSlaveId(slave_id);
    
    bool success = false;
    uint16_t modbus_value;
    
    // 데이터 변환
    if (!ConvertToModbusValue(value, point, modbus_value)) {
        SetError(Structs::ErrorCode::DATA_CONVERSION_ERROR, "Failed to convert value to Modbus format");
        return false;
    }
    
    // 데이터 타입에 따라 쓰기 (문자열로 저장됨)
    std::string data_type_str = point.data_type;
    if (data_type_str == "HOLDING_REGISTER") {
        success = WriteHoldingRegister(slave_id, point.address, modbus_value);
        driver_statistics_.IncrementProtocolCounter("register_writes");
        RecordRegisterAccess(point.address, false, true);
    } else if (data_type_str == "COIL") {
        success = WriteCoil(slave_id, point.address, modbus_value != 0);
        driver_statistics_.IncrementProtocolCounter("coil_writes");
    } else {
        SetError(Structs::ErrorCode::UNSUPPORTED_FUNCTION, "Write operation not supported for this data type");
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
    // DriverStatistics의 개별 필드 리셋
    driver_statistics_.total_reads.store(0);
    driver_statistics_.successful_reads.store(0);
    driver_statistics_.failed_reads.store(0);
    driver_statistics_.total_writes.store(0);
    driver_statistics_.successful_writes.store(0);
    driver_statistics_.failed_writes.store(0);
    // average_response_time은 std::chrono::milliseconds 타입이므로 직접 할당
    driver_statistics_.average_response_time = std::chrono::milliseconds(0);
}

ProtocolType ModbusDriver::GetProtocolType() const {
    return ProtocolType::MODBUS_TCP; // 정확한 enum 값 사용
}

Structs::DriverStatus ModbusDriver::GetStatus() const {
    if (!modbus_ctx_) {
        return Structs::DriverStatus::UNINITIALIZED;
    }
    
    if (!IsConnected()) {
        return Structs::DriverStatus::ERROR; // DISCONNECTED가 없으므로 ERROR 사용
    }
    
    return Structs::DriverStatus::RUNNING;
}

ErrorInfo ModbusDriver::GetLastError() const {
    return last_error_;
}

// =============================================================================
// 기본 Modbus 통신 (Core 기능)
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
            // 일반적인 읽기 실패
            SetError(Structs::ErrorCode::READ_FAILED, error_msg);
        }
        
        // Modbus 예외 코드 기록 및 프로토콜 에러로 재분류
        if (errno > 0 && errno < 255) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
            
            // Modbus 표준 예외 코드 범위(1-8)라면 프로토콜 에러로 재분류
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
// 고급 기능 - 진단 (Facade 메서드)
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
    
    // 기본 정보만 반환 (Worker 호환성 유지)
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
// 고급 기능 - 연결 풀링 (Facade 메서드)
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
    
    return ConnectionPoolStats{}; // 빈 구조체 반환
}

// =============================================================================
// 내부 메서드 (Core 기능)
// =============================================================================

bool ModbusDriver::SetupModbusConnection() {
    CleanupConnection();
    
    // TCP vs RTU 판단 (config_.endpoint 분석)
    if (config_.endpoint.find(':') != std::string::npos) {
        // TCP 방식
        auto pos = config_.endpoint.find(':');
        std::string host = config_.endpoint.substr(0, pos);
        int port = std::stoi(config_.endpoint.substr(pos + 1));
        
        modbus_ctx_ = modbus_new_tcp(host.c_str(), port);
    } else {
        // RTU 방식 (시리얼 포트)
        modbus_ctx_ = modbus_new_rtu(config_.endpoint.c_str(), 9600, 'N', 8, 1);
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
    // 표준 통계 업데이트
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
    
    // 응답 시간 업데이트 (std::chrono::milliseconds 타입)
    auto current_avg = driver_statistics_.average_response_time.count(); // milliseconds를 long으로
    auto new_avg = static_cast<long>((current_avg + response_time_ms) / 2.0); // 간단한 이동 평균
    driver_statistics_.average_response_time = std::chrono::milliseconds(new_avg);
    
    // 마지막 활동 시간은 현재 DriverStatistics 구조에 없으므로 제거
}

void ModbusDriver::SetError(Structs::ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
    // timestamp 필드가 없으므로 제거
}

Structs::DataValue ModbusDriver::ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const {
    Structs::DataValue result;
    
    // data_type 문자열을 기반으로 변환
    if (point.data_type == "INT16") {
        result = static_cast<int16_t>(raw_value);
    } else if (point.data_type == "UINT16") {
        result = raw_value;
    } else if (point.data_type == "FLOAT") {
        float scaled_value = static_cast<float>(raw_value) * point.scaling_factor + point.scaling_offset;
        result = scaled_value;
    } else if (point.data_type == "BOOL") {
        result = (raw_value != 0);
    } else {
        // 기본값으로 uint16 사용
        result = static_cast<unsigned int>(raw_value);
    }
    
    return result;
}

bool ModbusDriver::ConvertToModbusValue(const Structs::DataValue& value, const Structs::DataPoint& point, uint16_t& modbus_value) const {
    // data_type 문자열을 기반으로 변환
    if (point.data_type == "INT16") {
        if (std::holds_alternative<int16_t>(value)) {
            modbus_value = static_cast<uint16_t>(std::get<int16_t>(value));
        } else if (std::holds_alternative<int>(value)) {
            modbus_value = static_cast<uint16_t>(std::get<int>(value));
        } else {
            return false;
        }
    } else if (point.data_type == "UINT16") {
        if (std::holds_alternative<unsigned int>(value)) {
            modbus_value = static_cast<uint16_t>(std::get<unsigned int>(value));
        } else if (std::holds_alternative<int>(value)) {
            modbus_value = static_cast<uint16_t>(std::get<int>(value));
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
        modbus_value = static_cast<uint16_t>((float_val - point.scaling_offset) / point.scaling_factor);
    } else if (point.data_type == "BOOL") {
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
// 고급 기능 델리게이트 메서드 (선택적 호출)
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
// 나머지 고급 기능 스텁 (실제 구현은 각각의 클래스에서)
// =============================================================================

bool ModbusDriver::EnableFailover(int failure_threshold, int recovery_check_interval) {
    // 구현 예정 - ModbusFailover 클래스
    (void)failure_threshold;        // 미사용 매개변수 경고 억제
    (void)recovery_check_interval;  // 미사용 매개변수 경고 억제
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
    // 구현 예정 - ModbusPerformance 클래스
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