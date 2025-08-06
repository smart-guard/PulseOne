/**
 * @file ModbusRtuWorker.cpp - const 한정자 문제 완전 해결 버전
 * @brief Modbus RTU 워커 클래스 구현 (모든 const 문제 해결)
 * @author PulseOne Development Team
 * @date 2025-08-06
 * @version 6.0.0 - const 완전 해결
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

// ✅ 헬퍼 함수들
template<typename T>
std::string to_string_safe(T value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// ✅ 네임스페이스
namespace PulseOne {
namespace Workers {

// 네임스페이스 안에서 using 선언
using json = nlohmann::json;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(const DeviceInfo& device_info)
    : SerialBasedWorker(device_info)
    , next_group_id_(1)
    , polling_thread_running_(false) {
    
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusRtuWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseModbusConfig()) {
        logger.Error("Failed to parse Modbus RTU configuration");
        return;
    }
    
    // ModbusDriver 초기화 (실제로는 구현되지 않았으므로 스킵)
    if (!InitializeModbusDriver()) {
        logger.Warn("ModbusDriver not implemented yet, continuing without driver");
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
            if (!EstablishProtocolConnection()) {
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
            
            CloseProtocolConnection();
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
    
    // 🔥 실제 구현이 없으므로 임시로 성공 반환
    logger.Info("RTU connection established (mock)");
    return true;
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    auto& logger = LogManager::getInstance();
    logger.Info("Closing Modbus RTU protocol connection");
    return true;
}

bool ModbusRtuWorker::CheckProtocolConnection() {
    // 실제 연결 상태 확인 로직
    return true; // 임시로 항상 연결된 것으로 간주
}

bool ModbusRtuWorker::SendProtocolKeepAlive() {
    auto& logger = LogManager::getInstance();
    
    // RTU 특화: 첫 번째 활성 슬레이브에 Keep-alive 전송
    std::shared_lock<std::shared_mutex> slaves_lock(slaves_mutex_);
    
    for (auto& [slave_id, slave_info] : slaves_) {
        bool is_online = (GetPropertyValue(slave_info->properties, "is_online", "false") == "true");
        
        if (is_online) {
            int response_time = CheckSlaveStatus(slave_id);
            if (response_time >= 0) {
                UpdateSlaveStatus(slave_id, response_time, true);
                return true;
            } else {
                slave_info->properties["is_online"] = "false";
                logger.Warn("Slave " + to_string_safe(slave_id) + " is offline");
            }
        }
    }
    
    return false;
}

// =============================================================================
// 설정 API
// =============================================================================

void ModbusRtuWorker::ConfigureModbusRtu(const PulseOne::Structs::DriverConfig& config) {
    auto& logger = LogManager::getInstance();
    modbus_config_ = config;
    logger.Info("Modbus RTU configured for device: " + config.name);
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
    
    auto slave_info = std::make_shared<DeviceInfo>();
    slave_info->id = to_string_safe(slave_id);
    slave_info->name = device_name.empty() ? ("Slave_" + to_string_safe(slave_id)) : device_name;
    
    // properties에 추가 정보 저장
    slave_info->properties["slave_id"] = std::to_string(slave_id);
    slave_info->properties["is_online"] = "false";
    slave_info->properties["total_requests"] = "0";
    slave_info->properties["successful_requests"] = "0";
    slave_info->properties["crc_errors"] = "0";
    slave_info->properties["timeout_errors"] = "0";
    slave_info->properties["response_time_ms"] = "0";
    slave_info->properties["last_error"] = "";
    
    slaves_[slave_id] = slave_info;
    
    logger.Info("Added slave " + to_string_safe(slave_id) + " (" + slave_info->name + ")");
    
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

std::shared_ptr<DeviceInfo> ModbusRtuWorker::GetSlaveInfo(int slave_id) const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    return (it != slaves_.end()) ? it->second : nullptr;
}

int ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int /* timeout_ms */) {
    auto& logger = LogManager::getInstance();
    logger.Info("Scanning slaves from " + to_string_safe(start_id) + " to " + to_string_safe(end_id));
    
    int found_count = 0;
    
    for (int slave_id = start_id; slave_id <= end_id; ++slave_id) {
        int response_time = CheckSlaveStatus(slave_id);
        if (response_time >= 0) {
            AddSlave(slave_id);
            UpdateSlaveStatus(slave_id, response_time, true);
            found_count++;
            
            logger.Info("Found slave " + to_string_safe(slave_id) + 
                       " (response time: " + to_string_safe(response_time) + "ms)");
        }
        
        // 프레임 지연
        int frame_delay = GetFrameDelay();
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay));
    }
    
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

