/**
 * @file UnifiedModbusDriver.h
 * @brief UnifiedDriverBase 기반 Modbus 드라이버 (단순화 버전)
 * @version 1.3.0
 * @date 2024
 */

#ifndef PULSEONE_UNIFIED_MODBUS_DRIVER_H
#define PULSEONE_UNIFIED_MODBUS_DRIVER_H

// 🎯 의존성 파일들
#include "TestUnifiedDriverBase.h"
#include "Common/NewTypes.h"
#include "Common/CompatibilityBridge.h"

// 🔧 libmodbus 헤더
#ifdef HAVE_LIBMODBUS
#include <modbus/modbus.h>
#include <modbus/modbus-tcp.h>
#include <modbus/modbus-rtu.h>
#else
#warning "libmodbus not found - using simulation mode"
typedef void* modbus_t;
#endif

// 🔧 표준 라이브러리
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <variant>
#include <sstream>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 🎯 Modbus 특화 열거형 및 상수
// =============================================================================

enum class ModbusRegisterType {
    HOLDING_REGISTER = 0,
    INPUT_REGISTER = 1,
    COIL = 2,
    DISCRETE_INPUT = 3
};

enum class ModbusFunctionCode {
    READ_COILS = 1,
    READ_DISCRETE_INPUTS = 2, 
    READ_HOLDING_REGISTERS = 3,
    READ_INPUT_REGISTERS = 4,
    WRITE_SINGLE_COIL = 5,
    WRITE_SINGLE_REGISTER = 6,
    WRITE_MULTIPLE_COILS = 15,
    WRITE_MULTIPLE_REGISTERS = 16
};

enum class ModbusConnectionState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    RECONNECTING = 3,
    ERROR = 4
};

// =============================================================================
// 🎯 UnifiedDriverBase 기반 Modbus 드라이버 (단순화)
// =============================================================================

class UnifiedModbusDriver : public Test::UnifiedDriverBase {
private:
    // =================================================================
    // libmodbus 컨텍스트 및 연결 관리
    // =================================================================
    modbus_t* modbus_context_;
    std::atomic<ModbusConnectionState> connection_state_;
    mutable std::mutex connection_mutex_;
    
    // =================================================================
    // Modbus 특화 통계 (개별 관리)
    // =================================================================
    std::atomic<uint64_t> modbus_register_reads_;
    std::atomic<uint64_t> modbus_coil_reads_;
    std::atomic<uint64_t> modbus_register_writes_;
    std::atomic<uint64_t> modbus_coil_writes_;
    std::atomic<uint64_t> modbus_exceptions_;
    std::atomic<uint64_t> modbus_timeouts_;
    std::atomic<uint64_t> modbus_connections_;
    std::atomic<uint64_t> modbus_connection_failures_;
    
    // 예외 코드별 카운트
    std::unordered_map<uint8_t, uint64_t> exception_codes_;
    mutable std::mutex exception_mutex_;
    
    // =================================================================
    // 현재 Modbus 설정
    // =================================================================
    std::string current_ip_;
    int current_port_;
    int current_slave_id_;
    int response_timeout_ms_;
    int byte_timeout_ms_;
    
    // =================================================================
    // 고급 기능 플래그
    // =================================================================
    bool diagnostics_enabled_;
    bool auto_reconnect_enabled_;
    int max_reconnect_attempts_;
    int reconnect_delay_ms_;
    
public:
    // =================================================================
    // 생성자 및 소멸자
    // =================================================================
    
    UnifiedModbusDriver() 
        : Test::UnifiedDriverBase()
        , modbus_context_(nullptr)
        , connection_state_(ModbusConnectionState::DISCONNECTED)
        , modbus_register_reads_(0)
        , modbus_coil_reads_(0)
        , modbus_register_writes_(0)
        , modbus_coil_writes_(0)
        , modbus_exceptions_(0)
        , modbus_timeouts_(0)
        , modbus_connections_(0)
        , modbus_connection_failures_(0)
        , current_port_(502)
        , current_slave_id_(1)
        , response_timeout_ms_(1000)
        , byte_timeout_ms_(500)
        , diagnostics_enabled_(false)
        , auto_reconnect_enabled_(true)
        , max_reconnect_attempts_(3)
        , reconnect_delay_ms_(1000)
    {
        // 기본 Modbus TCP 디바이스 설정
        device_info_.id = New::GenerateUUID();
        device_info_.name = "Unified Modbus Driver";
        device_info_.protocol = New::ProtocolType::MODBUS_TCP;
        device_info_.endpoint = "127.0.0.1:502";
        device_info_.modbus_config.slave_id = 1;
        device_info_.modbus_config.connection_timeout_ms = 3000;
    }
    
