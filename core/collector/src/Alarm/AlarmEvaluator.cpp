#include "Alarm/AlarmEvaluator.h"
#include "Alarm/AlarmStateCache.h"
#include "Logging/LogManager.h"
#include <variant>

namespace PulseOne {
namespace Alarm {

AlarmEvaluator::AlarmEvaluator(PulseOne::Scripting::ScriptExecutor &executor,
                               AlarmStateCache &state_cache)
    : executor_(executor), state_cache_(state_cache) {}

AlarmEvaluation
AlarmEvaluator::evaluate(const Database::Entities::AlarmRuleEntity &rule,
                         const PulseOne::Structs::DataValue &value) {
  // DataValue(variant)에서 double 값 추출
  double dbl_value = std::visit(
      [](auto &&arg) -> double {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
          try {
            return std::stod(arg);
          } catch (...) {
            return 0.0;
          }
        } else if constexpr (std::is_same_v<T, bool>) {
          return arg ? 1.0 : 0.0;
        } else if constexpr (std::is_arithmetic_v<T>) {
          return static_cast<double>(arg);
        }
        return 0.0;
      },
      value);

  // ✅ Entry Debug Log
  if (rule.getId() == 202) {
    LogManager::getInstance().Debug(
        "[AlarmEvaluator] Evaluating Rule 202 - Extracted Value: " +
        std::to_string(dbl_value) + ", AlarmType: " +
        std::to_string(static_cast<int>(rule.getAlarmType())));
  }

  if (rule.getAlarmType() ==
      Database::Entities::AlarmRuleEntity::AlarmType::ANALOG) {
    return evaluateAnalog(rule, dbl_value);
  } else if (rule.getAlarmType() ==
             Database::Entities::AlarmRuleEntity::AlarmType::DIGITAL) {
    bool bool_value = (dbl_value != 0.0);
    return evaluateDigital(rule, bool_value);
  } else if (rule.getAlarmType() ==
             Database::Entities::AlarmRuleEntity::AlarmType::SCRIPT) {
    nlohmann::json context;
    // context 주입 로직 필요 (예: {"value": dbl_value})
    context["value"] = dbl_value;
    return evaluateScript(rule, context);
  }

  AlarmEvaluation eval;
  eval.rule_id = rule.getId();
  return eval;
}

AlarmEvaluation
AlarmEvaluator::evaluateAnalog(const Database::Entities::AlarmRuleEntity &rule,
                               double value) {
  AlarmEvaluation eval;
  eval.rule_id = rule.getId();
  eval.tenant_id = rule.getTenantId();
  eval.timestamp = std::chrono::system_clock::now();

  bool triggered = false;
  double hh_limit = rule.getHighHighLimit().value_or(1e18);
  double h_limit = rule.getHighLimit().value_or(1e18);
  double l_limit = rule.getLowLimit().value_or(-1e18);
  double ll_limit = rule.getLowLowLimit().value_or(-1e18);

  if (value >= hh_limit) {
    triggered = true;
    eval.condition_met = "HIGH_HIGH";
  } else if (value >= h_limit) {
    triggered = true;
    eval.condition_met = "HIGH";
  } else if (value <= ll_limit) {
    triggered = true;
    eval.condition_met = "LOW_LOW";
  } else if (value <= l_limit) {
    triggered = true;
    eval.condition_met = "LOW";
  }

  auto status = state_cache_.getAlarmStatus(rule.getId());

  // ✅ Debug Log 추가
  if (rule.getId() == 202 || triggered) {
    LogManager::getInstance().Debug(
        "[AlarmEvaluator] Rule " + std::to_string(rule.getId()) +
        " Evaluation - Value: " + std::to_string(value) + ", Triggered: " +
        (triggered ? "TRUE" : "FALSE") + ", Condition: " + eval.condition_met +
        ", CacheActive: " + (status.is_active ? "TRUE" : "FALSE") +
        ", OccurrenceID: " + std::to_string(status.occurrence_id));
  }

  if (triggered && !status.is_active) {
    eval.should_trigger = true;
    eval.state_changed = true;
  } else if (!triggered && status.is_active) {
    eval.should_clear = true;
    eval.state_changed = true;
  }

  eval.severity = rule.getSeverity();
  return eval;
}

AlarmEvaluation
AlarmEvaluator::evaluateDigital(const Database::Entities::AlarmRuleEntity &rule,
                                bool value) {
  AlarmEvaluation eval;
  eval.rule_id = rule.getId();
  eval.tenant_id = rule.getTenantId();
  eval.timestamp = std::chrono::system_clock::now();

  bool triggered = (value == true);

  auto status = state_cache_.getAlarmStatus(rule.getId());

  // ✅ Debug Log 추가
  if (rule.getId() == 8 || triggered) {
    LogManager::getInstance().Debug(
        "[AlarmEvaluator] Rule " + std::to_string(rule.getId()) +
        " Evaluation - Value: " + (value ? "TRUE" : "FALSE") +
        ", Triggered: " + (triggered ? "TRUE" : "FALSE") +
        ", CacheActive: " + (status.is_active ? "TRUE" : "FALSE") +
        ", OccurrenceID: " + std::to_string(status.occurrence_id));
  }

  if (triggered && !status.is_active) {
    eval.should_trigger = true;
    eval.state_changed = true;
  } else if (!triggered && status.is_active) {
    eval.should_clear = true;
    eval.state_changed = true;
  }

  eval.severity = rule.getSeverity();
  return eval;
}

AlarmEvaluation
AlarmEvaluator::evaluateScript(const Database::Entities::AlarmRuleEntity &rule,
                               const nlohmann::json &context) {
  AlarmEvaluation eval;
  eval.rule_id = rule.getId();
  eval.tenant_id = rule.getTenantId();
  eval.timestamp = std::chrono::system_clock::now();

  PulseOne::Scripting::ScriptContext s_ctx;
  s_ctx.id = rule.getId();
  s_ctx.tenant_id = rule.getTenantId();
  s_ctx.script = rule.getConditionScript();
  s_ctx.inputs = &context;

  auto res = executor_.executeSafe(s_ctx);

  bool triggered = false;
  if (res.success) {
    // DataVariant(variant) 결과 처리
    std::visit(
        [&triggered](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, bool>) {
            triggered = arg;
          } else if constexpr (std::is_arithmetic_v<T>) {
            triggered = (static_cast<double>(arg) != 0.0);
          }
        },
        res.value);
  }

  auto status = state_cache_.getAlarmStatus(rule.getId());
  if (triggered && !status.is_active) {
    eval.should_trigger = true;
    eval.state_changed = true;
  } else if (!triggered && status.is_active) {
    eval.should_clear = true;
    eval.state_changed = true;
  }

  eval.severity = rule.getSeverity();
  return eval;
}

} // namespace Alarm
} // namespace PulseOne