bool ModbusRtuWorker::AddDataPointToGroup(uint32_t group_id, const DataPoint& data_point) {
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
// 데이터 읽기/쓰기 (임시 구현)
// =============================================================================

bool ModbusRtuWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                                          uint16_t register_count, std::vector<uint16_t>& values) {
    auto& logger = LogManager::getInstance();
    
    LockBus();
    
    try {
        // 🔥 실제 ModbusDriver가 없으므로 임시 구현
        values.clear();
        values.resize(register_count, 0);  // 0으로 초기화된 값들
        
        // 임시로 랜덤한 값들 생성
        for (auto& value : values) {
            value = rand() % 65536;
        }
        
        UpdateSlaveStatus(slave_id, 50, true);  // 50ms 응답시간으로 설정
        
        UnlockBus();
        
        logger.Debug("Read " + to_string_safe(register_count) + " holding registers from slave " + 
                    to_string_safe(slave_id) + " starting at " + to_string_safe(start_address));
        
        return true;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("ReadHoldingRegisters exception: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::ReadInputRegisters(int slave_id, uint16_t start_address, 
                                        uint16_t register_count, std::vector<uint16_t>& values) {
    // ReadHoldingRegisters와 유사한 구현
    return ReadHoldingRegisters(slave_id, start_address, register_count, values);
}

bool ModbusRtuWorker::ReadCoils(int slave_id, uint16_t start_address, 
                               uint16_t coil_count, std::vector<bool>& values) {
    auto& logger = LogManager::getInstance();
    
    values.clear();
    values.resize(coil_count, false);
    
    // 🔥 std::vector<bool> 특수 처리 - auto& 대신 값으로 처리
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = (rand() % 2) == 1;
    }
    
    logger.Debug("Read " + to_string_safe(coil_count) + " coils from slave " + 
                to_string_safe(slave_id) + " starting at " + to_string_safe(start_address));
    
    return true;
}

bool ModbusRtuWorker::ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                                        uint16_t input_count, std::vector<bool>& values) {
    return ReadCoils(slave_id, start_address, input_count, values);
}

bool ModbusRtuWorker::WriteSingleRegister(int slave_id, uint16_t address, uint16_t value) {
    auto& logger = LogManager::getInstance();
    logger.Info("Write single register: slave=" + to_string_safe(slave_id) + 
               ", address=" + to_string_safe(address) + ", value=" + to_string_safe(value));
    return true;
}

bool ModbusRtuWorker::WriteSingleCoil(int slave_id, uint16_t address, bool value) {
    auto& logger = LogManager::getInstance();
    logger.Info("Write single coil: slave=" + to_string_safe(slave_id) + 
               ", address=" + to_string_safe(address) + ", value=" + (value ? "true" : "false"));
    return true;
}

bool ModbusRtuWorker::WriteMultipleRegisters(int slave_id, uint16_t start_address, 
                                            const std::vector<uint16_t>& values) {
    auto& logger = LogManager::getInstance();
    logger.Info("Write multiple registers: slave=" + to_string_safe(slave_id) + 
               ", start=" + to_string_safe(start_address) + ", count=" + to_string_safe(values.size()));
    return true;
}

bool ModbusRtuWorker::WriteMultipleCoils(int slave_id, uint16_t start_address, 
                                        const std::vector<bool>& values) {
    auto& logger = LogManager::getInstance();
    logger.Info("Write multiple coils: slave=" + to_string_safe(slave_id) + 
               ", start=" + to_string_safe(start_address) + ", count=" + to_string_safe(values.size()));
    return true;
}

