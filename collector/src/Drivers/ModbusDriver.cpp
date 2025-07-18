// =============================================================================
// collector/src/Drivers/ModbusDriver.cpp
// Modbus 드라이버 구현
// =============================================================================

#include "Drivers/ModbusDriver.h"
#include "Drivers/DriverFactory.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <errno.h>

using namespace PulseOne::Drivers;
using namespace std::chrono;

// =============================================================================
// 생성자/소멸자
// =============================================================================

ModbusDriver::ModbusDriver()
    : modbus_ctx_(nullptr)
    , status_(DriverStatus::UNINITIALIZED)
    , is_connected_(false)
    , stop_watchdog_(false)
    , last_successful_operation_(steady_clock::now())
{
}

ModbusDriver::~ModbusDriver() {
    Disconnect();
    
    // Watchdog 스레드 정리
    stop_watchdog_ = true;
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig& config) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    config_ = config;
    protocol_type_ = config.protocol_type;
    
    // 로거 초기화
    logger_ = std::make_unique<DriverLogger>(
        config_.device_id, 
        protocol_type_, 
        config_.endpoint
    );
    
    logger_->Info("Modbus driver initialization started", DriverLogCategory::GENERAL);
    
    // 프로토콜별 초기화
    bool success = false;
    
    try {
        if (protocol_type_ == ProtocolType::MODBUS_TCP) {
            // TCP 주소 파싱 (IP:Port)
            size_t colon_pos = config_.endpoint.find(':');
            if (colon_pos == std::string::npos) {
                SetError(ErrorCode::INVALID_PARAMETER, "Invalid TCP endpoint format. Expected IP:Port");
                return false;
            }
            
            std::string host = config_.endpoint.substr(0, colon_pos);
            int port = std::stoi(config_.endpoint.substr(colon_pos + 1));
            
            success = InitializeTCP(host, port);
            
        } else if (protocol_type_ == ProtocolType::MODBUS_RTU) {
            // RTU 설정 파싱 (COM포트 또는 시리얼 설정)
            std::string device = config_.endpoint;
            
            // 시리얼 설정 (기본값)
            int baud = 9600;
            char parity = 'N';
            int data_bits = 8;
            int stop_bits = 1;
            
            // 설정에서 시리얼 파라미터 읽기
            auto it = config_.protocol_settings.find("baud_rate");
            if (it != config_.protocol_settings.end()) {
                baud = std::stoi(it->second);
            }
            
            it = config_.protocol_settings.find("parity");
            if (it != config_.protocol_settings.end() && !it->second.empty()) {
                parity = it->second[0];
            }
            
            it = config_.protocol_settings.find("data_bits");
            if (it != config_.protocol_settings.end()) {
                data_bits = std::stoi(it->second);
            }
            
            it = config_.protocol_settings.find("stop_bits");
            if (it != config_.protocol_settings.end()) {
                stop_bits = std::stoi(it->second);
            }
            
            success = InitializeRTU(device, baud, parity, data_bits, stop_bits);
            
        } else {
            SetError(ErrorCode::INVALID_PARAMETER, "Unsupported Modbus protocol type");
            return false;
        }
        
        if (success) {
            // 타임아웃 설정
            modbus_set_response_timeout(modbus_ctx_, 
                config_.timeout_ms / 1000, 
                (config_.timeout_ms % 1000) * 1000);
                
            // 재시도 복구 설정
            modbus_set_error_recovery(modbus_ctx_, 
                static_cast<modbus_error_recovery_mode>(
                    MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL
                ));
            
            status_ = DriverStatus::INITIALIZED;
            
            // Watchdog 스레드 시작
            stop_watchdog_ = false;
            watchdog_thread_ = std::thread(&ModbusDriver::WatchdogLoop, this);
            
            logger_->Info("Modbus driver initialized successfully", DriverLogCategory::GENERAL);
            
            // 통계 초기화
            ResetStatistics();
            
            return true;
        }
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONFIGURATION_ERROR, 
                 "Configuration error: " + std::string(e.what()));
    }
    
    logger_->Error("Modbus driver initialization failed", DriverLogCategory::ERROR_HANDLING);
    return false;
}

bool ModbusDriver::InitializeTCP(const std::string& host, int port) {
    logger_->Info("Initializing Modbus TCP connection to " + host + ":" + std::to_string(port),
                  DriverLogCategory::CONNECTION);
    
    modbus_ctx_ = modbus_new_tcp(host.c_str(), port);
    if (modbus_ctx_ == nullptr) {
        SetError(ErrorCode::INSUFFICIENT_RESOURCES, 
                 "Failed to create Modbus TCP context");
        return false;
    }
    
    return true;
}

bool ModbusDriver::InitializeRTU(const std::string& device, int baud, 
                                 char parity, int data_bits, int stop_bits) {
    logger_->Info("Initializing Modbus RTU on " + device + 
                  " (Baud: " + std::to_string(baud) + 
                  ", Parity: " + parity + 
                  ", Data: " + std::to_string(data_bits) + 
                  ", Stop: " + std::to_string(stop_bits) + ")",
                  DriverLogCategory::CONNECTION);
    
    modbus_ctx_ = modbus_new_rtu(device.c_str(), baud, parity, data_bits, stop_bits);
    if (modbus_ctx_ == nullptr) {
        SetError(ErrorCode::INSUFFICIENT_RESOURCES, 
                 "Failed to create Modbus RTU context");
        return false;
    }
    
    return true;
}

bool ModbusDriver::Connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (status_ != DriverStatus::INITIALIZED && status_ != DriverStatus::DISCONNECTED) {
        SetError(ErrorCode::INVALID_PARAMETER, "Driver not initialized");
        return false;
    }
    
    if (is_connected_) {
        return true;
    }
    
    status_ = DriverStatus::CONNECTING;
    logger_->Info("Attempting to connect to Modbus device", DriverLogCategory::CONNECTION);
    
    auto connection_start = steady_clock::now();
    
    bool success = EstablishConnection();
    
    auto connection_end = steady_clock::now();
    auto duration_ms = duration_cast<milliseconds>(connection_end - connection_start).count();
    
    if (success) {
        is_connected_ = true;
        status_ = DriverStatus::CONNECTED;
        last_successful_operation_ = steady_clock::now();
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.connection_attempts++;
            statistics_.successful_connections++;
        }
        
        logger_->LogConnectionStatusChange(
            ConnectionStatus::DISCONNECTED,
            ConnectionStatus::CONNECTED,
            "Modbus connection established in " + std::to_string(duration_ms) + "ms"
        );
        
        NotifyConnectionStatusChanged(config_.device_id, 
                                      ConnectionStatus::DISCONNECTED,
                                      ConnectionStatus::CONNECTED,
                                      "Connected");
        
        UpdateDiagnostics();
        
        return true;
        
    } else {
        status_ = DriverStatus::ERROR;
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.connection_attempts++;
        }
        
        logger_->LogConnectionStatusChange(
            ConnectionStatus::DISCONNECTED,
            ConnectionStatus::ERROR,
            "Modbus connection failed after " + std::to_string(duration_ms) + "ms"
        );
        
        NotifyConnectionStatusChanged(config_.device_id,
                                      ConnectionStatus::DISCONNECTED,
                                      ConnectionStatus::ERROR,
                                      last_error_.message);
        
        return false;
    }
}

