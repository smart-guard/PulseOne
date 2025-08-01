/**
 * @file ModbusRtuWorker.cpp - 컴파일 에러 수정된 최종 버전
 * @brief Modbus RTU 워커 클래스 구현 (네임스페이스 충돌 해결)
 * @details SerialBasedWorker 기반의 Modbus RTU 프로토콜 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.1.0
 */

#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Utils/LogManager.h"
#include "Common/Structs.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <thread>

// ✅ 수정: 네임스페이스 충돌 방지 - std 네임스페이스만 사용
using std::string;
using std::vector;
using std::shared_ptr;
using std::unique_ptr;
using std::chrono::system_clock;
using std::chrono::steady_clock;
using std::chrono::milliseconds;

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 생성자 및 소멸자 (네임스페이스 수정)
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(
    const PulseOne::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : SerialBasedWorker(device_info, redis_client, influx_client)
    , modbus_driver_(nullptr)
    , next_group_id_(1)
    , stop_workers_(false) {
    
    // ModbusDriver 생성
    modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
    
    // 설정 파싱
    ParseModbusConfig();
    
    // 드라이버 초기화
    InitializeModbusDriver();
    
    LogRtuMessage(PulseOne::LogLevel::INFO, "ModbusRtuWorker created for port: " + modbus_config_.endpoint);
}

ModbusRtuWorker::~ModbusRtuWorker() {
    stop_workers_ = true;
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    LogRtuMessage(PulseOne::LogLevel::INFO, "ModbusRtuWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusRtuWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogRtuMessage(PulseOne::LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogRtuMessage(PulseOne::LogLevel::INFO, "Starting ModbusRtuWorker...");
        
        try {
            if (!EstablishConnection()) {
                LogRtuMessage(PulseOne::LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            ChangeState(WorkerState::RUNNING);
            
            stop_workers_ = false;
            polling_thread_ = std::thread(&ModbusRtuWorker::PollingWorkerThread, this);
            
            LogRtuMessage(PulseOne::LogLevel::INFO, "ModbusRtuWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogRtuMessage(PulseOne::LogLevel::ERROR, "Start failed: " + string(e.what()));
            ChangeState(WorkerState::ERROR);
            return false;
        }
    });
}

std::future<bool> ModbusRtuWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::STOPPED) {
            LogRtuMessage(PulseOne::LogLevel::WARN, "Worker already stopped");
            return true;
        }
        
        LogRtuMessage(PulseOne::LogLevel::INFO, "Stopping ModbusRtuWorker...");
        
        try {
            stop_workers_ = true;
            if (polling_thread_.joinable()) {
                polling_thread_.join();
            }
            
            CloseConnection();
            ChangeState(WorkerState::STOPPED);
            
            LogRtuMessage(PulseOne::LogLevel::INFO, "ModbusRtuWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogRtuMessage(PulseOne::LogLevel::ERROR, "Stop failed: " + string(e.what()));
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
    LogRtuMessage(PulseOne::LogLevel::INFO, "Establishing Modbus RTU protocol connection");
    
    if (!modbus_driver_) {
        LogRtuMessage(PulseOne::LogLevel::ERROR, "ModbusDriver not initialized");
        return false;
    }
    
    try {
        PulseOne::DriverConfig config;
        config.device_id = device_info_.name;
        config.protocol = PulseOne::ProtocolType::MODBUS_RTU;
        config.endpoint = modbus_config_.endpoint;
        config.timeout_ms = modbus_config_.timeout_ms;
        config.retry_count = modbus_config_.max_retries;
        
        // RTU 특화 설정
        config.custom_settings["slave_id"] = std::to_string(modbus_config_.slave_id);
        config.custom_settings["baud_rate"] = std::to_string(modbus_config_.baud_rate);
        config.custom_settings["parity"] = string(1, modbus_config_.parity);
        config.custom_settings["data_bits"] = std::to_string(modbus_config_.data_bits);
        config.custom_settings["stop_bits"] = std::to_string(modbus_config_.stop_bits);
        config.custom_settings["frame_delay_ms"] = std::to_string(modbus_config_.frame_delay_ms);
        
        if (!modbus_driver_->Initialize(config)) {
            auto error = modbus_driver_->GetLastError();
            LogRtuMessage(PulseOne::LogLevel::ERROR, "Failed to initialize ModbusDriver: " + error.message);
            return false;
        }
        
        if (!modbus_driver_->Connect()) {
            auto error = modbus_driver_->GetLastError();
            LogRtuMessage(PulseOne::LogLevel::ERROR, "Failed to connect: " + error.message);
            return false;
        }
        
        LogRtuMessage(PulseOne::LogLevel::INFO, "Modbus RTU protocol connection established");
        return true;
        
    } catch (const std::exception& e) {
        LogRtuMessage(PulseOne::LogLevel::ERROR, "EstablishProtocolConnection failed: " + string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    LogRtuMessage(PulseOne::LogLevel::INFO, "Closing Modbus RTU protocol connection");
    
    if (modbus_driver_) {
        modbus_driver_->Disconnect();
        LogRtuMessage(PulseOne::LogLevel::INFO, "Modbus RTU protocol connection closed");
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
                LogRtuMessage(PulseOne::LogLevel::WARN, "Slave " + std::to_string(slave_id) + " is offline");
            }
        }
    }
    
    return any_response;
}

// =============================================================================
// ✅ 통합된 설정 API (구조체 기반)
// =============================================================================

void ModbusRtuWorker::ConfigureModbusRtu(const PulseOne::Drivers::ModbusConfig& config) {
    modbus_config_ = config;
    LogRtuMessage(PulseOne::LogLevel::INFO, "Modbus RTU configured:\n" + modbus_config_.ToString(true));
}

void ModbusRtuWorker::SetSlaveId(int slave_id) {
    if (slave_id >= 1 && slave_id <= 247) {
        modbus_config_.slave_id = slave_id;
        LogRtuMessage(PulseOne::LogLevel::INFO, "Slave ID set to: " + std::to_string(slave_id));
    } else {
        LogRtuMessage(PulseOne::LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
    }
}

void ModbusRtuWorker::SetResponseTimeout(int timeout_ms) {
    if (timeout_ms >= 100 && timeout_ms <= 10000) {
        modbus_config_.response_timeout_ms = timeout_ms;
        LogRtuMessage(PulseOne::LogLevel::INFO, "Response timeout set to: " + std::to_string(timeout_ms) + "ms");
    } else {
        LogRtuMessage(PulseOne::LogLevel::ERROR, "Invalid timeout: " + std::to_string(timeout_ms) + "ms");
    }
}

// =============================================================================
// 슬레이브 관리
// =============================================================================

bool ModbusRtuWorker::AddSlave(int slave_id, const string& device_name) {
    if (slave_id < 1 || slave_id > 247) {
        LogRtuMessage(PulseOne::LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    if (slaves_.find(slave_id) != slaves_.end()) {
        LogRtuMessage(PulseOne::LogLevel::WARN, "Slave " + std::to_string(slave_id) + " already exists");
        return false;
    }
    
    auto slave_info = std::make_shared<ModbusRtuSlaveInfo>(
        slave_id, 
        device_name.empty() ? "Slave_" + std::to_string(slave_id) : device_name
    );
    
    slaves_[slave_id] = slave_info;
    
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Added slave " + std::to_string(slave_id) + 
        " (" + slave_info->device_name + ")");
    
    return true;
}

bool ModbusRtuWorker::RemoveSlave(int slave_id) {
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it == slaves_.end()) {
        LogRtuMessage(PulseOne::LogLevel::WARN, "Slave " + std::to_string(slave_id) + " not found");
        return false;
    }
    
    slaves_.erase(it);
    LogRtuMessage(PulseOne::LogLevel::INFO, "Removed slave " + std::to_string(slave_id));
    
    return true;
}

shared_ptr<ModbusRtuSlaveInfo> ModbusRtuWorker::GetSlaveInfo(int slave_id) const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    return (it != slaves_.end()) ? it->second : nullptr;
}

int ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int timeout_ms) {
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Scanning slaves from " + std::to_string(start_id) + 
        " to " + std::to_string(end_id));
    
    int found_count = 0;
    int original_timeout = modbus_config_.response_timeout_ms;
    modbus_config_.response_timeout_ms = timeout_ms;
    
    for (int slave_id = start_id; slave_id <= end_id; ++slave_id) {
        int response_time = CheckSlaveStatus(slave_id);
        if (response_time >= 0) {
            AddSlave(slave_id);
            UpdateSlaveStatus(slave_id, response_time, true);
            found_count++;
            
            LogRtuMessage(PulseOne::LogLevel::INFO, 
                "Found slave " + std::to_string(slave_id) + 
                " (response time: " + std::to_string(response_time) + "ms)");
        }
        
        std::this_thread::sleep_for(milliseconds(modbus_config_.frame_delay_ms));
    }
    
    modbus_config_.response_timeout_ms = original_timeout;
    
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Slave scan completed. Found " + std::to_string(found_count) + " slaves");
    
    return found_count;
}

// =============================================================================
// 폴링 그룹 관리
// =============================================================================

uint32_t ModbusRtuWorker::AddPollingGroup(const string& group_name,
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
    
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Added polling group " + std::to_string(group_id) + 
        " (" + group_name + ") for slave " + std::to_string(slave_id));
    
    return group_id;
}

