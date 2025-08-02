// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// 최종 완성된 ModbusDriver 구현 (모든 에러 수정됨)
// =============================================================================

#include "Drivers/Modbus/ModbusDriver.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include "Common/DriverStatistics.h"
#include "Common/DriverError.h"  
#include <iostream>
#include <chrono>
#include <sstream>
#include <condition_variable>
#include <algorithm>

using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 구조체 생성자들
// =============================================================================

ModbusDataPointInfo::ModbusDataPointInfo() 
    : scaling_factor(1.0)
    , scaling_offset(0.0)
    , min_value(0.0)
    , max_value(0.0) {}

ModbusPacketLog::ModbusPacketLog() 
    : timestamp(std::chrono::system_clock::now())
    , slave_id(0)
    , function_code(0)
    , start_address(0)
    , data_count(0)
    , success(false)
    , response_time_ms(0.0) {}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ModbusDriver::ModbusDriver() 
    : modbus_ctx_(nullptr)
    , is_connected_(false)
    , current_slave_id_(-1)
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false)
    , console_output_enabled_(false)
    , log_manager_(nullptr)
    , db_manager_(nullptr) {
    
    // 응답시간 히스토그램 초기화
    for (auto& bucket : response_time_buckets_) {
        bucket = 0;
    }

    // 통계 초기화
    // statistics_ 초기화는 생성자에서 수행됨
    
    // 에러 초기화
    last_error_.code = ErrorCode::SUCCESS;
    last_error_.message = "";

    scaling_config_ = ScalingConfig();
    scaling_enabled_ = false;
    current_connection_index_ = 0;
    scaling_monitor_running_ = false;
    health_check_running_ = false;
    pool_avg_response_time_ = 0.0;
    pool_success_rate_ = 100.0;
    pool_total_operations_ = 0;
    pool_successful_operations_ = 0;
}

ModbusDriver::~ModbusDriver() {
    DisableScaling();
    
    Disconnect();
    if (modbus_ctx_) {
        modbus_free(modbus_ctx_);
        modbus_ctx_ = nullptr;
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // TCP 컨텍스트 생성
    if (config.protocol == ProtocolType::MODBUS_TCP) {
        // 엔드포인트에서 주소와 포트 파싱
        std::string host = "127.0.0.1";
        int port = 502;
        
        if (!config.endpoint.empty()) {
            size_t colon_pos = config.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                host = config.endpoint.substr(0, colon_pos);
                port = std::stoi(config.endpoint.substr(colon_pos + 1));
            }
        }
        
        modbus_ctx_ = modbus_new_tcp(host.c_str(), port);
    } else {
        // RTU의 경우
        modbus_ctx_ = modbus_new_rtu(config.endpoint.c_str(), 9600, 'N', 8, 1);
    }
    
    if (!modbus_ctx_) {
        SetError(ErrorCode::INTERNAL_ERROR, "Failed to create Modbus context");
        return false;
    }
    
    return true;
}

bool ModbusDriver::Connect() {
    if (is_connected_) return true;
    
    if (!modbus_ctx_) {
        SetError(ErrorCode::CONNECTION_FAILED, "Modbus context not initialized");
        return false;
    }
    
    if (modbus_connect(modbus_ctx_) == 0) {
        is_connected_ = true;
        modbus_set_slave(modbus_ctx_, 1);
        current_slave_id_ = 1;
        
        UpdateStatistics(true, 0.0);
        return true;
    } else {
        SetError(ErrorCode::CONNECTION_FAILED, "Modbus connection failed: " + std::string(modbus_strerror(errno)));
        UpdateStatistics(false, 0.0);
        return false;
    }
}

bool ModbusDriver::Disconnect() {
    if (!is_connected_) return true;
    
    if (modbus_ctx_) {
        modbus_close(modbus_ctx_);
    }
    is_connected_ = false;
    return true;
}

bool ModbusDriver::IsConnected() const {
    return is_connected_.load();
}

ProtocolType ModbusDriver::GetProtocolType() const {
    return config_.protocol;
}

// ✅ 수정: 올바른 반환 타입
Structs::DriverStatus ModbusDriver::GetStatus() const {
    return is_connected_ ? Structs::DriverStatus::RUNNING : Structs::DriverStatus::STOPPED;
}