bool ModbusDriver::EstablishConnection() {
    if (modbus_ctx_ == nullptr) {
        SetError(ErrorCode::INVALID_PARAMETER, "Modbus context not initialized");
        return false;
    }
    
    int result = modbus_connect(modbus_ctx_);
    if (result == -1) {
        ErrorCode error_code = TranslateModbusError(errno);
        std::string error_msg = GetModbusErrorString(errno);
        SetError(error_code, "Connection failed: " + error_msg);
        return false;
    }
    
    // 연결 검증을 위한 간단한 테스트 읽기 (선택사항)
    if (config_.protocol_settings.find("test_on_connect") != config_.protocol_settings.end()) {
        uint16_t test_value;
        int test_result = modbus_read_input_registers(modbus_ctx_, 0, 1, &test_value);
        if (test_result == -1) {
            logger_->Warn("Connection test read failed, but connection is established", 
                         DriverLogCategory::CONNECTION);
        }
    }
    
    return true;
}

bool ModbusDriver::Disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!is_connected_) {
        return true;
    }
    
    status_ = DriverStatus::DISCONNECTING;
    logger_->Info("Disconnecting from Modbus device", DriverLogCategory::CONNECTION);
    
    CloseConnection();
    
    is_connected_ = false;
    status_ = DriverStatus::DISCONNECTED;
    
    logger_->LogConnectionStatusChange(
        ConnectionStatus::CONNECTED,
        ConnectionStatus::DISCONNECTED,
        "Modbus connection closed"
    );
    
    NotifyConnectionStatusChanged(config_.device_id,
                                  ConnectionStatus::CONNECTED,
                                  ConnectionStatus::DISCONNECTED,
                                  "Disconnected");
    
    return true;
}

void ModbusDriver::CloseConnection() {
    if (modbus_ctx_ != nullptr) {
        modbus_close(modbus_ctx_);
        modbus_free(modbus_ctx_);
        modbus_ctx_ = nullptr;
    }
}

bool ModbusDriver::IsConnected() const {
    return is_connected_ && status_ == DriverStatus::CONNECTED;
}

ProtocolType ModbusDriver::GetProtocolType() const {
    return protocol_type_;
}

DriverStatus ModbusDriver::GetStatus() const {
    return status_;
}

ErrorInfo ModbusDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

// =============================================================================
// 데이터 읽기/쓰기 구현
// =============================================================================

bool ModbusDriver::ReadValues(const std::vector<DataPoint>& points,
                              std::vector<TimestampedValue>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    values.clear();
    values.reserve(points.size());
    
    auto operation_start = steady_clock::now();
    bool overall_success = true;
    
    logger_->LogDataTransfer("RX", points.size() * 2, 0, true);
    
    for (const auto& point : points) {
        TimestampedValue tvalue;
        tvalue.timestamp = system_clock::now();
        
        try {
            // 슬레이브 ID 설정 (DataPoint의 properties에서 가져오거나 기본값 사용)
            int slave_id = 1;  // 기본값
            auto slave_it = point.properties.find("slave_id");
            if (slave_it != point.properties.end()) {
                slave_id = std::stoi(slave_it->second);
            }
            
            modbus_set_slave(modbus_ctx_, slave_id);
            
            // 데이터 타입에 따른 읽기 실행
            ModbusFunction function = GetReadFunction(point);
            std::vector<uint16_t> raw_values;
            
            bool read_success = ExecuteRead(slave_id, function, point.address, 1, raw_values);
            
            if (read_success && !raw_values.empty()) {
                tvalue.value = ConvertModbusValue(point, raw_values[0]);
                tvalue.quality = DataQuality::GOOD;
                
                logger_->LogModbusOperation(slave_id, point.address, 
                                           std::to_string(raw_values[0]), true, 0);
            } else {
                tvalue.value = DataValue(0);
                tvalue.quality = DataQuality::BAD;
                overall_success = false;
                
                logger_->LogModbusOperation(slave_id, point.address, "", false, 0);
            }
            
        } catch (const std::exception& e) {
            tvalue.value = DataValue(0);
            tvalue.quality = DataQuality::BAD;
            overall_success = false;
            
            logger_->Error("Exception reading point " + point.name + ": " + e.what(),
                          DriverLogCategory::ERROR_HANDLING);
        }
        
        values.push_back(tvalue);
    }
    
    auto operation_end = steady_clock::now();
    auto duration_ms = duration_cast<milliseconds>(operation_end - operation_start).count();
    
    // 통계 업데이트
    UpdateStatistics(overall_success, static_cast<double>(duration_ms));
    
    if (overall_success) {
        last_successful_operation_ = steady_clock::now();
    }
    
    logger_->LogPerformanceMetric("ReadValues", duration_ms, "ms");
    
    return overall_success;
}

bool ModbusDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    auto operation_start = steady_clock::now();
    
    try {
        // 슬레이브 ID 설정
        int slave_id = 1;
        auto slave_it = point.properties.find("slave_id");
        if (slave_it != point.properties.end()) {
            slave_id = std::stoi(slave_it->second);
        }
        
        modbus_set_slave(modbus_ctx_, slave_id);
        
        // 값 변환
        uint16_t modbus_value = ConvertToModbusValue(point, value);
        
        // 쓰기 실행
        ModbusFunction function = GetWriteFunction(point);
        bool success = ExecuteWrite(slave_id, function, point.address, modbus_value);
        
        auto operation_end = steady_clock::now();
        auto duration_ms = duration_cast<milliseconds>(operation_end - operation_start).count();
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_writes++;
            if (success) {
                statistics_.successful_writes++;
                last_successful_operation_ = steady_clock::now();
            } else {
                statistics_.failed_writes++;
            }
        }
        
        logger_->LogModbusOperation(slave_id, point.address, 
                                   std::to_string(modbus_value), success, duration_ms);
        logger_->LogPerformanceMetric("WriteValue", duration_ms, "ms");
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::DATA_FORMAT_ERROR, "Write value conversion error: " + std::string(e.what()));
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_writes++;
            statistics_.failed_writes++;
        }
        
        return false;
    }
}

// =============================================================================
// Modbus 특화 메소드 구현
// =============================================================================

bool ModbusDriver::ReadHoldingRegisters(int slave_id, int start_addr, int count, 
                                       std::vector<uint16_t>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    return ExecuteRead(slave_id, ModbusFunction::READ_HOLDING_REGISTERS, 
                       start_addr, count, values);
}

bool ModbusDriver::ReadInputRegisters(int slave_id, int start_addr, int count,
                                     std::vector<uint16_t>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    return ExecuteRead(slave_id, ModbusFunction::READ_INPUT_REGISTERS, 
                       start_addr, count, values);
}

bool ModbusDriver::ReadCoils(int slave_id, int start_addr, int count,
                           std::vector<bool>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    values.resize(count);
    uint8_t* coil_values = new uint8_t[count];
    
    modbus_set_slave(modbus_ctx_, slave_id);
    int result = modbus_read_bits(modbus_ctx_, start_addr, count, coil_values);
    
    bool success = (result == count);
    if (success) {
        for (int i = 0; i < count; ++i) {
            values[i] = (coil_values[i] != 0);
        }
    }
    
    delete[] coil_values;
    return success;
}

bool ModbusDriver::ReadDiscreteInputs(int slave_id, int start_addr, int count,
                                     std::vector<bool>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    values.resize(count);
    uint8_t* input_values = new uint8_t[count];
    
    modbus_set_slave(modbus_ctx_, slave_id);
    int result = modbus_read_input_bits(modbus_ctx_, start_addr, count, input_values);
    
    bool success = (result == count);
    if (success) {
        for (int i = 0; i < count; ++i) {
            values[i] = (input_values[i] != 0);
        }
    }
    
    delete[] input_values;
    return success;
}

bool ModbusDriver::WriteSingleRegister(int slave_id, int addr, uint16_t value) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    return ExecuteWrite(slave_id, ModbusFunction::WRITE_SINGLE_REGISTER, addr, value);
}

bool ModbusDriver::WriteMultipleRegisters(int slave_id, int start_addr,
                                         const std::vector<uint16_t>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    return ExecuteWriteMultiple(slave_id, ModbusFunction::WRITE_MULTIPLE_REGISTERS,
                               start_addr, values);
}

bool ModbusDriver::WriteSingleCoil(int slave_id, int addr, bool value) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    modbus_set_slave(modbus_ctx_, slave_id);
    int result = modbus_write_bit(modbus_ctx_, addr, value ? 1 : 0);
    
    return (result == 1);
}

bool ModbusDriver::WriteMultipleCoils(int slave_id, int start_addr,
                                     const std::vector<bool>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        return false;
    }
    
    uint8_t* coil_values = new uint8_t[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        coil_values[i] = values[i] ? 1 : 0;
    }
    
    modbus_set_slave(modbus_ctx_, slave_id);
    int result = modbus_write_bits(modbus_ctx_, start_addr, values.size(), coil_values);
    
    delete[] coil_values;
    return (result == static_cast<int>(values.size()));
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

bool ModbusDriver::ExecuteRead(int slave_id, ModbusFunction function, 
                              int start_addr, int count, 
                              std::vector<uint16_t>& values) {
    values.resize(count);
    
    modbus_set_slave(modbus_ctx_, slave_id);
    
    int result = -1;
    switch (function) {
        case ModbusFunction::WRITE_MULTIPLE_REGISTERS:
            result = modbus_write_registers(modbus_ctx_, start_addr, values.size(), values.data());
            break;
        default:
            SetError(ErrorCode::INVALID_PARAMETER, "Unsupported multiple write function");
            return false;
    }
    
    if (result == static_cast<int>(values.size())) {
        return true;
    } else {
        ErrorCode error_code = TranslateModbusError(errno);
        std::string error_msg = GetModbusErrorString(errno);
        SetError(error_code, "Multiple write failed: " + error_msg);
        return false;
    }
}

// =============================================================================
// 데이터 변환 유틸리티
// =============================================================================

DataValue ModbusDriver::ConvertModbusValue(const DataPoint& point, uint16_t raw_value) {
    // 스케일링 적용
    double scaled_value = static_cast<double>(raw_value) * point.scaling_factor + point.scaling_offset;
    
    // 데이터 타입에 따른 변환
    switch (point.data_type) {
        case DataType::BOOL:
            return DataValue(raw_value != 0);
        case DataType::INT16:
            return DataValue(static_cast<int16_t>(raw_value));
        case DataType::UINT16:
            return DataValue(raw_value);
        case DataType::INT32:
            return DataValue(static_cast<int32_t>(scaled_value));
        case DataType::UINT32:
            return DataValue(static_cast<uint32_t>(scaled_value));
        case DataType::FLOAT:
            return DataValue(static_cast<float>(scaled_value));
        case DataType::DOUBLE:
            return DataValue(scaled_value);
        default:
            return DataValue(static_cast<int32_t>(scaled_value));
    }
}

uint16_t ModbusDriver::ConvertToModbusValue(const DataPoint& point, const DataValue& value) {
    double numeric_value = 0.0;
    
    // DataValue에서 숫자 값 추출
    if (std::holds_alternative<bool>(value)) {
        numeric_value = std::get<bool>(value) ? 1.0 : 0.0;
    } else if (std::holds_alternative<int16_t>(value)) {
        numeric_value = static_cast<double>(std::get<int16_t>(value));
    } else if (std::holds_alternative<uint16_t>(value)) {
        numeric_value = static_cast<double>(std::get<uint16_t>(value));
    } else if (std::holds_alternative<int32_t>(value)) {
        numeric_value = static_cast<double>(std::get<int32_t>(value));
    } else if (std::holds_alternative<uint32_t>(value)) {
        numeric_value = static_cast<double>(std::get<uint32_t>(value));
    } else if (std::holds_alternative<float>(value)) {
        numeric_value = static_cast<double>(std::get<float>(value));
    } else if (std::holds_alternative<double>(value)) {
        numeric_value = std::get<double>(value);
    } else {
        throw std::invalid_argument("Unsupported data type for Modbus write");
    }
    
    // 역스케일링 적용
    double unscaled_value = (numeric_value - point.scaling_offset) / point.scaling_factor;
    
    // 범위 체크
    if (unscaled_value < 0) {
        unscaled_value = 0;
    } else if (unscaled_value > 65535) {
        unscaled_value = 65535;
    }
    
    return static_cast<uint16_t>(unscaled_value);
}

ModbusDriver::ModbusFunction ModbusDriver::GetReadFunction(const DataPoint& point) {
    // properties에서 함수 타입 확인
    auto func_it = point.properties.find("modbus_function");
    if (func_it != point.properties.end()) {
        if (func_it->second == "holding_registers" || func_it->second == "03") {
            return ModbusFunction::READ_HOLDING_REGISTERS;
        } else if (func_it->second == "input_registers" || func_it->second == "04") {
            return ModbusFunction::READ_INPUT_REGISTERS;
        } else if (func_it->second == "coils" || func_it->second == "01") {
            return ModbusFunction::READ_COILS;
        } else if (func_it->second == "discrete_inputs" || func_it->second == "02") {
            return ModbusFunction::READ_DISCRETE_INPUTS;
        }
    }
    
    // 기본값: Holding Registers
    return ModbusFunction::READ_HOLDING_REGISTERS;
}

ModbusDriver::ModbusFunction ModbusDriver::GetWriteFunction(const DataPoint& point) {
    // properties에서 함수 타입 확인
    auto func_it = point.properties.find("modbus_function");
    if (func_it != point.properties.end()) {
        if (func_it->second == "coils" || func_it->second == "05") {
            return ModbusFunction::WRITE_SINGLE_COIL;
        }
    }
    
    // 기본값: Single Register
    return ModbusFunction::WRITE_SINGLE_REGISTER;
}

// =============================================================================
// 에러 처리
// =============================================================================

void ModbusDriver::SetError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = code;
    last_error_.message = message;
    last_error_.timestamp = system_clock::now();
    
    if (logger_) {
        logger_->Error(message, DriverLogCategory::ERROR_HANDLING);
    }
    
    // 에러 콜백 호출
    NotifyError(config_.device_id, last_error_);
}

ErrorCode ModbusDriver::TranslateModbusError(int modbus_errno) {
    switch (modbus_errno) {
        case EMBXILFUN:  // Illegal function
            return ErrorCode::PROTOCOL_ERROR;
        case EMBXILADD:  // Illegal data address
            return ErrorCode::INVALID_ADDRESS;
        case EMBXILVAL:  // Illegal data value
            return ErrorCode::INVALID_PARAMETER;
        case EMBXSFAIL:  // Slave device failure
            return ErrorCode::DEVICE_NOT_RESPONDING;
        case EMBXACK:    // Acknowledge
            return ErrorCode::SUCCESS;
        case EMBXSBUSY:  // Slave device busy
            return ErrorCode::DEVICE_NOT_RESPONDING;
        case EMBXNACK:   // Negative acknowledge
            return ErrorCode::PROTOCOL_ERROR;
        case EMBXMEMPAR: // Memory parity error
            return ErrorCode::DATA_FORMAT_ERROR;
        case EMBXGPATH:  // Gateway path unavailable
            return ErrorCode::CONNECTION_FAILED;
        case EMBXGTAR:   // Gateway target device failed
            return ErrorCode::DEVICE_NOT_RESPONDING;
        case ECONNRESET:
        case ECONNREFUSED:
        case EHOSTUNREACH:
            return ErrorCode::CONNECTION_FAILED;
        case ETIMEDOUT:
            return ErrorCode::TIMEOUT;
        case EPERM:
        case EACCES:
            return ErrorCode::PERMISSION_DENIED;
        default:
            return ErrorCode::UNKNOWN_ERROR;
    }
}

