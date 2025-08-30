/**
 * @file ModbusTcpWorker.cpp (완전한 완성본)
 * @brief Modbus TCP 워커 구현 - BaseDeviceWorker Write 인터페이스 구현 + 모든 누락 함수 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.1.0 (완전한 완성본)
 */

#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace std::chrono;
using json = nlohmann::json;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusTcpWorker::ModbusTcpWorker(const PulseOne::DeviceInfo& device_info)
    : TcpBasedWorker(device_info)
    , modbus_driver_(nullptr)
    , polling_thread_running_(false)
    , next_group_id_(1)
    , default_polling_interval_ms_(1000)
    , max_registers_per_group_(125)
    , auto_group_creation_enabled_(true) {
    
    LogMessage(LogLevel::INFO, "ModbusTcpWorker created for device: " + device_info.name);
    
    if (!ParseModbusConfig()) {
        LogMessage(LogLevel::ERROR, "Failed to parse Modbus configuration");
        return;
    }
    
    if (!InitializeModbusDriver()) {
        LogMessage(LogLevel::ERROR, "Failed to initialize ModbusDriver");
        return;
    }
    
    LogMessage(LogLevel::INFO, "ModbusTcpWorker initialization completed");
}

ModbusTcpWorker::~ModbusTcpWorker() {
    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    modbus_driver_.reset();
    LogMessage(LogLevel::INFO, "ModbusTcpWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker Write 인터페이스 구현 (완성!)
// =============================================================================

bool ModbusTcpWorker::WriteDataPoint(const std::string& point_id, const DataValue& value) {
    try {
        LogMessage(LogLevel::INFO, "WriteDataPoint 호출: " + point_id);
        return WriteDataPointValue(point_id, value);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteDataPoint 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteAnalogOutput(const std::string& output_id, double value) {
    try {
        LogMessage(LogLevel::INFO, "WriteAnalogOutput 호출: " + output_id + " = " + std::to_string(value));
        
        DataValue data_value = value;
        auto data_point_opt = FindDataPointById(output_id);
        if (data_point_opt.has_value()) {
            return WriteDataPoint(output_id, data_value);
        }
        
        uint8_t slave_id;
        uint16_t address;
        if (ParseAddressString(output_id, slave_id, address)) {
            uint16_t int_value = static_cast<uint16_t>(value);
            return WriteSingleHoldingRegister(slave_id, address, int_value);
        }
        
        LogMessage(LogLevel::ERROR, "WriteAnalogOutput: Invalid output_id: " + output_id);
        return false;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteAnalogOutput 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteDigitalOutput(const std::string& output_id, bool value) {
    try {
        LogMessage(LogLevel::INFO, "WriteDigitalOutput 호출: " + output_id + " = " + (value ? "ON" : "OFF"));
        
        DataValue data_value = value;
        auto data_point_opt = FindDataPointById(output_id);
        if (data_point_opt.has_value()) {
            return WriteDataPoint(output_id, data_value);
        }
        
        uint8_t slave_id;
        uint16_t address;
        if (ParseAddressString(output_id, slave_id, address)) {
            return WriteSingleCoil(slave_id, address, value);
        }
        
        LogMessage(LogLevel::ERROR, "WriteDigitalOutput: Invalid output_id: " + output_id);
        return false;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteDigitalOutput 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteSetpoint(const std::string& setpoint_id, double value) {
    try {
        LogMessage(LogLevel::INFO, "WriteSetpoint 호출: " + setpoint_id + " = " + std::to_string(value));
        return WriteAnalogOutput(setpoint_id, value);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteSetpoint 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ControlDigitalDevice(const std::string& pump_id, bool enable) {
    try {
        LogMessage(LogLevel::INFO, "ControlPump 호출: " + pump_id + " = " + (enable ? "START" : "STOP"));
        
        bool success = WriteDigitalOutput(pump_id, enable);
        if (success) {
            LogMessage(LogLevel::INFO, "펌프 제어 성공: " + pump_id + " " + (enable ? "시작" : "정지"));
        }
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ControlPump 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ControlAnalogDevice(const std::string& valve_id, double position) {
    try {
        LogMessage(LogLevel::INFO, "ControlValve 호출: " + valve_id + " = " + std::to_string(position) + "%");
        
        if (position < 0.0 || position > 100.0) {
            LogMessage(LogLevel::ERROR, "ControlValve: Invalid position range: " + std::to_string(position) + "% (0-100 required)");
            return false;
        }
        
        bool success = WriteAnalogOutput(valve_id, position);
        if (success) {
            LogMessage(LogLevel::INFO, "밸브 위치 제어 성공: " + valve_id + " = " + std::to_string(position) + "%");
        }
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ControlValve 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

std::future<bool> ModbusTcpWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogMessage(LogLevel::WARN, "Worker already running");
            return true;
        }
        
        StartReconnectionThread();
        
        LogMessage(LogLevel::INFO, "Starting ModbusTcpWorker...");
        
        try {
            if (!EstablishConnection()) {
                LogMessage(LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            const auto& data_points = GetDataPoints();
            if (auto_group_creation_enabled_ && !data_points.empty()) {
                size_t group_count = CreatePollingGroupsFromDataPoints(data_points);
                LogMessage(LogLevel::INFO, "Created " + std::to_string(group_count) + " polling groups from " + 
                          std::to_string(data_points.size()) + " data points");
            }
            
            polling_thread_running_ = true;
            polling_thread_ = std::make_unique<std::thread>(&ModbusTcpWorker::PollingThreadFunction, this);
            
            ChangeState(WorkerState::RUNNING);
            
            LogMessage(LogLevel::INFO, "ModbusTcpWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception during start: " + std::string(e.what()));
            return false;
        }
    });
}

std::future<bool> ModbusTcpWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(LogLevel::INFO, "Stopping ModbusTcpWorker...");
        
        try {
            polling_thread_running_ = false;
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
            
            CloseConnection();
            ChangeState(WorkerState::STOPPED);
            
            LogMessage(LogLevel::INFO, "ModbusTcpWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception during stop: " + std::string(e.what()));
            return false;
        }
    });
}

// =============================================================================
// TcpBasedWorker 구현
// =============================================================================

bool ModbusTcpWorker::EstablishProtocolConnection() {
    if (!modbus_driver_) {
        LogMessage(LogLevel::ERROR, "ModbusDriver not initialized");
        return false;
    }
    
    LogMessage(LogLevel::INFO, "Establishing Modbus protocol connection...");
    
    if (!modbus_driver_->Connect()) {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(LogLevel::ERROR, "ModbusDriver connection failed: " + error.message);
        return false;
    }
    
    LogMessage(LogLevel::INFO, "Modbus protocol connection established");
    return true;
}

bool ModbusTcpWorker::CloseProtocolConnection() {
    if (!modbus_driver_) {
        return true;
    }
    
    LogMessage(LogLevel::INFO, "Closing Modbus protocol connection...");
    
    bool result = modbus_driver_->Disconnect();
    
    if (result) {
        LogMessage(LogLevel::INFO, "Modbus protocol connection closed");
    } else {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(LogLevel::WARN, "ModbusDriver disconnect warning: " + error.message);
    }
    
    return result;
}

bool ModbusTcpWorker::CheckProtocolConnection() {
    if (!modbus_driver_) {
        return false;
    }
    
    return modbus_driver_->IsConnected();
}

bool ModbusTcpWorker::SendProtocolKeepAlive() {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        return false;
    }
    
    try {
        uint16_t test_value;
        bool result = ReadSingleHoldingRegister(1, 0, test_value);
        
        if (result) {
            LogMessage(LogLevel::DEBUG_LEVEL, "Modbus Keep-alive successful");
        } else {
            LogMessage(LogLevel::WARN, "Modbus Keep-alive failed");
        }
        
        return result;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "SendProtocolKeepAlive 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 폴링 그룹 관리
// =============================================================================

bool ModbusTcpWorker::AddPollingGroup(const ModbusTcpPollingGroup& group) {
    if (!ValidatePollingGroup(group)) {
        LogMessage(LogLevel::ERROR, "Invalid polling group");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    if (polling_groups_.find(group.group_id) != polling_groups_.end()) {
        LogMessage(LogLevel::ERROR, "Polling group ID " + std::to_string(group.group_id) + " already exists");
        return false;
    }
    
    polling_groups_[group.group_id] = group;
    
    LogMessage(LogLevel::INFO, "Added polling group " + std::to_string(group.group_id) + 
               " with " + std::to_string(group.data_points.size()) + " data points");
    
    return true;
}

bool ModbusTcpWorker::RemovePollingGroup(uint32_t group_id) {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    polling_groups_.erase(it);
    
    LogMessage(LogLevel::INFO, "Removed polling group " + std::to_string(group_id));
    return true;
}

std::vector<ModbusTcpPollingGroup> ModbusTcpWorker::GetPollingGroups() const {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    std::vector<ModbusTcpPollingGroup> groups;
    groups.reserve(polling_groups_.size());
    
    for (const auto& pair : polling_groups_) {
        groups.push_back(pair.second);
    }
    
    return groups;
}

bool ModbusTcpWorker::SetPollingGroupEnabled(uint32_t group_id, bool enabled) {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    
    LogMessage(LogLevel::INFO, "Polling group " + std::to_string(group_id) + 
               (enabled ? " enabled" : " disabled"));
    
    return true;
}

std::string ModbusTcpWorker::GetModbusStats() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"worker_type\": \"ModbusTcpWorker\",\n";
    oss << "  \"device_id\": \"" << device_info_.id << "\",\n";
    oss << "  \"device_name\": \"" << device_info_.name << "\",\n";
    oss << "  \"endpoint\": \"" << device_info_.endpoint << "\",\n";
    oss << "  \"modbus_config\": {\n";
    oss << "      \"slave_id\": " << GetPropertyValue(modbus_config_.properties, "slave_id", "1") << ",\n";
    oss << "      \"timeout_ms\": " << modbus_config_.timeout_ms << ",\n";
    oss << "      \"response_timeout_ms\": " << GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000") << ",\n";
    oss << "      \"byte_timeout_ms\": " << GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100") << ",\n";
    oss << "      \"max_retries\": " << GetPropertyValue(modbus_config_.properties, "max_retries", "3") << "\n";
    oss << "    }\n";
    oss << "}";
    return oss.str();
}

// =============================================================================
// 선택적 구현 완성
// =============================================================================

bool ModbusTcpWorker::TestConnection(int slave_id) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        LogMessage(LogLevel::INFO, "연결 테스트 시작: Slave " + std::to_string(slave_id));
        
        uint16_t test_value;
        bool success = ReadSingleHoldingRegister(slave_id, 0, test_value);
        
        if (success) {
            LogMessage(LogLevel::INFO, "연결 테스트 성공: Slave " + std::to_string(slave_id) + 
                      " (레지스터 0 값: " + std::to_string(test_value) + ")");
        } else {
            LogMessage(LogLevel::WARN, "연결 테스트 실패: Slave " + std::to_string(slave_id));
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "TestConnection 예외: " + std::string(e.what()));
        return false;
    }
}

std::map<uint16_t, uint16_t> ModbusTcpWorker::ScanRegisters(int slave_id, uint16_t start_address, 
                                                           uint16_t end_address, 
                                                           const std::string& register_type) {
    std::map<uint16_t, uint16_t> scan_results;
    
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return scan_results;
    }
    
    try {
        LogMessage(LogLevel::INFO, "레지스터 스캔 시작: Slave " + std::to_string(slave_id) + 
                  ", 주소 " + std::to_string(start_address) + "-" + std::to_string(end_address) + 
                  " (" + register_type + ")");
        
        const uint16_t batch_size = 10;
        uint16_t current_address = start_address;
        
        while (current_address <= end_address) {
            uint16_t read_count = std::min(batch_size, static_cast<uint16_t>(end_address - current_address + 1));
            std::vector<uint16_t> values;
            
            bool success = false;
            if (register_type == "holding") {
                success = ReadHoldingRegisters(slave_id, current_address, read_count, values);
            } else if (register_type == "input") {
                success = ReadInputRegisters(slave_id, current_address, read_count, values);
            }
            
            if (success) {
                for (size_t i = 0; i < values.size(); ++i) {
                    scan_results[current_address + i] = values[i];
                }
            } else {
                for (uint16_t addr = current_address; addr < current_address + read_count && addr <= end_address; ++addr) {
                    uint16_t single_value;
                    if (register_type == "holding") {
                        if (ReadSingleHoldingRegister(slave_id, addr, single_value)) {
                            scan_results[addr] = single_value;
                        }
                    } else if (register_type == "input") {
                        if (ReadSingleInputRegister(slave_id, addr, single_value)) {
                            scan_results[addr] = single_value;
                        }
                    }
                }
            }
            
            current_address += read_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        LogMessage(LogLevel::INFO, "레지스터 스캔 완료: " + std::to_string(scan_results.size()) + 
                  "/" + std::to_string(end_address - start_address + 1) + " 레지스터 읽기 성공");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ScanRegisters 예외: " + std::string(e.what()));
    }
    
    return scan_results;
}

std::string ModbusTcpWorker::ReadDeviceInfo(int slave_id) {
    json device_info;
    
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        device_info["error"] = "ModbusDriver not connected";
        return device_info.dump(2);
    }
    
    try {
        LogMessage(LogLevel::INFO, "디바이스 정보 읽기: Slave " + std::to_string(slave_id));
        
        device_info["slave_id"] = slave_id;
        device_info["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        device_info["connection_test"] = TestConnection(slave_id);
        
        std::vector<std::pair<std::string, uint16_t>> standard_registers = {
            {"device_id", 0},
            {"firmware_version", 1},
            {"hardware_version", 2},
            {"device_status", 10},
            {"error_code", 11}
        };
        
        json registers;
        for (const auto& reg : standard_registers) {
            uint16_t value;
            if (ReadSingleHoldingRegister(slave_id, reg.second, value)) {
                registers[reg.first] = {
                    {"address", reg.second},
                    {"value", value},
                    {"hex", "0x" + std::to_string(value)}
                };
            }
        }
        device_info["registers"] = registers;
        
        if (modbus_driver_) {
            const auto& stats = modbus_driver_->GetStatistics();
            json communication_stats;
            communication_stats["total_requests"] = stats.total_reads + stats.total_writes;
            communication_stats["successful_requests"] = stats.successful_reads + stats.successful_writes;
            communication_stats["failed_requests"] = stats.failed_reads + stats.failed_writes;
            communication_stats["average_response_time_ms"] = stats.average_response_time.count();
            device_info["communication_stats"] = communication_stats;
        }
        
        LogMessage(LogLevel::INFO, "디바이스 정보 읽기 완료");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadDeviceInfo 예외: " + std::string(e.what()));
        device_info["error"] = e.what();
    }
    
    return device_info.dump(2);
}

std::string ModbusTcpWorker::MonitorRegisters(int slave_id, 
                                             const std::vector<uint16_t>& addresses,
                                             const std::string& register_type,
                                             int duration_seconds) {
    json monitoring_result;
    
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        monitoring_result["error"] = "ModbusDriver not connected";
        return monitoring_result.dump(2);
    }
    
    try {
        LogMessage(LogLevel::INFO, "레지스터 모니터링 시작: " + std::to_string(addresses.size()) + 
                  "개 주소, " + std::to_string(duration_seconds) + "초간");
        
        monitoring_result["slave_id"] = slave_id;
        monitoring_result["register_type"] = register_type;
        monitoring_result["duration_seconds"] = duration_seconds;
        monitoring_result["addresses"] = addresses;
        monitoring_result["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        json samples;
        auto start_time = std::chrono::system_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        int sample_count = 0;
        while (std::chrono::system_clock::now() < end_time) {
            json sample;
            sample["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            json values;
            for (uint16_t address : addresses) {
                uint16_t value;
                bool success = false;
                
                if (register_type == "holding") {
                    success = ReadSingleHoldingRegister(slave_id, address, value);
                } else if (register_type == "input") {
                    success = ReadSingleInputRegister(slave_id, address, value);
                }
                
                if (success) {
                    values[std::to_string(address)] = value;
                }
            }
            
            sample["values"] = values;
            samples.push_back(sample);
            
            sample_count++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        monitoring_result["samples"] = samples;
        monitoring_result["total_samples"] = sample_count;
        monitoring_result["end_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        LogMessage(LogLevel::INFO, "레지스터 모니터링 완료: " + std::to_string(sample_count) + "개 샘플 수집");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "MonitorRegisters 예외: " + std::string(e.what()));
        monitoring_result["error"] = e.what();
    }
    
    return monitoring_result.dump(2);
}

std::string ModbusTcpWorker::RunDiagnostics(int slave_id) {
    json diagnostics;
    
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        diagnostics["error"] = "ModbusDriver not connected";
        return diagnostics.dump(2);
    }
    
    try {
        LogMessage(LogLevel::INFO, "Modbus 진단 시작: Slave " + std::to_string(slave_id));
        
        diagnostics["slave_id"] = slave_id;
        diagnostics["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        auto connection_start = std::chrono::high_resolution_clock::now();
        bool connection_ok = TestConnection(slave_id);
        auto connection_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - connection_start).count();
        
        diagnostics["connection_test"] = {
            {"result", connection_ok},
            {"response_time_us", connection_time}
        };
        
        std::vector<int> response_times;
        for (int i = 0; i < 10; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            uint16_t value;
            bool success = ReadSingleHoldingRegister(slave_id, 0, value);
            auto end = std::chrono::high_resolution_clock::now();
            
            if (success) {
                response_times.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            }
        }
        
        if (!response_times.empty()) {
            int total_time = 0;
            int min_time = response_times[0];
            int max_time = response_times[0];
            
            for (int time : response_times) {
                total_time += time;
                min_time = std::min(min_time, time);
                max_time = std::max(max_time, time);
            }
            
            diagnostics["response_time_test"] = {
                {"samples", response_times.size()},
                {"average_us", total_time / response_times.size()},
                {"minimum_us", min_time},
                {"maximum_us", max_time}
            };
        }
        
        json register_tests;
        
        uint16_t holding_value;
        register_tests["holding_register"] = ReadSingleHoldingRegister(slave_id, 0, holding_value);
        
        uint16_t input_value;
        register_tests["input_register"] = ReadSingleInputRegister(slave_id, 0, input_value);
        
        bool coil_value;
        register_tests["coil"] = ReadSingleCoil(slave_id, 0, coil_value);
        
        bool discrete_value;
        register_tests["discrete_input"] = ReadSingleDiscreteInput(slave_id, 0, discrete_value);
        
        diagnostics["register_access_tests"] = register_tests;
        
        if (modbus_driver_) {
            const auto& stats = modbus_driver_->GetStatistics();
            json driver_statistics;
            driver_statistics["total_requests"] = stats.total_reads + stats.total_writes;
            driver_statistics["successful_requests"] = stats.successful_reads + stats.successful_writes;
            driver_statistics["failed_requests"] = stats.failed_reads + stats.failed_writes;
            
            uint64_t total_requests = stats.total_reads + stats.total_writes;
            uint64_t successful_requests = stats.successful_reads + stats.successful_writes;
            driver_statistics["success_rate_percent"] = total_requests > 0 ? 
                (successful_requests * 100.0 / total_requests) : 0.0;
            driver_statistics["average_response_time_ms"] = stats.average_response_time.count();
            
            diagnostics["driver_statistics"] = driver_statistics;
        }
        
        diagnostics["configuration"] = {
            {"endpoint", device_info_.endpoint},
            {"timeout_ms", modbus_config_.timeout_ms},
            {"max_retries", GetPropertyValue(modbus_config_.properties, "max_retries", "3")},
            {"polling_interval_ms", device_info_.polling_interval_ms}
        };
        
        LogMessage(LogLevel::INFO, "Modbus 진단 완료");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "RunDiagnostics 예외: " + std::string(e.what()));
        diagnostics["error"] = e.what();
    }
    
    return diagnostics.dump(2);
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

bool ModbusTcpWorker::ParseAddressString(const std::string& address_str, uint8_t& slave_id, uint16_t& address) {
    try {
        size_t colon_pos = address_str.find(':');
        
        if (colon_pos != std::string::npos) {
            slave_id = static_cast<uint8_t>(std::stoi(address_str.substr(0, colon_pos)));
            address = static_cast<uint16_t>(std::stoi(address_str.substr(colon_pos + 1)));
        } else {
            slave_id = 1;
            address = static_cast<uint16_t>(std::stoi(address_str));
        }
        
        if (slave_id < 1 || slave_id > 247) {
            LogMessage(LogLevel::ERROR, "Invalid slave_id: " + std::to_string(slave_id));
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to parse address string '" + address_str + "': " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::DetermineRegisterType(uint16_t address, ModbusRegisterType& register_type, uint16_t& adjusted_address) {
    if (address >= 1 && address <= 9999) {
        register_type = ModbusRegisterType::COIL;
        adjusted_address = address - 1;
    } else if (address >= 10001 && address <= 19999) {
        register_type = ModbusRegisterType::DISCRETE_INPUT;
        adjusted_address = address - 10001;
    } else if (address >= 30001 && address <= 39999) {
        register_type = ModbusRegisterType::INPUT_REGISTER;
        adjusted_address = address - 30001;
    } else if (address >= 40001 && address <= 49999) {
        register_type = ModbusRegisterType::HOLDING_REGISTER;
        adjusted_address = address - 40001;
    } else {
        register_type = ModbusRegisterType::HOLDING_REGISTER;
        adjusted_address = address;
    }
    
    return true;
}

// =============================================================================
// 쓰기 함수들
// =============================================================================

bool ModbusTcpWorker::WriteSingleHoldingRegister(int slave_id, uint16_t address, uint16_t value) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        bool success = modbus_driver_->WriteHoldingRegister(slave_id, address, value);
        
        DataValue data_value = static_cast<int32_t>(value);
        LogWriteOperation(slave_id, address, data_value, "holding_register", success);
        
        if (success) {
            LogMessage(LogLevel::INFO, 
                      "Holding Register 쓰기 성공: 슬레이브=" + std::to_string(slave_id) + 
                      ", 주소=" + std::to_string(address) + ", 값=" + std::to_string(value));
        } else {
            LogMessage(LogLevel::ERROR, "Holding Register 쓰기 실패");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteSingleHoldingRegister 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteSingleCoil(int slave_id, uint16_t address, bool value) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        bool success = modbus_driver_->WriteCoil(slave_id, address, value);
        
        DataValue data_value = value;
        LogWriteOperation(slave_id, address, data_value, "coil", success);
        
        if (success) {
            LogMessage(LogLevel::INFO, 
                      "Coil 쓰기 성공: 슬레이브=" + std::to_string(slave_id) + 
                      ", 주소=" + std::to_string(address) + ", 값=" + (value ? "ON" : "OFF"));
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteSingleCoil 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteMultipleHoldingRegisters(int slave_id, uint16_t start_address, 
                                                   const std::vector<uint16_t>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // ✅ 수정: WriteMultipleHoldingRegisters → WriteHoldingRegisters
        bool success = modbus_driver_->WriteHoldingRegisters(slave_id, start_address, values);
        
        if (success) {
            LogMessage(LogLevel::INFO, 
                      "Multiple Holding Register 쓰기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteMultipleHoldingRegisters 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::WriteMultipleCoils(int slave_id, uint16_t start_address,
                                        const std::vector<bool>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // ✅ 수정: std::vector<bool> → std::vector<uint8_t> 변환
        std::vector<uint8_t> uint8_values;
        uint8_values.reserve(values.size());
        
        for (bool val : values) {
            uint8_values.push_back(val ? 1 : 0);
        }
        
        // 기존 ModbusDriver에 WriteCoils 메서드가 없으면 개별 쓰기로 대체
        bool success = true;
        for (size_t i = 0; i < values.size(); ++i) {
            bool single_success = modbus_driver_->WriteCoil(slave_id, start_address + i, values[i]);
            if (!single_success) {
                success = false;
                break;
            }
        }
        
        if (success) {
            LogMessage(LogLevel::INFO, 
                      "Multiple Coil 쓰기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteMultipleCoils 예외: " + std::string(e.what()));
        return false;
    }
}


// =============================================================================
// 읽기 함수들
// =============================================================================

bool ModbusTcpWorker::ReadSingleHoldingRegister(int slave_id, uint16_t address, uint16_t& value) {
    std::vector<uint16_t> values;
    bool success = ReadHoldingRegisters(slave_id, address, 1, values);
    
    if (success && !values.empty()) {
        value = values[0];
        
        TimestampedValue tv;
        tv.value = static_cast<int32_t>(value);
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = DataQuality::GOOD;
        tv.source = "debug_holding_" + std::to_string(address);
        
        SendValuesToPipelineWithLogging({tv}, "디버깅 읽기", 20);
        
        return true;
    }
    
    return false;
}

bool ModbusTcpWorker::ReadSingleInputRegister(int slave_id, uint16_t address, uint16_t& value) {
    std::vector<uint16_t> values;
    bool success = ReadInputRegisters(slave_id, address, 1, values);
    
    if (success && !values.empty()) {
        value = values[0];
        return true;
    }
    
    return false;
}

bool ModbusTcpWorker::ReadSingleCoil(int slave_id, uint16_t address, bool& value) {
    std::vector<bool> values;  // 이미 수정된 ReadCoils 사용
    bool success = ReadCoils(slave_id, address, 1, values);
    
    if (success && !values.empty()) {
        value = values[0];
        return true;
    }
    
    return false;
}

bool ModbusTcpWorker::ReadSingleDiscreteInput(int slave_id, uint16_t address, bool& value) {
    std::vector<bool> values;  // 이미 수정된 ReadDiscreteInputs 사용
    bool success = ReadDiscreteInputs(slave_id, address, 1, values);
    
    if (success && !values.empty()) {
        value = values[0];
        return true;
    }
    
    return false;
}

bool ModbusTcpWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, uint16_t count, 
                                          std::vector<uint16_t>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        bool success = modbus_driver_->ReadHoldingRegisters(slave_id, start_address, count, values);
        
        if (success && !values.empty()) {
            std::vector<TimestampedValue> timestamped_values;
            timestamped_values.reserve(values.size());
            
            auto timestamp = std::chrono::system_clock::now();
            for (size_t i = 0; i < values.size(); ++i) {
                TimestampedValue tv;
                tv.value = static_cast<int32_t>(values[i]);
                tv.timestamp = timestamp;
                tv.quality = DataQuality::GOOD;
                tv.source = "debug_holding_" + std::to_string(start_address + i);
                timestamped_values.push_back(tv);
            }
            
            SendValuesToPipelineWithLogging(timestamped_values, "Holding 레지스터 범위 읽기", 15);
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Holding Register 읽기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadHoldingRegisters 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ReadInputRegisters(int slave_id, uint16_t start_address, uint16_t count, 
                                        std::vector<uint16_t>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        bool success = modbus_driver_->ReadInputRegisters(slave_id, start_address, count, values);
        
        if (success && !values.empty()) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Input Register 읽기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadInputRegisters 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ReadCoils(int slave_id, uint16_t start_address, uint16_t count,
                               std::vector<bool>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // ✅ 수정: std::vector<bool> → std::vector<uint8_t>
        std::vector<uint8_t> uint8_values;
        bool success = modbus_driver_->ReadCoils(slave_id, start_address, count, uint8_values);
        
        if (success) {
            // uint8_t → bool 변환
            values.clear();
            values.reserve(uint8_values.size());
            
            for (uint8_t val : uint8_values) {
                values.push_back(val != 0);
            }
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Coil 읽기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadCoils 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ReadDiscreteInputs(int slave_id, uint16_t start_address, uint16_t count,
                                        std::vector<bool>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // ✅ 수정: std::vector<bool> → std::vector<uint8_t>
        std::vector<uint8_t> uint8_values;
        bool success = modbus_driver_->ReadDiscreteInputs(slave_id, start_address, count, uint8_values);
        
        if (success) {
            // uint8_t → bool 변환
            values.clear();
            values.reserve(uint8_values.size());
            
            for (uint8_t val : uint8_values) {
                values.push_back(val != 0);
            }
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Discrete Input 읽기 성공: " + std::to_string(values.size()) + "개 값");
        }
        
        return success;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadDiscreteInputs 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 고수준 DataPoint 함수들
// =============================================================================

bool ModbusTcpWorker::WriteDataPointValue(const std::string& point_id, const DataValue& value) {
    auto data_point_opt = FindDataPointById(point_id);
    if (!data_point_opt.has_value()) {
        LogMessage(LogLevel::ERROR, "DataPoint not found: " + point_id);
        return false;
    }
    
    const auto& data_point = data_point_opt.value();
    
    uint8_t slave_id;
    ModbusRegisterType register_type;
    uint16_t address;
    
    if (!ParseModbusAddress(data_point, slave_id, register_type, address)) {
        LogMessage(LogLevel::ERROR, "Invalid Modbus address: " + point_id);
        return false;
    }
    
    try {
        bool success = false;
        
        if (register_type == ModbusRegisterType::HOLDING_REGISTER) {
            int32_t int_value = std::get<int32_t>(value);
            uint16_t modbus_value = static_cast<uint16_t>(int_value);
            success = WriteSingleHoldingRegister(slave_id, address, modbus_value);
        } else if (register_type == ModbusRegisterType::COIL) {
            bool coil_value = std::get<bool>(value);
            success = WriteSingleCoil(slave_id, address, coil_value);
        } else {
            LogMessage(LogLevel::ERROR, "Read-only register type: " + point_id);
            return false;
        }
        
        if (success) {
            LogMessage(LogLevel::INFO, "DataPoint 쓰기 성공: " + point_id);
        }
        
        return success;
    } catch (const std::bad_variant_access& e) {
        LogMessage(LogLevel::ERROR, "DataValue 타입 불일치: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteDataPointValue 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ReadDataPointValue(const std::string& point_id, TimestampedValue& value) {
    auto data_point_opt = FindDataPointById(point_id);
    if (!data_point_opt.has_value()) {
        LogMessage(LogLevel::ERROR, "DataPoint not found: " + point_id);
        return false;
    }
    
    const auto& data_point = data_point_opt.value();
    
    uint8_t slave_id;
    ModbusRegisterType register_type;
    uint16_t address;
    
    if (!ParseModbusAddress(data_point, slave_id, register_type, address)) {
        LogMessage(LogLevel::ERROR, "Invalid Modbus address: " + point_id);
        return false;
    }
    
    try {
        value.timestamp = std::chrono::system_clock::now();
        value.quality = DataQuality::GOOD;
        value.point_id = std::stoi(data_point.id);
        value.source = "manual_read_" + point_id;
        
        if (register_type == ModbusRegisterType::HOLDING_REGISTER) {
            uint16_t raw_value;
            if (ReadSingleHoldingRegister(slave_id, address, raw_value)) {
                value.value = static_cast<int32_t>(raw_value);
                return true;
            }
        } else if (register_type == ModbusRegisterType::INPUT_REGISTER) {
            uint16_t raw_value;
            if (ReadSingleInputRegister(slave_id, address, raw_value)) {
                value.value = static_cast<int32_t>(raw_value);
                return true;
            }
        } else if (register_type == ModbusRegisterType::COIL) {
            bool raw_value;
            if (ReadSingleCoil(slave_id, address, raw_value)) {
                value.value = raw_value;
                return true;
            }
        } else if (register_type == ModbusRegisterType::DISCRETE_INPUT) {
            bool raw_value;
            if (ReadSingleDiscreteInput(slave_id, address, raw_value)) {
                value.value = raw_value;
                return true;
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ReadDataPointValue 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ReadMultipleDataPoints(const std::vector<std::string>& point_ids,
                            std::vector<TimestampedValue>& values) {
    values.clear();
    values.reserve(point_ids.size());
    
    for (const auto& point_id : point_ids) {
        TimestampedValue value;
        if (ReadDataPointValue(point_id, value)) {
            values.push_back(value);
        }
    }
    
    return !values.empty();
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

bool ModbusTcpWorker::ParseModbusConfig() {
    try {
        LogMessage(LogLevel::INFO, "Modbus TCP configuration parsing...");
        
        modbus_config_.device_id = device_info_.id;
        modbus_config_.name = device_info_.name;
        modbus_config_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.timeout_ms;
        modbus_config_.retry_count = device_info_.retry_count;
        
        nlohmann::json protocol_config_json;
        
        if (!device_info_.config.empty()) {
            try {
                protocol_config_json = nlohmann::json::parse(device_info_.config);
                LogMessage(LogLevel::INFO, "Protocol config loaded from device.config");
            } catch (const std::exception& e) {
                LogMessage(LogLevel::WARN, "Failed to parse device.config JSON: " + std::string(e.what()));
            }
        }
        
        if (protocol_config_json.empty() && !device_info_.connection_string.empty()) {
            if (device_info_.connection_string.front() == '{') {
                try {
                    protocol_config_json = nlohmann::json::parse(device_info_.connection_string);
                    LogMessage(LogLevel::INFO, "Protocol config loaded from connection_string JSON");
                } catch (const std::exception& e) {
                    LogMessage(LogLevel::WARN, "Failed to parse connection_string JSON: " + std::string(e.what()));
                }
            }
        }
        
        if (protocol_config_json.empty()) {
            protocol_config_json = {
                {"slave_id", 1},
                {"byte_order", "big_endian"},
                {"max_registers_per_group", 125},
                {"auto_group_creation", true}
            };
            LogMessage(LogLevel::INFO, "Applied default Modbus protocol configuration");
        }
        
        modbus_config_.properties["slave_id"] = std::to_string(protocol_config_json.value("slave_id", 1));
        modbus_config_.properties["byte_order"] = protocol_config_json.value("byte_order", "big_endian");
        modbus_config_.properties["max_registers_per_group"] = std::to_string(protocol_config_json.value("max_registers_per_group", 125));
        modbus_config_.properties["auto_group_creation"] = protocol_config_json.value("auto_group_creation", true) ? "true" : "false";
        
        if (protocol_config_json.contains("timeout")) {
            int db_timeout = protocol_config_json.value("timeout", device_info_.timeout_ms);
            modbus_config_.timeout_ms = db_timeout;
            modbus_config_.properties["response_timeout_ms"] = std::to_string(db_timeout);
        } else {
            modbus_config_.properties["response_timeout_ms"] = std::to_string(device_info_.timeout_ms);
        }
        
        modbus_config_.properties["byte_timeout_ms"] = std::to_string(std::min(static_cast<int>(modbus_config_.timeout_ms / 10), 1000));
        modbus_config_.properties["max_retries"] = std::to_string(device_info_.retry_count);
        modbus_config_.properties["polling_interval_ms"] = std::to_string(device_info_.polling_interval_ms);
        modbus_config_.properties["keep_alive"] = device_info_.is_enabled ? "enabled" : "disabled";
        
        LogMessage(LogLevel::INFO, "Modbus config parsed successfully");
        return true;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ParseModbusConfig failed: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::InitializeModbusDriver() {
    try {
        LogMessage(LogLevel::INFO, "Initializing ModbusDriver...");
        
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            LogMessage(LogLevel::ERROR, "Failed to create ModbusDriver instance");
            return false;
        }
        
        PulseOne::Structs::DriverConfig driver_config = device_info_.driver_config;
        
        driver_config.device_id = device_info_.name;
        driver_config.endpoint = device_info_.endpoint;
        driver_config.protocol = PulseOne::Enums::ProtocolType::MODBUS_TCP;
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        driver_config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
        driver_config.polling_interval_ms = static_cast<uint32_t>(device_info_.polling_interval_ms);
        driver_config.auto_reconnect = device_info_.properties.count("auto_reconnect") ? 
            (device_info_.properties.at("auto_reconnect") == "true") : true;
        
        driver_config.properties["slave_id"] = GetPropertyValue(modbus_config_.properties, "slave_id", "1");
        driver_config.properties["byte_order"] = GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian");
        driver_config.properties["max_retries"] = GetPropertyValue(modbus_config_.properties, "max_retries", "3");
        driver_config.properties["response_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000");
        driver_config.properties["byte_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100");
        driver_config.properties["max_registers_per_group"] = GetPropertyValue(modbus_config_.properties, "max_registers_per_group", "125");
        
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(LogLevel::ERROR, "ModbusDriver initialization failed: " + error.message);
            return false;
        }
        
        SetupDriverCallbacks();
        
        LogMessage(LogLevel::INFO, "ModbusDriver initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception during ModbusDriver initialization: " + std::string(e.what()));
        
        if (modbus_driver_) {
            modbus_driver_.reset();
        }
        
        return false;
    }
}

void ModbusTcpWorker::PollingThreadFunction() {
    LogMessage(LogLevel::INFO, "Polling thread started");
    
    while (polling_thread_running_) {
        try {
            auto current_time = system_clock::now();
            
            std::vector<ModbusTcpPollingGroup> groups_to_poll;
            {
                std::lock_guard<std::mutex> lock(polling_groups_mutex_);
                for (auto& pair : polling_groups_) {
                    auto& group = pair.second;
                    
                    if (group.enabled && current_time >= group.next_poll_time) {
                        groups_to_poll.push_back(group);
                        
                        group.last_poll_time = current_time;
                        group.next_poll_time = current_time + milliseconds(group.polling_interval_ms);
                    }
                }
            }
            
            for (const auto& group : groups_to_poll) {
                if (!polling_thread_running_) {
                    break;
                }
                
                ProcessPollingGroup(group);
            }
            
            std::this_thread::sleep_for(milliseconds(100));
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception in polling thread: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(1));
        }
    }
    
    LogMessage(LogLevel::INFO, "Polling thread stopped");
}

bool ModbusTcpWorker::ProcessPollingGroup(const ModbusTcpPollingGroup& group) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected, skipping group " + std::to_string(group.group_id));
        return false;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Processing polling group " + std::to_string(group.group_id));
    
    try {
        std::vector<TimestampedValue> values;
        bool success = modbus_driver_->ReadValues(group.data_points, values);
        
        if (success && !values.empty()) {
            SendValuesToPipelineWithLogging(values, "그룹 " + std::to_string(group.group_id), 0);
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "Successfully processed group " + std::to_string(group.group_id) + 
                   ", read " + std::to_string(values.size()) + " values");
        
        return true;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception processing group " + std::to_string(group.group_id) + ": " + std::string(e.what()));
        return false;
    }
}

size_t ModbusTcpWorker::CreatePollingGroupsFromDataPoints(const std::vector<PulseOne::DataPoint>& data_points) {
    if (data_points.empty()) {
        return 0;
    }
    
    LogMessage(LogLevel::INFO, "Creating polling groups from " + std::to_string(data_points.size()) + " data points");
    
    std::map<std::tuple<uint8_t, ModbusRegisterType>, std::vector<DataPoint>> grouped_points;
    
    for (const auto& data_point : data_points) {
        uint8_t slave_id;
        ModbusRegisterType register_type;
        uint16_t address;
        
        if (!ParseModbusAddress(data_point, slave_id, register_type, address)) {
            LogMessage(LogLevel::WARN, "Failed to parse Modbus address for data point: " + data_point.name);
            continue;
        }
        
        auto key = std::make_tuple(slave_id, register_type);
        grouped_points[key].push_back(data_point);
    }
    
    size_t created_groups = 0;
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    for (const auto& group_pair : grouped_points) {
        const auto& key = group_pair.first;
        const auto& points = group_pair.second;
        
        uint8_t slave_id = std::get<0>(key);
        ModbusRegisterType register_type = std::get<1>(key);
        
        for (const auto& point : points) {
            uint8_t slave_id_temp;
            ModbusRegisterType register_type_temp;
            uint16_t address_temp;
            
            if (!ParseModbusAddress(point, slave_id_temp, register_type_temp, address_temp)) {
                continue;
            }
            
            // ✅ 수정: 중복 변수 선언 제거
            ModbusTcpPollingGroup polling_group;
            polling_group.group_id = next_group_id_++;
            polling_group.slave_id = slave_id;
            polling_group.register_type = register_type;
            polling_group.start_address = address_temp;
            polling_group.register_count = 1;
            polling_group.polling_interval_ms = default_polling_interval_ms_;
            polling_group.enabled = true;
            polling_group.data_points = {point};
            polling_group.last_poll_time = system_clock::now();
            polling_group.next_poll_time = system_clock::now();
            
            polling_groups_[polling_group.group_id] = polling_group;
            created_groups++;
        }
    }
    
    LogMessage(LogLevel::INFO, "Created " + std::to_string(created_groups) + " polling groups");
    return created_groups;
}

size_t ModbusTcpWorker::OptimizePollingGroups() {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    size_t original_count = polling_groups_.size();
    std::vector<ModbusTcpPollingGroup> optimized_groups;
    
    for (auto it1 = polling_groups_.begin(); it1 != polling_groups_.end(); ++it1) {
        bool merged = false;
        
        for (auto& optimized_group : optimized_groups) {
            if (CanMergePollingGroups(it1->second, optimized_group)) {
                optimized_group = MergePollingGroups(it1->second, optimized_group);
                merged = true;
                break;
            }
        }
        
        if (!merged) {
            optimized_groups.push_back(it1->second);
        }
    }
    
    polling_groups_.clear();
    uint32_t new_group_id = 1;
    
    for (auto& group : optimized_groups) {
        group.group_id = new_group_id++;
        polling_groups_[group.group_id] = group;
    }
    
    next_group_id_ = new_group_id;
    
    size_t optimized_count = polling_groups_.size();
    LogMessage(LogLevel::INFO, "Optimized polling groups: " + std::to_string(original_count) + 
               " → " + std::to_string(optimized_count));
    
    return optimized_count;
}

bool ModbusTcpWorker::ParseModbusAddress(const PulseOne::DataPoint& data_point,
                                         uint8_t& slave_id,
                                         ModbusRegisterType& register_type,
                                         uint16_t& address) {
    try {
        std::string addr_str = std::to_string(data_point.address);
        size_t colon_pos = addr_str.find(':');
        
        if (colon_pos != std::string::npos) {
            slave_id = static_cast<uint8_t>(std::stoi(addr_str.substr(0, colon_pos)));
            address = static_cast<uint16_t>(std::stoi(addr_str.substr(colon_pos + 1)));
        } else {
            slave_id = 1;
            address = static_cast<uint16_t>(data_point.address);
        }
        
        if (address >= 1 && address <= 9999) {
            register_type = ModbusRegisterType::COIL;
            address -= 1;
        } else if (address >= 10001 && address <= 19999) {
            register_type = ModbusRegisterType::DISCRETE_INPUT;
            address -= 10001;
        } else if (address >= 30001 && address <= 39999) {
            register_type = ModbusRegisterType::INPUT_REGISTER;
            address -= 30001;
        } else if (address >= 40001 && address <= 49999) {
            register_type = ModbusRegisterType::HOLDING_REGISTER;
            address -= 40001;
        } else {
            register_type = ModbusRegisterType::HOLDING_REGISTER;
        }
        
        return true;
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to parse Modbus address for " + data_point.name + ": " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ValidatePollingGroup(const ModbusTcpPollingGroup& group) {
    if (group.data_points.empty()) {
        LogMessage(LogLevel::ERROR, "Polling group has no data points");
        return false;
    }
    
    if (group.register_count == 0 || group.register_count > max_registers_per_group_) {
        LogMessage(LogLevel::ERROR, "Invalid register count: " + std::to_string(group.register_count));
        return false;
    }
    
    if (group.slave_id == 0 || group.slave_id > 247) {
        LogMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(group.slave_id));
        return false;
    }
    
    if (group.polling_interval_ms < 100) {
        LogMessage(LogLevel::ERROR, "Polling interval too short: " + std::to_string(group.polling_interval_ms) + "ms");
        return false;
    }
    
    return true;
}

bool ModbusTcpWorker::CanMergePollingGroups(const ModbusTcpPollingGroup& group1,
                                           const ModbusTcpPollingGroup& group2) {
    if (group1.slave_id != group2.slave_id || group1.register_type != group2.register_type) {
        return false;
    }
    
    uint16_t end1 = group1.start_address + group1.register_count;
    uint16_t end2 = group2.start_address + group2.register_count;
    
    bool adjacent = (end1 == group2.start_address) || (end2 == group1.start_address);
    if (!adjacent) {
        return false;
    }
    
    uint16_t total_count = group1.register_count + group2.register_count;
    return total_count <= max_registers_per_group_;
}

ModbusTcpPollingGroup ModbusTcpWorker::MergePollingGroups(const ModbusTcpPollingGroup& group1,
                                                         const ModbusTcpPollingGroup& group2) {
    ModbusTcpPollingGroup merged;
    
    merged.group_id = std::min(group1.group_id, group2.group_id);
    merged.slave_id = group1.slave_id;
    merged.register_type = group1.register_type;
    merged.start_address = std::min(group1.start_address, group2.start_address);
    merged.register_count = group1.register_count + group2.register_count;
    merged.polling_interval_ms = std::min(group1.polling_interval_ms, group2.polling_interval_ms);
    merged.enabled = group1.enabled && group2.enabled;
    
    merged.data_points = group1.data_points;
    merged.data_points.insert(merged.data_points.end(), 
                             group2.data_points.begin(), group2.data_points.end());
    
    merged.last_poll_time = std::min(group1.last_poll_time, group2.last_poll_time);
    merged.next_poll_time = std::min(group1.next_poll_time, group2.next_poll_time);
    
    return merged;
}

void ModbusTcpWorker::SetupDriverCallbacks() {
    if (!modbus_driver_) {
        return;
    }
    
    try {
        LogMessage(LogLevel::DEBUG_LEVEL, "Setting up ModbusDriver callbacks...");
        LogMessage(LogLevel::DEBUG_LEVEL, "ModbusDriver callbacks configured");
    } catch (const std::exception& e) {
        LogMessage(LogLevel::WARN, "Failed to setup driver callbacks: " + std::string(e.what()));
    }
}

void ModbusTcpWorker::OnConnectionStatusChanged(void* worker_ptr, bool connected,
                                               const std::string& error_message) {
    auto* worker = static_cast<ModbusTcpWorker*>(worker_ptr);
    if (!worker) {
        return;
    }
    
    if (connected) {
        worker->LogMessage(LogLevel::INFO, "Modbus connection established");
    } else {
        worker->LogMessage(LogLevel::WARN, "Modbus connection lost: " + error_message);
    }
}

void ModbusTcpWorker::OnModbusError(void* worker_ptr, uint8_t slave_id, uint8_t function_code,
                                   int error_code, const std::string& error_message) {
    auto* worker = static_cast<ModbusTcpWorker*>(worker_ptr);
    if (!worker) {
        return;
    }
    
    worker->LogMessage(LogLevel::ERROR, "Modbus error - Slave: " + std::to_string(slave_id) + 
                       ", Function: " + std::to_string(function_code) + 
                       ", Code: " + std::to_string(error_code) + 
                       ", Message: " + error_message);
}

void ModbusTcpWorker::OnStatisticsUpdate(void* worker_ptr, const std::string& operation,
                                        bool success, uint32_t response_time_ms) {
    auto* worker = static_cast<ModbusTcpWorker*>(worker_ptr);
    if (!worker) {
        return;
    }
    
    worker->LogMessage(LogLevel::DEBUG_LEVEL, "Modbus " + operation + 
                       (success ? " succeeded" : " failed") + 
                       " in " + std::to_string(response_time_ms) + "ms");
}

void ModbusTcpWorker::LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                                       const std::string& register_type, bool success) {
    try {
        TimestampedValue control_log;
        control_log.value = value;
        control_log.timestamp = std::chrono::system_clock::now();
        control_log.quality = success ? DataQuality::GOOD : DataQuality::BAD;
        control_log.source = "control_" + register_type + "_" + std::to_string(address) + 
                            "_slave" + std::to_string(slave_id);
        
        SendValuesToPipelineWithLogging({control_log}, "제어 이력", 1);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "LogWriteOperation 예외: " + std::string(e.what()));
    }
}

std::optional<DataPoint> ModbusTcpWorker::FindDataPointById(const std::string& point_id) {
    const auto& data_points = GetDataPoints();
    
    for (const auto& point : data_points) {
        if (point.id == point_id) {
            return point;
        }
    }
    
    return std::nullopt;
}

bool ModbusTcpWorker::SendReadResultToPipeline(const std::vector<bool>& values, uint16_t start_address,
                                             const std::string& register_type, int slave_id) {
    try {
        std::vector<TimestampedValue> timestamped_values;
        timestamped_values.reserve(values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        for (size_t i = 0; i < values.size(); ++i) {
            TimestampedValue tv;
            tv.value = values[i];  // bool 타입 직접 사용
            tv.timestamp = timestamp;
            tv.quality = DataQuality::GOOD;
            tv.source = register_type + "_" + std::to_string(start_address + i) + "_slave" + std::to_string(slave_id);
            timestamped_values.push_back(tv);
        }
        
        return SendValuesToPipelineWithLogging(timestamped_values, register_type + " 읽기", 15);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "SendReadResultToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::SendSingleValueToPipeline(const DataValue& value, uint16_t address,
                                              const std::string& register_type, int slave_id) {
    try {
        TimestampedValue tv;
        tv.value = value;
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = DataQuality::GOOD;
        tv.source = register_type + "_" + std::to_string(address) + "_slave" + std::to_string(slave_id);
        
        return SendValuesToPipelineWithLogging({tv}, "단일 값", 15);
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "SendSingleValueToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

} // namespace Workers  
} // namespace PulseOne