// ✅ 수정: 올바른 타입들 사용
bool ModbusDriver::ReadValues(const std::vector<Structs::DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
    if (scaling_enabled_.load()) {
        // 🔥 새로운 연결 풀 방식 (추가)
        success = PerformReadWithConnectionPool(points, values);
    } else {
        // 🔥 기존 단일 연결 방식 (기존 코드를 새 함수로 분리)
        success = PerformReadWithSingleConnection(points, values);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 🔥 통계 업데이트 (기존 + 새로운 프로토콜별 통계)
    UpdateStatistics(success, duration.count());
    statistics_.IncrementProtocolCounter("register_reads");  // 추가
    
    if (!success) {
        statistics_.IncrementProtocolCounter("slave_errors");  // 추가
        
        // 🔥 에러 처리 (새로운 하이브리드 방식으로 변경)
        int errno_code = errno;
        HandleModbusError(errno_code, "Read values failed");  // 변경
    } else {
        HandleModbusError(0, "Read values successful");  // 추가
    }
    
    // 🔥 풀 통계 업데이트 (추가)
    if (scaling_enabled_.load()) {
        UpdatePoolStatistics();
    }
    
    return success;
}

// ✅ 수정: 올바른 함수 시그니처
bool ModbusDriver::WriteValue(const Structs::DataPoint& point, 
                             const Structs::DataValue& value) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
    if (scaling_enabled_.load()) {
        // 🔥 새로운 연결 풀 방식 (추가)
        success = PerformWriteWithConnectionPool(point, value);
    } else {
        // 🔥 기존 단일 연결 방식 (기존 코드를 새 함수로 분리)
        success = PerformWriteWithSingleConnection(point, value);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 🔥 통계 업데이트 (기존 + 새로운 프로토콜별 통계)
    UpdateStatistics(success, duration.count());
    
    // 🔥 Modbus 특화 통계 (추가)
    if (point.address >= 40001 && point.address <= 49999) {
        statistics_.IncrementProtocolCounter("holding_register_writes");
    } else if (point.address >= 1 && point.address <= 9999) {
        statistics_.IncrementProtocolCounter("coil_writes");
    }
    
    if (!success) {
        HandleModbusError(errno, "Write value failed");
    } else {
        HandleModbusError(0, "Write value successful");
    }
    
    return success;
}

ErrorInfo ModbusDriver::GetLastError() const {
    
    return last_error_;
}

const DriverStatistics& ModbusDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

// =============================================================================
// 진단 기능
// =============================================================================

bool ModbusDriver::EnableDiagnostics(DatabaseManager& db_manager,
                                    bool enable_packet_logging,
                                    bool enable_console_output) {
    log_manager_ = &LogManager::getInstance();
    db_manager_ = &db_manager;
    packet_logging_enabled_ = enable_packet_logging;
    console_output_enabled_ = enable_console_output;
    diagnostics_enabled_ = true;
    
    if (log_manager_) {
        log_manager_->Info("Modbus diagnostics enabled");
    }
    
    return true;
}

void ModbusDriver::DisableDiagnostics() {
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    if (log_manager_) {
        log_manager_->Info("Modbus diagnostics disabled");
    }
}

void ModbusDriver::ToggleConsoleMonitoring() {
    console_output_enabled_ = !console_output_enabled_;
}

void ModbusDriver::TogglePacketLogging() {
    packet_logging_enabled_ = !packet_logging_enabled_;
}

std::string ModbusDriver::GetDiagnosticsJSON() const {
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"protocol\": \"Modbus\",\n";
    ss << "  \"protocol\": \"" << (config_.protocol == ProtocolType::MODBUS_TCP ? "TCP" : "RTU") << "\",\n";
    ss << "  \"endpoint\": \"" << config_.endpoint << "\",\n";
    ss << "  \"connected\": " << (is_connected_ ? "true" : "false") << ",\n";
    ss << "  \"current_slave_id\": " << current_slave_id_ << ",\n";
    ss << "  \"diagnostics_enabled\": " << (diagnostics_enabled_ ? "true" : "false") << ",\n";
    
    // 통계 정보
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ss << "  \"statistics\": {\n";
        ss << "    \"total_operations\": " << statistics_.total_operations << ",\n";
        ss << "    \"successful_operations\": " << statistics_.successful_operations << ",\n";
        ss << "    \"failed_operations\": " << statistics_.failed_operations << ",\n";
        ss << "    \"success_rate\": " << statistics_.success_rate << ",\n";
        ss << "    \"avg_response_time_ms\": " << statistics_.avg_response_time_ms << "\n";
        ss << "  },\n";
    }
    
    // 에러 정보
    {
        
        ss << "  \"last_error\": {\n";
        ss << "    \"code\": " << static_cast<int>(last_error_.code) << ",\n";
        ss << "    \"message\": \"" << last_error_.message << "\"\n";
        ss << "  }\n";
    }
    
    ss << "}";
    
    return ss.str();
}

std::string ModbusDriver::GetRecentPacketsJSON(int /*count*/) const {
    return "{\"packets\":[]}";
}

// =============================================================================
// Modbus 특화 진단 기능들
// =============================================================================

std::map<uint8_t, uint64_t> ModbusDriver::GetExceptionCodeStats() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::map<uint8_t, uint64_t> stats;
    for (const auto& pair : exception_counters_) {
        stats[pair.first] = pair.second.load();
    }
    return stats;
}

double ModbusDriver::GetCrcErrorRate() const {
    uint64_t total = total_crc_checks_.load();
    if (total == 0) return 0.0;
    return (static_cast<double>(crc_errors_.load()) / total) * 100.0;
}

std::map<int, ModbusDriver::SlaveHealthInfo> ModbusDriver::GetSlaveHealthStatus() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return slave_health_map_;
}

std::vector<uint64_t> ModbusDriver::GetResponseTimeHistogram() const {
    std::vector<uint64_t> histogram;
    histogram.reserve(response_time_buckets_.size());
    
    for (const auto& bucket : response_time_buckets_) {
        histogram.push_back(bucket.load());
    }
    return histogram;
}

std::string ModbusDriver::GetRegisterAccessReport() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"register_access_patterns\": [\n";
    
    bool first = true;
    for (const auto& pair : register_access_patterns_) {
        if (!first) oss << ",\n";
        first = false;
        
        const auto& pattern = pair.second;
        oss << "    {\n";
        oss << "      \"address\": " << pair.first << ",\n";
        oss << "      \"read_count\": " << pattern.read_count.load() << ",\n";
        oss << "      \"write_count\": " << pattern.write_count.load() << ",\n";
        oss << "      \"avg_response_time_ms\": " << pattern.avg_response_time_ms.load() << "\n";
        oss << "    }";
    }
    
    oss << "\n  ],\n";
    oss << "  \"total_registers_accessed\": " << register_access_patterns_.size() << "\n";
    oss << "}";
    
    return oss.str();
}

std::string ModbusDriver::GetModbusHealthReport() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"modbus_diagnostics\": {\n";
    oss << "    \"crc_error_rate\": " << GetCrcErrorRate() << ",\n";
    oss << "    \"total_crc_checks\": " << total_crc_checks_.load() << ",\n";
    oss << "    \"crc_errors\": " << crc_errors_.load() << ",\n";
    
    // Exception Code 통계
    oss << "    \"exception_codes\": {\n";
    auto exception_stats = GetExceptionCodeStats();
    bool first_exc = true;
    for (const auto& pair : exception_stats) {
        if (!first_exc) oss << ",\n";
        first_exc = false;
        oss << "      \"0x" << std::hex << (int)pair.first << std::dec << "\": " << pair.second;
    }
    oss << "\n    },\n";
    
    // 응답시간 히스토그램
    auto histogram = GetResponseTimeHistogram();
    oss << "    \"response_time_histogram\": {\n";
    oss << "      \"0-10ms\": " << histogram[0] << ",\n";
    oss << "      \"10-50ms\": " << histogram[1] << ",\n";
    oss << "      \"50-100ms\": " << histogram[2] << ",\n";
    oss << "      \"100-500ms\": " << histogram[3] << ",\n";
    oss << "      \"500ms+\": " << histogram[4] << "\n";
    oss << "    },\n";
    
    // 슬레이브 건강상태 요약
    auto slave_health = GetSlaveHealthStatus();
    oss << "    \"slave_summary\": {\n";
    oss << "      \"total_slaves\": " << slave_health.size() << ",\n";
    
    int online_count = 0;
    for (const auto& pair : slave_health) {
        if (pair.second.is_online) online_count++;
    }
    oss << "      \"online_slaves\": " << online_count << ",\n";
    oss << "      \"offline_slaves\": " << (slave_health.size() - online_count) << "\n";
    oss << "    }\n";
    
    oss << "  }\n";
    oss << "}";
    
    return oss.str();
}

