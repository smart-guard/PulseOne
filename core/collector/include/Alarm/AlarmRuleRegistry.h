#ifndef ALARM_RULE_REGISTRY_H
#define ALARM_RULE_REGISTRY_H

#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Alarm {

class AlarmRuleRegistry {
public:
  AlarmRuleRegistry(
      std::shared_ptr<Database::Repositories::AlarmRuleRepository> repo);

  void loadRules(int tenant_id = 0);
  std::vector<Database::Entities::AlarmRuleEntity>
  getRulesForPoint(int tenant_id, int point_id) const;
  std::vector<Database::Entities::AlarmRuleEntity>
  getAllRules(int tenant_id = 0) const;
  bool isTenantLoaded(int tenant_id) const;

private:
  std::shared_ptr<Database::Repositories::AlarmRuleRepository> repo_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<int, std::vector<Database::Entities::AlarmRuleEntity>>
      tenant_rules_;
  std::unordered_map<int, std::unordered_map<int, std::vector<int>>>
      tenant_point_rules_; // tenant_id -> point_id -> list of indices
};

} // namespace Alarm
} // namespace PulseOne

#endif
