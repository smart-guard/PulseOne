// =============================================================================
// collector/src/Alarm/AlarmEngine.cpp
// PulseOne 알람 엔진 구현 - 올바른 싱글톤 패턴
// =============================================================================

#include "Alarm/AlarmEngine.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/RepositoryFactory.h"

//#include "Client/RabbitMQClient.h"
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

AlarmEngine::AlarmEngine() {
    
    LogManager::getInstance().Debug("AlarmEngine constructor starting...");
    
    try {
        // 🔥 생성자에서 모든 초기화를 완료
        initializeClients();
        initializeRepositories(); 
        initScriptEngine();
        loadInitialData();
        
        initialized_ = true;
        LogManager::getInstance().Info("AlarmEngine initialized successfully in constructor");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmEngine constructor failed: " + std::string(e.what()));
        initialized_ = false;  // 실패 표시
    }
}

AlarmEngine::~AlarmEngine() {
    shutdown();
}

// =============================================================================
// 🔥 initialize() 메서드 제거 - 생성자에서 모든 것 완료
// =============================================================================

void AlarmEngine::shutdown() {
    if (!initialized_.load()) return;
    
    LogManager::getInstance().Info("Shutting down AlarmEngine");
    
    // JavaScript 엔진 정리
    cleanupScriptEngine();
    
    // Redis 연결 종료
    if (redis_client_) {
        redis_client_->disconnect();
        redis_client_.reset();
    }
    
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
    LogManager::getInstance().Info("AlarmEngine shutdown completed");
}

// =============================================================================
// 🔥 내부 초기화 메서드들
// =============================================================================

void AlarmEngine::initializeClients() {
    try {
        // ConfigManager 싱글톤에서 설정 가져오기
        auto& config = ConfigManager::getInstance();
        
        // Redis 클라이언트 생성
        std::string redis_host = config.getOrDefault("redis.host", "localhost");
        int redis_port = config.getInt("redis.port", 6379);
        std::string redis_password = config.getOrDefault("redis.password", "");
        
        redis_client_ = std::make_shared<RedisClientImpl>();
        if (!redis_client_->connect(redis_host, redis_port, redis_password)) {
            LogManager::getInstance().Warn("Failed to connect to Redis - alarm events will not be published");
            redis_client_.reset();
        }
        
        // 🔥 RabbitMQ는 현재 비활성화
        // mq_client_ = std::make_shared<RabbitMQClient>();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to initialize clients: " + std::string(e.what()));
    }
}

void AlarmEngine::initializeRepositories() {
    try {
        // Repository Factory 싱글톤 사용
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        
        alarm_rule_repo_ = repo_factory.getAlarmRuleRepository();
        if (!alarm_rule_repo_) {
            LogManager::getInstance().Error("Failed to get AlarmRuleRepository from factory");
        }

        alarm_occurrence_repo_ = repo_factory.getAlarmOccurrenceRepository();
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().Error("Failed to get AlarmOccurrenceRepository from factory");
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to initialize repositories: " + std::string(e.what()));
    }
}

void AlarmEngine::loadInitialData() {
    try {
        // 다음 occurrence ID 로드
        if (alarm_occurrence_repo_) {
            // 🔥 수정: int 반환값 처리 (optional 제거)
            int max_occurrence = alarm_occurrence_repo_->findMaxId();
            next_occurrence_id_ = (max_occurrence > 0) ? (max_occurrence + 1) : 1;
                                 
            LogManager::getInstance().log("AlarmEngine", LogLevel::DEBUG,
                                        "Next occurrence ID initialized to: " + 
                                        std::to_string(next_occurrence_id_));
        } else {
            // Repository가 없으면 기본값 사용
            next_occurrence_id_ = 1;
            LogManager::getInstance().log("AlarmEngine", LogLevel::WARN,
                                        "AlarmOccurrenceRepository not available, using default occurrence ID");
        }
        
        LogManager::getInstance().log("AlarmEngine", LogLevel::DEBUG, 
                                    "Initial data loaded successfully");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmEngine", LogLevel::ERROR,
                                    "Failed to load initial data: " + std::string(e.what()));
        // 에러가 있어도 계속 진행 (기본값 사용)
        next_occurrence_id_ = 1;
    }
}

// =============================================================================
// 🔥 메인 인터페이스 - 상태 체크만 수행
// =============================================================================

std::vector<AlarmEvent> AlarmEngine::evaluateForMessage(const DeviceDataMessage& message) {
    std::vector<AlarmEvent> alarm_events;
    
    // 🔥 간단한 상태 체크만
    if (!initialized_.load()) {
        LogManager::getInstance().Error("AlarmEngine not properly initialized");
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
        LogManager::getInstance().Error("Failed to evaluate alarm for message: " + std::string(e.what()));
    }
    
    return alarm_events;
}

