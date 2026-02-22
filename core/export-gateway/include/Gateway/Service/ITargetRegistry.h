/**
 * @file ITargetRegistry.h
 * @brief Target Registry Interface - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_I_TARGET_REGISTRY_H
#define GATEWAY_SERVICE_I_TARGET_REGISTRY_H

#include "Export/GatewayExportTypes.h"
#include <map>
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

  // [v3.2] edge_servers.config target_priorities 기반 허용 타겟 ID + 우선순위
  // 설정 key=target_id, value=priority(낮을수록 먼저). loadFromDatabase() 호출
  // 전에 세팅.
  virtual void setTargetPriorities(const std::map<int, int> &priority_map) {}

  // backward compat
  virtual void setAllowedTargetIds(const std::set<int> &ids) {}

  virtual std::optional<DynamicTarget>
  getTarget(const std::string &name) const = 0;
  virtual std::vector<DynamicTarget> getAllTargets() const = 0;
  virtual std::set<std::string> getAssignedDeviceIds() const = 0;

  virtual PulseOne::Gateway::Target::ITargetHandler *
  getHandler(const std::string &target_name) const = 0;

  // Mapping lookups
  virtual std::string getTargetFieldName(int target_id, int point_id) const = 0;
  virtual bool isPointMapped(int target_id, int point_id) const = 0;
  virtual int getOverrideSiteId(int target_id, int point_id) const = 0;
  virtual std::string getExternalBuildingId(int target_id,
                                            int site_id) const = 0;

  // [FIX] Scale / Offset for measured_value transformation
  // Default: identity (scale=1.0, offset=0.0 → no transformation)
  virtual double getScale(int /*target_id*/, int /*point_id*/) const {
    return 1.0;
  }
  virtual double getOffset(int /*target_id*/, int /*point_id*/) const {
    return 0.0;
  }
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_I_TARGET_REGISTRY_H