std::string ModbusDriver::GetModbusErrorString(int modbus_errno) {
    return std::string(modbus_strerror(modbus_errno));
}

// =============================================================================
// 통계 및 진단
// =============================================================================

void ModbusDriver::UpdateStatistics(bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_reads++;
    if (success) {
        statistics_.successful_reads++;
    } else {
        statistics_.failed_reads++;
    }
    
    // 응답 시간 통계 업데이트
    if (statistics_.total_reads == 1) {
        statistics_.average_response_time_ms = response_time_ms;
        statistics_.max_response_time_ms = response_time_ms;
    } else {
        // 이동 평균 계산
        statistics_.average_response_time_ms = 
            (statistics_.average_response_time_ms * (statistics_.total_reads - 1) + response_time_ms) / 
            statistics_.total_reads;
        
        if (response_time_ms > statistics_.max_response_time_ms) {
            statistics_.max_response_time_ms = response_time_ms;
        }
    }
    
    if (success) {
        statistics_.last_successful_read = system_clock::now();
    } else {
        statistics_.last_error = system_clock::now();
        statistics_.last_error_info = last_error_;
    }
}

DriverStatistics ModbusDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void ModbusDriver::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.Reset();
}

std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return diagnostics_;
}

void ModbusDriver::UpdateDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    diagnostics_["protocol"] = ProtocolTypeToString(protocol_type_);
    diagnostics_["endpoint"] = config_.endpoint;
    diagnostics_["status"] = std::to_string(static_cast<int>(status_));
    diagnostics_["connected"] = is_connected_ ? "true" : "false";
    
    // libmodbus 버전 정보
    diagnostics_["libmodbus_version"] = LIBMODBUS_VERSION_STRING;
    
    // 연결 정보
    if (modbus_ctx_ != nullptr) {
        diagnostics_["timeout_ms"] = std::to_string(config_.timeout_ms);
        diagnostics_["retry_count"] = std::to_string(config_.retry_count);
    }
    
    // 통계 정보
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        diagnostics_["total_reads"] = std::to_string(statistics_.total_reads);
        diagnostics_["successful_reads"] = std::to_string(statistics_.successful_reads);
        diagnostics_["failed_reads"] = std::to_string(statistics_.failed_reads);
        diagnostics_["success_rate"] = std::to_string(statistics_.GetSuccessRate()) + "%";
        diagnostics_["avg_response_time"] = std::to_string(statistics_.average_response_time_ms) + "ms";
        diagnostics_["max_response_time"] = std::to_string(statistics_.max_response_time_ms) + "ms";
    }
    
    // 마지막 활동 시간
    auto now = steady_clock::now();
    auto last_activity_sec = duration_cast<seconds>(now - last_successful_operation_).count();
    diagnostics_["last_activity_sec"] = std::to_string(last_activity_sec);
}

// =============================================================================
// Watchdog 및 연결 감시
// =============================================================================

void ModbusDriver::WatchdogLoop() {
    const auto check_interval = std::chrono::seconds(30);  // 30초마다 체크
    const auto connection_timeout = std::chrono::minutes(5);  // 5분 타임아웃
    
    while (!stop_watchdog_) {
        std::this_thread::sleep_for(check_interval);
        
        if (stop_watchdog_) {
            break;
        }
        
        // 연결 상태 체크
        if (is_connected_) {
            auto now = steady_clock::now();
            auto time_since_last_activity = now - last_successful_operation_;
            
            if (time_since_last_activity > connection_timeout) {
                logger_->Warn("Connection timeout detected, attempting reconnection",
                             DriverLogCategory::CONNECTION);
                
                // 재연결 시도
                bool reconnected = false;
                {
                    std::lock_guard<std::mutex> lock(connection_mutex_);
                    CloseConnection();
                    is_connected_ = false;
                    
                    if (EstablishConnection()) {
                        is_connected_ = true;
                        last_successful_operation_ = steady_clock::now();
                        reconnected = true;
                        
                        logger_->Info("Watchdog reconnection successful",
                                     DriverLogCategory::CONNECTION);
                        
                        NotifyConnectionStatusChanged(config_.device_id,
                                                      ConnectionStatus::ERROR,
                                                      ConnectionStatus::CONNECTED,
                                                      "Reconnected by watchdog");
                    } else {
                        status_ = DriverStatus::ERROR;
                        
                        logger_->Error("Watchdog reconnection failed",
                                      DriverLogCategory::ERROR_HANDLING);
                        
                        NotifyConnectionStatusChanged(config_.device_id,
                                                      ConnectionStatus::CONNECTED,
                                                      ConnectionStatus::ERROR,
                                                      "Watchdog reconnection failed");
                    }
                }
            }
        }
        
        // 진단 정보 업데이트
        UpdateDiagnostics();
    }
}

// =============================================================================
// 드라이버 자동 등록
// =============================================================================

// Modbus TCP 드라이버 등록
REGISTER_DRIVER(ProtocolType::MODBUS_TCP, ModbusDriver);

// Modbus RTU 드라이버 등록  
REGISTER_DRIVER(ProtocolType::MODBUS_RTU, ModbusDriver); (function) {
        case ModbusFunction::READ_HOLDING_REGISTERS:
            result = modbus_read_registers(modbus_ctx_, start_addr, count, values.data());
            break;
        case ModbusFunction::READ_INPUT_REGISTERS:
            result = modbus_read_input_registers(modbus_ctx_, start_addr, count, values.data());
            break;
        default:
            SetError(ErrorCode::INVALID_PARAMETER, "Unsupported read function");
            return false;
    }
    
    if (result == count) {
        return true;
    } else {
        ErrorCode error_code = TranslateModbusError(errno);
        std::string error_msg = GetModbusErrorString(errno);
        SetError(error_code, "Read failed: " + error_msg);
        return false;
    }
}

bool ModbusDriver::ExecuteWrite(int slave_id, ModbusFunction function, 
                               int addr, uint16_t value) {
    modbus_set_slave(modbus_ctx_, slave_id);
    
    int result = -1;
    switch (function) {
        case ModbusFunction::WRITE_SINGLE_REGISTER:
            result = modbus_write_register(modbus_ctx_, addr, value);
            break;
        case ModbusFunction::WRITE_SINGLE_COIL:
            result = modbus_write_bit(modbus_ctx_, addr, value ? 1 : 0);
            break;
        default:
            SetError(ErrorCode::INVALID_PARAMETER, "Unsupported write function");
            return false;
    }
    
    if (result == 1) {
        return true;
    } else {
        ErrorCode error_code = TranslateModbusError(errno);
        std::string error_msg = GetModbusErrorString(errno);
        SetError(error_code, "Write failed: " + error_msg);
        return false;
    }
}

// =============================================================================
// collector/src/Drivers/ModbusDriver.cpp - 완전 구현
// Modbus 드라이버 구현 (계속)
// =============================================================================

bool ModbusDriver::ExecuteWriteMultiple(int slave_id, ModbusFunction function,
                                       int start_addr, 
                                       const std::vector<uint16_t>& values) {
    modbus_set_slave(modbus_ctx_, slave_id);
    
    int result = -1;
    switch (function) {
        case ModbusFunction::WRITE_MULTIPLE_REGISTERS:
            result = modbus_write_registers(modbus_ctx_, start_addr, values.size(), values.data());
            break;
        case ModbusFunction::WRITE_MULTIPLE_COILS: {
            // uint16_t를 uint8_t로 변환 (코일용)
            std::vector<uint8_t> coil_values(values.size());
            for (size_t i = 0; i < values.size(); ++i) {
                coil_values[i] = values[i] ? 1 : 0;
            }
            result = modbus_write_bits(modbus_ctx_, start_addr, values.size(), coil_values.data());
            break;
        }
        default:
            SetError(ErrorCode::INVALID_PARAMETER, "Unsupported multiple write function");
            return false;
    }
    
    if (result == static_cast<int>(values.size())) {
        return true;
    } else {
        ErrorCode error_code = TranslateModbusError(errno);
        std::string error_msg = GetModbusErrorString(errno);
        SetError(error_code, "Multiple write failed: " + error_msg);
        return false;
    }
}

