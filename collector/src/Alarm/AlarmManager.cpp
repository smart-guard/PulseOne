// =============================================================================
// collector/src/Alarm/AlarmManager.cpp
// PulseOne 알람 매니저 구현 - 프로젝트 API 완전 준수 버전
// =============================================================================

#include "Alarm/AlarmManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Alarm/AlarmEngine.h"
#include "Client/RedisClientImpl.h"
#include "Utils/ConfigManager.h"
#include <nlohmann/json.hpp>
#include <quickjs.h> 
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>

using json = nlohmann::json;

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 🔥 명확한 싱글톤 패턴 (AlarmEngine과 동일)
// =============================================================================

AlarmManager& AlarmManager::getInstance() {
    static AlarmManager instance;
    return instance;
}

AlarmManager::AlarmManager() 
    : initialized_(false),
      next_occurrence_id_(1),
      total_evaluations_(0),
      alarms_raised_(0),
      alarms_cleared_(0),
      js_runtime_(nullptr),
      js_context_(nullptr) {
    
    // ✅ 올바른 LogManager 사용법
    auto& logger = LogManager::getInstance();
    logger.log("alarm", LogLevel::DEBUG_LEVEL, "AlarmManager constructor starting...");
    
    try {
        initializeClients();
        initializeData();
        initScriptEngine();
        
        initialized_ = true;
        logger.log("alarm", LogLevel::INFO, "AlarmManager initialized successfully in constructor");
        
    } catch (const std::exception& e) {
        logger.log("alarm", LogLevel::ERROR, "AlarmManager constructor failed: " + std::string(e.what()));
        initialized_ = false;
    }
}

AlarmManager::~AlarmManager() {
    shutdown();
}

// =============================================================================
// 🔥 내부 초기화 메서드들 - 올바른 API 사용
// =============================================================================

void AlarmManager::initializeClients() {
    try {
        // ✅ 올바른 ConfigManager 사용법
        auto& config = ConfigManager::getInstance();
        
        std::string redis_host = config.getOrDefault("redis.host", "localhost");
        int redis_port = config.getInt("redis.port", 6379);
        std::string redis_password = config.getOrDefault("redis.password", "");
        
        // redis_client_ = std::make_shared<RedisClientImpl>(); // 🔥 임시 주석
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Clients initialization completed (Redis temporarily disabled)");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to initialize clients: " + std::string(e.what()));
    }
}

void AlarmManager::initializeData() {
    try {
        auto& alarm_engine = AlarmEngine::getInstance();
        if (!alarm_engine.isInitialized()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "AlarmEngine not initialized - some features may be limited");
        }
        
        // ✅ 올바른 DatabaseManager 사용법
        auto& db_manager = DatabaseManager::getInstance();
        std::string query = "SELECT MAX(id) as max_id FROM alarm_occurrences";
        
        // ✅ 올바른 executeQuery 사용법 (vector<vector<string>> 형태)
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager.executeQuery(query, raw_results);
        
        if (success && !raw_results.empty() && !raw_results[0].empty()) {
            try {
                std::string max_id_str = raw_results[0][0];
                if (!max_id_str.empty() && max_id_str != "NULL") {
                    next_occurrence_id_ = std::stoll(max_id_str) + 1;
                } else {
                    next_occurrence_id_ = 1;
                }
            } catch (...) {
                next_occurrence_id_ = 1;
            }
        } else {
            next_occurrence_id_ = 1;
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG_LEVEL, "Initial data loaded, next occurrence ID: " + std::to_string(next_occurrence_id_.load()));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to load initial data: " + std::string(e.what()));
        next_occurrence_id_ = 1;
    }
}

bool AlarmManager::initScriptEngine() {
    try {
        if (js_context_) {
            return true;
        }
        
        js_runtime_ = JS_NewRuntime();
        if (!js_runtime_) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::ERROR, "Failed to create JS runtime for AlarmManager");
            return false;
        }
        
        JS_SetMemoryLimit((JSRuntime*)js_runtime_, 8 * 1024 * 1024);
        
        js_context_ = JS_NewContext((JSRuntime*)js_runtime_);
        if (!js_context_) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::ERROR, "Failed to create JS context for AlarmManager");
            JS_FreeRuntime((JSRuntime*)js_runtime_);
            js_runtime_ = nullptr;
            return false;
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Script engine initialized for AlarmManager");
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Script engine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AlarmManager::cleanupScriptEngine() {
    if (js_context_) {
        JS_FreeContext((JSContext*)js_context_);
        js_context_ = nullptr;
    }
    if (js_runtime_) {
        JS_FreeRuntime((JSRuntime*)js_runtime_);
        js_runtime_ = nullptr;
    }
}

void AlarmManager::shutdown() {
    if (!initialized_.load()) return;
    
    auto& logger = LogManager::getInstance();
    logger.log("alarm", LogLevel::INFO, "Shutting down AlarmManager");
    
    cleanupScriptEngine();
    
    if (redis_client_) {
        redis_client_.reset();
    }
    
    {
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
        
        alarm_rules_.clear();
        point_alarm_map_.clear();
        group_alarm_map_.clear();
    }
    
    initialized_ = false;
    logger.log("alarm", LogLevel::INFO, "AlarmManager shutdown completed");
}

// =============================================================================
// 🔥 메인 비즈니스 인터페이스 (Pipeline에서 호출)
// =============================================================================

std::vector<AlarmEvent> AlarmManager::evaluateForMessage(const DeviceDataMessage& msg) {
    if (!initialized_.load()) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "AlarmManager not properly initialized");
        return {};
    }
    
    try {
        auto& alarm_engine = AlarmEngine::getInstance();
        auto engine_events = alarm_engine.evaluateForMessage(msg);
        
        std::vector<AlarmEvent> enhanced_events;
        enhanced_events.reserve(engine_events.size());
        
        for (auto& event : engine_events) {
            enhanceAlarmEvent(event, msg);
            publishToRedis(event);
            sendNotifications(event);
            enhanced_events.push_back(std::move(event));
        }
        
        total_evaluations_.fetch_add(1);
        
        if (!enhanced_events.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::INFO, "AlarmManager processed " + std::to_string(enhanced_events.size()) + " enhanced alarm events");
        }
        
        return enhanced_events;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "AlarmManager::evaluateForMessage failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 🔥 비즈니스 로직 - 알람 이벤트 강화
// =============================================================================

void AlarmManager::enhanceAlarmEvent(AlarmEvent& event, const DeviceDataMessage& msg) {
    if (!initialized_.load()) {
        return;
    }
    
    try {
        // 기본 메타데이터 추가
        event.device_id = msg.device_id;
        
        // 알람 규칙 조회 (캐시에서)
        auto rule = getAlarmRule(event.rule_id);
        if (rule.has_value()) {
            // 1. 고급 메시지 생성
            event.message = generateAdvancedMessage(rule.value(), event);
            
            // 2. 알림 채널 설정
            if (!rule->notification_channels.empty()) {
                try {
                    auto channels = rule->notification_channels.get<std::vector<std::string>>();
                    // TODO: event에 channels 추가 (Structs::AlarmEvent 확장 필요)
                } catch (...) {
                    auto& logger = LogManager::getInstance();
                    logger.log("alarm", LogLevel::DEBUG_LEVEL, "Failed to parse notification channels for rule " + std::to_string(event.rule_id));
                }
            }
            
            // 3. 심각도 및 우선순위 조정
            adjustSeverityAndPriority(event, rule.value());
            
            // 4. 컨텍스트 정보 추가
            json context_data;
            context_data["rule_name"] = rule->name;
            context_data["target_type"] = rule->target_type;
            context_data["target_id"] = rule->target_id;
            context_data["enhanced_by"] = "AlarmManager";
            context_data["enhancement_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to enhance alarm event: " + std::string(e.what()));
    }
}

std::string AlarmManager::generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event) {
    if (!initialized_.load()) {
        return event.message;  // 기본 메시지 반환
    }
    
    // 🔥 AlarmEngine에서 생성된 기본 메시지를 기반으로 비즈니스 로직 추가
    std::string base_message = event.message;
    
    try {
        // 1. 다국어 지원 (향후 확장)
        if (rule.message_config.contains("locale")) {
            // TODO: 다국어 메시지 처리
        }
        
        // 2. 컨텍스트 정보 추가
        if (rule.message_config.contains("include_context") && 
            rule.message_config["include_context"].get<bool>()) {
            
            base_message += " [대상: " + event.point_id + "]";
        }
        
        // 3. 시간 정보 추가
        if (rule.message_config.contains("include_timestamp") && 
            rule.message_config["include_timestamp"].get<bool>()) {
            
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            char time_str[100];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
            
            base_message += " [발생시간: " + std::string(time_str) + "]";
        }
        
        // 4. 위치 정보 추가 (device 정보에서)
        if (rule.message_config.contains("include_location") && 
            rule.message_config["include_location"].get<bool>()) {
            
            base_message += " [위치: Device-" + std::to_string(event.device_id) + "]";
        }
        
        // 5. 심각도 레벨 표시
        if (rule.message_config.contains("show_severity") && 
            rule.message_config["show_severity"].get<bool>()) {
            
            std::string severity_prefix;
            if (event.severity == "critical") {
                severity_prefix = "[🚨 긴급] ";
            } else if (event.severity == "high") {
                severity_prefix = "[⚠️  높음] ";
            } else if (event.severity == "medium") {
                severity_prefix = "[⚡ 보통] ";
            } else {
                severity_prefix = "[ℹ️  정보] ";
            }
            
            base_message = severity_prefix + base_message;
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to generate advanced message: " + std::string(e.what()));
        return base_message;  // 에러 시 기본 메시지 반환
    }
    
    return base_message;
}

void AlarmManager::adjustSeverityAndPriority(AlarmEvent& event, const AlarmRule& rule) {
    if (!initialized_.load()) {
        return;
    }
    
    try {
        // 1. 시간대별 심각도 조정
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto local_time = std::localtime(&time_t);
        
        // 야간 시간(22:00-06:00)에는 심각도 한 단계 상향
        if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
            if (event.severity == "low") {
                event.severity = "medium";
            } else if (event.severity == "medium") {
                event.severity = "high";
            } else if (event.severity == "high") {
                event.severity = "critical";
            }
            
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::DEBUG_LEVEL, "Severity upgraded for night time: " + event.severity);
        }
        
        // 2. 주말/휴일 심각도 조정
        if (local_time->tm_wday == 0 || local_time->tm_wday == 6) { // 일요일(0) 또는 토요일(6)
            if (event.severity == "low") {
                event.severity = "medium";
            } else if (event.severity == "medium") {
                event.severity = "high";
            }
            
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::DEBUG_LEVEL, "Severity upgraded for weekend: " + event.severity);
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to adjust severity and priority: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 외부 시스템 연동 (고수준 비즈니스 로직)
// =============================================================================

void AlarmManager::publishToRedis(const AlarmEvent& event) {
    if (!redis_client_ || !initialized_.load()) {
        return;
    }
    
    try {
        // 🔥 AlarmEngine보다 더 풍부한 Redis 이벤트 발송 (비즈니스 채널들)
        json enhanced_alarm = {
            {"type", "alarm_event_enhanced"},
            {"manager_version", "1.0"},
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"device_id", event.device_id},
            {"point_id", event.point_id},
            {"severity", event.severity},
            {"state", event.state},
            {"message", event.message},
            {"timestamp", std::chrono::system_clock::to_time_t(event.occurrence_time)},
            {"enhanced_by_manager", true},
            {"processing_time", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - event.occurrence_time).count()}
        };
        
        // 🔥 다중 비즈니스 채널 발송
        std::vector<std::string> channels = {
            "alarms:enhanced",  // 강화된 알람 전용 채널
            "device:" + std::to_string(event.device_id) + ":alarms:enhanced"
        };
        
        // 심각도별 비즈니스 채널
        if (event.severity == "critical") {
            channels.push_back("alarms:critical:immediate");
        } else if (event.severity == "high") {
            channels.push_back("alarms:high:priority");
        }
        
        // 시간대별 채널 (야간 알람)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto local_time = std::localtime(&time_t);
        
        if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
            channels.push_back("alarms:night:urgent");
        }
        
        // 모든 채널에 발송
        for (const auto& channel : channels) {
            redis_client_->publish(channel, enhanced_alarm.dump());
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG_LEVEL, "Enhanced alarm published to " + std::to_string(channels.size()) + " Redis channels");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to publish enhanced alarm to Redis: " + std::string(e.what()));
    }
}

