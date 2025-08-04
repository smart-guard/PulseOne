// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// Modbus 드라이버 Facade 구현 - 심플하고 확장 가능한 구조
// =============================================================================

#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Modbus/ModbusDiagnostics.h"
#include "Drivers/Modbus/ModbusConnectionPool.h" 
#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusPerformance.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <iostream>
#include <sstream>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 소멸자 (Core 기능만 초기화)
// =============================================================================

ModbusDriver::ModbusDriver()
    : driver_statistics_("MODBUS")
    , modbus_ctx_(nullptr)
    , is_connected_(false)
    , current_slave_id_(1) {
    
    // 에러 정보 초기화
    last_error_.code = Structs::ErrorCode::SUCCESS;
    last_error_.message = "";
    last_error_.protocol = "MODBUS";
    last_error_.occurred_at = std::chrono::system_clock::now();
    
    // 마지막 성공 작업 시간 초기화
    last_successful_operation_ = std::chrono::steady_clock::now();
    
    // 고급 기능들은 nullptr로 초기화 (선택적 활성화)
    diagnostics_ = nullptr;
    connection_pool_ = nullptr;
    failover_ = nullptr;
    performance_ = nullptr;
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusDriver created (Core mode - minimal memory usage)");
}

ModbusDriver::~ModbusDriver() {
    auto& logger = LogManager::getInstance();
    
    // 연결 정리
    Disconnect();
    
    // Modbus 컨텍스트 정리
    if (modbus_ctx_) {
        modbus_free(modbus_ctx_);
        modbus_ctx_ = nullptr;
    }
    
    // 고급 기능들은 unique_ptr에 의해 자동 정리됨
    logger.Info("ModbusDriver destroyed");
}

// =============================================================================
// IProtocolDriver 인터페이스 구현 (Core 기능)
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    auto& logger = LogManager::getInstance();
    
    config_ = config;
    
    // TCP 컨텍스트 생성
    if (config.protocol == Enums::ProtocolType::MODBUS_TCP) {
        // 엔드포인트에서 주소와 포트 파싱
        std::string host = "127.0.0.1";
        int port = 502;
        
        if (!config.endpoint.empty()) {
            size_t colon_pos = config.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                host = config.endpoint.substr(0, colon_pos);
                try {
                    port = std::stoi(config.endpoint.substr(colon_pos + 1));
                } catch (const std::exception& e) {
                    SetError(Structs::ErrorCode::INVALID_PARAMETER, 
                            "Invalid port number in endpoint: " + config.endpoint);
                    return false;
                }
            } else {
                host = config.endpoint;
            }
        }
        
        modbus_ctx_ = modbus_new_tcp(host.c_str(), port);
        logger.Info("ModbusDriver initialized for TCP: " + host + ":" + std::to_string(port));
    }
    else if (config.protocol == Enums::ProtocolType::MODBUS_RTU) {
        // RTU는 향후 확장
        SetError(Structs::ErrorCode::NOT_SUPPORTED, "Modbus RTU not implemented in this version");
        return false;
    }
    else {
        SetError(Structs::ErrorCode::INVALID_PARAMETER, "Unsupported protocol type");
        return false;
    }
    
    if (!modbus_ctx_) {
        SetError(Structs::ErrorCode::INITIALIZATION_FAILED, "Failed to create Modbus context");
        return false;
    }
    
    // 타임아웃 설정
    if (config.timeout_ms > 0) {
        modbus_set_response_timeout(modbus_ctx_, config.timeout_ms / 1000, (config.timeout_ms % 1000) * 1000);
    }
    
    // 슬레이브 ID 설정
    if (config.custom_settings.find("slave_id") != config.custom_settings.end()) {
        try {
            current_slave_id_ = std::stoi(config.custom_settings.at("slave_id"));
        } catch (const std::exception& e) {
            logger.Warn("Invalid slave_id in custom_settings, using default: 1");
            current_slave_id_ = 1;
        }
    }
    
    logger.Info("ModbusDriver initialized successfully (Slave ID: " + std::to_string(current_slave_id_) + ")");
    return true;
}

