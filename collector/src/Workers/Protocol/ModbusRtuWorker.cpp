/**
 * @file ModbusRtuWorker.cpp
 * @brief Modbus RTU 워커 구현
 * @author PulseOne Development Team
 * @date 2025-01-21
 * @version 1.0.0
 */

#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <cstring>
#include <errno.h>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(const Drivers::DeviceInfo& device_info,
                                 std::shared_ptr<RedisClient> redis_client,
                                 std::shared_ptr<InfluxClient> influx_client)
    : SerialBasedWorker(device_info, redis_client, influx_client)
    , modbus_ctx_(nullptr)
    , default_slave_id_(1)
    , response_timeout_ms_(1000)
    , byte_timeout_ms_(1000)
    , bus_lock_time_ms_(10)  // 10ms 버스 락
    , next_group_id_(1)
    , stop_workers_(false)
    , total_reads_(0), successful_reads_(0), total_writes_(0)
    , successful_writes_(0), crc_errors_(0), timeout_errors_(0), bus_errors_(0) {
    
    LogModbusMessage(LogLevel::INFO, "ModbusRtuWorker created for device: " + device_info.name);
    
    // 기본 재연결 설정 (RTU는 시리얼이므로 더 빠른 재연결)
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 2000;          // RTU는 빠른 재연결
    settings.max_retries_per_cycle = 5;
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 30;  // RTU Keep-alive
    UpdateReconnectionSettings(settings);
}

