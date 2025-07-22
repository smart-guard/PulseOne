/**
 * @file ModbusTcpWorker.cpp
 * @brief Modbus TCP 워커 구현
 * @author PulseOne Development Team
 * @date 2025-01-21
 * @version 1.0.0
 */

#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <shared_mutex>
#include <arpa/inet.h>  // inet_addr용
#include <sys/socket.h> // inet_pton용

using namespace std::chrono;
using namespace PulseOne::Drivers;  // DataValue, DataQuality, TimestampedValue 사용을 위해

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusTcpWorker::ModbusTcpWorker(const Drivers::DeviceInfo& device_info,
                                 std::shared_ptr<RedisClient> redis_client,
                                 std::shared_ptr<InfluxClient> influx_client)
    : TcpBasedWorker(device_info, redis_client, influx_client)
    , default_slave_id_(1)
    , response_timeout_ms_(1000)
    , byte_timeout_ms_(1000)
    , max_connections_(10)
    , connection_reuse_enabled_(true)
    , next_group_id_(1)
    , stop_workers_(false)
    , total_reads_(0), successful_reads_(0), total_writes_(0)
    , successful_writes_(0), network_errors_(0), timeout_errors_(0), connection_errors_(0) {
    
    LogModbusTcpMessage(LogLevel::INFO, "ModbusTcpWorker created for device: " + device_info.name);
    
    // 기본 재연결 설정
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 5000;          // TCP는 더 긴 재연결 주기
    settings.max_retries_per_cycle = 3;
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 60;  // TCP Keep-alive
    UpdateReconnectionSettings(settings);
    
    // DeviceInfo에서 연결 정보 파싱
    if (!device_info.endpoint.empty()) {
        size_t colon_pos = device_info.endpoint.find(':');
        if (colon_pos != std::string::npos) {
            ip_address_ = device_info.endpoint.substr(0, colon_pos);
            port_ = static_cast<uint16_t>(std::stoi(device_info.endpoint.substr(colon_pos + 1)));
        }
    }
}

ModbusTcpWorker::~ModbusTcpWorker() {
    // 스레드 정리
    stop_workers_ = true;
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    // 모든 Modbus 연결 정리
    std::lock_guard<std::mutex> lock(connection_mutex_);
    for (auto& pair : connection_pool_) {
        if (pair.second) {
            modbus_close(pair.second);
            modbus_free(pair.second);
        }
    }
    connection_pool_.clear();
    
    LogModbusTcpMessage(LogLevel::INFO, "ModbusTcpWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusTcpWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogModbusTcpMessage(LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogModbusTcpMessage(LogLevel::INFO, "Starting ModbusTcpWorker...");
        
        try {
            // 기본 연결 설정
            if (!EstablishConnection()) {
                LogModbusTcpMessage(LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            // 상태 변경 - 부모 클래스 상태 직접 변경
            ChangeState(WorkerState::RUNNING);
            
            // 폴링 스레드 시작
            stop_workers_ = false;
            polling_thread_ = std::thread(&ModbusTcpWorker::PollingWorkerLoop, this);
            
            LogModbusTcpMessage(LogLevel::INFO, "ModbusTcpWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogModbusTcpMessage(LogLevel::ERROR, "Exception during start: " + std::string(e.what()));
            return false;
        }
    });
}

std::future<bool> ModbusTcpWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::STOPPED) {
            LogModbusTcpMessage(LogLevel::WARN, "Worker already stopped");
            return true;
        }
        
        LogModbusTcpMessage(LogLevel::INFO, "Stopping ModbusTcpWorker...");
        
        try {
            // 폴링 스레드 중지
            stop_workers_ = true;
            if (polling_thread_.joinable()) {
                polling_thread_.join();
            }
            
            // 연결 종료
            CloseConnection();
            
            // 상태 변경 - 부모 클래스 상태 직접 변경
            ChangeState(WorkerState::STOPPED);
            
            LogModbusTcpMessage(LogLevel::INFO, "ModbusTcpWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogModbusTcpMessage(LogLevel::ERROR, "Exception during stop: " + std::string(e.what()));
            return false;
        }
    });
}

