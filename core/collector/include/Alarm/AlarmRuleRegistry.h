#ifndef ALARM_RULE_REGISTRY_H
#define ALARM_RULE_REGISTRY_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/Repositories/AlarmRuleRepository.h"

namespace PulseOne {
namespace Alarm {

class AlarmRuleRegistry {
public:
    AlarmRuleRegistry(std::shared_ptr<Database::Repositories::AlarmRuleRepository> repo);
    
    void loadRules(int tenant_id = 0);
    std::vector<Database::Entities::AlarmRuleEntity> getRulesForPoint(int tenant_id, int point_id) const;
    std::vector<Database::Entities::AlarmRuleEntity> getAllRules(int tenant_id = 0) const;
    bool isTenantLoaded(int tenant_id) const;

private:
    std::shared_ptr<Database::Repositories::AlarmRuleRepository> repo_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<int, std::vector<Database::Entities::AlarmRuleEntity>> tenant_rules_;
    std::unordered_map<int, std::vector<int>> point_to_rules_; // point_id -> list of rule indices in tenant_rules_
};

} // namespace Alarm
} // namespace PulseOne

#endif