ModbusRtuWorker::~ModbusRtuWorker() {
    // 스레드 정리
    stop_workers_ = true;
    if (polling_thread_.joinable()) {
        polling_thread_.join();
    }
    
    // Modbus 컨텍스트 정리
    if (modbus_ctx_) {
        modbus_close(modbus_ctx_);
        modbus_free(modbus_ctx_);
        modbus_ctx_ = nullptr;
    }
    
    LogModbusMessage(LogLevel::INFO, "ModbusRtuWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusRtuWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogModbusMessage(LogLevel::INFO, "Starting Modbus RTU worker...");
        
        // 직접 연결 수립 (부모 클래스 호출 제거)
        if (!EstablishConnection()) {
            LogModbusMessage(LogLevel::ERROR, "Failed to establish connection");
            promise->set_value(false);
            return future;
        }
        
        // 상태를 RUNNING으로 변경
        ChangeState(WorkerState::RUNNING);
        
        // 폴링 스레드 시작
        stop_workers_ = false;
        polling_thread_ = std::thread(&ModbusRtuWorker::PollingWorkerLoop, this);
        
        LogModbusMessage(LogLevel::INFO, "Modbus RTU worker started successfully");
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "Start failed: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

std::future<bool> ModbusRtuWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogModbusMessage(LogLevel::INFO, "Stopping Modbus RTU worker...");
        
        // 상태를 STOPPING으로 변경
        ChangeState(WorkerState::STOPPING);
        
        // 폴링 스레드 정지
        stop_workers_ = true;
        if (polling_thread_.joinable()) {
            polling_thread_.join();
        }
        
        // 연결 해제
        CloseConnection();
        
        // 상태를 STOPPED로 변경
        ChangeState(WorkerState::STOPPED);
        
        LogModbusMessage(LogLevel::INFO, "Modbus RTU worker stopped");
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "Stop failed: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

WorkerState ModbusRtuWorker::GetState() const {
    return BaseDeviceWorker::GetState();  // SerialBasedWorker 대신 BaseDeviceWorker 직접 호출
}

// =============================================================================
// SerialBasedWorker 인터페이스 구현
// =============================================================================

bool ModbusRtuWorker::EstablishProtocolConnection() {
    std::lock_guard<std::mutex> lock(bus_mutex_);
    
    try {
        // SerialBasedWorker의 시리얼 설정 접근
        std::string port = serial_config_.port_name;
        int baud_rate = serial_config_.baud_rate;
        char parity = serial_config_.parity;
        int data_bits = serial_config_.data_bits;
        int stop_bits = serial_config_.stop_bits;
        
        LogModbusMessage(LogLevel::INFO, "Creating Modbus RTU context: " + port + 
                        " (" + std::to_string(baud_rate) + "," + std::string(1, parity) + 
                        "," + std::to_string(data_bits) + "," + std::to_string(stop_bits) + ")");
        
        modbus_ctx_ = modbus_new_rtu(port.c_str(), baud_rate, parity, data_bits, stop_bits);
        if (!modbus_ctx_) {
            LogModbusMessage(LogLevel::ERROR, "Failed to create Modbus RTU context: " + 
                           std::string(strerror(errno)));
            return false;
        }
        
        // 타임아웃 설정
        modbus_set_response_timeout(modbus_ctx_, response_timeout_ms_ / 1000, 
                                   (response_timeout_ms_ % 1000) * 1000);
        modbus_set_byte_timeout(modbus_ctx_, byte_timeout_ms_ / 1000, 
                               (byte_timeout_ms_ % 1000) * 1000);
        
        // Modbus 연결
        if (modbus_connect(modbus_ctx_) == -1) {
            LogModbusMessage(LogLevel::ERROR, "Failed to connect Modbus RTU: " + 
                           std::string(modbus_strerror(errno)));
            modbus_free(modbus_ctx_);
            modbus_ctx_ = nullptr;
            return false;
        }
        
        LogModbusMessage(LogLevel::INFO, "Modbus RTU protocol connection established");
        return true;
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "EstablishProtocolConnection failed: " + 
                        std::string(e.what()));
        if (modbus_ctx_) {
            modbus_free(modbus_ctx_);
            modbus_ctx_ = nullptr;
        }
        return false;
    }
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    std::lock_guard<std::mutex> lock(bus_mutex_);
    
    if (modbus_ctx_) {
        modbus_close(modbus_ctx_);
        modbus_free(modbus_ctx_);
        modbus_ctx_ = nullptr;
        LogModbusMessage(LogLevel::INFO, "Modbus RTU protocol connection closed");
    }
    
    return true;  // bool 타입 반환
}

bool ModbusRtuWorker::CheckProtocolConnection() {
    if (!modbus_ctx_) {
        return false;
    }
    
    // 기본 슬레이브로 간단한 읽기 테스트
    std::lock_guard<std::mutex> lock(bus_mutex_);
    
    modbus_set_slave(modbus_ctx_, default_slave_id_);
    uint16_t data[1];
    
    // 홀딩 레지스터 0번 주소 1개 읽기 시도
    int result = modbus_read_input_registers(modbus_ctx_, 0, 1, data);  // holding_registers 대신 input_registers 사용
    
    if (result == -1) {
        int error = errno;
        if (error != EMBXILFUN && error != EMBXILADD) {  // 기능 코드나 주소 에러가 아닌 경우
            LogModbusMessage(LogLevel::DEBUG, "Connection check failed: " + 
                           std::string(modbus_strerror(errno)));
            return false;
        }
    }
    
    return true;  // 통신 자체는 성공 (응답이 있음)
}

bool ModbusRtuWorker::SendProtocolKeepAlive() {
    if (!modbus_ctx_) {
        return false;
    }
    
    // 등록된 슬레이브들에게 Keep-alive 전송
    std::lock_guard<std::mutex> slaves_lock(slaves_mutex_);
    bool any_response = false;
    
    for (auto& [slave_id, slave_info] : slaves_) {
        if (slave_info->is_online) {
            int response_time = CheckSlaveStatus(slave_id);
            if (response_time >= 0) {
                any_response = true;
                UpdateSlaveStatus(slave_id, response_time, true);
            } else {
                slave_info->is_online = false;
                LogModbusMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + 
                               " is offline");
            }
        }
    }
    
    return any_response;
}

// =============================================================================
// Modbus RTU 특화 설정 관리
// =============================================================================

