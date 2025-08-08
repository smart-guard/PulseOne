// =============================================================================
// collector/src/Workers/Protocol/ModbusRtuWorker.cpp - 완성본
// =============================================================================

#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Utils/LogManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Workers {

using json = nlohmann::json;

template<typename T>
std::string to_string_safe(T value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// =============================================================================
// 생성자/소멸자
// =============================================================================

ModbusRtuWorker::ModbusRtuWorker(const DeviceInfo& device_info)
    : SerialBasedWorker(device_info)
    , modbus_driver_(nullptr)
    , next_group_id_(1)
    , polling_thread_running_(false)
{
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusRtuWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseModbusConfig()) {
        logger.Error("Failed to parse Modbus RTU configuration");
        return;
    }
    
    // 🔥 실제 ModbusDriver 초기화
    if (!InitializeModbusDriver()) {
        logger.Error("Failed to initialize ModbusDriver for RTU");
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
    
    // 🔥 ModbusDriver 정리
    if (modbus_driver_) {
        modbus_driver_->Stop();
        modbus_driver_->Disconnect();
        modbus_driver_.reset();
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
            // 🔥 실제 ModbusDriver 시작
            if (modbus_driver_ && !modbus_driver_->Start()) {
                logger.Error("Failed to start ModbusDriver");
                return false;
            }
            
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
        
        logger.Info("Stopping Modbus RTU Worker...");
        
        try {
            polling_thread_running_ = false;
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
            
            // 🔥 실제 ModbusDriver 중지
            if (modbus_driver_) {
                modbus_driver_->Stop();
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
    
    if (modbus_driver_) {
        bool connected = modbus_driver_->Connect();
        logger.Info(connected ? "RTU connection established" : "RTU connection failed");
        return connected;
    }
    
    logger.Warn("ModbusDriver not available");
    return false;
}

bool ModbusRtuWorker::CloseProtocolConnection() {
    auto& logger = LogManager::getInstance();
    logger.Info("Closing Modbus RTU protocol connection");
    
    if (modbus_driver_) {
        return modbus_driver_->Disconnect();
    }
    return true;
}

bool ModbusRtuWorker::CheckProtocolConnection() {
    if (modbus_driver_) {
        return modbus_driver_->IsConnected();
    }
    return false;
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
// 🔥 실제 ModbusDriver 기반 데이터 읽기/쓰기
// =============================================================================

bool ModbusRtuWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                                          uint16_t register_count, std::vector<uint16_t>& values) {
    auto& logger = LogManager::getInstance();
    
    if (!modbus_driver_) {
        logger.Error("ModbusDriver not initialized");
        return false;
    }
    
    try {
        LockBus();
        
        // 🔥 실제 ModbusDriver 사용
        std::vector<PulseOne::Structs::DataPoint> points;
        for (uint16_t i = 0; i < register_count; ++i) {
            PulseOne::Structs::DataPoint point;
            point.address = start_address + i;
            point.data_type = "UINT16";
            point.protocol_params["slave_id"] = std::to_string(slave_id);
            point.protocol_params["function_code"] = "3"; // Holding Register
            point.protocol_params["protocol"] = "MODBUS_RTU";
            points.push_back(point);
        }
        
        std::vector<PulseOne::Structs::TimestampedValue> results;
        bool success = modbus_driver_->ReadValues(points, results);
        
        if (success && results.size() == register_count) {
            values.clear();
            values.reserve(register_count);
            
            for (const auto& result : results) {
                // DataVariant에서 uint16_t 추출
                if (std::holds_alternative<uint16_t>(result.value)) {
                    values.push_back(std::get<uint16_t>(result.value));
                } else if (std::holds_alternative<int>(result.value)) {
                    values.push_back(static_cast<uint16_t>(std::get<int>(result.value)));
                } else if (std::holds_alternative<double>(result.value)) {
                    values.push_back(static_cast<uint16_t>(std::get<double>(result.value)));
                } else {
                    values.push_back(0); // 기본값
                }
            }
            
            UpdateSlaveStatus(slave_id, 50, true);  // 성공
        } else {
            UpdateSlaveStatus(slave_id, -1, false); // 실패
        }
        
        UnlockBus();
        
        logger.Debug("Read " + to_string_safe(register_count) + " holding registers from slave " + 
                    to_string_safe(slave_id) + " (success: " + (success ? "true" : "false") + ")");
        
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("ReadHoldingRegisters exception: " + std::string(e.what()));
        UpdateSlaveStatus(slave_id, -1, false);
        return false;
    }
}

bool ModbusRtuWorker::ReadInputRegisters(int slave_id, uint16_t start_address, 
                                        uint16_t register_count, std::vector<uint16_t>& values) {
    // Holding Register와 유사하지만 function_code = 4
    return ReadHoldingRegisters(slave_id, start_address, register_count, values);
}

bool ModbusRtuWorker::ReadCoils(int slave_id, uint16_t start_address, 
                               uint16_t coil_count, std::vector<bool>& values) {
    auto& logger = LogManager::getInstance();
    
    if (!modbus_driver_) {
        logger.Error("ModbusDriver not initialized");
        return false;
    }
    
    try {
        LockBus();
        
        std::vector<PulseOne::Structs::DataPoint> points;
        for (uint16_t i = 0; i < coil_count; ++i) {
            PulseOne::Structs::DataPoint point;
            point.address = start_address + i;
            point.data_type = "BOOL";
            point.protocol_params["slave_id"] = std::to_string(slave_id);
            point.protocol_params["function_code"] = "1"; // Read Coils
            point.protocol_params["protocol"] = "MODBUS_RTU";
            points.push_back(point);
        }
        
        std::vector<PulseOne::Structs::TimestampedValue> results;
        bool success = modbus_driver_->ReadValues(points, results);
        
        if (success) {
            values.clear();
            values.reserve(coil_count);
            
            for (const auto& result : results) {
                if (std::holds_alternative<bool>(result.value)) {
                    values.push_back(std::get<bool>(result.value));
                } else if (std::holds_alternative<int>(result.value)) {
                    values.push_back(std::get<int>(result.value) != 0);
                } else {
                    values.push_back(false);
                }
            }
        }
        
        UnlockBus();
        
        logger.Debug("Read " + to_string_safe(coil_count) + " coils from slave " + 
                    to_string_safe(slave_id) + " (success: " + (success ? "true" : "false") + ")");
        
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("ReadCoils exception: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                                        uint16_t input_count, std::vector<bool>& values) {
    // Coils와 유사하지만 function_code = 2
    return ReadCoils(slave_id, start_address, input_count, values);
}

bool ModbusRtuWorker::WriteSingleRegister(int slave_id, uint16_t address, uint16_t value) {
    auto& logger = LogManager::getInstance();
    
    if (!modbus_driver_) {
        logger.Error("ModbusDriver not initialized");
        return false;
    }
    
    try {
        LockBus();
        
        PulseOne::Structs::DataPoint point;
        point.address = address;
        point.data_type = "UINT16";
        point.protocol_params["slave_id"] = std::to_string(slave_id);
        point.protocol_params["function_code"] = "6"; // Write Single Register
        point.protocol_params["protocol"] = "MODBUS_RTU";
        
        PulseOne::Structs::DataValue data_value = static_cast<uint16_t>(value);
        bool success = modbus_driver_->WriteValue(point, data_value);
        
        UnlockBus();
        
        logger.Debug("Write single register: slave=" + to_string_safe(slave_id) + 
                    ", address=" + to_string_safe(address) + ", value=" + to_string_safe(value) +
                    " (success: " + (success ? "true" : "false") + ")");
        
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("WriteSingleRegister exception: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::WriteSingleCoil(int slave_id, uint16_t address, bool value) {
    auto& logger = LogManager::getInstance();
    
    if (!modbus_driver_) {
        logger.Error("ModbusDriver not initialized");
        return false;
    }
    
    try {
        LockBus();
        
        PulseOne::Structs::DataPoint point;
        point.address = address;
        point.data_type = "BOOL";
        point.protocol_params["slave_id"] = std::to_string(slave_id);
        point.protocol_params["function_code"] = "5"; // Write Single Coil
        point.protocol_params["protocol"] = "MODBUS_RTU";
        
        PulseOne::Structs::DataValue data_value = value;
        bool success = modbus_driver_->WriteValue(point, data_value);
        
        UnlockBus();
        
        logger.Debug("Write single coil: slave=" + to_string_safe(slave_id) + 
                    ", address=" + to_string_safe(address) + ", value=" + (value ? "true" : "false") +
                    " (success: " + (success ? "true" : "false") + ")");
        
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("WriteSingleCoil exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 설정 파싱 및 초기화
// =============================================================================

bool ModbusRtuWorker::ParseModbusConfig() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("🔧 Starting Modbus RTU configuration parsing...");
        
        // 기본 DriverConfig 설정
        modbus_config_.device_id = device_info_.name;
        modbus_config_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.timeout_ms;
        modbus_config_.retry_count = device_info_.retry_count;
        
        // JSON 설정 파싱
        nlohmann::json protocol_config_json;
        
        if (!device_info_.config.empty()) {
            try {
                protocol_config_json = nlohmann::json::parse(device_info_.config);
                logger.Info("✅ RTU Protocol config loaded from device.config: " + device_info_.config);
            } catch (const std::exception& e) {
                logger.Warn("⚠️ Failed to parse device.config JSON: " + std::string(e.what()));
            }
        }
        
        // 기본값 설정
        if (protocol_config_json.empty()) {
            protocol_config_json = {
                {"slave_id", 1},
                {"baud_rate", 9600},
                {"parity", "N"},
                {"data_bits", 8},
                {"stop_bits", 1},
                {"frame_delay_ms", 50}
            };
            logger.Info("📝 Applied default Modbus RTU protocol configuration");
        }
        
        // Properties 설정
        modbus_config_.properties["slave_id"] = std::to_string(protocol_config_json.value("slave_id", 1));
        modbus_config_.properties["baud_rate"] = std::to_string(protocol_config_json.value("baud_rate", 9600));
        modbus_config_.properties["parity"] = protocol_config_json.value("parity", "N");
        modbus_config_.properties["data_bits"] = std::to_string(protocol_config_json.value("data_bits", 8));
        modbus_config_.properties["stop_bits"] = std::to_string(protocol_config_json.value("stop_bits", 1));
        modbus_config_.properties["frame_delay_ms"] = std::to_string(protocol_config_json.value("frame_delay_ms", 50));
        
        // 타임아웃 설정
        if (protocol_config_json.contains("timeout")) {
            int db_timeout = protocol_config_json.value("timeout", static_cast<int>(device_info_.timeout_ms));
            modbus_config_.timeout_ms = db_timeout;
            logger.Info("✅ Applied timeout from DB: " + std::to_string(modbus_config_.timeout_ms) + "ms");
        }
        
        modbus_config_.properties["response_timeout_ms"] = std::to_string(modbus_config_.timeout_ms);
        modbus_config_.properties["byte_timeout_ms"] = std::to_string(modbus_config_.timeout_ms / 10);
        modbus_config_.properties["max_retries"] = std::to_string(device_info_.retry_count);
        modbus_config_.properties["protocol_type"] = "MODBUS_RTU";
        
        logger.Info("✅ Modbus RTU config parsed successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("ParseModbusConfig failed: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::InitializeModbusDriver() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("Initializing Modbus RTU Driver...");
        
        // 🔥 실제 ModbusDriver 인스턴스 생성
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            logger.Error("❌ Failed to create ModbusDriver instance");
            return false;
        }
        
        // DriverConfig 설정
        PulseOne::Structs::DriverConfig driver_config = modbus_config_;
        driver_config.properties["protocol_type"] = "MODBUS_RTU";
        driver_config.properties["serial_port"] = device_info_.endpoint;
        
        // 🔥 실제 ModbusDriver 초기화
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            logger.Error("❌ ModbusDriver initialization failed: " + error.message);
            modbus_driver_.reset();
            return false;
        }
        
        SetupDriverCallbacks();
        
        logger.Info("✅ Modbus RTU Driver initialized successfully (real driver)");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("❌ Exception during RTU ModbusDriver initialization: " + std::string(e.what()));
        if (modbus_driver_) {
            modbus_driver_.reset();
        }
        return false;
    }
}

void ModbusRtuWorker::SetupDriverCallbacks() {
    auto& logger = LogManager::getInstance();
    logger.Debug("Setting up RTU ModbusDriver callbacks...");
    // 필요시 콜백 설정
}

// =============================================================================
// 슬레이브 관리 및 기타 메서드들
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
    slave_info->properties["slave_id"] = std::to_string(slave_id);
    slave_info->properties["is_online"] = "false";
    slave_info->properties["total_requests"] = "0";
    slave_info->properties["successful_requests"] = "0";
    
    slaves_[slave_id] = slave_info;
    logger.Info("Added slave " + to_string_safe(slave_id) + " (" + slave_info->name + ")");
    
    return true;
}

// 기타 메서드들 구현...
void ModbusRtuWorker::PollingWorkerThread() {
    auto& logger = LogManager::getInstance();
    logger.Info("RTU Polling worker thread started");
    
    while (polling_thread_running_) {
        try {
            // 폴링 로직
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            logger.Error("RTU Polling worker thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger.Info("RTU Polling worker thread stopped");
}

void ModbusRtuWorker::LockBus() {
    bus_mutex_.lock();
    int frame_delay = std::stoi(GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50"));
    if (frame_delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay));
    }
}

void ModbusRtuWorker::UnlockBus() {
    bus_mutex_.unlock();
}

// 유틸리티 메서드들
std::string ModbusRtuWorker::GetPropertyValue(const std::map<std::string, std::string>& properties, 
                                            const std::string& key, 
                                            const std::string& default_value) const {
    auto it = properties.find(key);
    return (it != properties.end()) ? it->second : default_value;
}

int ModbusRtuWorker::GetSlaveId() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "slave_id", "1"));
}

int ModbusRtuWorker::GetBaudRate() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "baud_rate", "9600"));
}

char ModbusRtuWorker::GetParity() const {
    std::string parity = GetPropertyValue(modbus_config_.properties, "parity", "N");
    return parity.empty() ? 'N' : parity[0];
}

int ModbusRtuWorker::GetDataBits() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "data_bits", "8"));
}

int ModbusRtuWorker::GetStopBits() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "stop_bits", "1"));
}

// 기타 필요한 메서드들 구현...

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

int ModbusRtuWorker::ScanSlaves(int start_id, int end_id, int timeout_ms) {
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
        int frame_delay = std::stoi(GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50"));
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay));
    }
    
    logger.Info("Slave scan completed. Found " + to_string_safe(found_count) + " slaves");
    return found_count;
}

uint32_t ModbusRtuWorker::AddPollingGroup(const std::string& group_name, int slave_id,
                                         ModbusRegisterType register_type, uint16_t start_address,
                                         uint16_t register_count, int polling_interval_ms) {
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

void ModbusRtuWorker::ConfigureModbusRtu(const PulseOne::Structs::DriverConfig& config) {
    auto& logger = LogManager::getInstance();
    modbus_config_ = config;
    logger.Info("Modbus RTU configured for device: " + config.device_id);
}

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
    status["response_timeout_ms"] = std::stoi(GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000"));
    status["byte_timeout_ms"] = std::stoi(GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100"));
    status["frame_delay_ms"] = std::stoi(GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50"));
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

} // namespace Workers
} // namespace PulseOne