std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
    std::map<std::string, std::string> diagnostics;
    
    // 기존 진단 정보
    diagnostics["protocol"] = "Modbus";
    diagnostics["endpoint"] = config_.endpoint;
    diagnostics["connected"] = is_connected_ ? "true" : "false";
    diagnostics["current_slave_id"] = std::to_string(current_slave_id_);
    
    // 확장된 진단 정보
    diagnostics["crc_error_rate"] = std::to_string(GetCrcErrorRate()) + "%";
    diagnostics["total_crc_checks"] = std::to_string(total_crc_checks_.load());
    diagnostics["crc_errors"] = std::to_string(crc_errors_.load());
    
    // 응답시간 히스토그램
    auto histogram = GetResponseTimeHistogram();
    diagnostics["response_0_10ms"] = std::to_string(histogram[0]);
    diagnostics["response_10_50ms"] = std::to_string(histogram[1]);
    diagnostics["response_50_100ms"] = std::to_string(histogram[2]);
    diagnostics["response_100_500ms"] = std::to_string(histogram[3]);
    diagnostics["response_500ms_plus"] = std::to_string(histogram[4]);
    
    // 스케일링 상태
    if (scaling_enabled_.load()) {
        auto pool_status = GetPoolStatus();
        diagnostics["scaling_enabled"] = "true";
        diagnostics["total_connections"] = std::to_string(pool_status.total_connections);
        diagnostics["active_connections"] = std::to_string(pool_status.active_connections);
        diagnostics["pool_success_rate"] = std::to_string(pool_status.success_rate) + "%";
        diagnostics["pool_avg_response_ms"] = std::to_string(pool_status.avg_response_time_ms);
    } else {
        diagnostics["scaling_enabled"] = "false";
    }
    
    return diagnostics;
}

// =============================================================================
// Protected 진단 헬퍼 메소드들
// =============================================================================

void ModbusDriver::RecordExceptionCode(uint8_t exception_code) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    exception_counters_[exception_code].fetch_add(1);
}

void ModbusDriver::RecordCrcCheck(bool crc_valid) {
    total_crc_checks_.fetch_add(1);
    if (!crc_valid) {
        crc_errors_.fetch_add(1);
    }
}

void ModbusDriver::RecordResponseTime(int slave_id, uint32_t response_time_ms) {
    // 히스토그램 업데이트
    if (response_time_ms < 10) {
        response_time_buckets_[0].fetch_add(1);
    } else if (response_time_ms < 50) {
        response_time_buckets_[1].fetch_add(1);
    } else if (response_time_ms < 100) {
        response_time_buckets_[2].fetch_add(1);
    } else if (response_time_ms < 500) {
        response_time_buckets_[3].fetch_add(1);
    } else {
        response_time_buckets_[4].fetch_add(1);
    }
    
    // 슬레이브별 평균 응답시간 업데이트
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        auto& health = slave_health_map_[slave_id];
        
        uint32_t current_avg = health.avg_response_time_ms;
        uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;
        health.avg_response_time_ms = new_avg;
    }
}

void ModbusDriver::RecordRegisterAccess(uint16_t register_address, bool is_write, uint32_t response_time_ms) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto& pattern = register_access_patterns_[register_address];
    if (is_write) {
        pattern.write_count.fetch_add(1);
    } else {
        pattern.read_count.fetch_add(1);
    }
    
    pattern.last_access = std::chrono::system_clock::now();
    
    // 평균 응답시간 업데이트
    uint32_t current_avg = pattern.avg_response_time_ms.load();
    uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;
    pattern.avg_response_time_ms.store(new_avg);
}

void ModbusDriver::RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto& health = slave_health_map_[slave_id];
    if (success) {
        health.successful_requests++;
        health.is_online = true;
        health.last_response_time = std::chrono::system_clock::now();
        
        uint32_t current_avg = health.avg_response_time_ms;
        uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;
        health.avg_response_time_ms = new_avg;
    } else {
        health.failed_requests++;
        
        uint64_t total_requests = health.successful_requests + health.failed_requests;
        if (total_requests >= 5 && health.failed_requests >= 5) {
            health.is_online = false;
        }
    }
}

// =============================================================================
// Private 헬퍼 메소드들
// =============================================================================

void ModbusDriver::SetError(ErrorCode code, const std::string& message) {
    // 🔥 하이브리드 에러 정보 설정 (변경)
    last_error_.code = code;
    last_error_.message = message;
    last_error_.protocol = "MODBUS";
    last_error_.occurred_at = std::chrono::system_clock::now();
    
    // 🔥 통계 업데이트 (추가)
    statistics_.IncrementProtocolCounter("total_errors");
    
    // 기존 로깅 (그대로 유지)
    if (log_manager_) {
        log_manager_->Error("ModbusDriver Error [" + std::to_string(static_cast<int>(code)) + "]: " + message);
    }
}