bool ModbusDriver::Connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    auto& logger = LogManager::getInstance();
    
    if (is_connected_) {
        return true;
    }
    
    if (!modbus_ctx_) {
        SetError(Structs::ErrorCode::NOT_INITIALIZED, "Modbus context not initialized");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    int result = modbus_connect(modbus_ctx_);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (result == 0) {
        is_connected_ = true;
        last_successful_operation_ = std::chrono::steady_clock::now();
        UpdateStats(true, duration_ms, "connect");
        logger.Info("ModbusDriver connected successfully");
        return true;
    } else {
        is_connected_ = false;
        std::string error_msg = "Connection failed: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::CONNECTION_FAILED, error_msg);
        UpdateStats(false, duration_ms, "connect");
        logger.Error(error_msg);
        return false;
    }
}

bool ModbusDriver::Disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    auto& logger = LogManager::getInstance();
    
    if (!is_connected_) {
        return true;
    }
    
    if (modbus_ctx_) {
        modbus_close(modbus_ctx_);
    }
    
    is_connected_ = false;
    logger.Info("ModbusDriver disconnected");
    return true;
}

bool ModbusDriver::IsConnected() const {
    return is_connected_.load();
}

bool ModbusDriver::ReadValues(
    const std::vector<Structs::DataPoint>& points,
    std::vector<Structs::TimestampedValue>& values) {
    
    if (points.empty()) {
        return true;
    }
    
    // 연결 풀링이 활성화된 경우 해당 기능 사용
    if (connection_pool_ && connection_pool_->IsConnectionPoolingEnabled()) {
        return PerformReadWithConnectionPool(points, values);
    }
    
    // 기본 단일 연결 방식
    auto start_time = std::chrono::high_resolution_clock::now();
    bool overall_success = true;
    
    values.clear();
    values.reserve(points.size());
    
    for (const auto& point : points) {
        Structs::TimestampedValue timed_value;
        timed_value.timestamp = std::chrono::system_clock::now();
        
        // Modbus 읽기 작업 수행
        std::vector<uint16_t> raw_values;
        bool read_success = ReadHoldingRegisters(
            current_slave_id_, 
            static_cast<uint16_t>(point.address), 
            1, 
            raw_values
        );
        
        if (read_success && !raw_values.empty()) {
            timed_value.value = ConvertModbusValue(point, raw_values[0]);
        } else {
            timed_value.value = Structs::DataValue(0);
            overall_success = false;
        }
        
        values.push_back(timed_value);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    UpdateStats(overall_success, duration.count(), "read");
    
    if (overall_success) {
        last_successful_operation_ = std::chrono::steady_clock::now();
    }
    
    return overall_success;
}

bool ModbusDriver::WriteValue(
    const Structs::DataPoint& point,
    const Structs::DataValue& value) {
    
    // 연결 풀링이 활성화된 경우 해당 기능 사용
    if (connection_pool_ && connection_pool_->IsConnectionPoolingEnabled()) {
        return PerformWriteWithConnectionPool(point, value);
    }
    
    // 기본 단일 연결 방식
    auto start_time = std::chrono::high_resolution_clock::now();
    
    uint16_t modbus_value;
    bool convert_success = ConvertToModbusValue(value, point, modbus_value);
    
    bool success = false;
    if (convert_success) {
        success = WriteHoldingRegister(
            current_slave_id_,
            static_cast<uint16_t>(point.address),
            modbus_value
        );
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    UpdateStats(success, duration.count(), "write");
    
    if (success) {
        last_successful_operation_ = std::chrono::steady_clock::now();
    }
    
    return success;
}

Enums::ProtocolType ModbusDriver::GetProtocolType() const {
    return Enums::ProtocolType::MODBUS_TCP;
}

Structs::DriverStatus ModbusDriver::GetStatus() const {
    if (!modbus_ctx_) {
        return Structs::DriverStatus::UNINITIALIZED;
    }
    
    if (!is_connected_) {
        return Structs::DriverStatus::STOPPED;
    }
    
    return Structs::DriverStatus::RUNNING;
}

Structs::ErrorInfo ModbusDriver::GetLastError() const {
    return last_error_;
}

const DriverStatistics& ModbusDriver::GetStatistics() const {
    return driver_statistics_;
}

void ModbusDriver::ResetStatistics() {
    driver_statistics_.ResetStatistics();
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusDriver statistics reset");
}

// =============================================================================
// 기본 Modbus 통신 메서드 (Core 기능)
// =============================================================================

bool ModbusDriver::SetSlaveId(int slave_id) {
    if (slave_id < 1 || slave_id > 247) {
        SetError(Structs::ErrorCode::INVALID_PARAMETER, "Slave ID must be between 1 and 247");
        return false;
    }
    
    current_slave_id_ = slave_id;
    return true;
}

int ModbusDriver::GetSlaveId() const {
    return current_slave_id_;
}

bool ModbusDriver::TestConnection() {
    if (!EnsureConnection()) {
        return false;
    }
    
    // 간단한 테스트 읽기 (1개 레지스터)
    std::vector<uint16_t> test_values;
    return ReadHoldingRegisters(current_slave_id_, 0, 1, test_values);
}

bool ModbusDriver::ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!EnsureConnection()) {
        return false;
    }
    
    // 슬레이브 ID 설정
    if (modbus_set_slave(modbus_ctx_, slave_id) == -1) {
        std::string error_msg = "Failed to set slave ID: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
        return false;
    }
    
    // 레지스터 읽기
    values.resize(count);
    int result = modbus_read_registers(modbus_ctx_, start_addr, count, values.data());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    bool success = (result == count);
    
    if (success) {
        // 진단 기능이 활성화된 경우 추가 정보 기록
        RecordResponseTime(slave_id, static_cast<uint32_t>(duration_ms));
        RecordRegisterAccess(start_addr, true, false);
        RecordSlaveRequest(slave_id, true, static_cast<uint32_t>(duration_ms));
    } else {
        std::string error_msg = "Failed to read holding registers: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::READ_FAILED, error_msg);
        
        // 예외 코드 기록
        if (errno >= 0x01 && errno <= 0x04) {
            RecordExceptionCode(static_cast<uint8_t>(errno));
        }
        RecordSlaveRequest(slave_id, false, static_cast<uint32_t>(duration_ms));
    }
    
    return success;
}

