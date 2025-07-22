// =============================================================================
// collector/src/Drivers/Modbus/ModbusDriver.cpp
// 최소한의 ModbusDriver 구현 (컴파일만 되는 버전)
// =============================================================================

#include "Drivers/Modbus/ModbusDriver.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <chrono>

using namespace PulseOne::Drivers;
using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

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

    // 통계 초기화 (실제 멤버에 맞춰 수정)
    statistics_.successful_connections = 0;
    statistics_.failed_connections = 0;
    statistics_.avg_response_time_ms = 0.0;
    statistics_.last_error_time = system_clock::now();
    
    last_error_.code = ErrorCode::SUCCESS;
    last_error_.message = "";
}

ModbusDriver::~ModbusDriver() {
    Disconnect();
}

// =============================================================================
// IProtocolDriver 인터페이스 구현 (최소한)
// =============================================================================

bool ModbusDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // TCP 컨텍스트 생성
    modbus_ctx_ = modbus_new_tcp("127.0.0.1", Constants::DEFAULT_MODBUS_TCP_PORT);
    if (!modbus_ctx_) {
        SetError(ErrorCode::CONNECTION_FAILED, "Failed to create Modbus context");
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
        return true;
    } else {
        SetError(ErrorCode::CONNECTION_FAILED, "Modbus connection failed");
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

DriverStatus ModbusDriver::GetStatus() const {
    return is_connected_ ? DriverStatus::RUNNING : DriverStatus::STOPPED;
}

bool ModbusDriver::ReadValues(const std::vector<DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    values.clear();
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected");
        return false;
    }
    
    // 간단한 구현
    for (const auto& point : points) {
        TimestampedValue tvalue;
        tvalue.timestamp = system_clock::now();
        
        uint16_t raw_value = 0;
        int result = modbus_read_registers(modbus_ctx_, point.address, 1, &raw_value);
        
        if (result == 1) {
            tvalue.value = DataValue(static_cast<double>(raw_value));
            tvalue.quality = DataQuality::GOOD;
        } else {
            tvalue.value = DataValue(0.0);
            tvalue.quality = DataQuality::BAD;
        }
        
        values.push_back(tvalue);
    }
    
    return true;
}

bool ModbusDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected");
        return false;
    }
    
    // 간단한 값 변환
    uint16_t modbus_value = 0;
    if (std::holds_alternative<double>(value)) {
        modbus_value = static_cast<uint16_t>(std::get<double>(value));
    }
    
    int result = modbus_write_register(modbus_ctx_, point.address, modbus_value);
    return (result == 1);
}

const DriverStatistics& ModbusDriver::GetStatistics() const {
    return statistics_;
}

// =============================================================================
// 진단 기능 (최소한)
// =============================================================================

bool ModbusDriver::EnableDiagnostics(DatabaseManager& db_manager,
                                    bool enable_packet_logging,
                                    bool enable_console_output) {
    log_manager_ = &PulseOne::LogManager::getInstance();
    db_manager_ = &db_manager;
    packet_logging_enabled_ = enable_packet_logging;
    console_output_enabled_ = enable_console_output;
    diagnostics_enabled_ = true;
    
    if (log_manager_) {
        log_manager_->Info("Diagnostics enabled");
    }
    
    return true;
}

void ModbusDriver::DisableDiagnostics() {
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    if (log_manager_) {
        log_manager_->Info("Diagnostics disabled");
    }
}

void ModbusDriver::ToggleConsoleMonitoring() {
    console_output_enabled_ = !console_output_enabled_;
}

void ModbusDriver::TogglePacketLogging() {
    packet_logging_enabled_ = !packet_logging_enabled_;
}

// =============================================================================
// 유틸리티 메소드들 (스텁)
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
        case 0x03: return "Read Holding Registers";
        case 0x06: return "Write Single Register";
        default: return "Unknown Function";
    }
}

std::string ModbusDriver::FormatPacketForConsole(const ModbusPacketLog& /*log*/) const {
    return "Packet log formatting not implemented";
}

void ModbusDriver::TrimPacketHistory() {
    // 구현하지 않음
}

std::string ModbusDriver::GetDiagnosticsJSON() const {
    return "{\"status\":\"minimal_implementation\"}";
}

std::string ModbusDriver::GetRecentPacketsJSON(int /*count*/) const {
    return "{\"packets\":[]}";
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
    // 구현하지 않음
}


// 🆕 Exception Code 통계 조회 구현
std::map<uint8_t, uint64_t> ModbusDriver::GetExceptionCodeStats() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::map<uint8_t, uint64_t> stats;
    for (const auto& pair : exception_counters_) {
        stats[pair.first] = pair.second.load();
    }
    return stats;
}

// 🆕 CRC 에러율 조회 구현
double ModbusDriver::GetCrcErrorRate() const {
    uint64_t total = total_crc_checks_.load();
    uint64_t errors = crc_errors_.load();
    
    if (total == 0) return 0.0;
    return (double)errors / total * 100.0;
}

// 🆕 슬레이브 건강상태 조회 구현
std::map<int, ModbusDriver::SlaveHealthInfo> ModbusDriver::GetSlaveHealthStatus() const {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return slave_health_map_;
}

// 🆕 응답시간 히스토그램 조회 구현
std::vector<uint64_t> ModbusDriver::GetResponseTimeHistogram() const {
    std::vector<uint64_t> histogram;
    histogram.reserve(response_time_buckets_.size());
    
    for (const auto& bucket : response_time_buckets_) {
        histogram.push_back(bucket.load());
    }
    return histogram;
}

// 🆕 레지스터 접근 패턴 분석 보고서 구현
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



// 🆕 Exception Code 기록 구현
void ModbusDriver::RecordExceptionCode(uint8_t exception_code) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    exception_counters_[exception_code].fetch_add(1);
}

// 🆕 CRC 검사 결과 기록 구현
void ModbusDriver::RecordCrcCheck(bool crc_valid) {
    total_crc_checks_.fetch_add(1);
    if (!crc_valid) {
        crc_errors_.fetch_add(1);
    }
}




// 🆕 레지스터 접근 기록 구현
void ModbusDriver::RecordRegisterAccess(uint16_t register_address, bool is_write, uint32_t response_time_ms) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto& pattern = register_access_patterns_[register_address];
    if (is_write) {
        pattern.write_count++;  // 🔧 이미 일반 타입으로 수정됨
    } else {
        pattern.read_count++;   // 🔧 이미 일반 타입으로 수정됨
    }
    
    pattern.last_access = std::chrono::system_clock::now();
    
    // 평균 응답시간 업데이트 (이동 평균)
    uint32_t current_avg = pattern.avg_response_time_ms;  // 🔧 일반 타입
    uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;
    pattern.avg_response_time_ms = new_avg;              // 🔧 일반 할당
}

// 🆕 슬레이브 요청 결과 기록 구현
// ==============================================================================
// atomic 메서드 호출 제거 패치
// 파일: collector/src/Drivers/Modbus/ModbusDriver.cpp
// ==============================================================================

// 🔧 수정 1: GetModbusHealthReport() 함수에서 .load() 제거
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
        // 🔧 수정: .load() 제거 - 일반 bool 타입이므로 직접 접근
        if (pair.second.is_online) online_count++;
    }
    oss << "      \"online_slaves\": " << online_count << ",\n";
    oss << "      \"offline_slaves\": " << (slave_health.size() - online_count) << "\n";
    oss << "    }\n";
    
    oss << "  }\n";
    oss << "}";
    
    return oss.str();
}

// 🆕 응답시간 기록 구현
void ModbusDriver::RecordResponseTime(int slave_id, uint32_t response_time_ms) {
    // 히스토그램 업데이트 (atomic이므로 그대로 유지)
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
    
    // 🔧 수정: 슬레이브별 평균 응답시간 업데이트 (일반 타입으로 변경)
    {
        std::lock_guard<std::mutex> lock(diagnostics_mutex_);
        auto& health = slave_health_map_[slave_id];
        
        // 🔧 수정: .load() 제거 - 일반 uint32_t 타입이므로 직접 접근
        uint32_t current_avg = health.avg_response_time_ms;
        uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;  // 90% 기존값, 10% 새값
        
        // 🔧 수정: .store() 제거 - 일반 할당으로 변경
        health.avg_response_time_ms = new_avg;
    }
}

// 🔧 수정 3: RecordSlaveRequest() 함수에서 atomic 메서드 제거
void ModbusDriver::RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto& health = slave_health_map_[slave_id];
    if (success) {
        // 🔧 수정: .fetch_add() 제거 - 일반 증가 연산으로 변경
        health.successful_requests++;
        
        // 🔧 수정: .store() 제거 - 일반 할당으로 변경
        health.is_online = true;
        
        health.last_response_time = std::chrono::system_clock::now();
        
        // 평균 응답시간 업데이트
        uint32_t current_avg = health.avg_response_time_ms;
        uint32_t new_avg = (current_avg * 9 + response_time_ms) / 10;
        health.avg_response_time_ms = new_avg;
    } else {
        // 🔧 수정: .fetch_add() 제거 - 일반 증가 연산으로 변경
        health.failed_requests++;
        
        // 연속 실패가 5회 이상이면 오프라인으로 판단
        // 🔧 수정: .load() 제거 - 일반 타입이므로 직접 접근
        uint64_t total_requests = health.successful_requests + health.failed_requests;
        uint64_t recent_failures = health.failed_requests;
        
        if (total_requests >= 5 && recent_failures >= 5) {
            // 🔧 수정: .store() 제거 - 일반 할당으로 변경
            health.is_online = false;
        }
    }
    
    // 히스토그램은 별도로 업데이트 (success일 때만)
    if (success) {
        // unlock 후에 RecordResponseTime 호출하지 않고 여기서 직접 처리
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
    }
}

// 🆕 기존 GetDiagnostics() 함수 강화 (기존 구현 대체)
std::map<std::string, std::string> ModbusDriver::GetDiagnostics() const {
    std::map<std::string, std::string> diagnostics;
    
    // 기존 진단 정보 유지
    diagnostics["protocol"] = "modbus";
    diagnostics["status"] = std::to_string(static_cast<int>(GetStatus()));
    diagnostics["last_error"] = GetLastError().message;
    
    // 🆕 Modbus 특화 진단 정보 추가
    diagnostics["crc_error_rate"] = std::to_string(GetCrcErrorRate());
    diagnostics["total_crc_checks"] = std::to_string(total_crc_checks_.load());
    diagnostics["modbus_health_report"] = GetModbusHealthReport();
    diagnostics["register_access_report"] = GetRegisterAccessReport();
    
    return diagnostics;
}


// =============================================================================
// Private 헬퍼 메소드들
// =============================================================================

void ModbusDriver::SetError(ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
    
    if (logger_) {
        ErrorInfo error_info;
        error_info.code = code;
        error_info.message = message;
        logger_->LogError(error_info);
    }
}

void ModbusDriver::UpdateStatistics(bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (success) {
        statistics_.successful_connections++;
        if (statistics_.avg_response_time_ms == 0.0) {
            statistics_.avg_response_time_ms = response_time_ms;
        } else {
            statistics_.avg_response_time_ms = 
                (statistics_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
        }
    } else {
        statistics_.failed_connections++;
    }
    
    statistics_.last_error_time = system_clock::now();
}

// 스텁 함수들
bool ModbusDriver::LoadDataPointsFromDB() { return true; }
std::string ModbusDriver::QueryDeviceName(const std::string& device_id) { return "Modbus_" + device_id; }
bool ModbusDriver::QueryDataPoints(const std::string& /*device_id*/) { return true; }

} // namespace Drivers
} // namespace PulseOne
// 누락된 가상 함수들 구현
ProtocolType ModbusDriver::GetProtocolType() const {
    return ProtocolType::MODBUS_TCP;
}

ErrorInfo ModbusDriver::GetLastError() const {
    return last_error_;
}
