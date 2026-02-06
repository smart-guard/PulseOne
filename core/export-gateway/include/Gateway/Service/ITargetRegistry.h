/**
 * @file ITargetRegistry.h
 * @brief Target Registry Interface - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_I_TARGET_REGISTRY_H
#define GATEWAY_SERVICE_I_TARGET_REGISTRY_H

#include "Export/ExportTypes.h"
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace PulseOne {
namespace Gateway {
namespace Target {
class ITargetHandler;
}

namespace Service {

using PulseOne::Export::DynamicTarget;

/**
 * @brief Manages target configurations, handlers, and mappings
 */
class ITargetRegistry {
public:
  virtual ~ITargetRegistry() = default;

  virtual bool loadFromDatabase() = 0;
  virtual bool forceReload() = 0;

  virtual std::optional<DynamicTarget>
  getTarget(const std::string &name) const = 0;
  virtual std::vector<DynamicTarget> getAllTargets() const = 0;
  virtual std::set<std::string> getAssignedDeviceIds() const = 0;

  virtual PulseOne::Gateway::Target::ITargetHandler *
  getHandler(const std::string &target_name) const = 0;

  // Mapping lookups
  virtual std::string getTargetFieldName(int target_id, int point_id) const = 0;
  virtual int getOverrideSiteId(int target_id, int point_id) const = 0;
  virtual std::string getExternalBuildingId(int target_id,
                                            int site_id) const = 0;
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_I_TARGET_REGISTRY_H