bool ModbusDriver::ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!EnsureConnection()) {
        return false;
    }
    
    if (modbus_set_slave(modbus_ctx_, slave_id) == -1) {
        std::string error_msg = "Failed to set slave ID: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
        return false;
    }
    
    values.resize(count);
    int result = modbus_read_input_registers(modbus_ctx_, start_addr, count, values.data());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    bool success = (result == count);
    
    if (success) {
        RecordResponseTime(slave_id, static_cast<uint32_t>(duration_ms));
        RecordRegisterAccess(start_addr, true, false);
        RecordSlaveRequest(slave_id, true, static_cast<uint32_t>(duration_ms));
    } else {
        std::string error_msg = "Failed to read input registers: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::READ_FAILED, error_msg);
        RecordSlaveRequest(slave_id, false, static_cast<uint32_t>(duration_ms));
    }
    
    return success;
}

bool ModbusDriver::WriteHoldingRegister(int slave_id, uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!EnsureConnection()) {
        return false;
    }
    
    if (modbus_set_slave(modbus_ctx_, slave_id) == -1) {
        std::string error_msg = "Failed to set slave ID: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::INVALID_PARAMETER, error_msg);
        return false;
    }
    
    int result = modbus_write_register(modbus_ctx_, address, value);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    bool success = (result == 1);
    
    if (success) {
        RecordResponseTime(slave_id, static_cast<uint32_t>(duration_ms));
        RecordRegisterAccess(address, false, true);
        RecordSlaveRequest(slave_id, true, static_cast<uint32_t>(duration_ms));
    } else {
        std::string error_msg = "Failed to write holding register: " + std::string(modbus_strerror(errno));
        SetError(Structs::ErrorCode::WRITE_FAILED, error_msg);
        RecordSlaveRequest(slave_id, false, static_cast<uint32_t>(duration_ms));
    }
    
    return success;
}

bool ModbusDriver::ReadHoldingRegistersBulk(int slave_id, uint16_t start_addr, uint16_t count,
                                           std::vector<uint16_t>& values, int max_retries) {
    for (int retry = 0; retry < max_retries; ++retry) {
        if (ReadHoldingRegisters(slave_id, start_addr, count, values)) {
            return true;
        }
        
        if (retry < max_retries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
        }
    }
    
    return false;
}

// =============================================================================
// 진단 기능 (선택적 활성화)
// =============================================================================

bool ModbusDriver::EnableDiagnostics(bool packet_logging, bool console_output) {
    if (!diagnostics_) {
        diagnostics_ = std::make_unique<ModbusDiagnostics>(this);
    }
    
    diagnostics_->EnablePacketLogging(packet_logging);
    diagnostics_->EnableConsoleOutput(console_output);
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusDriver diagnostics enabled (packet_logging: " + 
               std::string(packet_logging ? "true" : "false") + 
               ", console_output: " + std::string(console_output ? "true" : "false") + ")");
    
    return true;
}