// =============================================================================
// 데이터 변환 유틸리티 (계속)
// =============================================================================

DataValue ModbusDriver::ConvertModbusValue(const DataPoint& point, uint16_t raw_value) {
    // 스케일링 적용
    double scaled_value = static_cast<double>(raw_value) * point.scaling_factor + point.scaling_offset;
    
    // 범위 체크
    if (scaled_value < point.min_value) {
        scaled_value = point.min_value;
        logger_->Warn("Value below minimum range for point " + point.name + 
                     ", clamped to " + std::to_string(point.min_value),
                     DriverLogCategory::DATA_PROCESSING);
    } else if (scaled_value > point.max_value) {
        scaled_value = point.max_value;
        logger_->Warn("Value above maximum range for point " + point.name + 
                     ", clamped to " + std::to_string(point.max_value),
                     DriverLogCategory::DATA_PROCESSING);
    }
    
    // 데이터 타입에 따른 변환
    switch (point.data_type) {
        case DataType::BOOL:
            return DataValue(raw_value != 0);
        case DataType::INT8:
            return DataValue(static_cast<int8_t>(std::clamp(static_cast<int>(scaled_value), -128, 127)));
        case DataType::UINT8:
            return DataValue(static_cast<uint8_t>(std::clamp(static_cast<int>(scaled_value), 0, 255)));
        case DataType::INT16:
            return DataValue(static_cast<int16_t>(raw_value));
        case DataType::UINT16:
            return DataValue(raw_value);
        case DataType::INT32:
            return DataValue(static_cast<int32_t>(scaled_value));
        case DataType::UINT32:
            return DataValue(static_cast<uint32_t>(scaled_value));
        case DataType::FLOAT:
            return DataValue(static_cast<float>(scaled_value));
        case DataType::DOUBLE:
            return DataValue(scaled_value);
        case DataType::STRING:
            return DataValue(std::to_string(scaled_value));
        default:
            logger_->Warn("Unsupported data type for point " + point.name + 
                         ", defaulting to int32",
                         DriverLogCategory::DATA_PROCESSING);
            return DataValue(static_cast<int32_t>(scaled_value));
    }
}

uint16_t ModbusDriver::ConvertToModbusValue(const DataPoint& point, const DataValue& value) {
    double numeric_value = 0.0;
    
    // DataValue에서 숫자 값 추출
    if (std::holds_alternative<bool>(value)) {
        numeric_value = std::get<bool>(value) ? 1.0 : 0.0;
    } else if (std::holds_alternative<int8_t>(value)) {
        numeric_value = static_cast<double>(std::get<int8_t>(value));
    } else if (std::holds_alternative<uint8_t>(value)) {
        numeric_value = static_cast<double>(std::get<uint8_t>(value));
    } else if (std::holds_alternative<int16_t>(value)) {
        numeric_value = static_cast<double>(std::get<int16_t>(value));
    } else if (std::holds_alternative<uint16_t>(value)) {
        numeric_value = static_cast<double>(std::get<uint16_t>(value));
    } else if (std::holds_alternative<int32_t>(value)) {
        numeric_value = static_cast<double>(std::get<int32_t>(value));
    } else if (std::holds_alternative<uint32_t>(value)) {
        numeric_value = static_cast<double>(std::get<uint32_t>(value));
    } else if (std::holds_alternative<float>(value)) {
        numeric_value = static_cast<double>(std::get<float>(value));
    } else if (std::holds_alternative<double>(value)) {
        numeric_value = std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        try {
            numeric_value = std::stod(std::get<std::string>(value));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Cannot convert string '" + 
                std::get<std::string>(value) + "' to numeric value: " + e.what());
        }
    } else {
        throw std::invalid_argument("Unsupported data type for Modbus write");
    }
    
    // 역스케일링 적용
    double unscaled_value = (numeric_value - point.scaling_offset) / point.scaling_factor;
    
    // 범위 체크 및 클램핑
    if (unscaled_value < 0) {
        logger_->Warn("Write value below 0 for point " + point.name + 
                     ", clamped to 0", DriverLogCategory::DATA_PROCESSING);
        unscaled_value = 0;
    } else if (unscaled_value > 65535) {
        logger_->Warn("Write value above 65535 for point " + point.name + 
                     ", clamped to 65535", DriverLogCategory::DATA_PROCESSING);
        unscaled_value = 65535;
    }
    
    return static_cast<uint16_t>(std::round(unscaled_value));
}

ModbusDriver::ModbusFunction ModbusDriver::GetReadFunction(const DataPoint& point) {
    // properties에서 함수 타입 확인
    auto func_it = point.properties.find("modbus_function");
    if (func_it != point.properties.end()) {
        std::string func_type = func_it->second;
        std::transform(func_type.begin(), func_type.end(), func_type.begin(), ::tolower);
        
        if (func_type == "holding_registers" || func_type == "03" || func_type == "3") {
            return ModbusFunction::READ_HOLDING_REGISTERS;
        } else if (func_type == "input_registers" || func_type == "04" || func_type == "4") {
            return ModbusFunction::READ_INPUT_REGISTERS;
        } else if (func_type == "coils" || func_type == "01" || func_type == "1") {
            return ModbusFunction::READ_COILS;
        } else if (func_type == "discrete_inputs" || func_type == "02" || func_type == "2") {
            return ModbusFunction::READ_DISCRETE_INPUTS;
        }
    }
    
    // 데이터 타입 기반 기본값
    if (point.data_type == DataType::BOOL) {
        return ModbusFunction::READ_DISCRETE_INPUTS;
    }
    
    // 기본값: Holding Registers
    return ModbusFunction::READ_HOLDING_REGISTERS;
}

ModbusDriver::ModbusFunction ModbusDriver::GetWriteFunction(const DataPoint& point) {
    // properties에서 함수 타입 확인
    auto func_it = point.properties.find("modbus_function");
    if (func_it != point.properties.end()) {
        std::string func_type = func_it->second;
        std::transform(func_type.begin(), func_type.end(), func_type.begin(), ::tolower);
        
        if (func_type == "coils" || func_type == "05" || func_type == "5") {
            return ModbusFunction::WRITE_SINGLE_COIL;
        } else if (func_type == "multiple_coils" || func_type == "15" || func_type == "0f") {
            return ModbusFunction::WRITE_MULTIPLE_COILS;
        } else if (func_type == "multiple_registers" || func_type == "16" || func_type == "10") {
            return ModbusFunction::WRITE_MULTIPLE_REGISTERS;
        }
    }
    
    // 데이터 타입 기반 기본값
    if (point.data_type == DataType::BOOL) {
        return ModbusFunction::WRITE_SINGLE_COIL;
    }
    
    // 기본값: Single Register
    return ModbusFunction::WRITE_SINGLE_REGISTER;
}

// =============================================================================
// 에러 처리 (계속)
// =============================================================================

void ModbusDriver::SetError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = code;
    last_error_.message = message;
    last_error_.timestamp = system_clock::now();
    last_error_.context = "ModbusDriver";
    last_error_.file = __FILE__;
    last_error_.line = __LINE__;
    
    if (logger_) {
        logger_->LogError(last_error_, "Modbus driver error occurred");
    }
    
    // 에러 콜백 호출
    NotifyError(config_.device_id, last_error_);
    
    // 심각한 에러의 경우 상태 변경
    if (code == ErrorCode::CONNECTION_FAILED || 
        code == ErrorCode::DEVICE_NOT_RESPONDING) {
        status_ = DriverStatus::ERROR;
    }
}