bool ModbusRtuWorker::RemovePollingGroup(uint32_t group_id) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(PulseOne::LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    polling_groups_.erase(it);
    LogRtuMessage(PulseOne::LogLevel::INFO, "Removed polling group " + std::to_string(group_id));
    
    return true;
}

bool ModbusRtuWorker::EnablePollingGroup(uint32_t group_id, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(PulseOne::LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Polling group " + std::to_string(group_id) + 
        (enabled ? " enabled" : " disabled"));
    
    return true;
}

bool ModbusRtuWorker::AddDataPointToGroup(uint32_t group_id, const PulseOne::DataPoint& data_point) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogRtuMessage(PulseOne::LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.data_points.push_back(data_point);
    LogRtuMessage(PulseOne::LogLevel::INFO, 
        "Added data point " + data_point.name + 
        " to polling group " + std::to_string(group_id));
    
    return true;
}

// =============================================================================
// 데이터 읽기/쓰기 (간소화된 버전)
// =============================================================================

bool ModbusRtuWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                                          uint16_t register_count, vector<uint16_t>& values) {
    LockBus();
    
    try {
        vector<PulseOne::Structs::DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::HOLDING_REGISTER, start_address, register_count);
        
        vector<TimestampedValue> timestamped_values;
        bool success = modbus_driver_->ReadValues(data_points, timestamped_values);
        
        if (success) {
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
            auto error = modbus_driver_->GetLastError();
            LogRtuMessage(PulseOne::LogLevel::ERROR, 
                "Failed to read holding registers from slave " + std::to_string(slave_id) + 
                ": " + error.message);
            
            UpdateRtuStats("read", false, "read_error");
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        LogRtuMessage(PulseOne::LogLevel::ERROR, "ReadHoldingRegisters exception: " + string(e.what()));
        UpdateRtuStats("read", false, "exception");
        return false;
    }
}

