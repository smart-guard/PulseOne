// UnifiedModbusDriver.cpp - 실제 헤더 구조에 맞는 구현
#include "Drivers/UnifiedModbusDriver.h"
#include <iostream>
#include <sstream>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 헤더에서 선언만 된 확장 메서드들 구현
// =============================================================================

bool UnifiedModbusDriver::WriteHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, const std::vector<uint16_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_registers(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    // 시뮬레이션 모드
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (!success) {
        HandleModbusError();
        return false;
    }
    
    modbus_register_writes_.fetch_add(count);
    return true;
}

bool UnifiedModbusDriver::WriteCoil(int slave_id, uint16_t addr, bool value) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_bit(modbus_context_, addr, value ? 1 : 0);
    success = (result != -1);
#else
    // 시뮬레이션 모드
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (!success) {
        HandleModbusError();
        return false;
    }
    
    modbus_coil_writes_.fetch_add(1);
    return true;
}

bool UnifiedModbusDriver::WriteCoils(int slave_id, uint16_t start_addr, uint16_t count, const std::vector<uint8_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_bits(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    // 시뮬레이션 모드
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (!success) {
        HandleModbusError();
        return false;
    }
    
    modbus_coil_writes_.fetch_add(count);
    return true;
}

bool UnifiedModbusDriver::ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint8_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    values.resize(count);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_read_input_bits(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    // 시뮬레이션 모드
    for (uint16_t i = 0; i < count; i++) {
        values[i] = (start_addr + i + 2000) % 2;
    }
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (!success) {
        HandleModbusError();
        return false;
    }
    
    modbus_coil_reads_.fetch_add(1);
    return true;
}

std::string UnifiedModbusDriver::GetDetailedDiagnostics() const {
    std::ostringstream oss;
    oss << "=== UnifiedModbusDriver 상세 진단 정보 ===\n";
    oss << "드라이버 버전: 1.3.0\n";
    oss << "현재 시간: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
    
    oss << "\n📡 연결 정보:\n";
    oss << "  엔드포인트: " << device_info_.endpoint << "\n";
    oss << "  슬레이브 ID: " << current_slave_id_ << "\n";
    oss << "  응답 타임아웃: " << response_timeout_ms_ << "ms\n";
    oss << "  바이트 타임아웃: " << byte_timeout_ms_ << "ms\n";
    oss << "  연결 상태: " << GetConnectionStateString() << "\n";
    
    oss << "\n🔧 libmodbus 정보:\n";
#ifdef HAVE_LIBMODBUS
    oss << "  libmodbus: 활성화\n";
    oss << "  컨텍스트: " << (modbus_context_ ? "유효" : "NULL") << "\n";
#else
    oss << "  libmodbus: 시뮬레이션 모드\n";
#endif
    
    oss << "\n📊 Modbus 통계:\n";
    oss << "  레지스터 읽기: " << modbus_register_reads_.load() << "\n";
    oss << "  입력 레지스터 읽기: " << modbus_register_reads_.load() << "\n";
    oss << "  코일 읽기: " << modbus_coil_reads_.load() << "\n";
    oss << "  레지스터 쓰기: " << modbus_register_writes_.load() << "\n";
    oss << "  코일 쓰기: " << modbus_coil_writes_.load() << "\n";
    oss << "  예외 발생: " << modbus_exceptions_.load() << "\n";
    oss << "  타임아웃: " << modbus_timeouts_.load() << "\n";
    oss << "  연결 성공: " << modbus_connections_.load() << "\n";
    oss << "  연결 실패: " << modbus_connection_failures_.load() << "\n";
    
    oss << "\n🎯 데이터 포인트 정보:\n";
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        oss << "  등록된 포인트 수: " << data_points_.size() << "\n";
        if (!data_points_.empty()) {
            oss << "  포인트 목록:\n";
            for (size_t i = 0; i < std::min(data_points_.size(), size_t(5)); ++i) {
                const auto& point = data_points_[i];
                oss << "    [" << i << "] " << point.id << " (" << point.name << ")\n";
            }
            if (data_points_.size() > 5) {
                oss << "    ... 총 " << data_points_.size() << "개\n";
            }
        }
    }
    
    oss << "\n⚙️ 설정 플래그:\n";
    oss << "  진단 모드: " << (diagnostics_enabled_ ? "활성화" : "비활성화") << "\n";
    oss << "  자동 재연결: " << (auto_reconnect_enabled_ ? "활성화" : "비활성화") << "\n";
    oss << "  최대 재연결 시도: " << max_reconnect_attempts_ << "\n";
    oss << "  재연결 지연: " << reconnect_delay_ms_ << "ms\n";
    
    return oss.str();
}

void UnifiedModbusDriver::ResetModbusStatistics() {
    modbus_register_reads_.store(0);
    modbus_coil_reads_.store(0);
    modbus_register_writes_.store(0);
    modbus_coil_writes_.store(0);
    modbus_exceptions_.store(0);
    modbus_timeouts_.store(0);
    modbus_connections_.store(0);
    modbus_connection_failures_.store(0);
    
    {
        std::lock_guard<std::mutex> lock(exception_mutex_);
        exception_codes_.clear();
    }
}

bool UnifiedModbusDriver::TestConnection() {
    if (IsModbusConnected()) {
        // 이미 연결되어 있으면 간단한 읽기 테스트
        std::vector<uint16_t> test_values;
        bool test_result = ReadHoldingRegisters(current_slave_id_, 0, 1, test_values);
        return test_result;
    } else {
        // 연결되어 있지 않으면 연결 시도
        return ConnectProtocol();
    }
}

} // namespace Drivers
} // namespace PulseOne