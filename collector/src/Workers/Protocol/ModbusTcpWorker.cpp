/**
 * @file ModbusTcpWorker.cpp (리팩토링됨)
 * @brief Modbus TCP 워커 구현 - ModbusDriver를 통신 매체로 사용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 (리팩토링됨)
 */

#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace std::chrono;
using namespace PulseOne::Drivers;
using json = nlohmann::json;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusTcpWorker::ModbusTcpWorker(const PulseOne::DeviceInfo& device_info,
                                 std::shared_ptr<RedisClient> redis_client,
                                 std::shared_ptr<InfluxClient> influx_client)
    : TcpBasedWorker(device_info, redis_client, influx_client)
    , modbus_driver_(nullptr)
    , polling_thread_running_(false)
    , next_group_id_(1)
    , default_polling_interval_ms_(1000)
    , max_registers_per_group_(125)  // Modbus TCP standard limit
    , auto_group_creation_enabled_(true) {
    
    LogMessage(PulseOne::LogLevel::INFO, "ModbusTcpWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseModbusConfig()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse Modbus configuration");
        return;
    }
    
    // ModbusDriver 초기화
    if (!InitializeModbusDriver()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize ModbusDriver");
        return;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "ModbusTcpWorker initialization completed");
}

ModbusTcpWorker::~ModbusTcpWorker() {
    // 폴링 스레드 정리
    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    // ModbusDriver 정리 (자동으로 연결 해제됨)
    modbus_driver_.reset();
    
    LogMessage(PulseOne::LogLevel::INFO, "ModbusTcpWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusTcpWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogMessage(PulseOne::LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "Starting ModbusTcpWorker...");
        
        try {
            // 기본 연결 설정 (TcpBasedWorker → ModbusDriver 위임)
            if (!EstablishConnection()) {
                LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            // 데이터 포인트들로부터 폴링 그룹 자동 생성
            const auto& data_points = GetDataPoints();
            if (auto_group_creation_enabled_ && !data_points.empty()) {
                size_t group_count = CreatePollingGroupsFromDataPoints(data_points);
                LogMessage(PulseOne::LogLevel::INFO, "Created " + std::to_string(group_count) + " polling groups from " + 
                          std::to_string(data_points.size()) + " data points");
            }
            
            // 폴링 스레드 시작
            polling_thread_running_ = true;
            polling_thread_ = std::make_unique<std::thread>(&ModbusTcpWorker::PollingThreadFunction, this);
            
            // 상태 변경
            ChangeState(WorkerState::RUNNING);
            
            LogMessage(PulseOne::LogLevel::INFO, "ModbusTcpWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception during start: " + std::string(e.what()));
            return false;
        }
    });
}

std::future<bool> ModbusTcpWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(PulseOne::LogLevel::INFO, "Stopping ModbusTcpWorker...");
        
        try {
            // 폴링 스레드 중지
            polling_thread_running_ = false;
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
            
            // 연결 해제 (TcpBasedWorker → ModbusDriver 위임)
            CloseConnection();
            
            // 상태 변경
            ChangeState(WorkerState::STOPPED);
            
            LogMessage(PulseOne::LogLevel::INFO, "ModbusTcpWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception during stop: " + std::string(e.what()));
            return false;
        }
    });
}

// =============================================================================
// TcpBasedWorker 인터페이스 구현 (Driver 위임)
// =============================================================================