ErrorCode ModbusDriver::TranslateModbusError(int modbus_errno) {
    switch (modbus_errno) {
        // Modbus 예외 코드들
        case EMBXILFUN:  // 01 - Illegal function
            return ErrorCode::PROTOCOL_ERROR;
        case EMBXILADD:  // 02 - Illegal data address
            return ErrorCode::INVALID_ADDRESS;
        case EMBXILVAL:  // 03 - Illegal data value
            return ErrorCode::INVALID_PARAMETER;
        case EMBXSFAIL:  // 04 - Slave device failure
            return ErrorCode::DEVICE_NOT_RESPONDING;
        case EMBXACK:    // 05 - Acknowledge
            return ErrorCode::SUCCESS;
        case EMBXSBUSY:  // 06 - Slave device busy
            return ErrorCode::DEVICE_NOT_RESPONDING;
        case EMBXNACK:   // 07 - Negative acknowledge
            return ErrorCode::PROTOCOL_ERROR;
        case EMBXMEMPAR: // 08 - Memory parity error
            return ErrorCode::DATA_FORMAT_ERROR;
        case EMBXGPATH:  // 10 - Gateway path unavailable
            return ErrorCode::CONNECTION_FAILED;
        case EMBXGTAR:   // 11 - Gateway target device failed
            return ErrorCode::DEVICE_NOT_RESPONDING;
            
        // 시스템 에러 코드들
        case ECONNRESET:
        case ECONNREFUSED:
        case EHOSTUNREACH:
        case ENETUNREACH:
            return ErrorCode::CONNECTION_FAILED;
        case ETIMEDOUT:
            return ErrorCode::TIMEOUT;
        case EPERM:
        case EACCES:
            return ErrorCode::PERMISSION_DENIED;
        case EAGAIN:
        case EWOULDBLOCK:
            return ErrorCode::TIMEOUT;
        case EINVAL:
            return ErrorCode::INVALID_PARAMETER;
        case ENOMEM:
            return ErrorCode::INSUFFICIENT_RESOURCES;
        case ENODEV:
        case ENOENT:
            return ErrorCode::DEVICE_NOT_RESPONDING;
        default:
            return ErrorCode::UNKNOWN_ERROR;
    }
}

std::string ModbusDriver::GetModbusErrorString(int modbus_errno) {
    std::string base_error = modbus_strerror(modbus_errno);
    
    // 추가 컨텍스트 정보 제공
    switch (modbus_errno) {
        case EMBXILFUN:
            return base_error + " (Function code not supported by device)";
        case EMBXILADD:
            return base_error + " (Address not available in device)";
        case EMBXILVAL:
            return base_error + " (Invalid data value for this register)";
        case EMBXSFAIL:
            return base_error + " (Device internal error)";
        case EMBXSBUSY:
            return base_error + " (Device is processing another command)";
        case ECONNRESET:
            return base_error + " (Connection reset by peer)";
        case ECONNREFUSED:
            return base_error + " (Connection refused by server)";
        case ETIMEDOUT:
            return base_error + " (Operation timed out)";
        default:
            return base_error;
    }
}

// =============================================================================
// 통계 및 진단 (계속)
// =============================================================================

void ModbusDriver::UpdateStatistics(bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_reads++;
    if (success) {
        statistics_.successful_reads++;
        statistics_.last_successful_read = system_clock::now();
    } else {
        statistics_.failed_reads++;
        statistics_.last_error = system_clock::now();
        statistics_.last_error_info = last_error_;
    }
    
    // 응답 시간 통계 업데이트
    if (response_time_ms > 0) {
        if (statistics_.total_reads == 1) {
            statistics_.average_response_time_ms = response_time_ms;
            statistics_.max_response_time_ms = response_time_ms;
        } else {
            // 이동 평균 계산 (최근 100개 샘플에 더 가중치)
            double weight = std::min(100.0, static_cast<double>(statistics_.total_reads));
            statistics_.average_response_time_ms = 
                (statistics_.average_response_time_ms * (weight - 1) + response_time_ms) / weight;
            
            if (response_time_ms > statistics_.max_response_time_ms) {
                statistics_.max_response_time_ms = response_time_ms;
            }
        }
    }
    
    // 바이트 전송 통계 (추정값)
    statistics_.total_bytes_sent += 8;  // 기본 Modbus 요청 크기
    if (success) {
        statistics_.total_bytes_received += 16;  // 기본 응답 크기
    }
}

DriverStatistics ModbusDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void ModbusDriver::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.Reset();
    logger_->Info("Modbus driver statistics reset", DriverLogCategory::GENERAL);
}

std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return diagnostics_;
}

void ModbusDriver::UpdateDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    // 기본 정보
    diagnostics_["protocol"] = ProtocolTypeToString(protocol_type_);
    diagnostics_["endpoint"] = config_.endpoint;
    diagnostics_["device_name"] = config_.device_name;
    diagnostics_["status"] = std::to_string(static_cast<int>(status_));
    diagnostics_["connected"] = is_connected_ ? "true" : "false";
    
    // libmodbus 정보
    diagnostics_["libmodbus_version"] = LIBMODBUS_VERSION_STRING;
    diagnostics_["modbus_backend"] = (protocol_type_ == ProtocolType::MODBUS_TCP) ? "TCP" : "RTU";
    
    // 연결 설정
    if (modbus_ctx_ != nullptr) {
        diagnostics_["timeout_ms"] = std::to_string(config_.timeout_ms);
        diagnostics_["retry_count"] = std::to_string(config_.retry_count);
        diagnostics_["polling_interval_ms"] = std::to_string(config_.polling_interval_ms);
        
        // TCP 전용 정보
        if (protocol_type_ == ProtocolType::MODBUS_TCP) {
            diagnostics_["tcp_nodelay"] = "true";  // libmodbus 기본값
        }
        
        // RTU 전용 정보
        if (protocol_type_ == ProtocolType::MODBUS_RTU) {
            auto baud_it = config_.protocol_settings.find("baud_rate");
            if (baud_it != config_.protocol_settings.end()) {
                diagnostics_["baud_rate"] = baud_it->second;
            }
            
            auto parity_it = config_.protocol_settings.find("parity");
            if (parity_it != config_.protocol_settings.end()) {
                diagnostics_["parity"] = parity_it->second;
            }
        }
    }
    
    // 통계 정보
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        diagnostics_["total_reads"] = std::to_string(statistics_.total_reads);
        diagnostics_["successful_reads"] = std::to_string(statistics_.successful_reads);
        diagnostics_["failed_reads"] = std::to_string(statistics_.failed_reads);
        diagnostics_["total_writes"] = std::to_string(statistics_.total_writes);
        diagnostics_["successful_writes"] = std::to_string(statistics_.successful_writes);
        diagnostics_["failed_writes"] = std::to_string(statistics_.failed_writes);
        
        double success_rate = statistics_.GetSuccessRate();
        diagnostics_["success_rate"] = std::to_string(success_rate) + "%";
        
        diagnostics_["avg_response_time_ms"] = std::to_string(statistics_.average_response_time_ms);
        diagnostics_["max_response_time_ms"] = std::to_string(statistics_.max_response_time_ms);
        
        diagnostics_["bytes_sent"] = std::to_string(statistics_.total_bytes_sent);
        diagnostics_["bytes_received"] = std::to_string(statistics_.total_bytes_received);
        
        diagnostics_["connection_attempts"] = std::to_string(statistics_.connection_attempts);
        diagnostics_["successful_connections"] = std::to_string(statistics_.successful_connections);
    }
    
    // 마지막 활동 정보
    auto now = steady_clock::now();
    auto last_activity_ms = duration_cast<milliseconds>(now - last_successful_operation_).count();
    diagnostics_["last_activity_ms"] = std::to_string(last_activity_ms);
    
    // 현재 상태 설명
    switch (status_) {
        case DriverStatus::UNINITIALIZED:
            diagnostics_["status_description"] = "Driver not initialized";
            break;
        case DriverStatus::INITIALIZED:
            diagnostics_["status_description"] = "Driver initialized, not connected";
            break;
        case DriverStatus::CONNECTING:
            diagnostics_["status_description"] = "Connection in progress";
            break;
        case DriverStatus::CONNECTED:
            diagnostics_["status_description"] = "Connected and operational";
            break;
        case DriverStatus::DISCONNECTING:
            diagnostics_["status_description"] = "Disconnection in progress";
            break;
        case DriverStatus::DISCONNECTED:
            diagnostics_["status_description"] = "Disconnected";
            break;
        case DriverStatus::ERROR:
            diagnostics_["status_description"] = "Error state: " + last_error_.message;
            break;
        case DriverStatus::MAINTENANCE:
            diagnostics_["status_description"] = "Maintenance mode";
            break;
    }
    
    // 환경 정보
    diagnostics_["thread_count"] = watchdog_thread_.joinable() ? "2" : "1";  // 메인 + watchdog
    diagnostics_["memory_usage"] = "N/A";  // 실제 구현에서는 메모리 사용량 측정
    
    // 마지막 에러 정보
    {
        std::lock_guard<std::mutex> error_lock(error_mutex_);
        if (last_error_.code != ErrorCode::SUCCESS) {
            diagnostics_["last_error_code"] = std::to_string(static_cast<int>(last_error_.code));
            diagnostics_["last_error_message"] = last_error_.message;
            diagnostics_["last_error_time"] = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    last_error_.timestamp.time_since_epoch()).count());
        }
    }
}