void ModbusDriver::UpdateStatistics(bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_operations++;
    
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    statistics_.success_rate = 
        (double)statistics_.successful_operations / statistics_.total_operations * 100.0;
    
    statistics_.last_connection_time = system_clock::now();
    
    if (response_time_ms > 0) {
        if (statistics_.avg_response_time_ms == 0.0) {
            statistics_.avg_response_time_ms = response_time_ms;
        } else {
            statistics_.avg_response_time_ms = 
                (statistics_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
        }
    }
}

// ✅ 수정: DataType enum 올바르게 처리
Structs::DataValue ModbusDriver::ConvertModbusValue(
    const Structs::DataPoint& point, 
    uint16_t raw_value) const {
    
    // 🔥 기존 문자열 기반 data_type 처리 + 스케일링 적용
    Structs::DataValue result;
    
    if (point.data_type == "BOOL") {
        result = Structs::DataValue(raw_value != 0);
    } 
    else if (point.data_type == "INT16") {
        result = Structs::DataValue(static_cast<int16_t>(raw_value));
    } 
    else if (point.data_type == "UINT16") {
        result = Structs::DataValue(static_cast<uint16_t>(raw_value));
    }
    else if (point.data_type == "UINT32") {
        result = Structs::DataValue(static_cast<uint32_t>(raw_value));
    } 
    else if (point.data_type == "INT32") {
        // 32비트는 2개 레지스터 조합 (향후 확장)
        result = Structs::DataValue(static_cast<int32_t>(raw_value));
    } 
    else if (point.data_type == "FLOAT32") {
        float scaled_value = static_cast<float>(raw_value);
        
        // 스케일링 팩터 적용 (DataPoint 구조체의 scaling_factor 사용)
        scaled_value *= static_cast<float>(point.scaling_factor);
        
        // 오프셋 적용 (DataPoint 구조체의 scaling_offset 사용)
        scaled_value += static_cast<float>(point.scaling_offset);
        
        // properties에서도 확인 (호환성)
        if (point.properties.count("scaling_factor")) {
            float factor = std::stof(point.properties.at("scaling_factor"));
            scaled_value *= factor;
        }
        if (point.properties.count("offset")) {
            float offset = std::stof(point.properties.at("offset"));
            scaled_value += offset;
        }
        
        result = Structs::DataValue(scaled_value);
    } 
    else if (point.data_type == "FLOAT64" || point.data_type == "DOUBLE") {
        double scaled_value = static_cast<double>(raw_value);
        
        // 스케일링 팩터 적용 (DataPoint 구조체의 scaling_factor 사용)
        scaled_value *= point.scaling_factor;
        
        // 오프셋 적용 (DataPoint 구조체의 scaling_offset 사용)
        scaled_value += point.scaling_offset;
        
        // properties에서도 확인 (호환성)
        if (point.properties.count("scaling_factor")) {
            double factor = std::stod(point.properties.at("scaling_factor"));
            scaled_value *= factor;
        }
        if (point.properties.count("offset")) {
            double offset = std::stod(point.properties.at("offset"));
            scaled_value += offset;
        }
        
        result = Structs::DataValue(scaled_value);
    } 
    else if (point.data_type == "STRING") {
        result = Structs::DataValue(std::to_string(raw_value));
    } 
    else {
        // 기본값: DOUBLE로 처리
        result = Structs::DataValue(static_cast<double>(raw_value));
    }
    
    return result;
}

uint16_t ModbusDriver::ConvertToModbusValue(
    const Structs::DataPoint& point, 
    const Structs::DataValue& value) const {
    
    // 🔥 기존 문자열 기반 data_type 처리 + 역변환 스케일링 적용
    uint16_t result = 0;
    
    if (std::holds_alternative<bool>(value)) {
        result = std::get<bool>(value) ? 1 : 0;
    } 
    else if (std::holds_alternative<int16_t>(value)) {
        result = static_cast<uint16_t>(std::get<int16_t>(value));
    } 
    else if (std::holds_alternative<unsigned int>(value)) {
        result = std::get<unsigned int>(value);
    }
    else if (std::holds_alternative<int32_t>(value)) {
        result = static_cast<uint16_t>(std::get<int32_t>(value));
    } 
    else if (std::holds_alternative<uint32_t>(value)) {
        result = static_cast<uint16_t>(std::get<uint32_t>(value));
    }
    else if (std::holds_alternative<float>(value)) {
        float float_val = std::get<float>(value);
        
        // 역변환: properties 오프셋 제거
        if (point.properties.count("offset")) {
            float offset = std::stof(point.properties.at("offset"));
            float_val -= offset;
        }
        
        // 역변환: properties 스케일링 팩터 나누기
        if (point.properties.count("scaling_factor")) {
            float factor = std::stof(point.properties.at("scaling_factor"));
            if (factor != 0.0f) {
                float_val /= factor;
            }
        }
        
        // 역변환: DataPoint 구조체 오프셋 제거
        float_val -= static_cast<float>(point.scaling_offset);
        
        // 역변환: DataPoint 구조체 스케일링 팩터 나누기
        if (point.scaling_factor != 0.0) {
            float_val /= static_cast<float>(point.scaling_factor);
        }
        
        result = static_cast<uint16_t>(float_val);
    } 
    else if (std::holds_alternative<double>(value)) {
        double double_val = std::get<double>(value);
        
        // 역변환: properties 오프셋 제거
        if (point.properties.count("offset")) {
            double offset = std::stod(point.properties.at("offset"));
            double_val -= offset;
        }
        
        // 역변환: properties 스케일링 팩터 나누기
        if (point.properties.count("scaling_factor")) {
            double factor = std::stod(point.properties.at("scaling_factor"));
            if (factor != 0.0) {
                double_val /= factor;
            }
        }
        
        // 역변환: DataPoint 구조체 오프셋 제거
        double_val -= point.scaling_offset;
        
        // 역변환: DataPoint 구조체 스케일링 팩터 나누기
        if (point.scaling_factor != 0.0) {
            double_val /= point.scaling_factor;
        }
        
        result = static_cast<uint16_t>(double_val);
    } 
    else if (std::holds_alternative<std::string>(value)) {
        try {
            result = static_cast<uint16_t>(std::stoi(std::get<std::string>(value)));
        } catch (...) {
            result = 0;
        }
    }
    
    return result;
}

// =============================================================================
// 스텁 메소드들
// =============================================================================

std::string ModbusDriver::GetPointName(int address) const {
    return "Register_" + std::to_string(address);
}

std::string ModbusDriver::GetPointDescription(int address) const {
    return "Modbus register at address " + std::to_string(address);
}

std::string ModbusDriver::FormatPointValue(int /*address*/, uint16_t raw_value) const {
    return std::to_string(raw_value);
}

std::string ModbusDriver::FormatMultipleValues(uint16_t start_addr,
                                             const std::vector<uint16_t>& values) const {
    std::string result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) result += ", ";
        result += "Addr" + std::to_string(start_addr + i) + ":" + std::to_string(values[i]);
    }
    return result;
}

std::string ModbusDriver::FormatRawPacket(const std::vector<uint8_t>& packet) const {
    std::string result;
    for (size_t i = 0; i < packet.size(); ++i) {
        if (i > 0) result += " ";
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", packet[i]);
        result += hex;
    }
    return result;
}