    explicit UnifiedModbusDriver(const New::UnifiedDeviceInfo& device_info) 
        : Test::UnifiedDriverBase(device_info)
        , modbus_context_(nullptr)
        , connection_state_(ModbusConnectionState::DISCONNECTED)
        , modbus_register_reads_(0)
        , modbus_coil_reads_(0)
        , modbus_register_writes_(0)
        , modbus_coil_writes_(0)
        , modbus_exceptions_(0)
        , modbus_timeouts_(0)
        , modbus_connections_(0)
        , modbus_connection_failures_(0)
        , diagnostics_enabled_(false)
        , auto_reconnect_enabled_(true)
        , max_reconnect_attempts_(3)
        , reconnect_delay_ms_(1000)
    {
        ParseEndpoint();
    }
    
    virtual ~UnifiedModbusDriver() {
        Stop();
        CleanupModbus();
    }
    
    // 복사/이동 방지
    UnifiedModbusDriver(const UnifiedModbusDriver&) = delete;
    UnifiedModbusDriver& operator=(const UnifiedModbusDriver&) = delete;
// =================================================================
    // 🎯 확장 Modbus 메서드들 (cpp 파일에 구현)
    // =================================================================
    
    /**
     * @brief 다중 Holding Register 쓰기
     */
    bool WriteHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                               const std::vector<uint16_t>& values);
    
    /**
     * @brief Coil 쓰기
     */
    bool WriteCoil(int slave_id, uint16_t addr, bool value);
    
    /**
     * @brief 다중 Coil 쓰기
     */
    bool WriteCoils(int slave_id, uint16_t start_addr, uint16_t count, 
                    const std::vector<uint8_t>& values);
    
    /**
     * @brief Discrete Input 읽기
     */
    bool ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, 
                            std::vector<uint8_t>& values);
    
    /**
     * @brief 상세한 진단 정보 반환
     */
    std::string GetDetailedDiagnostics() const;
    
    /**
     * @brief 통계 초기화
     */
    void ResetModbusStatistics();
    
    /**
     * @brief 연결 테스트
     */
    bool TestConnection();    
protected:
    // =================================================================
    // 🎯 UnifiedDriverBase 인터페이스 구현
    // =================================================================
    
    bool InitializeProtocol() override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        CleanupModbus();
        
        if (!ParseEndpoint()) {
            return false;
        }
        
        // Modbus 컨텍스트 생성
        if (device_info_.protocol == New::ProtocolType::MODBUS_TCP) {
            modbus_context_ = CreateTcpContext();
        } else if (device_info_.protocol == New::ProtocolType::MODBUS_RTU) {
            modbus_context_ = CreateRtuContext();
        } else {
            return false;
        }
        
        if (!modbus_context_) {
            return false;
        }
        
        SetTimeouts();
        SetSlaveId(current_slave_id_);
        
        return true;
    }
    
    bool ConnectProtocol() override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (!modbus_context_) {
            return false;
        }
        
        if (connection_state_.load() == ModbusConnectionState::CONNECTED) {
            return true;
        }
        
        connection_state_.store(ModbusConnectionState::CONNECTING);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = false;
#ifdef HAVE_LIBMODBUS
        int result = modbus_connect(modbus_context_);
        success = (result != -1);
#else
        // 시뮬레이션 모드
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        success = true;
#endif
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (success) {
            connection_state_.store(ModbusConnectionState::CONNECTED);
            modbus_connections_.fetch_add(1);
        } else {
            HandleConnectionError();
            modbus_connection_failures_.fetch_add(1);
        }
        
        return success;
    }
    
    void DisconnectProtocol() override {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        if (modbus_context_ && connection_state_.load() == ModbusConnectionState::CONNECTED) {
#ifdef HAVE_LIBMODBUS
            modbus_close(modbus_context_);
#endif
            connection_state_.store(ModbusConnectionState::DISCONNECTED);
        }
    }
    
    std::vector<New::UnifiedDataPoint> ReadDataProtocol() override {
        std::vector<New::UnifiedDataPoint> results;
        
        if (!EnsureConnection()) {
            return results;
        }
        
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        for (const auto& point : data_points_) {
            New::UnifiedDataPoint result_point = point;
            
            if (ReadSingleDataPoint(point, result_point)) {
                result_point.quality = New::DataQuality::GOOD;
                result_point.last_update = New::GetCurrentTime();
                results.push_back(result_point);
            } else {
                result_point.quality = New::DataQuality::BAD;
                results.push_back(result_point);
            }
        }
        
        return results;
    }
    
    bool WriteValueProtocol(const std::string& data_point_id, 
                           const std::variant<bool, int32_t, uint32_t, float, double, std::string>& value) override {
        
        if (!EnsureConnection()) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = std::find_if(data_points_.begin(), data_points_.end(),
            [&data_point_id](const New::UnifiedDataPoint& point) {
                return point.id == data_point_id;
            });
        
        if (it == data_points_.end()) {
            return false;
        }
        
        return WriteSingleDataPoint(*it, value);
    }
    
    void CleanupProtocol() override {
        CleanupModbus();
    }
    