// =============================================================================
// Watchdog 및 연결 감시 (계속)
// =============================================================================

void ModbusDriver::WatchdogLoop() {
    const auto check_interval = std::chrono::seconds(30);  // 30초마다 체크
    const auto connection_timeout = std::chrono::minutes(5);  // 5분 타임아웃
    const auto max_consecutive_failures = 3;  // 연속 실패 임계값
    
    int consecutive_failures = 0;
    
    logger_->Info("Modbus watchdog started", DriverLogCategory::GENERAL);
    
    while (!stop_watchdog_) {
        std::this_thread::sleep_for(check_interval);
        
        if (stop_watchdog_) {
            break;
        }
        
        try {
            // 연결 상태 체크
            if (is_connected_) {
                auto now = steady_clock::now();
                auto time_since_last_activity = now - last_successful_operation_;
                
                if (time_since_last_activity > connection_timeout) {
                    logger_->Warn("Connection timeout detected (" + 
                                 std::to_string(duration_cast<minutes>(time_since_last_activity).count()) + 
                                 " min), testing connection",
                                 DriverLogCategory::CONNECTION);
                    
                    // 간단한 연결 테스트 (레지스터 1개 읽기 시도)
                    bool connection_ok = false;
                    {
                        std::lock_guard<std::mutex> lock(connection_mutex_);
                        if (modbus_ctx_ != nullptr) {
                            uint16_t test_value;
                            // 슬레이브 1, 주소 0에서 1개 레지스터 읽기 시도
                            modbus_set_slave(modbus_ctx_, 1);
                            int result = modbus_read_input_registers(modbus_ctx_, 0, 1, &test_value);
                            connection_ok = (result == 1);
                            
                            if (connection_ok) {
                                last_successful_operation_ = steady_clock::now();
                                consecutive_failures = 0;
                                logger_->Info("Watchdog connection test passed", 
                                             DriverLogCategory::CONNECTION);
                            } else {
                                consecutive_failures++;
                                logger_->Warn("Watchdog connection test failed (attempt " + 
                                             std::to_string(consecutive_failures) + ")",
                                             DriverLogCategory::CONNECTION);
                            }
                        }
                    }
                    
                    // 연속 실패가 임계값을 넘으면 재연결 시도
                    if (!connection_ok && consecutive_failures >= max_consecutive_failures) {
                        logger_->Error("Connection test failed " + std::to_string(consecutive_failures) + 
                                      " times, attempting reconnection",
                                      DriverLogCategory::ERROR_HANDLING);
                        
                        // 재연결 시도
                        bool reconnected = false;
                        {
                            std::lock_guard<std::mutex> lock(connection_mutex_);
                            CloseConnection();
                            is_connected_ = false;
                            status_ = DriverStatus::CONNECTING;
                            
                            if (EstablishConnection()) {
                                is_connected_ = true;
                                status_ = DriverStatus::CONNECTED;
                                last_successful_operation_ = steady_clock::now();
                                consecutive_failures = 0;
                                reconnected = true;
                                
                                logger_->Info("Watchdog reconnection successful",
                                             DriverLogCategory::CONNECTION);
                                
                                NotifyConnectionStatusChanged(config_.device_id,
                                                              ConnectionStatus::ERROR,
                                                              ConnectionStatus::CONNECTED,
                                                              "Reconnected by watchdog");
                            } else {
                                status_ = DriverStatus::ERROR;
                                
                                logger_->Error("Watchdog reconnection failed",
                                              DriverLogCategory::ERROR_HANDLING);
                                
                                NotifyConnectionStatusChanged(config_.device_id,
                                                              ConnectionStatus::CONNECTED,
                                                              ConnectionStatus::ERROR,
                                                              "Watchdog reconnection failed");
                            }
                        }
                        
                        // 재연결 실패 시 더 긴 대기
                        if (!reconnected) {
                            std::this_thread::sleep_for(std::chrono::minutes(1));
                        }
                    }
                }
            }
            
            // 진단 정보 정기 업데이트
            UpdateDiagnostics();
            
            // 메모리 사용량 체크 (선택적)
            static int diagnostic_counter = 0;
            if (++diagnostic_counter >= 10) {  // 5분마다 (30초 * 10)
                diagnostic_counter = 0;
                
                logger_->LogPerformanceMetric("WatchdogCycle", 
                    duration_cast<milliseconds>(check_interval).count(), "ms");
                
                // 통계 로깅
                auto stats = GetStatistics();
                if (stats.total_reads > 0) {
                    logger_->Info("Modbus stats - Success rate: " + 
                                 std::to_string(stats.GetSuccessRate()) + 
                                 "%, Avg response: " + 
                                 std::to_string(stats.average_response_time_ms) + "ms",
                                 DriverLogCategory::PERFORMANCE);
                }
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Watchdog exception: " + std::string(e.what()),
                          DriverLogCategory::ERROR_HANDLING);
        }
    }
    
    logger_->Info("Modbus watchdog stopped", DriverLogCategory::GENERAL);
}

// =============================================================================
// 고급 Modbus 기능들
// =============================================================================

/**
 * @brief 연속된 레지스터를 효율적으로 읽기
 * @param slave_id 슬레이브 ID
 * @param start_addr 시작 주소
 * @param count 읽을 레지스터 수
 * @param values 읽은 값들
 * @param function_code 사용할 함수 코드
 * @return 성공 시 true
 */