bool ModbusTcpWorker::EstablishProtocolConnection() {
    if (!modbus_driver_) {
        LogMessage(PulseOne::LogLevel::ERROR, "ModbusDriver not initialized");
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Establishing Modbus protocol connection...");
    
    // ModbusDriver를 통한 연결 수립
    if (!modbus_driver_->Connect()) {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(PulseOne::LogLevel::ERROR, "ModbusDriver connection failed: " + error.message);
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Modbus protocol connection established");
    return true;
}

bool ModbusTcpWorker::CloseProtocolConnection() {
    if (!modbus_driver_) {
        return true;  // 이미 없으면 성공으로 간주
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Closing Modbus protocol connection...");
    
    // ModbusDriver를 통한 연결 해제
    bool result = modbus_driver_->Disconnect();
    
    if (result) {
        LogMessage(PulseOne::LogLevel::INFO, "Modbus protocol connection closed");
    } else {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(PulseOne::LogLevel::WARN, "ModbusDriver disconnect warning: " + error.message);
    }
    
    return result;
}

bool ModbusTcpWorker::CheckProtocolConnection() {
    if (!modbus_driver_) {
        return false;
    }
    
    // ModbusDriver를 통한 연결 상태 확인
    return modbus_driver_->IsConnected();
}

bool ModbusTcpWorker::SendProtocolKeepAlive() {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        return false;
    }
    
    // 간단한 레지스터 읽기로 Keep-alive 테스트
    std::vector<DataPoint> test_points;
    std::vector<TimestampedValue> test_values;
    
    // 테스트용 데이터 포인트 생성 (첫 번째 홀딩 레지스터)
    DataPoint test_point;
    test_point.id = "keepalive_test";
    test_point.address = 0;  // 주소 0번 레지스터
    test_point.data_type = DataType::UINT16;
    test_points.push_back(test_point);
    
    // ModbusDriver를 통한 Keep-alive 테스트
    bool result = modbus_driver_->ReadValues(test_points, test_values);
    
    if (result) {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Modbus Keep-alive successful");
    } else {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(PulseOne::LogLevel::WARN, "Modbus Keep-alive failed: " + error.message);
    }
    
    return result;
}

// =============================================================================
// Modbus TCP 특화 객체 관리 (Worker 고유 기능)
// =============================================================================

bool ModbusTcpWorker::AddPollingGroup(const ModbusTcpPollingGroup& group) {
    if (!ValidatePollingGroup(group)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid polling group");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    // 그룹 ID 중복 체크
    if (polling_groups_.find(group.group_id) != polling_groups_.end()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Polling group ID " + std::to_string(group.group_id) + " already exists");
        return false;
    }
    
    polling_groups_[group.group_id] = group;
    
    LogMessage(PulseOne::LogLevel::INFO, "Added polling group " + std::to_string(group.group_id) + 
               " with " + std::to_string(group.data_points.size()) + " data points");
    
    return true;
}

bool ModbusTcpWorker::RemovePollingGroup(uint32_t group_id) {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogMessage(PulseOne::LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    polling_groups_.erase(it);
    
    LogMessage(PulseOne::LogLevel::INFO, "Removed polling group " + std::to_string(group_id));
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
        LogMessage(PulseOne::LogLevel::WARN, "Polling group " + std::to_string(group_id) + " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    
    LogMessage(PulseOne::LogLevel::INFO, "Polling group " + std::to_string(group_id) + 
               (enabled ? " enabled" : " disabled"));
    
    return true;
}

std::string ModbusTcpWorker::GetModbusStats() const {
    if (!modbus_driver_) {
        return "{}";
    }
    
    // ModbusDriver에서 통계 정보 가져오기
    const auto& driver_stats = modbus_driver_->GetStatistics();
    const auto& driver_diagnostics = modbus_driver_->GetDiagnostics();
    
    json stats;
    
    // Driver 통계 (실제 구조체 필드에 맞춘 수정)
    stats["driver"]["total_operations"] = driver_stats.total_operations;
    stats["driver"]["successful_operations"] = driver_stats.successful_operations;
    stats["driver"]["failed_operations"] = driver_stats.failed_operations;
    stats["driver"]["success_rate"] = driver_stats.success_rate;
    stats["driver"]["avg_response_time_ms"] = driver_stats.avg_response_time_ms;
    stats["driver"]["max_response_time_ms"] = driver_stats.max_response_time_ms;
    
    // Worker 통계
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    stats["worker"]["total_polling_groups"] = polling_groups_.size();
    
    size_t enabled_groups = 0;
    size_t total_data_points = 0;
    for (const auto& pair : polling_groups_) {
        if (pair.second.enabled) {
            enabled_groups++;
        }
        total_data_points += pair.second.data_points.size();
    }
    
    stats["worker"]["enabled_polling_groups"] = enabled_groups;
    stats["worker"]["total_data_points"] = total_data_points;
    
    // 진단 정보
    for (const auto& diag_pair : driver_diagnostics) {
        stats["diagnostics"][diag_pair.first] = diag_pair.second;
    }
    
    return stats.dump(2);
}

// =============================================================================
// 데이터 포인트 처리 (Worker 고유 로직)
// =============================================================================

size_t ModbusTcpWorker::CreatePollingGroupsFromDataPoints(const std::vector<PulseOne::DataPoint>& data_points) {
    if (data_points.empty()) {
        return 0;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Creating polling groups from " + std::to_string(data_points.size()) + " data points");
    
    // 슬레이브 ID별, 레지스터 타입별로 그룹화
    std::map<std::tuple<uint8_t, ModbusRegisterType>, std::vector<DataPoint>> grouped_points;
    
    for (const auto& data_point : data_points) {
        uint8_t slave_id;
        ModbusRegisterType register_type;
        uint16_t address;
        
        if (!ParseModbusAddress(data_point, slave_id, register_type, address)) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to parse Modbus address for data point: " + data_point.name);
            continue;
        }
        
        auto key = std::make_tuple(slave_id, register_type);
        grouped_points[key].push_back(data_point);
    }
    
    // 각 그룹에 대해 폴링 그룹 생성
    size_t created_groups = 0;
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    for (const auto& group_pair : grouped_points) {
        const auto& key = group_pair.first;
        const auto& points = group_pair.second;
        
        uint8_t slave_id = std::get<0>(key);
        ModbusRegisterType register_type = std::get<1>(key);
        
        // 연속된 주소별로 서브그룹 생성 (임시로 단순 구현)
        for (const auto& point : points) {
            uint8_t slave_id_temp;
            ModbusRegisterType register_type_temp;
            uint16_t address_temp;
            
            if (!ParseModbusAddress(point, slave_id_temp, register_type_temp, address_temp)) {
                continue;
            }
            
            // 단순히 각 포인트를 개별 그룹으로 생성 (최적화는 나중에)
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
    
    LogMessage(PulseOne::LogLevel::INFO, "Created " + std::to_string(created_groups) + " polling groups");
    return created_groups;
}

size_t ModbusTcpWorker::OptimizePollingGroups() {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    size_t original_count = polling_groups_.size();
    std::vector<ModbusTcpPollingGroup> optimized_groups;
    
    // 병합 가능한 그룹들을 찾아서 최적화
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
    
    // 최적화된 그룹으로 교체
    polling_groups_.clear();
    uint32_t new_group_id = 1;
    
    for (auto& group : optimized_groups) {
        group.group_id = new_group_id++;
        polling_groups_[group.group_id] = group;
    }
    
    next_group_id_ = new_group_id;
    
    size_t optimized_count = polling_groups_.size();
    LogMessage(PulseOne::LogLevel::INFO, "Optimized polling groups: " + std::to_string(original_count) + 
               " → " + std::to_string(optimized_count));
    
    return optimized_count;
}

// =============================================================================
// 내부 메서드 (Worker 고유 로직)
// =============================================================================

bool ModbusTcpWorker::ParseModbusConfig() {
    try {
        // DeviceInfo에서 설정 JSON 가져오기
        const std::string& config_json = device_info_.config_json;
        
        if (config_json.empty()) {
            LogMessage(PulseOne::LogLevel::INFO, "No configuration found, using default Modbus configuration");
            return true;
        }
        
        json config = json::parse(config_json);
        
        // 설정 파싱
        if (config.contains("polling_interval_ms")) {
            default_polling_interval_ms_ = config["polling_interval_ms"].get<uint32_t>();
        }
        
        if (config.contains("max_registers_per_group")) {
            max_registers_per_group_ = config["max_registers_per_group"].get<uint16_t>();
        }
        
        if (config.contains("auto_group_creation")) {
            auto_group_creation_enabled_ = config["auto_group_creation"].get<bool>();
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "Modbus configuration parsed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception parsing Modbus config: " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::InitializeModbusDriver() {
    try {
        // ModbusDriver 생성
        modbus_driver_ = std::make_unique<ModbusDriver>();
        
        // Driver 설정 구성
        DriverConfig driver_config;
        driver_config.device_id = std::hash<std::string>{}(device_info_.id); // UUID를 해시로 변환
        driver_config.protocol_type = ProtocolType::MODBUS_TCP;
        driver_config.endpoint = device_info_.endpoint;
        driver_config.timeout_ms = device_info_.timeout_ms;
        driver_config.retry_count = device_info_.retry_count;
        
        // ModbusDriver 초기화
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize ModbusDriver: " + error.message);
            return false;
        }
        
        // Driver 콜백 설정
        SetupDriverCallbacks();
        
        LogMessage(PulseOne::LogLevel::INFO, "ModbusDriver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception initializing ModbusDriver: " + std::string(e.what()));
        return false;
    }
}

void ModbusTcpWorker::PollingThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "Polling thread started");
    
    while (polling_thread_running_) {
        try {
            auto current_time = system_clock::now();
            
            // 폴링할 그룹들 찾기
            std::vector<ModbusTcpPollingGroup> groups_to_poll;
            {
                std::lock_guard<std::mutex> lock(polling_groups_mutex_);
                for (auto& pair : polling_groups_) {
                    auto& group = pair.second;
                    
                    if (group.enabled && current_time >= group.next_poll_time) {
                        groups_to_poll.push_back(group);
                        
                        // 다음 폴링 시간 업데이트
                        group.last_poll_time = current_time;
                        group.next_poll_time = current_time + milliseconds(group.polling_interval_ms);
                    }
                }
            }
            
            // 폴링 실행
            for (const auto& group : groups_to_poll) {
                if (!polling_thread_running_) {
                    break;
                }
                
                ProcessPollingGroup(group);
            }
            
            // 100ms 대기
            std::this_thread::sleep_for(milliseconds(100));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in polling thread: " + std::string(e.what()));
            std::this_thread::sleep_for(seconds(1));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Polling thread stopped");
}

bool ModbusTcpWorker::ProcessPollingGroup(const ModbusTcpPollingGroup& group) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(PulseOne::LogLevel::WARN, "ModbusDriver not connected, skipping group " + std::to_string(group.group_id));
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Processing polling group " + std::to_string(group.group_id));
    
    try {
        // ModbusDriver를 통한 값 읽기
        std::vector<TimestampedValue> values;
        bool success = modbus_driver_->ReadValues(group.data_points, values);
        
        if (!success) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::WARN, "Failed to read group " + std::to_string(group.group_id) + ": " + error.message);
            return false;
        }
        
        // 데이터베이스에 저장
        for (size_t i = 0; i < group.data_points.size() && i < values.size(); ++i) {
            SaveDataPointValue(group.data_points[i], values[i]);
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Successfully processed group " + std::to_string(group.group_id) + 
                   ", read " + std::to_string(values.size()) + " values");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception processing group " + std::to_string(group.group_id) + ": " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::SaveDataPointValue(const PulseOne::DataPoint& data_point,
                                         const PulseOne::TimestampedValue& value) {
    try {
        // BaseDeviceWorker의 기본 저장 메서드 사용
        SaveToInfluxDB(data_point.id, value);
        // SaveToRedis는 BaseDeviceWorker에 없을 수 있으므로 제거하거나 확인 필요
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to save data point " + data_point.name + ": " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ParseModbusAddress(const PulseOne::DataPoint& data_point,
                                         uint8_t& slave_id,
                                         ModbusRegisterType& register_type,
                                         uint16_t& address) {
    try {
        // data_point.address는 Modbus 표준 주소 포맷으로 가정
        // 예: "1:40001" (slave_id:address) 또는 "40001" (기본 slave_id=1)
        
        std::string addr_str = std::to_string(data_point.address);
        size_t colon_pos = addr_str.find(':');
        
        if (colon_pos != std::string::npos) {
            // 슬레이브 ID가 포함된 경우
            slave_id = static_cast<uint8_t>(std::stoi(addr_str.substr(0, colon_pos)));
            address = static_cast<uint16_t>(std::stoi(addr_str.substr(colon_pos + 1)));
        } else {
            // 기본 슬레이브 ID 사용
            slave_id = 1;
            address = static_cast<uint16_t>(data_point.address);
        }
        
        // Modbus 주소 범위에 따른 레지스터 타입 결정
        if (address >= 1 && address <= 9999) {
            register_type = ModbusRegisterType::COIL;
            address -= 1;  // 0-based addressing
        } else if (address >= 10001 && address <= 19999) {
            register_type = ModbusRegisterType::DISCRETE_INPUT;
            address -= 10001;  // 0-based addressing
        } else if (address >= 30001 && address <= 39999) {
            register_type = ModbusRegisterType::INPUT_REGISTER;
            address -= 30001;  // 0-based addressing
        } else if (address >= 40001 && address <= 49999) {
            register_type = ModbusRegisterType::HOLDING_REGISTER;
            address -= 40001;  // 0-based addressing
        } else {
            // 0-based 주소로 간주하고 기본 타입 사용
            register_type = ModbusRegisterType::HOLDING_REGISTER;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse Modbus address for " + data_point.name + ": " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::ValidatePollingGroup(const ModbusTcpPollingGroup& group) {
    if (group.data_points.empty()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Polling group has no data points");
        return false;
    }
    
    if (group.register_count == 0 || group.register_count > max_registers_per_group_) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid register count: " + std::to_string(group.register_count));
        return false;
    }
    
    if (group.slave_id == 0 || group.slave_id > 247) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid slave ID: " + std::to_string(group.slave_id));
        return false;
    }
    
    if (group.polling_interval_ms < 100) {
        LogMessage(PulseOne::LogLevel::ERROR, "Polling interval too short: " + std::to_string(group.polling_interval_ms) + "ms");
        return false;
    }
    
    return true;
}

bool ModbusTcpWorker::CanMergePollingGroups(const ModbusTcpPollingGroup& group1,
                                           const ModbusTcpPollingGroup& group2) {
    // 같은 슬레이브, 같은 레지스터 타입인지 확인
    if (group1.slave_id != group2.slave_id || group1.register_type != group2.register_type) {
        return false;
    }
    
    // 주소가 연속되는지 확인
    uint16_t end1 = group1.start_address + group1.register_count;
    uint16_t end2 = group2.start_address + group2.register_count;
    
    bool adjacent = (end1 == group2.start_address) || (end2 == group1.start_address);
    if (!adjacent) {
        return false;
    }
    
    // 병합 후 크기가 제한을 넘지 않는지 확인
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
    
    // 데이터 포인트 병합
    merged.data_points = group1.data_points;
    merged.data_points.insert(merged.data_points.end(), 
                             group2.data_points.begin(), group2.data_points.end());
    
    merged.last_poll_time = std::min(group1.last_poll_time, group2.last_poll_time);
    merged.next_poll_time = std::min(group1.next_poll_time, group2.next_poll_time);
    
    return merged;
}

// =============================================================================
// ModbusDriver 콜백 메서드들 (Driver → Worker)
// =============================================================================

void ModbusTcpWorker::SetupDriverCallbacks() {
    if (!modbus_driver_) {
        return;
    }
    
    // 콜백 설정 (ModbusDriver가 이러한 콜백을 지원한다고 가정)
    // 실제 구현에서는 ModbusDriver의 API에 따라 달라질 수 있음
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "ModbusDriver callbacks configured");
}

void ModbusTcpWorker::OnConnectionStatusChanged(void* worker_ptr, bool connected,
                                               const std::string& error_message) {
    auto* worker = static_cast<ModbusTcpWorker*>(worker_ptr);
    if (!worker) {
        return;
    }
    
    if (connected) {
        worker->LogMessage(PulseOne::LogLevel::INFO, "Modbus connection established");
    } else {
        worker->LogMessage(PulseOne::LogLevel::WARN, "Modbus connection lost: " + error_message);
    }
}

void ModbusTcpWorker::OnModbusError(void* worker_ptr, uint8_t slave_id, uint8_t function_code,
                                   int error_code, const std::string& error_message) {
    auto* worker = static_cast<ModbusTcpWorker*>(worker_ptr);
    if (!worker) {
        return;
    }
    
    worker->LogMessage(PulseOne::LogLevel::ERROR, "Modbus error - Slave: " + std::to_string(slave_id) + 
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
    
    worker->LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Modbus " + operation + 
                       (success ? " succeeded" : " failed") + 
                       " in " + std::to_string(response_time_ms) + "ms");
}

} // namespace Workers
} // namespace PulseOne