std::string ModbusDriver::GetFunctionName(uint8_t function_code) const {
    switch (function_code) {
        case 0x01: return "Read Coils";
        case 0x02: return "Read Discrete Inputs";
        case 0x03: return "Read Holding Registers";
        case 0x04: return "Read Input Registers";
        case 0x05: return "Write Single Coil";
        case 0x06: return "Write Single Register";
        case 0x0F: return "Write Multiple Coils";
        case 0x10: return "Write Multiple Registers";
        default: return "Unknown Function";
    }
}

std::string ModbusDriver::FormatPacketForConsole(const ModbusPacketLog& /*log*/) const {
    return "Packet log formatting not implemented";
}

void ModbusDriver::TrimPacketHistory() {
    // 구현하지 않음 (스텁)
}

void ModbusDriver::LogModbusPacket(const std::string& /*direction*/,
                                  int /*slave_id*/,
                                  uint8_t /*function_code*/,
                                  uint16_t /*start_addr*/,
                                  uint16_t /*count*/,
                                  const std::vector<uint16_t>& /*values*/,
                                  bool /*success*/,
                                  const std::string& /*error_msg*/,
                                  double /*response_time_ms*/) {
    // 구현하지 않음 (스텁)
}

bool ModbusDriver::LoadDataPointsFromDB() { 
    return true; 
}

std::string ModbusDriver::QueryDeviceName(const std::string& device_id) { 
    return "Modbus_" + device_id; 
}

bool ModbusDriver::QueryDataPoints(const std::string& /*device_id*/) { 
    return true; 
}

// =============================================================================
// 🔥 6. 하이브리드 에러 시스템 메서드 추가
// =============================================================================

std::string ModbusDriver::GetDetailedErrorInfo() const {
    return last_error_.GetDetailedInfo();
}

std::string ModbusDriver::GetErrorJson() const {
    return last_error_.ToJsonString();
}

int ModbusDriver::GetModbusErrorCode() const {
    return last_error_.native_error_code;
}

std::string ModbusDriver::GetModbusErrorName() const {
    return last_error_.native_error_name;
}

void ModbusDriver::HandleModbusError(int modbus_error, const std::string& context) {
    // 🔥 Modbus 에러를 PulseOne 표준 에러로 변환 (추가)
    // TODO: ModbusErrorConverter 구현 필요
    
    if (last_error_.IsFailure()) {
        statistics_.IncrementProtocolCounter("total_errors");
        
        // 에러 타입별 세부 통계
        switch (last_error_.code) {
            case ErrorCode::CONNECTION_TIMEOUT:
                statistics_.IncrementProtocolCounter("timeout_errors");
                break;
            case ErrorCode::CHECKSUM_ERROR:
                statistics_.IncrementProtocolCounter("crc_errors");
                break;
            case ErrorCode::MAINTENANCE_ACTIVE:
                statistics_.IncrementProtocolCounter("slave_busy_errors");
                break;
            case ErrorCode::INVALID_PARAMETER:
                statistics_.IncrementProtocolCounter("address_errors");
                break;
            default:
                statistics_.IncrementProtocolCounter("other_errors");
                break;
        }
    }
}

bool ModbusDriver::EnableScaling(const ScalingConfig& config) {
    if (scaling_enabled_.load()) {
        HandleModbusError(-1, "Scaling already enabled");
        return false;
    }
    
    scaling_config_ = config;
    
    // 연결 풀 초기화
    if (!InitializeConnectionPool()) {
        return false;
    }
    
    // 모니터링 스레드 시작
    scaling_monitor_running_ = true;
    health_check_running_ = true;
    
    scaling_monitor_thread_ = std::thread(&ModbusDriver::ScalingMonitorThread, this);
    health_check_thread_ = std::thread(&ModbusDriver::HealthCheckThread, this);
    
    scaling_enabled_ = true;
    
    // 통계 업데이트
    statistics_.SetProtocolStatus("scaling_enabled", "true");
    statistics_.SetProtocolStatus("load_balancing_strategy", 
        std::to_string(static_cast<int>(scaling_config_.strategy)));
    
    RecordScalingEvent(ScalingEvent::SCALE_UP, "Scaling enabled", 1, 
                      connection_pool_.size(), 0.0);
    
    return true;
}

void ModbusDriver::DisableScaling() {
    if (!scaling_enabled_.load()) return;
    
    // 모니터링 스레드 종료
    scaling_monitor_running_ = false;
    health_check_running_ = false;
    
    if (scaling_monitor_thread_.joinable()) {
        scaling_monitor_thread_.join();
    }
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
    
    // 연결 풀 정리 (메인 연결 하나만 남김)
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        if (!connection_pool_.empty()) {
            // 첫 번째 연결을 메인 연결로 사용
            auto main_connection = std::move(connection_pool_[0]);
            
            // 나머지 연결들 정리
            for (size_t i = 1; i < connection_pool_.size(); ++i) {
                if (connection_pool_[i] && connection_pool_[i]->ctx) {
                    if (connection_pool_[i]->is_connected) {
                        modbus_close(connection_pool_[i]->ctx.get());
                    }
                }
            }
            
            // 메인 modbus_ctx_로 설정
            if (main_connection && main_connection->ctx) {
                modbus_ctx_ = main_connection->ctx.release();
                is_connected_ = main_connection->is_connected.load();
            }
        }
        
        // 풀 정리
        connection_pool_.clear();
        while (!available_connections_.empty()) {
            available_connections_.pop();
        }
    }
    
    scaling_enabled_ = false;
    statistics_.SetProtocolStatus("scaling_enabled", "false");
    
    RecordScalingEvent(ScalingEvent::SCALE_DOWN, "Scaling disabled", 
                      connection_pool_.size(), 1, 0.0);
}

ModbusDriver::PoolStatus ModbusDriver::GetPoolStatus() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    PoolStatus status;
    status.total_connections = connection_pool_.size();
    status.available_connections = available_connections_.size();
    status.active_connections = status.total_connections - status.available_connections;
    status.avg_response_time_ms = pool_avg_response_time_.load();
    status.success_rate = pool_success_rate_.load();
    status.total_operations = pool_total_operations_.load();
    status.current_strategy = scaling_config_.strategy;
    
    // 건강한 연결 수 계산
    status.healthy_connections = 0;
    for (const auto& conn : connection_pool_) {
        if (conn && conn->IsHealthy()) {
            status.healthy_connections++;
        }
    }
    
    // 연결별 상세 정보
    for (const auto& conn : connection_pool_) {
        if (conn) {
            PoolStatus::ConnectionInfo info;
            info.id = conn->connection_id;
            info.connected = conn->is_connected.load();
            info.busy = conn->is_busy.load();
            info.healthy = conn->IsHealthy();
            info.operations = conn->total_operations.load();
            info.avg_response_ms = conn->avg_response_time_ms.load();
            info.idle_time = conn->GetIdleTime();
            info.lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - conn->created_at);
            
            status.connections.push_back(info);
        }
    }
    
    return status;
}