bool ModbusDriver::ReadHoldingRegistersBulk(int slave_id, int start_addr, int count, 
                                           std::vector<uint16_t>& values,
                                           uint8_t function_code) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    // Modbus 프로토콜 제한: 한 번에 최대 125개 레지스터
    const int max_registers_per_request = 125;
    values.clear();
    values.reserve(count);
    
    bool overall_success = true;
    auto operation_start = steady_clock::now();
    
    for (int i = 0; i < count; i += max_registers_per_request) {
        int remaining = count - i;
        int read_count = std::min(remaining, max_registers_per_request);
        
        std::vector<uint16_t> batch_values;
        bool batch_success = false;
        
        switch (function_code) {
            case 3:  // Read Holding Registers
                batch_success = ReadHoldingRegisters(slave_id, start_addr + i, read_count, batch_values);
                break;
            case 4:  // Read Input Registers  
                batch_success = ReadInputRegisters(slave_id, start_addr + i, read_count, batch_values);
                break;
            default:
                SetError(ErrorCode::INVALID_PARAMETER, "Unsupported function code: " + std::to_string(function_code));
                return false;
        }
        
        if (batch_success) {
            values.insert(values.end(), batch_values.begin(), batch_values.end());
        } else {
            // 배치 실패 시 개별 읽기 시도
            logger_->Warn("Batch read failed, trying individual reads for addresses " + 
                         std::to_string(start_addr + i) + "-" + 
                         std::to_string(start_addr + i + read_count - 1),
                         DriverLogCategory::ERROR_HANDLING);
            
            for (int j = 0; j < read_count; ++j) {
                std::vector<uint16_t> single_value;
                bool single_success = (function_code == 3) ?
                    ReadHoldingRegisters(slave_id, start_addr + i + j, 1, single_value) :
                    ReadInputRegisters(slave_id, start_addr + i + j, 1, single_value);
                
                if (single_success && !single_value.empty()) {
                    values.push_back(single_value[0]);
                } else {
                    values.push_back(0);  // 실패한 값은 0으로
                    overall_success = false;
                }
            }
        }
    }
    
    auto operation_end = steady_clock::now();
    auto duration_ms = duration_cast<milliseconds>(operation_end - operation_start).count();
    
    UpdateStatistics(overall_success, static_cast<double>(duration_ms));
    
    logger_->LogModbusOperation(slave_id, start_addr, 
                               "Bulk read " + std::to_string(count) + " registers", 
                               overall_success, duration_ms);
    
    return overall_success;
}

/**
 * @brief 진단 레지스터 읽기 (Modbus 함수 08)
 * @param slave_id 슬레이브 ID
 * @param sub_function 서브 함수 코드
 * @param data 진단 데이터
 * @return 성공 시 true
 */
bool ModbusDriver::ReadDiagnostics(int slave_id, uint16_t sub_function, 
                                  std::vector<uint16_t>& data) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    // libmodbus에서 진단 함수를 직접 지원하지 않으므로 raw 요청 구성
    uint8_t raw_req[6];
    raw_req[0] = slave_id;
    raw_req[1] = 0x08;  // Diagnostics function
    raw_req[2] = (sub_function >> 8) & 0xFF;
    raw_req[3] = sub_function & 0xFF;
    raw_req[4] = 0x00;  // Data high (일반적으로 0)
    raw_req[5] = 0x00;  // Data low
    
    modbus_set_slave(modbus_ctx_, slave_id);
    
    uint8_t rsp[256];
    int rsp_length = modbus_send_raw_request(modbus_ctx_, raw_req, 6);
    
    if (rsp_length > 0) {
        rsp_length = modbus_receive_confirmation(modbus_ctx_, rsp);
        
        if (rsp_length >= 4 && rsp[1] == 0x08) {
            // 성공적인 응답
            data.clear();
            data.push_back((rsp[2] << 8) | rsp[3]);  // 서브 함수 에코
            if (rsp_length > 4) {
                data.push_back((rsp[4] << 8) | rsp[5]);  // 데이터
            }
            
            logger_->LogModbusOperation(slave_id, sub_function, 
                                       "Diagnostics sub-function", true, 0);
            return true;
        }
    }
    
    SetError(ErrorCode::PROTOCOL_ERROR, "Diagnostics function failed");
    logger_->LogModbusOperation(slave_id, sub_function, 
                               "Diagnostics sub-function", false, 0);
    return false;
}

// =============================================================================
// 추가 유틸리티 메소드들
// =============================================================================

/**
 * @brief 연결 품질 테스트
 * @return 연결 품질 점수 (0-100)
 */
int ModbusDriver::TestConnectionQuality() {
    if (!IsConnected()) {
        return 0;
    }
    
    const int test_count = 10;
    int successful_tests = 0;
    double total_response_time = 0.0;
    
    logger_->Info("Starting connection quality test (" + std::to_string(test_count) + " iterations)",
                  DriverLogCategory::DIAGNOSTICS);
    
    for (int i = 0; i < test_count; ++i) {
        auto start_time = steady_clock::now();
        
        std::vector<uint16_t> test_values;
        bool success = ReadInputRegisters(1, 0, 1, test_values);  // 간단한 테스트 읽기
        
        auto end_time = steady_clock::now();
        auto response_time = duration_cast<microseconds>(end_time - start_time).count() / 1000.0;
        
        if (success) {
            successful_tests++;
            total_response_time += response_time;
        }
        
        // 테스트 간 짧은 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    int quality_score = (successful_tests * 100) / test_count;
    double avg_response_time = successful_tests > 0 ? total_response_time / successful_tests : 0.0;
    
    logger_->Info("Connection quality test completed - Score: " + 
                 std::to_string(quality_score) + "%, Avg response: " + 
                 std::to_string(avg_response_time) + "ms",
                 DriverLogCategory::DIAGNOSTICS);
    
    return quality_score;
}

/**
 * @brief 디바이스 정보 읽기 (Vendor, Product 등)
 * @param slave_id 슬레이브 ID  
 * @param device_info 디바이스 정보 맵
 * @return 성공 시 true
 */
bool ModbusDriver::ReadDeviceInfo(int slave_id, std::map<std::string, std::string>& device_info) {
    device_info.clear();
    
    // 표준 디바이스 식별 레지스터들 시도
    std::vector<uint16_t> vendor_id;
    if (ReadInputRegisters(slave_id, 0, 1, vendor_id) && !vendor_id.empty()) {
        device_info["vendor_id"] = std::to_string(vendor_id[0]);
    }
    
    std::vector<uint16_t> product_code;
    if (ReadInputRegisters(slave_id, 1, 1, product_code) && !product_code.empty()) {
        device_info["product_code"] = std::to_string(product_code[0]);
    }
    
    std::vector<uint16_t> version;
    if (ReadInputRegisters(slave_id, 2, 1, version) && !version.empty()) {
        device_info["version"] = std::to_string(version[0]);
    }
    
    // 기본 정보
    device_info["slave_id"] = std::to_string(slave_id);
    device_info["protocol"] = "Modbus";
    device_info["last_scan"] = std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    
    return !device_info.empty();
}

// =============================================================================
// 정리 및 소멸자 완성
// =============================================================================

void ModbusDriver::CleanupResources() {
    logger_->Info("Cleaning up Modbus driver resources", DriverLogCategory::GENERAL);
    
    // 연결 정리
    CloseConnection();
    
    // 스레드 정리
    stop_watchdog_ = true;
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
    
    // 상태 초기화
    is_connected_ = false;
    status_ = DriverStatus::DISCONNECTED;
    
    logger_->Info("Modbus driver cleanup completed", DriverLogCategory::GENERAL);
}

// =============================================================================
// 드라이버 자동 등록 (완성)
// =============================================================================

// Modbus TCP 드라이버 등록
REGISTER_DRIVER(ProtocolType::MODBUS_TCP, ModbusDriver);

// Modbus RTU 드라이버 등록  
REGISTER_DRIVER(ProtocolType::MODBUS_RTU, ModbusDriver);

// Modbus ASCII 드라이버 등록 (동일한 구현체 사용)
REGISTER_DRIVER(ProtocolType::MODBUS_ASCII, ModbusDriver);