// =============================================================================
// collector/src/Alarm/AlarmEngine.cpp - Component-based Architecture
// =============================================================================

#include "Alarm/AlarmEngine.h"
#include "Alarm/AlarmRuleRegistry.h"
#include "Alarm/AlarmStateCache.h"
#include "Alarm/AlarmEvaluator.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

#include <algorithm>
#include <variant>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 싱글톤 및 생성자/소멸자
// =============================================================================

AlarmEngine& AlarmEngine::getInstance() {
    static AlarmEngine instance;
    return instance;
}

AlarmEngine::AlarmEngine() {
    LogManager::getInstance().Debug("AlarmEngine (Component-based) initializing...");
    
    try {
        // 1. Repository 초기화
        initializeRepositories(); 
        
        // 2. 신규 컴포넌트 초기화
        registry_ = std::make_unique<AlarmRuleRegistry>(alarm_rule_repo_);
        cache_ = std::make_unique<AlarmStateCache>();
        evaluator_ = std::make_unique<AlarmEvaluator>(executor_, *cache_);

        // 3. ScriptExecutor 초기화
        if (!executor_.initialize()) {
            LogManager::getInstance().Error("ScriptExecutor initialization failed");
        }
        
        // 4. 초기 데이터 로드
        loadInitialData();
        registry_->loadRules(0); // Default tenant
        
        initialized_ = true;
        LogManager::getInstance().Info("AlarmEngine initialized successfully with component architecture");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmEngine initialization failed: " + std::string(e.what()));
        initialized_ = false;
    }
}

AlarmEngine::~AlarmEngine() {
    shutdown();
}

void AlarmEngine::shutdown() {
    if (!initialized_.load()) return;
    
    LogManager::getInstance().Info("AlarmEngine shutting down...");
    
    executor_.shutdown();
    
    // Components are managed by unique_ptr
    
    initialized_ = false;
    LogManager::getInstance().Info("AlarmEngine shutdown complete");
}

// =============================================================================
// 초기화 메서드들
// =============================================================================

void AlarmEngine::initializeRepositories() {
    auto& repo_factory = Database::RepositoryFactory::getInstance();
    alarm_rule_repo_ = repo_factory.getAlarmRuleRepository();
    alarm_occurrence_repo_ = repo_factory.getAlarmOccurrenceRepository();
    
    if (!alarm_rule_repo_ || !alarm_occurrence_repo_) {
        LogManager::getInstance().Error("Failed to initialize necessary repositories for AlarmEngine");
    }
}

void AlarmEngine::loadInitialData() {
    if (alarm_occurrence_repo_) {
        int max_id = alarm_occurrence_repo_->findMaxId();
        next_occurrence_id_ = (max_id > 0) ? (max_id + 1) : 1;
        LogManager::getInstance().Debug("Next occurrence ID set to: " + std::to_string(next_occurrence_id_));
    }
}

// =============================================================================
// 메인 인터페이스 - Delegation to Components
// =============================================================================

std::vector<AlarmEvent> AlarmEngine::evaluateForMessage(const DeviceDataMessage& message) {
    std::vector<AlarmEvent> all_events;
    
    if (!initialized_.load()) return all_events;

    for (const auto& point : message.points) {
        auto events = evaluateForPoint(message.tenant_id, point.point_id, point.value);
        all_events.insert(all_events.end(), events.begin(), events.end());
    }
    
    total_evaluations_.fetch_add(message.points.size());
    return all_events;
}