void ModbusDriver::DisableDiagnostics() {
    diagnostics_.reset();  // unique_ptr를 nullptr로 설정하여 메모리 해제
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusDriver diagnostics disabled and memory freed");
}

bool ModbusDriver::IsDiagnosticsEnabled() const {
    return diagnostics_ != nullptr;
}

std::string ModbusDriver::GetDiagnosticsJSON() const {
    if (diagnostics_) {
        return diagnostics_->GetDiagnosticsJSON();
    }
    
    // 기본 정보만 반환 (진단 기능 비활성화 상태)
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"protocol\": \"MODBUS\",\n";
    oss << "  \"diagnostics_enabled\": false,\n";
    oss << "  \"basic_statistics\": {\n";
    oss << "    \"total_reads\": " << driver_statistics_.total_reads.load() << ",\n";
    oss << "    \"successful_reads\": " << driver_statistics_.successful_reads.load() << ",\n";
    oss << "    \"failed_reads\": " << driver_statistics_.failed_reads.load() << ",\n";
    oss << "    \"success_rate\": " << driver_statistics_.GetSuccessRate() << "\n";
    oss << "  }\n";
    oss << "}";
    
    return oss.str();
}

std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
    if (diagnostics_) {
        return diagnostics_->GetDiagnostics();
    }
    
    // 기본 정보만 반환
    std::map<std::string, std::string> basic_diagnostics;
    basic_diagnostics["protocol"] = "MODBUS";
    basic_diagnostics["diagnostics_enabled"] = "false";
    basic_diagnostics["connected"] = is_connected_ ? "true" : "false";
    basic_diagnostics["current_slave_id"] = std::to_string(current_slave_id_);
    basic_diagnostics["total_reads"] = std::to_string(driver_statistics_.total_reads.load());
    basic_diagnostics["success_rate"] = std::to_string(driver_statistics_.GetSuccessRate());
    
    return basic_diagnostics;
}

// =============================================================================
// 내부 헬퍼 메서드들 (Core 기능)
// =============================================================================

void ModbusDriver::UpdateStats(bool success, double response_time_ms, const std::string& operation) {
    // 표준 필드 업데이트
    if (operation == "read" || operation.empty()) {
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
    
    // Modbus 특화 통계
    driver_statistics_.IncrementProtocolCounter("total_operations", 1);
    
    if (operation == "read" || operation.empty()) {
        driver_statistics_.IncrementProtocolCounter("register_reads", 1);
    } else if (operation == "write") {
        driver_statistics_.IncrementProtocolCounter("register_writes", 1);
    }
    
    driver_statistics_.IncrementProtocolCounter("crc_checks", 1);
    
    if (success) {
        driver_statistics_.IncrementProtocolCounter("successful_operations", 1);
        
        // 응답 시간 메트릭 업데이트
        if (response_time_ms > 0) {
            double current_avg = driver_statistics_.GetProtocolMetric("avg_response_time_ms");
            if (current_avg == 0.0) {
                driver_statistics_.SetProtocolMetric("avg_response_time_ms", response_time_ms);
            } else {
                double new_avg = (current_avg * 0.9) + (response_time_ms * 0.1);
                driver_statistics_.SetProtocolMetric("avg_response_time_ms", new_avg);
            }
        }
        
        driver_statistics_.last_success_time = Utils::GetCurrentTimestamp();
        driver_statistics_.consecutive_failures.store(0);
        
    } else {
        driver_statistics_.IncrementProtocolCounter("failed_operations", 1);
        driver_statistics_.IncrementProtocolCounter("slave_errors", 1);
        driver_statistics_.consecutive_failures.fetch_add(1);
    }
    
    // 성공률 계산
    uint64_t total_ops = driver_statistics_.GetProtocolCounter("total_operations");
    uint64_t successful_ops = driver_statistics_.GetProtocolCounter("successful_operations");
    
    if (total_ops > 0) {
        double success_rate = (static_cast<double>(successful_ops) / total_ops) * 100.0;
        driver_statistics_.SetProtocolMetric("success_rate", success_rate);
        driver_statistics_.success_rate.store(success_rate);
    }
    
    driver_statistics_.last_connection_time = std::chrono::system_clock::now();
    driver_statistics_.UpdateTotalOperations();
}

void ModbusDriver::SetError(Structs::ErrorCode code, const std::string& message) {
    // 표준 에러 정보 설정
    last_error_.code = code;
    last_error_.message = message;
    last_error_.protocol = "MODBUS";
    last_error_.occurred_at = std::chrono::system_clock::now();
    
    // 통계 업데이트
    driver_statistics_.IncrementProtocolCounter("total_errors", 1);
    driver_statistics_.last_error_time = Utils::GetCurrentTimestamp();
    
    // 로깅
    auto& logger = LogManager::getInstance();
    logger.Error("ModbusDriver Error [" + std::to_string(static_cast<int>(code)) + "]: " + message);
}

bool ModbusDriver::EnsureConnection() {
    if (is_connected_) {
        return true;
    }
    
    return Connect();
}

bool ModbusDriver::ReconnectWithRetry(int max_retries) {
    for (int retry = 0; retry < max_retries; ++retry) {
        if (Connect()) {
            return true;
        }
        
        if (retry < max_retries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (retry + 1)));
        }
    }
    
    return false;
}

