// =============================================================================
// collector/include/Drivers/Modbus/ModbusFailover.h
// Modbus 페일오버 클래스 (기본 골격)
// =============================================================================
#ifndef PULSEONE_MODBUS_FAILOVER_H
#define PULSEONE_MODBUS_FAILOVER_H

#include <string>
#include <vector>

namespace PulseOne {
namespace Drivers {

class ModbusDriver; // 전방 선언

class ModbusFailover {
public:
    explicit ModbusFailover(ModbusDriver* parent_driver);
    ~ModbusFailover();
    
    // 페일오버 관리
    bool EnableFailover(int failure_threshold = 3, int recovery_check_interval = 60) { return true; }
    void DisableFailover() {}
    bool IsFailoverEnabled() const { return false; }
    
    // 백업 엔드포인트
    bool AddBackupEndpoint(const std::string& endpoint) { return true; }
    void RemoveBackupEndpoint(const std::string& endpoint) {}
    std::vector<std::string> GetActiveEndpoints() const { return {}; }

private:
    ModbusDriver* parent_driver_;
};

} // namespace Drivers
} // namespace PulseOne

#endif