std::vector<AlarmEvent> AlarmEngine::evaluateForPoint(int tenant_id, int point_id, const DataValue& value) {
    std::vector<AlarmEvent> events;
    if (!initialized_.load()) return events;

    // Check if tenant rules are loaded, if not, load them
    if (!registry_->isTenantLoaded(tenant_id)) {
        LogManager::getInstance().Info("Loading alarm rules for tenant: " + std::to_string(tenant_id));
        registry_->loadRules(tenant_id);
    }

    // Update state cache first
    cache_->updatePointState(point_id, value);

    // Get rules for this point
    auto rules = registry_->getRulesForPoint(tenant_id, point_id);
    
    for (const auto& rule : rules) {
        if (!rule.isEnabled()) continue;

        AlarmEvaluation eval = evaluator_->evaluate(rule, value);
        
        if (eval.state_changed) {
            if (eval.should_trigger) {
                // Raise alarm
                auto occ_id = raiseAlarm(rule, eval, value);
                if (occ_id.has_value()) {
                    AlarmEvent ev;
                    ev.device_id = getDeviceIdForPoint(point_id);
                    ev.point_id = point_id;
                    ev.rule_id = rule.getId();
                    ev.occurrence_id = *occ_id;
                    ev.current_value = value;
                    ev.trigger_value = value;
                    ev.threshold_value = getThresholdValue(rule, eval);
                    ev.trigger_condition = determineTriggerCondition(rule, eval);
                    ev.alarm_type = convertToAlarmType(rule.getAlarmType());
                    ev.severity = rule.getSeverity();
                    ev.message = generateMessage(rule, eval, value);
                    ev.state = AlarmState::ACTIVE;
                    ev.timestamp = std::chrono::system_clock::now();
                    ev.occurrence_time = eval.timestamp;
                    ev.tenant_id = tenant_id;
                    ev.condition_met = true;
                    ev.location = getPointLocation(point_id);
                    
                    events.push_back(ev);
                    alarms_raised_.fetch_add(1);
                }
            } else if (eval.should_clear) {
                // Clear alarm
                if (clearActiveAlarm(rule.getId(), value)) {
                    AlarmEvent ev;
                    ev.device_id = getDeviceIdForPoint(point_id);
                    ev.point_id = point_id;
                    ev.rule_id = rule.getId();
                    ev.current_value = value;
                    ev.alarm_type = convertToAlarmType(rule.getAlarmType());
                    ev.severity = rule.getSeverity();
                    ev.message = "Cleared: " + rule.getName();
                    ev.state = AlarmState::CLEARED;
                    ev.timestamp = std::chrono::system_clock::now();
                    ev.occurrence_time = eval.timestamp;
                    ev.tenant_id = tenant_id;
                    ev.condition_met = false;
                    
                    events.push_back(ev);
                    alarms_cleared_.fetch_add(1);
                }
            }
        }
    }
    
    return events;
}

AlarmEvaluation AlarmEngine::evaluateRule(const AlarmRuleEntity& rule, const DataValue& value) {
    return evaluator_->evaluate(rule, value);
}

// =============================================================================
// 알람 관리 (Persistence)
// =============================================================================

