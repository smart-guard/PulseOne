// =============================================================================
// collector/src/Drivers/Modbus/ModbusFailover.cpp
// =============================================================================
#include "Drivers/Modbus/ModbusFailover.h"
#include "Drivers/Modbus/ModbusDriver.h"

namespace PulseOne {
namespace Drivers {

ModbusFailover::ModbusFailover(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver) {
}

ModbusFailover::~ModbusFailover() {
}

} // namespace Drivers
} // namespace PulseOne