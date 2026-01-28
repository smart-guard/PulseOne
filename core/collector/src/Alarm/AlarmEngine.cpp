// =============================================================================
// collector/src/Alarm/AlarmEngine.cpp - Component-based Architecture
// =============================================================================

#include "Alarm/AlarmEngine.h"
#include "Alarm/AlarmEvaluator.h"
#include "Alarm/AlarmRuleRegistry.h"
#include "Alarm/AlarmStateCache.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <variant>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 싱글톤 및 생성자/소멸자
// =============================================================================

AlarmEngine &AlarmEngine::getInstance() {
  static AlarmEngine instance;
  return instance;
}

AlarmEngine::AlarmEngine() {
  LogManager::getInstance().Debug(
      "AlarmEngine (Component-based) initializing...");

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
    LogManager::getInstance().Info(
        "AlarmEngine initialized successfully with component architecture");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("AlarmEngine initialization failed: " +
                                    std::string(e.what()));
    initialized_ = false;
  }
}

AlarmEngine::~AlarmEngine() { shutdown(); }

void AlarmEngine::shutdown() {
  if (!initialized_.load())
    return;

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
  auto &repo_factory = Database::RepositoryFactory::getInstance();
  alarm_rule_repo_ = repo_factory.getAlarmRuleRepository();
  alarm_occurrence_repo_ = repo_factory.getAlarmOccurrenceRepository();
  data_point_repo_ = repo_factory.getDataPointRepository();

  if (!alarm_rule_repo_ || !alarm_occurrence_repo_ || !data_point_repo_) {
    LogManager::getInstance().Error(
        "Failed to initialize necessary repositories for AlarmEngine");
  }
}

void AlarmEngine::loadInitialData() {
  if (alarm_occurrence_repo_) {
    // 1. Max ID 설정
    int max_id = alarm_occurrence_repo_->findMaxId();
    next_occurrence_id_ = (max_id > 0) ? (max_id + 1) : 1;
    LogManager::getInstance().Debug("Next occurrence ID set to: " +
                                    std::to_string(next_occurrence_id_.load()));

    // 2. 현재 활성 알람들을 캐시에 로드하여 중복 생성 방지 (Startup Recovery)
    if (cache_) {
      auto active_alarms = alarm_occurrence_repo_->findActive();
      for (const auto &alarm : active_alarms) {
        cache_->setAlarmStatus(alarm.getRuleId(), true, alarm.getId());
      }
      LogManager::getInstance().Info(
          "AlarmEngine: Startup recovery complete. " +
          std::to_string(active_alarms.size()) + " active alarms restored.");
    }
  }
}

// =============================================================================
// 메인 인터페이스 - Delegation to Components
// =============================================================================

std::vector<AlarmEvent>
AlarmEngine::evaluateForMessage(const DeviceDataMessage &message) {
  std::vector<AlarmEvent> events;

  if (!initialized_.load())
    return events;

  if (message.points.empty())
    return events;

  // 테넌트 규칙이 로드되지 않았으면 로드
  if (!registry_->isTenantLoaded(message.tenant_id)) {
    LogManager::getInstance().Info("AlarmEngine: Loading rules for tenant " +
                                       std::to_string(message.tenant_id),
                                   "AlarmEngine");
    registry_->loadRules(
        message.tenant_id); // Assuming loadTenantRules is a wrapper for this
  }

  LogManager::getInstance().Debug(
      "AlarmEngine: Evaluating message for tenant " +
      std::to_string(message.tenant_id) + " with " +
      std::to_string(message.points.size()) + " points");

  for (const auto &point : message.points) {
    auto point_events = evaluateForPoint(message.tenant_id, point);
    events.insert(events.end(), point_events.begin(), point_events.end());
  }

  total_evaluations_.fetch_add(message.points.size());
  return events;
}

