/**
 * @file ModbusTcpWorker.cpp (리팩토링됨)
 * @brief Modbus TCP 워커 구현 - ModbusDriver를 통신 매체로 사용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 (리팩토링됨)
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
    , max_registers_per_group_(125)  // Modbus TCP standard limit
    , auto_group_creation_enabled_(true) {
    
    LogMessage(LogLevel::INFO, "ModbusTcpWorker created for device: " + device_info.name);
    
    // 설정 파싱
    if (!ParseModbusConfig()) {
        LogMessage(LogLevel::ERROR, "Failed to parse Modbus configuration");
        return;
    }
    
    // ModbusDriver 초기화
    if (!InitializeModbusDriver()) {
        LogMessage(LogLevel::ERROR, "Failed to initialize ModbusDriver");
        return;
    }
    
    LogMessage(LogLevel::INFO, "ModbusTcpWorker initialization completed");
}

ModbusTcpWorker::~ModbusTcpWorker() {
    // 폴링 스레드 정리
    polling_thread_running_ = false;
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    // ModbusDriver 정리 (자동으로 연결 해제됨)
    modbus_driver_.reset();
    
    LogMessage(LogLevel::INFO, "ModbusTcpWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> ModbusTcpWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogMessage(LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogMessage(LogLevel::INFO, "Starting ModbusTcpWorker...");
        
        try {
            // 기본 연결 설정 (TcpBasedWorker → ModbusDriver 위임)
            if (!EstablishConnection()) {
                LogMessage(LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            // 데이터 포인트들로부터 폴링 그룹 자동 생성
            const auto& data_points = GetDataPoints();
            if (auto_group_creation_enabled_ && !data_points.empty()) {
                size_t group_count = CreatePollingGroupsFromDataPoints(data_points);
                LogMessage(LogLevel::INFO, "Created " + std::to_string(group_count) + " polling groups from " + 
                          std::to_string(data_points.size()) + " data points");
            }
            
            // 폴링 스레드 시작
            polling_thread_running_ = true;
            polling_thread_ = std::make_unique<std::thread>(&ModbusTcpWorker::PollingThreadFunction, this);
            
            // 상태 변경
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
            // 폴링 스레드 중지
            polling_thread_running_ = false;
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
            
            // 연결 해제 (TcpBasedWorker → ModbusDriver 위임)
            CloseConnection();
            
            // 상태 변경
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
// TcpBasedWorker 인터페이스 구현 (Driver 위임)
// =============================================================================

bool ModbusTcpWorker::EstablishProtocolConnection() {
    if (!modbus_driver_) {
        LogMessage(LogLevel::ERROR, "ModbusDriver not initialized");
        return false;
    }
    
    LogMessage(LogLevel::INFO, "Establishing Modbus protocol connection...");
    
    // ModbusDriver를 통한 연결 수립
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
        return true;  // 이미 없으면 성공으로 간주
    }
    
    LogMessage(LogLevel::INFO, "Closing Modbus protocol connection...");
    
    // ModbusDriver를 통한 연결 해제
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
    test_point.data_type = "UINT16";
    test_points.push_back(test_point);
    
    // ModbusDriver를 통한 Keep-alive 테스트
    bool result = modbus_driver_->ReadValues(test_points, test_values);
    
    if (result) {
        LogMessage(LogLevel::DEBUG_LEVEL, "Modbus Keep-alive successful");
    } else {
        const auto& error = modbus_driver_->GetLastError();
        LogMessage(LogLevel::WARN, "Modbus Keep-alive failed: " + error.message);
    }
    
    return result;
}

// =============================================================================
// Modbus TCP 특화 객체 관리 (Worker 고유 기능)
// =============================================================================

bool ModbusTcpWorker::AddPollingGroup(const ModbusTcpPollingGroup& group) {
    if (!ValidatePollingGroup(group)) {
        LogMessage(LogLevel::ERROR, "Invalid polling group");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(polling_groups_mutex_);
    
    // 그룹 ID 중복 체크
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
    // ✅ 수정: properties 사용
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
// 데이터 포인트 처리 (Worker 고유 로직)
// =============================================================================

size_t ModbusTcpWorker::CreatePollingGroupsFromDataPoints(const std::vector<PulseOne::DataPoint>& data_points) {
    if (data_points.empty()) {
        return 0;
    }
    
    LogMessage(LogLevel::INFO, "Creating polling groups from " + std::to_string(data_points.size()) + " data points");
    
    // 슬레이브 ID별, 레지스터 타입별로 그룹화
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
    
    LogMessage(LogLevel::INFO, "Created " + std::to_string(created_groups) + " polling groups");
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
    LogMessage(LogLevel::INFO, "Optimized polling groups: " + std::to_string(original_count) + 
               " → " + std::to_string(optimized_count));
    
    return optimized_count;
}

// =============================================================================
// 내부 메서드 (Worker 고유 로직)
// =============================================================================

/**
 * @brief ParseModbusConfig() 완전 구현
 * @details 문서 가이드라인에 따른 5단계 파싱 프로세스
 * 
 * 🔥 구현 전략:
 * 1단계: connection_string에서 프로토콜별 설정 JSON 파싱
 * 2단계: Modbus 특화 설정 추출 (프로토콜별)
 * 3단계: DeviceInfo에서 공통 통신 설정 가져오기
 * 4단계: Worker 레벨 설정 적용
 * 5단계: 설정 검증 및 안전한 기본값 적용
 */

