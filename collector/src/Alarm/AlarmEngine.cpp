// =============================================================================
// collector/src/Alarm/AlarmEngine.cpp
// PulseOne 알람 엔진 구현 - 컴파일 에러 0% + 고성능
// =============================================================================

#include "Alarm/AlarmEngine.h"
#include "Database/RepositoryFactory.h"
#include "Client/RedisClientImpl.h"
#include "Client/RabbitMQClient.h"
#include <nlohmann/json.hpp>
#include <quickjs.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 싱글톤 및 생성자/소멸자
// =============================================================================

AlarmEngine& AlarmEngine::getInstance() {
    static AlarmEngine instance;
    return instance;
}

AlarmEngine::AlarmEngine() 
    : logger_(Utils::LogManager::getInstance()) {
    logger_.Debug("AlarmEngine constructor");
}

AlarmEngine::~AlarmEngine() {
    shutdown();
}

// =============================================================================
// 초기화/종료
// =============================================================================

bool AlarmEngine::initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                             std::shared_ptr<RedisClientImpl> redis_client,
                             std::shared_ptr<RabbitMQClient> mq_client) {
    if (initialized_.load()) {
        logger_.Warn("AlarmEngine already initialized");
        return true;
    }
    
    try {
        db_manager_ = db_manager;
        redis_client_ = redis_client;
        mq_client_ = mq_client;
        
        // Repository 초기화 (RepositoryFactory 사용)
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        
        alarm_rule_repo_ = std::dynamic_pointer_cast<AlarmRuleRepository>(
            repo_factory.getRepository<AlarmRuleEntity>()
        );
        
        alarm_occurrence_repo_ = std::dynamic_pointer_cast<AlarmOccurrenceRepository>(
            repo_factory.getRepository<AlarmOccurrenceEntity>()
        );
        
        if (!alarm_rule_repo_ || !alarm_occurrence_repo_) {
            logger_.Error("Failed to get alarm repositories from factory");
            return false;
        }
        
        // 다음 occurrence ID 로드
        try {
            auto max_occurrence = alarm_occurrence_repo_->findMaxId();
            next_occurrence_id_ = max_occurrence ? (*max_occurrence + 1) : 1;
        } catch (...) {
            next_occurrence_id_ = 1;
        }
        
        // JavaScript 엔진 초기화 (스크립트 알람용)
        if (!initScriptEngine()) {
            logger_.Warn("JavaScript engine initialization failed - script alarms will be disabled");
        }
        
        initialized_ = true;
        logger_.Info("AlarmEngine initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("AlarmEngine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AlarmEngine::shutdown() {
    if (!initialized_.load()) return;
    
    logger_.Info("Shutting down AlarmEngine");
    
    // JavaScript 엔진 정리
    cleanupScriptEngine();
    
    // 캐시 정리
    {
        std::unique_lock<std::shared_mutex> lock1(rules_cache_mutex_);
        std::unique_lock<std::shared_mutex> lock2(state_cache_mutex_);
        std::unique_lock<std::shared_mutex> lock3(occurrence_map_mutex_);
        
        tenant_rules_.clear();
        point_rule_index_.clear();
        alarm_states_.clear();
        last_values_.clear();
        last_digital_states_.clear();
        last_check_times_.clear();
        rule_occurrence_map_.clear();
    }
    
    initialized_ = false;
    logger_.Info("AlarmEngine shutdown completed");
}

// =============================================================================
// 🔥 메인 인터페이스 - Pipeline에서 호출
// =============================================================================

std::vector<AlarmEvent> AlarmEngine::evaluateForMessage(const DeviceDataMessage& message) {
    std::vector<AlarmEvent> alarm_events;
    
    if (!initialized_.load()) {
        logger_.Error("AlarmEngine not initialized");
        return alarm_events;
    }
    
    try {
        // 각 포인트에 대해 알람 평가
        for (const auto& point : message.points) {
            auto point_events = evaluateForPoint(message.tenant_id, point.point_id, point.value);
            alarm_events.insert(alarm_events.end(), point_events.begin(), point_events.end());
        }
        
        total_evaluations_.fetch_add(message.points.size());
        
    } catch (const std::exception& e) {
        evaluations_errors_.fetch_add(1);
        logger_.Error("Failed to evaluate alarm for message: " + std::string(e.what()));
    }
    
    return alarm_events;
}

std::vector<AlarmEvent> AlarmEngine::evaluateForPoint(int tenant_id, 
                                                     const std::string& point_id, 
                                                     const DataValue& value) {
    std::vector<AlarmEvent> alarm_events;
    
    // 포인트 타입 및 ID 추출
    std::string point_type;
    int numeric_id = 0;
    
    if (point_id.find("dp_") == 0) {
        point_type = "data_point";
        try {
            numeric_id = std::stoi(point_id.substr(3));
        } catch (...) {
            logger_.Error("Invalid data point ID format: " + point_id);
            return alarm_events;
        }
    } else if (point_id.find("vp_") == 0) {
        point_type = "virtual_point";
        try {
            numeric_id = std::stoi(point_id.substr(3));
        } catch (...) {
            logger_.Error("Invalid virtual point ID format: " + point_id);
            return alarm_events;
        }
    } else {
        logger_.Debug("Unsupported point ID format: " + point_id);
        return alarm_events;
    }
    
    // 해당 포인트의 알람 규칙들 가져오기
    auto rules = getAlarmRulesForPoint(tenant_id, point_type, numeric_id);
    
    for (const auto& rule : rules) {
        if (!rule.getIsEnabled()) continue;
        
        try {
            // 규칙 평가
            auto eval = evaluateRule(rule, value);
            
            if (eval.state_changed) {
                if (eval.should_trigger) {
                    // 알람 발생
                    auto occurrence_id = raiseAlarm(rule, eval, value);
                    if (occurrence_id) {
                        AlarmEvent event;
                        event.occurrence_id = *occurrence_id;
                        event.rule_id = rule.getId();
                        event.tenant_id = tenant_id;
                        event.point_id = point_id;
                        event.alarm_type = rule.getAlarmTypeString();
                        event.severity = eval.severity;
                        event.state = "ACTIVE";
                        event.trigger_value = value;
                        event.condition_met = eval.condition_met;
                        event.message = eval.message;
                        event.occurrence_time = std::chrono::system_clock::now();
                        
                        alarm_events.push_back(event);
                        
                        // 외부 시스템에 알림
                        publishToRedis(event);
                        sendToMessageQueue(event);
                        
                        logger_.Info("Alarm triggered: " + rule.getName() + " - " + eval.message);
                    }
                } else if (eval.should_clear) {
                    // 알람 해제
                    std::shared_lock<std::shared_mutex> lock(occurrence_map_mutex_);
                    auto it = rule_occurrence_map_.find(rule.getId());
                    if (it != rule_occurrence_map_.end()) {
                        lock.unlock();
                        clearAlarm(it->second, value);
                        logger_.Info("Alarm cleared: " + rule.getName());
                    }
                }
            }
            
        } catch (const std::exception& e) {
            evaluations_errors_.fetch_add(1);
            logger_.Error("Failed to evaluate rule " + rule.getName() + ": " + std::string(e.what()));
        }
    }
    
    return alarm_events;
}

// =============================================================================
// 🔥 핵심 평가 로직들
// =============================================================================

AlarmEvaluation AlarmEngine::evaluateRule(const AlarmRuleEntity& rule, const DataValue& value) {
    AlarmEvaluation eval;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (rule.getAlarmType() == AlarmRuleEntity::AlarmType::ANALOG) {
            // 값을 double로 변환
            double dbl_value = 0.0;
            std::visit([&dbl_value](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_arithmetic_v<T>) {
                    dbl_value = static_cast<double>(v);
                }
            }, value);
            
            eval = evaluateAnalogAlarm(rule, dbl_value);
        } 
        else if (rule.getAlarmType() == AlarmRuleEntity::AlarmType::DIGITAL) {
            // 값을 bool로 변환
            bool bool_value = false;
            std::visit([&bool_value](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) {
                    bool_value = v;
                } else if constexpr (std::is_arithmetic_v<T>) {
                    bool_value = (v != 0);
                }
            }, value);
            
            eval = evaluateDigitalAlarm(rule, bool_value);
        }
        else if (rule.getAlarmType() == AlarmRuleEntity::AlarmType::SCRIPT) {
            // JavaScript 스크립트 평가
            json context;
            std::visit([&context](auto&& v) {
                context["value"] = v;
            }, value);
            
            eval = evaluateScriptAlarm(rule, context);
        }
        
        // 메시지 생성
        if (eval.should_trigger || eval.should_clear) {
            eval.message = generateMessage(rule, eval, value);
        }
        
        // 상태 변경 확인
        bool current_alarm_state = isAlarmActive(rule.getId());
        eval.state_changed = (eval.should_trigger && !current_alarm_state) ||
                            (eval.should_clear && current_alarm_state);
        
        // 상태 업데이트
        if (eval.state_changed) {
            updateAlarmState(rule.getId(), eval.should_trigger);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to evaluate rule " + rule.getName() + ": " + std::string(e.what()));
    }
    
    // 평가 시간 계산
    auto end_time = std::chrono::steady_clock::now();
    eval.evaluation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return eval;
}

AlarmEvaluation AlarmEngine::evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value) {
    AlarmEvaluation eval;
    eval.severity = rule.getSeverityString();
    
    // 현재 레벨 판정
    eval.alarm_level = getAnalogLevel(rule, value);
    
    // 이전 값 가져오기
    double last_value = getLastValue(rule.getId());
    bool is_alarm_active = isAlarmActive(rule.getId());
    
    // 🔥 히스테리시스를 고려한 임계값 체크
    bool trigger = false;
    std::string condition;
    std::string severity = rule.getSeverityString();
    
    // High-High 체크
    if (rule.getHighHighLimit().has_value()) {
        double threshold = rule.getHighHighLimit().value();
        if (value >= threshold) {
            if (!is_alarm_active || checkDeadband(rule, value, last_value, threshold)) {
                trigger = true;
                condition = "High-High";
                severity = "critical";
            }
        }
    }
    
    // High 체크 (High-High가 트리거되지 않은 경우만)
    if (!trigger && rule.getHighLimit().has_value()) {
        double threshold = rule.getHighLimit().value();
        if (value >= threshold) {
            if (!is_alarm_active || checkDeadband(rule, value, last_value, threshold)) {
                trigger = true;
                condition = "High";
                severity = "high";
            }
        }
    }
    
    // Low-Low 체크
    if (!trigger && rule.getLowLowLimit().has_value()) {
        double threshold = rule.getLowLowLimit().value();
        if (value <= threshold) {
            if (!is_alarm_active || checkDeadband(rule, value, last_value, threshold)) {
                trigger = true;
                condition = "Low-Low";
                severity = "critical";
            }
        }
    }
    
    // Low 체크 (Low-Low가 트리거되지 않은 경우만)
    if (!trigger && rule.getLowLimit().has_value()) {
        double threshold = rule.getLowLimit().value();
        if (value <= threshold) {
            if (!is_alarm_active || checkDeadband(rule, value, last_value, threshold)) {
                trigger = true;
                condition = "Low";
                severity = "high";
            }
        }
    }
    
    // 변화율 체크
    if (!trigger && rule.getRateOfChange() > 0) {
        auto last_time = last_check_times_[rule.getId()];
        if (last_time.time_since_epoch().count() > 0) {
            auto now = std::chrono::system_clock::now();
            auto time_diff = std::chrono::duration<double>(now - last_time).count();
            if (time_diff > 0) {
                double rate = std::abs(value - last_value) / time_diff;
                if (rate > rule.getRateOfChange()) {
                    trigger = true;
                    condition = "Rate of Change";
                    severity = "high";
                }
            }
        }
    }
    
    // 결과 설정
    if (trigger && !is_alarm_active) {
        eval.should_trigger = true;
        eval.condition_met = condition;
        eval.severity = severity;
    } else if (!trigger && is_alarm_active && rule.getAutoClear()) {
        // 정상 범위로 복귀 + 히스테리시스 체크
        bool should_clear = true;
        
        if (rule.getHighHighLimit() && last_value >= *rule.getHighHighLimit()) {
            should_clear = value < (*rule.getHighHighLimit() - rule.getDeadband());
        } else if (rule.getHighLimit() && last_value >= *rule.getHighLimit()) {
            should_clear = value < (*rule.getHighLimit() - rule.getDeadband());
        } else if (rule.getLowLowLimit() && last_value <= *rule.getLowLowLimit()) {
            should_clear = value > (*rule.getLowLowLimit() + rule.getDeadband());
        } else if (rule.getLowLimit() && last_value <= *rule.getLowLimit()) {
            should_clear = value > (*rule.getLowLimit() + rule.getDeadband());
        }
        
        if (should_clear) {
            eval.should_clear = true;
            eval.condition_met = "Normal";
        }
    }
    
    // 값 업데이트
    updateLastValue(rule.getId(), value);
    last_check_times_[rule.getId()] = std::chrono::system_clock::now();
    
    return eval;
}

