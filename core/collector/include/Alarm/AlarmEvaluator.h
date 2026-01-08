#ifndef ALARM_EVALUATOR_H
#define ALARM_EVALUATOR_H

#include <memory>
#include <nlohmann/json.hpp>
#include "Alarm/AlarmTypes.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Scripting/ScriptExecutor.h"
#include "Alarm/AlarmStateCache.h"

namespace PulseOne {
namespace Alarm {

class AlarmEvaluator {
public:
    AlarmEvaluator(PulseOne::Scripting::ScriptExecutor& executor, AlarmStateCache& state_cache);

    AlarmEvaluation evaluate(const Database::Entities::AlarmRuleEntity& rule, 
                             const PulseOne::Structs::DataValue& value);

private:
    AlarmEvaluation evaluateAnalog(const Database::Entities::AlarmRuleEntity& rule, double value);
    AlarmEvaluation evaluateDigital(const Database::Entities::AlarmRuleEntity& rule, bool value);
    AlarmEvaluation evaluateScript(const Database::Entities::AlarmRuleEntity& rule, const nlohmann::json& context);

    PulseOne::Scripting::ScriptExecutor& executor_;
    AlarmStateCache& state_cache_;
};

} // namespace Alarm
} // namespace PulseOne

#endif