std::vector<AlarmEvent>
AlarmEngine::evaluateForPoint(int tenant_id, const TimestampedValue &tv) {
  std::vector<AlarmEvent> events;
  if (!initialized_.load())
    return events;

  // Check if tenant rules are loaded, if not, load them
  if (!registry_->isTenantLoaded(tenant_id)) {
    LogManager::getInstance().Info("Loading alarm rules for tenant: " +
                                   std::to_string(tenant_id));
    registry_->loadRules(tenant_id);
  }

  // Update state cache first
  cache_->updatePointState(tv.point_id, tv.value);

  // Get rules for this point
  auto rules = registry_->getRulesForPoint(tenant_id, tv.point_id);

  std::string val_str;
  std::visit(
      [&val_str](auto &&v) {
        std::ostringstream oss;
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          oss << (v ? "true" : "false");
        } else if constexpr (std::is_floating_point_v<T>) {
          oss << std::fixed << std::setprecision(6) << v;
        } else {
          oss << v;
        }
        val_str = oss.str();
      },
      tv.value);

  LogManager::getInstance().log(
      "alarm", PulseOne::Enums::LogLevel::DEBUG,
      "AlarmEngine: Evaluating Point " + std::to_string(tv.point_id) +
          " Value: " + val_str + " Quality: " +
          (tv.quality == Structs::DataQuality::GOOD ? "GOOD" : "BAD") +
          " Rules Found: " + std::to_string(rules.size()));

  if (rules.empty()) {
    if (tv.point_id == 43) {
      LogManager::getInstance().Debug("AlarmEngine: No rules found for point " +
                                      std::to_string(tv.point_id) +
                                      " in tenant " +
                                      std::to_string(tenant_id));
    }
  }

  for (const auto &rule : rules) {
    if (!rule.isEnabled())
      continue;

    AlarmEvaluation eval = evaluator_->evaluate(rule, tv.value);

    if (eval.state_changed) {
      if (eval.should_trigger) {
        // Raise alarm
        auto occ_id = raiseAlarm(rule, eval, tv.value);
        if (occ_id.has_value()) {
          AlarmEvent ev;
          ev.device_id = getDeviceIdForPoint(tv.point_id);
          ev.point_id = tv.point_id;
          ev.rule_id = rule.getId();
          ev.occurrence_id = *occ_id;
          ev.current_value = tv.value;
          ev.trigger_value = tv.value;
          ev.threshold_value = getThresholdValue(rule, eval);
          ev.trigger_condition = determineTriggerCondition(rule, eval);
          ev.alarm_type = convertToAlarmType(rule.getAlarmType());
          ev.severity = rule.getSeverity();
          ev.message = generateMessage(rule, eval, tv.value);
          ev.state = AlarmState::ACTIVE;
          ev.timestamp = std::chrono::system_clock::now();
          ev.occurrence_time = eval.timestamp;
          ev.tenant_id = tenant_id;
          ev.condition_met = true;
          ev.location = getPointLocation(tv.point_id);

          events.push_back(ev);
          alarms_raised_.fetch_add(1);
        }
      } else if (eval.should_clear) {
        // Clear alarm
        if (clearActiveAlarm(rule.getId(), tv.value)) {
          AlarmEvent ev;
          ev.device_id = getDeviceIdForPoint(tv.point_id);
          ev.point_id = tv.point_id;
          ev.rule_id = rule.getId();
          ev.current_value = tv.value;
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
    } else {
      // 상태는 변하지 않았지만, 이미 활성 상태라면 DB 값 업데이트 (실시간 변동
      // 반영)
      auto status = cache_->getAlarmStatus(rule.getId());
      if (status.is_active && status.occurrence_id > 0) {
        // 필요 시 별도 메서드 호출 가능 (여기서는 간략화)
        updateActiveAlarmValue(rule, status.occurrence_id, tv.value);
      }
    }
  }

  return events;
}

AlarmEvaluation AlarmEngine::evaluateRule(const AlarmRuleEntity &rule,
                                          const DataValue &value) {
  return evaluator_->evaluate(rule, value);
}

// =============================================================================
// 알람 관리 (Persistence)
// =============================================================================

std::optional<int64_t> AlarmEngine::raiseAlarm(const AlarmRuleEntity &rule,
                                               const AlarmEvaluation &eval,
                                               const DataValue &trigger_value) {
  if (!alarm_occurrence_repo_)
    return std::nullopt;

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
    std::visit([&trigger_json](auto &&v) { trigger_json = v; }, trigger_value);
    occ.setTriggerValue(trigger_json.dump());
    occ.setTriggerCondition(eval.condition_met);

    // Populate device and point information
    int point_id = rule.getTargetId().value_or(0);
    occ.setPointId(point_id);
    occ.setDeviceId(getDeviceIdForPoint(point_id));

    auto id = alarm_occurrence_repo_->save(occ);
    if (id > 0) {
      cache_->setAlarmStatus(rule.getId(), true, id);
      return id;
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to raise alarm: " +
                                    std::string(e.what()));
  }
  return std::nullopt;
}