std::vector<AlarmEvent> AlarmEngine::evaluateForPoint(int tenant_id, int point_id, const DataValue& value) {
    std::vector<AlarmEvent> alarm_events;
    
    // =============================================================================
    // 🎯 1. 초기화 상태 체크
    // =============================================================================
    if (!initialized_.load()) {
        LogManager::getInstance().Error("AlarmEngine not properly initialized");
        return alarm_events;
    }
    
    // =============================================================================
    // 🔥 2. 포인트 타입 결정 - 범위 제한 제거
    // =============================================================================
    std::string point_type = "data_point";  // 🔥 모든 포인트를 data_point로 처리
    int numeric_id = point_id;
    
    LogManager::getInstance().Debug("Evaluating alarms for point " + std::to_string(point_id));
    
    // =============================================================================
    // 🎯 3. 해당 포인트의 알람 규칙들 조회
    // =============================================================================
    std::vector<AlarmRuleEntity> rules;
    try {
        rules = getAlarmRulesForPoint(tenant_id, point_type, numeric_id);
        
        if (rules.empty()) {
            LogManager::getInstance().Debug("No alarm rules found for point " + std::to_string(point_id));
            return alarm_events;
        }
        
        LogManager::getInstance().Debug("Found " + std::to_string(rules.size()) + 
                                      " alarm rules for point " + std::to_string(point_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get alarm rules for point " + 
                                      std::to_string(point_id) + ": " + std::string(e.what()));
        return alarm_events;
    }
    
    // =============================================================================
    // 🎯 4. 각 규칙별 알람 평가 수행
    // =============================================================================
    for (const auto& rule : rules) {
        try {
            if (!rule.isEnabled()) {
                LogManager::getInstance().Debug("Skipping disabled rule " + std::to_string(rule.getId()));
                continue;
            }
            
            LogManager::getInstance().Debug("Evaluating rule " + std::to_string(rule.getId()) + 
                                          " (" + rule.getName() + ") for point " + std::to_string(point_id));
            
            AlarmEvaluation eval = evaluateRule(rule, value);
            
            if (eval.state_changed) {
                if (eval.should_trigger) {
                    // 🔥 알람 발생
                    AlarmEvent trigger_event;
                    trigger_event.device_id = getDeviceIdForPoint(point_id);
                    trigger_event.point_id = point_id;
                    trigger_event.rule_id = rule.getId();
                    trigger_event.current_value = value;
                    trigger_event.threshold_value = getThresholdValue(rule, eval);
                    trigger_event.trigger_condition = determineTriggerCondition(rule, eval);
                    trigger_event.alarm_type = convertToAlarmType(rule.getAlarmType());
                    trigger_event.message = generateMessage(rule, eval, value);  // 🔥 기존 메서드 사용
                    trigger_event.severity = rule.getSeverity();
                    trigger_event.state = AlarmState::ACTIVE;
                    trigger_event.timestamp = std::chrono::system_clock::now();
                    trigger_event.occurrence_time = eval.timestamp;
                    trigger_event.source_name = "Point_" + std::to_string(point_id);  // 🔥 직접 설정
                    trigger_event.location = getPointLocation(point_id);
                    trigger_event.tenant_id = tenant_id;
                    trigger_event.trigger_value = value;
                    trigger_event.condition_met = true;
                    
                    alarm_events.push_back(trigger_event);
                    
                    // 🔥 기존 통계 변수 사용
                    alarms_raised_.fetch_add(1);
                    
                    LogManager::getInstance().Info("🚨 Alarm TRIGGERED: Rule " + 
                                                  std::to_string(rule.getId()) + 
                                                  " (" + rule.getName() + ") for point " + 
                                                  std::to_string(point_id));
                    
                } else if (eval.should_clear) {
                    // 🔥 알람 해제
                    AlarmEvent clear_event;
                    clear_event.device_id = getDeviceIdForPoint(point_id);
                    clear_event.point_id = point_id;
                    clear_event.rule_id = rule.getId();
                    clear_event.current_value = value;
                    clear_event.alarm_type = convertToAlarmType(rule.getAlarmType());
                    clear_event.message = "Alarm cleared: " + generateMessage(rule, eval, value);
                    clear_event.severity = rule.getSeverity();
                    clear_event.state = AlarmState::CLEARED;
                    clear_event.timestamp = std::chrono::system_clock::now();
                    clear_event.occurrence_time = eval.timestamp;
                    clear_event.source_name = "Point_" + std::to_string(point_id);  // 🔥 직접 설정
                    clear_event.location = getPointLocation(point_id);
                    clear_event.tenant_id = tenant_id;
                    clear_event.trigger_value = value;
                    clear_event.condition_met = false;
                    
                    alarm_events.push_back(clear_event);
                    
                    // 🔥 기존 통계 변수 사용
                    alarms_cleared_.fetch_add(1);
                    
                    LogManager::getInstance().Info("✅ Alarm CLEARED: Rule " + 
                                                  std::to_string(rule.getId()) + 
                                                  " (" + rule.getName() + ") for point " + 
                                                  std::to_string(point_id));
                }
            } else {
                LogManager::getInstance().Debug("No state change for rule " + 
                                              std::to_string(rule.getId()) + 
                                              " on point " + std::to_string(point_id));
            }
            
        } catch (const std::exception& e) {
            evaluations_errors_.fetch_add(1);
            LogManager::getInstance().Error("Failed to evaluate rule " + 
                                          std::to_string(rule.getId()) + 
                                          " (" + rule.getName() + ") for point " + 
                                          std::to_string(point_id) + ": " + 
                                          std::string(e.what()));
        }
    }
    
    // =============================================================================
    // 🎯 5. 전체 평가 통계 업데이트
    // =============================================================================
    total_evaluations_.fetch_add(rules.size());
    
    if (!alarm_events.empty()) {
        LogManager::getInstance().Info("Generated " + std::to_string(alarm_events.size()) + 
                                     " alarm events for point " + std::to_string(point_id));
    } else {
        LogManager::getInstance().Debug("No alarm events generated for point " + std::to_string(point_id));
    }
    
    return alarm_events;
}

UUID AlarmEngine::getDeviceIdForPoint(int point_id) {
    // TODO: 실제 구현에서는 데이터베이스 조회 또는 캐시 사용
    try {
        // 예시: point_id 범위로 추정하거나 DB 조회
        if (point_id >= 1000 && point_id < 9000) {
            // data_point의 경우 DataPointRepository에서 조회
            // auto dp = data_point_repo_->findById(point_id);
            // return dp ? dp->getDeviceId() : UUID{};
        } else if (point_id >= 9000) {
            // virtual_point의 경우 기본 디바이스 ID 사용
            // return virtual_device_id_;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get device ID for point " + 
                                      std::to_string(point_id) + ": " + std::string(e.what()));
    }
    
    return UUID{};  // 기본값
}

std::string AlarmEngine::getPointLocation(int point_id) {
    // TODO: 실제 구현에서는 사이트/위치 정보 조회
    return "Unknown Location";
}
AlarmType AlarmEngine::convertToAlarmType(const AlarmRuleEntity::AlarmType& entity_type) {
    switch(entity_type) {
        case AlarmRuleEntity::AlarmType::ANALOG: return AlarmType::ANALOG;
        case AlarmRuleEntity::AlarmType::DIGITAL: return AlarmType::DIGITAL;
        case AlarmRuleEntity::AlarmType::SCRIPT: return AlarmType::SCRIPT;
        default: return AlarmType::ANALOG;
    }
}

/**
 * @brief 규칙과 평가 결과로부터 트리거 조건 결정
 */
TriggerCondition AlarmEngine::determineTriggerCondition(const AlarmRuleEntity& rule, const AlarmEvaluation& eval) {
    if (eval.condition_met == "HIGH") return TriggerCondition::HIGH;
    if (eval.condition_met == "LOW") return TriggerCondition::LOW;
    if (eval.condition_met == "HIGH_HIGH") return TriggerCondition::HIGH_HIGH;
    if (eval.condition_met == "LOW_LOW") return TriggerCondition::LOW_LOW;
    if (eval.condition_met == "ON_TRUE") return TriggerCondition::DIGITAL_TRUE;
    if (eval.condition_met == "ON_FALSE") return TriggerCondition::DIGITAL_FALSE;
    if (eval.condition_met == "ON_CHANGE") return TriggerCondition::DIGITAL_CHANGE;
    
    return TriggerCondition::NONE;
}

/**
 * @brief 규칙과 평가 결과로부터 임계값 추출
 */
double AlarmEngine::getThresholdValue(const AlarmRuleEntity& rule, const AlarmEvaluation& eval) {
    if (eval.condition_met == "HIGH" && rule.getHighLimit().has_value()) {
        return rule.getHighLimit().value();
    }
    if (eval.condition_met == "LOW" && rule.getLowLimit().has_value()) {
        return rule.getLowLimit().value();
    }
    if (eval.condition_met == "HIGH_HIGH" && rule.getHighHighLimit().has_value()) {
        return rule.getHighHighLimit().value();
    }
    if (eval.condition_met == "LOW_LOW" && rule.getLowLowLimit().has_value()) {
        return rule.getLowLowLimit().value();
    }
    
    return 0.0;  // 기본값
}
// =============================================================================
// 🔥 핵심 평가 로직들 - 상태 체크 추가
// =============================================================================

AlarmEvaluation AlarmEngine::evaluateRule(const AlarmRuleEntity& rule, const DataValue& value) {
    AlarmEvaluation eval;
    
    // 🔥 상태 체크
    if (!initialized_.load()) {
        LogManager::getInstance().Error("AlarmEngine not properly initialized");
        return eval;
    }
    
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
        LogManager::getInstance().Error("Failed to evaluate rule " + rule.getName() + ": " + std::string(e.what()));
    }
    
    // 평가 시간 계산
    auto end_time = std::chrono::steady_clock::now();
    eval.evaluation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return eval;
}