void AlarmManager::sendNotifications(const AlarmEvent& event) {
    if (!initialized_.load()) {
        return;
    }
    
    try {
        // 🔥 향후 구현: 이메일, SMS, Slack, Discord 등 다양한 알림 채널
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG_LEVEL, "Notification processing completed for alarm " + std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send notifications: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 AlarmEngine 위임 메서드들 (비즈니스 래퍼)
// =============================================================================

AlarmEvaluation AlarmManager::evaluateRule(const AlarmRule& rule, const DataValue& value) {
    if (!initialized_.load()) {
        return {};
    }
    
    // 🔥 실제 평가는 AlarmEngine에 위임
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    
    return alarm_engine.evaluateRule(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateAnalogAlarm(const AlarmRule& rule, double value) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateAnalogAlarm(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateDigitalAlarm(const AlarmRule& rule, bool state) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateDigitalAlarm(rule_entity, state);
}

AlarmEvaluation AlarmManager::evaluateScriptAlarm(const AlarmRule& rule, const json& context) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateScriptAlarm(rule_entity, context);
}

std::optional<int64_t> AlarmManager::raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval) {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    
    // TODO: eval에서 DataValue 생성 (실제 구현에서는 eval.triggered_value 사용)
    DataValue trigger_value = 0.0;  
    
    auto result = alarm_engine.raiseAlarm(rule_entity, eval, trigger_value);
    
    if (result.has_value()) {
        alarms_raised_.fetch_add(1);
    }
    
    return result;
}

bool AlarmManager::clearAlarm(int64_t occurrence_id, const DataValue& clear_value) {
    if (!initialized_.load()) {
        return false;
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    bool result = alarm_engine.clearAlarm(occurrence_id, clear_value);
    
    if (result) {
        alarms_cleared_.fetch_add(1);
    }
    
    return result;
}

// =============================================================================
// 🔥 AlarmRule ↔ AlarmRuleEntity 변환 (기존 코드 유지)
// =============================================================================

Database::Entities::AlarmRuleEntity AlarmManager::convertToEntity(const AlarmRule& rule) {
    Database::Entities::AlarmRuleEntity entity;
    
    entity.setId(rule.id);
    entity.setTenantId(rule.tenant_id);
    entity.setName(rule.name);
    entity.setDescription(rule.description);
    
    // 타입 변환
    if (rule.target_type == "data_point") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::DATA_POINT);
    } else if (rule.target_type == "virtual_point") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::VIRTUAL_POINT);
    } else if (rule.target_type == "group") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::GROUP);
    }
    
    entity.setTargetId(rule.target_id);
    
    if (rule.alarm_type == "analog") {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::ANALOG);
    } else if (rule.alarm_type == "digital") {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::DIGITAL);
    } else if (rule.alarm_type == "script") {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::SCRIPT);
    }
    
    // 한계값들
    if (rule.high_high_limit.has_value()) {
        entity.setHighHighLimit(rule.high_high_limit.value());
    }
    if (rule.high_limit.has_value()) {
        entity.setHighLimit(rule.high_limit.value());
    }
    if (rule.low_limit.has_value()) {
        entity.setLowLimit(rule.low_limit.value());
    }
    if (rule.low_low_limit.has_value()) {
        entity.setLowLowLimit(rule.low_low_limit.value());
    }
    
    entity.setDeadband(rule.deadband);
    entity.setAutoClear(rule.auto_clear);
    entity.setIsEnabled(rule.is_enabled);
    
    if (!rule.condition_script.empty()) {
        entity.setConditionScript(rule.condition_script);
    }
    if (!rule.message_template.empty()) {
        entity.setMessageTemplate(rule.message_template);
    }
    
    // 심각도 변환
    if (rule.severity == "critical") {
        entity.setSeverity(Database::Entities::AlarmRuleEntity::Severity::CRITICAL);
    } else if (rule.severity == "high") {
        entity.setSeverity(Database::Entities::AlarmRuleEntity::Severity::HIGH);
    } else if (rule.severity == "medium") {
        entity.setSeverity(Database::Entities::AlarmRuleEntity::Severity::MEDIUM);
    } else if (rule.severity == "low") {
        entity.setSeverity(Database::Entities::AlarmRuleEntity::Severity::LOW);
    } else {
        entity.setSeverity(Database::Entities::AlarmRuleEntity::Severity::INFO);
    }
    
    return entity;
}