bool ModbusRtuWorker::ConfigureModbusRtu(int default_slave_id, 
                                         int response_timeout,
                                         int byte_timeout) {
    if (default_slave_id < 1 || default_slave_id > 247) {
        LogModbusMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(default_slave_id));
        return false;
    }
    
    default_slave_id_ = default_slave_id;
    response_timeout_ms_ = response_timeout;
    byte_timeout_ms_ = byte_timeout;
    
    // Modbus 컨텍스트가 이미 생성된 경우 타임아웃 업데이트
    if (modbus_ctx_) {
        std::lock_guard<std::mutex> lock(bus_mutex_);
        modbus_set_response_timeout(modbus_ctx_, response_timeout_ms_ / 1000, 
                                   (response_timeout_ms_ % 1000) * 1000);
        modbus_set_byte_timeout(modbus_ctx_, byte_timeout_ms_ / 1000, 
                               (byte_timeout_ms_ % 1000) * 1000);
    }
    
    LogModbusMessage(LogLevel::INFO, "Modbus RTU configured - Slave ID: " + 
                    std::to_string(default_slave_id_) + ", Timeouts: " + 
                    std::to_string(response_timeout_ms_) + "/" + 
                    std::to_string(byte_timeout_ms_) + "ms");
    
    return true;
}

bool ModbusRtuWorker::AddSlave(int slave_id, const std::string& description) {
    if (slave_id < 1 || slave_id > 247) {
        LogModbusMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
        return false;
    }
    
    std::lock_guard<std::mutex> lock(slaves_mutex_);
    
    if (slaves_.find(slave_id) != slaves_.end()) {
        LogModbusMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + " already exists");
        return false;
    }
    
    auto slave_info = std::make_unique<ModbusSlaveInfo>(slave_id);
    slaves_[slave_id] = std::move(slave_info);
    
    LogModbusMessage(LogLevel::INFO, "Added slave " + std::to_string(slave_id) + 
                    (description.empty() ? "" : " (" + description + ")"));
    
    return true;
}

bool ModbusRtuWorker::RemoveSlave(int slave_id) {
    std::lock_guard<std::mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it == slaves_.end()) {
        LogModbusMessage(LogLevel::WARN, "Slave " + std::to_string(slave_id) + " not found");
        return false;
    }
    
    slaves_.erase(it);
    LogModbusMessage(LogLevel::INFO, "Removed slave " + std::to_string(slave_id));
    
    return true;
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
    if (!ValidatePollingGroup(slave_id, register_type, start_address, register_count)) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    uint32_t group_id = next_group_id_++;
    
    ModbusPollingGroup group;
    group.group_id = group_id;
    group.group_name = group_name;
    group.slave_id = slave_id;
    group.register_type = register_type;
    group.start_address = start_address;
    group.register_count = register_count;
    group.polling_interval_ms = polling_interval_ms;
    group.enabled = true;
    group.next_poll_time = system_clock::now();
    
    polling_groups_[group_id] = group;  // std::move 제거
    
    LogModbusMessage(LogLevel::INFO, "Added polling group '" + group_name + "' (ID: " + 
                    std::to_string(group_id) + ") - Slave: " + std::to_string(slave_id) + 
                    ", Type: " + std::to_string(static_cast<int>(register_type)) + 
                    ", Address: " + std::to_string(start_address) + 
                    ", Count: " + std::to_string(register_count) + 
                    ", Interval: " + std::to_string(polling_interval_ms) + "ms");
    
    return group_id;
}

bool ModbusRtuWorker::RemovePollingGroup(uint32_t group_id) {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogModbusMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + 
                        " not found");
        return false;
    }
    
    LogModbusMessage(LogLevel::INFO, "Removed polling group '" + it->second.group_name + 
                    "' (ID: " + std::to_string(group_id) + ")");
    
    polling_groups_.erase(it);
    return true;
}

