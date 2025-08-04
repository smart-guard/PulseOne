// =============================================================================
// collector/include/Drivers/Modbus/ModbusFailover.h
// Modbus 페일오버 및 복구 기능 (스텁 구현)
// =============================================================================

#ifndef PULSEONE_MODBUS_FAILOVER_H
#define PULSEONE_MODBUS_FAILOVER_H

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>

namespace PulseOne {
namespace Drivers {

// 전방 선언
class ModbusDriver;

/**
 * @brief Modbus 페일오버 및 복구 기능 (스텁)
 * @details 향후 완전 구현 예정
 */
class ModbusFailover {
public:
    explicit ModbusFailover(ModbusDriver* parent_driver);
    ~ModbusFailover();
    
    // 복사/이동 방지
    ModbusFailover(const ModbusFailover&) = delete;
    ModbusFailover& operator=(const ModbusFailover&) = delete;
    
    // =======================================================================
    // 페일오버 기능 (스텁)
    // =======================================================================
    
    bool EnableFailover(int failure_threshold = 3, int recovery_check_interval = 60);
    void DisableFailover();
    bool IsEnabled() const;
    
    bool AddBackupEndpoint(const std::string& endpoint);
    void RemoveBackupEndpoint(const std::string& endpoint);
    std::vector<std::string> GetActiveEndpoints() const;
    
    bool SwitchToBackup();
    bool CheckPrimaryRecovery();

private:
    ModbusDriver* parent_driver_;
    std::atomic<bool> enabled_{false};
    std::vector<std::string> backup_endpoints_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MODBUS_FAILOVER_H