WorkerState ModbusTcpWorker::GetState() const {
    return TcpBasedWorker::GetState();
}

// =============================================================================
// TcpBasedWorker 인터페이스 구현
// =============================================================================

bool ModbusTcpWorker::EstablishProtocolConnection() {
    LogModbusTcpMessage(LogLevel::INFO, "Establishing Modbus TCP protocol connection...");
    
    // 기본 Modbus TCP 연결 테스트 (기본 서버에 대해)
    std::string host = ip_address_;
    int port = port_;
    
    modbus_t* ctx = modbus_new_tcp(host.c_str(), port);
    if (!ctx) {
        LogModbusTcpMessage(LogLevel::ERROR, "Failed to create Modbus TCP context");
        return false;
    }
    
    // 타임아웃 설정
    modbus_set_response_timeout(ctx, response_timeout_ms_ / 1000, (response_timeout_ms_ % 1000) * 1000);
    modbus_set_byte_timeout(ctx, byte_timeout_ms_ / 1000, (byte_timeout_ms_ % 1000) * 1000);
    
    if (modbus_connect(ctx) == -1) {
        LogModbusTcpMessage(LogLevel::ERROR, "Failed to connect: " + std::string(modbus_strerror(errno)));
        modbus_free(ctx);
        return false;
    }
    
    // 연결 성공 - 연결 풀에 추가
    std::string connection_key = host + ":" + std::to_string(port);
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connection_pool_[connection_key] = ctx;
    }
    
    LogModbusTcpMessage(LogLevel::INFO, "Modbus TCP protocol connection established to " + connection_key);
    return true;
}

bool ModbusTcpWorker::CloseProtocolConnection() {
    LogModbusTcpMessage(LogLevel::INFO, "Closing Modbus TCP protocol connections...");
    
    std::lock_guard<std::mutex> lock(connection_mutex_);
    for (auto& pair : connection_pool_) {
        if (pair.second) {
            modbus_close(pair.second);
            modbus_free(pair.second);
        }
    }
    connection_pool_.clear();
    
    LogModbusTcpMessage(LogLevel::INFO, "All Modbus TCP protocol connections closed");
    return true;
}

bool ModbusTcpWorker::CheckProtocolConnection() {
    std::string host = ip_address_;
    int port = port_;
    std::string connection_key = host + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connection_mutex_);
    auto it = connection_pool_.find(connection_key);
    if (it == connection_pool_.end() || !it->second) {
        return false;
    }
    
    // 간단한 읽기 테스트로 연결 상태 확인
    uint16_t test_value;
    int result = modbus_read_registers(it->second, 0, 1, &test_value);
    return (result != -1);
}

bool ModbusTcpWorker::SendProtocolKeepAlive() {
    // Modbus TCP는 별도의 Keep-alive가 없으므로 연결 상태만 확인
    return CheckProtocolConnection();
}

// =============================================================================
// Modbus TCP 특화 설정 관리
// =============================================================================

void ModbusTcpWorker::ConfigureModbusTcp(int default_slave_id, 
                                         int response_timeout_ms,
                                         int byte_timeout_ms) {
    default_slave_id_ = default_slave_id;
    response_timeout_ms_ = response_timeout_ms;
    byte_timeout_ms_ = byte_timeout_ms;
    
    LogModbusTcpMessage(LogLevel::INFO, 
        "Modbus TCP configured - SlaveID: " + std::to_string(default_slave_id) +
        ", ResponseTimeout: " + std::to_string(response_timeout_ms) + "ms" +
        ", ByteTimeout: " + std::to_string(byte_timeout_ms) + "ms");
}

void ModbusTcpWorker::ConfigureConnectionPool(int max_connections, 
                                             bool connection_reuse_enabled) {
    max_connections_ = max_connections;
    connection_reuse_enabled_ = connection_reuse_enabled;
    
    LogModbusTcpMessage(LogLevel::INFO, 
        "Connection pool configured - MaxConnections: " + std::to_string(max_connections) +
        ", ReuseEnabled: " + (connection_reuse_enabled ? "true" : "false"));
}

// =============================================================================
// 폴링 그룹 관리
// =============================================================================

