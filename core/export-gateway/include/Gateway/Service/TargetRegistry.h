/**
 * @file TargetRegistry.h
 * @brief Target Registry Implementation - PulseOne::Gateway::Service
 */

#ifndef GATEWAY_SERVICE_TARGET_REGISTRY_H
#define GATEWAY_SERVICE_TARGET_REGISTRY_H

#include "Gateway/Service/ITargetRegistry.h"
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Gateway {
namespace Service {

/**
 * @brief Implementation of ITargetRegistry
 */
class TargetRegistry : public ITargetRegistry {
private:
  int gateway_id_{0};

  mutable std::shared_mutex targets_mutex_;
  std::vector<DynamicTarget> targets_;

  mutable std::mutex handlers_mutex_;
  std::unordered_map<std::string,
                     std::unique_ptr<PulseOne::Gateway::Target::ITargetHandler>>
      handlers_;

  mutable std::shared_mutex mappings_mutex_;
  std::unordered_map<int, std::unordered_map<int, std::string>>
      target_point_mappings_;
  std::unordered_map<int, std::unordered_map<int, int>>
      target_point_site_mappings_;
  std::unordered_map<int, std::unordered_map<int, std::string>>
      target_site_mappings_;
  std::set<std::string> assigned_device_ids_;

public:
  explicit TargetRegistry(int gateway_id);
  ~TargetRegistry() override;

  bool loadFromDatabase() override;
  bool forceReload() override { return loadFromDatabase(); }

  std::optional<DynamicTarget>
  getTarget(const std::string &name) const override;
  std::vector<DynamicTarget> getAllTargets() const override;
  std::set<std::string> getAssignedDeviceIds() const override;

  PulseOne::Gateway::Target::ITargetHandler *
  getHandler(const std::string &target_name) const override;

  std::string getTargetFieldName(int target_id, int point_id) const override;
  bool isPointMapped(int target_id, int point_id) const override;
  int getOverrideSiteId(int target_id, int point_id) const override;
  std::string getExternalBuildingId(int target_id, int site_id) const override;

private:
  void initializeHandlers();
};

} // namespace Service
} // namespace Gateway
} // namespace PulseOne

#endif // GATEWAY_SERVICE_TARGET_REGISTRY_H
