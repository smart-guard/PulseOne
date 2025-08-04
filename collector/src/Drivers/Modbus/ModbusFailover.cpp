// =============================================================================
// collector/src/Drivers/Modbus/ModbusFailover.cpp
// =============================================================================

#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Drivers {

ModbusFailover::ModbusFailover(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver)
    , failover_enabled_(false) {
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusFailover created");
}

ModbusFailover::~ModbusFailover() {
    DisableFailover();
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusFailover destroyed");
}

bool ModbusFailover::EnableFailover(int failure_threshold, int recovery_check_interval) {
    failover_enabled_ = true;
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus failover enabled");
    return true;
}

void ModbusFailover::DisableFailover() {
    failover_enabled_ = false;
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus failover disabled");
}

bool ModbusFailover::IsFailoverEnabled() const {
    return failover_enabled_;
}

bool ModbusFailover::AddBackupEndpoint(const std::string& endpoint) {
    backup_endpoints_.push_back(endpoint);
    return true;
}

void ModbusFailover::RemoveBackupEndpoint(const std::string& endpoint) {
    // 향후 구현
}

std::vector<std::string> ModbusFailover::GetActiveEndpoints() const {
    return backup_endpoints_;
}

std::string ModbusFailover::GetCurrentEndpoint() const {
    return current_endpoint_;
}

} // namespace Drivers
} // namespace PulseOne