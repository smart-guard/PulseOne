// =============================================================================
// collector/src/Drivers/Modbus/ModbusDiagnostics.cpp
// Modbus 진단 및 모니터링 기능 구현
// =============================================================================

#include "Drivers/Modbus/ModbusDiagnostics.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자/소멸자
// =============================================================================

ModbusDiagnostics::ModbusDiagnostics(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver)
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false)
    , console_output_enabled_(false)
    , total_requests_(0)
    , successful_requests_(0)
    , failed_requests_(0)
    , total_crc_checks_(0)
    , crc_errors_(0)
{
    // 배열 초기화
    for (auto& count : exception_code_counts_) {
        count.store(0);
    }
    
    for (auto& count : response_time_histogram_) {
        count.store(0);
    }
    
    diagnostics_start_time_ = std::chrono::system_clock::now();
    
    InitializeDiagnostics();
}

ModbusDiagnostics::~ModbusDiagnostics() {
    DisableDiagnostics();
    CleanupDiagnostics();
}

// =============================================================================
// 진단 기능 활성화/비활성화
// =============================================================================

bool ModbusDiagnostics::EnableDiagnostics(bool packet_logging, bool console_output) {
    if (!parent_driver_) {
        return false;
    }
    
    diagnostics_enabled_.store(true);
    packet_logging_enabled_.store(packet_logging);
    console_output_enabled_.store(console_output);
    
    diagnostics_start_time_ = std::chrono::system_clock::now();
    
    if (console_output) {
        PrintDiagnosticInfo("Modbus diagnostics enabled");
    }
    
    return true;
}

void ModbusDiagnostics::DisableDiagnostics() {
    diagnostics_enabled_.store(false);
    packet_logging_enabled_.store(false);
    console_output_enabled_.store(false);
    
    if (console_output_enabled_.load()) {
        PrintDiagnosticInfo("Modbus diagnostics disabled");
    }
}

bool ModbusDiagnostics::IsDiagnosticsEnabled() const {
    return diagnostics_enabled_.load();
}

void ModbusDiagnostics::EnablePacketLogging(bool enable) {
    packet_logging_enabled_.store(enable);
}

void ModbusDiagnostics::EnableConsoleOutput(bool enable) {
    console_output_enabled_.store(enable);
}

bool ModbusDiagnostics::IsPacketLoggingEnabled() const {
    return packet_logging_enabled_.load();
}

bool ModbusDiagnostics::IsConsoleOutputEnabled() const {
    return console_output_enabled_.load();
}

// =============================================================================
// 진단 정보 수집
// =============================================================================

void ModbusDiagnostics::RecordExceptionCode(uint8_t exception_code) {
    if (!diagnostics_enabled_.load()) return;
    
    if (exception_code < exception_code_counts_.size()) {
        exception_code_counts_[exception_code].fetch_add(1);
    }
    
    if (console_output_enabled_.load()) {
        PrintDiagnosticInfo("Modbus exception code: " + std::to_string(exception_code));
    }
}

void ModbusDiagnostics::RecordCrcCheck(bool crc_valid) {
    if (!diagnostics_enabled_.load()) return;
    
    total_crc_checks_.fetch_add(1);
    
    if (!crc_valid) {
        crc_errors_.fetch_add(1);
        
        if (console_output_enabled_.load()) {
            PrintDiagnosticInfo("CRC error detected");
        }
    }
}

void ModbusDiagnostics::RecordResponseTime(int slave_id, uint32_t response_time_ms) {
    if (!diagnostics_enabled_.load()) return;
    
    // 히스토그램 업데이트
    size_t histogram_index = GetResponseTimeHistogramIndex(response_time_ms);
    if (histogram_index < response_time_histogram_.size()) {
        response_time_histogram_[histogram_index].fetch_add(1);
    }
    
    // Slave 건강 상태 업데이트
    std::lock_guard<std::mutex> lock(slave_health_mutex_);
    auto& health_info = slave_health_map_[slave_id];
    health_info.slave_id = slave_id;
    
    // 평균 응답 시간 업데이트 (간단한 이동 평균)
    uint32_t current_avg = health_info.average_response_time_ms.load();
    uint32_t new_avg = (current_avg + response_time_ms) / 2;
    health_info.average_response_time_ms.store(new_avg);
}

void ModbusDiagnostics::RecordRegisterAccess(uint16_t address, bool is_read, bool is_write) {
    if (!diagnostics_enabled_.load()) return;
    
    std::lock_guard<std::mutex> lock(register_access_mutex_);
    
    // 최대 1000개 레지스터만 추적 (메모리 제한)
    if (register_access_map_.size() >= 1000 && 
        register_access_map_.find(address) == register_access_map_.end()) {
        return;
    }
    
    auto& access_pattern = register_access_map_[address];
    access_pattern.address = address;
    access_pattern.last_access = std::chrono::system_clock::now();
    
    if (is_read) {
        access_pattern.read_count.fetch_add(1);
    }
    
    if (is_write) {
        access_pattern.write_count.fetch_add(1);
    }
}

void ModbusDiagnostics::RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms) {
    if (!diagnostics_enabled_.load()) return;
    
    total_requests_.fetch_add(1);
    
    if (success) {
        successful_requests_.fetch_add(1);
    } else {
        failed_requests_.fetch_add(1);
    }
    
    // Slave 건강 상태 업데이트
    std::lock_guard<std::mutex> lock(slave_health_mutex_);
    auto& health_info = slave_health_map_[slave_id];
    health_info.slave_id = slave_id;
    health_info.total_requests.fetch_add(1);
    
    auto now = std::chrono::system_clock::now();
    
    if (success) {
        health_info.successful_requests.fetch_add(1);
        health_info.last_success = now;
    } else {
        health_info.failed_requests.fetch_add(1);
        health_info.last_failure = now;
    }
    
    // 응답 시간도 기록
    RecordResponseTime(slave_id, response_time_ms);
}

void ModbusDiagnostics::LogPacket(int slave_id, uint8_t function_code, uint16_t start_addr, 
                                  uint16_t count, bool is_request, bool success, 
                                  uint32_t response_time_ms, const std::string& error) {
    if (!packet_logging_enabled_.load()) return;
    
    ModbusPacketLog log;
    log.timestamp = std::chrono::system_clock::now();
    log.slave_id = slave_id;
    log.function_code = function_code;
    log.start_address = start_addr;
    log.count = count;
    log.is_request = is_request;
    log.success = success;
    log.response_time_ms = response_time_ms;
    log.error_message = error;
    
    std::lock_guard<std::mutex> lock(packet_log_mutex_);
    
    packet_log_queue_.push_back(log);
    
    // 순환 버퍼 관리 (최대 크기 유지)
    if (packet_log_queue_.size() > MAX_PACKET_LOGS) {
        packet_log_queue_.pop_front();
    }
    
    if (console_output_enabled_.load()) {
        std::ostringstream oss;
        oss << "Packet [" << (is_request ? "REQ" : "RSP") << "] "
            << "Slave:" << slave_id << ", FC:" << static_cast<int>(function_code)
            << ", Addr:" << start_addr << ", Count:" << count
            << ", Success:" << (success ? "Y" : "N");
        if (!error.empty()) {
            oss << ", Error:" << error;
        }
        PrintDiagnosticInfo(oss.str());
    }
}

// =============================================================================
// 진단 정보 조회
// =============================================================================

std::string ModbusDiagnostics::GetDiagnosticsJSON() const {
    if (!diagnostics_enabled_.load()) {
        return R"({"diagnostics":"disabled"})";
    }
    
    return CreateDiagnosticsJSON();
}

std::map<std::string, std::string> ModbusDiagnostics::GetDiagnostics() const {
    std::map<std::string, std::string> diagnostics;
    
    if (!diagnostics_enabled_.load()) {
        diagnostics["status"] = "disabled";
        return diagnostics;
    }
    
    diagnostics["status"] = "enabled";
    diagnostics["total_requests"] = std::to_string(total_requests_.load());
    diagnostics["successful_requests"] = std::to_string(successful_requests_.load());
    diagnostics["failed_requests"] = std::to_string(failed_requests_.load());
    diagnostics["crc_error_rate"] = std::to_string(GetCrcErrorRate());
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - diagnostics_start_time_);
    diagnostics["uptime_seconds"] = std::to_string(uptime.count());
    
    return diagnostics;
}

std::vector<uint64_t> ModbusDiagnostics::GetResponseTimeHistogram() const {
    std::vector<uint64_t> histogram;
    histogram.reserve(response_time_histogram_.size());
    
    for (const auto& bucket : response_time_histogram_) {
        histogram.push_back(bucket.load());
    }
    
    return histogram;
}

std::map<uint8_t, uint64_t> ModbusDiagnostics::GetExceptionCodeStats() const {
    std::map<uint8_t, uint64_t> stats;
    
    for (size_t i = 0; i < exception_code_counts_.size(); ++i) {
        uint64_t count = exception_code_counts_[i].load();
        if (count > 0) {
            stats[static_cast<uint8_t>(i)] = count;
        }
    }
    
    return stats;
}

double ModbusDiagnostics::GetCrcErrorRate() const {
    uint64_t total_checks = total_crc_checks_.load();
    if (total_checks == 0) {
        return 0.0;
    }
    
    uint64_t errors = crc_errors_.load();
    return static_cast<double>(errors) / static_cast<double>(total_checks) * 100.0;
}

std::map<int, SlaveHealthInfo> ModbusDiagnostics::GetSlaveHealthStatus() const {
    std::lock_guard<std::mutex> lock(slave_health_mutex_);
    
    // unordered_map을 map으로 변환
    std::map<int, SlaveHealthInfo> result;
    for (const auto& pair : slave_health_map_) {
        result[pair.first] = pair.second;
    }
    return result;
}

std::map<uint16_t, RegisterAccessPattern> ModbusDiagnostics::GetRegisterAccessPatterns() const {
    std::lock_guard<std::mutex> lock(register_access_mutex_);
    
    // unordered_map을 map으로 변환
    std::map<uint16_t, RegisterAccessPattern> result;
    for (const auto& pair : register_access_map_) {
        result[pair.first] = pair.second;
    }
    return result;
}

std::string ModbusDiagnostics::GetRecentPacketsJSON(int count) const {
    auto packets = GetRecentPackets(count);
    return CreatePacketLogJSON(packets);
}

std::vector<ModbusPacketLog> ModbusDiagnostics::GetRecentPackets(int count) const {
    std::lock_guard<std::mutex> lock(packet_log_mutex_);
    
    std::vector<ModbusPacketLog> recent_packets;
    
    int start_index = std::max(0, static_cast<int>(packet_log_queue_.size()) - count);
    
    for (int i = start_index; i < static_cast<int>(packet_log_queue_.size()); ++i) {
        recent_packets.push_back(packet_log_queue_[i]);
    }
    
    return recent_packets;
}

void ModbusDiagnostics::ResetDiagnostics() {
    total_requests_.store(0);
    successful_requests_.store(0);
    failed_requests_.store(0);
    total_crc_checks_.store(0);
    crc_errors_.store(0);
    
    for (auto& count : exception_code_counts_) {
        count.store(0);
    }
    
    for (auto& count : response_time_histogram_) {
        count.store(0);
    }
    
    {
        std::lock_guard<std::mutex> lock(slave_health_mutex_);
        slave_health_map_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(register_access_mutex_);
        register_access_map_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(packet_log_mutex_);
        packet_log_queue_.clear();
    }
    
    diagnostics_start_time_ = std::chrono::system_clock::now();
    
    if (console_output_enabled_.load()) {
        PrintDiagnosticInfo("Diagnostics reset");
    }
}

void ModbusDiagnostics::TogglePacketLogging() {
    bool current = packet_logging_enabled_.load();
    packet_logging_enabled_.store(!current);
    
    if (console_output_enabled_.load()) {
        PrintDiagnosticInfo("Packet logging " + std::string(!current ? "enabled" : "disabled"));
    }
}

void ModbusDiagnostics::ToggleConsoleMonitoring() {
    bool current = console_output_enabled_.load();
    console_output_enabled_.store(!current);
    
    PrintDiagnosticInfo("Console monitoring " + std::string(!current ? "enabled" : "disabled"));
}

// =============================================================================
// 내부 메서드
// =============================================================================

void ModbusDiagnostics::InitializeDiagnostics() {
    // 초기화 로직 (향후 확장 가능)
}

void ModbusDiagnostics::CleanupDiagnostics() {
    // 정리 로직 (향후 확장 가능)
}

void ModbusDiagnostics::UpdateStatistics() {
    // 통계 업데이트 로직 (향후 확장 가능)
}

size_t ModbusDiagnostics::GetResponseTimeHistogramIndex(uint32_t response_time_ms) const {
    if (response_time_ms <= 10) return 0;          // 0-10ms
    else if (response_time_ms <= 50) return 1;     // 10-50ms
    else if (response_time_ms <= 100) return 2;    // 50-100ms
    else if (response_time_ms <= 500) return 3;    // 100-500ms
    else return 4;                                  // 500ms+
}

std::string ModbusDiagnostics::CreateDiagnosticsJSON() const {
    std::ostringstream json;
    
    json << "{"
         << "\"status\":\"enabled\","
         << "\"uptime_seconds\":" << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - diagnostics_start_time_).count() << ","
         << "\"total_requests\":" << total_requests_.load() << ","
         << "\"successful_requests\":" << successful_requests_.load() << ","
         << "\"failed_requests\":" << failed_requests_.load() << ","
         << "\"success_rate\":" << std::fixed << std::setprecision(2);
    
    uint64_t total = total_requests_.load();
    if (total > 0) {
        double success_rate = static_cast<double>(successful_requests_.load()) / static_cast<double>(total) * 100.0;
        json << success_rate;
    } else {
        json << "0.00";
    }
    
    json << ",\"crc_checks\":" << total_crc_checks_.load()
         << ",\"crc_errors\":" << crc_errors_.load()
         << ",\"crc_error_rate\":" << std::fixed << std::setprecision(2) << GetCrcErrorRate()
         << ",\"response_time_histogram\":[";
    
    for (size_t i = 0; i < response_time_histogram_.size(); ++i) {
        if (i > 0) json << ",";
        json << response_time_histogram_[i].load();
    }
    
    json << "],\"exception_codes\":{";
    
    bool first_exception = true;
    for (size_t i = 1; i < exception_code_counts_.size(); ++i) {  // Skip index 0 (no exception)
        uint64_t count = exception_code_counts_[i].load();
        if (count > 0) {
            if (!first_exception) json << ",";
            json << "\"" << i << "\":" << count;
            first_exception = false;
        }
    }
    
    json << "},\"slave_count\":" << slave_health_map_.size()
         << ",\"register_patterns\":" << register_access_map_.size()
         << ",\"packet_logs\":" << packet_log_queue_.size()
         << "}";
    
    return json.str();
}

std::string ModbusDiagnostics::CreatePacketLogJSON(const std::vector<ModbusPacketLog>& logs) const {
    std::ostringstream json;
    
    json << "{\"packets\":[";
    
    for (size_t i = 0; i < logs.size(); ++i) {
        if (i > 0) json << ",";
        
        const auto& log = logs[i];
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            log.timestamp.time_since_epoch()).count();
        
        json << "{"
             << "\"timestamp\":" << timestamp_ms << ","
             << "\"slave_id\":" << log.slave_id << ","
             << "\"function_code\":" << static_cast<int>(log.function_code) << ","
             << "\"start_address\":" << log.start_address << ","
             << "\"count\":" << log.count << ","
             << "\"is_request\":" << (log.is_request ? "true" : "false") << ","
             << "\"success\":" << (log.success ? "true" : "false") << ","
             << "\"response_time_ms\":" << log.response_time_ms;
        
        if (!log.error_message.empty()) {
            json << ",\"error\":\"" << log.error_message << "\"";
        }
        
        json << "}";
    }
    
    json << "],\"count\":" << logs.size() << "}";
    
    return json.str();
}

void ModbusDiagnostics::PrintDiagnosticInfo(const std::string& message) const {
    if (console_output_enabled_.load()) {
        // LogManager를 통한 로깅 (실제 환경에서는 적절한 로거 사용)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << "[MODBUS-DIAG " << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
            << "] " << message;
        
        // 실제 로거가 있다면 사용, 없으면 std::cout으로 대체
        std::cout << oss.str() << std::endl;
    }
}

} // namespace Drivers
} // namespace PulseOne