bool AlarmEngine::updateActiveAlarmValue(const AlarmRuleEntity &rule,
                                         int64_t occurrence_id,
                                         const DataValue &value) {
  if (!alarm_occurrence_repo_)
    return false;

  // 1. 기존 알람 조회
  auto alarm_opt =
      alarm_occurrence_repo_->findById(static_cast<int>(occurrence_id));
  if (!alarm_opt)
    return false;

  auto &alarm = *alarm_opt;

  // 2. 값 문자열 변환
  std::string val_str;
  std::visit(
      [&val_str](auto &&v) {
        std::ostringstream oss;
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          oss << (v ? "true" : "false");
        } else if constexpr (std::is_floating_point_v<T>) {
          oss << std::fixed << std::setprecision(6) << v;
        } else {
          oss << v;
        }
        val_str = oss.str();
      },
      value);

  // 3. 변경 사항 확인 (값이 이전과 동일하면 DB 업데이트 스킵하여 성능 최적화)
  if (alarm.getTriggerValue() == val_str)
    return true;

  // 4. 정보 업데이트
  alarm.setTriggerValue(val_str);

  // 다시 평가하여 상태 메시지 갱신
  auto eval = evaluator_->evaluate(rule, value);
  alarm.setAlarmMessage(generateMessage(rule, eval, value));
  alarm.setTriggerCondition(eval.condition_met);

  return alarm_occurrence_repo_->update(alarm);
}

bool AlarmEngine::clearAlarm(int64_t occurrence_id,
                             const DataValue &current_value) {
  if (!alarm_occurrence_repo_)
    return false;

  try {
    auto occ = alarm_occurrence_repo_->findById(occurrence_id);
    if (occ.has_value()) {
      occ->setClearedTime(std::chrono::system_clock::now());
      occ->setState(AlarmState::CLEARED);

      nlohmann::json clear_json;
      std::visit([&clear_json](auto &&v) { clear_json = v; }, current_value);
      occ->setClearedValue(clear_json.dump());

      if (alarm_occurrence_repo_->update(*occ)) {
        cache_->setAlarmStatus(occ->getRuleId(), false, 0);
        return true;
      }
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Failed to clear alarm: " +
                                    std::string(e.what()));
  }
  return false;
}

bool AlarmEngine::clearActiveAlarm(int rule_id,
                                   const DataValue &current_value) {
  auto status = cache_->getAlarmStatus(rule_id);
  if (status.is_active && status.occurrence_id > 0) {
    return clearAlarm(status.occurrence_id, current_value);
  }
  return false;
}

// =============================================================================
// Helper Methods
// =============================================================================

std::string AlarmEngine::generateMessage(const AlarmRuleEntity &rule,
                                         const AlarmEvaluation &eval,
                                         const DataValue &value) {
  std::ostringstream oss;
  std::visit(
      [&oss](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_arithmetic_v<T>) {
          if constexpr (std::is_same_v<T, bool>) {
            oss << (v ? "true" : "false");
          } else {
            oss << std::fixed << std::setprecision(2) << static_cast<double>(v);
          }
        } else if constexpr (std::is_same_v<T, std::string>) {
          oss << v;
        } else {
          oss << "Unknown Value";
        }
      },
      value);

  return rule.getName() + ": " + eval.condition_met + " (Value: " + oss.str() +
         ")";
}

