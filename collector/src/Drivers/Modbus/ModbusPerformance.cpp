// =============================================================================
// collector/src/Drivers/Modbus/ModbusPerformance.cpp
// =============================================================================
#include "Drivers/Modbus/ModbusPerformance.h"
#include "Drivers/Modbus/ModbusDriver.h"

namespace PulseOne {
namespace Drivers {

ModbusPerformance::ModbusPerformance(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver) {
}

ModbusPerformance::~ModbusPerformance() {
}

} // namespace Drivers
} // namespace PulseOne