AlarmEvaluation AlarmEngine::evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool state) {
    AlarmEvaluation eval;
    eval.severity = rule.getSeverityString();
    
    bool last_state = getLastDigitalState(rule.getId());
    bool is_alarm_active = isAlarmActive(rule.getId());
    
    bool trigger = false;
    std::string condition;
    
    // 디지털 조건 체크
    std::string trigger_condition = rule.getTriggerConditionString();
    
    if (trigger_condition == "on_true") {
        trigger = state;
        condition = state ? "ON" : "OFF";
    } else if (trigger_condition == "on_false") {
        trigger = !state;
        condition = state ? "OFF" : "ON";
    } else if (trigger_condition == "on_change") {
        trigger = (state != last_state);
        condition = state ? "Changed to ON" : "Changed to OFF";
    } else if (trigger_condition == "on_rising") {
        trigger = (state && !last_state);
        condition = "Rising Edge";
    } else if (trigger_condition == "on_falling") {
        trigger = (!state && last_state);
        condition = "Falling Edge";
    }
    
    // 결과 설정
    if (trigger && !is_alarm_active) {
        eval.should_trigger = true;
        eval.condition_met = condition;
    } else if (!trigger && is_alarm_active && rule.getAutoClear()) {
        eval.should_clear = true;
        eval.condition_met = "Condition Cleared";
    }
    
    // 상태 업데이트
    updateLastDigitalState(rule.getId(), state);
    
    return eval;
}