bool ModbusDriver::InitializeConnectionPool() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    connection_pool_.clear();
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    // 초기 연결 수만큼 생성
    size_t initial_count = std::max(scaling_config_.min_connections, 
                                   scaling_config_.initial_connections);
    
    for (size_t i = 0; i < initial_count; ++i) {
        auto conn = CreateConnection(i);
        if (!conn) {
            return false;
        }
        
        connection_pool_.push_back(std::move(conn));
        available_connections_.push(i);
    }
    
    return true;
}

std::unique_ptr<ModbusDriver::ModbusConnection> ModbusDriver::CreateConnection(int connection_id) {
    auto conn = std::make_unique<ModbusConnection>(connection_id);
    
    // 기존 modbus_ctx_ 생성 로직 재사용
    if (config_.protocol == ProtocolType::MODBUS_TCP) {
        std::string host = "127.0.0.1";
        int port = 502;
        
        if (!config_.endpoint.empty()) {
            size_t colon_pos = config_.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                host = config_.endpoint.substr(0, colon_pos);
                port = std::stoi(config_.endpoint.substr(colon_pos + 1));
            }
        }
        
        modbus_t* ctx = modbus_new_tcp(host.c_str(), port);
        if (!ctx) {
            return nullptr;
        }
        
        conn->ctx.reset(ctx);
        conn->endpoint = config_.endpoint;
        
        // 연결 시도
        if (EstablishConnection(conn.get())) {
            conn->is_connected = true;
        }
    }
    
    return conn;
}

ModbusDriver::ModbusConnection* ModbusDriver::AcquireConnection(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // 사용 가능한 연결 대기
    if (pool_cv_.wait_for(lock, timeout, [this] { return !available_connections_.empty(); })) {
        int conn_id = available_connections_.front();
        available_connections_.pop();
        
        auto& conn = connection_pool_[conn_id];
        conn->is_busy = true;
        conn->last_used = std::chrono::system_clock::now();
        
        return conn.get();
    }
    
    return nullptr;  // 타임아웃
}

void ModbusDriver::ReleaseConnection(ModbusConnection* conn) {
    if (!conn) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    conn->is_busy = false;
    available_connections_.push(conn->connection_id);
    
    pool_cv_.notify_one();
}


bool ModbusDriver::PerformReadWithSingleConnection(
    const std::vector<Structs::DataPoint>& points,
    std::vector<TimestampedValue>& values) {
    
    values.clear();
    
    if (!is_connected_ || !modbus_ctx_) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to Modbus device");
        return false;
    }
    
    bool overall_success = true;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& point : points) {
        TimestampedValue tvalue;
        tvalue.timestamp = std::chrono::system_clock::now();
        
        // 슬레이브 ID 변경이 필요한 경우
        if (point.properties.count("slave_id")) {
            int slave_id = std::stoi(point.properties.at("slave_id"));
            if (slave_id != current_slave_id_) {
                modbus_set_slave(modbus_ctx_, slave_id);
                current_slave_id_ = slave_id;
            }
        }
        
        // Modbus 기능별 읽기 수행
        std::string function = "holding_registers"; // 기본값
        if (point.properties.count("modbus_function")) {
            function = point.properties.at("modbus_function");
        }
        
        uint16_t raw_value = 0;
        int result = -1;
        
        if (function == "holding_registers" || function == "03") {
            result = modbus_read_registers(modbus_ctx_, point.address, 1, &raw_value);
        } 
        else if (function == "input_registers" || function == "04") {
            result = modbus_read_input_registers(modbus_ctx_, point.address, 1, &raw_value);
        }
        else if (function == "coils" || function == "01") {
            uint8_t coil_value = 0;
            result = modbus_read_bits(modbus_ctx_, point.address, 1, &coil_value);
            raw_value = coil_value ? 1 : 0;
        }
        else if (function == "discrete_inputs" || function == "02") {
            uint8_t input_value = 0;
            result = modbus_read_input_bits(modbus_ctx_, point.address, 1, &input_value);
            raw_value = input_value ? 1 : 0;
        }
        
        if (result > 0) {
            // 성공적으로 읽음
            tvalue.value = ConvertModbusValue(point, raw_value);
            tvalue.quality = DataQuality::GOOD;
            
            // 레지스터 접근 패턴 업데이트
            UpdateRegisterAccessPattern(point.address, true, false);
            
            // 진단 정보 출력
            if (console_output_enabled_) {
                std::cout << "[READ] Slave:" << current_slave_id_ 
                         << " Addr:" << point.address 
                         << " Value:" << raw_value 
                         << " Function:" << function << std::endl;
            }
            
            // 패킷 로깅
            if (packet_logging_enabled_) {
                LogModbusPacket("READ", current_slave_id_, 
                              (function == "holding_registers") ? 0x03 : 0x04,
                              point.address, 1, {raw_value}, true, "", 0.0);
            }
        } 
        else {
            // 읽기 실패
            tvalue.value = Structs::DataValue(0.0);
            tvalue.quality = DataQuality::BAD;
            overall_success = false;
            
            // 에러 정보 기록
            std::string error_msg = "Failed to read " + function + 
                                  " at address " + std::to_string(point.address) + 
                                  ": " + modbus_strerror(errno);
            
            if (console_output_enabled_) {
                std::cout << "[ERROR] " << error_msg << std::endl;
            }
            
            // Modbus 예외 코드별 통계
            uint8_t exception_code = static_cast<uint8_t>(errno);
            if (exception_counters_.count(exception_code)) {
                exception_counters_[exception_code]++;
            } else {
                exception_counters_[exception_code] = 1;
            }
        }
        
        values.push_back(tvalue);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 응답시간 히스토그램 업데이트
    UpdateResponseTimeHistogram(duration.count());
    
    return overall_success;
}


