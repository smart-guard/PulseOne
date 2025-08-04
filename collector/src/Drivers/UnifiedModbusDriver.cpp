// UnifiedModbusDriver.cpp - 경고 제거 버전
#include "Drivers/UnifiedModbusDriver.h"
#include <iostream>
#include <sstream>

namespace PulseOne {
namespace Drivers {

bool UnifiedModbusDriver::WriteHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, const std::vector<uint16_t>& values) {
    if (!EnsureConnection()) {
        return false;
    }
    
    SetSlaveId(slave_id);
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_registers(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
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
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_bit(modbus_context_, addr, value ? 1 : 0);
    success = (result != -1);
#else
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
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
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_write_bits(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    
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
    bool success = false;
    
#ifdef HAVE_LIBMODBUS
    int result = modbus_read_input_bits(modbus_context_, start_addr, count, values.data());
    success = (result != -1);
#else
    for (uint16_t i = 0; i < count; i++) {
        values[i] = (start_addr + i + 2000) % 2;
    }
    success = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
    
    if (!success) {
        HandleModbusError();
        return false;
    }
    
    modbus_coil_reads_.fetch_add(1);
    return true;
}

std::string UnifiedModbusDriver::GetDetailedDiagnostics() const {
    std::ostringstream oss;
    oss << "=== UnifiedModbusDriver 진단 정보 ===\n";
    oss << "드라이버 버전: 1.3.0\n";
    oss << "엔드포인트: " << device_info_.endpoint << "\n";
    oss << "슬레이브 ID: " << current_slave_id_ << "\n";
    oss << "연결 상태: " << GetConnectionStateString() << "\n";
    
#ifdef HAVE_LIBMODBUS
    oss << "libmodbus: 활성화 (v3.1.10)\n";
#else
    oss << "libmodbus: 시뮬레이션 모드\n";
#endif
    
    oss << "레지스터 읽기: " << modbus_register_reads_.load() << "\n";
    oss << "코일 읽기: " << modbus_coil_reads_.load() << "\n";
    oss << "레지스터 쓰기: " << modbus_register_writes_.load() << "\n";
    oss << "코일 쓰기: " << modbus_coil_writes_.load() << "\n";
    oss << "예외 발생: " << modbus_exceptions_.load() << "\n";
    oss << "타임아웃: " << modbus_timeouts_.load() << "\n";
    
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
}

bool UnifiedModbusDriver::TestConnection() {
    if (IsModbusConnected()) {
        std::vector<uint16_t> test_values;
        return ReadHoldingRegisters(current_slave_id_, 0, 1, test_values);
    } else {
        return ConnectProtocol();
    }
}

} // namespace Drivers
} // namespace PulseOne
