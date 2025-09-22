// =============================================================================
// collector/src/Drivers/Modbus/ModbusFailover.cpp
// Modbus 페일오버 기능 구현 - 수정된 버전
// =============================================================================

#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Drivers {

ModbusFailover::ModbusFailover(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver)
    , failover_enabled_(false)
    , current_endpoint_("primary") {  // ✅ 누락된 멤버 초기화 추가
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusFailover created");
}

ModbusFailover::~ModbusFailover() {
    DisableFailover();
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusFailover destroyed");
}

bool ModbusFailover::EnableFailover(int failure_threshold, int recovery_check_interval) {
    failover_enabled_ = true;  // ✅ 올바른 멤버 변수 사용
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus failover enabled with threshold: " + std::to_string(failure_threshold) +
                ", interval: " + std::to_string(recovery_check_interval));
    return true;
}

void ModbusFailover::DisableFailover() {
    failover_enabled_ = false;  // ✅ 올바른 멤버 변수 사용
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus failover disabled");
}

bool ModbusFailover::IsFailoverEnabled() const {  // ✅ 헤더와 일치하는 메서드명
    return failover_enabled_;
}

bool ModbusFailover::AddBackupEndpoint(const std::string& endpoint) {
    backup_endpoints_.push_back(endpoint);
    auto& logger = LogManager::getInstance();
    logger.Info("Added backup endpoint: " + endpoint);
    return true;
}

void ModbusFailover::RemoveBackupEndpoint(const std::string& endpoint) {
    auto it = std::remove(backup_endpoints_.begin(), backup_endpoints_.end(), endpoint);
    if (it != backup_endpoints_.end()) {
        backup_endpoints_.erase(it, backup_endpoints_.end());
        auto& logger = LogManager::getInstance();
        logger.Info("Removed backup endpoint: " + endpoint);
    }
}

std::vector<std::string> ModbusFailover::GetActiveEndpoints() const {
    return backup_endpoints_;
}

bool ModbusFailover::SwitchToBackup() {
    if (!failover_enabled_ || backup_endpoints_.empty()) {
        return false;
    }
    
    // 간단한 스텁 구현 - 첫 번째 백업으로 전환
    current_endpoint_ = backup_endpoints_[0];
    auto& logger = LogManager::getInstance();
    logger.Info("Switched to backup endpoint: " + current_endpoint_);
    return true;
}

bool ModbusFailover::CheckPrimaryRecovery() {
    if (!failover_enabled_) {
        return false;
    }
    
    // 스텁 구현 - 항상 복구되지 않았다고 가정
    auto& logger = LogManager::getInstance();
    logger.Debug("Checking primary endpoint recovery...");
    return false;
}

std::string ModbusFailover::GetCurrentEndpoint() const {  // ✅ 헤더에 선언된 메서드 구현
    return current_endpoint_;
}

} // namespace Drivers
} // namespace PulseOne