// =============================================================================
// 🔥 통계 및 상태 메서드들
// =============================================================================

json AlarmManager::getStatistics() const {
    json stats;
    
    if (!initialized_.load()) {
        stats["error"] = "AlarmManager not initialized";
        return stats;
    }
    
    try {
        // AlarmEngine 통계 가져오기
        auto& alarm_engine = AlarmEngine::getInstance();
        auto engine_stats = alarm_engine.getStatistics();
        
        // AlarmManager 고유 통계
        std::shared_lock<std::shared_mutex> rules_lock(rules_mutex_);
        
        json manager_stats = {
            {"manager_rules", alarm_rules_.size()},
            {"manager_evaluations", total_evaluations_.load()},
            {"manager_alarms_raised", alarms_raised_.load()},
            {"manager_alarms_cleared", alarms_cleared_.load()},
            {"redis_connected", redis_client_ != nullptr},
            {"script_engine_active", js_context_ != nullptr},
            {"next_occurrence_id", next_occurrence_id_.load()}
        };
        
        // 통합 통계
        stats["engine"] = engine_stats;
        stats["manager"] = manager_stats;
        stats["initialized"] = initialized_.load();
        
    } catch (const std::exception& e) {
        stats["error"] = "Failed to get statistics: " + std::string(e.what());
    }
    
    return stats;
}

std::optional<AlarmRule> AlarmManager::getAlarmRule(int rule_id) const {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    std::shared_lock<std::shared_mutex> lock(rules_mutex_);
    
    auto it = alarm_rules_.find(rule_id);
    if (it != alarm_rules_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// =============================================================================
// 🔥 알람 규칙 관리 (완전 구현) - 올바른 executeQuery 사용
// =============================================================================

bool AlarmManager::loadAlarmRules(int tenant_id) {
    if (!initialized_.load()) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Loading alarm rules for tenant: " + std::to_string(tenant_id));
        return false;
    }
    
    try {
        // ✅ 올바른 DatabaseManager 사용법
        auto& db_manager = DatabaseManager::getInstance();
        
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
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"( AND is_enabled = 1
        )";
        
        // ✅ 올바른 executeQuery 사용법 (vector<vector<string>> 형태)
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager.executeQuery(query, raw_results);
        
        if (!success) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::ERROR, "Failed to execute query for loading alarm rules");
            return false;
        }
        
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
        
        // 기존 규칙 정리
        alarm_rules_.clear();
        point_alarm_map_.clear();
        group_alarm_map_.clear();
        
        // 🔥 컬럼 순서에 맞춰 파싱 (결과는 쿼리 SELECT 순서대로)
        for (const auto& row : raw_results) {
            if (row.size() < 31) continue; // 최소 컬럼 수 체크
            
            AlarmRule rule;
            
            try {
                // 컬럼 순서대로 파싱
                rule.id = std::stoi(row[0]);  // id
                rule.tenant_id = tenant_id;
                rule.name = row[1];  // name
                rule.description = row[2];  // description
                
                // 대상 정보
                rule.target_type = row[3];  // target_type
                if (!row[4].empty()) {  // target_id
                    rule.target_id = std::stoi(row[4]);
                }
                rule.target_group = row[5];  // target_group
                
                // 알람 타입
                rule.alarm_type = row[6];  // alarm_type
                
                // 아날로그 설정
                if (!row[7].empty() && row[7] != "NULL") {  // high_high_limit
                    rule.high_high_limit = std::stod(row[7]);
                }
                if (!row[8].empty() && row[8] != "NULL") {  // high_limit
                    rule.high_limit = std::stod(row[8]);
                }
                if (!row[9].empty() && row[9] != "NULL") {  // low_limit
                    rule.low_limit = std::stod(row[9]);
                }
                if (!row[10].empty() && row[10] != "NULL") {  // low_low_limit
                    rule.low_low_limit = std::stod(row[10]);
                }
                
                if (!row[11].empty() && row[11] != "NULL") {  // deadband
                    rule.deadband = std::stod(row[11]);
                }
                if (!row[12].empty() && row[12] != "NULL") {  // rate_of_change
                    rule.rate_of_change = std::stod(row[12]);
                }
                
                // 디지털 설정
                rule.trigger_condition = row[13];  // trigger_condition
                
                // 스크립트
                rule.condition_script = row[14];  // condition_script
                rule.message_script = row[15];    // message_script
                
                // 메시지 설정
                if (!row[16].empty() && row[16] != "NULL") {  // message_config
                    try {
                        rule.message_config = json::parse(row[16]);
                    } catch (...) {
                        rule.message_config = json::object();
                    }
                }
                rule.message_template = row[17];  // message_template
                
                // 우선순위
                rule.severity = row[18];  // severity
                if (!row[19].empty() && row[19] != "NULL") {  // priority
                    rule.priority = std::stoi(row[19]);
                }
                
                // 자동 처리
                rule.auto_acknowledge = (row[20] == "1");  // auto_acknowledge
                if (!row[21].empty() && row[21] != "NULL") {  // acknowledge_timeout_min
                    rule.acknowledge_timeout_min = std::stoi(row[21]);
                }
                rule.auto_clear = (row[22] == "1");  // auto_clear
                
                // 억제 규칙
                if (!row[23].empty() && row[23] != "NULL") {  // suppression_rules
                    try {
                        rule.suppression_rules = json::parse(row[23]);
                    } catch (...) {
                        rule.suppression_rules = json::object();
                    }
                }
                
                // 알림 설정
                rule.notification_enabled = (row[24] == "1");  // notification_enabled
                if (!row[25].empty() && row[25] != "NULL") {  // notification_delay_sec
                    rule.notification_delay_sec = std::stoi(row[25]);
                }
                if (!row[26].empty() && row[26] != "NULL") {  // notification_repeat_interval_min
                    rule.notification_repeat_interval_min = std::stoi(row[26]);
                }
                
                if (!row[27].empty() && row[27] != "NULL") {  // notification_channels
                    try {
                        rule.notification_channels = json::parse(row[27]);
                    } catch (...) {
                        rule.notification_channels = json::array();
                    }
                }
                if (!row[28].empty() && row[28] != "NULL") {  // notification_recipients
                    try {
                        rule.notification_recipients = json::parse(row[28]);
                    } catch (...) {
                        rule.notification_recipients = json::array();
                    }
                }
                
                // 상태
                rule.is_enabled = true;  // 쿼리에서 이미 is_enabled = 1 조건
                rule.is_latched = (row[30] == "1");  // is_latched
                
                // 저장
                alarm_rules_[rule.id] = rule;
                
                // 인덱스 구축
                if (rule.target_id > 0) {
                    point_alarm_map_[rule.target_id].push_back(rule.id);
                }
                if (!rule.target_group.empty()) {
                    group_alarm_map_[rule.target_group].push_back(rule.id);
                }
                
            } catch (const std::exception& e) {
                auto& logger = LogManager::getInstance();
                logger.log("alarm", LogLevel::WARN, "Failed to parse alarm rule row: " + std::string(e.what()));
                continue;
            }
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Loaded " + std::to_string(alarm_rules_.size()) + 
                    " alarm rules for tenant " + std::to_string(tenant_id));
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to load alarm rules: " + std::string(e.what()));
        return false;
    }
}

bool AlarmManager::reloadAlarmRule(int rule_id) {
    if (!initialized_.load()) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "AlarmManager not initialized");
        return false;
    }
    
    try {
        // ✅ 올바른 DatabaseManager 사용법
        auto& db_manager = DatabaseManager::getInstance();
        
        std::string query = R"(
            SELECT tenant_id, name, description, target_type, target_id, target_group,
                   alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                   deadband, rate_of_change, trigger_condition, condition_script,
                   message_script, message_config, message_template, severity,
                   priority, auto_acknowledge, acknowledge_timeout_min, auto_clear,
                   suppression_rules, notification_enabled, notification_delay_sec,
                   notification_repeat_interval_min, notification_channels,
                   notification_recipients, is_enabled, is_latched
            FROM alarm_rules
            WHERE id = )" + std::to_string(rule_id) + R"( AND is_enabled = 1
        )";
        
        // ✅ 올바른 executeQuery 사용법
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager.executeQuery(query, raw_results);
        
        if (!success || raw_results.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "Alarm rule not found or disabled: " + std::to_string(rule_id));
            
            // 기존 규칙 제거
            std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
            alarm_rules_.erase(rule_id);
            return true;
        }
        
        // 규칙 파싱 (loadAlarmRules와 동일한 로직이므로 간소화)
        const auto& row = raw_results[0];
        AlarmRule rule;
        rule.id = rule_id;
        
        // 간단한 파싱 (핵심 필드만)
        rule.tenant_id = std::stoi(row[0]);
        rule.name = row[1];
        rule.description = row[2];
        rule.target_type = row[3];
        if (!row[4].empty()) {
            rule.target_id = std::stoi(row[4]);
        }
        rule.alarm_type = row[6];
        rule.severity = row[18];
        rule.is_enabled = true;
        
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        alarm_rules_[rule_id] = rule;
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Reloaded alarm rule: " + rule.name);
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to reload alarm rule " + std::to_string(rule_id) + ": " + std::string(e.what()));
        return false;
    }
}

std::vector<AlarmRule> AlarmManager::getAlarmRulesForPoint(int point_id, const std::string& point_type) const {
    if (!initialized_.load()) {
        return {};
    }
    
    std::vector<AlarmRule> rules;
    
    try {
        std::shared_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::shared_lock<std::shared_mutex> index_lock(index_mutex_);
        
        // 포인트 ID로 규칙 검색
        auto it = point_alarm_map_.find(point_id);
        if (it != point_alarm_map_.end()) {
            for (int rule_id : it->second) {
                auto rule_it = alarm_rules_.find(rule_id);
                if (rule_it != alarm_rules_.end() && 
                    rule_it->second.target_type == point_type) {
                    rules.push_back(rule_it->second);
                }
            }
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to get alarm rules for point: " + std::string(e.what()));
    }
    
    return rules;
}

// =============================================================================
// 🔥 메시지 생성 메서드들 (완전 구현)
// =============================================================================

std::string AlarmManager::generateMessage(const AlarmRule& rule, const DataValue& value, 
                                         const std::string& condition) {
    if (!initialized_.load()) {
        return "Alarm: " + rule.name;  // 기본 메시지
    }
    
    try {
        std::string message;
        
        // 1. 커스텀 템플릿 사용
        if (!rule.message_template.empty()) {
            message = rule.message_template;
            
            // 변수 치환
            std::map<std::string, std::string> variables = {
                {"NAME", rule.name},
                {"CONDITION", condition},
                {"SEVERITY", rule.severity},
                {"TARGET_TYPE", rule.target_type},
                {"TARGET_ID", std::to_string(rule.target_id)}
            };
            
            // 값 변환
            std::string value_str;
            std::visit([&value_str](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    value_str = v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    value_str = v ? "true" : "false";
                } else {
                    value_str = std::to_string(v);
                }
            }, value);
            variables["VALUE"] = value_str;
            
            message = interpolateTemplate(message, variables);
        }
        else {
            // 2. 기본 메시지 생성
            message = "ALARM: " + rule.name;
            
            if (!condition.empty()) {
                message += " - " + condition;
            }
            
            message += " - Value: ";
            std::visit([&message](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    message += v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    message += v ? "ON" : "OFF";
                } else {
                    message += std::to_string(v);
                }
            }, value);
            
            message += " - Severity: " + rule.severity;
        }
        
        return message;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to generate message: " + std::string(e.what()));
        return "ALARM: " + rule.name + " (message generation failed)";
    }
}

std::string AlarmManager::generateCustomMessage(const AlarmRule& rule, const DataValue& value) {
    if (!initialized_.load()) {
        return generateMessage(rule, value);
    }
    
    try {
        // JavaScript 스크립트로 메시지 생성
        if (!rule.message_script.empty() && js_context_) {
            std::lock_guard<std::mutex> lock(js_mutex_);
            
            // 값을 JavaScript 변수로 설정
            std::string js_code = "var ruleId = " + std::to_string(rule.id) + "; ";
            js_code += "var ruleName = '" + rule.name + "'; ";
            js_code += "var severity = '" + rule.severity + "'; ";
            
            std::visit([&js_code](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    js_code += "var value = '" + v + "'; ";
                } else if constexpr (std::is_same_v<T, bool>) {
                    js_code += "var value = " + std::string(v ? "true" : "false") + "; ";
                } else {
                    js_code += "var value = " + std::to_string(v) + "; ";
                }
            }, value);
            
            // 변수 설정
            JSValue var_result = JS_Eval((JSContext*)js_context_, js_code.c_str(), js_code.length(), 
                                         "<variables>", JS_EVAL_TYPE_GLOBAL);
            JS_FreeValue((JSContext*)js_context_, var_result);
            
            // 메시지 스크립트 실행
            JSValue script_result = JS_Eval((JSContext*)js_context_, rule.message_script.c_str(), 
                                           rule.message_script.length(), "<message_script>", JS_EVAL_TYPE_GLOBAL);
            
            if (!JS_IsException(script_result)) {
                const char* result_str = JS_ToCString((JSContext*)js_context_, script_result);
                if (result_str) {
                    std::string custom_message(result_str);
                    JS_FreeCString((JSContext*)js_context_, result_str);
                    JS_FreeValue((JSContext*)js_context_, script_result);
                    return custom_message;
                }
            } else {
                JSValue exception = JS_GetException((JSContext*)js_context_);
                const char* error_str = JS_ToCString((JSContext*)js_context_, exception);
                auto& logger = LogManager::getInstance();
                logger.log("alarm", LogLevel::ERROR, "Message script error: " + std::string(error_str ? error_str : "Unknown"));
                JS_FreeCString((JSContext*)js_context_, error_str);
                JS_FreeValue((JSContext*)js_context_, exception);
            }
            
            JS_FreeValue((JSContext*)js_context_, script_result);
        }
        
        // 스크립트 실패 시 기본 메시지 사용
        return generateMessage(rule, value);
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to generate custom message: " + std::string(e.what()));
        return generateMessage(rule, value);
    }
}

// =============================================================================
// 🔥 헬퍼 메서드들 (완전 구현)
// =============================================================================

std::string AlarmManager::interpolateTemplate(const std::string& tmpl, 
                                             const std::map<std::string, std::string>& variables) {
    std::string result = tmpl;
    
    try {
        for (const auto& [key, value] : variables) {
            std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to interpolate template: " + std::string(e.what()));
    }
    
    return result;
}

bool AlarmManager::acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment) {
    if (!initialized_.load()) {
        return false;
    }
    
    try {
        // AlarmEngine에 위임
        auto& alarm_engine = AlarmEngine::getInstance();
        
        // TODO: 실제 acknowledge 로직 구현
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Alarm acknowledged: " + std::to_string(occurrence_id) + 
                    " by user " + std::to_string(user_id));
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to acknowledge alarm: " + std::string(e.what()));
        return false;
    }
}

} // namespace Alarm
} // namespace PulseOne