/**
 * @file ModbusRtuWorker.cpp
 * @brief Modbus RTU 워커 클래스 구현
 * @details SerialBasedWorker 기반의 Modbus RTU 프로토콜 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <thread>

using namespace PulseOne::Workers::Protocol;
using namespace PulseOne::Drivers;
using namespace std::chrono;

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(
    const Drivers::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : SerialBasedWorker(device_info, redis_client, influx_client)
    , modbus_driver_(nullptr)
    , default_slave_id_(1)
    , response_timeout_ms_(1000)
    , byte_timeout_ms_(100)
    , inter_frame_delay_ms_(50)
    , next_group_id_(1)
    , stop_workers_(false)
    , total_reads_(0)
    , successful_reads_(0)
    , total_writes_(0)
    , successful_writes_(0)
    , crc_errors_(0)
    , timeout_errors_(0)
    , frame_errors_(0) {
    
    // ModbusDriver 생성
    modbus_driver_ = std::make_unique<Drivers::ModbusDriver>();
    
    // 기본 시리얼 버스 설정
    ConfigureSerialBus(SerialBusConfig());
    
    // DeviceInfo에서 시리얼 포트 정보 파싱
    if (!device_info.endpoint.empty()) {
        serial_bus_config_.port_name = device_info.endpoint;
        
        // endpoint에서 설정 파라미터 파싱 (예: "/dev/ttyUSB0:9600:8:N:1")
        size_t colon_pos = device_info.endpoint.find(':');
        if (colon_pos != std::string::npos) {
            std::string params = device_info.endpoint.substr(colon_pos + 1);
            std::istringstream iss(params);
            std::string token;
            
            // 보드레이트
            if (std::getline(iss, token, ':')) {
                serial_bus_config_.baud_rate = std::stoi(token);
            }
            // 데이터 비트
            if (std::getline(iss, token, ':')) {
                serial_bus_config_.data_bits = std::stoi(token);
            }
            // 패리티
            if (std::getline(iss, token, ':')) {
                serial_bus_config_.parity = token[0];
            }
            // 스톱 비트
            if (std::getline(iss, token, ':')) {
                serial_bus_config_.stop_bits = std::stoi(token);
            }
        }
    }
    
    LogRtuMessage(LogLevel::INFO, "ModbusRtuWorker created for port: " + serial_bus_config_.port_name);
}

ModbusRtuWorker::~ModbusRtuWorker() {
    // 스레드 정리
    stop_workers_ = true;
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    LogRtuMessage(LogLevel::INFO, "ModbusRtuWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusRtuWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogRtuMessage(LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogRtuMessage(LogLevel::INFO, "Starting ModbusRtuWorker...");
        
        try {
            // 기본 연결 설정
            if (!EstablishConnection()) {
                LogRtuMessage(LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            // 상태 변경
            ChangeState(WorkerState::RUNNING);
            
            // 폴링 스레드 시작
            stop_workers_ = false;
            polling_thread_ = std::thread(&ModbusRtuWorker::PollingWorkerThread, this);
            
            LogRtuMessage(LogLevel::INFO, "ModbusRtuWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogRtuMessage(LogLevel::ERROR, "Start failed: " + std::string(e.what()));
            ChangeState(WorkerState::ERROR);
            return false;
        }
    });
}

std::future<bool> ModbusRtuWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::STOPPED) {
            LogRtuMessage(LogLevel::WARN, "Worker already stopped");
            return true;
        }
        
        LogRtuMessage(LogLevel::INFO, "Stopping ModbusRtuWorker...");
        
        try {
            // 스레드 중지
            stop_workers_ = true;
            if (polling_thread_.joinable()) {
                polling_thread_.join();
            }
            
            // 연결 해제
            CloseConnection();
            
            // 상태 변경
            ChangeState(WorkerState::STOPPED);
            
            LogRtuMessage(LogLevel::INFO, "ModbusRtuWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogRtuMessage(LogLevel::ERROR, "Stop failed: " + std::string(e.what()));
            return false;
        }
    });
}

WorkerState ModbusRtuWorker::GetState() const {
    return BaseDeviceWorker::GetState();
}

// =============================================================================
// SerialBasedWorker 인터페이스 구현
// =============================================================================

bool ModbusRtuWorker::EstablishProtocolConnection() {
    LogRtuMessage(LogLevel::INFO, "Establishing Modbus RTU protocol connection");
    
    if (!modbus_driver_) {
        LogRtuMessage(LogLevel::ERROR, "ModbusDriver not initialized");
        return false;
    }
    
    try {
        // ModbusDriver 설정 (기본 멤버만 사용)
        DriverConfig config;
        config.protocol_type = ProtocolType::MODBUS_RTU;
        config.endpoint = serial_bus_config_.port_name;  // 시리얼 포트 경로
        config.timeout_ms = response_timeout_ms_;
        config.retry_count = 3;  // 기본 재시도 횟수
        
        // RTU 전용 설정은 ModbusDriver가 내부적으로 endpoint를 파싱하여 처리
        // 또는 별도의 방법으로 전달 (예: 환경 변수, 전역 설정 등)
        
        // 드라이버 초기화
        if (!modbus_driver_->Initialize(config)) {
            LogRtuMessage(LogLevel::ERROR, "Failed to initialize ModbusDriver");
            return false;
        }
        
        // 연결 수립
        if (!modbus_driver_->Connect()) {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, "Failed to connect: " + error.message);
            return false;
        }
        
        LogRtuMessage(LogLevel::INFO, "Modbus RTU protocol connection established");
        return true;
        
    } catch (const std::exception& e) {
        LogRtuMessage(LogLevel::ERROR, "EstablishProtocolConnection failed: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    LogRtuMessage(LogLevel::INFO, "Closing Modbus RTU protocol connection");
    
    if (modbus_driver_) {
        modbus_driver_->Disconnect();
        LogRtuMessage(LogLevel::INFO, "Modbus RTU protocol connection closed");
    }
    
    return true;
}

bool ModbusRtuWorker::CheckProtocolConnection() {
    if (!modbus_driver_) {
        return false;
    }
    
    return modbus_driver_->IsConnected();
}

bool ModbusRtuWorker::SendProtocolKeepAlive() {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        return false;
    }
    
    // 등록된 슬레이브들에게 Keep-alive 전송
    std::shared_lock<std::shared_mutex> slaves_lock(slaves_mutex_);
    bool any_response = false;
    
    for (auto& [slave_id, slave_info] : slaves_) {
        if (slave_info->is_online) {
            int response_time = CheckSlaveStatus(slave_id);
            if (response_time >= 0) {
                any_response = true;
                UpdateSlaveStatus(slave_id, response_time, true);
            } else {
                slave_info->is_online = false;
                LogRtuMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + " is offline");
            }
        }
    }
    
    return any_response;
}

// =============================================================================
// 설정 관리
// =============================================================================

void ModbusRtuWorker::ConfigureModbusRtu(int default_slave_id,
                                         int response_timeout_ms,
                                         int byte_timeout_ms,
                                         int inter_frame_delay_ms) {
    default_slave_id_ = default_slave_id;
    response_timeout_ms_ = response_timeout_ms;
    byte_timeout_ms_ = byte_timeout_ms;
    inter_frame_delay_ms_ = inter_frame_delay_ms;
    
    LogRtuMessage(LogLevel::INFO, 
        "Modbus RTU configured - Slave ID: " + std::to_string(default_slave_id) +
        ", Response timeout: " + std::to_string(response_timeout_ms) + "ms" +
        ", Byte timeout: " + std::to_string(byte_timeout_ms) + "ms" +
        ", Inter-frame delay: " + std::to_string(inter_frame_delay_ms) + "ms");
}

void ModbusRtuWorker::ConfigureSerialBus(const SerialBusConfig& config) {
    serial_bus_config_ = config;
    
    LogRtuMessage(LogLevel::INFO, 
        "Serial bus configured - Port: " + config.port_name +
        ", Baud: " + std::to_string(config.baud_rate) +
        ", Data bits: " + std::to_string(config.data_bits) +
        ", Parity: " + std::string(1, config.parity) +
        ", Stop bits: " + std::to_string(config.stop_bits));
}

void ModbusRtuWorker::ConfigureReconnection(int max_retry_attempts,
                                           int retry_delay_ms,
                                           bool exponential_backoff) {
    // BaseDeviceWorker의 재연결 설정 사용 (실제 구조체에 맞춰 수정)
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = retry_delay_ms;              // retry_delay_ms 대신
    settings.max_retries_per_cycle = max_retry_attempts;      // max_retry_attempts 대신
    settings.wait_time_after_max_retries_ms = retry_delay_ms * 8;  // max_retry_delay_ms 대신
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 30;  // RTU Keep-alive
    
    UpdateReconnectionSettings(settings);
    
    LogRtuMessage(LogLevel::INFO, 
        "Reconnection configured - Max retries: " + std::to_string(max_retry_attempts) +
        ", Retry delay: " + std::to_string(retry_delay_ms) + "ms" +
        ", Exponential backoff: " + (exponential_backoff ? "enabled" : "disabled"));
}

// =============================================================================
// 슬레이브 관리
// =============================================================================

bool ModbusRtuWorker::AddSlave(int slave_id, const std::string& device_name) {
    if (slave_id < 1 || slave_id > 247) {
        LogRtuMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    if (slaves_.find(slave_id) != slaves_.end()) {
        LogRtuMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + " already exists");
        return false;
    }
    
    auto slave_info = std::make_shared<ModbusRtuSlaveInfo>(
        slave_id, 
        device_name.empty() ? "Slave_" + std::to_string(slave_id) : device_name
    );
    
    slaves_[slave_id] = slave_info;
    
    LogRtuMessage(LogLevel::INFO, 
        "Added slave " + std::to_string(slave_id) + 
        " (" + slave_info->device_name + ")");
    
    return true;
}

bool ModbusRtuWorker::RemoveSlave(int slave_id) {
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it == slaves_.end()) {
        LogRtuMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + " not found");
        return false;
    }
    
    slaves_.erase(it);
    LogRtuMessage(LogLevel::INFO, "Removed slave " + std::to_string(slave_id));
    
    return true;
}

std::shared_ptr<ModbusRtuSlaveInfo> ModbusRtuWorker::GetSlaveInfo(int slave_id) const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    return (it != slaves_.end()) ? it->second : nullptr;
}

int ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int timeout_ms) {
    LogRtuMessage(LogLevel::INFO, 
        "Scanning slaves from " + std::to_string(start_id) + 
        " to " + std::to_string(end_id));
    
    int found_count = 0;
    int original_timeout = response_timeout_ms_;
    response_timeout_ms_ = timeout_ms;
    
    for (int slave_id = start_id; slave_id <= end_id; ++slave_id) {
        int response_time = CheckSlaveStatus(slave_id);
        if (response_time >= 0) {
            AddSlave(slave_id);
            UpdateSlaveStatus(slave_id, response_time, true);
            found_count++;
            
            LogRtuMessage(LogLevel::INFO, 
                "Found slave " + std::to_string(slave_id) + 
                " (response time: " + std::to_string(response_time) + "ms)");
        }
        
        // 스캔 간 지연
        std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms_));
    }
    
    response_timeout_ms_ = original_timeout;
    
    LogRtuMessage(LogLevel::INFO, 
        "Slave scan completed. Found " + std::to_string(found_count) + " slaves");
    
    return found_count;
}

// =============================================================================
// 폴링 그룹 관리
// =============================================================================

uint32_t ModbusRtuWorker::AddPollingGroup(const std::string& group_name,
                                         int slave_id,
                                         ModbusRegisterType register_type,
                                         uint16_t start_address,
                                         uint16_t register_count,
                                         int polling_interval_ms) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    uint32_t group_id = next_group_id_++;
    
    ModbusRtuPollingGroup group;
    group.group_id = group_id;
    group.group_name = group_name;
    group.slave_id = slave_id;
    group.register_type = register_type;
    group.start_address = start_address;
    group.register_count = register_count;
    group.polling_interval_ms = polling_interval_ms;
    group.enabled = true;
    group.last_poll_time = system_clock::now();
    group.next_poll_time = system_clock::now();
    
    polling_groups_[group_id] = group;
    
    LogRtuMessage(LogLevel::INFO, 
        "Added polling group " + std::to_string(group_id) + 
        " (" + group_name + ") for slave " + std::to_string(slave_id));
    
    return group_id;
}

bool ModbusRtuWorker::RemovePollingGroup(uint32_t group_id) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    polling_groups_.erase(it);
    LogRtuMessage(LogLevel::INFO, "Removed polling group " + std::to_string(group_id));
    
    return true;
}

bool ModbusRtuWorker::EnablePollingGroup(uint32_t group_id, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    LogRtuMessage(LogLevel::INFO, 
        "Polling group " + std::to_string(group_id) + 
        (enabled ? " enabled" : " disabled"));
    
    return true;
}

bool ModbusRtuWorker::AddDataPointToGroup(uint32_t group_id, const Drivers::DataPoint& data_point) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.data_points.push_back(data_point);
    LogRtuMessage(LogLevel::INFO, 
        "Added data point " + data_point.name + 
        " to polling group " + std::to_string(group_id));
    
    return true;
}

// =============================================================================
// 데이터 읽기/쓰기 (ModbusDriver 사용)
// =============================================================================

bool ModbusRtuWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                                          uint16_t register_count, std::vector<uint16_t>& values) {
    LockBus();
    
    try {
        // DataPoint 벡터 생성
        std::vector<DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::HOLDING_REGISTER, start_address, register_count);
        
        // ModbusDriver를 통한 읽기
        std::vector<TimestampedValue> timestamped_values;
        bool success = modbus_driver_->ReadValues(data_points, timestamped_values);
        
        if (success) {
            // 결과 변환 (std::variant를 uint16_t로 안전하게 변환)
            values.clear();
            values.reserve(timestamped_values.size());
            for (const auto& tv : timestamped_values) {
                uint16_t uint16_val = 0;
                std::visit([&uint16_val](const auto& val) {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, uint16_t>) {
                        uint16_val = val;
                    } else if constexpr (std::is_same_v<T, std::monostate>) {
                        uint16_val = 0;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        uint16_val = static_cast<uint16_t>(val);
                    } else {
                        uint16_val = 0;
                    }
                }, tv.value);
                values.push_back(uint16_val);
            }
            
            UpdateRtuStats("read", true);
            UpdateSlaveStatus(slave_id, 0, true);  // 응답 시간은 드라이버에서 측정
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to read holding registers from slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("read", false, "read_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "ReadHoldingRegisters exception: " + std::string(e.what()));
        UpdateRtuStats("read", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::ReadInputRegisters(int slave_id, uint16_t start_address, 
                                        uint16_t register_count, std::vector<uint16_t>& values) {
    LockBus();
    
    try {
        // DataPoint 벡터 생성
        std::vector<DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::INPUT_REGISTER, start_address, register_count);
        
        // ModbusDriver를 통한 읽기
        std::vector<TimestampedValue> timestamped_values;
        bool success = modbus_driver_->ReadValues(data_points, timestamped_values);
        
        if (success) {
            // 결과 변환 (std::variant를 uint16_t로 안전하게 변환)
            values.clear();
            values.reserve(timestamped_values.size());
            for (const auto& tv : timestamped_values) {
                uint16_t uint16_val = 0;
                std::visit([&uint16_val](const auto& val) {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, uint16_t>) {
                        uint16_val = val;
                    } else if constexpr (std::is_same_v<T, std::monostate>) {
                        uint16_val = 0;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        uint16_val = static_cast<uint16_t>(val);
                    } else {
                        uint16_val = 0;
                    }
                }, tv.value);
                values.push_back(uint16_val);
            }
            
            UpdateRtuStats("read", true);
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to read input registers from slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("read", false, "read_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "ReadInputRegisters exception: " + std::string(e.what()));
        UpdateRtuStats("read", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::ReadCoils(int slave_id, uint16_t start_address, 
                               uint16_t coil_count, std::vector<bool>& values) {
    LockBus();
    
    try {
        // DataPoint 벡터 생성
        std::vector<DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::COIL, start_address, coil_count);
        
        // ModbusDriver를 통한 읽기
        std::vector<TimestampedValue> timestamped_values;
        bool success = modbus_driver_->ReadValues(data_points, timestamped_values);
        
        if (success) {
            // 결과 변환 (std::variant 안전한 접근)
            values.clear();
            values.reserve(timestamped_values.size());
            for (const auto& tv : timestamped_values) {
                // std::variant 값을 안전하게 bool로 변환
                bool bool_val = false;
                std::visit([&bool_val](const auto& val) {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        bool_val = val;
                    } else if constexpr (std::is_same_v<T, std::monostate>) {
                        bool_val = false;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        bool_val = (val != static_cast<T>(0));
                    } else {
                        bool_val = false;
                    }
                }, tv.value);
                values.push_back(bool_val);
            }
            
            UpdateRtuStats("read", true);
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to read coils from slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("read", false, "read_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "ReadCoils exception: " + std::string(e.what()));
        UpdateRtuStats("read", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                                        uint16_t input_count, std::vector<bool>& values) {
    LockBus();
    
    try {
        // DataPoint 벡터 생성
        std::vector<DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::DISCRETE_INPUT, start_address, input_count);
        
        // ModbusDriver를 통한 읽기
        std::vector<TimestampedValue> timestamped_values;
        bool success = modbus_driver_->ReadValues(data_points, timestamped_values);
        
        if (success) {
            // 결과 변환 (std::variant 안전한 접근)
            values.clear();
            values.reserve(timestamped_values.size());
            for (const auto& tv : timestamped_values) {
                // std::variant 값을 안전하게 bool로 변환
                bool bool_val = false;
                std::visit([&bool_val](const auto& val) {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        bool_val = val;
                    } else if constexpr (std::is_same_v<T, std::monostate>) {
                        bool_val = false;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        bool_val = (val != static_cast<T>(0));
                    } else {
                        bool_val = false;
                    }
                }, tv.value);
                values.push_back(bool_val);
            }
            
            UpdateRtuStats("read", true);
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to read discrete inputs from slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("read", false, "read_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "ReadDiscreteInputs exception: " + std::string(e.what()));
        UpdateRtuStats("read", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::WriteSingleRegister(int slave_id, uint16_t address, uint16_t value) {
    LockBus();
    
    try {
        // DataPoint 생성
        DataPoint data_point;
        data_point.address = address;
        data_point.data_type = DataType::UINT16;
        data_point.name = "RTU_" + std::to_string(slave_id) + "_" + std::to_string(address);
        
        // DataValue 생성 (std::variant로 직접 할당)
        DataValue data_value = static_cast<uint16_t>(value);
        
        // ModbusDriver를 통한 쓰기
        bool success = modbus_driver_->WriteValue(data_point, data_value);
        
        if (success) {
            UpdateRtuStats("write", true);
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to write single register to slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("write", false, "write_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "WriteSingleRegister exception: " + std::string(e.what()));
        UpdateRtuStats("write", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::WriteSingleCoil(int slave_id, uint16_t address, bool value) {
    LockBus();
    
    try {
        // DataPoint 생성
        DataPoint data_point;
        data_point.address = address;
        data_point.data_type = DataType::BOOL;
        data_point.name = "RTU_" + std::to_string(slave_id) + "_" + std::to_string(address);
        
        // DataValue 생성 (std::variant로 직접 할당)
        DataValue data_value = value;
        
        // ModbusDriver를 통한 쓰기
        bool success = modbus_driver_->WriteValue(data_point, data_value);
        
        if (success) {
            UpdateRtuStats("write", true);
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            ErrorInfo error = modbus_driver_->GetLastError();
            LogRtuMessage(LogLevel::ERROR, 
                "Failed to write single coil to slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("write", false, "write_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(LogLevel::ERROR, "WriteSingleCoil exception: " + std::string(e.what()));
        UpdateRtuStats("write", false, "exception");
        return false;
    }
}

bool ModbusRtuWorker::WriteMultipleRegisters(int slave_id, uint16_t start_address, 
                                            const std::vector<uint16_t>& values) {
    // 단일 레지스터 쓰기를 반복 (ModbusDriver가 다중 쓰기를 지원하지 않는 경우)
    for (size_t i = 0; i < values.size(); ++i) {
        if (!WriteSingleRegister(slave_id, start_address + i, values[i])) {
            return false;
        }
    }
    return true;
}

bool ModbusRtuWorker::WriteMultipleCoils(int slave_id, uint16_t start_address, 
                                        const std::vector<bool>& values) {
    // 단일 코일 쓰기를 반복 (ModbusDriver가 다중 쓰기를 지원하지 않는 경우)
    for (size_t i = 0; i < values.size(); ++i) {
        if (!WriteSingleCoil(slave_id, start_address + i, values[i])) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// 상태 및 통계 조회
// =============================================================================

std::string ModbusRtuWorker::GetModbusRtuStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"protocol\": \"Modbus RTU\",\n";
    ss << "  \"serial_port\": \"" << serial_bus_config_.port_name << "\",\n";
    ss << "  \"baud_rate\": " << serial_bus_config_.baud_rate << ",\n";
    ss << "  \"total_reads\": " << total_reads_ << ",\n";
    ss << "  \"successful_reads\": " << successful_reads_ << ",\n";
    ss << "  \"total_writes\": " << total_writes_ << ",\n";
    ss << "  \"successful_writes\": " << successful_writes_ << ",\n";
    ss << "  \"crc_errors\": " << crc_errors_ << ",\n";
    ss << "  \"timeout_errors\": " << timeout_errors_ << ",\n";
    ss << "  \"frame_errors\": " << frame_errors_ << ",\n";
    
    // 성공률 계산
    double read_success_rate = total_reads_ > 0 ? 
        (static_cast<double>(successful_reads_) / total_reads_ * 100.0) : 0.0;
    double write_success_rate = total_writes_ > 0 ? 
        (static_cast<double>(successful_writes_) / total_writes_ * 100.0) : 0.0;
    
    ss << "  \"read_success_rate_percent\": " << std::fixed << std::setprecision(2) << read_success_rate << ",\n";
    ss << "  \"write_success_rate_percent\": " << std::fixed << std::setprecision(2) << write_success_rate << "\n";
    ss << "}";
    
    return ss.str();
}

std::string ModbusRtuWorker::GetSerialBusStatus() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"port_name\": \"" << serial_bus_config_.port_name << "\",\n";
    ss << "  \"baud_rate\": " << serial_bus_config_.baud_rate << ",\n";
    ss << "  \"data_bits\": " << serial_bus_config_.data_bits << ",\n";
    ss << "  \"parity\": \"" << serial_bus_config_.parity << "\",\n";
    ss << "  \"stop_bits\": " << serial_bus_config_.stop_bits << ",\n";
    ss << "  \"response_timeout_ms\": " << response_timeout_ms_ << ",\n";
    ss << "  \"byte_timeout_ms\": " << byte_timeout_ms_ << ",\n";
    ss << "  \"inter_frame_delay_ms\": " << inter_frame_delay_ms_ << ",\n";
    ss << "  \"is_connected\": " << (const_cast<ModbusRtuWorker*>(this)->CheckProtocolConnection() ? "true" : "false") << "\n";
    ss << "}";
    
    return ss.str();
}

std::string ModbusRtuWorker::GetSlaveStatusList() const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"slaves\": [\n";
    
    bool first = true;
    for (const auto& [slave_id, slave_info] : slaves_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"slave_id\": " << slave_id << ",\n";
        ss << "      \"device_name\": \"" << slave_info->device_name << "\",\n";
        ss << "      \"is_online\": " << (slave_info->is_online ? "true" : "false") << ",\n";
        ss << "      \"response_time_ms\": " << slave_info->response_time_ms.load() << ",\n";
        ss << "      \"total_requests\": " << slave_info->total_requests.load() << ",\n";
        ss << "      \"successful_requests\": " << slave_info->successful_requests.load() << ",\n";
        ss << "      \"crc_errors\": " << slave_info->crc_errors.load() << ",\n";
        ss << "      \"timeout_errors\": " << slave_info->timeout_errors.load() << ",\n";
        
        // 성공률 계산
        uint64_t total = slave_info->total_requests.load();
        uint64_t successful = slave_info->successful_requests.load();
        double success_rate = total > 0 ? (static_cast<double>(successful) / total * 100.0) : 0.0;
        
        ss << "      \"success_rate_percent\": " << std::fixed << std::setprecision(2) << success_rate << ",\n";
        ss << "      \"last_error\": \"" << slave_info->last_error << "\"\n";
        ss << "    }";
    }
    
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

std::string ModbusRtuWorker::GetPollingGroupStatus() const {
    std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"polling_groups\": [\n";
    
    bool first = true;
    for (const auto& [group_id, group] : polling_groups_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"group_id\": " << group_id << ",\n";
        ss << "      \"group_name\": \"" << group.group_name << "\",\n";
        ss << "      \"slave_id\": " << group.slave_id << ",\n";
        ss << "      \"register_type\": " << static_cast<int>(group.register_type) << ",\n";
        ss << "      \"start_address\": " << group.start_address << ",\n";
        ss << "      \"register_count\": " << group.register_count << ",\n";
        ss << "      \"polling_interval_ms\": " << group.polling_interval_ms << ",\n";
        ss << "      \"enabled\": " << (group.enabled ? "true" : "false") << ",\n";
        ss << "      \"data_points_count\": " << group.data_points.size() << "\n";
        ss << "    }";
    }
    
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

void ModbusRtuWorker::PollingWorkerThread() {
    LogRtuMessage(LogLevel::INFO, "Polling worker thread started");
    
    while (!stop_workers_) {
        try {
            auto now = system_clock::now();
            
            // 폴링 그룹들 확인 및 처리
            {
                std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
                
                for (auto& [group_id, group] : polling_groups_) {
                    if (!group.enabled || stop_workers_) {
                        continue;
                    }
                    
                    // 폴링 시간 확인
                    if (now >= group.next_poll_time) {
                        // 그룹 처리 (뮤텍스 해제 후)
                        lock.~shared_lock();
                        ProcessPollingGroup(const_cast<ModbusRtuPollingGroup&>(group));
                        
                        // 다음 폴링 시간 설정
                        std::unique_lock<std::shared_mutex> write_lock(polling_groups_mutex_);
                        auto& mutable_group = polling_groups_[group_id];
                        mutable_group.last_poll_time = now;
                        mutable_group.next_poll_time = now + milliseconds(mutable_group.polling_interval_ms);
                        write_lock.unlock();
                        
                        // 새로운 shared_lock 획득
                        lock = std::shared_lock<std::shared_mutex>(polling_groups_mutex_);
                    }
                }
            }
            
            // 폴링 스레드 휴식 (100ms)
            std::this_thread::sleep_for(milliseconds(100));
            
        } catch (const std::exception& e) {
            LogRtuMessage(LogLevel::ERROR, "Polling worker thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(1));
        }
    }
    
    LogRtuMessage(LogLevel::INFO, "Polling worker thread stopped");
}

bool ModbusRtuWorker::ProcessPollingGroup(ModbusRtuPollingGroup& group) {
    LogRtuMessage(LogLevel::DEBUG, 
        "Processing polling group " + std::to_string(group.group_id) + 
        " (" + group.group_name + ")");
    
    std::vector<uint16_t> values;
    bool success = false;
    
    try {
        // 레지스터 타입에 따른 읽기
        switch (group.register_type) {
            case ModbusRegisterType::HOLDING_REGISTER:
                success = ReadHoldingRegisters(group.slave_id, group.start_address, 
                                             group.register_count, values);
                break;
                
            case ModbusRegisterType::INPUT_REGISTER:
                success = ReadInputRegisters(group.slave_id, group.start_address, 
                                           group.register_count, values);
                break;
                
            case ModbusRegisterType::COIL:
            case ModbusRegisterType::DISCRETE_INPUT:
                {
                    std::vector<bool> bool_values;
                    if (group.register_type == ModbusRegisterType::COIL) {
                        success = ReadCoils(group.slave_id, group.start_address, 
                                          group.register_count, bool_values);
                    } else {
                        success = ReadDiscreteInputs(group.slave_id, group.start_address, 
                                                   group.register_count, bool_values);
                    }
                    
                    // bool 값을 uint16_t로 변환 (std::variant 안전한 접근)
                    values.reserve(bool_values.size());
                    for (const bool& val : bool_values) {
                        values.push_back(val ? 1 : 0);
                    }
                }
                break;
                
            default:
                LogRtuMessage(LogLevel::ERROR, 
                    "Unknown register type in group " + std::to_string(group.group_id));
                return false;
        }
        
        if (success && !values.empty()) {
            // 데이터 포인트들에 값 할당 및 저장
            for (size_t i = 0; i < group.data_points.size() && i < values.size(); ++i) {
                auto& data_point = group.data_points[i];
                
                // TimestampedValue 생성하여 저장
                TimestampedValue timestamped_value;
                timestamped_value.value = static_cast<double>(values[i]);
                timestamped_value.quality = DataQuality::GOOD;
                timestamped_value.timestamp = system_clock::now();
                
                // Redis/InfluxDB에 데이터 저장 (BaseDeviceWorker 메서드 사용)
                SaveToInfluxDB(data_point.id, timestamped_value);
            }
            
            LogRtuMessage(LogLevel::DEBUG, 
                "Successfully processed group " + std::to_string(group.group_id) + 
                ", read " + std::to_string(values.size()) + " values");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogRtuMessage(LogLevel::ERROR, 
            "ProcessPollingGroup exception for group " + std::to_string(group.group_id) + 
            ": " + std::string(e.what()));
        return false;
    }
}

void ModbusRtuWorker::UpdateSlaveStatus(int slave_id, int response_time_ms, bool success) {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        auto& slave_info = it->second;
        
        slave_info->total_requests++;
        if (success) {
            slave_info->successful_requests++;
            slave_info->is_online = true;
            slave_info->last_response = system_clock::now();
            
            if (response_time_ms > 0) {
                // 이동 평균으로 응답 시간 업데이트
                uint32_t current_avg = slave_info->response_time_ms.load();
                uint32_t new_avg = (current_avg * 7 + response_time_ms) / 8;  // 8-점 이동 평균
                slave_info->response_time_ms = new_avg;
            }
            
            slave_info->last_error.clear();
        } else {
            slave_info->is_online = false;
            
            // 에러 타입별 카운트 (에러 정보가 있다면)
            if (modbus_driver_) {
                ErrorInfo error = modbus_driver_->GetLastError();
                slave_info->last_error = error.message;
                
                // 에러 타입에 따른 카운터 증가
                if (error.message.find("CRC") != std::string::npos) {
                    slave_info->crc_errors++;
                } else if (error.message.find("timeout") != std::string::npos || 
                          error.message.find("Timeout") != std::string::npos) {
                    slave_info->timeout_errors++;
                }
            }
        }
    }
}

int ModbusRtuWorker::CheckSlaveStatus(int slave_id) {
    auto start_time = steady_clock::now();
    
    try {
        // 간단한 읽기 테스트 (홀딩 레지스터 0번 주소 1개)
        std::vector<uint16_t> test_values;
        bool success = ReadHoldingRegisters(slave_id, 0, 1, test_values);
        
        auto end_time = steady_clock::now();
        int response_time = duration_cast<milliseconds>(end_time - start_time).count();
        
        return success ? response_time : -1;
        
    } catch (const std::exception& e) {
        LogRtuMessage(LogLevel::DEBUG, 
            "CheckSlaveStatus exception for slave " + std::to_string(slave_id) + 
            ": " + std::string(e.what()));
        return -1;
    }
}

void ModbusRtuWorker::UpdateRtuStats(const std::string& operation, bool success, 
                                     const std::string& error_type) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (operation == "read") {
        total_reads_++;
        if (success) {
            successful_reads_++;
        }
    } else if (operation == "write") {
        total_writes_++;
        if (success) {
            successful_writes_++;
        }
    }
    
    if (!success && !error_type.empty()) {
        if (error_type == "crc") {
            crc_errors_++;
        } else if (error_type == "timeout") {
            timeout_errors_++;
        } else if (error_type == "frame") {
            frame_errors_++;
        }
    }
}

void ModbusRtuWorker::LockBus() {
    bus_mutex_.lock();
    
    // 프레임 간 지연 적용 (이전 트랜잭션과의 간격 보장)
    if (inter_frame_delay_ms_ > 0) {
        std::this_thread::sleep_for(milliseconds(inter_frame_delay_ms_));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

void ModbusRtuWorker::LogRtuMessage(LogLevel level, const std::string& message) {
    std::string prefix = "[ModbusRTU:" + serial_bus_config_.port_name + "] ";
    LogMessage(level, prefix + message);
}

std::vector<Drivers::DataPoint> ModbusRtuWorker::CreateDataPoints(int slave_id, 
                                                                 ModbusRegisterType register_type,
                                                                 uint16_t start_address, 
                                                                 uint16_t count) {
    std::vector<DataPoint> data_points;
    data_points.reserve(count);
    
    for (uint16_t i = 0; i < count; ++i) {
        DataPoint point;
        point.address = start_address + i;
        point.name = "RTU_" + std::to_string(slave_id) + "_" + std::to_string(start_address + i);
        
        // 레지스터 타입에 따른 데이터 타입 설정
        switch (register_type) {
            case ModbusRegisterType::COIL:
            case ModbusRegisterType::DISCRETE_INPUT:
                point.data_type = DataType::BOOL;
                break;
            case ModbusRegisterType::HOLDING_REGISTER:
            case ModbusRegisterType::INPUT_REGISTER:
                point.data_type = DataType::UINT16;
                break;
        }
        
        data_points.push_back(point);
    }
    
    return data_points;
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne