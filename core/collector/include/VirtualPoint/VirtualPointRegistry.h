#ifndef VIRTUAL_POINT_REGISTRY_H
#define VIRTUAL_POINT_REGISTRY_H

#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "VirtualPoint/VirtualPointTypes.h"
#include "Common/Structs.h"

namespace PulseOne {
namespace VirtualPoint {

// Forward declaration of VirtualPointDef to be moved here or kept in VirtualPointEngine.h
// Usually, it's better to keep it in a common header if shared, or define it here.
// For now, let's assume it's moved here or in VirtualPointTypes.h.
// Actually, it's currently in VirtualPointEngine.h. Let's move it to VirtualPointRegistry.h.

struct VirtualPointDef {
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    std::string formula;
    VirtualPointState state = VirtualPointState::INACTIVE;
    ExecutionType execution_type = ExecutionType::JAVASCRIPT;
    ErrorHandling error_handling = ErrorHandling::RETURN_NULL;
    CalculationTrigger trigger = CalculationTrigger::ON_CHANGE;
    std::string data_type = "float";
    std::string unit;
    std::chrono::milliseconds calculation_interval_ms{1000};
    nlohmann::json input_points;
    nlohmann::json input_mappings;
    std::string script_id;
    bool is_enabled = true;
    std::chrono::system_clock::time_point last_calculation;
    PulseOne::Structs::DataValue last_value;

    std::string getStateString() const { return virtualPointStateToString(state); }
    std::string getExecutionTypeString() const { return executionTypeToString(execution_type); }
    std::string getErrorHandlingString() const { return errorHandlingToString(error_handling); }
};

class VirtualPointRegistry {
public:
    VirtualPointRegistry() = default;
    ~VirtualPointRegistry() = default;

    bool loadVirtualPoints(int tenant_id = 0);
    std::optional<VirtualPointDef> getVirtualPoint(int vp_id) const;
    bool reloadVirtualPoint(int vp_id);
    void clear();

    // Dependency Management
    std::vector<int> getAffectedVirtualPoints(const PulseOne::Structs::DeviceDataMessage& msg) const;
    std::vector<int> getDependentVirtualPoints(int point_id) const;
    bool hasDependency(int vp_id, int point_id) const;
    
    // Internal maps access (if needed by facade)
    const std::unordered_map<int, VirtualPointDef>& getAllVirtualPoints() const { return virtual_points_; }

protected:
    // Helper to convert entity types (to be moved from Engine)
    ExecutionType convertEntityExecutionType(const std::string& entity_type_str);
    ErrorHandling convertEntityErrorHandling(const std::string& entity_handling_str);

private:
    std::unordered_map<int, VirtualPointDef> virtual_points_;
    mutable std::shared_mutex vp_mutex_;

    std::unordered_map<int, std::unordered_set<int>> point_to_vp_map_;
    std::unordered_map<int, std::unordered_set<int>> vp_dependencies_;
    mutable std::shared_mutex dep_mutex_;
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_REGISTRY_H