uint32_t ModbusTcpWorker::AddPollingGroup(const std::string& group_name,
                                         int slave_id,
                                         const std::string& target_ip,
                                         int target_port,
                                         ModbusRegisterType register_type,
                                         uint16_t start_address,
                                         uint16_t register_count,
                                         int polling_interval_ms) {
    
    // 설정 검증
    if (!ValidatePollingGroup(slave_id, target_ip, target_port, register_type, 
                             start_address, register_count)) {
        LogModbusTcpMessage(LogLevel::ERROR, "Invalid polling group configuration");
        return 0;
    }
    
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    uint32_t group_id = next_group_id_++;
    ModbusTcpPollingGroup group;
    group.group_id = group_id;
    group.group_name = group_name;
    group.slave_id = slave_id;
    group.target_ip = target_ip;
    group.target_port = target_port;
    group.register_type = register_type;
    group.start_address = start_address;
    group.register_count = register_count;
    group.polling_interval_ms = polling_interval_ms;
    group.enabled = true;
    group.next_poll_time = std::chrono::system_clock::now();
    
    polling_groups_[group_id] = group;
    
    LogModbusTcpMessage(LogLevel::INFO, 
        "Added polling group '" + group_name + "' (ID: " + std::to_string(group_id) + 
        ") for " + target_ip + ":" + std::to_string(target_port) + 
        ", SlaveID: " + std::to_string(slave_id));
    
    return group_id;
}

bool ModbusTcpWorker::RemovePollingGroup(uint32_t group_id) {
    std::unique_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogModbusTcpMessage(LogLevel::WARN, "Polling group not found: " + std::to_string(group_id));
        return false;
    }
    
    std::string group_name = it->second.group_name;
    polling_groups_.erase(it);
    
    LogModbusTcpMessage(LogLevel::INFO, "Removed polling group '" + group_name + "' (ID: " + std::to_string(group_id) + ")");
    return true;
}

bool ModbusTcpWorker::EnablePollingGroup(uint32_t group_id, bool enabled) {
    std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogModbusTcpMessage(LogLevel::WARN, "Polling group not found: " + std::to_string(group_id));
        return false;
    }
    
    const_cast<ModbusTcpPollingGroup&>(it->second).enabled = enabled;
    
    LogModbusTcpMessage(LogLevel::INFO, 
        "Polling group " + std::to_string(group_id) + " " + (enabled ? "enabled" : "disabled"));
    return true;
}

bool ModbusTcpWorker::AddDataPointToGroup(uint32_t group_id, const Drivers::DataPoint& data_point) {
    std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogModbusTcpMessage(LogLevel::WARN, "Polling group not found: " + std::to_string(group_id));
        return false;
    }
    
    const_cast<ModbusTcpPollingGroup&>(it->second).data_points.push_back(data_point);
    
    LogModbusTcpMessage(LogLevel::DEBUG, 
        "Added data point '" + data_point.name + "' to group " + std::to_string(group_id));
    return true;
}

// =============================================================================
// 슬레이브 관리
// =============================================================================

bool ModbusTcpWorker::AddSlave(int slave_id, const std::string& ip_address, int port) {
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto slave_info = std::make_shared<ModbusTcpSlaveInfo>(slave_id, ip_address, port);
    slaves_[slave_id] = slave_info;
    
    LogModbusTcpMessage(LogLevel::INFO, 
        "Added slave " + std::to_string(slave_id) + " at " + ip_address + ":" + std::to_string(port));
    return true;
}

bool ModbusTcpWorker::RemoveSlave(int slave_id) {
    std::unique_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it == slaves_.end()) {
        LogModbusTcpMessage(LogLevel::WARN, "Slave not found: " + std::to_string(slave_id));
        return false;
    }
    
    slaves_.erase(it);
    LogModbusTcpMessage(LogLevel::INFO, "Removed slave " + std::to_string(slave_id));
    return true;
}

std::shared_ptr<ModbusTcpSlaveInfo> ModbusTcpWorker::GetSlaveInfo(int slave_id) const {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        return it->second;
    }
    return nullptr;
}