AlarmEvaluation AlarmEngine::evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context) {
    AlarmEvaluation eval;
    eval.severity = rule.getSeverityString();
    
    if (!js_context_) {
        logger_.Error("JavaScript engine not available for script alarm");
        return eval;
    }
    
    try {
        std::lock_guard<std::mutex> lock(js_mutex_);
        
        // 컨텍스트 변수 설정
        for (auto& [key, value] : context.items()) {
            std::string js_code;
            
            if (value.is_number()) {
                js_code = "var " + key + " = " + std::to_string(value.get<double>()) + ";";
            } else if (value.is_boolean()) {
                js_code = "var " + key + " = " + (value.get<bool>() ? "true" : "false") + ";";
            } else if (value.is_string()) {
                js_code = "var " + key + " = '" + value.get<std::string>() + "';";
            } else {
                js_code = "var " + key + " = null;";
            }
            
            JSValue var_result = JS_Eval((JSContext*)js_context_, js_code.c_str(), js_code.length(), 
                                         "<context>", JS_EVAL_TYPE_GLOBAL);
            JS_FreeValue((JSContext*)js_context_, var_result);
        }
        
        // 스크립트 실행
        std::string script = rule.getConditionScript();
        JSValue eval_result = JS_Eval((JSContext*)js_context_, script.c_str(), script.length(),
                                      "<alarm_script>", JS_EVAL_TYPE_GLOBAL);
        
        if (JS_IsException(eval_result)) {
            JSValue exception = JS_GetException((JSContext*)js_context_);
            const char* error_str = JS_ToCString((JSContext*)js_context_, exception);
            logger_.Error("Script alarm evaluation error: " + std::string(error_str ? error_str : "Unknown"));
            JS_FreeCString((JSContext*)js_context_, error_str);
            JS_FreeValue((JSContext*)js_context_, exception);
        } else {
            bool triggered = false;
            
            if (JS_IsBool(eval_result)) {
                triggered = JS_ToBool((JSContext*)js_context_, eval_result);
            } else if (JS_IsNumber(eval_result)) {
                double val;
                JS_ToFloat64((JSContext*)js_context_, &val, eval_result);
                triggered = (val != 0.0);
            }
            
            bool is_alarm_active = isAlarmActive(rule.getId());
            
            if (triggered && !is_alarm_active) {
                eval.should_trigger = true;
                eval.condition_met = "Script Condition";
            } else if (!triggered && is_alarm_active && rule.getAutoClear()) {
                eval.should_clear = true;
                eval.condition_met = "Script Cleared";
            }
        }
        
        JS_FreeValue((JSContext*)js_context_, eval_result);
        
    } catch (const std::exception& e) {
        logger_.Error("Script alarm evaluation failed: " + std::string(e.what()));
    }
    
    return eval;
}

