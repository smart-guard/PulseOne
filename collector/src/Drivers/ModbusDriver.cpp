// =============================================================================
// collector/src/Drivers/ModbusDriver.cpp (진단 기능 추가 부분)
// 기존 구현에 추가할 진단 기능들
// =============================================================================

#include "Drivers/ModbusDriver.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

// 기존 생성자에 추가할 초기화
ModbusDriver::ModbusDriver()
    : modbus_ctx_(nullptr)
    , status_(DriverStatus::UNINITIALIZED)
    , is_connected_(false)
    , stop_watchdog_(false)
    , last_successful_operation_(steady_clock::now())
    // ✅ 새로 추가: 진단 관련 초기화
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false)
    , console_output_enabled_(false)
    , log_manager_(nullptr)
    , db_manager_(nullptr)
{
    packet_history_.reserve(MAX_PACKET_HISTORY);
}

// =============================================================================
// ✅ 새로 추가: 진단 기능 구현
// =============================================================================

bool ModbusDriver::EnableDiagnostics(DatabaseManager& db_manager,
                                     bool enable_packet_logging,
                                     bool enable_console_output) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    // 기존 시스템 참조 설정
    log_manager_ = &LogManager::getInstance();
    db_manager_ = &db_manager;
    
    // 설정 적용
    packet_logging_enabled_ = enable_packet_logging;
    console_output_enabled_ = enable_console_output;
    
    // 디바이스 이름 조회
    if (!config_.device_id.empty()) {
        device_name_ = QueryDeviceName(config_.device_id);
        if (device_name_.empty()) {
            device_name_ = "modbus_device_" + config_.device_id.substr(0, 8);
        }
    }
    
    // 데이터베이스에서 포인트 정보 로드
    bool success = LoadDataPointsFromDB();
    
    if (success) {
        diagnostics_enabled_ = true;
        
        log_manager_->logDriver("modbus_diagnostics",
            "Diagnostics enabled for device: " + device_name_ + 
            " (" + std::to_string(point_info_map_.size()) + " points loaded)");
        
        if (console_output_enabled_) {
            std::cout << "✅ Modbus diagnostics enabled for " << device_name_ 
                     << " (" << point_info_map_.size() << " points)" << std::endl;
        }
    } else {
        log_manager_->logError("Failed to load data points for device: " + config_.device_id);
    }
    
    return success;
}

void ModbusDriver::DisableDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    if (log_manager_) {
        log_manager_->logDriver("modbus_diagnostics",
            "Diagnostics disabled for device: " + device_name_);
    }
    
    // 히스토리 클리어
    {
        std::lock_guard<std::mutex> packet_lock(packet_log_mutex_);
        packet_history_.clear();
    }
    
    {
        std::lock_guard<std::mutex> points_lock(points_mutex_);
        point_info_map_.clear();
    }
}

void ModbusDriver::ToggleConsoleMonitoring() {
    console_output_enabled_ = !console_output_enabled_;
    
    if (log_manager_) {
        log_manager_->logDriver("modbus_diagnostics",
            "Console monitoring " + std::string(console_output_enabled_ ? "enabled" : "disabled") +
            " for device: " + device_name_);
    }
    
    if (console_output_enabled_) {
        std::cout << "🖥️  Real-time console monitoring started for " << device_name_ << std::endl;
    } else {
        std::cout << "🖥️  Real-time console monitoring stopped for " << device_name_ << std::endl;
    }
}

void ModbusDriver::TogglePacketLogging() {
    packet_logging_enabled_ = !packet_logging_enabled_;
    
    if (log_manager_) {
        log_manager_->logDriver("modbus_diagnostics",
            "Packet logging " + std::string(packet_logging_enabled_ ? "enabled" : "disabled") +
            " for device: " + device_name_);
    }
}

// =============================================================================
// ✅ 데이터베이스 연동 구현
// =============================================================================

bool ModbusDriver::LoadDataPointsFromDB() {
    if (!db_manager_ || config_.device_id.empty()) {
        return false;
    }
    
    try {
        return QueryDataPoints(config_.device_id);
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->logError("Database query failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ModbusDriver::QueryDataPoints(const std::string& device_id) {
    // PostgreSQL 쿼리 실행
    std::string query = R"(
        SELECT 
            name, description, address, data_type, unit,
            scaling_factor, scaling_offset, min_value, max_value
        FROM data_points 
        WHERE device_id = $1 AND is_enabled = true
        ORDER BY address
    )";
    
    try {
        // 실제 데이터베이스 쿼리 실행
        auto result = db_manager_->ExecuteQuery(query, {device_id});
        
        std::lock_guard<std::mutex> lock(points_mutex_);
        point_info_map_.clear();
        
        // 결과 파싱 및 캐시
        for (const auto& row : result) {
            ModbusDataPointInfo point;
            point.name = row["name"].as<std::string>();
            point.description = row["description"].as<std::string>("");
            point.unit = row["unit"].as<std::string>("");
            point.data_type = row["data_type"].as<std::string>("uint16");
            point.scaling_factor = row["scaling_factor"].as<double>(1.0);
            point.scaling_offset = row["scaling_offset"].as<double>(0.0);
            point.min_value = row["min_value"].as<double>(0.0);
            point.max_value = row["max_value"].as<double>(65535.0);
            
            int address = row["address"].as<int>();
            point_info_map_[address] = point;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->logError("Failed to query data points: " + std::string(e.what()));
        }
        return false;
    }
}

std::string ModbusDriver::QueryDeviceName(const std::string& device_id) {
    if (!db_manager_) {
        return "";
    }
    
    std::string query = "SELECT name FROM devices WHERE id = $1";
    
    try {
        auto result = db_manager_->ExecuteQuery(query, {device_id});
        if (!result.empty()) {
            return result[0]["name"].as<std::string>();
        }
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->logError("Failed to query device name: " + std::string(e.what()));
        }
    }
    
    return "";
}

// =============================================================================
// ✅ 패킷 로깅 구현
// =============================================================================

void ModbusDriver::LogModbusPacket(const std::string& direction,
                                  int slave_id, uint8_t function_code,
                                  uint16_t start_addr, uint16_t count,
                                  const std::vector<uint16_t>& values,
                                  bool success,
                                  const std::string& error_msg,
                                  double response_time_ms) {
    
    if (!diagnostics_enabled_ || !packet_logging_enabled_) {
        return;
    }
    
    // 패킷 로그 구성
    PacketLog log;
    log.timestamp = std::chrono::system_clock::now();
    log.direction = direction;
    log.slave_id = slave_id;
    log.function_code = function_code;
    log.start_address = start_addr;
    log.data_count = count;
    log.values = values;
    log.success = success;
    log.error_message = error_msg;
    log.response_time_ms = response_time_ms;
    
    // 원시 패킷 구성 (예제)
    log.raw_packet.push_back(slave_id);
    log.raw_packet.push_back(function_code);
    log.raw_packet.push_back((start_addr >> 8) & 0xFF);
    log.raw_packet.push_back(start_addr & 0xFF);
    
    if (direction == "TX") {
        // 송신 패킷
        log.raw_packet.push_back((count >> 8) & 0xFF);
        log.raw_packet.push_back(count & 0xFF);
    } else if (direction == "RX" && success && !values.empty()) {
        // 수신 패킷
        log.raw_packet.push_back(values.size() * 2); // 바이트 수
        for (uint16_t value : values) {
            log.raw_packet.push_back((value >> 8) & 0xFF);
            log.raw_packet.push_back(value & 0xFF);
        }
        
        // 엔지니어 친화적 값 디코딩
        log.decoded_values = FormatMultipleValues(start_addr, values);
    }
    
    // CRC 추가 (실제로는 libmodbus에서 자동 처리)
    log.raw_packet.push_back(0x00); // CRC Low
    log.raw_packet.push_back(0x00); // CRC High
    
    // 패킷 히스토리에 추가
    {
        std::lock_guard<std::mutex> lock(packet_log_mutex_);
        packet_history_.push_back(log);
        TrimPacketHistory();
    }
    
    // 기존 LogManager로 패킷 로깅
    if (log_manager_) {
        std::string raw_packet = FormatRawPacket(log.raw_packet);
        std::string decoded = GetFunctionName(function_code) + 
                             " (Slave " + std::to_string(slave_id) + ")";
        
        if (direction == "RX") {
            if (success) {
                decoded += " - SUCCESS (" + std::to_string(response_time_ms) + "ms)";
                if (!log.decoded_values.empty()) {
                    decoded += "\nValues: " + log.decoded_values;
                }
            } else {
                decoded += " - FAILED: " + error_msg;
            }
        }
        
        log_manager_->logPacket("modbus", device_name_, raw_packet, decoded);
    }
    
    // 콘솔 실시간 출력
    if (console_output_enabled_) {
        std::cout << FormatPacketForConsole(log) << std::endl;
    }
}

// =============================================================================
// ✅ 포매팅 함수들
// =============================================================================

std::string ModbusDriver::GetPointName(int address) const {
    std::lock_guard<std::mutex> lock(points_mutex_);
    auto it = point_info_map_.find(address);
    if (it != point_info_map_.end()) {
        return it->second.name;
    }
    return "ADDR_" + std::to_string(address);
}

std::string ModbusDriver::GetPointDescription(int address) const {
    std::lock_guard<std::mutex> lock(points_mutex_);
    auto it = point_info_map_.find(address);
    if (it != point_info_map_.end()) {
        return it->second.description;
    }
    return "Unknown point at address " + std::to_string(address);
}

std::string ModbusDriver::FormatPointValue(int address, uint16_t raw_value) const {
    std::lock_guard<std::mutex> lock(points_mutex_);
    auto it = point_info_map_.find(address);
    
    if (it != point_info_map_.end()) {
        const ModbusDataPointInfo& point = it->second;
        
        // 스케일링 적용
        double scaled_value = static_cast<double>(raw_value) * point.scaling_factor + point.scaling_offset;
        
        // BOOL 타입 처리
        if (point.data_type == "bool" || point.data_type == "BOOL") {
            return (raw_value != 0) ? "ON" : "OFF";
        }
        
        // 숫자 형식화
        std::ostringstream oss;
        if (point.scaling_factor == 1.0 && point.scaling_offset == 0.0) {
            oss << raw_value;
        } else {
            oss << std::fixed << std::setprecision(2) << scaled_value;
        }
        
        // 단위 추가
        if (!point.unit.empty()) {
            oss << " " << point.unit;
        }
        
        return oss.str();
    }
    
    // 기본값
    return std::to_string(raw_value);
}

std::string ModbusDriver::FormatMultipleValues(uint16_t start_addr, 
                                              const std::vector<uint16_t>& values) const {
    std::ostringstream oss;
    
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        
        int address = start_addr + i;
        std::string point_name = GetPointName(address);
        std::string formatted_value = FormatPointValue(address, values[i]);
        
        oss << address << " (" << point_name << "): " 
            << values[i] << " -> " << formatted_value;
    }
    
    return oss.str();
}

std::string ModbusDriver::FormatRawPacket(const std::vector<uint8_t>& packet) const {
    std::ostringstream oss;
    for (size_t i = 0; i < packet.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
            << static_cast<unsigned>(packet[i]);
    }
    return oss.str();
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
        case 0x17: return "Read/Write Multiple Registers";
        default: return "Unknown Function (" + std::to_string(function_code) + ")";
    }
}

std::string ModbusDriver::FormatPacketForConsole(const PacketLog& log) const {
    std::ostringstream oss;
    
    // 타임스탬프
    auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        log.timestamp.time_since_epoch()) % 1000;
    
    oss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    
    if (log.direction == "TX") {
        oss << "📤 TX -> Slave " << log.slave_id << ": " << GetFunctionName(log.function_code)
            << "\n  📍 Address: " << log.start_address;
        if (log.data_count > 1) {
            oss << "-" << (log.start_address + log.data_count - 1)
                << " (" << log.data_count << " registers)";
        }
        oss << "\n  📦 Raw: " << FormatRawPacket(log.raw_packet);
        
    } else { // RX
        oss << "📥 RX <- Slave " << log.slave_id << ": ";
        if (log.success) {
            oss << "✅ SUCCESS (" << log.response_time_ms << "ms)";
            oss << "\n  📦 Raw: " << FormatRawPacket(log.raw_packet);
            if (!log.decoded_values.empty()) {
                oss << "\n  📊 Values: " << log.decoded_values;
            }
        } else {
            oss << "❌ FAILED: " << log.error_message;
        }
    }
    
    return oss.str();
}

void ModbusDriver::TrimPacketHistory() {
    if (packet_history_.size() > MAX_PACKET_HISTORY) {
        packet_history_.erase(packet_history_.begin(), 
                             packet_history_.begin() + (packet_history_.size() - MAX_PACKET_HISTORY));
    }
}

// =============================================================================
// ✅ 웹 인터페이스용 JSON 생성
// =============================================================================

std::string ModbusDriver::GetDiagnosticsJSON() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"device_id\": \"" << config_.device_id << "\",\n";
    oss << "  \"device_name\": \"" << device_name_ << "\",\n";
    oss << "  \"diagnostics_enabled\": " << (diagnostics_enabled_ ? "true" : "false") << ",\n";
    oss << "  \"packet_logging_enabled\": " << (packet_logging_enabled_ ? "true" : "false") << ",\n";
    oss << "  \"console_output_enabled\": " << (console_output_enabled_ ? "true" : "false") << ",\n";
    
    {
        std::lock_guard<std::mutex> lock(points_mutex_);
        oss << "  \"data_points_count\": " << point_info_map_.size() << ",\n";
    }
    
    {
        std::lock_guard<std::mutex> lock(packet_log_mutex_);
        oss << "  \"packet_history_count\": " << packet_history_.size() << ",\n";
    }
    
    oss << "  \"connection_status\": \"" << (is_connected_ ? "connected" : "disconnected") << "\",\n";
    oss << "  \"last_error\": \"" << last_error_.message << "\"\n";
    oss << "}";
    
    return oss.str();
}

std::string ModbusDriver::GetRecentPacketsJSON(int count) const {
    std::lock_guard<std::mutex> lock(packet_log_mutex_);
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"packets\": [\n";
    
    int start_index = std::max(0, static_cast<int>(packet_history_.size()) - count);
    bool first = true;
    
    for (int i = start_index; i < static_cast<int>(packet_history_.size()); ++i) {
        if (!first) oss << ",\n";
        first = false;
        
        const PacketLog& log = packet_history_[i];
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            log.timestamp.time_since_epoch()).count();
        
        oss << "    {\n";
        oss << "      \"timestamp\": " << timestamp_ms << ",\n";
        oss << "      \"direction\": \"" << log.direction << "\",\n";
        oss << "      \"slave_id\": " << log.slave_id << ",\n";
        oss << "      \"function_code\": " << static_cast<int>(log.function_code) << ",\n";
        oss << "      \"function_name\": \"" << GetFunctionName(log.function_code) << "\",\n";
        oss << "      \"start_address\": " << log.start_address << ",\n";
        oss << "      \"data_count\": " << log.data_count << ",\n";
        oss << "      \"success\": " << (log.success ? "true" : "false") << ",\n";
        oss << "      \"response_time_ms\": " << log.response_time_ms << ",\n";
        oss << "      \"raw_packet\": \"" << FormatRawPacket(log.raw_packet) << "\",\n";
        oss << "      \"decoded_values\": \"" << log.decoded_values << "\",\n";
        oss << "      \"error_message\": \"" << log.error_message << "\"\n";
        oss << "    }";
    }
    
    oss << "\n  ],\n";
    oss << "  \"total_count\": " << packet_history_.size() << "\n";
    oss << "}";
    
    return oss.str();
}

// =============================================================================
// ✅ 기존 ReadValues/WriteValue 메소드에 진단 기능 추가
// =============================================================================

// 기존 ReadValues 메소드 수정 (진단 기능 추가)
bool ModbusDriver::ReadValues(const std::vector<DataPoint>& points,
                              std::vector<TimestampedValue>& values) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    values.clear();
    values.reserve(points.size());
    
    auto operation_start = steady_clock::now();
    bool overall_success = true;
    
    for (const auto& point : points) {
        TimestampedValue tvalue;
        tvalue.timestamp = system_clock::now();
        
        try {
            // 슬레이브 ID 설정
            int slave_id = 1;
            auto slave_it = point.properties.find("slave_id");
            if (slave_it != point.properties.end()) {
                slave_id = std::stoi(slave_it->second);
            }
            
            modbus_set_slave(modbus_ctx_, slave_id);
            
            // ✅ 진단: 송신 패킷 로깅
            if (diagnostics_enabled_) {
                LogModbusPacket("TX", slave_id, 0x03, point.address, 1);
            }
            
            // 데이터 타입에 따른 읽기 실행
            ModbusFunction function = GetReadFunction(point);
            std::vector<uint16_t> raw_values;
            
            auto read_start = steady_clock::now();
            bool read_success = ExecuteRead(slave_id, function, point.address, 1, raw_values);
            auto read_end = steady_clock::now();
            auto response_time = duration_cast<milliseconds>(read_end - read_start).count();
            
            // ✅ 진단: 수신 패킷 로깅
            if (diagnostics_enabled_) {
                if (read_success && !raw_values.empty()) {
                    LogModbusPacket("RX", slave_id, 0x03, point.address, 1, 
                                   raw_values, true, "", response_time);
                } else {
                    LogModbusPacket("RX", slave_id, 0x03, point.address, 1, 
                                   {}, false, GetLastError().message, response_time);
                }
            }
            
            if (read_success && !raw_values.empty()) {
                tvalue.value = ConvertModbusValue(point, raw_values[0]);
                tvalue.quality = DataQuality::GOOD;
            } else {
                tvalue.value = DataValue(0);
                tvalue.quality = DataQuality::BAD;
                overall_success = false;
            }
            
        } catch (const std::exception& e) {
            tvalue.value = DataValue(0);
            tvalue.quality = DataQuality::BAD;
            overall_success = false;
            
            if (logger_) {
                logger_->Error("Exception reading point " + point.name + ": " + e.what(),
                              DriverLogCategory::ERROR_HANDLING);
            }
        }
        
        values.push_back(tvalue);
    }
    
    auto operation_end = steady_clock::now();
    auto duration_ms = duration_cast<milliseconds>(operation_end - operation_start).count();
    
    UpdateStatistics(overall_success, static_cast<double>(duration_ms));
    
    if (overall_success) {
        last_successful_operation_ = steady_clock::now();
    }
    
    return overall_success;
}

// 기존 WriteValue 메소드 수정 (진단 기능 추가)
bool ModbusDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "Not connected to device");
        return false;
    }
    
    auto operation_start = steady_clock::now();
    
    try {
        // 슬레이브 ID 설정
        int slave_id = 1;
        auto slave_it = point.properties.find("slave_id");
        if (slave_it != point.properties.end()) {
            slave_id = std::stoi(slave_it->second);
        }
        
        modbus_set_slave(modbus_ctx_, slave_id);
        
        // 값 변환
        uint16_t modbus_value = ConvertToModbusValue(point, value);
        
        // ✅ 진단: 송신 패킷 로깅
        if (diagnostics_enabled_) {
            LogModbusPacket("TX", slave_id, 0x06, point.address, 1, {modbus_value});
        }
        
        // 쓰기 실행
        ModbusFunction function = GetWriteFunction(point);
        auto write_start = steady_clock::now();
        bool success = ExecuteWrite(slave_id, function, point.address, modbus_value);
        auto write_end = steady_clock::now();
        auto response_time = duration_cast<milliseconds>(write_end - write_start).count();
        
        // ✅ 진단: 수신 패킷 로깅
        if (diagnostics_enabled_) {
            if (success) {
                LogModbusPacket("RX", slave_id, 0x06, point.address, 1, 
                               {modbus_value}, true, "", response_time);
            } else {
                LogModbusPacket("RX", slave_id, 0x06, point.address, 1, 
                               {}, false, GetLastError().message, response_time);
            }
        }
        
        auto operation_end = steady_clock::now();
        auto duration_ms = duration_cast<milliseconds>(operation_end - operation_start).count();
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_writes++;
            if (success) {
                statistics_.successful_writes++;
                last_successful_operation_ = steady_clock::now();
            } else {
                statistics_.failed_writes++;
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::DATA_FORMAT_ERROR, "Write value conversion error: " + std::string(e.what()));
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_writes++;
            statistics_.failed_writes++;
        }
        
        return false;
    }
}