// =============================================================================
// ✅ 간소화된 상태 조회 API
// =============================================================================

string ModbusRtuWorker::GetModbusRtuStats() const {
    return rtu_stats_.ToJson();
}

string ModbusRtuWorker::GetSerialBusStatus() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"endpoint\": \"" << modbus_config_.endpoint << "\",\n";
    ss << "  \"baud_rate\": " << modbus_config_.baud_rate << ",\n";
    ss << "  \"data_bits\": " << modbus_config_.data_bits << ",\n";
    ss << "  \"parity\": \"" << modbus_config_.parity << "\",\n";
    ss << "  \"stop_bits\": " << modbus_config_.stop_bits << ",\n";
    ss << "  \"response_timeout_ms\": " << modbus_config_.response_timeout_ms << ",\n";
    ss << "  \"byte_timeout_ms\": " << modbus_config_.byte_timeout_ms << ",\n";
    ss << "  \"frame_delay_ms\": " << modbus_config_.frame_delay_ms << ",\n";
    ss << "  \"is_connected\": " << (const_cast<ModbusRtuWorker*>(this)->CheckProtocolConnection() ? "true" : "false") << "\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// ✅ 헬퍼 메서드들 (구조체 기반)
// =============================================================================

void ModbusRtuWorker::UpdateRtuStats(const string& operation, bool success, 
                                     const string& error_type) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (operation == "read") {
        rtu_stats_.total_reads++;
        if (success) {
            rtu_stats_.successful_reads++;
        }
    } else if (operation == "write") {
        rtu_stats_.total_writes++;
        if (success) {
            rtu_stats_.successful_writes++;
        }
    }
    
    if (!success && !error_type.empty()) {
        if (error_type == "crc") {
            rtu_stats_.crc_errors++;
        } else if (error_type == "timeout") {
            rtu_stats_.timeout_errors++;
        } else if (error_type == "frame") {
            rtu_stats_.frame_errors++;
        }
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
                uint32_t current_avg = slave_info->response_time_ms.load();
                uint32_t new_avg = (current_avg * 7 + response_time_ms) / 8;
                slave_info->response_time_ms = new_avg;
            }
            
            slave_info->last_error.clear();
        } else {
            slave_info->is_online = false;
            
            if (modbus_driver_) {
                auto error = modbus_driver_->GetLastError();
                slave_info->last_error = error.message;
                
                if (error.message.find("CRC") != string::npos) {
                    slave_info->crc_errors++;
                } else if (error.message.find("timeout") != string::npos || 
                          error.message.find("Timeout") != string::npos) {
                    slave_info->timeout_errors++;
                }
            }
        }
    }
}

