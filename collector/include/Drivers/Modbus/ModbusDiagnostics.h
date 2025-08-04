// =============================================================================
// collector/include/Drivers/Modbus/ModbusDiagnostics.h
// Modbus 진단 기능 클래스 (기본 골격)
// =============================================================================
#ifndef PULSEONE_MODBUS_DIAGNOSTICS_H
#define PULSEONE_MODBUS_DIAGNOSTICS_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace PulseOne {
namespace Drivers {

class ModbusDriver; // 전방 선언

class ModbusDiagnostics {
public:
    explicit ModbusDiagnostics(ModbusDriver* parent_driver);
    ~ModbusDiagnostics();
    
    // 기본 진단 API
    std::string GetDiagnosticsJSON() const;
    std::map<std::string, std::string> GetDiagnostics() const;
    
    // 데이터 기록
    void RecordExceptionCode(uint8_t exception_code) {}
    void RecordCrcCheck(bool crc_valid) {}
    void RecordResponseTime(int slave_id, uint32_t response_time_ms) {}
    void RecordRegisterAccess(uint16_t address, bool is_read, bool is_write) {}
    void RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms) {}
    
    // 패킷 로깅
    void EnablePacketLogging(bool enable = true) {}
    void EnableConsoleOutput(bool enable = true) {}

private:
    ModbusDriver* parent_driver_;
};

} // namespace Drivers
} // namespace PulseOne

#endif