int ModbusTcpWorker::ScanSlaves(const std::string& ip_range, int port, int timeout_ms) {
    LogModbusTcpMessage(LogLevel::INFO, "Scanning slaves in range: " + ip_range + ":" + std::to_string(port));
    
    std::vector<std::string> ip_list = ParseIpRange(ip_range);
    int found_count = 0;
    
    for (const auto& ip : ip_list) {
        // 각 IP에 대해 슬레이브 1-247 테스트 (간단히 1-10만)
        for (int slave_id = 1; slave_id <= 10; ++slave_id) {
            modbus_t* ctx = modbus_new_tcp(ip.c_str(), port);
            if (!ctx) continue;
            
            modbus_set_response_timeout(ctx, timeout_ms / 1000, (timeout_ms % 1000) * 1000);
            modbus_set_slave(ctx, slave_id);
            
            if (modbus_connect(ctx) != -1) {
                // 간단한 읽기 테스트
                uint16_t test_value;
                if (modbus_read_registers(ctx, 0, 1, &test_value) != -1) {
                    AddSlave(slave_id, ip, port);
                    found_count++;
                    LogModbusTcpMessage(LogLevel::INFO, 
                        "Found slave " + std::to_string(slave_id) + " at " + ip + ":" + std::to_string(port));
                }
                modbus_close(ctx);
            }
            modbus_free(ctx);
            
            // 스캔 중단 체크
            if (stop_workers_) break;
        }
        if (stop_workers_) break;
    }
    
    LogModbusTcpMessage(LogLevel::INFO, "Scan completed. Found " + std::to_string(found_count) + " slaves");
    return found_count;
}

// =============================================================================
// 데이터 읽기/쓰기
// =============================================================================

bool ModbusTcpWorker::ReadHoldingRegisters(int slave_id, const std::string& target_ip, int target_port,
                                          uint16_t start_address, uint16_t register_count, 
                                          std::vector<uint16_t>& values) {
    auto start_time = std::chrono::steady_clock::now();
    
    modbus_t* ctx = GetModbusConnection(target_ip, target_port, slave_id);
    if (!ctx) {
        UpdateModbusTcpStats("read", false, "connection");
        return false;
    }
    
    values.resize(register_count);
    int result = modbus_read_registers(ctx, start_address, register_count, values.data());
    
    auto end_time = std::chrono::steady_clock::now();
    int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string connection_key = target_ip + ":" + std::to_string(target_port);
    ReturnModbusConnection(connection_key, ctx);
    
    if (result == -1) {
        UpdateModbusTcpStats("read", false, "timeout");
        UpdateSlaveStatus(slave_id, response_time, false);
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to read holding registers from slave " + std::to_string(slave_id) + 
            ": " + ModbusErrorToString(errno));
        return false;
    }
    
    UpdateModbusTcpStats("read", true);
    UpdateSlaveStatus(slave_id, response_time, true);
    return true;
}

bool ModbusTcpWorker::ReadInputRegisters(int slave_id, const std::string& target_ip, int target_port,
                                        uint16_t start_address, uint16_t register_count, 
                                        std::vector<uint16_t>& values) {
    auto start_time = std::chrono::steady_clock::now();
    
    modbus_t* ctx = GetModbusConnection(target_ip, target_port, slave_id);
    if (!ctx) {
        UpdateModbusTcpStats("read", false, "connection");
        return false;
    }
    
    values.resize(register_count);
    int result = modbus_read_input_registers(ctx, start_address, register_count, values.data());
    
    auto end_time = std::chrono::steady_clock::now();
    int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string connection_key = target_ip + ":" + std::to_string(target_port);
    ReturnModbusConnection(connection_key, ctx);
    
    if (result == -1) {
        UpdateModbusTcpStats("read", false, "timeout");
        UpdateSlaveStatus(slave_id, response_time, false);
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to read input registers from slave " + std::to_string(slave_id) + 
            ": " + ModbusErrorToString(errno));
        return false;
    }
    
    UpdateModbusTcpStats("read", true);
    UpdateSlaveStatus(slave_id, response_time, true);
    return true;
}

