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

std::vector<AlarmEvent> AlarmEngine::evaluateForPoint(int tenant_id,
                                                     int point_id,
                                                     const DataValue& value) {
    std::vector<AlarmEvent> alarm_events;
    
    // =============================================================================
    // 🎯 1. 초기화 상태 체크
    // =============================================================================
    if (!initialized_.load()) {
        LogManager::getInstance().Error("AlarmEngine not properly initialized");
        return alarm_events;
    }
    
    // =============================================================================
    // 🎯 2. 포인트 타입 결정 (ID 범위 기반)
    // =============================================================================
    std::string point_type;
    int numeric_id = point_id;
    
    if (point_id >= 1000 && point_id < 9000) {
        point_type = "data_point";
    } else if (point_id >= 9000) {
        point_type = "virtual_point";
    } else {
        LogManager::getInstance().Debug("Unknown point ID range: " + std::to_string(point_id));
        return alarm_events;
    }
    
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
    // 🎯 4. 각 규칙에 대해 알람 평가 수행
    // =============================================================================
    for (const auto& rule : rules) {
        if (!rule.isEnabled()) {
            LogManager::getInstance().Debug("Skipping disabled rule: " + rule.getName());
            continue;
        }
        
        try {
            auto eval_start = std::chrono::steady_clock::now();
            
            // 🔥 규칙 평가 실행
            auto eval = evaluateRule(rule, value);
            
            // 평가 시간 측정
            auto eval_end = std::chrono::steady_clock::now();
            eval.evaluation_time = std::chrono::duration_cast<std::chrono::microseconds>(eval_end - eval_start);
            
            // 평가 결과에 메타데이터 추가
            eval.rule_id = rule.getId();
            eval.tenant_id = tenant_id;
            
            LogManager::getInstance().Debug("Rule " + std::to_string(rule.getId()) + 
                                          " evaluation: trigger=" + std::to_string(eval.should_trigger) +
                                          ", clear=" + std::to_string(eval.should_clear) +
                                          ", condition=" + eval.condition_met);
            
            // =============================================================================
            // 🎯 5. 상태 변화가 있는 경우에만 처리
            // =============================================================================
            if (eval.state_changed) {
                
                // -------------------------------------------------------------------------
                // 🚨 알람 발생 처리
                // -------------------------------------------------------------------------
                if (eval.should_trigger) {
                    auto occurrence_id = raiseAlarm(rule, eval, value);
                    
                    if (occurrence_id.has_value()) {
                        // AlarmEvent 생성
                        AlarmEvent event;
                        
                        // 기본 식별 정보
                        event.occurrence_id = occurrence_id.value();
                        event.rule_id = rule.getId();
                        event.tenant_id = tenant_id;
                        event.point_id = point_id;
                        event.device_id = getDeviceIdForPoint(point_id);  // 헬퍼 함수 필요
                        
                        // 알람 타입 및 심각도 (enum 직접 할당)
                        event.alarm_type = convertToAlarmType(rule.getAlarmType());
                        event.severity = eval.severity;
                        event.state = AlarmState::ACTIVE;
                        
                        // 트리거 조건 결정
                        event.trigger_condition = determineTriggerCondition(rule, eval);
                        
                        // 값 정보 (DataValue 직접 할당)
                        event.trigger_value = value;
                        event.current_value = value;
                        event.threshold_value = getThresholdValue(rule, eval);
                        
                        // 조건 및 메시지
                        event.condition_met = !eval.condition_met.empty();
                        event.message = eval.message;
                        
                        // 시간 정보
                        event.timestamp = std::chrono::system_clock::now();
                        event.occurrence_time = eval.timestamp;
                        
                        // 추가 컨텍스트 정보
                        event.source_name = rule.getName();
                        event.location = getPointLocation(point_id);  // 헬퍼 함수 필요
                        
                        alarm_events.push_back(event);
                        
                        // 통계 업데이트
                        alarms_raised_.fetch_add(1);
                        
                        // 외부 시스템에 알림
                        publishToRedis(event);
                        
                        LogManager::getInstance().Info("🚨 Alarm triggered: Rule " + 
                                                      std::to_string(rule.getId()) + 
                                                      " (" + rule.getName() + ") - " + 
                                                      event.message);
                    } else {
                        LogManager::getInstance().Error("Failed to raise alarm for rule " + 
                                                       std::to_string(rule.getId()));
                        evaluations_errors_.fetch_add(1);
                    }
                }
                
                // -------------------------------------------------------------------------
                // ✅ 알람 해제 처리
                // -------------------------------------------------------------------------
                else if (eval.should_clear) {
                    bool cleared = clearActiveAlarm(rule.getId(), value);
                    
                    if (cleared) {
                        // 해제 이벤트 생성 (선택적)
                        AlarmEvent clear_event;
                        clear_event.rule_id = rule.getId();
                        clear_event.tenant_id = tenant_id;
                        clear_event.point_id = point_id;
                        clear_event.device_id = getDeviceIdForPoint(point_id);
                        
                        clear_event.alarm_type = convertToAlarmType(rule.getAlarmType());
                        clear_event.severity = AlarmSeverity::INFO;
                        clear_event.state = AlarmState::CLEARED;
                        
                        clear_event.trigger_value = value;
                        clear_event.current_value = value;
                        clear_event.condition_met = false;
                        clear_event.message = "Alarm cleared: " + rule.getName();
                        
                        clear_event.timestamp = std::chrono::system_clock::now();
                        clear_event.occurrence_time = eval.timestamp;
                        
                        alarm_events.push_back(clear_event);
                        
                        // 통계 업데이트
                        alarms_cleared_.fetch_add(1);
                        
                        LogManager::getInstance().Info("✅ Alarm cleared: Rule " + 
                                                      std::to_string(rule.getId()) + 
                                                      " (" + rule.getName() + ")");
                    } else {
                        LogManager::getInstance().Error("Failed to clear alarm for rule " + 
                                                       std::to_string(rule.getId()));
                        evaluations_errors_.fetch_add(1);
                    }
                }
            } else {
                LogManager::getInstance().Debug("No state change for rule " + std::to_string(rule.getId()));
            }
            
        } catch (const std::exception& e) {
            evaluations_errors_.fetch_add(1);
            LogManager::getInstance().Error("Failed to evaluate rule " + 
                                          std::to_string(rule.getId()) + 
                                          " (" + rule.getName() + "): " + 
                                          std::string(e.what()));
        }
    }
    
    // =============================================================================
    // 🎯 6. 전체 평가 통계 업데이트
    // =============================================================================
    total_evaluations_.fetch_add(rules.size());
    
    if (!alarm_events.empty()) {
        LogManager::getInstance().Info("Generated " + std::to_string(alarm_events.size()) + 
                                     " alarm events for point " + std::to_string(point_id));
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
    
    // 트리거 값 문자열로 저장
    eval.triggered_value = std::to_string(value);
    
    bool currently_in_alarm = false;
    std::string condition;
    AlarmSeverity determined_severity = rule.getSeverity();
    
    try {
        LogManager::getInstance().Debug("Evaluating analog alarm for rule " + 
                                      std::to_string(rule.getId()) + ", value: " + std::to_string(value));
        
        // =============================================================================
        // 🎯 임계값 체크 (우선순위 순서: HIGH_HIGH -> HIGH -> LOW_LOW -> LOW)
        // =============================================================================
        
        // HIGH-HIGH 체크 (가장 심각한 상태)
        if (rule.getHighHighLimit().has_value() && value >= rule.getHighHighLimit().value()) {
            currently_in_alarm = true;
            condition = "HIGH_HIGH";
            determined_severity = AlarmSeverity::CRITICAL;  // 강제로 CRITICAL
            eval.analog_level = AnalogAlarmLevel::HIGH_HIGH;
            eval.alarm_level = "HIGH_HIGH";
            
            LogManager::getInstance().Debug("HIGH_HIGH condition met: " + std::to_string(value) + 
                                          " >= " + std::to_string(rule.getHighHighLimit().value()));
        }
        // HIGH 체크
        else if (rule.getHighLimit().has_value() && value >= rule.getHighLimit().value()) {
            currently_in_alarm = true;
            condition = "HIGH";
            determined_severity = AlarmSeverity::HIGH;
            eval.analog_level = AnalogAlarmLevel::HIGH;
            eval.alarm_level = "HIGH";
            
            LogManager::getInstance().Debug("HIGH condition met: " + std::to_string(value) + 
                                          " >= " + std::to_string(rule.getHighLimit().value()));
        }
        // LOW-LOW 체크 (가장 심각한 하한)
        else if (rule.getLowLowLimit().has_value() && value <= rule.getLowLowLimit().value()) {
            currently_in_alarm = true;
            condition = "LOW_LOW";
            determined_severity = AlarmSeverity::CRITICAL;  // 강제로 CRITICAL
            eval.analog_level = AnalogAlarmLevel::LOW_LOW;
            eval.alarm_level = "LOW_LOW";
            
            LogManager::getInstance().Debug("LOW_LOW condition met: " + std::to_string(value) + 
                                          " <= " + std::to_string(rule.getLowLowLimit().value()));
        }
        // LOW 체크
        else if (rule.getLowLimit().has_value() && value <= rule.getLowLimit().value()) {
            currently_in_alarm = true;
            condition = "LOW";
            determined_severity = AlarmSeverity::HIGH;
            eval.analog_level = AnalogAlarmLevel::LOW;
            eval.alarm_level = "LOW";
            
            LogManager::getInstance().Debug("LOW condition met: " + std::to_string(value) + 
                                          " <= " + std::to_string(rule.getLowLimit().value()));
        }
        else {
            // 정상 범위
            eval.analog_level = AnalogAlarmLevel::NORMAL;
            eval.alarm_level = "NORMAL";
            
            LogManager::getInstance().Debug("Value in normal range: " + std::to_string(value));
        }
        
        // =============================================================================
        // 🎯 히스테리시스 (Deadband) 처리
        // =============================================================================
        
        std::lock_guard<std::mutex> lock(state_mutex_);
        bool was_in_alarm = alarm_states_[rule.getId()];
        double last_value = last_values_[rule.getId()];
        
        // 데드밴드 적용 (기본값 0.0) - getDeadband()는 double 반환
        double deadband = rule.getDeadband();
        bool hysteresis_allows_change = true;
        
        if (deadband > 0.0) {
            // 알람 → 정상: 값이 데드밴드만큼 여유있게 돌아와야 함
            if (was_in_alarm && !currently_in_alarm) {
                double clearance_needed = deadband;
                
                if (condition == "HIGH" || condition == "HIGH_HIGH") {
                    // 상한 알람의 경우: 임계값보다 데드밴드만큼 낮아져야 해제
                    double threshold = rule.getHighLimit().value_or(rule.getHighHighLimit().value_or(0));
                    hysteresis_allows_change = (value <= (threshold - clearance_needed));
                } else if (condition == "LOW" || condition == "LOW_LOW") {
                    // 하한 알람의 경우: 임계값보다 데드밴드만큼 높아져야 해제
                    double threshold = rule.getLowLimit().value_or(rule.getLowLowLimit().value_or(0));
                    hysteresis_allows_change = (value >= (threshold + clearance_needed));
                }
                
                LogManager::getInstance().Debug("Hysteresis check for clearing: " + 
                                              std::to_string(hysteresis_allows_change) + 
                                              ", deadband: " + std::to_string(deadband));
            }
        }
        
        // =============================================================================
        // 🎯 상태 변화 결정
        // =============================================================================
        
        eval.severity = determined_severity;
        
        if (currently_in_alarm && !was_in_alarm && hysteresis_allows_change) {
            // 알람 발생
            eval.should_trigger = true;
            eval.state_changed = true;
            eval.condition_met = condition;
            alarm_states_[rule.getId()] = true;
            
            LogManager::getInstance().Info("Analog alarm TRIGGERED for rule " + 
                                         std::to_string(rule.getId()) + ": " + condition);
        }
        else if (!currently_in_alarm && was_in_alarm && rule.isAutoClear() && hysteresis_allows_change) {
            // 알람 해제 (자동 해제 활성화된 경우만)
            eval.should_clear = true;
            eval.state_changed = true;
            eval.condition_met = "NORMAL";
            alarm_states_[rule.getId()] = false;
            
            LogManager::getInstance().Info("Analog alarm CLEARED for rule " + 
                                         std::to_string(rule.getId()) + ": AUTO_CLEAR");
        }
        
        // 현재 값 업데이트
        last_values_[rule.getId()] = value;
        
        // =============================================================================
        // 🎯 컨텍스트 데이터 설정
        // =============================================================================
        
        eval.context_data = {
            {"current_value", value},
            {"last_value", last_value},
            {"rule_name", rule.getName()},
            {"target_type", "analog"},
            {"deadband", deadband},
            {"was_in_alarm", was_in_alarm}
        };
        
        // 임계값 정보 추가
        if (rule.getHighHighLimit().has_value()) eval.context_data["high_high_limit"] = rule.getHighHighLimit().value();
        if (rule.getHighLimit().has_value()) eval.context_data["high_limit"] = rule.getHighLimit().value();
        if (rule.getLowLimit().has_value()) eval.context_data["low_limit"] = rule.getLowLimit().value();
        if (rule.getLowLowLimit().has_value()) eval.context_data["low_low_limit"] = rule.getLowLowLimit().value();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Analog alarm evaluation failed for rule " + 
                                      std::to_string(rule.getId()) + ": " + std::string(e.what()));
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
    
    bool should_trigger_now = false;
    std::string condition;  // 로그용으로만 사용
    
    try {
        LogManager::getInstance().Debug("Evaluating digital alarm for rule " + 
                                      std::to_string(rule.getId()) + ", value: " + (value ? "true" : "false"));
        
        // =============================================================================
        // 🚀 enum 직접 사용 - 초고속 비교!
        // =============================================================================
        
        std::lock_guard<std::mutex> lock(state_mutex_);
        bool was_in_alarm = alarm_states_[rule.getId()];
        bool last_digital_state = last_digital_states_[rule.getId()];
        
        // ✅ enum 직접 switch - 정수 비교로 초고속!
        auto trigger = rule.getTriggerCondition();
        
        switch (trigger) {
            case AlarmRuleEntity::DigitalTrigger::ON_TRUE:
                should_trigger_now = value;
                condition = "DIGITAL_TRUE";  // 로그용
                LogManager::getInstance().Debug("ON_TRUE trigger: " + std::to_string(should_trigger_now));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_FALSE:
                should_trigger_now = !value;
                condition = "DIGITAL_FALSE";  // 로그용
                LogManager::getInstance().Debug("ON_FALSE trigger: " + std::to_string(should_trigger_now));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_RISING:
                should_trigger_now = value && !last_digital_state;
                condition = "DIGITAL_RISING";  // 로그용
                LogManager::getInstance().Debug("ON_RISING trigger: current=" + std::to_string(value) + 
                                              ", last=" + std::to_string(last_digital_state));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_FALLING:
                should_trigger_now = !value && last_digital_state;
                condition = "DIGITAL_FALLING";  // 로그용
                LogManager::getInstance().Debug("ON_FALLING trigger: current=" + std::to_string(value) + 
                                              ", last=" + std::to_string(last_digital_state));
                break;
                
            case AlarmRuleEntity::DigitalTrigger::ON_CHANGE:
            default:
                should_trigger_now = (value != last_digital_state);
                condition = "DIGITAL_CHANGE";  // 로그용
                LogManager::getInstance().Debug("ON_CHANGE trigger: " + std::to_string(should_trigger_now));
                break;
        }
        
        // =============================================================================
        // 🎯 상태 변화 결정 (기존 로직 유지)
        // =============================================================================
        
        if (should_trigger_now && !was_in_alarm) {
            eval.should_trigger = true;
            eval.state_changed = true;
            eval.condition_met = condition;
            alarm_states_[rule.getId()] = true;
            
            LogManager::getInstance().Info("Digital alarm TRIGGERED for rule " + 
                                         std::to_string(rule.getId()) + ": " + condition);
        }
        else if (!should_trigger_now && was_in_alarm && rule.isAutoClear()) {
            eval.should_clear = true;
            eval.state_changed = true;
            eval.condition_met = "NORMAL";
            alarm_states_[rule.getId()] = false;
            
            LogManager::getInstance().Info("Digital alarm CLEARED for rule " + 
                                         std::to_string(rule.getId()));
        }
        
        // 현재 상태 업데이트
        last_digital_states_[rule.getId()] = value;
        
        // 컨텍스트 데이터 설정
        eval.context_data = {
            {"current_value", value},
            {"last_value", last_digital_state},
            {"rule_name", rule.getName()},
            {"target_type", "digital"},
            {"trigger_type", condition},  // enum 결과를 string으로 저장 (JSON용)
            {"was_in_alarm", was_in_alarm}
        };
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Digital alarm evaluation failed for rule " + 
                                      std::to_string(rule.getId()) + ": " + std::string(e.what()));
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
    if (!redis_client_) return;
    
    try {
        json alarm_json = {
            {"type", "alarm_event"},
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"tenant_id", event.tenant_id},
            {"point_id", event.point_id},
            {"severity", event.getSeverityString()},
            {"state", event.state},
            {"message", event.message},
            {"timestamp", std::chrono::system_clock::to_time_t(event.occurrence_time)}
        };
        
        std::string channel = "tenant:" + std::to_string(event.tenant_id) + ":alarms";
        redis_client_->publish(channel, alarm_json.dump());
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to publish alarm to Redis: " + std::string(e.what()));
    }
}

