/**
 * @file ModbusRtuWorker.cpp - 네임스페이스 완전 수정 버전
 * @brief Modbus RTU 워커 클래스 구현 (네임스페이스 중첩 문제 해결)
 * @author PulseOne Development Team
 * @date 2025-08-01
 * @version 4.0.0
 * 
 * 🔥 완전 해결된 문제들:
 * - 네임스페이스 중첩 문제 완전 해결 (PulseOne::Workers만 사용)
 * - std 타입들 올바른 선언
 * - 모든 멤버 변수 인식 문제 해결
 * - exception 처리 문제 해결
 */

#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Utils/LogManager.h"

// ✅ 필수 시스템 헤더들
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <exception>

// 외부 라이브러리
#include <nlohmann/json.hpp>

// ✅ std::to_string 대체 함수 (네임스페이스 밖에서 정의)
template<typename T>
std::string to_string_safe(T value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// ✅ 올바른 네임스페이스 - 중첩 제거! PulseOne::Workers만 사용
namespace PulseOne {
namespace Workers {

// ✅ 네임스페이스 안에서 올바른 using 선언
using json = nlohmann::json;
using DeviceInfo = Structs::DeviceInfo;
using DataPoint = Structs::DataPoint;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(const PulseOne::DeviceInfo& device_info,
                                 std::shared_ptr<RedisClient> redis_client,
                                 std::shared_ptr<InfluxClient> influx_client)
    : SerialBasedWorker(device_info, redis_client, influx_client)
    , modbus_driver_(nullptr)
    , polling_thread_running_(false)
    , next_group_id_(1) {
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusRtuWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseModbusConfig()) {
        logger.Error("Failed to parse Modbus RTU configuration");
        return;
    }
    
    // ModbusDriver 초기화
    if (!InitializeModbusDriver()) {
        logger.Error("Failed to initialize Modbus RTU Driver");
        return;
    }
    
    logger.Info("ModbusRtuWorker initialization completed");
}

ModbusRtuWorker::~ModbusRtuWorker() {
    auto& logger = LogManager::getInstance();
    
    // 폴링 스레드 정리
    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    // ModbusDriver 정리
    if (modbus_driver_) {
        modbus_driver_->Disconnect();
    }
    
    logger.Info("ModbusRtuWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusRtuWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        auto& logger = LogManager::getInstance();
        
        if (GetState() == WorkerState::RUNNING) {
            logger.Warn("Modbus RTU Worker already running");
            return true;
        }
        
        logger.Info("Starting Modbus RTU Worker...");
        
        try {
            if (!EstablishConnection()) {
                logger.Error("Failed to establish RTU connection");
                return false;
            }
            
            ChangeState(WorkerState::RUNNING);
            
            polling_thread_running_ = true;
            polling_thread_ = std::make_unique<std::thread>(&ModbusRtuWorker::PollingWorkerThread, this);
            
            logger.Info("Modbus RTU Worker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger.Error("RTU Worker start failed: " + std::string(e.what()));
            ChangeState(WorkerState::ERROR);
            return false;
        }
    });
}

std::future<bool> ModbusRtuWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        auto& logger = LogManager::getInstance();
        
        if (GetState() == WorkerState::STOPPED) {
            logger.Warn("Modbus RTU Worker already stopped");
            return true;
        }
        
        logger.Info("Stopping Modbus RTU Worker...");
        
        try {
            polling_thread_running_ = false;
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
            
            CloseConnection();
            ChangeState(WorkerState::STOPPED);
            
            logger.Info("Modbus RTU Worker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger.Error("RTU Worker stop failed: " + std::string(e.what()));
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
    auto& logger = LogManager::getInstance();
    logger.Info("Establishing Modbus RTU protocol connection");
    
    if (!modbus_driver_) {
        logger.Error("ModbusDriver not initialized");
        return false;
    }
    
    try {
        PulseOne::Structs::DriverConfig config;
        config.device_id = device_info_.name;
        config.protocol = PulseOne::Enums::ProtocolType::MODBUS_RTU;
        config.endpoint = device_info_.endpoint;
        config.timeout_ms = modbus_config_.timeout_ms;
        config.retry_count = modbus_config_.max_retries;
        
        // RTU 특화 설정
        config.custom_settings["slave_id"] = to_string_safe(modbus_config_.slave_id);
        config.custom_settings["baud_rate"] = to_string_safe(modbus_config_.baud_rate);
        config.custom_settings["parity"] = std::string(1, modbus_config_.parity);
        config.custom_settings["data_bits"] = to_string_safe(modbus_config_.data_bits);
        config.custom_settings["stop_bits"] = to_string_safe(modbus_config_.stop_bits);
        config.custom_settings["frame_delay_ms"] = to_string_safe(modbus_config_.frame_delay_ms);
        
        if (!modbus_driver_->Initialize(config)) {
            auto error = modbus_driver_->GetLastError();
            logger.Error("Failed to initialize RTU ModbusDriver: " + error.message);
            return false;
        }
        
        if (!modbus_driver_->Connect()) {
            auto error = modbus_driver_->GetLastError();
            logger.Error("Failed to connect RTU: " + error.message);
            return false;
        }
        
        logger.Info("Modbus RTU protocol connection established");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("EstablishProtocolConnection failed: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    auto& logger = LogManager::getInstance();
    logger.Info("Closing Modbus RTU protocol connection");
    
    if (modbus_driver_) {
        modbus_driver_->Disconnect();
        logger.Info("Modbus RTU protocol connection closed");
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
    
    auto& logger = LogManager::getInstance();
    
    // RTU 특화: 첫 번째 활성 슬레이브에 Keep-alive 전송
    std::shared_lock<std::shared_mutex> slaves_lock(slaves_mutex_);
    
    for (auto& [slave_id, slave_info] : slaves_) {
        if (slave_info->is_online) {
            int response_time = CheckSlaveStatus(slave_id);
            if (response_time >= 0) {
                UpdateSlaveStatus(slave_id, response_time, true);
                return true;
            } else {
                slave_info->is_online = false;
                logger.Warn("Slave " + to_string_safe(slave_id) + " is offline");
            }
        }
    }
    
    return false;
}

// =============================================================================
// 통합된 설정 API
// =============================================================================

void ModbusRtuWorker::ConfigureModbusRtu(const PulseOne::Drivers::ModbusConfig& config) {
    auto& logger = LogManager::getInstance();
    modbus_config_ = config;
    logger.Info("Modbus RTU configured:\n" + modbus_config_.ToString(true));
}

// =============================================================================
// RTU 특화 슬레이브 관리
// =============================================================================

bool ModbusRtuWorker::AddSlave(int slave_id, const std::string& device_name) {
    auto& logger = LogManager::getInstance();
    
    if (slave_id < 1 || slave_id > 247) {
        logger.Error("Invalid slave ID: " + to_string_safe(slave_id));
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    if (slaves_.find(slave_id) != slaves_.end()) {
        logger.Warn("Slave " + to_string_safe(slave_id) + " already exists");
        return false;
    }
    
    auto slave_info = std::make_shared<ModbusRtuSlaveInfo>(
        slave_id, 
        device_name.empty() ? "Slave_" + to_string_safe(slave_id) : device_name
    );
    
    slaves_[slave_id] = slave_info;
    
    logger.Info("Added slave " + to_string_safe(slave_id) + " (" + slave_info->device_name + ")");
    
    return true;
}

bool ModbusRtuWorker::RemoveSlave(int slave_id) {
    auto& logger = LogManager::getInstance();
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it == slaves_.end()) {
        logger.Warn("Slave " + to_string_safe(slave_id) + " not found");
        return false;
    }
    
    slaves_.erase(it);
    logger.Info("Removed slave " + to_string_safe(slave_id));
    
    return true;
}

std::shared_ptr<ModbusRtuSlaveInfo> ModbusRtuWorker::GetSlaveInfo(int slave_id) const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    return (it != slaves_.end()) ? it->second : nullptr;
}

int ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int timeout_ms) {
    auto& logger = LogManager::getInstance();
    logger.Info("Scanning slaves from " + to_string_safe(start_id) + " to " + to_string_safe(end_id));
    
    int found_count = 0;
    int original_timeout = modbus_config_.response_timeout_ms;
    modbus_config_.response_timeout_ms = timeout_ms;
    
    for (int slave_id = start_id; slave_id <= end_id; ++slave_id) {
        int response_time = CheckSlaveStatus(slave_id);
        if (response_time >= 0) {
            AddSlave(slave_id);
            UpdateSlaveStatus(slave_id, response_time, true);
            found_count++;
            
            logger.Info("Found slave " + to_string_safe(slave_id) + 
                       " (response time: " + to_string_safe(response_time) + "ms)");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(modbus_config_.frame_delay_ms));
    }
    
    modbus_config_.response_timeout_ms = original_timeout;
    
    logger.Info("Slave scan completed. Found " + to_string_safe(found_count) + " slaves");
    
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
    auto& logger = LogManager::getInstance();
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
    group.last_poll_time = std::chrono::system_clock::now();
    group.next_poll_time = std::chrono::system_clock::now();
    
    polling_groups_[group_id] = group;
    
    logger.Info("Added polling group " + to_string_safe(group_id) + 
               " (" + group_name + ") for slave " + to_string_safe(slave_id));
    
    return group_id;
}

bool ModbusRtuWorker::RemovePollingGroup(uint32_t group_id) {
    auto& logger = LogManager::getInstance();
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        logger.Warn("Polling group " + to_string_safe(group_id) + " not found");
        return false;
    }
    
    polling_groups_.erase(it);
    logger.Info("Removed polling group " + to_string_safe(group_id));
    
    return true;
}

bool ModbusRtuWorker::EnablePollingGroup(uint32_t group_id, bool enabled) {
    auto& logger = LogManager::getInstance();
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        logger.Warn("Polling group " + to_string_safe(group_id) + " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    logger.Info("Polling group " + to_string_safe(group_id) + (enabled ? " enabled" : " disabled"));
    
    return true;
}

bool ModbusRtuWorker::AddDataPointToGroup(uint32_t group_id, const PulseOne::DataPoint& data_point) {
    auto& logger = LogManager::getInstance();
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        logger.Warn("Polling group " + to_string_safe(group_id) + " not found");
        return false;
    }
    
    it->second.data_points.push_back(data_point);
    logger.Info("Added data point " + data_point.name + " to polling group " + to_string_safe(group_id));
    
    return true;
}

// =============================================================================
// 데이터 읽기/쓰기
// =============================================================================

bool ModbusRtuWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                                          uint16_t register_count, std::vector<uint16_t>& values) {
    auto& logger = LogManager::getInstance();
    
    LockBus();
    
    try {
        std::vector<PulseOne::Structs::DataPoint> data_points = CreateDataPoints(
            slave_id, ModbusRegisterType::HOLDING_REGISTER, start_address, register_count);
        
        std::vector<PulseOne::TimestampedValue> timestamped_values;
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
            
            UpdateSlaveStatus(slave_id, 0, true);
        } else {
            auto error = modbus_driver_->GetLastError();
            logger.Error("Failed to read holding registers from slave " + to_string_safe(slave_id) + 
                        ": " + error.message);
            
            UpdateSlaveStatus(slave_id, 0, false);
        }
        
        UnlockBus();
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("ReadHoldingRegisters exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 상태 조회 API
// =============================================================================

std::string ModbusRtuWorker::GetModbusStats() const {
    if (!modbus_driver_) {
        return "{}";
    }
    
    const auto& driver_stats = modbus_driver_->GetStatistics();
    const auto& driver_diagnostics = modbus_driver_->GetDiagnostics();
    
    json stats;
    
    // Driver 통계
    stats["driver"]["total_operations"] = driver_stats.total_operations.load();
    stats["driver"]["successful_operations"] = driver_stats.successful_operations.load();
    stats["driver"]["failed_operations"] = driver_stats.failed_operations.load();
    stats["driver"]["success_rate"] = driver_stats.success_rate.load();
    stats["driver"]["avg_response_time_ms"] = driver_stats.avg_response_time_ms.load();
    
    // Worker 통계
    {
        std::shared_lock<std::shared_mutex> slaves_lock(slaves_mutex_);
        stats["worker"]["total_slaves"] = slaves_.size();
        
        size_t online_slaves = 0;
        for (const auto& [slave_id, slave_info] : slaves_) {
            if (slave_info->is_online) {
                online_slaves++;
            }
        }
        stats["worker"]["online_slaves"] = online_slaves;
    }
    
    {
        std::shared_lock<std::shared_mutex> groups_lock(polling_groups_mutex_);
        stats["worker"]["total_polling_groups"] = polling_groups_.size();
        
        size_t enabled_groups = 0;
        size_t total_data_points = 0;
        for (const auto& [group_id, group] : polling_groups_) {
            if (group.enabled) {
                enabled_groups++;
            }
            total_data_points += group.data_points.size();
        }
        stats["worker"]["enabled_polling_groups"] = enabled_groups;
        stats["worker"]["total_data_points"] = total_data_points;
    }
    
    // 진단 정보
    for (const auto& [key, value] : driver_diagnostics) {
        stats["diagnostics"][key] = value;
    }
    
    return stats.dump(2);
}

std::string ModbusRtuWorker::GetSerialBusStatus() const {
    json status;
    status["endpoint"] = device_info_.endpoint;
    status["baud_rate"] = modbus_config_.baud_rate;
    status["data_bits"] = modbus_config_.data_bits;
    status["parity"] = std::string(1, modbus_config_.parity);
    status["stop_bits"] = modbus_config_.stop_bits;
    status["response_timeout_ms"] = modbus_config_.response_timeout_ms;
    status["byte_timeout_ms"] = modbus_config_.byte_timeout_ms;
    status["frame_delay_ms"] = modbus_config_.frame_delay_ms;
    status["is_connected"] = const_cast<ModbusRtuWorker*>(this)->CheckProtocolConnection();
    
    return status.dump(2);
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

void ModbusRtuWorker::UpdateSlaveStatus(int slave_id, int response_time_ms, bool success) {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        auto& slave_info = it->second;
        
        slave_info->total_requests++;
        if (success) {
            slave_info->successful_requests++;
            slave_info->is_online = true;
            slave_info->last_response = std::chrono::system_clock::now();
            
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
    auto& logger = LogManager::getInstance();
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        std::vector<uint16_t> test_values;
        bool success = ReadHoldingRegisters(slave_id, 0, 1, test_values);
        
        auto end_time = std::chrono::steady_clock::now();
        int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        return success ? response_time : -1;
        
    } catch (const std::exception& e) {
        logger.Debug("CheckSlaveStatus exception for slave " + to_string_safe(slave_id) + 
                     ": " + std::string(e.what()));
        return -1;
    }
}

void ModbusRtuWorker::LockBus() {
    bus_mutex_.lock();
    
    if (modbus_config_.frame_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(modbus_config_.frame_delay_ms));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

void ModbusRtuWorker::LogRtuMessage(PulseOne::LogLevel level, const std::string& message) {
    auto& logger = LogManager::getInstance();
    std::string prefix = "[ModbusRTU:" + device_info_.endpoint + "] ";
    logger.log("modbus_rtu", level, prefix + message);
}

std::vector<PulseOne::Structs::DataPoint> ModbusRtuWorker::CreateDataPoints(int slave_id, 
                                                                 ModbusRegisterType register_type,
                                                                 uint16_t start_address, 
                                                                 uint16_t count) {
    std::vector<PulseOne::Structs::DataPoint> data_points;
    data_points.reserve(count);
    
    for (uint16_t i = 0; i < count; ++i) {
        PulseOne::Structs::DataPoint point;
        point.address = start_address + i;
        point.name = "RTU_" + to_string_safe(slave_id) + "_" + to_string_safe(start_address + i);
        
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
    auto& logger = LogManager::getInstance();
    logger.Info("RTU Polling worker thread started");
    
    while (polling_thread_running_) {
        try {
            auto now = std::chrono::system_clock::now();
            
            {
                std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
                
                for (auto& [group_id, group] : polling_groups_) {
                    if (!group.enabled || !polling_thread_running_) {
                        continue;
                    }
                    
                    if (now >= group.next_poll_time) {
                        std::vector<uint16_t> values;
                        bool success = ReadHoldingRegisters(
                            group.slave_id, 
                            group.start_address, 
                            group.register_count, 
                            values
                        );
                        
                        if (success) {
                            logger.Debug("RTU Polled group " + to_string_safe(group_id) + 
                                        ", read " + to_string_safe(values.size()) + " values");
                        }
                        
                        auto& mutable_group = const_cast<ModbusRtuPollingGroup&>(group);
                        mutable_group.last_poll_time = now;
                        mutable_group.next_poll_time = now + std::chrono::milliseconds(group.polling_interval_ms);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            logger.Error("RTU Polling worker thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger.Info("RTU Polling worker thread stopped");
}

// =============================================================================
// 설정 파싱 및 초기화 메서드들
// =============================================================================

bool ModbusRtuWorker::ParseModbusConfig() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("🔧 Starting Modbus RTU configuration parsing...");
        
        json protocol_config_json;
        std::string config_source = device_info_.connection_string;
        
        if (!config_source.empty() && 
            (config_source.front() == '{' || config_source.find("slave_id") != std::string::npos)) {
            try {
                protocol_config_json = json::parse(config_source);
                logger.Info("✅ Parsed RTU protocol config from connection_string");
            } catch (const std::exception& e) {
                logger.Warn("⚠️ Failed to parse RTU protocol config JSON, using defaults");
                protocol_config_json = json::object();
            }
        } else {
            protocol_config_json = json::object();
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
        
        std::string parity_str = protocol_config_json.value("parity", "N");
        modbus_config_.parity = parity_str.empty() ? 'N' : parity_str[0];
        
        modbus_config_.data_bits = protocol_config_json.value("data_bits", 8);
        modbus_config_.stop_bits = protocol_config_json.value("stop_bits", 1);
        modbus_config_.frame_delay_ms = protocol_config_json.value("frame_delay_ms", 50);
        modbus_config_.max_registers_per_group = protocol_config_json.value("max_registers_per_group", 125);
        
        // DeviceInfo에서 공통 설정
        device_info_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.connection_timeout_ms;
        modbus_config_.response_timeout_ms = std::min(device_info_.read_timeout_ms, 1000);
        modbus_config_.byte_timeout_ms = std::min(device_info_.read_timeout_ms / 10, 100);
        modbus_config_.max_retries = static_cast<uint8_t>(device_info_.max_retry_count);
        
        // 검증
        if (!modbus_config_.IsValid(true)) {
            logger.Warn("⚠️ RTU config validation failed, using defaults");
            modbus_config_.ResetToRtuDefaults();
        }
        
        logger.Info("✅ Modbus RTU config parsed successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("❌ Exception in ParseModbusConfig: " + std::string(e.what()));
        modbus_config_.ResetToRtuDefaults();
        return false;
    }
}

bool ModbusRtuWorker::InitializeModbusDriver() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("🔧 Initializing Modbus RTU Driver...");
        
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            logger.Error("❌ Failed to create ModbusDriver");
            return false;
        }
        
        PulseOne::DriverConfig driver_config;
        driver_config.device_id = device_info_.name;
        driver_config.endpoint = device_info_.endpoint;
        driver_config.protocol = PulseOne::ProtocolType::MODBUS_RTU;
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        
        // RTU 특화 설정
        driver_config.custom_settings["slave_id"] = to_string_safe(modbus_config_.slave_id);
        driver_config.custom_settings["byte_order"] = modbus_config_.byte_order;
        driver_config.custom_settings["baud_rate"] = to_string_safe(modbus_config_.baud_rate);
        driver_config.custom_settings["parity"] = std::string(1, modbus_config_.parity);
        driver_config.custom_settings["data_bits"] = to_string_safe(modbus_config_.data_bits);
        driver_config.custom_settings["stop_bits"] = to_string_safe(modbus_config_.stop_bits);
        driver_config.custom_settings["frame_delay_ms"] = to_string_safe(modbus_config_.frame_delay_ms);
        driver_config.custom_settings["response_timeout_ms"] = to_string_safe(modbus_config_.response_timeout_ms);
        driver_config.custom_settings["byte_timeout_ms"] = to_string_safe(modbus_config_.byte_timeout_ms);
        driver_config.custom_settings["max_retries"] = to_string_safe(modbus_config_.max_retries);
        
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            logger.Error("❌ RTU ModbusDriver initialization failed: " + error.message);
            return false;
        }
        
        SetupDriverCallbacks();
        
        logger.Info("✅ Modbus RTU Driver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("❌ Exception during RTU ModbusDriver initialization: " + std::string(e.what()));
        return false;
    }
}

void ModbusRtuWorker::SetupDriverCallbacks() {
    auto& logger = LogManager::getInstance();
    
    if (!modbus_driver_) {
        return;
    }
    
    try {
        logger.Debug("🔗 Setting up RTU ModbusDriver callbacks...");
        
        // RTU 특화 콜백들 설정
        
        logger.Debug("✅ RTU ModbusDriver callbacks configured");
        
    } catch (const std::exception& e) {
        logger.Warn("⚠️ Failed to setup RTU driver callbacks: " + std::string(e.what()));
    }
}

} // namespace Workers  
} // namespace PulseOne