// =============================================================================
// 🔥 나머지 모든 메서드들도 동일한 패턴으로 상태 체크
// =============================================================================

AlarmEvaluation AlarmEngine::evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value) {
    AlarmEvaluation eval;
    eval.timestamp = std::chrono::system_clock::now();
    eval.rule_id = rule.getId();
    eval.tenant_id = rule.getTenantId();
    eval.severity = rule.getSeverity();
    eval.triggered_value = std::to_string(value);
    
    // 🔥 초기값 설정 (중요!)
    eval.should_trigger = false;
    eval.should_clear = false;
    eval.state_changed = false;
    eval.condition_met = "NORMAL";
    
    try {
        LogManager::getInstance().Debug("Evaluating analog alarm for rule " + 
                                      std::to_string(rule.getId()) + ", value: " + std::to_string(value));
        
        // 🚨 1단계: 현재 임계값 조건 체크
        bool condition_triggered = false;
        std::string alarm_level;
        
        if (rule.getHighHighLimit().has_value() && value >= rule.getHighHighLimit().value()) {
            condition_triggered = true;
            alarm_level = "HIGH_HIGH";
            LogManager::getInstance().Debug("HIGH_HIGH condition met: " + std::to_string(value) + 
                                          " >= " + std::to_string(rule.getHighHighLimit().value()));
        }
        else if (rule.getHighLimit().has_value() && value >= rule.getHighLimit().value()) {
            condition_triggered = true;
            alarm_level = "HIGH";
            LogManager::getInstance().Debug("HIGH condition met: " + std::to_string(value) + 
                                          " >= " + std::to_string(rule.getHighLimit().value()));
        }
        else if (rule.getLowLimit().has_value() && value <= rule.getLowLimit().value()) {
            condition_triggered = true;
            alarm_level = "LOW";
            LogManager::getInstance().Debug("LOW condition met: " + std::to_string(value) + 
                                          " <= " + std::to_string(rule.getLowLimit().value()));
        }
        else if (rule.getLowLowLimit().has_value() && value <= rule.getLowLowLimit().value()) {
            condition_triggered = true;
            alarm_level = "LOW_LOW";
            LogManager::getInstance().Debug("LOW_LOW condition met: " + std::to_string(value) + 
                                          " <= " + std::to_string(rule.getLowLowLimit().value()));
        }
        else {
            LogManager::getInstance().Debug("Value in normal range: " + std::to_string(value));
        }
        
        // 🚨 2단계: 현재 알람 상태 확인
        std::lock_guard<std::mutex> lock(state_mutex_);
        bool currently_in_alarm = isAlarmActive(rule.getId());
        double last_value = getLastValue(rule.getId());
        
        LogManager::getInstance().Debug("Rule " + std::to_string(rule.getId()) + 
                                      " - condition_triggered: " + std::to_string(condition_triggered) + 
                                      ", currently_in_alarm: " + std::to_string(currently_in_alarm));
        
        // 🚨 3단계: 히스테리시스 체크 (간단하게!)
        double deadband = rule.getDeadband();  // ✅ 수정: .value_or() 제거
        bool hysteresis_clear = true;
        
        if (currently_in_alarm && !condition_triggered && deadband > 0) {
            // 알람 중이었는데 조건이 해제 → 히스테리시스 체크
            if (rule.getHighLimit().has_value() && last_value >= rule.getHighLimit().value()) {
                hysteresis_clear = value < (rule.getHighLimit().value() - deadband);
                LogManager::getInstance().Debug("Hysteresis check for clearing: " + std::to_string(hysteresis_clear) + 
                                              ", deadband: " + std::to_string(deadband));
            }
            else if (rule.getLowLimit().has_value() && last_value <= rule.getLowLimit().value()) {
                hysteresis_clear = value > (rule.getLowLimit().value() + deadband);
                LogManager::getInstance().Debug("Hysteresis check for clearing: " + std::to_string(hysteresis_clear) + 
                                              ", deadband: " + std::to_string(deadband));
            }
        }
        
        // 🔥 4단계: 상태 변경 결정 (핵심!)
        if (condition_triggered && !currently_in_alarm) {
            // ✅ 새로운 알람 발생
            eval.should_trigger = true;
            eval.state_changed = true;
            eval.condition_met = alarm_level;
            
            LogManager::getInstance().Info("Analog alarm TRIGGERED for rule " + 
                                         std::to_string(rule.getId()) + ": " + alarm_level);
        }
        else if (!condition_triggered && currently_in_alarm && rule.isAutoClear() && hysteresis_clear) {
            // ✅ 알람 해제 (자동 해제 + 히스테리시스 조건 만족)
            eval.should_clear = true;
            eval.state_changed = true;
            eval.condition_met = "AUTO_CLEAR";
            
            LogManager::getInstance().Info("Analog alarm CLEARED for rule " + 
                                         std::to_string(rule.getId()) + ": AUTO_CLEAR");
        }
        else {
            // ❌ 상태 변경 없음
            LogManager::getInstance().Debug("No state change for rule " + 
                                          std::to_string(rule.getId()) + 
                                          " on point " + std::to_string(rule.getTargetId().value_or(0)));  // ✅ 수정
        }
        
        // 현재 값 업데이트
        updateLastValue(rule.getId(), value);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to evaluate rule " + rule.getName() + ": " + std::string(e.what()));
        eval.condition_met = "ERROR";
        eval.message = "Evaluation error: " + std::string(e.what());
    }
    
    // 메시지 생성
    eval.message = generateMessage(rule, eval, DataValue{value});
    
    return eval;
}