bool ModbusRtuWorker::SetPollingGroupEnabled(uint32_t group_id, bool enabled) {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    auto it = polling_groups_.find(group_id);
    if (it == polling_groups_.end()) {
        LogModbusMessage(LogLevel::WARN, "Polling group " + std::to_string(group_id) + 
                        " not found");
        return false;
    }
    
    it->second.enabled = enabled;
    LogModbusMessage(LogLevel::INFO, "Polling group " + std::to_string(group_id) + 
                    (enabled ? " enabled" : " disabled"));
    
    return true;
}

// =============================================================================
// 동기 읽기/쓰기 작업
// =============================================================================

bool ModbusRtuWorker::ReadRegisters(int slave_id,
                                    ModbusRegisterType register_type,
                                    uint16_t address,
                                    uint16_t count,
                                    std::vector<uint16_t>& values) {
    if (!modbus_ctx_) {
        LogModbusMessage(LogLevel::ERROR, "Modbus context not initialized");
        return false;
    }
    
    LockBus();  // 시리얼 버스 락
    
    auto start_time = steady_clock::now();
    bool success = false;
    
    try {
        // 슬레이브 설정
        modbus_set_slave(modbus_ctx_, slave_id);
        
        values.resize(count);
        int result = -1;
        
        // 레지스터 타입에 따른 읽기
        switch (register_type) {
            case ModbusRegisterType::COIL:
                {
                    std::vector<uint8_t> coil_data(count);
                    result = modbus_read_bits(modbus_ctx_, address, count, coil_data.data());  // modbus_read_coils 대신
                    if (result != -1) {
                        for (int i = 0; i < count; ++i) {
                            values[i] = coil_data[i] ? 1 : 0;
                        }
                    }
                }
                break;
                
            case ModbusRegisterType::DISCRETE_INPUT:
                {
                    std::vector<uint8_t> input_data(count);
                    result = modbus_read_input_bits(modbus_ctx_, address, count, input_data.data());  // 올바른 함수명
                    if (result != -1) {
                        for (int i = 0; i < count; ++i) {
                            values[i] = input_data[i] ? 1 : 0;
                        }
                    }
                }
                break;
                
            case ModbusRegisterType::HOLDING_REGISTER:
                result = modbus_read_registers(modbus_ctx_, address, count, values.data());  // modbus_read_holding_registers 대신
                break;
                
            case ModbusRegisterType::INPUT_REGISTER:
                result = modbus_read_input_registers(modbus_ctx_, address, count, values.data());
                break;
        }
        
        success = (result != -1);
        
        if (!success) {
            int error = errno;
            std::string error_type = "unknown";
            
            if (error == EMBXSFAIL) error_type = "crc";
            else if (error == ETIMEDOUT) error_type = "timeout";
            else error_type = "bus";
            
            LogModbusMessage(LogLevel::ERROR, "Read failed - Slave: " + std::to_string(slave_id) + 
                           ", Type: " + std::to_string(static_cast<int>(register_type)) + 
                           ", Address: " + std::to_string(address) + 
                           ", Count: " + std::to_string(count) + 
                           ", Error: " + ModbusErrorToString(error));
            
            UpdateModbusStats("read", false, error_type);
        } else {
            UpdateModbusStats("read", true);
        }
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "Read exception: " + std::string(e.what()));
        UpdateModbusStats("read", false, "exception");
    }
    
    // 응답 시간 계산
    auto end_time = steady_clock::now();
    int response_time = duration_cast<milliseconds>(end_time - start_time).count();
    
    // 슬레이브 상태 업데이트
    UpdateSlaveStatus(slave_id, response_time, success);
    
    UnlockBus();  // 시리얼 버스 언락
    
    return success;
}

bool ModbusRtuWorker::WriteRegister(int slave_id,
                                   ModbusRegisterType register_type,
                                   uint16_t address,
                                   uint16_t value) {
    return WriteRegisters(slave_id, register_type, address, {value});
}

bool ModbusRtuWorker::WriteRegisters(int slave_id,
                                     ModbusRegisterType register_type,
                                     uint16_t address,
                                     const std::vector<uint16_t>& values) {
    if (!modbus_ctx_) {
        LogModbusMessage(LogLevel::ERROR, "Modbus context not initialized");
        return false;
    }
    
    if (register_type != ModbusRegisterType::COIL && 
        register_type != ModbusRegisterType::HOLDING_REGISTER) {
        LogModbusMessage(LogLevel::ERROR, "Invalid register type for write operation");
        return false;
    }
    
    LockBus();  // 시리얼 버스 락
    
    auto start_time = steady_clock::now();
    bool success = false;
    
    try {
        // 슬레이브 설정
        modbus_set_slave(modbus_ctx_, slave_id);
        
        int result = -1;
        
        if (register_type == ModbusRegisterType::COIL) {
            if (values.size() == 1) {
                // 단일 코일 쓰기
                result = modbus_write_bit(modbus_ctx_, address, values[0] ? 1 : 0);
            } else {
                // 다중 코일 쓰기
                std::vector<uint8_t> coil_data(values.size());
                for (size_t i = 0; i < values.size(); ++i) {
                    coil_data[i] = values[i] ? 1 : 0;
                }
                result = modbus_write_bits(modbus_ctx_, address, values.size(), coil_data.data());
            }
        } else {  // HOLDING_REGISTER
            if (values.size() == 1) {
                // 단일 레지스터 쓰기
                result = modbus_write_register(modbus_ctx_, address, values[0]);
            } else {
                // 다중 레지스터 쓰기
                result = modbus_write_registers(modbus_ctx_, address, values.size(), values.data());
            }
        }
        
        success = (result != -1);
        
        if (!success) {
            int error = errno;
            std::string error_type = "unknown";
            
            if (error == EMBXSFAIL) error_type = "crc";
            else if (error == ETIMEDOUT) error_type = "timeout";
            else error_type = "bus";
            
            LogModbusMessage(LogLevel::ERROR, "Write failed - Slave: " + std::to_string(slave_id) + 
                           ", Type: " + std::to_string(static_cast<int>(register_type)) + 
                           ", Address: " + std::to_string(address) + 
                           ", Count: " + std::to_string(values.size()) + 
                           ", Error: " + ModbusErrorToString(error));
            
            UpdateModbusStats("write", false, error_type);
        } else {
            UpdateModbusStats("write", true);
        }
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "Write exception: " + std::string(e.what()));
        UpdateModbusStats("write", false, "exception");
    }
    
    // 응답 시간 계산
    auto end_time = steady_clock::now();
    int response_time = duration_cast<milliseconds>(end_time - start_time).count();
    
    // 슬레이브 상태 업데이트
    UpdateSlaveStatus(slave_id, response_time, success);
    
    UnlockBus();  // 시리얼 버스 언락
    
    return success;
}

// =============================================================================
// 진단 및 모니터링
// =============================================================================

std::vector<int> ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int timeout_ms) {
    std::vector<int> found_slaves;
    
    if (!modbus_ctx_) {
        LogModbusMessage(LogLevel::ERROR, "Modbus context not initialized");
        return found_slaves;
    }
    
    LogModbusMessage(LogLevel::INFO, "Scanning slaves from " + std::to_string(start_id) + 
                    " to " + std::to_string(end_id) + " (timeout: " + 
                    std::to_string(timeout_ms) + "ms)");
    
    // 임시로 짧은 타임아웃 설정
    int original_timeout = response_timeout_ms_;
    modbus_set_response_timeout(modbus_ctx_, timeout_ms / 1000, (timeout_ms % 1000) * 1000);
    
    for (int slave_id = start_id; slave_id <= end_id; ++slave_id) {
        LockBus();
        
        modbus_set_slave(modbus_ctx_, slave_id);
        uint16_t data[1];
        
        // 홀딩 레지스터 0번 읽기 시도
        int result = modbus_read_input_registers(modbus_ctx_, 0, 1, data);  // holding_registers 대신
        
        if (result != -1 || errno == EMBXILFUN || errno == EMBXILADD) {
            // 응답이 있음 (에러여도 슬레이브가 응답한 것)
            found_slaves.push_back(slave_id);
            LogModbusMessage(LogLevel::INFO, "Found slave: " + std::to_string(slave_id));
            
            // 자동으로 슬레이브 추가
            AddSlave(slave_id, "Auto-discovered");
        }
        
        UnlockBus();
        
        // 다음 슬레이브 스캔 전 잠시 대기 (버스 안정화)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 원래 타임아웃 복원
    modbus_set_response_timeout(modbus_ctx_, original_timeout / 1000, (original_timeout % 1000) * 1000);
    
    LogModbusMessage(LogLevel::INFO, "Slave scan completed. Found " + 
                    std::to_string(found_slaves.size()) + " slaves");
    
    return found_slaves;
}

int ModbusRtuWorker::CheckSlaveStatus(int slave_id) {
    if (!modbus_ctx_) {
        return -1;
    }
    
    auto start_time = steady_clock::now();
    
    LockBus();
    modbus_set_slave(modbus_ctx_, slave_id);
    
    uint16_t data[1];
    int result = modbus_read_input_registers(modbus_ctx_, 0, 1, data);  // holding_registers 대신
    
    UnlockBus();
    
    if (result == -1 && errno != EMBXILFUN && errno != EMBXILADD) {
        return -1;  // 통신 실패
    }
    
    auto end_time = steady_clock::now();
    return duration_cast<milliseconds>(end_time - start_time).count();
}

std::string ModbusRtuWorker::GetModbusStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_reads\": " << total_reads_ << ",\n";
    ss << "  \"successful_reads\": " << successful_reads_ << ",\n";
    ss << "  \"total_writes\": " << total_writes_ << ",\n";
    ss << "  \"successful_writes\": " << successful_writes_ << ",\n";
    ss << "  \"crc_errors\": " << crc_errors_ << ",\n";
    ss << "  \"timeout_errors\": " << timeout_errors_ << ",\n";
    ss << "  \"bus_errors\": " << bus_errors_ << ",\n";
    
    // 성공률 계산
    uint64_t total_operations = total_reads_ + total_writes_;
    uint64_t successful_operations = successful_reads_ + successful_writes_;
    double success_rate = total_operations > 0 ? 
        (static_cast<double>(successful_operations) / total_operations * 100.0) : 0.0;
    
    ss << "  \"success_rate_percent\": " << std::fixed << std::setprecision(2) << success_rate << ",\n";
    ss << "  \"polling_groups_count\": " << polling_groups_.size() << ",\n";
    ss << "  \"active_slaves_count\": " << slaves_.size() << "\n";
    ss << "}";
    
    return ss.str();
}

std::string ModbusRtuWorker::GetSlaveList() const {
    std::lock_guard<std::mutex> lock(slaves_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"slaves\": [\n";
    
    bool first = true;
    for (const auto& [slave_id, slave_info] : slaves_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"slave_id\": " << slave_id << ",\n";
        ss << "      \"is_online\": " << (slave_info->is_online ? "true" : "false") << ",\n";
        ss << "      \"response_time_ms\": " << slave_info->response_time_ms.load() << ",\n";
        ss << "      \"total_requests\": " << slave_info->total_requests.load() << ",\n";
        ss << "      \"successful_requests\": " << slave_info->successful_requests.load() << ",\n";
        
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

std::string ModbusRtuWorker::GetPollingGroupList() const {
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
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
        ss << "      \"enabled\": " << (group.enabled ? "true" : "false") << "\n";
        ss << "    }";
    }
    
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// 고급 기능들
// =============================================================================

bool ModbusRtuWorker::BroadcastWrite(ModbusRegisterType register_type, 
                                     uint16_t address, 
                                     uint16_t value) {
    if (!modbus_ctx_) {
        LogModbusMessage(LogLevel::ERROR, "Modbus context not initialized");
        return false;
    }
    
    if (register_type != ModbusRegisterType::COIL && 
        register_type != ModbusRegisterType::HOLDING_REGISTER) {
        LogModbusMessage(LogLevel::ERROR, "Invalid register type for broadcast write");
        return false;
    }
    
    LockBus();
    
    // 브로드캐스트 주소 (0) 사용
    modbus_set_slave(modbus_ctx_, 0);
    
    bool success = false;
    
    try {
        int result = -1;
        
        if (register_type == ModbusRegisterType::COIL) {
            result = modbus_write_bit(modbus_ctx_, address, value ? 1 : 0);
        } else {  // HOLDING_REGISTER
            result = modbus_write_register(modbus_ctx_, address, value);
        }
        
        success = (result != -1);
        
        if (success) {
            LogModbusMessage(LogLevel::INFO, "Broadcast write successful - Type: " + 
                           std::to_string(static_cast<int>(register_type)) + 
                           ", Address: " + std::to_string(address) + 
                           ", Value: " + std::to_string(value));
        } else {
            LogModbusMessage(LogLevel::ERROR, "Broadcast write failed: " + 
                           ModbusErrorToString(errno));
        }
        
    } catch (const std::exception& e) {
        LogModbusMessage(LogLevel::ERROR, "Broadcast write exception: " + std::string(e.what()));
    }
    
    UnlockBus();
    
    return success;
}

void ModbusRtuWorker::SetBusLockTime(int lock_time_ms) {
    bus_lock_time_ms_ = lock_time_ms;
    LogModbusMessage(LogLevel::INFO, "Bus lock time set to " + std::to_string(lock_time_ms) + "ms");
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

void ModbusRtuWorker::PollingWorkerLoop() {
    LogModbusMessage(LogLevel::INFO, "Polling worker started");
    
    while (!stop_workers_) {
        try {
            auto current_time = system_clock::now();
            
            // 폴링 그룹들 처리
            {
                std::lock_guard<std::mutex> lock(polling_groups_mutex_);
                
                for (auto& [group_id, group] : polling_groups_) {
                    if (!group.enabled) continue;
                    
                    if (current_time >= group.next_poll_time) {
                        ProcessPollingGroup(group);
                        
                        // 다음 폴링 시간 설정
                        group.next_poll_time = current_time + 
                            milliseconds(group.polling_interval_ms);
                    }
                }
            }
            
            // 짧은 대기 (CPU 사용률 제어)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            LogModbusMessage(LogLevel::ERROR, "Polling worker exception: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    LogModbusMessage(LogLevel::INFO, "Polling worker stopped");
}

bool ModbusRtuWorker::ProcessPollingGroup(ModbusPollingGroup& group) {
    auto start_time = steady_clock::now();
    
    // 폴링 카운트는 별도 멤버 변수로 관리하거나 제거
    
    std::vector<uint16_t> values;
    bool success = ReadRegisters(group.slave_id, group.register_type, 
                                group.start_address, group.register_count, values);
    
    if (success) {
        auto end_time = steady_clock::now();
        int response_time = duration_cast<milliseconds>(end_time - start_time).count();
        group.last_poll_time = system_clock::now();
        
        // 데이터 포인트들에 값 할당 및 저장
        for (size_t i = 0; i < group.data_points.size() && i < values.size(); ++i) {
            Drivers::TimestampedValue timestamped_value;
            timestamped_value.value = static_cast<double>(values[i]);
            timestamped_value.timestamp = system_clock::now();
            timestamped_value.quality = Drivers::DataQuality::GOOD;  // enum 타입 사용
            
            // Redis 및 InfluxDB 저장
            SaveToInfluxDB(group.data_points[i].id, timestamped_value);
        }
        
        LogModbusMessage(LogLevel::DEBUG, "Polling group '" + group.group_name + 
                        "' processed successfully (" + std::to_string(response_time) + "ms)");
    } else {
        LogModbusMessage(LogLevel::WARN, "Polling group '" + group.group_name + 
                        "' failed - Slave: " + std::to_string(group.slave_id));
    }
    
    return success;
}

std::string ModbusRtuWorker::ModbusErrorToString(int error_code) const {
    switch (error_code) {
        case EMBXILFUN:  return "Illegal function";
        case EMBXILADD:  return "Illegal data address";
        case EMBXILVAL:  return "Illegal data value";
        case EMBXSFAIL:  return "Slave device failure";
        case EMBXACK:    return "Acknowledge";
        case EMBXSBUSY:  return "Slave device busy";
        case EMBXNACK:   return "Negative acknowledge";
        case EMBXMEMPAR: return "Memory parity error";
        case EMBXGPATH:  return "Gateway path unavailable";
        case EMBXGTAR:   return "Gateway target device failed to respond";
        case ETIMEDOUT:  return "Connection timed out";
        case ECONNRESET: return "Connection reset by peer";
        case ECONNREFUSED: return "Connection refused";
        default:         return "Unknown error (" + std::to_string(error_code) + ")";
    }
}

void ModbusRtuWorker::UpdateModbusStats(const std::string& operation, bool success, 
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
    
    // 에러 타입별 통계
    if (!success) {
        if (error_type == "crc") {
            crc_errors_++;
        } else if (error_type == "timeout") {
            timeout_errors_++;
        } else if (error_type == "bus") {
            bus_errors_++;
        }
    }
}

void ModbusRtuWorker::UpdateSlaveStatus(int slave_id, int response_time_ms, bool success) {
    std::lock_guard<std::mutex> lock(slaves_mutex_);
    
    auto it = slaves_.find(slave_id);
    if (it != slaves_.end()) {
        auto& slave_info = it->second;
        
        slave_info->total_requests++;
        if (success) {
            slave_info->successful_requests++;
            slave_info->is_online = true;
            slave_info->response_time_ms = response_time_ms;
            slave_info->last_response = system_clock::now();
            slave_info->last_error.clear();
        } else {
            slave_info->is_online = false;
            slave_info->last_error = ModbusErrorToString(errno);
        }
    }
}

void ModbusRtuWorker::LockBus() {
    bus_mutex_.lock();
    
    // 시리얼 버스 안정화를 위한 짧은 대기
    if (bus_lock_time_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(bus_lock_time_ms_));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

void ModbusRtuWorker::LogModbusMessage(LogLevel level, const std::string& message) const {
    // BaseDeviceWorker의 LogMessage 사용 (const_cast 필요)
    const_cast<ModbusRtuWorker*>(this)->LogMessage(level, "[ModbusRTU] " + message);
}

bool ModbusRtuWorker::ValidatePollingGroup(int slave_id, ModbusRegisterType register_type,
                                          uint16_t start_address, uint16_t register_count) const {
    // 슬레이브 ID 검증
    if (slave_id < 1 || slave_id > 247) {
        LogModbusMessage(LogLevel::ERROR, "Invalid slave ID: " + std::to_string(slave_id));
        return false;
    }
    
    // 레지스터 개수 검증
    if (register_count == 0 || register_count > 125) {  // Modbus 표준 제한
        LogModbusMessage(LogLevel::ERROR, "Invalid register count: " + std::to_string(register_count));
        return false;
    }
    
    // 주소 범위 검증 (0-65535)
    if (start_address + register_count > 65536) {
        LogModbusMessage(LogLevel::ERROR, "Address range overflow: " + 
                        std::to_string(start_address) + " + " + std::to_string(register_count));
        return false;
    }
    
    // 레지스터 타입별 제한 검증
    switch (register_type) {
        case ModbusRegisterType::COIL:
        case ModbusRegisterType::DISCRETE_INPUT:
            if (register_count > 2000) {  // 비트 타입 제한
                LogModbusMessage(LogLevel::ERROR, "Too many bits requested: " + 
                               std::to_string(register_count));
                return false;
            }
            break;
            
        case ModbusRegisterType::HOLDING_REGISTER:
        case ModbusRegisterType::INPUT_REGISTER:
            if (register_count > 125) {  // 16비트 레지스터 제한
                LogModbusMessage(LogLevel::ERROR, "Too many registers requested: " + 
                               std::to_string(register_count));
                return false;
            }
            break;
    }
    
    return true;
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne