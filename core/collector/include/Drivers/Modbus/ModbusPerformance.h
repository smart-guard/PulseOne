// =============================================================================
// collector/include/Drivers/Modbus/ModbusPerformance.h  
// =============================================================================

#ifndef PULSEONE_MODBUS_PERFORMANCE_H
#define PULSEONE_MODBUS_PERFORMANCE_H

#include <cstddef>

namespace PulseOne {
namespace Drivers {

class ModbusDriver;

class ModbusPerformance {
public:
    explicit ModbusPerformance(ModbusDriver* parent_driver);
    ~ModbusPerformance();
    
    ModbusPerformance(const ModbusPerformance&) = delete;
    ModbusPerformance& operator=(const ModbusPerformance&) = delete;
    
    bool EnablePerformanceMode();
    void DisablePerformanceMode();
    bool IsPerformanceModeEnabled() const;
    
    void SetReadBatchSize(size_t batch_size);
    void SetWriteBatchSize(size_t batch_size);
    size_t GetReadBatchSize() const;
    size_t GetWriteBatchSize() const;
    
    int TestConnectionQuality();
    bool StartRealtimeMonitoring(int interval_seconds = 5);
    void StopRealtimeMonitoring();
    bool IsRealtimeMonitoringEnabled() const;
    
    bool UpdateTimeout(int timeout_ms);
    bool UpdateRetryCount(int retry_count);
    bool UpdateSlaveResponseDelay(int delay_ms);

private:
    ModbusDriver* parent_driver_;
    bool performance_mode_enabled_;
    bool realtime_monitoring_enabled_;
    size_t read_batch_size_;
    size_t write_batch_size_;
};

} // namespace Drivers
} // namespace PulseOne

#endif