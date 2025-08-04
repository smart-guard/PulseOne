// =============================================================================
// collector/src/Drivers/Modbus/ModbusDiagnostics.cpp
// =============================================================================
#include "Drivers/Modbus/ModbusDiagnostics.h"
#include "Drivers/Modbus/ModbusDriver.h"

namespace PulseOne {
namespace Drivers {

ModbusDiagnostics::ModbusDiagnostics(ModbusDriver* parent_driver)
    : parent_driver_(parent_driver) {
}

ModbusDiagnostics::~ModbusDiagnostics() {
}

std::string ModbusDiagnostics::GetDiagnosticsJSON() const {
    return R"({"diagnostics":"basic_implementation"})";
}

std::map<std::string, std::string> ModbusDiagnostics::GetDiagnostics() const {
    std::map<std::string, std::string> diagnostics;
    diagnostics["status"] = "basic_implementation";
    return diagnostics;
}

} // namespace Drivers
} // namespace PulseOne