// =============================================================================
// 상태 조회 API
// =============================================================================

std::string ModbusRtuWorker::GetModbusStats() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"worker_type\": \"ModbusRtuWorker\",\n";
    oss << "  \"device_id\": \"" << device_info_.id << "\",\n";
    oss << "  \"device_name\": \"" << device_info_.name << "\",\n";
    oss << "  \"endpoint\": \"" << device_info_.endpoint << "\",\n";
    oss << "  \"worker_info\": {\n";
    oss << "    \"polling_thread_running\": " << (polling_thread_running_.load() ? "true" : "false") << ",\n";
    oss << "    \"next_group_id\": " << next_group_id_ << ",\n";
    oss << "    \"slave_count\": " << slaves_.size() << ",\n";
    oss << "    \"polling_group_count\": " << polling_groups_.size() << "\n";
    oss << "  }\n";
    oss << "}";
    
    return oss.str();
}

std::string ModbusRtuWorker::GetSerialBusStatus() const {
    json status;
    status["endpoint"] = device_info_.endpoint;
    status["baud_rate"] = GetBaudRate();
    status["data_bits"] = GetDataBits();
    status["parity"] = std::string(1, GetParity());
    status["stop_bits"] = GetStopBits();
    status["response_timeout_ms"] = GetResponseTimeout();
    status["byte_timeout_ms"] = GetByteTimeout();
    status["frame_delay_ms"] = GetFrameDelay();
    status["is_connected"] = const_cast<ModbusRtuWorker*>(this)->CheckProtocolConnection();
    
    return status.dump(2);
}

std::string ModbusRtuWorker::GetSlaveStatusList() const {
    json slaves_json = json::array();
    
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    for (const auto& [slave_id, slave_info] : slaves_) {
        json slave_json;
        slave_json["slave_id"] = slave_id;
        slave_json["name"] = slave_info->name;
        slave_json["is_online"] = GetPropertyValue(slave_info->properties, "is_online", "false");
        slave_json["total_requests"] = GetPropertyValue(slave_info->properties, "total_requests", "0");
        slave_json["successful_requests"] = GetPropertyValue(slave_info->properties, "successful_requests", "0");
        slave_json["response_time_ms"] = GetPropertyValue(slave_info->properties, "response_time_ms", "0");
        slaves_json.push_back(slave_json);
    }
    
    return slaves_json.dump(2);
}