std::optional<int64_t> AlarmEngine::raiseAlarm(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& trigger_value) {
    if (!alarm_occurrence_repo_) return std::nullopt;

    try {
        AlarmOccurrenceEntity occ;
        occ.setRuleId(rule.getId());
        occ.setTenantId(rule.getTenantId());
        occ.setOccurrenceTime(eval.timestamp);
        occ.setState(AlarmState::ACTIVE);
        occ.setSeverity(eval.severity);
        occ.setAlarmMessage(generateMessage(rule, eval, trigger_value));
        
        // Convert trigger_value to JSON
        nlohmann::json trigger_json;
        std::visit([&trigger_json](auto&& v) { trigger_json = v; }, trigger_value);
        occ.setTriggerValue(trigger_json.dump());
        occ.setTriggerCondition(eval.condition_met);

        auto id = alarm_occurrence_repo_->save(occ);
        if (id > 0) {
            cache_->setAlarmStatus(rule.getId(), true, id);
            return id;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to raise alarm: " + std::string(e.what()));
    }
    return std::nullopt;
}

bool AlarmEngine::clearAlarm(int64_t occurrence_id, const DataValue& current_value) {
    if (!alarm_occurrence_repo_) return false;

    try {
        auto occ = alarm_occurrence_repo_->findById(occurrence_id);
        if (occ.has_value()) {
            occ->setClearedTime(std::chrono::system_clock::now());
            occ->setState(AlarmState::CLEARED);
            
            nlohmann::json clear_json;
            std::visit([&clear_json](auto&& v) { clear_json = v; }, current_value);
            occ->setClearedValue(clear_json.dump());

            if (alarm_occurrence_repo_->update(*occ)) {
                cache_->setAlarmStatus(occ->getRuleId(), false, 0);
                return true;
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to clear alarm: " + std::string(e.what()));
    }
    return false;
}

bool AlarmEngine::clearActiveAlarm(int rule_id, const DataValue& current_value) {
    auto status = cache_->getAlarmStatus(rule_id);
    if (status.is_active && status.occurrence_id > 0) {
        return clearAlarm(status.occurrence_id, current_value);
    }
    return false;
}

// =============================================================================
// Helper Methods
// =============================================================================

std::string AlarmEngine::generateMessage(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& value) {
    std::string val_str = std::visit([](auto&& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) return v;
        else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) return v ? "true" : "false";
        else return std::to_string(v);
    }, value);

    return rule.getName() + ": " + eval.condition_met + " (Value: " + val_str + ")";
}

UniqueId AlarmEngine::getDeviceIdForPoint(int point_id) {
    // In a real scenario, this would look up in a cache or DB.
    // For this refactoring, we return a placeholder or dummy.
    return "DEV_" + std::to_string(point_id / 100);
}

std::string AlarmEngine::getPointLocation(int point_id) {
    return "Location for Pt " + std::to_string(point_id);
}

AlarmType AlarmEngine::convertToAlarmType(const AlarmRuleEntity::AlarmType& entity_type) {
    switch (entity_type) {
        case AlarmRuleEntity::AlarmType::ANALOG: return AlarmType::ANALOG;
        case AlarmRuleEntity::AlarmType::DIGITAL: return AlarmType::DIGITAL;
        case AlarmRuleEntity::AlarmType::SCRIPT: return AlarmType::SCRIPT;
        default: return AlarmType::ANALOG;
    }
}

TriggerCondition AlarmEngine::determineTriggerCondition(const AlarmRuleEntity& rule, const AlarmEvaluation& eval) {
    if (eval.condition_met == "HIGH") return TriggerCondition::HIGH;
    if (eval.condition_met == "LOW") return TriggerCondition::LOW;
    if (eval.condition_met == "HIGH_HIGH") return TriggerCondition::HIGH_HIGH;
    if (eval.condition_met == "LOW_LOW") return TriggerCondition::LOW_LOW;
    return TriggerCondition::NONE;
}

double AlarmEngine::getThresholdValue(const AlarmRuleEntity& rule, const AlarmEvaluation& eval) {
    if (eval.condition_met == "HIGH") return rule.getHighLimit().value_or(0.0);
    if (eval.condition_met == "LOW") return rule.getLowLimit().value_or(0.0);
    if (eval.condition_met == "HIGH_HIGH") return rule.getHighHighLimit().value_or(0.0);
    if (eval.condition_met == "LOW_LOW") return rule.getLowLowLimit().value_or(0.0);
    return 0.0;
}

size_t AlarmEngine::getActiveAlarmsCount() const {
    if (!alarm_occurrence_repo_) return 0;
    return alarm_occurrence_repo_->findActive().size();
}

void AlarmEngine::SeedPointValue(int point_id, const DataValue& value) {
    if (cache_) cache_->updatePointState(point_id, value);
}

// Redirecting legacy header methods to dummy/empty or minimal impl if still used
bool AlarmEngine::initScriptEngine() { return true; }
void AlarmEngine::cleanupScriptEngine() {}
bool AlarmEngine::registerSystemFunctions() { return true; }
AlarmEvaluation AlarmEngine::evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value) { return AlarmEvaluation(); }
AlarmEvaluation AlarmEngine::evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool value) { return AlarmEvaluation(); }
AlarmEvaluation AlarmEngine::evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context) { return AlarmEvaluation(); }

} // namespace Alarm
} // namespace PulseOne