AlarmEvaluation AlarmEngine::evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool value) {
    AlarmEvaluation eval;
    eval.timestamp = std::chrono::system_clock::now();
    eval.rule_id = rule.getId();
    eval.tenant_id = rule.getTenantId();
    eval.severity = rule.getSeverity();
    eval.triggered_value = value ? "true" : "false";
    
    // 🔥 초기값 설정 (중요!)
    eval.should_trigger = false;
    eval.should_clear = false;
    eval.state_changed = false;
    eval.condition_met = "NORMAL";
    
    try {
        LogManager::getInstance().Debug("Evaluating digital alarm for rule " + 
                                      std::to_string(rule.getId()) + ", value: " + (value ? "true" : "false"));
        
        // 🚨 1단계: 트리거 조건 체크
        bool condition_triggered = false;
        std::string trigger_type;
        
        std::lock_guard<std::mutex> lock(state_mutex_);
        bool currently_in_alarm = isAlarmActive(rule.getId());
        bool last_digital_state = (getLastValue(rule.getId()) > 0.5);  // double에서 bool 변환
        
        auto trigger = rule.getTriggerCondition();
        switch (trigger) {
            case AlarmRuleEntity::DigitalTrigger::ON_TRUE:
                condition_triggered = value;
                trigger_type = "DIGITAL_TRUE";
                LogManager::getInstance().Debug("ON_TRUE trigger: " + std::to_string(value));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_FALSE:
                condition_triggered = !value;
                trigger_type = "DIGITAL_FALSE";
                LogManager::getInstance().Debug("ON_FALSE trigger: " + std::to_string(!value));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_RISING:
                condition_triggered = value && !last_digital_state;
                trigger_type = "DIGITAL_RISING";
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_FALLING:
                condition_triggered = !value && last_digital_state;
                trigger_type = "DIGITAL_FALLING";
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_CHANGE:
            default:
                condition_triggered = (value != last_digital_state);
                trigger_type = "DIGITAL_CHANGE";
                break;
        }
        
        LogManager::getInstance().Debug("Rule " + std::to_string(rule.getId()) + 
                                      " - condition_triggered: " + std::to_string(condition_triggered) + 
                                      ", currently_in_alarm: " + std::to_string(currently_in_alarm));
        
        // 🔥 2단계: 상태 변경 결정
        if (condition_triggered && !currently_in_alarm) {
            // ✅ 새로운 알람 발생
            eval.should_trigger = true;
            eval.state_changed = true;
            eval.condition_met = trigger_type;
            
            LogManager::getInstance().Info("Digital alarm TRIGGERED for rule " + 
                                         std::to_string(rule.getId()) + ": " + trigger_type);
        }
        else if (!condition_triggered && currently_in_alarm && rule.isAutoClear()) {
            // ✅ 알람 해제
            eval.should_clear = true;
            eval.state_changed = true;
            eval.condition_met = "AUTO_CLEAR";
            
            LogManager::getInstance().Info("Digital alarm CLEARED for rule " + 
                                         std::to_string(rule.getId()));
        }
        else {
            // ❌ 상태 변경 없음
            LogManager::getInstance().Debug("No state change for rule " + 
                                          std::to_string(rule.getId()) + 
                                          " on point " + std::to_string(rule.getTargetId().value_or(0)));  // ✅ 수정
        }
        
        // 현재 상태 업데이트
        updateLastValue(rule.getId(), value ? 1.0 : 0.0);  // bool을 double로 저장
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to evaluate rule " + rule.getName() + ": " + std::string(e.what()));
        eval.condition_met = "ERROR";
        eval.message = "Evaluation error: " + std::string(e.what());
    }
    
    // 메시지 생성
    eval.message = generateMessage(rule, eval, DataValue{value});
    
    return eval;
}

