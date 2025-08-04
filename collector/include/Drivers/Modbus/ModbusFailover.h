// =============================================================================
// collector/include/Drivers/Modbus/ModbusFailover.h
// Modbus 페일오버 및 복구 기능 (스텁 구현) - 수정된 버전
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
    bool IsFailoverEnabled() const;  // ✅ 구현 파일과 일치하도록 수정
    
    bool AddBackupEndpoint(const std::string& endpoint);
    void RemoveBackupEndpoint(const std::string& endpoint);
    std::vector<std::string> GetActiveEndpoints() const;
    
    bool SwitchToBackup();
    bool CheckPrimaryRecovery();
    std::string GetCurrentEndpoint() const;  // ✅ 누락된 메서드 추가

private:
    ModbusDriver* parent_driver_;
    std::atomic<bool> failover_enabled_{false};  // ✅ 구현 파일과 일치하도록 수정
    std::vector<std::string> backup_endpoints_;
    std::string current_endpoint_;  // ✅ 누락된 멤버 변수 추가
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MODBUS_FAILOVER_H