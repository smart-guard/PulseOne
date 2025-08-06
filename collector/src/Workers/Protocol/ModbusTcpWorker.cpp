/**
 * @file ModbusTcpWorker.cpp (리팩토링됨)
 * @brief Modbus TCP 워커 구현 - ModbusDriver를 통신 매체로 사용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 (리팩토링됨)
 */

#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
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
    if (!modbus_driver_) {
        return "{\"error\":\"driver_not_initialized\"}";
    }
    
    // ✅ 수정: GetDiagnostics() → GetDiagnosticsJSON() 사용
    std::string json_diagnostics = modbus_driver_->GetDiagnosticsJSON();
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"worker_type\": \"ModbusTcpWorker\",\n";
    oss << "  \"device_id\": \"" << device_info_.id << "\",\n";          // device_id → id 수정
    oss << "  \"device_name\": \"" << device_info_.name << "\",\n";
    oss << "  \"endpoint\": \"" << device_info_.endpoint << "\",\n";
    
    // ✅ 표준화된 통계 정보 (DriverStatistics 사용)
    const auto& stats = modbus_driver_->GetStatistics();
    oss << "  \"statistics\": {\n";
    oss << "    \"total_reads\": " << stats.total_reads.load() << ",\n";
    oss << "    \"successful_reads\": " << stats.successful_reads.load() << ",\n";
    oss << "    \"failed_reads\": " << stats.failed_reads.load() << ",\n";
    oss << "    \"total_writes\": " << stats.total_writes.load() << ",\n";
    oss << "    \"successful_writes\": " << stats.successful_writes.load() << ",\n";
    oss << "    \"failed_writes\": " << stats.failed_writes.load() << ",\n";
    oss << "    \"success_rate\": " << stats.GetSuccessRate() << ",\n";
    
    // ✅ Modbus 특화 통계 (프로토콜 카운터)
    oss << "    \"register_reads\": " << stats.GetProtocolCounter("register_reads") << ",\n";
    oss << "    \"register_writes\": " << stats.GetProtocolCounter("register_writes") << ",\n";
    oss << "    \"crc_checks\": " << stats.GetProtocolCounter("crc_checks") << ",\n";
    oss << "    \"slave_errors\": " << stats.GetProtocolCounter("slave_errors") << ",\n";
    oss << "    \"avg_response_time_ms\": " << stats.GetProtocolMetric("avg_response_time_ms") << "\n";
    oss << "  },\n";
    
    // ✅ 드라이버 진단 정보 (JSON 문자열 직접 삽입)
    oss << "  \"driver_diagnostics\": " << json_diagnostics << ",\n";
    
    // TCP Worker 특화 정보
    oss << "  \"worker_info\": {\n";
    oss << "    \"default_polling_interval_ms\": " << default_polling_interval_ms_ << ",\n";
    oss << "    \"modbus_config\": {\n";
    oss << "      \"slave_id\": " << modbus_config_.slave_id << ",\n";
    oss << "      \"timeout_ms\": " << modbus_config_.timeout_ms << ",\n";
    oss << "      \"response_timeout_ms\": " << modbus_config_.response_timeout_ms << ",\n";
    oss << "      \"byte_timeout_ms\": " << modbus_config_.byte_timeout_ms << ",\n";
    oss << "      \"max_retries\": " << static_cast<int>(modbus_config_.max_retries) << "\n";
    oss << "    }\n";
    oss << "  }\n";
    
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
        LogMessage(LogLevel::INFO, "🔧 Starting Modbus configuration parsing...");
        
        // =====================================================================
        // 🔥 1단계: connection_string에서 프로토콜별 설정 JSON 파싱
        // =====================================================================
        
        nlohmann::json protocol_config_json;
        std::string config_source = device_info_.connection_string;
        
        LogMessage(LogLevel::DEBUG_LEVEL, 
                   "📋 Raw connection_string: '" + config_source + "'");
        
        // connection_string이 JSON 형태인지 확인
        if (!config_source.empty() && 
            (config_source.front() == '{' || config_source.find("slave_id") != std::string::npos)) {
            try {
                protocol_config_json = nlohmann::json::parse(config_source);
                LogMessage(LogLevel::INFO, 
                          "✅ Parsed protocol config from connection_string: " + config_source);
            } catch (const std::exception& e) {
                LogMessage(LogLevel::WARN, 
                          "⚠️ Failed to parse protocol config JSON, using defaults: " + std::string(e.what()));
                protocol_config_json = nlohmann::json::object();
            }
        } else {
            LogMessage(LogLevel::INFO, 
                      "ℹ️ No protocol JSON config found in connection_string, using defaults");
            protocol_config_json = nlohmann::json::object();
        }
        
        // 프로토콜 기본값 설정 (JSON이 비어있을 때)
        if (protocol_config_json.empty()) {
            protocol_config_json = {
                {"slave_id", 1},
                {"byte_order", "big_endian"},
                {"max_registers_per_group", 125},
                {"auto_group_creation", true}
            };
            LogMessage(LogLevel::INFO, 
                      "📝 Applied default Modbus protocol configuration");
        }
        
        // =====================================================================
        // 🔥 2단계: Modbus 특화 설정 추출 (프로토콜별)
        // =====================================================================
        
        modbus_config_.slave_id = protocol_config_json.value("slave_id", 1);
        modbus_config_.byte_order = protocol_config_json.value("byte_order", "big_endian");
        modbus_config_.max_registers_per_group = protocol_config_json.value("max_registers_per_group", 125);
        modbus_config_.auto_group_creation = protocol_config_json.value("auto_group_creation", true);
        
        // 🔥 문자열 연결 문제 해결: std::string으로 명시적 변환
        std::string debug_msg = "🔌 Extracted protocol-specific config:\n";
        debug_msg += "   - slave_id: " + std::to_string(modbus_config_.slave_id) + "\n";
        debug_msg += "   - byte_order: " + modbus_config_.byte_order + "\n";
        debug_msg += "   - max_registers_per_group: " + std::to_string(modbus_config_.max_registers_per_group);
        
        LogMessage(LogLevel::DEBUG_LEVEL, debug_msg);
        
        // =====================================================================
        // 🔥 3단계: DeviceInfo에서 공통 통신 설정 가져오기 (이미 매핑됨!)
        // =====================================================================
        
        if (device_info_.connection_timeout_ms.has_value()) {
            modbus_config_.timeout_ms = static_cast<uint32_t>(device_info_.connection_timeout_ms.value());
        } else {
            modbus_config_.timeout_ms = static_cast<uint32_t>(device_info_.timeout_ms);  // 기본값 사용
        }

        if (device_info_.read_timeout_ms.has_value()) {
            modbus_config_.response_timeout_ms = static_cast<uint32_t>(device_info_.read_timeout_ms.value());
            modbus_config_.byte_timeout_ms = static_cast<uint32_t>(
                std::min(device_info_.read_timeout_ms.value() / 10, 1000)
            );
        } else {
            modbus_config_.response_timeout_ms = static_cast<uint32_t>(device_info_.timeout_ms);
            modbus_config_.byte_timeout_ms = static_cast<uint32_t>(
                std::min(device_info_.timeout_ms / 10, 1000)
            );
        }
        modbus_config_.max_retries = static_cast<uint8_t>(device_info_.retry_count);
        
        std::string comm_msg = "⚙️ Mapped communication settings from DeviceInfo:\n";
        comm_msg += "   - connection_timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        comm_msg += "   - read_timeout: " + std::to_string(modbus_config_.response_timeout_ms) + "ms\n";
        comm_msg += "   - byte_timeout: " + std::to_string(modbus_config_.byte_timeout_ms) + "ms\n";
        comm_msg += "   - max_retries: " + std::to_string(modbus_config_.max_retries);
        
        LogMessage(LogLevel::DEBUG_LEVEL, comm_msg);
        
        // =====================================================================
        // 🔥 4단계: Worker 레벨 설정 적용 (DeviceInfo에서 직접)
        // =====================================================================
        
        default_polling_interval_ms_ = device_info_.polling_interval_ms;
        
        // scan_rate_override가 있으면 우선 적용
        if (device_info_.scan_rate_override.has_value()) {
            default_polling_interval_ms_ = static_cast<uint32_t>(device_info_.scan_rate_override.value());
            LogMessage(LogLevel::INFO, 
                      "📊 Using scan_rate_override: " + std::to_string(default_polling_interval_ms_) + "ms");
        }
        
        // 프로토콜 config에서 오버라이드가 있으면 최종 적용
        if (protocol_config_json.contains("polling_interval_ms")) {
            default_polling_interval_ms_ = protocol_config_json["polling_interval_ms"].get<uint32_t>();
            LogMessage(LogLevel::INFO, 
                      "📊 Protocol config override polling_interval: " + std::to_string(default_polling_interval_ms_) + "ms");
        }
        
        max_registers_per_group_ = modbus_config_.max_registers_per_group;
        auto_group_creation_enabled_ = modbus_config_.auto_group_creation;
        
        // =====================================================================
        // 🔥 5단계: 설정 검증 및 안전한 기본값 적용
        // =====================================================================
        
        bool validation_errors = false;
        
        // Modbus 프로토콜별 검증
        if (modbus_config_.slave_id < 1 || modbus_config_.slave_id > 247) {
            LogMessage(LogLevel::ERROR, 
                      "❌ Invalid slave_id: " + std::to_string(modbus_config_.slave_id) + 
                      " (valid range: 1-247)");
            modbus_config_.slave_id = 1;
            validation_errors = true;
        }
        
        if (modbus_config_.timeout_ms < 100 || modbus_config_.timeout_ms > 30000) {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid timeout: " + std::to_string(modbus_config_.timeout_ms) + 
                      "ms (valid range: 100-30000ms), using 3000ms");
            modbus_config_.timeout_ms = 3000;
            validation_errors = true;
        }
        
        if (modbus_config_.response_timeout_ms < 100 || modbus_config_.response_timeout_ms > 10000) {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid response_timeout: " + std::to_string(modbus_config_.response_timeout_ms) + 
                      "ms (valid range: 100-10000ms), using 1000ms");
            modbus_config_.response_timeout_ms = 1000;
            validation_errors = true;
        }
        
        if (modbus_config_.byte_order != "big_endian" && modbus_config_.byte_order != "little_endian") {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid byte_order: " + modbus_config_.byte_order + 
                      " (valid: big_endian, little_endian), using big_endian");
            modbus_config_.byte_order = "big_endian";
            validation_errors = true;
        }
        
        if (max_registers_per_group_ < 1 || max_registers_per_group_ > 125) {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid max_registers_per_group: " + std::to_string(max_registers_per_group_) + 
                      " (valid range: 1-125), using 125");
            max_registers_per_group_ = 125;
            validation_errors = true;
        }
        
        if (default_polling_interval_ms_ < 100 || default_polling_interval_ms_ > 60000) {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid polling_interval: " + std::to_string(default_polling_interval_ms_) + 
                      "ms (valid range: 100-60000ms), using 1000ms");
            default_polling_interval_ms_ = 1000;
            validation_errors = true;
        }
        
        if (modbus_config_.max_retries > 10) {
            LogMessage(LogLevel::WARN, 
                      "⚠️ Invalid max_retries: " + std::to_string(modbus_config_.max_retries) + 
                      " (valid range: 0-10), using 3");
            modbus_config_.max_retries = 3;
            validation_errors = true;
        }
        
        // =====================================================================
        // 최종 결과 로깅 - 문자열 연결 문제 해결
        // =====================================================================
        
        std::string result_msg = "✅ Modbus config parsed successfully";
        if (validation_errors) {
            result_msg += " (with corrections)";
        }
        result_msg += ":\n";
        result_msg += "   🔌 Protocol settings (from connection_string):\n";
        result_msg += "      - slave_id: " + std::to_string(modbus_config_.slave_id) + "\n";
        result_msg += "      - byte_order: " + modbus_config_.byte_order + "\n";
        result_msg += "      - max_registers_per_group: " + std::to_string(max_registers_per_group_) + "\n";
        result_msg += "      - auto_group_creation: " + (auto_group_creation_enabled_ ? std::string("enabled") : std::string("disabled")) + "\n";
        result_msg += "   ⚙️  Communication settings (from DeviceSettings):\n";
        result_msg += "      - connection_timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        result_msg += "      - read_timeout: " + std::to_string(modbus_config_.response_timeout_ms) + "ms\n";
        result_msg += "      - byte_timeout: " + std::to_string(modbus_config_.byte_timeout_ms) + "ms\n";
        result_msg += "      - max_retries: " + std::to_string(modbus_config_.max_retries) + "\n";
        result_msg += "      - polling_interval: " + std::to_string(default_polling_interval_ms_) + "ms\n";
        result_msg += "      - keep_alive: " + (device_info_.keep_alive_enabled ? std::string("enabled") : std::string("disabled"));
        
        LogMessage(LogLevel::INFO, result_msg);
        
        // ModbusConfig 자체 검증 수행
        if (!modbus_config_.IsValid()) {
            LogMessage(LogLevel::ERROR, 
                      "❌ ModbusConfig validation failed even after corrections!");
            modbus_config_.ResetToDefaults();
            LogMessage(LogLevel::WARN, 
                      "🔄 Reset to safe defaults: " + modbus_config_.ToString());
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                   "❌ Exception in ParseModbusConfig: " + std::string(e.what()));
        
        // 예외 시 안전한 기본값 설정
        modbus_config_.ResetToDefaults();
        default_polling_interval_ms_ = 1000;
        max_registers_per_group_ = 125;
        auto_group_creation_enabled_ = true;
        
        LogMessage(LogLevel::WARN, 
                   "🔄 Applied emergency defaults after exception: " + modbus_config_.ToString());
        
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
        
        // =====================================================================
        // 🔥 1단계: ModbusDriver 인스턴스 생성
        // =====================================================================
        
        modbus_driver_ = std::make_unique<PulseOne::Drivers::ModbusDriver>();
        
        if (!modbus_driver_) {
            LogMessage(LogLevel::ERROR, "❌ Failed to create ModbusDriver instance");
            return false;
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "✅ ModbusDriver instance created");
        
        // =====================================================================
        // 🔥 2단계: 파싱된 설정을 DriverConfig로 변환
        // =====================================================================
        
        PulseOne::Structs::DriverConfig driver_config;
        
        // 기본 디바이스 정보
        driver_config.device_id = device_info_.name;  // device_info_.id는 UUID, name이 더 적합
        driver_config.endpoint = device_info_.endpoint;
        driver_config.protocol = PulseOne::Enums::ProtocolType::MODBUS_TCP;
        
        // 타이밍 설정 (파싱된 ModbusConfig 사용)
        driver_config.timeout_ms = modbus_config_.timeout_ms;
        
        std::string config_msg = "📋 DriverConfig prepared:\n";
        config_msg += "   - device_id: " + driver_config.device_id + "\n";
        config_msg += "   - endpoint: " + driver_config.endpoint + "\n";
        config_msg += "   - protocol: MODBUS_TCP\n";
        config_msg += "   - timeout: " + std::to_string(driver_config.timeout_ms) + "ms";
        
        LogMessage(LogLevel::DEBUG_LEVEL, config_msg);
        
        // =====================================================================
        // 🔥 3단계: 프로토콜별 설정을 custom_settings로 전달 (필드명 수정)
        // =====================================================================
        
        driver_config.custom_settings["slave_id"] = std::to_string(modbus_config_.slave_id);
        driver_config.custom_settings["byte_order"] = modbus_config_.byte_order;
        driver_config.custom_settings["max_retries"] = std::to_string(modbus_config_.max_retries);
        driver_config.custom_settings["response_timeout_ms"] = std::to_string(modbus_config_.response_timeout_ms);
        driver_config.custom_settings["byte_timeout_ms"] = std::to_string(modbus_config_.byte_timeout_ms);
        driver_config.custom_settings["max_registers_per_group"] = std::to_string(modbus_config_.max_registers_per_group);
        
        std::string protocol_msg = "🔧 Protocol settings configured:\n";
        protocol_msg += "   - slave_id: " + std::to_string(modbus_config_.slave_id) + "\n";
        protocol_msg += "   - byte_order: " + modbus_config_.byte_order + "\n";
        protocol_msg += "   - max_retries: " + std::to_string(modbus_config_.max_retries) + "\n";
        protocol_msg += "   - response_timeout: " + std::to_string(modbus_config_.response_timeout_ms) + "ms";
        
        LogMessage(LogLevel::DEBUG_LEVEL, protocol_msg);
        
        // =====================================================================
        // 🔥 4단계: ModbusDriver 초기화 수행
        // =====================================================================
        
        if (!modbus_driver_->Initialize(driver_config)) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(LogLevel::ERROR, 
                      "❌ ModbusDriver initialization failed: " + error.message + 
                      " (code: " + std::to_string(static_cast<int>(error.code)) + ")");
            return false;
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "✅ ModbusDriver initialization successful");
        
        // =====================================================================
        // 🔥 5단계: Driver 콜백 설정 (선택적)
        // =====================================================================
        
        // 필요시 ModbusDriver에 콜백 함수들 등록
        // 예: 연결 상태 변경, 에러 발생 시 처리 등
        SetupDriverCallbacks();
        
        // =====================================================================
        // 최종 결과 로깅 - 문자열 연결 문제 해결
        // =====================================================================
        
        std::string final_msg = "✅ ModbusDriver initialized successfully:\n";
        final_msg += "   📡 Connection details:\n";
        final_msg += "      - endpoint: " + device_info_.endpoint + "\n";
        final_msg += "      - slave_id: " + std::to_string(modbus_config_.slave_id) + "\n";
        final_msg += "      - timeout: " + std::to_string(modbus_config_.timeout_ms) + "ms\n";
        final_msg += "   ⚙️  Advanced settings:\n";
        final_msg += "      - byte_order: " + modbus_config_.byte_order + "\n";
        final_msg += "      - max_retries: " + std::to_string(modbus_config_.max_retries) + "\n";
        final_msg += "      - response_timeout: " + std::to_string(modbus_config_.response_timeout_ms) + "ms\n";
        final_msg += "      - max_registers_per_group: " + std::to_string(modbus_config_.max_registers_per_group);
        
        LogMessage(LogLevel::INFO, final_msg);
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                   "❌ Exception during ModbusDriver initialization: " + std::string(e.what()));
        
        // 예외 발생 시 driver 정리
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
        
        if (!success) {
            const auto& error = modbus_driver_->GetLastError();
            LogMessage(LogLevel::WARN, "Failed to read group " + std::to_string(group.group_id) + ": " + error.message);
            return false;
        }
        
        // 데이터베이스에 저장
        for (size_t i = 0; i < group.data_points.size() && i < values.size(); ++i) {
            SaveDataPointValue(group.data_points[i], values[i]);
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "Successfully processed group " + std::to_string(group.group_id) + 
                   ", read " + std::to_string(values.size()) + " values");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception processing group " + std::to_string(group.group_id) + ": " + std::string(e.what()));
        return false;
    }
}

bool ModbusTcpWorker::SaveDataPointValue(const PulseOne::DataPoint& data_point,
                                         const PulseOne::TimestampedValue& value) {
    try {
        // BaseDeviceWorker의 기본 저장 메서드 사용
        // SaveToInfluxDB(data_point.id, value);
        // SaveToRedis는 BaseDeviceWorker에 없을 수 있으므로 제거하거나 확인 필요
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Failed to save data point " + data_point.name + ": " + std::string(e.what()));
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

} // namespace Workers
} // namespace PulseOne