bool ModbusTcpWorker::ParseModbusConfig() {
    try {
        LogMessage(LogLevel::INFO, "🔧 Starting Modbus TCP configuration parsing...");
        
        // 기본 DriverConfig 설정
        modbus_config_.device_id = device_info_.id;
        modbus_config_.name = device_info_.name;
        modbus_config_.endpoint = device_info_.endpoint;
        modbus_config_.timeout_ms = device_info_.timeout_ms;
        modbus_config_.retry_count = device_info_.retry_count;
        
        // 🔥 핵심 수정: config와 connection_string 올바른 파싱
        nlohmann::json protocol_config_json;
        
        // 1단계: device_info_.config에서 JSON 설정 찾기 (우선순위 1)
        if (!device_info_.config.empty()) {
            try {
                protocol_config_json = nlohmann::json::parse(device_info_.config);
                LogMessage(LogLevel::INFO, "✅ Protocol config loaded from device.config: " + device_info_.config);
            } catch (const std::exception& e) {
                LogMessage(LogLevel::WARN, "⚠️ Failed to parse device.config JSON: " + std::string(e.what()));
            }
        }
        
        // 2단계: connection_string이 JSON인지 확인 (우선순위 2)
        if (protocol_config_json.empty() && !device_info_.connection_string.empty()) {
            // JSON 형태인지 확인 ('{' 로 시작하는지)
            if (device_info_.connection_string.front() == '{') {
                try {
                    protocol_config_json = nlohmann::json::parse(device_info_.connection_string);
                    LogMessage(LogLevel::INFO, "✅ Protocol config loaded from connection_string JSON");
                } catch (const std::exception& e) {
                    LogMessage(LogLevel::WARN, "⚠️ Failed to parse connection_string JSON: " + std::string(e.what()));
                }
            } else {
                LogMessage(LogLevel::INFO, "📝 connection_string is not JSON format, using endpoint as broker URL");
            }
        }
        
        // 3단계: 기본값 설정 (DB에서 설정을 못 가져온 경우만)
        if (protocol_config_json.empty()) {
            protocol_config_json = {
                {"slave_id", 1},
                {"byte_order", "big_endian"},
                {"max_registers_per_group", 125},
                {"auto_group_creation", true}
            };
            LogMessage(LogLevel::INFO, "📝 Applied default Modbus protocol configuration");
        }
        
        // 4단계: 실제 DB 설정값들을 properties에 저장
        modbus_config_.properties["slave_id"] = std::to_string(protocol_config_json.value("slave_id", 1));
        modbus_config_.properties["byte_order"] = protocol_config_json.value("byte_order", "big_endian");
        modbus_config_.properties["max_registers_per_group"] = std::to_string(protocol_config_json.value("max_registers_per_group", 125));
        modbus_config_.properties["auto_group_creation"] = protocol_config_json.value("auto_group_creation", true) ? "true" : "false";
        
        // 🔥 DB에서 가져온 timeout 값 적용
        if (protocol_config_json.contains("timeout")) {
            int db_timeout = protocol_config_json.value("timeout", device_info_.timeout_ms);
            modbus_config_.timeout_ms = db_timeout;  // 실제 사용할 타임아웃 업데이트
            modbus_config_.properties["response_timeout_ms"] = std::to_string(db_timeout);
            LogMessage(LogLevel::INFO, "✅ Applied timeout from DB: " + std::to_string(db_timeout) + "ms");
        } else {
            modbus_config_.properties["response_timeout_ms"] = std::to_string(device_info_.timeout_ms);
        }
        
        // 5단계: 통신 설정 완성 - 🔥 타입 캐스팅 수정
        modbus_config_.properties["byte_timeout_ms"] = std::to_string(std::min(static_cast<int>(modbus_config_.timeout_ms / 10), 1000));
        modbus_config_.properties["max_retries"] = std::to_string(device_info_.retry_count);
        
        // 6단계: Worker 전용 설정
        modbus_config_.properties["polling_interval_ms"] = std::to_string(device_info_.polling_interval_ms);
        modbus_config_.properties["keep_alive"] = device_info_.is_enabled ? "enabled" : "disabled";
        
        // 🎉 성공 로그 - 실제 적용된 설정 표시 - 🔥 문자열 연결 수정
        std::string config_summary = "✅ Modbus config parsed successfully:\n";
        config_summary += "   🔌 Protocol settings (from ";
        config_summary += (!device_info_.config.empty() ? "device.config" : "connection_string");
        config_summary += "):\n";
        config_summary += "      - slave_id: " + modbus_config_.properties["slave_id"] + "\n";
        config_summary += "      - byte_order: " + modbus_config_.properties["byte_order"] + "\n";
        config_summary += "      - max_registers_per_group: " + modbus_config_.properties["max_registers_per_group"] + "\n";
        config_summary += "      - auto_group_creation: " + modbus_config_.properties["auto_group_creation"] + "\n";
        config_summary += "   ⚙️  Communication settings (from DeviceSettings):\n";
        config_summary += "      - connection_timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        config_summary += "      - read_timeout: " + modbus_config_.properties["response_timeout_ms"] + "ms\n";
        config_summary += "      - byte_timeout: " + modbus_config_.properties["byte_timeout_ms"] + "ms\n";
        config_summary += "      - max_retries: " + modbus_config_.properties["max_retries"] + "\n";
        config_summary += "      - polling_interval: " + modbus_config_.properties["polling_interval_ms"] + "ms\n";
        config_summary += "      - keep_alive: " + modbus_config_.properties["keep_alive"];
        
        LogMessage(LogLevel::INFO, config_summary);
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "ParseModbusConfig failed: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief InitializeModbusDriver() 완전 구현
 * @details 파싱된 ModbusConfig를 DriverConfig로 변환하여 ModbusDriver 초기화
 */

bool ModbusTcpWorker::InitializeModbusDriver() {
    try {
        LogMessage(LogLevel::INFO, "🔧 Initializing ModbusDriver...");
        
        // ModbusDriver 인스턴스 생성
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            LogMessage(LogLevel::ERROR, "❌ Failed to create ModbusDriver instance");
            return false;
        }
        
        // 🔥 수정 1: device_info_.driver_config 직접 사용 (WorkerFactory에서 완전 매핑된 것)
        PulseOne::Structs::DriverConfig driver_config = device_info_.driver_config;
        
        // 🔥 수정 2: Modbus 특화 설정만 추가/업데이트
        // (기존 properties는 그대로 유지)
        
        // =======================================================================
        // 기본 필드 업데이트 (Modbus 파싱 결과 반영)
        // =======================================================================
        driver_config.device_id = device_info_.name;
        driver_config.endpoint = device_info_.endpoint;
        driver_config.protocol = PulseOne::Enums::ProtocolType::MODBUS_TCP;
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        driver_config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
        driver_config.polling_interval_ms = static_cast<uint32_t>(device_info_.polling_interval_ms);
        if (device_info_.properties.count("auto_reconnect")) {
            driver_config.auto_reconnect = (device_info_.properties.at("auto_reconnect") == "true");
        } else {
            driver_config.auto_reconnect = true; // 기본값
        }
        
        // =======================================================================
        // 🔥 핵심: 기존 properties 보존하면서 Modbus 설정 추가
        // =======================================================================
        
        // Modbus 프로토콜 특화 설정들 (기존 properties에 추가)
        driver_config.properties["slave_id"] = GetPropertyValue(modbus_config_.properties, "slave_id", "1");
        driver_config.properties["byte_order"] = GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian");
        driver_config.properties["max_retries"] = GetPropertyValue(modbus_config_.properties, "max_retries", "3");
        driver_config.properties["response_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000");
        driver_config.properties["byte_timeout_ms"] = GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100");
        driver_config.properties["max_registers_per_group"] = GetPropertyValue(modbus_config_.properties, "max_registers_per_group", "125");
        
        // 🔥 로깅: 최종 properties 상태 확인
        LogMessage(LogLevel::INFO, "📊 Final DriverConfig properties count: " + std::to_string(driver_config.properties.size()));
        
        // 디버그용: 모든 properties 출력
        std::string properties_log = "📋 All DriverConfig properties:\n";
        for (const auto& [key, value] : driver_config.properties) {
            properties_log += "   [" + key + "] = " + value + "\n";
        }
        LogMessage(LogLevel::DEBUG_LEVEL, properties_log);
        
        // =======================================================================
        // ModbusDriver 초기화
        // =======================================================================
        
        std::string config_msg = "📋 DriverConfig prepared:\n";
        config_msg += "   - device_id: " + driver_config.device_id + "\n";
        config_msg += "   - endpoint: " + driver_config.endpoint + "\n";
        config_msg += "   - protocol: MODBUS_TCP\n";
        config_msg += "   - timeout: " + std::to_string(driver_config.timeout_ms) + "ms\n";
        config_msg += "   - properties: " + std::to_string(driver_config.properties.size()) + " items";
        
        LogMessage(LogLevel::DEBUG_LEVEL, config_msg);
        
        // ModbusDriver 초기화 수행
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(LogLevel::ERROR, "❌ ModbusDriver initialization failed: " + error.message + " (code: " + std::to_string(static_cast<int>(error.code)) + ")");
            return false;
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "✅ ModbusDriver initialization successful");
        
        // Driver 콜백 설정
        SetupDriverCallbacks();
        
        // 최종 결과 로깅
        std::string final_msg = "✅ ModbusDriver initialized successfully:\n";
        final_msg += "   📡 Connection details:\n";
        final_msg += "      - endpoint: " + device_info_.endpoint + "\n";
        final_msg += "      - slave_id: " + GetPropertyValue(modbus_config_.properties, "slave_id", "1") + "\n";
        final_msg += "      - timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        final_msg += "   ⚙️  Advanced settings:\n";
        final_msg += "      - byte_order: " + GetPropertyValue(modbus_config_.properties, "byte_order", "big_endian") + "\n";
        final_msg += "      - max_retries: " + GetPropertyValue(modbus_config_.properties, "max_retries", "3") + "\n";
        final_msg += "      - response_timeout: " + GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000") + "ms\n";
        final_msg += "      - max_registers_per_group: " + GetPropertyValue(modbus_config_.properties, "max_registers_per_group", "125") + "\n";
        final_msg += "   📊 Total properties: " + std::to_string(driver_config.properties.size());
        
        LogMessage(LogLevel::INFO, final_msg);
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "❌ Exception during ModbusDriver initialization: " + std::string(e.what()));
        
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
        // ModbusDriver를 통한 값 읽기
        std::vector<TimestampedValue> values;
        bool success = modbus_driver_->ReadValues(group.data_points, values);
        
        if (success && !values.empty()) {
            // 🔥 한줄로 파이프라인 전송!
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

/**
 * @brief Driver 콜백 설정 (선택적 구현)
 * @details ModbusDriver의 이벤트들을 Worker에서 처리하기 위한 콜백 등록
 */
void ModbusTcpWorker::SetupDriverCallbacks() {
    if (!modbus_driver_) {
        return;
    }
    
    try {
        LogMessage(LogLevel::DEBUG_LEVEL, "🔗 Setting up ModbusDriver callbacks...");
        
        // 예시: 연결 상태 변경 콜백
        // modbus_driver_->SetConnectionStatusCallback([this](bool connected) {
        //     if (connected) {
        //         LogMessage(LogLevel::INFO, "📡 Modbus connection established");
        //         OnProtocolConnected();
        //     } else {
        //         LogMessage(LogLevel::WARN, "📡 Modbus connection lost");
        //         OnProtocolDisconnected();
        //     }
        // });
        
        // 예시: 에러 발생 콜백
        // modbus_driver_->SetErrorCallback([this](const ErrorInfo& error) {
        //     LogMessage(LogLevel::ERROR, 
        //               "🚨 Modbus error: " + error.message + 
        //               " (code: " + std::to_string(static_cast<int>(error.code)) + ")");
        //     OnProtocolError(error);
        // });
        
        // 예시: 데이터 수신 콜백  
        // modbus_driver_->SetDataReceivedCallback([this](const std::vector<TimestampedValue>& values) {
        //     LogMessage(LogLevel::DEBUG_LEVEL, 
        //               "📊 Received " + std::to_string(values.size()) + " Modbus values");
        //     OnDataReceived(values);
        // });
        
        LogMessage(LogLevel::DEBUG_LEVEL, "✅ ModbusDriver callbacks configured");
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::WARN, 
                   "⚠️ Failed to setup driver callbacks: " + std::string(e.what()));
        // 콜백 설정 실패는 치명적이지 않으므로 계속 진행
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

// =============================================================================
// 🔥 1. uint16_t 값들 (Holding/Input Register) 파이프라인 전송
// =============================================================================

bool ModbusTcpWorker::SendModbusDataToPipeline(const std::vector<uint16_t>& raw_values, 
                                               uint16_t start_address,
                                               const std::string& register_type,
                                               uint32_t priority) {
    if (raw_values.empty()) {
        return false;
    }
    
    try {
        std::vector<TimestampedValue> timestamped_values;
        timestamped_values.reserve(raw_values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        
        for (size_t i = 0; i < raw_values.size(); ++i) {
            TimestampedValue tv;
            tv.value = static_cast<int32_t>(raw_values[i]);  // DataValue는 variant
            tv.timestamp = timestamp;
            tv.quality = DataQuality::GOOD;
            tv.source = "modbus_" + register_type + "_" + std::to_string(start_address + i);
            timestamped_values.push_back(tv);
        }
        
        // 공통 전송 함수 호출
        return SendValuesToPipelineWithLogging(timestamped_values, 
                                               register_type + " registers", 
                                               priority);
                                               
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendModbusDataToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 2. uint8_t 값들 (Coil/Discrete Input) 파이프라인 전송
// =============================================================================

bool ModbusTcpWorker::SendModbusBoolDataToPipeline(const std::vector<uint8_t>& raw_values,
                                                   uint16_t start_address,
                                                   const std::string& register_type,
                                                   uint32_t priority) {
    if (raw_values.empty()) {
        return false;
    }
    
    try {
        std::vector<TimestampedValue> timestamped_values;
        timestamped_values.reserve(raw_values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        
        for (size_t i = 0; i < raw_values.size(); ++i) {
            TimestampedValue tv;
            tv.value = static_cast<bool>(raw_values[i]); // DataValue는 bool 지원
            tv.timestamp = timestamp;
            tv.quality = DataQuality::GOOD;
            tv.source = "modbus_" + register_type + "_" + std::to_string(start_address + i);
            timestamped_values.push_back(tv);
        }
        
        // 공통 전송 함수 호출
        return SendValuesToPipelineWithLogging(timestamped_values,
                                               register_type + " inputs",
                                               priority);
                                               
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendModbusBoolDataToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 3. 최종 공통 전송 함수 (로깅 포함)
// =============================================================================

bool ModbusTcpWorker::SendValuesToPipelineWithLogging(const std::vector<TimestampedValue>& values,
                                                      const std::string& context,
                                                      uint32_t priority) {
    if (values.empty()) {
        return false;
    }
    
    try {
        // BaseDeviceWorker::SendDataToPipeline() 호출
        bool success = SendDataToPipeline(values, priority);
        
        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "파이프라인 전송 성공 (" + context + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        } else {
            LogMessage(LogLevel::WARN, 
                      "파이프라인 전송 실패 (" + context + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendValuesToPipelineWithLogging 예외 (" + context + "): " + 
                  std::string(e.what()));
        return false;
    }
}


// 운영용 쓰기 함수 구현
bool ModbusTcpWorker::WriteSingleHoldingRegister(int slave_id, uint16_t address, uint16_t value) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // 🔥 ModbusDriver에 올바른 함수명으로 위임
        bool success = modbus_driver_->WriteHoldingRegister(slave_id, address, value);
        
        // 제어 이력 기록
        DataValue data_value = static_cast<int32_t>(value);  // 🔥 올바른 변환
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
        // 🔥 ModbusDriver에 올바른 함수명으로 위임 (수정 필요시)
        bool success = modbus_driver_->WriteCoil(slave_id, address, value);  // 🔥 함수명 확인 필요
        
        // 제어 이력 기록
        DataValue data_value = value;  // 🔥 bool은 직접 할당 가능
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

// 디버깅용 읽기 함수 구현
bool ModbusTcpWorker::ReadSingleHoldingRegister(int slave_id, uint16_t address, uint16_t& value) {
    std::vector<uint16_t> values;
    bool success = ReadHoldingRegisters(slave_id, address, 1, values);
    
    if (success && !values.empty()) {
        value = values[0];
        
        // 디버깅 읽기도 파이프라인 전송 (낮은 우선순위)
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

bool ModbusTcpWorker::ReadHoldingRegisters(int slave_id, uint16_t start_address, uint16_t count, 
                                          std::vector<uint16_t>& values) {
    if (!modbus_driver_ || !modbus_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "ModbusDriver not connected");
        return false;
    }
    
    try {
        // ModbusDriver에 위임
        bool success = modbus_driver_->ReadHoldingRegisters(slave_id, start_address, count, values);
        
        if (success && !values.empty()) {
            // 디버깅 읽기 결과도 파이프라인 전송
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

// 고수준 DataPoint 기반 함수
bool ModbusTcpWorker::WriteDataPointValue(const std::string& point_id, const DataValue& value) {
    auto data_point_opt = FindDataPointById(point_id);
    if (!data_point_opt.has_value()) {
        LogMessage(LogLevel::ERROR, "DataPoint not found: " + point_id);
        return false;
    }
    
    const auto& data_point = data_point_opt.value();
    
    // DataPoint 타입에 따라 적절한 쓰기 함수 호출
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
            // 🔥 std::get을 올바르게 사용 (variant에서 값 추출)
            int32_t int_value = std::get<int32_t>(value);  // DataValue는 variant 타입
            uint16_t modbus_value = static_cast<uint16_t>(int_value);
            success = WriteSingleHoldingRegister(slave_id, address, modbus_value);
            
        } else if (register_type == ModbusRegisterType::COIL) {
            // 🔥 std::get을 올바르게 사용
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

// 제어 이력 기록 헬퍼
void ModbusTcpWorker::LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                                       const std::string& register_type, bool success) {
    try {
        TimestampedValue control_log;
        control_log.value = value;
        control_log.timestamp = std::chrono::system_clock::now();
        control_log.quality = success ? DataQuality::GOOD : DataQuality::BAD;
        control_log.source = "control_" + register_type + "_" + std::to_string(address) + 
                            "_slave" + std::to_string(slave_id);
        
        // 제어 이력은 높은 우선순위로 기록
        SendValuesToPipelineWithLogging({control_log}, "제어 이력", 1);
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "LogWriteOperation 예외: " + std::string(e.what()));
    }
}

std::optional<DataPoint> ModbusTcpWorker::FindDataPointById(const std::string& point_id) {
    // GetDataPoints()는 BaseDeviceWorker에서 제공되는 함수
    const auto& data_points = GetDataPoints();
    
    for (const auto& point : data_points) {
        if (point.id == point_id) {
            return point;
        }
    }
    
    return std::nullopt;  // 찾지 못함
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