std::string ModbusRtuWorker::GetPollingGroupStatus() const {
    json groups_json = json::array();
    
    std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
    for (const auto& [group_id, group] : polling_groups_) {
        json group_json;
        group_json["group_id"] = group_id;
        group_json["group_name"] = group.group_name;
        group_json["slave_id"] = group.slave_id;
        group_json["register_type"] = static_cast<int>(group.register_type);
        group_json["start_address"] = group.start_address;
        group_json["register_count"] = group.register_count;
        group_json["polling_interval_ms"] = group.polling_interval_ms;
        group_json["enabled"] = group.enabled;
        group_json["data_point_count"] = group.data_points.size();
        groups_json.push_back(group_json);
    }
    
    return groups_json.dump(2);
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

void ModbusRtuWorker::UpdateSlaveStatus(int slave_id, int response_time_ms, bool success) {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        auto& slave_info = it->second;
        
        int total_requests = std::stoi(GetPropertyValue(slave_info->properties, "total_requests", "0")) + 1;
        slave_info->properties["total_requests"] = std::to_string(total_requests);
        
        if (success) {
            int successful_requests = std::stoi(GetPropertyValue(slave_info->properties, "successful_requests", "0")) + 1;
            slave_info->properties["successful_requests"] = std::to_string(successful_requests);
            slave_info->properties["is_online"] = "true";
            
            if (response_time_ms > 0) {
                int current_avg = std::stoi(GetPropertyValue(slave_info->properties, "response_time_ms", "0"));
                int new_avg = (current_avg * 7 + response_time_ms) / 8;  // 이동 평균
                slave_info->properties["response_time_ms"] = std::to_string(new_avg);
            }
            
            slave_info->properties["last_error"] = "";
        } else {
            slave_info->properties["is_online"] = "false";
            slave_info->properties["last_error"] = "Communication failed";
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
    
    int frame_delay = GetFrameDelay();
    if (frame_delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

std::vector<DataPoint> ModbusRtuWorker::CreateDataPoints(int slave_id, 
                                                        ModbusRegisterType register_type,
                                                        uint16_t start_address, 
                                                        uint16_t count) {
    std::vector<DataPoint> data_points;
    data_points.reserve(count);
    
    for (uint16_t i = 0; i < count; ++i) {
        DataPoint point;
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
                        
                        // const 캐스팅으로 수정
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
        logger.Info("Starting Modbus RTU configuration parsing...");
        
        // DriverConfig 기본값 설정
        modbus_config_.device_id = device_info_.id;
        modbus_config_.name = device_info_.name;
        modbus_config_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.timeout_ms;
        modbus_config_.retry_count = device_info_.retry_count;
        
        // RTU 기본값들을 properties에 설정
        if (modbus_config_.properties.find("slave_id") == modbus_config_.properties.end()) {
            modbus_config_.properties["slave_id"] = "1";
        }
        if (modbus_config_.properties.find("baud_rate") == modbus_config_.properties.end()) {
            modbus_config_.properties["baud_rate"] = "9600";
        }
        if (modbus_config_.properties.find("parity") == modbus_config_.properties.end()) {
            modbus_config_.properties["parity"] = "N";
        }
        if (modbus_config_.properties.find("data_bits") == modbus_config_.properties.end()) {
            modbus_config_.properties["data_bits"] = "8";
        }
        if (modbus_config_.properties.find("stop_bits") == modbus_config_.properties.end()) {
            modbus_config_.properties["stop_bits"] = "1";
        }
        if (modbus_config_.properties.find("frame_delay_ms") == modbus_config_.properties.end()) {
            modbus_config_.properties["frame_delay_ms"] = "50";
        }
        if (modbus_config_.properties.find("response_timeout_ms") == modbus_config_.properties.end()) {
            modbus_config_.properties["response_timeout_ms"] = "1000";
        }
        if (modbus_config_.properties.find("byte_timeout_ms") == modbus_config_.properties.end()) {
            modbus_config_.properties["byte_timeout_ms"] = "100";
        }
        if (modbus_config_.properties.find("max_retries") == modbus_config_.properties.end()) {
            modbus_config_.properties["max_retries"] = "3";
        }
        
        logger.Info("Modbus RTU config parsed successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("Exception in ParseModbusConfig: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::InitializeModbusDriver() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("Initializing Modbus RTU Driver...");
        
        // 🔥 실제 ModbusDriver 구현이 없으므로 경고 후 성공 반환
        logger.Warn("ModbusDriver implementation not available, using mock driver");
        
        SetupDriverCallbacks();
        
        logger.Info("Modbus RTU Driver initialized successfully (mock)");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("Exception during RTU ModbusDriver initialization: " + std::string(e.what()));
        return false;
    }
}

void ModbusRtuWorker::SetupDriverCallbacks() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Debug("Setting up RTU ModbusDriver callbacks...");
        
        // RTU 특화 콜백들 설정 (실제 구현 시)
        
        logger.Debug("RTU ModbusDriver callbacks configured");
        
    } catch (const std::exception& e) {
        logger.Warn("Failed to setup RTU driver callbacks: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 const 헬퍼 메서드 (가장 중요!)
// =============================================================================

std::string ModbusRtuWorker::GetPropertyValue(const std::map<std::string, std::string>& properties, 
                                            const std::string& key, 
                                            const std::string& default_value) const {
    auto it = properties.find(key);
    return (it != properties.end()) ? it->second : default_value;
}

} // namespace Workers  
} // namespace PulseOne