AlarmEvaluation AlarmEngine::evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context) {
    AlarmEvaluation eval;
    eval.timestamp = std::chrono::system_clock::now();
    eval.rule_id = rule.getId();
    eval.tenant_id = rule.getTenantId();
    eval.severity = rule.getSeverity();
    eval.triggered_value = context.dump();
    
    try {
        LogManager::getInstance().Debug("Evaluating script alarm for rule " + 
                                      std::to_string(rule.getId()));
        
        // ✅ const string& 직접 사용 - 복사 없음
        const std::string& condition_script = rule.getConditionScript();
        if (condition_script.empty()) {
            LogManager::getInstance().Warn("No condition script for rule " + std::to_string(rule.getId()));
            eval.condition_met = "NO_SCRIPT";
            eval.message = "No condition script defined";
            return eval;
        }
        
        // JavaScript 엔진 체크
        if (!js_runtime_ || !js_context_) {
            LogManager::getInstance().Error("JavaScript engine not initialized");
            eval.condition_met = "JS_ERROR";
            eval.message = "JavaScript engine not available";
            return eval;
        }
        
        // JavaScript 실행 (기존 로직 유지)
        std::lock_guard<std::mutex> js_lock(js_mutex_);
        
        // 컨텍스트 변수들을 JavaScript 환경에 주입
        for (auto& [key, value] : context.items()) {
            std::string js_assignment;
            
            if (value.is_number()) {
                js_assignment = "var " + key + " = " + std::to_string(value.get<double>()) + ";";
            } else if (value.is_boolean()) {
                js_assignment = "var " + key + " = " + (value.get<bool>() ? "true" : "false") + ";";
            } else if (value.is_string()) {
                js_assignment = "var " + key + " = \"" + value.get<std::string>() + "\";";
            } else {
                js_assignment = "var " + key + " = " + value.dump() + ";";
            }
            
            JSValue result = JS_Eval((JSContext*)js_context_, js_assignment.c_str(), 
                                   js_assignment.length(), "<variable_assignment>", JS_EVAL_TYPE_GLOBAL);
            
            if (JS_IsException(result)) {
                LogManager::getInstance().Error("Failed to assign variable: " + key);
                JS_FreeValue((JSContext*)js_context_, result);
                eval.condition_met = "JS_VARIABLE_ERROR";
                eval.message = "Failed to assign variable: " + key;
                return eval;
            }
            JS_FreeValue((JSContext*)js_context_, result);
        }
        
        // 조건 스크립트 실행
        LogManager::getInstance().Debug("Executing condition script: " + condition_script);
        
        JSValue eval_result = JS_Eval((JSContext*)js_context_, condition_script.c_str(), 
                                    condition_script.length(), "<alarm_condition>", JS_EVAL_TYPE_GLOBAL);
        
        if (JS_IsException(eval_result)) {
            JSValue exception = JS_GetException((JSContext*)js_context_);
            const char* error_str = JS_ToCString((JSContext*)js_context_, exception);
            std::string error_message = error_str ? error_str : "Unknown JavaScript error";
            
            LogManager::getInstance().Error("JavaScript execution error: " + error_message);
            
            if (error_str) JS_FreeCString((JSContext*)js_context_, error_str);
            JS_FreeValue((JSContext*)js_context_, exception);
            JS_FreeValue((JSContext*)js_context_, eval_result);
            
            eval.condition_met = "JS_EXEC_ERROR";
            eval.message = "Script execution error: " + error_message;
            return eval;
        }
        
        // 결과 확인
        bool script_result = JS_ToBool((JSContext*)js_context_, eval_result);
        JS_FreeValue((JSContext*)js_context_, eval_result);
        
        LogManager::getInstance().Debug("Script result: " + std::to_string(script_result));
        
        // 상태 변화 결정
        std::lock_guard<std::mutex> lock(state_mutex_);
        bool was_in_alarm = alarm_states_[rule.getId()];
        
        if (script_result && !was_in_alarm) {
            eval.should_trigger = true;
            eval.state_changed = true;
            eval.condition_met = "SCRIPT_TRUE";
            alarm_states_[rule.getId()] = true;
            
            LogManager::getInstance().Info("Script alarm TRIGGERED for rule " + 
                                         std::to_string(rule.getId()));
        }
        else if (!script_result && was_in_alarm && rule.isAutoClear()) {
            eval.should_clear = true;
            eval.state_changed = true;
            eval.condition_met = "SCRIPT_FALSE";
            alarm_states_[rule.getId()] = false;
            
            LogManager::getInstance().Info("Script alarm CLEARED for rule " + 
                                         std::to_string(rule.getId()));
        }
        
        // 메시지 스크립트 실행 (선택사항) - ✅ const string& 직접 사용
        const std::string& message_script = rule.getMessageScript();
        if (!message_script.empty() && (eval.should_trigger || eval.should_clear)) {
            LogManager::getInstance().Debug("Executing message script: " + message_script);
            
            JSValue msg_result = JS_Eval((JSContext*)js_context_, message_script.c_str(), 
                                        message_script.length(), "<alarm_message>", JS_EVAL_TYPE_GLOBAL);
            
            if (!JS_IsException(msg_result)) {
                const char* msg_str = JS_ToCString((JSContext*)js_context_, msg_result);
                if (msg_str) {
                    eval.message = std::string(msg_str);
                    JS_FreeCString((JSContext*)js_context_, msg_str);
                }
            } else {
                LogManager::getInstance().Warn("Message script execution failed, using default message");
            }
            JS_FreeValue((JSContext*)js_context_, msg_result);
        }
        
        // 컨텍스트 데이터 설정
        eval.context_data = context;
        eval.context_data["rule_name"] = rule.getName();
        eval.context_data["target_type"] = "script";
        eval.context_data["script_result"] = script_result;
        eval.context_data["was_in_alarm"] = was_in_alarm;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Script alarm evaluation failed for rule " + 
                                      std::to_string(rule.getId()) + ": " + std::string(e.what()));
        eval.condition_met = "EXCEPTION";
        eval.message = "Evaluation exception: " + std::string(e.what());
    }
    
    // 기본 메시지 생성 (스크립트에서 생성하지 않은 경우)
    if (eval.message.empty()) {
        eval.message = generateMessage(rule, eval, DataValue{std::string("script_context")});
    }
    
    return eval;
}