int AlarmEngine::getDeviceIdForPoint(int point_id) {
  if (point_id <= 0)
    return 0;

  {
    std::shared_lock<std::shared_mutex> lock(device_cache_mutex_);
    auto it = point_device_cache_.find(point_id);
    if (it != point_device_cache_.end()) {
      return it->second;
    }
  }

  if (!data_point_repo_)
    return 0;

  try {
    auto point = data_point_repo_->findById(point_id);
    if (point.has_value()) {
      int device_id = point->getDeviceId();
      std::unique_lock<std::shared_mutex> lock(device_cache_mutex_);
      point_device_cache_[point_id] = device_id;
      return device_id;
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Error getting device ID for point " +
                                    std::to_string(point_id) + ": " + e.what());
  }

  return 0;
}

std::string AlarmEngine::getPointLocation(int point_id) {
  return "Location for Pt " + std::to_string(point_id);
}

AlarmType
AlarmEngine::convertToAlarmType(const AlarmRuleEntity::AlarmType &entity_type) {
  switch (entity_type) {
  case AlarmRuleEntity::AlarmType::ANALOG:
    return AlarmType::ANALOG;
  case AlarmRuleEntity::AlarmType::DIGITAL:
    return AlarmType::DIGITAL;
  case AlarmRuleEntity::AlarmType::SCRIPT:
    return AlarmType::SCRIPT;
  default:
    return AlarmType::ANALOG;
  }
}

TriggerCondition
AlarmEngine::determineTriggerCondition(const AlarmRuleEntity &rule,
                                       const AlarmEvaluation &eval) {
  if (eval.condition_met == "HIGH")
    return TriggerCondition::HIGH;
  if (eval.condition_met == "LOW")
    return TriggerCondition::LOW;
  if (eval.condition_met == "HIGH_HIGH")
    return TriggerCondition::HIGH_HIGH;
  if (eval.condition_met == "LOW_LOW")
    return TriggerCondition::LOW_LOW;
  return TriggerCondition::NONE;
}

double AlarmEngine::getThresholdValue(const AlarmRuleEntity &rule,
                                      const AlarmEvaluation &eval) {
  if (eval.condition_met == "HIGH")
    return rule.getHighLimit().value_or(0.0);
  if (eval.condition_met == "LOW")
    return rule.getLowLimit().value_or(0.0);
  if (eval.condition_met == "HIGH_HIGH")
    return rule.getHighHighLimit().value_or(0.0);
  if (eval.condition_met == "LOW_LOW")
    return rule.getLowLowLimit().value_or(0.0);
  return 0.0;
}

size_t AlarmEngine::getActiveAlarmsCount() const {
  if (!alarm_occurrence_repo_)
    return 0;
  return alarm_occurrence_repo_->findActive().size();
}

void AlarmEngine::SeedPointValue(int point_id, const DataValue &value) {
  if (cache_)
    cache_->updatePointState(point_id, value);
}

// Redirecting legacy header methods to dummy/empty or minimal impl if still
// used
bool AlarmEngine::initScriptEngine() { return true; }
void AlarmEngine::cleanupScriptEngine() {}
bool AlarmEngine::registerSystemFunctions() { return true; }
AlarmEvaluation AlarmEngine::evaluateAnalogAlarm(const AlarmRuleEntity &rule,
                                                 double value) {
  return AlarmEvaluation();
}
AlarmEvaluation AlarmEngine::evaluateDigitalAlarm(const AlarmRuleEntity &rule,
                                                  bool value) {
  return AlarmEvaluation();
}
AlarmEvaluation
AlarmEngine::evaluateScriptAlarm(const AlarmRuleEntity &rule,
                                 const nlohmann::json &context) {
  return AlarmEvaluation();
}

} // namespace Alarm
} // namespace PulseOne