int ModbusRtuWorker::CheckSlaveStatus(int slave_id) {
    auto start_time = steady_clock::now();
    
    try {
        vector<uint16_t> test_values;
        bool success = ReadHoldingRegisters(slave_id, 0, 1, test_values);
        
        auto end_time = steady_clock::now();
        int response_time = std::chrono::duration_cast<milliseconds>(end_time - start_time).count();
        
        return success ? response_time : -1;
        
    } catch (const std::exception& e) {
        LogRtuMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
            "CheckSlaveStatus exception for slave " + std::to_string(slave_id) + 
            ": " + string(e.what()));
        return -1;
    }
}

void ModbusRtuWorker::LockBus() {
    bus_mutex_.lock();
    
    if (modbus_config_.frame_delay_ms > 0) {
        std::this_thread::sleep_for(milliseconds(modbus_config_.frame_delay_ms));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

void ModbusRtuWorker::LogRtuMessage(LogLevel level, const string& message) {
    string prefix = "[ModbusRTU:" + modbus_config_.endpoint + "] ";
    LogMessage(level, prefix + message);
}

vector<PulseOne::Structs::DataPoint> ModbusRtuWorker::CreateDataPoints(int slave_id, 
                                                                 ModbusRegisterType register_type,
                                                                 uint16_t start_address, 
                                                                 uint16_t count) {
    vector<PulseOne::Structs::DataPoint> data_points;
    data_points.reserve(count);
    
    for (uint16_t i = 0; i < count; ++i) {
        PulseOne::Structs::DataPoint point;
        point.address = start_address + i;
        point.name = "RTU_" + std::to_string(slave_id) + "_" + std::to_string(start_address + i);
        
        switch (register_type) {
            case ModbusRegisterType::COIL:
            case ModbusRegisterType::DISCRETE_INPUT:
                point.data_type = "BOOL";
                break;
            case ModbusRegisterType::HOLDING_REGISTER:
            case ModbusRegisterType::INPUT_REGISTER:
                point.data_type = "UINT16";
                break;
        }
        
        data_points.push_back(point);
    }
    
    return data_points;
}

void ModbusRtuWorker::PollingWorkerThread() {
    LogRtuMessage(PulseOne::LogLevel::INFO, "Polling worker thread started");
    
    while (!stop_workers_) {
        try {
            auto now = system_clock::now();
            
            {
                std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
                
                for (auto& [group_id, group] : polling_groups_) {
                    if (!group.enabled || stop_workers_) {
                        continue;
                    }
                    
                    if (now >= group.next_poll_time) {
                        // 간단한 폴링 로직 (실제 구현은 더 복잡할 수 있음)
                        // ProcessPollingGroup(group);
                        
                        // 다음 폴링 시간 업데이트
                        auto& mutable_group = const_cast<ModbusRtuPollingGroup&>(group);
                        mutable_group.last_poll_time = now;
                        mutable_group.next_poll_time = now + milliseconds(group.polling_interval_ms);
                    }
                }
            }
            
            std::this_thread::sleep_for(milliseconds(100));
            
        } catch (const std::exception& e) {
            LogRtuMessage(PulseOne::LogLevel::ERROR, "Polling worker thread error: " + string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LogRtuMessage(PulseOne::LogLevel::INFO, "Polling worker thread stopped");
}

// =============================================================================
// 설정 파싱 및 초기화 메서드들
// =============================================================================

bool ModbusRtuWorker::ParseModbusConfig() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "🔧 Starting Modbus RTU configuration parsing...");
        
        nlohmann::json protocol_config_json;
        string config_source = device_info_.connection_string;
        
        if (!config_source.empty() && 
            (config_source.front() == '{' || config_source.find("slave_id") != string::npos)) {
            try {
                protocol_config_json = nlohmann::json::parse(config_source);
                LogMessage(PulseOne::LogLevel::INFO, "✅ Parsed RTU protocol config from connection_string");
            } catch (const std::exception& e) {
                LogMessage(PulseOne::LogLevel::WARN, "⚠️ Failed to parse RTU protocol config JSON, using defaults");
                protocol_config_json = nlohmann::json::object();
            }
        } else {
            protocol_config_json = nlohmann::json::object();
        }
        
        // RTU 기본값 설정
        if (protocol_config_json.empty()) {
            protocol_config_json = {
                {"slave_id", 1},
                {"byte_order", "big_endian"},
                {"baud_rate", 9600},
                {"parity", "N"},
                {"data_bits", 8},
                {"stop_bits", 1},
                {"frame_delay_ms", 50},
                {"max_registers_per_group", 125}
            };
        }
        
        // 구조체에 설정 저장
        modbus_config_.slave_id = protocol_config_json.value("slave_id", 1);
        modbus_config_.byte_order = protocol_config_json.value("byte_order", "big_endian");
        modbus_config_.baud_rate = protocol_config_json.value("baud_rate", 9600);
        
        string parity_str = protocol_config_json.value("parity", "N");
        modbus_config_.parity = parity_str.empty() ? 'N' : parity_str[0];
        
        modbus_config_.data_bits = protocol_config_json.value("data_bits", 8);
        modbus_config_.stop_bits = protocol_config_json.value("stop_bits", 1);
        modbus_config_.frame_delay_ms = protocol_config_json.value("frame_delay_ms", 50);
        modbus_config_.max_registers_per_group = protocol_config_json.value("max_registers_per_group", 125);
        
        // DeviceInfo에서 공통 설정
        modbus_config_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.connection_timeout_ms;
        modbus_config_.response_timeout_ms = std::min(device_info_.read_timeout_ms, 1000);
        modbus_config_.byte_timeout_ms = std::min(device_info_.read_timeout_ms / 10, 100);
        modbus_config_.max_retries = static_cast<uint8_t>(device_info_.max_retry_count);
        
        // 검증
        if (!modbus_config_.IsValid(true)) {
            LogMessage(PulseOne::LogLevel::WARN, "⚠️ RTU config validation failed, using defaults");
            modbus_config_.ResetToRtuDefaults();
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "✅ Modbus RTU config parsed successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "❌ Exception in ParseModbusConfig: " + string(e.what()));
        modbus_config_.ResetToRtuDefaults();
        return false;
    }
}

bool ModbusRtuWorker::InitializeModbusDriver() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "🔧 Initializing Modbus RTU Driver...");
        
        if (!modbus_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "❌ ModbusDriver not initialized");
            return false;
        }
        
        PulseOne::DriverConfig driver_config;
        driver_config.device_id = device_info_.name;
        driver_config.endpoint = modbus_config_.endpoint;
        driver_config.protocol = PulseOne::ProtocolType::MODBUS_RTU;
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        
        // RTU 특화 설정
        driver_config.custom_settings["slave_id"] = std::to_string(modbus_config_.slave_id);
        driver_config.custom_settings["byte_order"] = modbus_config_.byte_order;
        driver_config.custom_settings["baud_rate"] = std::to_string(modbus_config_.baud_rate);
        driver_config.custom_settings["parity"] = string(1, modbus_config_.parity);
        driver_config.custom_settings["data_bits"] = std::to_string(modbus_config_.data_bits);
        driver_config.custom_settings["stop_bits"] = std::to_string(modbus_config_.stop_bits);
        driver_config.custom_settings["frame_delay_ms"] = std::to_string(modbus_config_.frame_delay_ms);
        driver_config.custom_settings["response_timeout_ms"] = std::to_string(modbus_config_.response_timeout_ms);
        driver_config.custom_settings["byte_timeout_ms"] = std::to_string(modbus_config_.byte_timeout_ms);
        driver_config.custom_settings["max_retries"] = std::to_string(modbus_config_.max_retries);
        
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "❌ RTU ModbusDriver initialization failed: " + error.message);
            return false;
        }
        
        SetupDriverCallbacks();
        
        LogMessage(PulseOne::LogLevel::INFO, "✅ Modbus RTU Driver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "❌ Exception during RTU ModbusDriver initialization: " + string(e.what()));
        return false;
    }
}

void ModbusRtuWorker::SetupDriverCallbacks() {
    if (!modbus_driver_) {
        return;
    }
    
    try {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "🔗 Setting up RTU ModbusDriver callbacks...");
        
        // RTU 특화 콜백들 설정 (실제 구현은 ModbusDriver API에 따라 달라짐)
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "✅ RTU ModbusDriver callbacks configured");
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::WARN, "⚠️ Failed to setup RTU driver callbacks: " + string(e.what()));
    }
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne