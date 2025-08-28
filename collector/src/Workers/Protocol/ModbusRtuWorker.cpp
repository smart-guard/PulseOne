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
        
        PulseOne::Structs::DataPoint point;
        point.address = start_address;
        point.data_type = "UINT16";
        point.protocol_params["slave_id"] = std::to_string(slave_id);
        point.protocol_params["function_code"] = "3"; // Read Holding Registers
        point.protocol_params["register_count"] = std::to_string(register_count);
        point.protocol_params["protocol"] = "MODBUS_RTU";
        
        std::vector<PulseOne::Structs::TimestampedValue> results;
        bool success = modbus_driver_->ReadValues({point}, results);
        
        UnlockBus();
        
        if (success && !results.empty()) {
            values.clear();
            values.reserve(results.size());
            
            for (const auto& result : results) {
                // DataValue에서 uint16_t 추출
                if (std::holds_alternative<int32_t>(result.value)) {
                    values.push_back(static_cast<uint16_t>(std::get<int32_t>(result.value)));
                }
            }
            
            // 🔥 파이프라인 전송 추가
            if (!values.empty()) {
                SendModbusRtuDataToPipeline(values, start_address, "holding", 0);
            }
            
            logger.Debug("Read holding registers: slave=" + to_string_safe(slave_id) + 
                        ", start=" + to_string_safe(start_address) + 
                        ", count=" + to_string_safe(register_count) + 
                        ", result_count=" + to_string_safe(values.size()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        UnlockBus();
        logger.Error("ReadHoldingRegisters exception: " + std::string(e.what()));
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
        
        PulseOne::Structs::DataPoint point;
        point.address = start_address;
        point.data_type = "BOOL";
        point.protocol_params["slave_id"] = std::to_string(slave_id);
        point.protocol_params["function_code"] = "1"; // Read Coils
        point.protocol_params["register_count"] = std::to_string(coil_count);
        point.protocol_params["protocol"] = "MODBUS_RTU";
        
        std::vector<PulseOne::Structs::TimestampedValue> results;
        bool success = modbus_driver_->ReadValues({point}, results);
        
        UnlockBus();
        
        if (success && !results.empty()) {
            values.clear();
            values.reserve(results.size());
            
            for (const auto& result : results) {
                // DataValue에서 bool 추출
                if (std::holds_alternative<bool>(result.value)) {
                    values.push_back(std::get<bool>(result.value));
                }
            }
            
            // 🔥 파이프라인 전송 추가
            if (!values.empty()) {
                SendModbusRtuBoolDataToPipeline(values, start_address, "coil", 0);
            }
            
            logger.Debug("Read coils: slave=" + to_string_safe(slave_id) + 
                        ", start=" + to_string_safe(start_address) + 
                        ", count=" + to_string_safe(coil_count) + 
                        " (success: " + (success ? "true" : "false") + ")");
        }
        
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
        
        // 🔥 제어 이력 파이프라인 전송 추가
        if (success) {
            TimestampedValue control_log;
            control_log.value = static_cast<int32_t>(value);
            control_log.timestamp = std::chrono::system_clock::now();
            control_log.quality = DataQuality::GOOD;
            control_log.source = "control_holding_" + std::to_string(address) + 
                                "_slave" + std::to_string(slave_id);
            
            // 제어 이력은 높은 우선순위로 기록
            SendValuesToPipelineWithLogging({control_log}, "제어 이력", 1);
        }
        
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
        
        // 🔥 제어 이력 파이프라인 전송 추가
        if (success) {
            TimestampedValue control_log;
            control_log.value = value;
            control_log.timestamp = std::chrono::system_clock::now();
            control_log.quality = DataQuality::GOOD;
            control_log.source = "control_coil_" + std::to_string(address) + 
                                "_slave" + std::to_string(slave_id);
            
            // 제어 이력은 높은 우선순위로 기록
            SendValuesToPipelineWithLogging({control_log}, "제어 이력", 1);
        }
        
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

/**
 * @brief ModbusRtuWorker.cpp - InitializeModbusDriver() 완전 수정
 * @details ModbusTcpWorker와 동일한 수준의 완전한 설정 적용
 */

bool ModbusRtuWorker::InitializeModbusDriver() {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("🔧 Initializing Modbus RTU Driver...");
        
        // 🔥 실제 ModbusDriver 인스턴스 생성
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            logger.Error("❌ Failed to create ModbusDriver instance");
            return false;
        }
        
        logger.Debug("✅ ModbusDriver instance created");
        
        // =======================================================================
        // 🔥 핵심 수정: device_info_.driver_config 직접 사용 (ModbusTcpWorker와 동일)
        // =======================================================================
        
        // WorkerFactory에서 완전 매핑된 DriverConfig 사용
        PulseOne::Structs::DriverConfig driver_config = device_info_.driver_config;
        
        // =======================================================================
        // 기본 필드 업데이트 (RTU 특화)
        // =======================================================================
        driver_config.device_id = device_info_.name;
        driver_config.endpoint = device_info_.endpoint;
        driver_config.protocol = PulseOne::Enums::ProtocolType::MODBUS_RTU;
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        
        // 🔥 수정: DeviceInfo 필드들 안전하게 접근 (ModbusTcpWorker와 동일)
        if (device_info_.retry_count > 0) {
            driver_config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
        } else {
            driver_config.retry_count = 3; // 기본값
        }
        
        if (device_info_.polling_interval_ms > 0) {
            driver_config.polling_interval_ms = static_cast<uint32_t>(device_info_.polling_interval_ms);
        } else {
            driver_config.polling_interval_ms = 1000; // 기본값
        }
        
        // 🔥 수정: auto_reconnect는 DriverConfig에서 기본값 사용 또는 properties에서 설정
        if (device_info_.properties.count("auto_reconnect")) {
            driver_config.auto_reconnect = (device_info_.properties.at("auto_reconnect") == "true");
        } else {
            driver_config.auto_reconnect = true; // 기본값: 자동 재연결 활성화
        }
        
        // =======================================================================
        // 🔥 Modbus RTU 프로토콜 특화 설정들 추가 (ModbusTcpWorker 패턴 적용)
        // =======================================================================
        
        // 기존 properties가 이미 WorkerFactory에서 설정되었으므로 유지
        // Modbus RTU 특화 설정만 추가
        driver_config.properties["protocol_type"] = "MODBUS_RTU";
        driver_config.properties["serial_port"] = device_info_.endpoint;
        
        // Modbus 공통 설정들 (ModbusTcpWorker와 동일)
        driver_config.properties["slave_id"] = GetPropertyValue(modbus_config_.properties, "slave_id", "1");
        driver_config.properties["byte_order"] = GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian");
        driver_config.properties["max_retries"] = GetPropertyValue(modbus_config_.properties, "max_retries", "3");
        driver_config.properties["response_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000");
        driver_config.properties["byte_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100");
        driver_config.properties["max_registers_per_group"] = GetPropertyValue(modbus_config_.properties, "max_registers_per_group", "125");
        
        // RTU 특화 시리얼 설정들
        driver_config.properties["baud_rate"] = GetPropertyValue(modbus_config_.properties, "baud_rate", "9600");
        driver_config.properties["parity"] = GetPropertyValue(modbus_config_.properties, "parity", "N");
        driver_config.properties["data_bits"] = GetPropertyValue(modbus_config_.properties, "data_bits", "8");
        driver_config.properties["stop_bits"] = GetPropertyValue(modbus_config_.properties, "stop_bits", "1");
        driver_config.properties["frame_delay_ms"] = GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50");
        
        // =======================================================================
        // 🔥 중요: properties 상태 로깅 (디버깅용) - ModbusTcpWorker와 동일
        // =======================================================================
        logger.Info("📊 Final DriverConfig state:");
        logger.Info("   - properties count: " + std::to_string(driver_config.properties.size()));
        logger.Info("   - timeout_ms: " + std::to_string(driver_config.timeout_ms));
        logger.Info("   - retry_count: " + std::to_string(driver_config.retry_count));
        logger.Info("   - auto_reconnect: " + std::string(driver_config.auto_reconnect ? "true" : "false"));
        
        // DeviceSettings 핵심 필드들 확인 (ModbusTcpWorker와 동일)
        std::vector<std::string> key_fields = {
            "retry_interval_ms", "backoff_time_ms", "keep_alive_enabled",
            "slave_id", "byte_order", "baud_rate", "parity"  // RTU 특화 필드 추가
        };
        
        logger.Info("📋 Key properties status:");
        for (const auto& field : key_fields) {
            if (driver_config.properties.count(field)) {
                logger.Info("   ✅ " + field + ": " + driver_config.properties.at(field));
            } else {
                logger.Warn("   ❌ " + field + ": NOT FOUND");
            }
        }
        
        // =======================================================================
        // ModbusDriver 초기화 (ModbusTcpWorker와 동일한 로깅)
        // =======================================================================
        
        std::string config_msg = "📋 DriverConfig prepared:\n";
        config_msg += "   - device_id: " + driver_config.device_id + "\n";
        config_msg += "   - endpoint: " + driver_config.endpoint + "\n";
        config_msg += "   - protocol: MODBUS_RTU\n";
        config_msg += "   - timeout: " + std::to_string(driver_config.timeout_ms) + "ms\n";
        config_msg += "   - properties: " + std::to_string(driver_config.properties.size()) + " items";
        
        logger.Debug(config_msg);
        
        // 🔥 실제 ModbusDriver 초기화
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            logger.Error("❌ ModbusDriver initialization failed: " + error.message + 
                        " (code: " + std::to_string(static_cast<int>(error.code)) + ")");
            modbus_driver_.reset();
            return false;
        }
        
        logger.Debug("✅ ModbusDriver initialization successful");
        
        // Driver 콜백 설정
        SetupDriverCallbacks();
        
        // 최종 결과 로깅 (ModbusTcpWorker와 동일한 상세도)
        std::string final_msg = "✅ Modbus RTU Driver initialized successfully:\n";
        final_msg += "   📡 Connection details:\n";
        final_msg += "      - serial_port: " + device_info_.endpoint + "\n";
        final_msg += "      - slave_id: " + GetPropertyValue(modbus_config_.properties, "slave_id", "1") + "\n";
        final_msg += "      - timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        final_msg += "   ⚙️  RTU settings:\n";
        final_msg += "      - baud_rate: " + GetPropertyValue(modbus_config_.properties, "baud_rate", "9600") + "\n";
        final_msg += "      - parity: " + GetPropertyValue(modbus_config_.properties, "parity", "N") + "\n";
        final_msg += "      - data_bits: " + GetPropertyValue(modbus_config_.properties, "data_bits", "8") + "\n";
        final_msg += "      - stop_bits: " + GetPropertyValue(modbus_config_.properties, "stop_bits", "1") + "\n";
        final_msg += "   🔧 Advanced settings:\n";
        final_msg += "      - byte_order: " + GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian") + "\n";
        final_msg += "      - max_retries: " + GetPropertyValue(modbus_config_.properties, "max_retries", "3") + "\n";
        final_msg += "      - response_timeout: " + GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000") + "ms\n";
        final_msg += "      - frame_delay: " + GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50") + "ms\n";
        final_msg += "   📊 Total properties: " + std::to_string(driver_config.properties.size());
        
        logger.Info(final_msg);
        
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
            // 🔥 기존 폴링 로직에 파이프라인 전송 추가
            for (auto& [slave_id, slave_info] : slaves_) {
                if (!slave_info || !slave_info->enabled) {
                    continue;
                }
                
                // 예시: Holding Register 스캔
                std::vector<uint16_t> holding_values;
                if (ReadHoldingRegisters(slave_id, 0, 10, holding_values)) {
                    // ReadHoldingRegisters 내부에서 이미 파이프라인 전송됨
                    logger.Debug("Polled slave " + std::to_string(slave_id) + 
                                ": " + std::to_string(holding_values.size()) + " values");
                }
                
                // 예시: Coil 스캔
                std::vector<bool> coil_values;
                if (ReadCoils(slave_id, 0, 8, coil_values)) {
                    // ReadCoils 내부에서 이미 파이프라인 전송됨
                    logger.Debug("Polled coils slave " + std::to_string(slave_id) + 
                                ": " + std::to_string(coil_values.size()) + " values");
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 1초 간격
            
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


bool ModbusRtuWorker::SendModbusRtuDataToPipeline(const std::vector<uint16_t>& raw_values, 
                                                  uint16_t start_address,
                                                  const std::string& register_type,
                                                  uint32_t priority) {
    if (raw_values.empty()) {
        return false;
    }
    
    try {
        std::vector<PulseOne::Structs::TimestampedValue> timestamped_values;
        timestamped_values.reserve(raw_values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        
        // 🔥 protected 멤버에 접근
        const auto& current_data_points = GetDataPoints();
        
        for (size_t i = 0; i < raw_values.size(); ++i) {
            uint32_t modbus_address = start_address + i;
            
            // 🔥 DataPoint 찾기
            const PulseOne::Structs::DataPoint* data_point = nullptr;
            for (const auto& dp : current_data_points) {
                if (dp.address == modbus_address) {
                    data_point = &dp;
                    break;
                }
            }
            
            if (!data_point) {
                LogMessage(LogLevel::DEBUG_LEVEL, "설정되지 않은 Modbus RTU 주소: " + std::to_string(modbus_address));
                continue; // 🔥 설정되지 않은 주소는 건너뛰기
            }
            
            PulseOne::Structs::TimestampedValue tv;
            
            // 🔥 핵심 필드들
            tv.value = static_cast<int32_t>(raw_values[i]);
            tv.timestamp = timestamp;
            tv.quality = PulseOne::Enums::DataQuality::GOOD;
            tv.point_id = std::stoi(data_point->id);
            tv.source = "modbus_rtu_" + register_type + "_" + std::to_string(modbus_address);
            
            // 🔥 상태변화 감지 (이전값 비교) - protected 멤버 접근
            auto prev_it = previous_values_.find(tv.point_id);
            if (prev_it != previous_values_.end()) {
                tv.previous_value = prev_it->second;
                tv.value_changed = (tv.value != prev_it->second);
            } else {
                tv.previous_value = PulseOne::Structs::DataValue{};
                tv.value_changed = true; // 첫 수집은 변화로 간주
            }
            
            // 🔥 DataPoint 설정값들 적용
            tv.change_threshold = data_point->log_deadband;
            tv.force_rdb_store = tv.value_changed || (data_point->log_enabled && data_point->log_interval_ms <= 1000);
            tv.sequence_number = GetNextSequenceNumber();
            tv.raw_value = static_cast<double>(raw_values[i]);
            tv.scaling_factor = data_point->scaling_factor;
            tv.scaling_offset = data_point->scaling_offset;
            
            // 🔥 스케일링 적용 (필요시)
            if (data_point->scaling_factor != 1.0 || data_point->scaling_offset != 0.0) {
                double scaled_value = (static_cast<double>(raw_values[i]) * data_point->scaling_factor) + data_point->scaling_offset;
                tv.value = scaled_value;
            }
            
            timestamped_values.push_back(tv);
            
            // 🔥 이전값 캐시 업데이트 - protected 멤버 접근
            previous_values_[tv.point_id] = tv.value;
        }
        
        // 실제로 전송할 데이터가 있는 경우만 전송
        if (timestamped_values.empty()) {
            LogMessage(LogLevel::DEBUG_LEVEL, "전송할 설정된 데이터포인트가 없음: " + register_type + " " + std::to_string(start_address));
            return true; // 에러는 아니므로 true 반환
        }
        
        // 🔥 BaseDeviceWorker::SendValuesToPipelineWithLogging() 호출
        bool success = SendValuesToPipelineWithLogging(timestamped_values, 
                                                       "RTU " + register_type + " registers", 
                                                       priority);
        
        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Modbus RTU 데이터 전송 성공: " + std::to_string(timestamped_values.size()) + 
                      "/" + std::to_string(raw_values.size()) + " 포인트 (주소 " + 
                      std::to_string(start_address) + "-" + std::to_string(start_address + raw_values.size() - 1) + ")");
        }
        
        return success;
                                               
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendModbusRtuDataToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}


// 🔥 2-2. bool 값들 (Coil/Discrete Input) 파이프라인 전송
bool ModbusRtuWorker::SendModbusRtuBoolDataToPipeline(const std::vector<bool>& raw_values,
                                                     uint16_t start_address,
                                                     const std::string& register_type,
                                                     uint32_t priority) {
    if (raw_values.empty()) {
        return false;
    }
    
    try {
        std::vector<PulseOne::Structs::TimestampedValue> timestamped_values;
        timestamped_values.reserve(raw_values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        
        // 🔥 protected 멤버에 접근
        const auto& current_data_points = GetDataPoints();
        
        for (size_t i = 0; i < raw_values.size(); ++i) {
            uint32_t modbus_address = start_address + i;
            
            // 🔥 DataPoint 찾기
            const PulseOne::Structs::DataPoint* data_point = nullptr;
            for (const auto& dp : current_data_points) {
                if (dp.address == modbus_address) {
                    data_point = &dp;
                    break;
                }
            }
            
            if (!data_point) {
                LogMessage(LogLevel::DEBUG_LEVEL, "설정되지 않은 Modbus RTU 주소: " + std::to_string(modbus_address));
                continue;
            }
            
            PulseOne::Structs::TimestampedValue tv;
            
            // 🔥 핵심 필드들
            tv.value = static_cast<bool>(raw_values[i]);
            tv.timestamp = timestamp;
            tv.quality = PulseOne::Enums::DataQuality::GOOD;
            tv.point_id = std::stoi(data_point->id);
            tv.source = "modbus_rtu_" + register_type + "_" + std::to_string(modbus_address);
            
            // 🔥 상태변화 감지 (디지털 신호)
            auto prev_it = previous_values_.find(tv.point_id);
            if (prev_it != previous_values_.end()) {
                tv.previous_value = prev_it->second;
                tv.value_changed = (tv.value != prev_it->second);
            } else {
                tv.previous_value = PulseOne::Structs::DataValue{};
                tv.value_changed = true;
            }
            
            // 🔥 DataPoint 설정값들 적용
            tv.change_threshold = 0.0; // 디지털은 임계값 없음
            tv.force_rdb_store = tv.value_changed; // 디지털은 상태변화시만 저장
            tv.sequence_number = GetNextSequenceNumber();
            tv.raw_value = static_cast<double>(raw_values[i]); // 0.0 또는 1.0
            tv.scaling_factor = 1.0; // 디지털은 스케일링 없음
            tv.scaling_offset = 0.0;
            
            timestamped_values.push_back(tv);
            
            // 🔥 이전값 캐시 업데이트
            previous_values_[tv.point_id] = tv.value;
        }
        
        if (timestamped_values.empty()) {
            LogMessage(LogLevel::DEBUG_LEVEL, "전송할 설정된 데이터포인트가 없음: " + register_type + " " + std::to_string(start_address));
            return true;
        }
        
        // 🔥 BaseDeviceWorker::SendValuesToPipelineWithLogging() 호출
        bool success = SendValuesToPipelineWithLogging(timestamped_values,
                                                       "RTU " + register_type + " inputs",
                                                       priority);
        
        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Modbus RTU Bool 데이터 전송 성공: " + std::to_string(timestamped_values.size()) + 
                      "/" + std::to_string(raw_values.size()) + " 포인트");
        }
        
        return success;
                                               
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendModbusRtuBoolDataToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 범용 장비 제어 인터페이스 구현 (메인)
// =============================================================================

bool ModbusRtuWorker::ControlDigitalDevice(const std::string& device_id, bool enable) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("ControlDigitalDevice 호출: " + device_id + " = " + (enable ? "ENABLE" : "DISABLE"));
        
        // WriteDigitalOutput을 통해 실제 제어 수행
        bool success = WriteDigitalOutput(device_id, enable);
        
        if (success) {
            logger.Info("디지털 장비 제어 성공: " + device_id + " " + (enable ? "활성화" : "비활성화"));
        } else {
            logger.Error("디지털 장비 제어 실패: " + device_id);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger.Error("ControlDigitalDevice 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::ControlAnalogDevice(const std::string& device_id, double value) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("ControlAnalogDevice 호출: " + device_id + " = " + std::to_string(value));
        
        // 일반적인 범위 검증 (0-100%)
        if (value < 0.0 || value > 100.0) {
            logger.Warn("ControlAnalogDevice: 값이 일반적 범위를 벗어남: " + std::to_string(value) + 
                       "% (0-100 권장, 하지만 계속 진행)");
        }
        
        // WriteAnalogOutput을 통해 실제 제어 수행
        bool success = WriteAnalogOutput(device_id, value);
        
        if (success) {
            logger.Info("아날로그 장비 제어 성공: " + device_id + " = " + std::to_string(value));
        } else {
            logger.Error("아날로그 장비 제어 실패: " + device_id);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger.Error("ControlAnalogDevice 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 기존 WriteDataPoint, WriteAnalogOutput, WriteDigitalOutput 구현 유지
// (이전에 구현한 코드와 동일)
// =============================================================================

bool ModbusRtuWorker::WriteDataPoint(const std::string& point_id, const DataValue& value) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("WriteDataPoint 호출: " + point_id);
        return WriteDataPointValue(point_id, value);
    } catch (const std::exception& e) {
        logger.Error("WriteDataPoint 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::WriteAnalogOutput(const std::string& output_id, double value) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("WriteAnalogOutput 호출: " + output_id + " = " + std::to_string(value));
        
        // 먼저 DataPoint로 찾아보기
        DataValue data_value = value;
        auto data_point_opt = FindDataPointById(output_id);
        if (data_point_opt.has_value()) {
            return WriteDataPoint(output_id, data_value);
        }
        
        // DataPoint가 없으면 직접 주소 파싱
        uint8_t slave_id;
        uint16_t address;
        if (ParseModbusAddress(output_id, slave_id, address)) {
            uint16_t int_value = static_cast<uint16_t>(value);
            return WriteSingleRegister(slave_id, address, int_value);
        }
        
        logger.Error("WriteAnalogOutput: Invalid output_id: " + output_id);
        return false;
    } catch (const std::exception& e) {
        logger.Error("WriteAnalogOutput 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::WriteDigitalOutput(const std::string& output_id, bool value) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("WriteDigitalOutput 호출: " + output_id + " = " + (value ? "ON" : "OFF"));
        
        // 먼저 DataPoint로 찾아보기
        DataValue data_value = value;
        auto data_point_opt = FindDataPointById(output_id);
        if (data_point_opt.has_value()) {
            return WriteDataPoint(output_id, data_value);
        }
        
        // DataPoint가 없으면 직접 주소 파싱
        uint8_t slave_id;
        uint16_t address;
        if (ParseModbusAddress(output_id, slave_id, address)) {
            return WriteSingleCoil(slave_id, address, value);
        }
        
        logger.Error("WriteDigitalOutput: Invalid output_id: " + output_id);
        return false;
    } catch (const std::exception& e) {
        logger.Error("WriteDigitalOutput 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::WriteSetpoint(const std::string& setpoint_id, double value) {
    auto& logger = LogManager::getInstance();
    
    try {
        logger.Info("WriteSetpoint 호출: " + setpoint_id + " = " + std::to_string(value));
        return WriteAnalogOutput(setpoint_id, value);
    } catch (const std::exception& e) {
        logger.Error("WriteSetpoint 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드들 구현
// =============================================================================

bool ModbusRtuWorker::WriteDataPointValue(const std::string& point_id, const DataValue& value) {
    auto& logger = LogManager::getInstance();
    
    auto data_point_opt = FindDataPointById(point_id);
    if (!data_point_opt.has_value()) {
        logger.Error("DataPoint not found: " + point_id);
        return false;
    }
    
    const auto& data_point = data_point_opt.value();
    
    uint8_t slave_id;
    uint16_t address;
    
    // Modbus 주소 파싱 (DataPoint에서)
    if (!ParseModbusAddressFromDataPoint(data_point, slave_id, address)) {
        logger.Error("Invalid Modbus address for DataPoint: " + point_id);
        return false;
    }
    
    try {
        bool success = false;
        
        // data_type에 따라 레지스터 타입 결정
        if (data_point.data_type == "UINT16" || data_point.data_type == "INT16" || 
            data_point.data_type == "FLOAT32") {
            // Holding Register 쓰기
            int32_t int_value = std::get<int32_t>(value);
            uint16_t modbus_value = static_cast<uint16_t>(int_value);
            success = WriteSingleRegister(slave_id, address, modbus_value);
            LogWriteOperation(slave_id, address, value, "holding_register", success);
        } else if (data_point.data_type == "BOOL") {
            // Coil 쓰기
            bool coil_value = std::get<bool>(value);
            success = WriteSingleCoil(slave_id, address, coil_value);
            LogWriteOperation(slave_id, address, value, "coil", success);
        } else {
            logger.Error("Unsupported data type for writing: " + data_point.data_type);
            return false;
        }
        
        if (success) {
            logger.Info("DataPoint 쓰기 성공: " + point_id);
        }
        
        return success;
    } catch (const std::bad_variant_access& e) {
        logger.Error("DataValue 타입 불일치: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        logger.Error("WriteDataPointValue 예외: " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::ParseModbusAddress(const std::string& address_str, uint8_t& slave_id, uint16_t& address) {
    auto& logger = LogManager::getInstance();
    
    try {
        // "slave_id:address" 또는 "address" 형식 파싱
        size_t colon_pos = address_str.find(':');
        
        if (colon_pos != std::string::npos) {
            // slave_id가 포함된 경우
            slave_id = static_cast<uint8_t>(std::stoi(address_str.substr(0, colon_pos)));
            address = static_cast<uint16_t>(std::stoi(address_str.substr(colon_pos + 1)));
        } else {
            // 기본 slave_id 사용
            slave_id = static_cast<uint8_t>(GetSlaveId());  // 설정에서 기본 슬레이브 ID
            address = static_cast<uint16_t>(std::stoi(address_str));
        }
        
        if (slave_id < 1 || slave_id > 247) {
            logger.Error("Invalid slave_id: " + std::to_string(slave_id));
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        logger.Error("Failed to parse address string '" + address_str + "': " + std::string(e.what()));
        return false;
    }
}

bool ModbusRtuWorker::ParseModbusAddressFromDataPoint(const DataPoint& data_point, 
                                                     uint8_t& slave_id, uint16_t& address) {
    auto& logger = LogManager::getInstance();
    
    try {
        // protocol_params에서 slave_id 확인
        if (data_point.protocol_params.count("slave_id")) {
            slave_id = static_cast<uint8_t>(std::stoi(data_point.protocol_params.at("slave_id")));
        } else {
            slave_id = static_cast<uint8_t>(GetSlaveId());  // 기본값
        }
        
        address = static_cast<uint16_t>(data_point.address);
        
        // Modbus 표준 주소 범위 조정
        if (address >= 40001 && address <= 49999) {
            address -= 40001;  // Holding Register (0-based)
        } else if (address >= 1 && address <= 9999) {
            address -= 1;      // Coil (0-based)
        } else if (address >= 10001 && address <= 19999) {
            address -= 10001;  // Discrete Input (0-based)
        } else if (address >= 30001 && address <= 39999) {
            address -= 30001;  // Input Register (0-based)
        }
        // 그 외는 그대로 사용 (이미 0-based인 경우)
        
        return true;
    } catch (const std::exception& e) {
        logger.Error("Failed to parse DataPoint address: " + std::string(e.what()));
        return false;
    }
}

std::optional<DataPoint> ModbusRtuWorker::FindDataPointById(const std::string& point_id) {
    // BaseDeviceWorker::GetDataPoints() 사용
    const auto& data_points = GetDataPoints();
    
    for (const auto& point : data_points) {
        if (point.id == point_id) {
            return point;
        }
    }
    
    return std::nullopt;  // 찾지 못함
}

void ModbusRtuWorker::LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                                       const std::string& register_type, bool success) {
    auto& logger = LogManager::getInstance();
    
    try {
        // 제어 이력을 파이프라인으로 전송
        TimestampedValue control_log;
        control_log.value = value;
        control_log.timestamp = std::chrono::system_clock::now();
        control_log.quality = success ? DataQuality::GOOD : DataQuality::BAD;
        control_log.source = "control_rtu_" + register_type + "_" + std::to_string(address) + 
                            "_slave" + std::to_string(slave_id);
        
        // 제어 이력은 높은 우선순위로 기록
        SendValuesToPipelineWithLogging({control_log}, "RTU 제어 이력", 1);
        
    } catch (const std::exception& e) {
        logger.Error("LogWriteOperation 예외: " + std::string(e.what()));
    }
}

} // namespace Workers
} // namespace PulseOne