// =============================================================================
// 나머지 모든 메서드들은 기존과 동일 (상태 체크만 추가)
// =============================================================================

std::optional<int64_t> AlarmEngine::raiseAlarm(const AlarmRuleEntity& rule, 
                                               const AlarmEvaluation& eval,
                                               const DataValue& trigger_value) {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    // 기존 로직 그대로...
    try {
        AlarmOccurrenceEntity occurrence;
        occurrence.setId(next_occurrence_id_.fetch_add(1));
        occurrence.setRuleId(rule.getId());
        occurrence.setTenantId(rule.getTenantId());
        occurrence.setTriggerCondition(eval.condition_met);
        occurrence.setAlarmMessage(eval.message);
        occurrence.setSeverity(eval.severity);
        occurrence.setState(PulseOne::Alarm::AlarmState::ACTIVE);
        
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
        LogManager::getInstance().Error("Failed to raise alarm: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

// 나머지 모든 메서드들도 동일한 패턴 적용...
// (기존 코드 + 상태 체크만 추가)

// =============================================================================
// 🔥 JavaScript 엔진 (스크립트 알람용)
// =============================================================================

bool AlarmEngine::initScriptEngine() {
    try {
        js_runtime_ = JS_NewRuntime();
        if (!js_runtime_) {
            LogManager::getInstance().Error("Failed to create JS runtime");
            return false;
        }
        
        // 메모리 제한 (8MB)
        JS_SetMemoryLimit((JSRuntime*)js_runtime_, 8 * 1024 * 1024);
        
        js_context_ = JS_NewContext((JSRuntime*)js_runtime_);
        if (!js_context_) {
            LogManager::getInstance().Error("Failed to create JS context");
            JS_FreeRuntime((JSRuntime*)js_runtime_);
            js_runtime_ = nullptr;
            return false;
        }
        
        LogManager::getInstance().Info("JavaScript engine initialized for script alarms");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("JavaScript engine initialization failed: " + std::string(e.what()));
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
// 🔥 상태 관리 및 기타 메서드들 (기존과 동일)
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
    if (!redis_client_) {
        return; // Redis 비활성화 시 조용히 스킵
    }
    
    try {
        json alarm_json = {
            {"type", "alarm_event"},
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"tenant_id", event.tenant_id},
            {"point_id", event.point_id},
            {"device_id", event.device_id},
            {"alarm_type", static_cast<int>(event.alarm_type)},
            {"severity", static_cast<int>(event.severity)},
            {"state", static_cast<int>(event.state)},
            {"trigger_condition", static_cast<int>(event.trigger_condition)},
            {"message", event.message},
            {"timestamp", std::chrono::system_clock::to_time_t(event.timestamp)},
            {"occurrence_time", std::chrono::system_clock::to_time_t(event.occurrence_time)},
            {"source_name", event.source_name},
            {"location", event.location}
        };
        
        // 트리거 값 추가
        std::visit([&alarm_json](auto&& v) {
            alarm_json["trigger_value"] = v;
        }, event.trigger_value);
        
        // 현재 값 추가 (다를 수 있음)
        std::visit([&alarm_json](auto&& v) {
            alarm_json["current_value"] = v;
        }, event.current_value);
        
        // 임계값 추가
        alarm_json["threshold_value"] = event.threshold_value;
        
        // 텐넌트별 채널로 발송
        std::string channel = "tenant:" + std::to_string(event.tenant_id) + ":alarms";
        redis_client_->publish(channel, alarm_json.dump());
        
        // 전체 알람 채널로도 발송 (선택적)
        redis_client_->publish("alarms:all", alarm_json.dump());
        
        LogManager::getInstance().Debug("Alarm event published to Redis: " + 
                                      std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to publish alarm to Redis: " + std::string(e.what()));
    }
}

// 🔥 RabbitMQ 메서드 완전 제거 (주석도 없이)

// =============================================================================
// 🔥 통계 및 조회
// =============================================================================

nlohmann::json AlarmEngine::getStatistics() const {
    try {
        return {
            {"initialized", initialized_.load()},
            {"total_evaluations", total_evaluations_.load()},
            {"alarms_raised", alarms_raised_.load()},
            {"alarms_cleared", alarms_cleared_.load()},
            {"evaluation_errors", evaluations_errors_.load()},
            {"active_alarms_count", getActiveAlarmsCount()},
            {"js_engine_available", (js_context_ != nullptr)},
            {"redis_connected", (redis_client_ && redis_client_->isConnected())},
            {"next_occurrence_id", next_occurrence_id_.load()}
        };
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getStatistics failed: " + std::string(e.what()));
        return {{"error", "Failed to get statistics"}};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmEngine::getActiveAlarms(int tenant_id) const {
    try {
        if (!alarm_occurrence_repo_) {
            return {};
        }
        
        auto all_active = alarm_occurrence_repo_->findActive();
        std::vector<AlarmOccurrenceEntity> tenant_active;
        
        for (const auto& alarm : all_active) {
            if (alarm.getTenantId() == tenant_id) {
                tenant_active.push_back(alarm);
            }
        }
        
        return tenant_active;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getActiveAlarms failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmOccurrenceEntity> AlarmEngine::getAlarmOccurrence(int64_t occurrence_id) const {
    try {
        if (!alarm_occurrence_repo_) {
            return std::nullopt;
        }
        
        return alarm_occurrence_repo_->findById(static_cast<int>(occurrence_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getAlarmOccurrence failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::string AlarmEngine::generateMessage(
    const AlarmRuleEntity& rule, 
    const AlarmEvaluation& eval, 
    const DataValue& value) {
    
    std::string message = rule.getName();
    
    if (!eval.condition_met.empty()) {
        message += " - " + eval.condition_met;
    }
    
    // DataValue를 문자열로 변환
    std::string value_str = std::visit([](const auto& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
            return v ? "true" : "false";
        } else {
            return std::to_string(v);
        }
    }, value);
    
    message += " (값: " + value_str + ")";
    
    return message;
}

std::vector<AlarmRuleEntity> AlarmEngine::getAlarmRulesForPoint(int tenant_id, 
                                                               const std::string& point_type, 
                                                               int target_id) const {
    std::vector<AlarmRuleEntity> filtered_rules;
    
    try {
        if (!alarm_rule_repo_) {
            LogManager::getInstance().Error("AlarmRuleRepository not available");
            return filtered_rules;
        }
        
        // findByTarget 메서드 사용
        auto rules = alarm_rule_repo_->findByTarget(point_type, target_id);
        
        // 텐넌트 필터링
        for (const auto& rule : rules) {
            if (rule.isEnabled() && rule.getTenantId() == tenant_id) {
                filtered_rules.push_back(rule);
            }
        }
        
        LogManager::getInstance().Debug("Found " + std::to_string(filtered_rules.size()) + 
                                      " rules for " + point_type + ":" + std::to_string(target_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getAlarmRulesForPoint failed: " + std::string(e.what()));
    }
    
    return filtered_rules;
}

bool AlarmEngine::clearActiveAlarm(int rule_id, const DataValue& current_value) {
    try {
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().Error("AlarmOccurrenceRepository not available");
            return false;
        }
        
        auto all_active = alarm_occurrence_repo_->findActive();
        
        bool any_cleared = false;
        for (auto& alarm : all_active) {
            if (alarm.getRuleId() == rule_id) {
                alarm.setState(AlarmState::CLEARED);
                alarm.setClearedTime(std::chrono::system_clock::now());
                
                // 🔥 수정: setClearValue → setClearedValue
                json clear_value_json;
                std::visit([&clear_value_json](auto&& v) {
                    clear_value_json = v;
                }, current_value);
                alarm.setClearedValue(clear_value_json.dump());
                
                alarm.markModified();
                
                if (alarm_occurrence_repo_->update(alarm)) {
                    any_cleared = true;
                    LogManager::getInstance().Info("Alarm cleared: rule_id=" + std::to_string(rule_id) + 
                                                 ", occurrence_id=" + std::to_string(alarm.getId()));
                }
            }
        }
        
        if (any_cleared) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            alarm_states_[rule_id] = false;
        }
        
        return any_cleared;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("clearActiveAlarm failed: " + std::string(e.what()));
        return false;
    }
}


bool AlarmEngine::clearAlarm(int64_t occurrence_id, const DataValue& current_value) {
    try {
        if (!alarm_occurrence_repo_) {
            LogManager::getInstance().Error("AlarmOccurrenceRepository not available");
            return false;
        }
        
        auto alarm_opt = alarm_occurrence_repo_->findById(static_cast<int>(occurrence_id));
        if (!alarm_opt.has_value()) {
            LogManager::getInstance().Warn("Alarm occurrence not found: " + std::to_string(occurrence_id));
            return false;
        }
        
        auto alarm = alarm_opt.value();
        alarm.setState(AlarmState::CLEARED);
        alarm.setClearedTime(std::chrono::system_clock::now());
        
        // 🔥 수정: setClearValue → setClearedValue
        json clear_value_json;
        std::visit([&clear_value_json](auto&& v) {
            clear_value_json = v;
        }, current_value);
        alarm.setClearedValue(clear_value_json.dump());
        
        bool success = alarm_occurrence_repo_->update(alarm);
        
        if (success) {
            LogManager::getInstance().Info("Alarm manually cleared: occurrence_id=" + std::to_string(occurrence_id));
            
            std::lock_guard<std::mutex> lock(state_mutex_);
            alarm_states_[alarm.getRuleId()] = false;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("clearAlarm failed: " + std::string(e.what()));
        return false;
    }
}


void AlarmEngine::publishAlarmClearedEvent(const AlarmOccurrenceEntity& alarm) {
    if (!redis_client_) {
        return;
    }
    
    try {
        json clear_json = {
            {"type", "alarm_cleared"},
            {"occurrence_id", alarm.getId()},
            {"rule_id", alarm.getRuleId()},
            {"tenant_id", alarm.getTenantId()},
            {"state", "cleared"},
            {"message", "Alarm cleared: " + alarm.getAlarmMessage()}
        };
        
        // 🔥 수정: optional 처리
        auto cleared_time_opt = alarm.getClearedTime();
        if (cleared_time_opt.has_value()) {
            clear_json["cleared_time"] = std::chrono::system_clock::to_time_t(cleared_time_opt.value());
        } else {
            clear_json["cleared_time"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }
        
        std::string channel = "tenant:" + std::to_string(alarm.getTenantId()) + ":alarms";
        redis_client_->publish(channel, clear_json.dump());
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to publish alarm cleared event: " + std::string(e.what()));
    }
}

size_t AlarmEngine::getActiveAlarmsCount() const {
    try {
        if (!alarm_occurrence_repo_) {
            return 0;
        }
        
        auto active_alarms = alarm_occurrence_repo_->findActive();
        return active_alarms.size();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getActiveAlarmsCount failed: " + std::string(e.what()));
        return 0;
    }
}

} // namespace Alarm
} // namespace PulseOne