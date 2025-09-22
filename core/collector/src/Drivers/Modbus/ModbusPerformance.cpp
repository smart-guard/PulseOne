// =============================================================================
// collector/src/Drivers/Modbus/ModbusPerformance.cpp
// =============================================================================

#include "Drivers/Modbus/ModbusPerformance.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Drivers {

ModbusPerformance::ModbusPerformance(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver)
    , performance_mode_enabled_(false)
    , realtime_monitoring_enabled_(false)
    , read_batch_size_(10)
    , write_batch_size_(5) {
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusPerformance created");
}

ModbusPerformance::~ModbusPerformance() {
    DisablePerformanceMode();
    auto& logger = LogManager::getInstance();
    logger.Info("ModbusPerformance destroyed");
}

bool ModbusPerformance::EnablePerformanceMode() {
    performance_mode_enabled_ = true;
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus performance mode enabled");
    return true;
}

void ModbusPerformance::DisablePerformanceMode() {
    StopRealtimeMonitoring();
    performance_mode_enabled_ = false;
    auto& logger = LogManager::getInstance();
    logger.Info("Modbus performance mode disabled");
}

bool ModbusPerformance::IsPerformanceModeEnabled() const {
    return performance_mode_enabled_;
}

void ModbusPerformance::SetReadBatchSize(size_t batch_size) {
    read_batch_size_ = batch_size;
}

void ModbusPerformance::SetWriteBatchSize(size_t batch_size) {
    write_batch_size_ = batch_size;
}

size_t ModbusPerformance::GetReadBatchSize() const {
    return read_batch_size_;
}

size_t ModbusPerformance::GetWriteBatchSize() const {
    return write_batch_size_;
}

int ModbusPerformance::TestConnectionQuality() {
    return 100; // 향후 구현
}

bool ModbusPerformance::StartRealtimeMonitoring(int interval_seconds) {
    realtime_monitoring_enabled_ = true;
    return true;
}

void ModbusPerformance::StopRealtimeMonitoring() {
    realtime_monitoring_enabled_ = false;
}

bool ModbusPerformance::IsRealtimeMonitoringEnabled() const {
    return realtime_monitoring_enabled_;
}

bool ModbusPerformance::UpdateTimeout(int timeout_ms) {
    return true; // 향후 구현
}

bool ModbusPerformance::UpdateRetryCount(int retry_count) {
    return true; // 향후 구현
}

bool ModbusPerformance::UpdateSlaveResponseDelay(int delay_ms) {
    return true; // 향후 구현
}

} // namespace Drivers
} // namespace PulseOne