bool ModbusDriver::PerformWriteWithSingleConnection(
    const Structs::DataPoint& point,
    const Structs::DataValue& value) {
    
    if (!is_connected_ || !modbus_ctx_) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to Modbus device");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 슬레이브 ID 변경이 필요한 경우
    if (point.properties.count("slave_id")) {
        int slave_id = std::stoi(point.properties.at("slave_id"));
        if (slave_id != current_slave_id_) {
            modbus_set_slave(modbus_ctx_, slave_id);
            current_slave_id_ = slave_id;
        }
    }
    
    // Modbus 기능별 쓰기 수행
    std::string function = "holding_registers"; // 기본값
    if (point.properties.count("modbus_function")) {
        function = point.properties.at("modbus_function");
    }
    
    int result = -1;
    
    if (function == "holding_registers" || function == "06") {
        uint16_t modbus_value = ConvertToModbusValue(point, value);
        result = modbus_write_register(modbus_ctx_, point.address, modbus_value);
        
        if (console_output_enabled_) {
            std::cout << "[WRITE] Slave:" << current_slave_id_ 
                     << " Addr:" << point.address 
                     << " Value:" << modbus_value << std::endl;
        }
    }
    else if (function == "coils" || function == "05") {
        bool coil_value = false;
        if (std::holds_alternative<bool>(value)) {
            coil_value = std::get<bool>(value);
        } else if (std::holds_alternative<int>(value)) {
            coil_value = (std::get<int>(value) != 0);
        }
        
        result = modbus_write_bit(modbus_ctx_, point.address, coil_value ? 1 : 0);
        
        if (console_output_enabled_) {
            std::cout << "[WRITE] Slave:" << current_slave_id_ 
                     << " Addr:" << point.address 
                     << " Coil:" << (coil_value ? "ON" : "OFF") << std::endl;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    bool success = (result > 0);
    
    if (success) {
        // 레지스터 접근 패턴 업데이트
        UpdateRegisterAccessPattern(point.address, false, true);
        
        // 패킷 로깅
        if (packet_logging_enabled_) {
            uint16_t written_value = ConvertToModbusValue(point, value);
            LogModbusPacket("WRITE", current_slave_id_, 
                          (function == "holding_registers") ? 0x06 : 0x05,
                          point.address, 1, {written_value}, true, "", duration.count());
        }
    } else {
        // 쓰기 실패
        std::string error_msg = "Failed to write " + function + 
                              " at address " + std::to_string(point.address) + 
                              ": " + modbus_strerror(errno);
        
        SetError(ErrorCode::DATA_FORMAT_ERROR, error_msg);
        
        if (console_output_enabled_) {
            std::cout << "[ERROR] " << error_msg << std::endl;
        }
        
        // Modbus 예외 코드별 통계
        uint8_t exception_code = static_cast<uint8_t>(errno);
        if (exception_counters_.count(exception_code)) {
            exception_counters_[exception_code]++;
        } else {
            exception_counters_[exception_code] = 1;
        }
    }
    
    // 응답시간 히스토그램 업데이트
    UpdateResponseTimeHistogram(duration.count());
    
    return success;
}

// =============================================================================
// 🔥 11. 연결 풀 작업 메서드들 (추가)
// =============================================================================

bool ModbusDriver::PerformReadWithConnectionPool(const std::vector<Structs::DataPoint>& points,
                                                std::vector<TimestampedValue>& values) {
    auto conn = AcquireConnection();
    if (!conn) {
        HandleModbusError(-1, "No available connections in pool");
        return false;
    }
    
    bool success = PerformReadWithConnection(conn, points, values);
    
    ReleaseConnection(conn);
    return success;
}

bool ModbusDriver::PerformReadWithConnection(
    ModbusConnection* conn, 
    const std::vector<Structs::DataPoint>& points,
    std::vector<TimestampedValue>& values) {
    
    if (!conn || !conn->ctx || !conn->is_connected) {
        return false;
    }
    
    values.clear();
    bool overall_success = true;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 임시로 현재 컨텍스트를 교체
    modbus_t* original_ctx = modbus_ctx_;
    bool original_connected = is_connected_;
    
    modbus_ctx_ = conn->ctx.get();
    is_connected_ = conn->is_connected.load();
    
    // 기존 단일 연결 로직 재사용
    bool success = PerformReadWithSingleConnection(points, values);
    
    // 원래 컨텍스트 복원
    modbus_ctx_ = original_ctx;
    is_connected_ = original_connected;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // 연결별 통계 업데이트
    conn->UpdateStats(success, duration_ms);
    
    return success;
}


bool ModbusDriver::PerformWriteWithConnection(
    ModbusConnection* conn,
    const Structs::DataPoint& point,
    const Structs::DataValue& value) {
    
    if (!conn || !conn->ctx || !conn->is_connected) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 임시로 현재 컨텍스트를 교체
    modbus_t* original_ctx = modbus_ctx_;
    bool original_connected = is_connected_;
    
    modbus_ctx_ = conn->ctx.get();
    is_connected_ = conn->is_connected.load();
    
    // 기존 단일 연결 로직 재사용
    bool success = PerformWriteWithSingleConnection(point, value);
    
    // 원래 컨텍스트 복원
    modbus_ctx_ = original_ctx;
    is_connected_ = original_connected;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // 연결별 통계 업데이트
    conn->UpdateStats(success, duration_ms);
    
    return success;
}

bool ModbusDriver::PerformWriteWithConnectionPool(
    const Structs::DataPoint& point,
    const Structs::DataValue& value) {
    
    auto conn = AcquireConnection();
    if (!conn) {
        HandleModbusError(-1, "No available connections in pool");
        return false;
    }
    
    bool success = PerformWriteWithConnection(conn, point, value);
    
    ReleaseConnection(conn);
    return success;
}


// =============================================================================
// 🔥 12. 모니터링 스레드들 (추가)
// =============================================================================

void ModbusDriver::ScalingMonitorThread() {
    while (scaling_monitor_running_.load()) {
        std::this_thread::sleep_for(scaling_config_.scale_check_interval);
        
        if (!scaling_enabled_.load()) continue;
        
        UpdatePoolStatistics();
        
        // 스케일 업 조건 확인
        if (ShouldScaleUp() && connection_pool_.size() < scaling_config_.max_connections) {
            PerformScaleUp(1, "Performance threshold exceeded");
        }
        
        // 스케일 다운 조건 확인
        if (ShouldScaleDown() && connection_pool_.size() > scaling_config_.min_connections) {
            PerformScaleDown(1, "Low utilization detected");
        }
    }
}

void ModbusDriver::UpdatePoolStatistics() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (connection_pool_.empty()) return;
    
    uint64_t total_ops = 0;
    uint64_t successful_ops = 0;
    double total_response_time = 0.0;
    size_t active_connections = 0;
    
    for (const auto& conn : connection_pool_) {
        if (conn && conn->is_connected) {
            active_connections++;
            total_ops += conn->total_operations.load();
            successful_ops += conn->successful_operations.load();
            total_response_time += conn->avg_response_time_ms.load();
        }
    }
    
    pool_total_operations_ = total_ops;
    pool_successful_operations_ = successful_ops;
    
    if (total_ops > 0) {
        pool_success_rate_ = (static_cast<double>(successful_ops) / total_ops) * 100.0;
    }
    
    if (active_connections > 0) {
        pool_avg_response_time_ = total_response_time / active_connections;
    }
}

bool ModbusDriver::ShouldScaleUp() const {
    double avg_response = pool_avg_response_time_.load();
    double success_rate = pool_success_rate_.load();
    
    return (avg_response > scaling_config_.max_response_time_ms) || 
           (success_rate < scaling_config_.min_success_rate);
}

bool ModbusDriver::ShouldScaleDown() const {
    double avg_response = pool_avg_response_time_.load();
    double success_rate = pool_success_rate_.load();
    
    bool performance_good = (avg_response < scaling_config_.max_response_time_ms * 0.5) && 
                           (success_rate > scaling_config_.min_success_rate);
    
    return performance_good;
}


// =============================================================================
// 🔥 진단 헬퍼 메서드들 - 기존 구조 확장
// =============================================================================

void ModbusDriver::UpdateRegisterAccessPattern(uint16_t address, bool is_read, bool is_write) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto& pattern = register_access_patterns_[address];
    if (is_read) {
        pattern.read_count++;
    }
    if (is_write) {
        pattern.write_count++;
    }
}

