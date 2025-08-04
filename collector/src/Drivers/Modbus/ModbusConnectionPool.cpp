// =============================================================================
// collector/src/Drivers/Modbus/ModbusConnectionPool.cpp
// =============================================================================
#include "Drivers/Modbus/ModbusConnectionPool.h"
#include "Drivers/Modbus/ModbusDriver.h"

namespace PulseOne {
namespace Drivers {

ModbusConnectionPool::ModbusConnectionPool(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver) {
}

ModbusConnectionPool::~ModbusConnectionPool() {
}

} // namespace Drivers
} // namespace PulseOne