bool ModbusTcpWorker::ReadCoils(int slave_id, const std::string& target_ip, int target_port,
                               uint16_t start_address, uint16_t coil_count, 
                               std::vector<bool>& values) {
    auto start_time = std::chrono::steady_clock::now();
    
    modbus_t* ctx = GetModbusConnection(target_ip, target_port, slave_id);
    if (!ctx) {
        UpdateModbusTcpStats("read", false, "connection");
        return false;
    }
    
    std::vector<uint8_t> coil_bytes((coil_count + 7) / 8);  // 바이트 단위로 할당
    int result = modbus_read_bits(ctx, start_address, coil_count, coil_bytes.data());
    
    auto end_time = std::chrono::steady_clock::now();
    int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string connection_key = target_ip + ":" + std::to_string(target_port);
    ReturnModbusConnection(connection_key, ctx);
    
    if (result == -1) {
        UpdateModbusTcpStats("read", false, "timeout");
        UpdateSlaveStatus(slave_id, response_time, false);
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to read coils from slave " + std::to_string(slave_id) + 
            ": " + ModbusErrorToString(errno));
        return false;
    }
    
    // uint8_t 배열을 bool 벡터로 변환
    values.resize(coil_count);
    for (uint16_t i = 0; i < coil_count; ++i) {
        values[i] = (coil_bytes[i] != 0);
    }
    
    UpdateModbusTcpStats("read", true);
    UpdateSlaveStatus(slave_id, response_time, true);
    return true;
}

bool ModbusTcpWorker::WriteSingleRegister(int slave_id, const std::string& target_ip, int target_port,
                                         uint16_t address, uint16_t value) {
    auto start_time = std::chrono::steady_clock::now();
    
    modbus_t* ctx = GetModbusConnection(target_ip, target_port, slave_id);
    if (!ctx) {
        UpdateModbusTcpStats("write", false, "connection");
        return false;
    }
    
    int result = modbus_write_register(ctx, address, value);
    
    auto end_time = std::chrono::steady_clock::now();
    int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string connection_key = target_ip + ":" + std::to_string(target_port);
    ReturnModbusConnection(connection_key, ctx);
    
    if (result == -1) {
        UpdateModbusTcpStats("write", false, "timeout");
        UpdateSlaveStatus(slave_id, response_time, false);
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to write register to slave " + std::to_string(slave_id) + 
            ": " + ModbusErrorToString(errno));
        return false;
    }
    
    UpdateModbusTcpStats("write", true);
    UpdateSlaveStatus(slave_id, response_time, true);
    return true;
}

bool ModbusTcpWorker::WriteMultipleRegisters(int slave_id, const std::string& target_ip, int target_port,
                                            uint16_t start_address, const std::vector<uint16_t>& values) {
    auto start_time = std::chrono::steady_clock::now();
    
    modbus_t* ctx = GetModbusConnection(target_ip, target_port, slave_id);
    if (!ctx) {
        UpdateModbusTcpStats("write", false, "connection");
        return false;
    }
    
    int result = modbus_write_registers(ctx, start_address, values.size(), values.data());
    
    auto end_time = std::chrono::steady_clock::now();
    int response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string connection_key = target_ip + ":" + std::to_string(target_port);
    ReturnModbusConnection(connection_key, ctx);
    
    if (result == -1) {
        UpdateModbusTcpStats("write", false, "timeout");
        UpdateSlaveStatus(slave_id, response_time, false);
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to write registers to slave " + std::to_string(slave_id) + 
            ": " + ModbusErrorToString(errno));
        return false;
    }
    
    UpdateModbusTcpStats("write", true);
    UpdateSlaveStatus(slave_id, response_time, true);
    return true;
}

// =============================================================================
// 상태 및 통계 조회
// =============================================================================

std::string ModbusTcpWorker::GetModbusTcpStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::ostringstream oss;
    oss << "{"
        << "\"total_reads\": " << total_reads_ << ","
        << "\"successful_reads\": " << successful_reads_ << ","
        << "\"total_writes\": " << total_writes_ << ","
        << "\"successful_writes\": " << successful_writes_ << ","
        << "\"network_errors\": " << network_errors_ << ","
        << "\"timeout_errors\": " << timeout_errors_ << ","
        << "\"connection_errors\": " << connection_errors_ << ","
        << "\"read_success_rate\": " << (total_reads_ > 0 ? (double)successful_reads_ / total_reads_ * 100.0 : 0.0) << ","
        << "\"write_success_rate\": " << (total_writes_ > 0 ? (double)successful_writes_ / total_writes_ * 100.0 : 0.0)
        << "}";
    
    return oss.str();
}