// =============================================================================
// 🔥 알람 발생/해제
// =============================================================================

std::optional<int64_t> AlarmEngine::raiseAlarm(const AlarmRuleEntity& rule, 
                                               const AlarmEvaluation& eval,
                                               const DataValue& trigger_value) {
    try {
        AlarmOccurrenceEntity occurrence;
        occurrence.setId(next_occurrence_id_.fetch_add(1));
        occurrence.setRuleId(rule.getId());
        occurrence.setTenantId(rule.getTenantId());
        occurrence.setTriggerCondition(eval.condition_met);
        occurrence.setAlarmMessage(eval.message);
        occurrence.setSeverity(eval.severity);
        occurrence.setState("active");
        
        // 트리거 값을 JSON으로 저장
        json trigger_json;
        std::visit([&trigger_json](auto&& v) {
            trigger_json = v;
        }, trigger_value);
        occurrence.setTriggerValue(trigger_json.dump());
        
        // 데이터베이스 저장
        if (alarm_occurrence_repo_->save(occurrence)) {
            // 매핑 업데이트
            {
                std::unique_lock<std::shared_mutex> lock(occurrence_map_mutex_);
                rule_occurrence_map_[rule.getId()] = occurrence.getId();
            }
            
            alarms_raised_.fetch_add(1);
            return occurrence.getId();
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to raise alarm: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

bool AlarmEngine::clearAlarm(int64_t occurrence_id, const DataValue& clear_value) {
    try {
        auto occurrence = alarm_occurrence_repo_->findById(occurrence_id);
        if (!occurrence) return false;
        
        occurrence->setState("cleared");
        occurrence->setClearedTime(std::chrono::system_clock::now());
        
        // 해제 값을 JSON으로 저장
        json clear_json;
        std::visit([&clear_json](auto&& v) {
            clear_json = v;
        }, clear_value);
        occurrence->setClearedValue(clear_json.dump());
        
        // 데이터베이스 업데이트
        if (alarm_occurrence_repo_->update(*occurrence)) {
            // 매핑에서 제거
            {
                std::unique_lock<std::shared_mutex> lock(occurrence_map_mutex_);
                for (auto it = rule_occurrence_map_.begin(); it != rule_occurrence_map_.end(); ++it) {
                    if (it->second == occurrence_id) {
                        rule_occurrence_map_.erase(it);
                        break;
                    }
                }
            }
            
            alarms_cleared_.fetch_add(1);
            return true;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to clear alarm: " + std::string(e.what()));
    }
    
    return false;
}

// =============================================================================
// 🔥 알람 규칙 관리
// =============================================================================

bool AlarmEngine::loadAlarmRules(int tenant_id) {
    try {
        auto rules = alarm_rule_repo_->findByTenantIdAndEnabled(tenant_id, true);
        
        std::unique_lock<std::shared_mutex> lock(rules_cache_mutex_);
        
        // 기존 규칙 정리
        tenant_rules_[tenant_id].clear();
        
        // 포인트 인덱스 정리
        auto it = point_rule_index_.begin();
        while (it != point_rule_index_.end()) {
            if (it->first.find("t" + std::to_string(tenant_id) + "_") == 0) {
                it = point_rule_index_.erase(it);
            } else {
                ++it;
            }
        }
        
        // 새 규칙들 로드
        for (const auto& rule : rules) {
            tenant_rules_[tenant_id].push_back(rule);
            
            // 포인트 인덱스 구축
            if (rule.getTargetId().has_value()) {
                std::string key = "t" + std::to_string(tenant_id) + "_" + 
                                rule.getTargetTypeString() + "_" + 
                                std::to_string(*rule.getTargetId());
                point_rule_index_[key].push_back(rule.getId());
            }
        }
        
        logger_.Info("Loaded " + std::to_string(rules.size()) + 
                    " alarm rules for tenant " + std::to_string(tenant_id));
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to load alarm rules: " + std::string(e.what()));
        return false;
    }
}

std::vector<AlarmRuleEntity> AlarmEngine::getAlarmRulesForPoint(int tenant_id, 
                                                               const std::string& point_type, 
                                                               int point_id) const {
    std::vector<AlarmRuleEntity> result;
    
    std::shared_lock<std::shared_mutex> lock(rules_cache_mutex_);
    
    std::string key = "t" + std::to_string(tenant_id) + "_" + point_type + "_" + std::to_string(point_id);
    auto it = point_rule_index_.find(key);
    
    if (it != point_rule_index_.end()) {
        auto tenant_it = tenant_rules_.find(tenant_id);
        if (tenant_it != tenant_rules_.end()) {
            for (int rule_id : it->second) {
                auto rule_it = std::find_if(tenant_it->second.begin(), tenant_it->second.end(),
                    [rule_id](const AlarmRuleEntity& rule) {
                        return rule.getId() == rule_id;
                    });
                
                if (rule_it != tenant_it->second.end()) {
                    result.push_back(*rule_it);
                }
            }
        }
    }
    
    return result;
}

// =============================================================================
// 🔥 헬퍼 메서드들
// =============================================================================

bool AlarmEngine::checkDeadband(const AlarmRuleEntity& rule, double current, double previous, double threshold) {
    double deadband = rule.getDeadband();
    if (deadband <= 0) return true;
    
    if (current >= threshold) {
        // 상승 방향: 이전 값이 (threshold - deadband) 아래에서 올라온 경우
        return previous < (threshold - deadband);
    } else {
        // 하강 방향: 이전 값이 (threshold + deadband) 위에서 내려온 경우
        return previous > (threshold + deadband);
    }
}

std::string AlarmEngine::getAnalogLevel(const AlarmRuleEntity& rule, double value) {
    if (rule.getHighHighLimit() && value >= *rule.getHighHighLimit()) {
        return "high_high";
    }
    if (rule.getHighLimit() && value >= *rule.getHighLimit()) {
        return "high";
    }
    if (rule.getLowLowLimit() && value <= *rule.getLowLowLimit()) {
        return "low_low";
    }
    if (rule.getLowLimit() && value <= *rule.getLowLimit()) {
        return "low";
    }
    return "normal";
}

std::string AlarmEngine::generateMessage(const AlarmRuleEntity& rule, 
                                        const AlarmEvaluation& eval, 
                                        const DataValue& value) {
    // 기본 메시지 템플릿
    std::string message = rule.getName() + " - " + eval.condition_met;
    
    // 값이 있으면 추가
    std::string value_str;
    std::visit([&value_str](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            value_str = v;
        } else if constexpr (std::is_arithmetic_v<T>) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << v;
            value_str = oss.str();
        }
    }, value);
    
    if (!value_str.empty()) {
        message += " (값: " + value_str + ")";
    }
    
    // 메시지 템플릿이 있으면 사용
    if (!rule.getMessageTemplate().empty()) {
        std::map<std::string, std::string> variables;
        variables["{name}"] = rule.getName();
        variables["{condition}"] = eval.condition_met;
        variables["{value}"] = value_str;
        variables["{severity}"] = eval.severity;
        
        message = interpolateTemplate(rule.getMessageTemplate(), variables);
    }
    
    return message;
}

std::string AlarmEngine::interpolateTemplate(const std::string& tmpl, 
                                            const std::map<std::string, std::string>& variables) {
    std::string result = tmpl;
    
    for (const auto& [key, value] : variables) {
        size_t pos = 0;
        while ((pos = result.find(key, pos)) != std::string::npos) {
            result.replace(pos, key.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

// =============================================================================
// 🔥 상태 관리 헬퍼들
// =============================================================================

bool AlarmEngine::isAlarmActive(int rule_id) const {
    std::shared_lock<std::shared_mutex> lock(state_cache_mutex_);
    auto it = alarm_states_.find(rule_id);
    return (it != alarm_states_.end()) ? it->second : false;
}

void AlarmEngine::updateAlarmState(int rule_id, bool active) {
    std::unique_lock<std::shared_mutex> lock(state_cache_mutex_);
    alarm_states_[rule_id] = active;
}

double AlarmEngine::getLastValue(int rule_id) const {
    std::shared_lock<std::shared_mutex> lock(state_cache_mutex_);
    auto it = last_values_.find(rule_id);
    return (it != last_values_.end()) ? it->second : 0.0;
}

void AlarmEngine::updateLastValue(int rule_id, double value) {
    std::unique_lock<std::shared_mutex> lock(state_cache_mutex_);
    last_values_[rule_id] = value;
}

bool AlarmEngine::getLastDigitalState(int rule_id) const {
    std::shared_lock<std::shared_mutex> lock(state_cache_mutex_);
    auto it = last_digital_states_.find(rule_id);
    return (it != last_digital_states_.end()) ? it->second : false;
}

void AlarmEngine::updateLastDigitalState(int rule_id, bool state) {
    std::unique_lock<std::shared_mutex> lock(state_cache_mutex_);
    last_digital_states_[rule_id] = state;
}

// =============================================================================
// 🔥 외부 시스템 연동
// =============================================================================

void AlarmEngine::publishToRedis(const AlarmEvent& event) {
    if (!redis_client_) return;
    
    try {
        json alarm_json = {
            {"type", "alarm_event"},
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"tenant_id", event.tenant_id},
            {"point_id", event.point_id},
            {"severity", event.severity},
            {"state", event.state},
            {"message", event.message},
            {"timestamp", std::chrono::system_clock::to_time_t(event.occurrence_time)}
        };
        
        std::string channel = "tenant:" + std::to_string(event.tenant_id) + ":alarms";
        redis_client_->publish(channel, alarm_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to publish alarm to Redis: " + std::string(e.what()));
    }
}

void AlarmEngine::sendToMessageQueue(const AlarmEvent& event) {
    if (!mq_client_) return;
    
    try {
        json alarm_json = {
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"tenant_id", event.tenant_id},
            {"point_id", event.point_id},
            {"severity", event.severity},
            {"state", event.state},
            {"message", event.message},
            {"timestamp", std::chrono::system_clock::to_time_t(event.occurrence_time)}
        };
        
        std::string routing_key = "alarm.tenant." + std::to_string(event.tenant_id);
        mq_client_->publish("alarms", routing_key, alarm_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to send alarm to message queue: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 JavaScript 엔진 (스크립트 알람용)
// =============================================================================

bool AlarmEngine::initScriptEngine() {
    try {
        js_runtime_ = JS_NewRuntime();
        if (!js_runtime_) {
            logger_.Error("Failed to create JS runtime");
            return false;
        }
        
        // 메모리 제한 (8MB)
        JS_SetMemoryLimit((JSRuntime*)js_runtime_, 8 * 1024 * 1024);
        
        js_context_ = JS_NewContext((JSRuntime*)js_runtime_);
        if (!js_context_) {
            logger_.Error("Failed to create JS context");
            JS_FreeRuntime((JSRuntime*)js_runtime_);
            js_runtime_ = nullptr;
            return false;
        }
        
        logger_.Info("JavaScript engine initialized for script alarms");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("JavaScript engine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AlarmEngine::cleanupScriptEngine() {
    if (js_context_) {
        JS_FreeContext((JSContext*)js_context_);
        js_context_ = nullptr;
    }
    if (js_runtime_) {
        JS_FreeRuntime((JSRuntime*)js_runtime_);
        js_runtime_ = nullptr;
    }
}

// =============================================================================
// 🔥 통계 및 조회
// =============================================================================

nlohmann::json AlarmEngine::getStatistics() const {
    return {
        {"total_evaluations", total_evaluations_.load()},
        {"alarms_raised", alarms_raised_.load()},
        {"alarms_cleared", alarms_cleared_.load()},
        {"evaluation_errors", evaluations_errors_.load()},
        {"cached_rules_count", tenant_rules_.size()},
        {"active_alarms_count", rule_occurrence_map_.size()}
    };
}

std::vector<AlarmOccurrenceEntity> AlarmEngine::getActiveAlarms(int tenant_id) const {
    try {
        return alarm_occurrence_repo_->findActiveByTenantId(tenant_id);
    } catch (const std::exception& e) {
        logger_.Error("Failed to get active alarms: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmOccurrenceEntity> AlarmEngine::getAlarmOccurrence(int64_t occurrence_id) const {
    try {
        return alarm_occurrence_repo_->findById(occurrence_id);
    } catch (const std::exception& e) {
        logger_.Error("Failed to get alarm occurrence: " + std::string(e.what()));
        return std::nullopt;
    }
}

} // namespace Alarm
} // namespace PulseOne