bool ModbusDriver::SetupModbusConnection() {
    // Connect() 메서드에서 이미 처리됨
    return Connect();
}

void ModbusDriver::CleanupConnection() {
    Disconnect();
}

// =============================================================================
// 데이터 변환 메서드들
// =============================================================================

Structs::DataValue ModbusDriver::ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const {
    try {
        if (point.data_type == "BOOL") {
            return Structs::DataValue(raw_value != 0);
        }
        else if (point.data_type == "INT16") {
            return Structs::DataValue(static_cast<int16_t>(raw_value));
        }
        else if (point.data_type == "UINT16") {
            return Structs::DataValue(static_cast<uint16_t>(raw_value));
        }
        else if (point.data_type == "FLOAT") {
            // 16비트를 float로 변환 (스케일링 적용)
            float float_val = static_cast<float>(raw_value);
            if (point.scaling_factor != 0.0) {
                float_val *= point.scaling_factor;
            }
            float_val += point.scaling_offset;
            return Structs::DataValue(float_val);
        }
        else {
            // 기본값
            return Structs::DataValue(static_cast<uint16_t>(raw_value));
        }
    }
    catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Data conversion error: " + std::string(e.what()));
        return Structs::DataValue(0);
    }
}

bool ModbusDriver::ConvertToModbusValue(const Structs::DataValue& value, const Structs::DataPoint& point, uint16_t& modbus_value) const {
    try {
        if (point.data_type == "BOOL") {
            modbus_value = std::get<bool>(value) ? 1 : 0;
        }
        else if (point.data_type == "INT16") {
            modbus_value = static_cast<uint16_t>(std::get<int16_t>(value));
        }
        else if (point.data_type == "UINT16") {
            modbus_value = std::get<uint16_t>(value);
        }
        else if (point.data_type == "FLOAT") {
            float float_val = std::get<float>(value);
            // 스케일링 역적용
            float_val -= point.scaling_offset;
            if (point.scaling_factor != 0.0) {
                float_val /= point.scaling_factor;
            }
            modbus_value = static_cast<uint16_t>(float_val);
        }
        else {
            modbus_value = 0;
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Value conversion error: " + std::string(e.what()));
        modbus_value = 0;
        return false;
    }
}

// =============================================================================
// 고급 기능 델리게이트 메서드들 (해당 모듈이 활성화된 경우에만 호출)
// =============================================================================

void ModbusDriver::RecordExceptionCode(uint8_t exception_code) {
    if (diagnostics_) {
        diagnostics_->RecordExceptionCode(exception_code);
    }
}

void ModbusDriver::RecordCrcCheck(bool crc_valid) {
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
        return connection_pool_->PerformRead(points, values);
    }
    
    // 연결 풀이 없으면 기본 방식으로 fallback
    return ReadValues(points, values);
}

bool ModbusDriver::PerformWriteWithConnectionPool(const Structs::DataPoint& point,
                                                 const Structs::DataValue& value) {
    if (connection_pool_) {
        return connection_pool_->PerformWrite(point, value);
    }
    
    // 연결 풀이 없으면 기본 방식으로 fallback
    return WriteValue(point, value);
}

} // namespace Drivers
} // namespace PulseOne