std::string ModbusTcpWorker::GetPollingGroupStatus() const {
    std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
    
    std::ostringstream oss;
    oss << "[";
    
    bool first = true;
    for (const auto& pair : polling_groups_) {
        if (!first) oss << ",";
        first = false;
        
        const auto& group = pair.second;
        oss << "{"
            << "\"group_id\": " << group.group_id << ","
            << "\"group_name\": \"" << group.group_name << "\","
            << "\"slave_id\": " << group.slave_id << ","
            << "\"target\": \"" << group.target_ip << ":" << group.target_port << "\","
            << "\"enabled\": " << (group.enabled ? "true" : "false") << ","
            << "\"polling_interval_ms\": " << group.polling_interval_ms << ","
            << "\"data_points_count\": " << group.data_points.size()
            << "}";
    }
    
    oss << "]";
    return oss.str();
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

void ModbusTcpWorker::PollingWorkerLoop() {
    LogModbusTcpMessage(LogLevel::INFO, "Polling worker started");
    
    while (!stop_workers_) {
        try {
            auto current_time = std::chrono::system_clock::now();
            
            std::shared_lock<std::shared_mutex> lock(polling_groups_mutex_);
            for (auto& pair : polling_groups_) {
                auto& group = const_cast<ModbusTcpPollingGroup&>(pair.second);
                
                if (!group.enabled) continue;
                
                if (current_time >= group.next_poll_time) {
                    ProcessPollingGroup(group);
                    
                    // 다음 폴링 시간 설정
                    group.last_poll_time = current_time;
                    group.next_poll_time = current_time + 
                        std::chrono::milliseconds(group.polling_interval_ms);
                }
            }
            
        } catch (const std::exception& e) {
            LogModbusTcpMessage(LogLevel::ERROR, "Exception in polling loop: " + std::string(e.what()));
        }
        
        // 100ms 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LogModbusTcpMessage(LogLevel::INFO, "Polling worker stopped");
}

bool ModbusTcpWorker::ProcessPollingGroup(ModbusTcpPollingGroup& group) {
    LogModbusTcpMessage(LogLevel::DEBUG, 
        "Processing polling group " + std::to_string(group.group_id) + 
        " (" + group.group_name + ")");
    
    std::vector<uint16_t> values;
    bool success = false;
    
    switch (group.register_type) {
        case ModbusRegisterType::HOLDING_REGISTER:
            success = ReadHoldingRegisters(group.slave_id, group.target_ip, group.target_port,
                                         group.start_address, group.register_count, values);
            break;
            
        case ModbusRegisterType::INPUT_REGISTER:
            success = ReadInputRegisters(group.slave_id, group.target_ip, group.target_port,
                                       group.start_address, group.register_count, values);
            break;
            
        case ModbusRegisterType::COIL:
        case ModbusRegisterType::DISCRETE_INPUT:
            // 코일 처리는 별도 로직 필요 (bool 벡터)
            LogModbusTcpMessage(LogLevel::WARN, "Coil/Discrete input polling not implemented yet");
            return false;
            
        default:
            LogModbusTcpMessage(LogLevel::ERROR, "Unknown register type in group " + std::to_string(group.group_id));
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
            timestamped_value.timestamp = std::chrono::system_clock::now();
            
            // Redis/InfluxDB에 데이터 저장 (기본 BaseDeviceWorker 메서드 사용)
            SaveToInfluxDB(data_point.id, timestamped_value);
        }
        
        LogModbusTcpMessage(LogLevel::DEBUG, 
            "Successfully processed group " + std::to_string(group.group_id) + 
            ", read " + std::to_string(values.size()) + " values");
    }
    
    return success;
}

modbus_t* ModbusTcpWorker::GetModbusConnection(const std::string& ip_address, int port, int slave_id) {
    std::string connection_key = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    // 기존 연결 재사용 검사
    auto it = connection_pool_.find(connection_key);
    if (it != connection_pool_.end() && it->second && connection_reuse_enabled_) {
        modbus_set_slave(it->second, slave_id);
        return it->second;
    }
    
    // 새 연결 생성
    modbus_t* ctx = modbus_new_tcp(ip_address.c_str(), port);
    if (!ctx) {
        LogModbusTcpMessage(LogLevel::ERROR, "Failed to create Modbus context for " + connection_key);
        return nullptr;
    }
    
    // 타임아웃 설정
    modbus_set_response_timeout(ctx, response_timeout_ms_ / 1000, (response_timeout_ms_ % 1000) * 1000);
    modbus_set_byte_timeout(ctx, byte_timeout_ms_ / 1000, (byte_timeout_ms_ % 1000) * 1000);
    modbus_set_slave(ctx, slave_id);
    
    if (modbus_connect(ctx) == -1) {
        LogModbusTcpMessage(LogLevel::ERROR, 
            "Failed to connect to " + connection_key + ": " + ModbusErrorToString(errno));
        modbus_free(ctx);
        return nullptr;
    }
    
    // 연결 풀에 저장 (크기 제한 확인)
    if (connection_pool_.size() < static_cast<size_t>(max_connections_)) {
        connection_pool_[connection_key] = ctx;
    }
    
    return ctx;
}

void ModbusTcpWorker::ReturnModbusConnection(const std::string& connection_key, modbus_t* ctx) {
    if (!connection_reuse_enabled_) {
        // 재사용하지 않으면 바로 정리
        modbus_close(ctx);
        modbus_free(ctx);
        return;
    }
    
    // 연결 풀에 유지 (이미 lock 내에서 호출됨을 가정)
    // 실제로는 connection_mutex_ 보호가 필요하지만 
    // GetModbusConnection과 함께 호출되므로 생략
}

std::string ModbusTcpWorker::ModbusErrorToString(int error_code) const {
    switch (error_code) {
        case EMBXILFUN: return "Illegal function";
        case EMBXILADD: return "Illegal data address";
        case EMBXILVAL: return "Illegal data value";
        case EMBXSFAIL: return "Slave device failure";
        case EMBXACK:   return "Acknowledge";
        case EMBXSBUSY: return "Slave device busy";
        case EMBXNACK:  return "Negative acknowledge";
        case EMBXMEMPAR: return "Memory parity error";
        case EMBXGPATH: return "Gateway path unavailable";
        case EMBXGTAR:  return "Gateway target device failed to respond";
        default:
            return "Unknown error (" + std::to_string(error_code) + "): " + std::string(strerror(error_code));
    }
}

void ModbusTcpWorker::UpdateModbusTcpStats(const std::string& operation, bool success, 
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
    
    if (!success) {
        if (error_type == "network") {
            network_errors_++;
        } else if (error_type == "timeout") {
            timeout_errors_++;
        } else if (error_type == "connection") {
            connection_errors_++;
        }
    }
}

void ModbusTcpWorker::UpdateSlaveStatus(int slave_id, int response_time_ms, bool success) {
    std::shared_lock<std::shared_mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        auto& slave_info = it->second;
        slave_info->last_response = std::chrono::system_clock::now();
        slave_info->response_time_ms = response_time_ms;
        slave_info->total_requests++;
        
        if (success) {
            slave_info->successful_requests++;
            slave_info->is_online = true;
            slave_info->last_error.clear();
        } else {
            slave_info->is_online = false;
            slave_info->last_error = ModbusErrorToString(errno);
        }
    }
}