void ModbusDriver::UpdateResponseTimeHistogram(double response_time_ms) {
    size_t bucket_index = 0;
    
    if (response_time_ms <= 10.0) {
        bucket_index = 0;      // 0-10ms
    } else if (response_time_ms <= 50.0) {
        bucket_index = 1;      // 10-50ms
    } else if (response_time_ms <= 100.0) {
        bucket_index = 2;      // 50-100ms
    } else if (response_time_ms <= 500.0) {
        bucket_index = 3;      // 100-500ms
    } else {
        bucket_index = 4;      // 500ms+
    }
    
    if (bucket_index < response_time_buckets_.size()) {
        response_time_buckets_[bucket_index]++;
    }
}


// =============================================================================
// 🔥 13. ModbusConnection 멤버 메서드들 (추가)
// =============================================================================

ModbusDriver::ModbusConnection::ModbusConnection(int id) 
    : ctx(nullptr, modbus_free), connection_id(id)
    , last_used(std::chrono::system_clock::now())
    , created_at(std::chrono::system_clock::now()) {}

double ModbusDriver::ModbusConnection::GetSuccessRate() const {
    uint64_t total = total_operations.load();
    if (total == 0) return 100.0;
    return (static_cast<double>(successful_operations.load()) / total) * 100.0;
}

bool ModbusDriver::ModbusConnection::IsHealthy() const {
    return is_connected.load() && 
           GetSuccessRate() > 80.0 && 
           avg_response_time_ms.load() < 1000.0;
}

std::chrono::milliseconds ModbusDriver::ModbusConnection::GetIdleTime() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - last_used);
}

void ModbusDriver::ModbusConnection::UpdateStats(bool success, double response_time_ms) {
    total_operations++;
    if (success) {
        successful_operations++;
    }
    
    // 이동 평균으로 응답시간 업데이트
    double current_avg = avg_response_time_ms.load();
    double new_avg = (current_avg == 0.0) ? response_time_ms : 
                     (current_avg * 0.9 + response_time_ms * 0.1);
    avg_response_time_ms.store(new_avg);
    
    last_used = std::chrono::system_clock::now();
}

// =============================================================================
// 🔥 연결 관리 - 기존 Connect/Disconnect 활용
// =============================================================================

bool ModbusDriver::EstablishConnection(ModbusConnection* conn) {
    if (!conn || !conn->ctx) {
        return false;
    }
    
    // 기존 Connect 로직 재사용
    modbus_t* backup_ctx = modbus_ctx_;
    modbus_ctx_ = conn->ctx.get();
    
    bool success = (modbus_connect(modbus_ctx_) == 0);
    if (success) {
        modbus_set_slave(modbus_ctx_, 1);  // 기본 슬레이브 ID
    }
    
    modbus_ctx_ = backup_ctx;  // 원복
    
    return success;
}

// =============================================================================
// 🔥 기존 Bulk 읽기 메서드들과 통합
// =============================================================================

bool ModbusDriver::ReadHoldingRegistersBulk(int slave_id, uint16_t start_addr, 
                                           uint16_t count, std::vector<uint16_t>& registers,
                                           int max_retries) {
    if (!is_connected_ || !modbus_ctx_) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected");
        return false;
    }
    
    if (slave_id != current_slave_id_) {
        modbus_set_slave(modbus_ctx_, slave_id);
        current_slave_id_ = slave_id;
    }
    
    registers.resize(count);
    
    for (int retry = 0; retry <= max_retries; retry++) {
        int result = modbus_read_registers(modbus_ctx_, start_addr, count, registers.data());
        
        if (result == count) {
            // 성공
            if (console_output_enabled_) {
                std::cout << "[BULK_READ] Slave:" << slave_id 
                         << " Start:" << start_addr 
                         << " Count:" << count 
                         << " Success on retry:" << retry << std::endl;
            }
            
            return true;
        } else if (retry < max_retries) {
            // 재시도
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (console_output_enabled_) {
                std::cout << "[BULK_READ] Retry " << (retry + 1) << "/" << max_retries 
                         << " for Slave:" << slave_id << std::endl;
            }
        }
    }
    
    // 최종 실패
    SetError(ErrorCode::CONNECTION_TIMEOUT, "Bulk read failed after " + std::to_string(max_retries) + " retries");
    return false;
}

} // namespace Drivers
} // namespace PulseOne