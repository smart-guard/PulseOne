// =============================================================================
// collector/include/Drivers/Modbus/ModbusPerformance.h
// Modbus 성능 최적화 클래스 (기본 골격)  
// =============================================================================
#ifndef PULSEONE_MODBUS_PERFORMANCE_H
#define PULSEONE_MODBUS_PERFORMANCE_H

namespace PulseOne {
namespace Drivers {

class ModbusDriver; // 전방 선언

class ModbusPerformance {
public:
    explicit ModbusPerformance(ModbusDriver* parent_driver);
    ~ModbusPerformance();
    
    // 성능 최적화
    bool EnablePerformanceMode() { return true; }
    void DisablePerformanceMode() {}
    bool IsPerformanceModeEnabled() const { return false; }
    
    // 배치 크기 설정
    void SetReadBatchSize(size_t batch_size) {}
    void SetWriteBatchSize(size_t batch_size) {}
    size_t GetReadBatchSize() const { return 10; }
    size_t GetWriteBatchSize() const { return 5; }

private:
    ModbusDriver* parent_driver_;
};

} // namespace Drivers
} // namespace PulseOne

#endif