void ModbusTcpWorker::LogModbusTcpMessage(LogLevel level, const std::string& message) const {
    // BaseDeviceWorker의 LogMessage 사용 (const_cast 필요)
    const_cast<ModbusTcpWorker*>(this)->LogMessage(level, "[ModbusTCP] " + message);
}

bool ModbusTcpWorker::ValidatePollingGroup(int slave_id, const std::string& target_ip, int target_port,
                                          ModbusRegisterType register_type,
                                          uint16_t start_address, uint16_t register_count) const {
    // 슬레이브 ID 검증
    if (slave_id < 1 || slave_id > 247) {
        LogModbusTcpMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
        return false;
    }
    
    // IP 주소 검증
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, target_ip.c_str(), &(sa.sin_addr)) != 1) {
        LogModbusTcpMessage(LogLevel::ERROR, "Invalid IP address: " + target_ip);
        return false;
    }
    
    // 포트 검증
    if (target_port < 1 || target_port > 65535) {
        LogModbusTcpMessage(LogLevel::ERROR, "Invalid port: " + std::to_string(target_port));
        return false;
    }
    
    // 레지스터 개수 검증
    if (register_count == 0 || register_count > 125) {  // Modbus 표준 제한
        LogModbusTcpMessage(LogLevel::ERROR, "Invalid register count: " + std::to_string(register_count));
        return false;
    }
    
    // 주소 범위 검증 (0-65535)
    if (start_address + register_count > 65536) {
        LogModbusTcpMessage(LogLevel::ERROR, "Address range overflow: " + 
                           std::to_string(start_address) + " + " + std::to_string(register_count));
        return false;
    }
    
    // 레지스터 타입별 제한 검증
    switch (register_type) {
        case ModbusRegisterType::COIL:
        case ModbusRegisterType::DISCRETE_INPUT:
            if (register_count > 2000) {  // 비트 타입 제한
                LogModbusTcpMessage(LogLevel::ERROR, "Too many bits requested: " + 
                                   std::to_string(register_count));
                return false;
            }
            break;
            
        case ModbusRegisterType::HOLDING_REGISTER:
        case ModbusRegisterType::INPUT_REGISTER:
            if (register_count > 125) {  // 16비트 레지스터 제한
                LogModbusTcpMessage(LogLevel::ERROR, "Too many registers requested: " + 
                                   std::to_string(register_count));
                return false;
            }
            break;
    }
    
    return true;
}