public:
    // =================================================================
    // 🎯 Modbus 특화 공개 메서드들
    // =================================================================
    
    bool ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                              std::vector<uint16_t>& values) {
        
        if (!EnsureConnection()) {
            return false;
        }
        
        SetSlaveId(slave_id);
        values.resize(count);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = false;
#ifdef HAVE_LIBMODBUS
        int result = modbus_read_registers(modbus_context_, start_addr, count, values.data());
        success = (result != -1);
#else
        // 시뮬레이션 모드
        for (uint16_t i = 0; i < count; i++) {
            values[i] = (start_addr + i) % 65536;
        }
        success = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!success) {
            HandleModbusError();
            return false;
        }
        
        modbus_register_reads_.fetch_add(1);
        
        return true;
    }
    
    bool ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                            std::vector<uint16_t>& values) {
        
        if (!EnsureConnection()) {
            return false;
        }
        
        SetSlaveId(slave_id);
        values.resize(count);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = false;
#ifdef HAVE_LIBMODBUS
        int result = modbus_read_input_registers(modbus_context_, start_addr, count, values.data());
        success = (result != -1);
#else
        // 시뮬레이션 모드
        for (uint16_t i = 0; i < count; i++) {
            values[i] = (start_addr + i + 1000) % 65536;
        }
        success = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!success) {
            HandleModbusError();
            return false;
        }
        
        modbus_register_reads_.fetch_add(1);
        
        return true;
    }
    
    bool ReadCoils(int slave_id, uint16_t start_addr, uint16_t count, 
                   std::vector<uint8_t>& values) {
        
        if (!EnsureConnection()) {
            return false;
        }
        
        SetSlaveId(slave_id);
        values.resize(count);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = false;
#ifdef HAVE_LIBMODBUS
        int result = modbus_read_bits(modbus_context_, start_addr, count, values.data());
        success = (result != -1);
#else
        // 시뮬레이션 모드
        for (uint16_t i = 0; i < count; i++) {
            values[i] = (start_addr + i) % 2;
        }
        success = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!success) {
            HandleModbusError();
            return false;
        }
        
        modbus_coil_reads_.fetch_add(1);
        
        return true;
    }
    
    bool WriteHoldingRegister(int slave_id, uint16_t addr, uint16_t value) {
        
        if (!EnsureConnection()) {
            return false;
        }
        
        SetSlaveId(slave_id);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = false;
#ifdef HAVE_LIBMODBUS
        int result = modbus_write_register(modbus_context_, addr, value);
        success = (result != -1);
#else
        // 시뮬레이션 모드
        success = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!success) {
            HandleModbusError();
            return false;
        }
        
        modbus_register_writes_.fetch_add(1);
        
        return true;
    }
    
    // =================================================================
    // 🎯 상태 및 통계 조회
    // =================================================================
    
    bool IsModbusConnected() const {
        return connection_state_.load() == ModbusConnectionState::CONNECTED;
    }
    
    std::string GetConnectionStateString() const {
        switch (connection_state_.load()) {
            case ModbusConnectionState::DISCONNECTED: return "DISCONNECTED";
            case ModbusConnectionState::CONNECTING: return "CONNECTING";
            case ModbusConnectionState::CONNECTED: return "CONNECTED";
            case ModbusConnectionState::RECONNECTING: return "RECONNECTING";
            case ModbusConnectionState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::unordered_map<uint8_t, uint64_t> GetExceptionCodes() const {
        std::lock_guard<std::mutex> lock(exception_mutex_);
        return exception_codes_;
    }
    
    uint64_t GetTotalExceptions() const {
        return modbus_exceptions_.load();
    }
    
    uint64_t GetTimeoutCount() const {
        return modbus_timeouts_.load();
    }
    
    uint64_t GetModbusConnectionCount() const {
        return modbus_connections_.load();
    }
    
    uint64_t GetModbusConnectionFailures() const {
        return modbus_connection_failures_.load();
    }
    
    std::string GetModbusStatisticsJSON() const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"connection_state\": \"" << GetConnectionStateString() << "\",\n";
        ss << "  \"register_reads\": " << modbus_register_reads_.load() << ",\n";
        ss << "  \"coil_reads\": " << modbus_coil_reads_.load() << ",\n";
        ss << "  \"register_writes\": " << modbus_register_writes_.load() << ",\n";
        ss << "  \"coil_writes\": " << modbus_coil_writes_.load() << ",\n";
        ss << "  \"exceptions\": " << modbus_exceptions_.load() << ",\n";
        ss << "  \"timeouts\": " << modbus_timeouts_.load() << ",\n";
        ss << "  \"connections\": " << modbus_connections_.load() << ",\n";
        ss << "  \"connection_failures\": " << modbus_connection_failures_.load() << "\n";
        ss << "}";
        return ss.str();
    }
    
    // =================================================================
    // 🎯 기존 호환성 메서드들
    // =================================================================
    
    void SetLegacyDeviceInfo(const std::string& device_id, const std::string& ip_address, 
                            int port, int slave_id, bool enabled = true) {
        device_info_.id = device_id;
        device_info_.name = "Modbus Device " + device_id;
        device_info_.endpoint = ip_address + ":" + std::to_string(port);
        device_info_.enabled = enabled;
        device_info_.protocol = New::ProtocolType::MODBUS_TCP;
        
        device_info_.modbus_config.slave_id = slave_id;
        device_info_.modbus_config.connection_timeout_ms = 3000;
        
        ParseEndpoint();
    }
    
    void AddLegacyDataPoint(const std::string& point_id, const std::string& name,
                           uint16_t modbus_address, int function_code, 
                           const std::string& data_type = "uint16") {
        New::UnifiedDataPoint unified_point;
        
        unified_point.id = point_id;
        unified_point.device_id = device_info_.id;
        unified_point.name = name;
        unified_point.description = "Legacy DataPoint: " + name;
        
        unified_point.modbus_address.register_address = modbus_address;
        unified_point.modbus_address.function_code = function_code;
        unified_point.modbus_address.count = 1;
        
        unified_point.data_type = data_type;
        unified_point.scale_factor = 1.0;
        unified_point.offset = 0.0;
        unified_point.unit = "";
        unified_point.read_only = (function_code == 3 || function_code == 4);
        
        // 베이스 클래스의 멤버에 직접 추가
        std::lock_guard<std::mutex> lock(data_mutex_);
        data_points_.push_back(unified_point);
    }
    
private:
    // =================================================================
    // 🎯 내부 헬퍼 메서드들
    // =================================================================
    
    bool ParseEndpoint() {
        auto colon_pos = device_info_.endpoint.find(':');
        if (colon_pos != std::string::npos) {
            current_ip_ = device_info_.endpoint.substr(0, colon_pos);
            current_port_ = std::stoi(device_info_.endpoint.substr(colon_pos + 1));
        } else {
            current_ip_ = device_info_.endpoint;
            current_port_ = 502;
        }
        
        current_slave_id_ = device_info_.modbus_config.slave_id;
        response_timeout_ms_ = device_info_.timeout_ms;
        
        return !current_ip_.empty() && current_port_ > 0;
    }
    
    modbus_t* CreateTcpContext() {
#ifdef HAVE_LIBMODBUS
        return modbus_new_tcp(current_ip_.c_str(), current_port_);
#else
        return reinterpret_cast<modbus_t*>(0x12345678); // 시뮬레이션 포인터
#endif
    }
    
    modbus_t* CreateRtuContext() {
        return nullptr; // RTU는 향후 구현
    }
    
    void SetTimeouts() {
#ifdef HAVE_LIBMODBUS
        if (modbus_context_) {
            modbus_set_response_timeout(modbus_context_, 
                                       response_timeout_ms_ / 1000, 
                                       (response_timeout_ms_ % 1000) * 1000);
            modbus_set_byte_timeout(modbus_context_, 
                                   byte_timeout_ms_ / 1000,
                                   (byte_timeout_ms_ % 1000) * 1000);
        }
#endif
    }
    
    bool EnsureConnection() {
        if (connection_state_.load() != ModbusConnectionState::CONNECTED) {
            return ConnectProtocol();
        }
        return true;
    }
    
    void SetSlaveId(int slave_id) {
        if (modbus_context_ && slave_id != current_slave_id_) {
#ifdef HAVE_LIBMODBUS
            modbus_set_slave(modbus_context_, slave_id);
#endif
            current_slave_id_ = slave_id;
        }
    }
    
    void HandleConnectionError() {
        connection_state_.store(ModbusConnectionState::ERROR);
        modbus_timeouts_.fetch_add(1);
        
        if (auto_reconnect_enabled_) {
            connection_state_.store(ModbusConnectionState::RECONNECTING);
        }
    }
    
    void HandleModbusError() {
#ifdef HAVE_LIBMODBUS
        int error_code = errno;
        
        if (error_code == ETIMEDOUT || error_code == EAGAIN) {
            modbus_timeouts_.fetch_add(1);
        }
        
        if (error_code >= 1 && error_code <= 8) {
            modbus_exceptions_.fetch_add(1);
            
            std::lock_guard<std::mutex> lock(exception_mutex_);
            exception_codes_[static_cast<uint8_t>(error_code)]++;
        }
#else
        // 시뮬레이션 모드에서는 가끔 에러 발생
        static int error_counter = 0;
        if (++error_counter % 50 == 0) {
            modbus_timeouts_.fetch_add(1);
        }
#endif
    }
    
    bool ReadSingleDataPoint(const New::UnifiedDataPoint& point, New::UnifiedDataPoint& result) {
        int slave_id = current_slave_id_;
        
        switch (point.modbus_address.function_code) {
            case 3: // Holding Registers
                {
                    std::vector<uint16_t> values;
                    if (!ReadHoldingRegisters(slave_id, point.modbus_address.register_address, 1, values)) {
                        return false;
                    }
                    result.current_value = static_cast<double>(values[0]);
                }
                break;
                
            case 4: // Input Registers
                {
                    std::vector<uint16_t> values;
                    if (!ReadInputRegisters(slave_id, point.modbus_address.register_address, 1, values)) {
                        return false;
                    }
                    result.current_value = static_cast<double>(values[0]);
                }
                break;
                
            case 1: // Coils
                {
                    std::vector<uint8_t> values;
                    if (!ReadCoils(slave_id, point.modbus_address.register_address, 1, values)) {
                        return false;
                    }
                    result.current_value = static_cast<bool>(values[0]);
                }
                break;
                
            case 2: // Discrete Inputs
                {
                    std::vector<uint8_t> values;
                    if (!ReadCoils(slave_id, point.modbus_address.register_address, 1, values)) {
                        return false;
                    }
                    result.current_value = static_cast<bool>(values[0]);
                }
                break;
                
            default:
                return false;
        }
        
        return true;
    }
    
    bool WriteSingleDataPoint(const New::UnifiedDataPoint& point, 
                             const std::variant<bool, int32_t, uint32_t, float, double, std::string>& value) {
        
        switch (point.modbus_address.function_code) {
            case 6: // Write Single Register
            case 16: // Write Multiple Registers
                {
                    auto numeric_value = std::visit([](const auto& v) -> uint16_t {
                        if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                            return static_cast<uint16_t>(v);
                        } else {
                            return 0;
                        }
                    }, value);
                    
                    return WriteHoldingRegister(current_slave_id_, 
                                               point.modbus_address.register_address, 
                                               numeric_value);
                }
                
            case 5: // Write Single Coil
            case 15: // Write Multiple Coils
                {
                    auto bool_value = std::visit([](const auto& v) -> bool {
                        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
                            return v;
                        } else if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                            return v != 0;
                        } else {
                            return false;
                        }
                    }, value);
                    
#ifdef HAVE_LIBMODBUS
                    auto start_time = std::chrono::high_resolution_clock::now();
                    int result = modbus_write_bit(modbus_context_, 
                                                  point.modbus_address.register_address, 
                                                  bool_value ? 1 : 0);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    
                    bool success = (result != -1);
                    if (success) {
                        modbus_coil_writes_.fetch_add(1);
                    } else {
                        HandleModbusError();
                    }
                    return success;
#else
                    // 시뮬레이션 모드
                    modbus_coil_writes_.fetch_add(1);
                    return true;
#endif
                }
                
            default:
                return false;
        }
    }
    
    void CleanupModbus() {
        if (modbus_context_) {
            if (connection_state_.load() == ModbusConnectionState::CONNECTED) {
#ifdef HAVE_LIBMODBUS
                modbus_close(modbus_context_);
#endif
                connection_state_.store(ModbusConnectionState::DISCONNECTED);
            }
#ifdef HAVE_LIBMODBUS
            modbus_free(modbus_context_);
#endif
            modbus_context_ = nullptr;
        }
    }
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_UNIFIED_MODBUS_DRIVER_H