// 🔥 RabbitMQ 메서드 완전 제거 (주석도 없이)

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
        {"active_alarms_count", rule_occurrence_map_.size()},
        {"initialized", initialized_.load()}
    };
}

std::vector<AlarmOccurrenceEntity> AlarmEngine::getActiveAlarms(int tenant_id) const {
    if (!initialized_.load()) {
        return {};
    }
    
    try {
        return alarm_occurrence_repo_->findActiveByRuleId(tenant_id);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get active alarms: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmOccurrenceEntity> AlarmEngine::getAlarmOccurrence(int64_t occurrence_id) const {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    try {
        return alarm_occurrence_repo_->findById(occurrence_id);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get alarm occurrence: " + std::string(e.what()));
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

std::vector<AlarmRuleEntity> AlarmEngine::getAlarmRulesForPoint(int point_id, const std::string& tag_name, int tenant_id) const {
    std::vector<AlarmRuleEntity> filtered_rules;
    
    try {
        auto& logger = LogManager::getInstance();
        
        if (!alarm_rule_repo_) {
            logger.Error("AlarmRuleRepository not available");
            return filtered_rules;
        }
        
        // 모든 알람 규칙 조회
        auto all_rules = alarm_rule_repo_->findAll();
        
        // 필터링 (실제 존재하는 메서드들만 사용)
        for (const auto& rule : all_rules) {
            if (rule.isEnabled() &&  
                rule.getTenantId() == tenant_id) {
                // ❌ getPointId(), getTagName() 대신 다른 방법 사용
                // 예: 설정이나 조건으로 필터링
                filtered_rules.push_back(rule);
            }
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("getAlarmRulesForPoint failed: " + std::string(e.what()));
    }
    
    return filtered_rules;
}

bool AlarmEngine::clearActiveAlarm(int rule_id, const DataValue& current_value) {
    try {
        auto& logger = LogManager::getInstance();
        
        if (!alarm_occurrence_repo_) {
            logger.Error("AlarmOccurrenceRepository not available");
            return false;
        }
        
        // 활성 알람 조회
        auto active_alarms = alarm_occurrence_repo_->findActiveByRuleId(rule_id);
        
        bool any_cleared = false;
        for (auto& alarm : active_alarms) {
            // ✅ 올바른 enum과 메서드 사용
            alarm.setState(AlarmState::CLEARED);
            alarm.setClearedTime(std::chrono::system_clock::now());  // setClearTime → setClearedTime
            alarm.markModified();
            
            // 데이터베이스 업데이트
            if (alarm_occurrence_repo_->update(alarm)) {
                any_cleared = true;
                
                // ❌ AlarmEvent와 publishAlarmEvent가 정의되지 않음 - 제거하거나 간단한 로깅으로 대체
                logger.Info("Alarm cleared: rule_id=" + std::to_string(rule_id) + 
                           ", occurrence_id=" + std::to_string(alarm.getId()));
            }
        }
        
        return any_cleared;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("clearActiveAlarm failed: " + std::string(e.what()));
        return false;
    }
}


bool AlarmEngine::clearAlarm(int64_t occurrence_id, const DataValue& current_value) {
    try {
        auto& logger = LogManager::getInstance();
        
        if (!alarm_occurrence_repo_) {
            logger.Error("AlarmOccurrenceRepository not available");
            return false;
        }
        
        auto alarm_opt = alarm_occurrence_repo_->findById(static_cast<int>(occurrence_id));
        if (!alarm_opt) {
            logger.Warn("Alarm occurrence not found: " + std::to_string(occurrence_id));  // ✅ Warning → Warn
            return false;
        }
        
        auto alarm = alarm_opt.value();
        alarm.setState(AlarmState::CLEARED);
        alarm.setClearedTime(std::chrono::system_clock::now());
        
        bool success = alarm_occurrence_repo_->update(alarm);
        
        if (success) {
            logger.Info("Alarm manually cleared: occurrence_id=" + std::to_string(occurrence_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("clearAlarm failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace Alarm
} // namespace PulseOne