std::vector<std::string> ModbusTcpWorker::ParseIpRange(const std::string& ip_range) const {
    std::vector<std::string> ip_list;
    
    // 간단한 IP 범위 파싱 (예: "192.168.1.1-192.168.1.100")
    size_t dash_pos = ip_range.find('-');
    if (dash_pos == std::string::npos) {
        // 단일 IP
        ip_list.push_back(ip_range);
        return ip_list;
    }
    
    std::string start_ip = ip_range.substr(0, dash_pos);
    std::string end_ip = ip_range.substr(dash_pos + 1);
    
    // IP 주소를 4개 부분으로 분리
    auto split_ip = [](const std::string& ip) -> std::vector<int> {
        std::vector<int> parts;
        std::istringstream iss(ip);
        std::string part;
        
        while (std::getline(iss, part, '.')) {
            parts.push_back(std::stoi(part));
        }
        return parts;
    };
    
    try {
        std::vector<int> start_parts = split_ip(start_ip);
        std::vector<int> end_parts = split_ip(end_ip);
        
        if (start_parts.size() != 4 || end_parts.size() != 4) {
            LogModbusTcpMessage(LogLevel::ERROR, "Invalid IP range format: " + ip_range);
            return ip_list;
        }
        
        // 마지막 옥텟만 범위로 처리 (간단한 구현)
        if (start_parts[0] == end_parts[0] && start_parts[1] == end_parts[1] && 
            start_parts[2] == end_parts[2]) {
            
            for (int i = start_parts[3]; i <= end_parts[3]; ++i) {
                std::ostringstream oss;
                oss << start_parts[0] << "." << start_parts[1] << "." 
                    << start_parts[2] << "." << i;
                ip_list.push_back(oss.str());
            }
        } else {
            LogModbusTcpMessage(LogLevel::WARN, 
                               "Complex IP range not supported, using start IP only: " + start_ip);
            ip_list.push_back(start_ip);
        }
        
    } catch (const std::exception& e) {
        LogModbusTcpMessage(LogLevel::ERROR, "Error parsing IP range: " + std::string(e.what()));
        ip_list.push_back(start_ip);  // 기본값으로 시작 IP만 사용
    }
    
    return ip_list;
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne