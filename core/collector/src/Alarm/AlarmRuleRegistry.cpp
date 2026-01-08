#include "Alarm/AlarmRuleRegistry.h"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Alarm {

AlarmRuleRegistry::AlarmRuleRegistry(std::shared_ptr<Database::Repositories::AlarmRuleRepository> repo)
    : repo_(repo) {}

void AlarmRuleRegistry::loadRules(int tenant_id) {
    if (!repo_) return;
    
    try {
        auto rules = repo_->findByTenant(tenant_id);
        
        std::unique_lock<std::shared_mutex> lock(mutex_);
        tenant_rules_[tenant_id] = rules;
        
        // Rebuild index
        point_to_rules_.clear();
        for (size_t i = 0; i < rules.size(); ++i) {
            if (rules[i].isEnabled()) {
                auto target_id = rules[i].getTargetId();
                if (target_id.has_value()) {
                    point_to_rules_[*target_id].push_back(static_cast<int>(i));
                }
            }
        }
        
        LogManager::getInstance().Info("AlarmRuleRegistry: Loaded " + std::to_string(rules.size()) + 
                                     " rules for tenant " + std::to_string(tenant_id));
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmRuleRegistry: Failed to load rules: " + std::string(e.what()));
    }
}

std::vector<Database::Entities::AlarmRuleEntity> AlarmRuleRegistry::getRulesForPoint(int tenant_id, int point_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<Database::Entities::AlarmRuleEntity> result;
    auto it_tenant = tenant_rules_.find(tenant_id);
    if (it_tenant == tenant_rules_.end()) return result;
    
    auto it_point = point_to_rules_.find(point_id);
    if (it_point == point_to_rules_.end()) return result;
    
    for (int idx : it_point->second) {
        if (idx >= 0 && static_cast<size_t>(idx) < it_tenant->second.size()) {
            result.push_back(it_tenant->second[idx]);
        }
    }
    
    return result;
}

std::vector<Database::Entities::AlarmRuleEntity> AlarmRuleRegistry::getAllRules(int tenant_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = tenant_rules_.find(tenant_id);
    if (it != tenant_rules_.end()) return it->second;
    return {};
}

bool AlarmRuleRegistry::isTenantLoaded(int tenant_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return tenant_rules_.find(tenant_id) != tenant_rules_.end();
}

} // namespace Alarm
} // namespace PulseOne
