// =============================================================================
// collector/src/Alarm/AlarmManager.cpp
// PulseOne 알람 매니저 구현 - 완성본
// =============================================================================

#include "Alarm/AlarmManager.h"
#include "Client/RedisClientImpl.h"
#include "Client/RabbitMQClient.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 생성자/소멸자
// =============================================================================

AlarmManager::AlarmManager() 
    : logger_(Utils::LogManager::getInstance()) {
    logger_.Debug("AlarmManager constructor");
}

AlarmManager::~AlarmManager() {
    shutdown();
}

AlarmManager& AlarmManager::getInstance() {
    static AlarmManager instance;
    return instance;
}

// =============================================================================
// 초기화/종료
// =============================================================================

bool AlarmManager::initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                              std::shared_ptr<RedisClientImpl> redis_client,
                              std::shared_ptr<RabbitMQClient> mq_client) {
    if (initialized_) {
        logger_.Warn("AlarmManager already initialized");
        return true;
    }
    
    try {
        db_manager_ = db_manager;
        redis_client_ = redis_client;
        mq_client_ = mq_client;
        
        // 다음 occurrence ID 로드 (데이터베이스에서 최대값 조회)
        std::string query = "SELECT MAX(id) as max_id FROM alarm_occurrences";
        auto results = db_manager_->executeQuery(query);
        if (!results.empty() && results[0].count("max_id")) {
            try {
                next_occurrence_id_ = std::stoll(results[0].at("max_id")) + 1;
            } catch (...) {
                next_occurrence_id_ = 1;
            }
        }
        
        initialized_ = true;
        logger_.Info("AlarmManager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("AlarmManager initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AlarmManager::shutdown() {
    if (!initialized_) return;
    
    logger_.Info("Shutting down AlarmManager");
    alarm_rules_.clear();
    point_alarm_map_.clear();
    active_alarms_.clear();
    initialized_ = false;
}

// =============================================================================
// 알람 규칙 로드
// =============================================================================

bool AlarmManager::loadAlarmRules(int tenant_id) {
    try {
        std::string query = R"(
            SELECT id, name, description, target_type, target_id, target_group,
                   alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                   deadband, rate_of_change, trigger_condition, condition_script,
                   message_script, message_config, message_template, severity,
                   priority, auto_acknowledge, acknowledge_timeout_min, auto_clear,
                   suppression_rules, notification_enabled, notification_delay_sec,
                   notification_repeat_interval_min, notification_channels,
                   notification_recipients, is_enabled, is_latched
            FROM alarm_rules
            WHERE tenant_id = ? AND is_enabled = 1
        )";
        
        auto results = db_manager_->executeQuery(query, {std::to_string(tenant_id)});
        
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
        
        // 기존 규칙 정리
        alarm_rules_.clear();
        point_alarm_map_.clear();
        group_alarm_map_.clear();
        
        for (const auto& row : results) {
            AlarmRule rule;
            rule.id = std::stoi(row.at("id"));
            rule.tenant_id = tenant_id;
            rule.name = row.at("name");
            rule.description = row.at("description");
            
            // 대상 정보
            rule.target_type = row.at("target_type");
            if (!row.at("target_id").empty()) {
                rule.target_id = std::stoi(row.at("target_id"));
            }
            rule.target_group = row.at("target_group");
            
            // 알람 타입
            rule.alarm_type = row.at("alarm_type");
            
            // 아날로그 설정
            if (!row.at("high_high_limit").empty()) {
                rule.high_high_limit = std::stod(row.at("high_high_limit"));
            }
            if (!row.at("high_limit").empty()) {
                rule.high_limit = std::stod(row.at("high_limit"));
            }
            if (!row.at("low_limit").empty()) {
                rule.low_limit = std::stod(row.at("low_limit"));
            }
            if (!row.at("low_low_limit").empty()) {
                rule.low_low_limit = std::stod(row.at("low_low_limit"));
            }
            rule.deadband = std::stod(row.at("deadband"));
            rule.rate_of_change = std::stod(row.at("rate_of_change"));
            
            // 디지털 설정
            rule.trigger_condition = row.at("trigger_condition");
            
            // 스크립트
            rule.condition_script = row.at("condition_script");
            rule.message_script = row.at("message_script");
            
            // 메시지 설정
            if (!row.at("message_config").empty()) {
                rule.message_config = json::parse(row.at("message_config"));
            }
            rule.message_template = row.at("message_template");
            
            // 우선순위
            rule.severity = row.at("severity");
            rule.priority = std::stoi(row.at("priority"));
            
            // 자동 처리
            rule.auto_acknowledge = row.at("auto_acknowledge") == "1";
            rule.acknowledge_timeout_min = std::stoi(row.at("acknowledge_timeout_min"));
            rule.auto_clear = row.at("auto_clear") == "1";
            
            // 억제 규칙
            if (!row.at("suppression_rules").empty()) {
                rule.suppression_rules = json::parse(row.at("suppression_rules"));
            }
            
            // 알림 설정
            rule.notification_enabled = row.at("notification_enabled") == "1";
            rule.notification_delay_sec = std::stoi(row.at("notification_delay_sec"));
            rule.notification_repeat_interval_min = std::stoi(row.at("notification_repeat_interval_min"));
            
            if (!row.at("notification_channels").empty()) {
                rule.notification_channels = json::parse(row.at("notification_channels"));
            }
            if (!row.at("notification_recipients").empty()) {
                rule.notification_recipients = json::parse(row.at("notification_recipients"));
            }
            
            // 상태
            rule.is_enabled = true;
            rule.is_latched = row.at("is_latched") == "1";
            
            // 저장
            alarm_rules_[rule.id] = rule;
            
            // 인덱스 구축
            if (rule.target_id > 0) {
                point_alarm_map_[rule.target_id].push_back(rule.id);
            }
            if (!rule.target_group.empty()) {
                group_alarm_map_[rule.target_group].push_back(rule.id);
            }
        }
        
        logger_.Info("Loaded " + std::to_string(alarm_rules_.size()) + 
                    " alarm rules for tenant " + std::to_string(tenant_id));
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to load alarm rules: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Pipeline 인터페이스 - 메인 처리 함수
// =============================================================================

std::vector<AlarmEvent> AlarmManager::evaluateForMessage(const DeviceDataMessage& msg) {
    std::vector<AlarmEvent> alarm_events;
    
    if (!initialized_) {
        logger_.Error("AlarmManager not initialized");
        return alarm_events;
    }
    
    // 적용 가능한 알람 규칙 찾기
    std::vector<int> rule_ids;
    
    // 메시지에 명시된 규칙
    if (!msg.applicable_alarm_rules.empty()) {
        rule_ids = msg.applicable_alarm_rules;
    }
    
    // 포인트 기반 추가 탐색
    std::shared_lock<std::shared_mutex> index_lock(index_mutex_);
    for (const auto& point : msg.points) {
        // point_id에서 숫자 추출
        size_t pos = point.point_id.find_last_of("_");
        if (pos != std::string::npos) {
            try {
                int point_id = std::stoi(point.point_id.substr(pos + 1));
                
                auto it = point_alarm_map_.find(point_id);
                if (it != point_alarm_map_.end()) {
                    rule_ids.insert(rule_ids.end(), it->second.begin(), it->second.end());
                }
            } catch (...) {
                // ID 파싱 실패 무시
            }
        }
    }
    index_lock.unlock();
    
    // 중복 제거
    std::sort(rule_ids.begin(), rule_ids.end());
    rule_ids.erase(std::unique(rule_ids.begin(), rule_ids.end()), rule_ids.end());
    
    // 각 규칙 평가
    for (int rule_id : rule_ids) {
        std::shared_lock<std::shared_mutex> rules_lock(rules_mutex_);
        
        auto it = alarm_rules_.find(rule_id);
        if (it == alarm_rules_.end() || !it->second.is_enabled) {
            continue;
        }
        
        const auto& rule = it->second;
        rules_lock.unlock();
        
        // 해당 포인트 값 찾기
        for (const auto& point : msg.points) {
            // 포인트 타입 확인
            bool is_target = false;
            
            if (rule.target_type == "data_point" && point.point_id.find("dp_") == 0) {
                size_t pos = point.point_id.find_last_of("_");
                if (pos != std::string::npos) {
                    try {
                        int point_id = std::stoi(point.point_id.substr(pos + 1));
                        is_target = (point_id == rule.target_id);
                    } catch (...) {}
                }
            } else if (rule.target_type == "virtual_point" && point.point_id.find("vp_") == 0) {
                size_t pos = point.point_id.find_last_of("_");
                if (pos != std::string::npos) {
                    try {
                        int vp_id = std::stoi(point.point_id.substr(pos + 1));
                        is_target = (vp_id == rule.target_id);
                    } catch (...) {}
                }
            }
            
            if (!is_target) continue;
            
            // 알람 평가
            auto eval = evaluateRule(rule, point.value);
            
            // 상태 변경 확인
            if (eval.state_changed) {
                if (eval.should_trigger) {
                    // 알람 발생
                    auto occurrence_id = raiseAlarm(rule, eval);
                    
                    if (occurrence_id) {
                        // AlarmEvent 생성
                        AlarmEvent event;
                        event.occurrence_id = *occurrence_id;
                        event.rule_id = rule.id;
                        event.tenant_id = msg.tenant_id;
                        event.device_id = msg.device_id;
                        event.point_id = point.point_id;
                        event.alarm_type = rule.alarm_type;
                        event.severity = eval.severity;
                        event.state = "ACTIVE";
                        event.trigger_value = point.value;
                        event.threshold_value = 0.0;  // 임계값 설정
                        event.condition_met = eval.condition_met;
                        event.message = eval.message;
                        event.occurrence_time = std::chrono::system_clock::now();
                        event.notification_channels = rule.notification_channels.get<std::vector<std::string>>();
                        
                        alarm_events.push_back(event);
                        
                        logger_.Info("Alarm triggered: " + rule.name + " - " + eval.message);
                    }
                } else if (eval.should_clear) {
                    // 알람 해제
                    auto active_it = rule_occurrence_map_.find(rule.id);
                    if (active_it != rule_occurrence_map_.end()) {
                        clearAlarm(active_it->second, point.value);
                        
                        logger_.Info("Alarm cleared: " + rule.name);
                    }
                }
            }
        }
    }
    
    total_evaluations_ += rule_ids.size();
    
    return alarm_events;
}

// =============================================================================
// 알람 평가
// =============================================================================

AlarmEvaluation AlarmManager::evaluateRule(const AlarmRule& rule, const DataValue& value) {
    AlarmEvaluation eval;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 값을 double로 변환
        double dbl_value = 0.0;
        std::visit([&dbl_value](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<T>) {
                dbl_value = static_cast<double>(v);
            }
        }, value);
        
        // 알람 타입별 평가
        if (rule.alarm_type == "analog") {
            eval = evaluateAnalogAlarm(rule, dbl_value);
        } else if (rule.alarm_type == "digital") {
            eval = evaluateDigitalAlarm(rule, dbl_value != 0.0);
        } else if (rule.alarm_type == "script") {
            json context = {{"value", dbl_value}};
            eval = evaluateScriptAlarm(rule, context);
        }
        
        // 메시지 생성
        if (eval.should_trigger || eval.should_clear) {
            eval.message = generateMessage(rule, value, eval.condition_met);
        }
        
        // 상태 변경 확인
        eval.state_changed = (eval.should_trigger && !rule.in_alarm_state) ||
                            (eval.should_clear && rule.in_alarm_state);
        
        // 런타임 상태 업데이트
        const_cast<AlarmRule&>(rule).last_value = dbl_value;
        const_cast<AlarmRule&>(rule).last_check_time = std::chrono::system_clock::now();
        if (eval.state_changed) {
            const_cast<AlarmRule&>(rule).in_alarm_state = eval.should_trigger;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to evaluate rule " + rule.name + ": " + std::string(e.what()));
    }
    
    // 평가 시간 계산
    auto end_time = std::chrono::steady_clock::now();
    eval.evaluation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return eval;
}

// =============================================================================
// 아날로그 알람 평가
// =============================================================================

AlarmEvaluation AlarmManager::evaluateAnalogAlarm(const AlarmRule& rule, double value) {
    AlarmEvaluation eval;
    eval.severity = rule.severity;
    
    // 현재 레벨 판정
    eval.alarm_level = getAnalogLevel(rule, value);
    
    // 히스테리시스를 고려한 임계값 체크
    if (rule.high_high_limit && value >= *rule.high_high_limit - rule.deadband) {
        if (!rule.in_alarm_state || rule.last_value < *rule.high_high_limit) {
            eval.should_trigger = true;
            eval.condition_met = "High-High";
            eval.severity = "critical";
        }
    } else if (rule.high_limit && value >= *rule.high_limit - rule.deadband) {
        if (!rule.in_alarm_state || rule.last_value < *rule.high_limit) {
            eval.should_trigger = true;
            eval.condition_met = "High";
            eval.severity = "high";
        }
    } else if (rule.low_low_limit && value <= *rule.low_low_limit + rule.deadband) {
        if (!rule.in_alarm_state || rule.last_value > *rule.low_low_limit) {
            eval.should_trigger = true;
            eval.condition_met = "Low-Low";
            eval.severity = "critical";
        }
    } else if (rule.low_limit && value <= *rule.low_limit + rule.deadband) {
        if (!rule.in_alarm_state || rule.last_value > *rule.low_limit) {
            eval.should_trigger = true;
            eval.condition_met = "Low";
            eval.severity = "high";
        }
    } else {
        // 정상 범위로 복귀
        if (rule.in_alarm_state) {
            bool clear = true;
            
            // 히스테리시스 체크
            if (rule.high_high_limit && rule.last_value >= *rule.high_high_limit) {
                clear = value < *rule.high_high_limit - rule.deadband;
            } else if (rule.high_limit && rule.last_value >= *rule.high_limit) {
                clear = value < *rule.high_limit - rule.deadband;
            } else if (rule.low_low_limit && rule.last_value <= *rule.low_low_limit) {
                clear = value > *rule.low_low_limit + rule.deadband;
            } else if (rule.low_limit && rule.last_value <= *rule.low_limit) {
                clear = value > *rule.low_limit + rule.deadband;
            }
            
            if (clear && rule.auto_clear) {
                eval.should_clear = true;
                eval.condition_met = "Normal";
            }
        }
    }
    
    // 변화율 체크
    if (rule.rate_of_change > 0 && rule.last_check_time.time_since_epoch().count() > 0) {
        auto time_diff = std::chrono::system_clock::now() - rule.last_check_time;
        double seconds = std::chrono::duration<double>(time_diff).count();
        if (seconds > 0) {
            double rate = std::abs(value - rule.last_value) / seconds;
            if (rate > rule.rate_of_change) {
                eval.should_trigger = true;
                eval.condition_met = "Rate of Change";
                eval.severity = "high";
            }
        }
    }
    
    return eval;
}

// =============================================================================
// 디지털 알람 평가
// =============================================================================

AlarmEvaluation AlarmManager::evaluateDigitalAlarm(const AlarmRule& rule, bool state) {
    AlarmEvaluation eval;
    eval.severity = rule.severity;
    
    bool trigger = false;
    
    if (rule.trigger_condition == "on_true") {
        trigger = state;
        eval.condition_met = "ON";
    } else if (rule.trigger_condition == "on_false") {
        trigger = !state;
        eval.condition_met = "OFF";
    } else if (rule.trigger_condition == "on_change") {
        trigger = (state != rule.last_digital_state);
        eval.condition_met = state ? "Changed to ON" : "Changed to OFF";
    } else if (rule.trigger_condition == "on_rising") {
        trigger = (state && !rule.last_digital_state);
        eval.condition_met = "Rising Edge";
    } else if (rule.trigger_condition == "on_falling") {
        trigger = (!state && rule.last_digital_state);
        eval.condition_met = "Falling Edge";
    }
    
    if (trigger && !rule.in_alarm_state) {
        eval.should_trigger = true;
    } else if (!trigger && rule.in_alarm_state && rule.auto_clear) {
        eval.should_clear = true;
        eval.condition_met = "Condition Cleared";
    }
    
    // 상태 업데이트
    const_cast<AlarmRule&>(rule).last_digital_state = state;
    
    return eval;
}

// =============================================================================
// 메시지 생성
// =============================================================================

std::string AlarmManager::generateMessage(const AlarmRule& rule, const DataValue& value, 
                                         const std::string& condition) {
    // 커스텀 메시지 확인
    std::string custom_msg = generateCustomMessage(rule, value);
    if (!custom_msg.empty()) {
        return custom_msg;
    }
    
    // 기본 템플릿 사용
    if (!rule.message_template.empty()) {
        std::map<std::string, std::string> variables;
        
        // 값 변환
        std::string value_str;
        std::visit([&value_str](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                value_str = v;
            } else if constexpr (std::is_arithmetic_v<T>) {
                value_str = std::to_string(v);
            }
        }, value);
        
        variables["{value}"] = value_str;
        variables["{name}"] = rule.name;
        variables["{condition}"] = condition;
        variables["{severity}"] = rule.severity;
        variables["{unit}"] = "";  // 단위 정보가 있으면 추가
        
        return interpolateTemplate(rule.message_template, variables);
    }
    
    // 기본 메시지
    return rule.name + " - " + condition;
}

std::string AlarmManager::generateCustomMessage(const AlarmRule& rule, const DataValue& value) {
    if (rule.message_config.empty()) {
        return "";
    }
    
    try {
        // 디지털 알람 - 0/1 값에 따른 메시지
        if (rule.alarm_type == "digital") {
            int int_value = 0;
            std::visit([&int_value](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_arithmetic_v<T>) {
                    int_value = static_cast<int>(v);
                }
            }, value);
            
            std::string key = std::to_string(int_value);
            if (rule.message_config.contains(key)) {
                auto msg_obj = rule.message_config[key];
                if (msg_obj.is_object() && msg_obj.contains("text")) {
                    return msg_obj["text"];
                } else if (msg_obj.is_string()) {
                    return msg_obj;
                }
            }
        }
        
        // 아날로그 알람 - 레벨별 메시지
        if (rule.alarm_type == "analog") {
            double dbl_value = 0.0;
            std::visit([&dbl_value](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_arithmetic_v<T>) {
                    dbl_value = static_cast<double>(v);
                }
            }, value);
            
            std::string level = getAnalogLevel(rule, dbl_value);
            
            if (rule.message_config.contains(level)) {
                auto msg_obj = rule.message_config[level];
                if (msg_obj.is_object() && msg_obj.contains("text")) {
                    std::string msg = msg_obj["text"];
                    
                    // 변수 치환
                    size_t pos = 0;
                    while ((pos = msg.find("{value}")) != std::string::npos) {
                        msg.replace(pos, 7, std::to_string(dbl_value));
                    }
                    
                    return msg;
                } else if (msg_obj.is_string()) {
                    return msg_obj;
                }
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to generate custom message: " + std::string(e.what()));
    }
    
    return "";
}

// =============================================================================
// 알람 발생/해제
// =============================================================================

std::optional<int64_t> AlarmManager::raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval) {
    try {
        AlarmOccurrence occurrence;
        occurrence.id = next_occurrence_id_++;
        occurrence.rule_id = rule.id;
        occurrence.tenant_id = rule.tenant_id;
        occurrence.occurrence_time = std::chrono::system_clock::now();
        occurrence.trigger_condition = eval.condition_met;
        occurrence.alarm_message = eval.message;
        occurrence.severity = eval.severity;
        occurrence.state = "active";
        
        // 데이터베이스 저장
        if (saveOccurrenceToDB(occurrence)) {
            // 메모리에 저장
            std::unique_lock<std::shared_mutex> lock(active_mutex_);
            active_alarms_[occurrence.id] = occurrence;
            rule_occurrence_map_[rule.id] = occurrence.id;
            
            alarms_raised_++;
            
            // Redis 캐시 업데이트
            if (redis_client_) {
                updateRedisCache(occurrence);
            }
            
            return occurrence.id;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to raise alarm: " + std::string(e.what()));
    }
    
    return std::nullopt;
}

bool AlarmManager::clearAlarm(int64_t occurrence_id, const DataValue& clear_value) {
    try {
        std::unique_lock<std::shared_mutex> lock(active_mutex_);
        
        auto it = active_alarms_.find(occurrence_id);
        if (it == active_alarms_.end()) {
            return false;
        }
        
        auto& occurrence = it->second;
        occurrence.state = "cleared";
        occurrence.cleared_time = std::chrono::system_clock::now();
        occurrence.cleared_value = clear_value;
        
        // 데이터베이스 업데이트
        if (updateOccurrenceInDB(occurrence)) {
            // rule_occurrence_map에서 제거
            for (auto& [rule_id, occ_id] : rule_occurrence_map_) {
                if (occ_id == occurrence_id) {
                    rule_occurrence_map_.erase(rule_id);
                    break;
                }
            }
            
            // 활성 알람에서 제거
            active_alarms_.erase(it);
            alarms_cleared_++;
            
            return true;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to clear alarm: " + std::string(e.what()));
    }
    
    return false;
}

// =============================================================================
// 데이터베이스 작업
// =============================================================================

bool AlarmManager::saveOccurrenceToDB(const AlarmOccurrence& occurrence) {
    try {
        std::string query = R"(
            INSERT INTO alarm_occurrences 
            (rule_id, tenant_id, trigger_value, trigger_condition,
             alarm_message, severity, state)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )";
        
        std::string trigger_value = json(occurrence.trigger_value).dump();
        
        db_manager_->executeUpdate(query, {
            std::to_string(occurrence.rule_id),
            std::to_string(occurrence.tenant_id),
            trigger_value,
            occurrence.trigger_condition,
            occurrence.alarm_message,
            occurrence.severity,
            occurrence.state
        });
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to save occurrence to DB: " + std::string(e.what()));
        return false;
    }
}

bool AlarmManager::updateOccurrenceInDB(const AlarmOccurrence& occurrence) {
    try {
        std::string query = R"(
            UPDATE alarm_occurrences 
            SET state = ?, cleared_time = datetime('now'), cleared_value = ?
            WHERE id = ?
        )";
        
        std::string cleared_value = occurrence.cleared_value ? 
            json(*occurrence.cleared_value).dump() : "null";
        
        db_manager_->executeUpdate(query, {
            occurrence.state,
            cleared_value,
            std::to_string(occurrence.id)
        });
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to update occurrence in DB: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드
// =============================================================================

std::string AlarmManager::getAnalogLevel(const AlarmRule& rule, double value) {
    if (rule.high_high_limit && value >= *rule.high_high_limit) {
        return "high_high";
    }
    if (rule.high_limit && value >= *rule.high_limit) {
        return "high";
    }
    if (rule.low_low_limit && value <= *rule.low_low_limit) {
        return "low_low";
    }
    if (rule.low_limit && value <= *rule.low_limit) {
        return "low";
    }
    return "normal";
}

std::string AlarmManager::interpolateTemplate(const std::string& tmpl, 
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

void AlarmManager::updateRedisCache(const AlarmOccurrence& occurrence) {
    if (!redis_client_) return;
    
    try {
        std::string key = "tenant:" + std::to_string(occurrence.tenant_id) + 
                         ":alarm:active:" + std::to_string(occurrence.id);
        
        json alarm_json = {
            {"id", occurrence.id},
            {"rule_id", occurrence.rule_id},
            {"severity", occurrence.severity},
            {"message", occurrence.alarm_message},
            {"state", occurrence.state},
            {"occurrence_time", std::chrono::system_clock::to_time_t(occurrence.occurrence_time)}
        };
        
        redis_client_->set(key, alarm_json.dump());
        redis_client_->expire(key, 86400);  // 24시간 TTL
        
        // 활성 알람 SET에 추가
        std::string set_key = "tenant:" + std::to_string(occurrence.tenant_id) + ":alarms:active";
        redis_client_->sadd(set_key, std::to_string(occurrence.id));
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to update Redis cache: " + std::string(e.what()));
    }
}

// =============================================================================
// 통계
// =============================================================================

json AlarmManager::getStatistics() const {
    std::shared_lock<std::shared_mutex> rules_lock(rules_mutex_);
    std::shared_lock<std::shared_mutex> active_lock(active_mutex_);
    
    return {
        {"total_rules", alarm_rules_.size()},
        {"enabled_rules", std::count_if(alarm_rules_.begin(), alarm_rules_.end(),
            [](const auto& p) { return p.second.is_enabled; })},
        {"active_alarms", active_alarms_.size()},
        {"total_evaluations", total_evaluations_.load()},
        {"alarms_raised", alarms_raised_.load()},
        {"alarms_cleared", alarms_cleared_.load()}
    };
}

} // namespace Alarm
} // namespace PulseOne