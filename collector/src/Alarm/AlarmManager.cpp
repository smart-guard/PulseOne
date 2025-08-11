// =============================================================================
// collector/src/Alarm/AlarmManager.cpp
// PulseOne 알람 매니저 구현 - 고수준 비즈니스 로직 담당
// =============================================================================

#include "Alarm/AlarmManager.h"
#include "Alarm/AlarmEngine.h"  // 🔥 AlarmEngine 사용
#include "VirtualPoint/ScriptLibraryManager.h"
#include "Client/RedisClientImpl.h"
// #include "Client/RabbitMQClient.h"  // 🔥 주석 처리
#include <quickjs.h> 
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
// 🔥 단순화된 초기화 (AlarmEngine 패턴 따르기)
// =============================================================================

bool AlarmManager::initialize() {
    if (initialized_) {
        logger_.Warn("AlarmManager already initialized");
        return true;
    }
    
    try {
        // 🔥 DatabaseManager 싱글톤 사용
        db_manager_ = std::make_shared<Database::DatabaseManager>(
            Database::DatabaseManager::getInstance()
        );
        
        // 🔥 Redis 클라이언트 내부 생성 (ConfigManager 사용)
        auto& config = Utils::ConfigManager::getInstance();
        std::string redis_host = config.getString("redis.host", "localhost");
        int redis_port = config.getInt("redis.port", 6379);
        
        redis_client_ = std::make_shared<RedisClientImpl>();
        if (!redis_client_->connect(redis_host, redis_port)) {
            logger_.Warn("Failed to connect to Redis - alarm events will not be published");
            redis_client_.reset();
        }
        
        // 🔥 RabbitMQ는 현재 비활성화
        // mq_client_ = std::make_shared<RabbitMQClient>();
        
        // 🔥 AlarmEngine 초기화 확인
        auto& alarm_engine = AlarmEngine::getInstance();
        if (!alarm_engine.isInitialized()) {
            if (!alarm_engine.initialize()) {
                logger_.Error("Failed to initialize AlarmEngine");
                return false;
            }
        }
        
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
    
    // 🔥 스크립트 엔진 정리
    cleanupScriptEngine();
    
    // Redis 연결 종료
    if (redis_client_) {
        redis_client_->disconnect();
        redis_client_.reset();
    }
    
    alarm_rules_.clear();
    point_alarm_map_.clear();
    active_alarms_.clear();
    initialized_ = false;
}

// =============================================================================
// 🔥 고수준 Pipeline 인터페이스 - AlarmEngine 위임
// =============================================================================

std::vector<AlarmEvent> AlarmManager::evaluateForMessage(const DeviceDataMessage& msg) {
    if (!initialized_) {
        logger_.Error("AlarmManager not initialized");
        return {};
    }
    
    try {
        // 🔥 실제 알람 평가는 AlarmEngine에 위임
        auto& alarm_engine = AlarmEngine::getInstance();
        auto engine_events = alarm_engine.evaluateForMessage(msg);
        
        // 🔥 AlarmManager는 추가적인 비즈니스 로직 처리
        std::vector<AlarmEvent> enhanced_events;
        
        for (auto& event : engine_events) {
            // 비즈니스 로직 추가 (메시지 강화, 알림 설정 등)
            enhanceAlarmEvent(event, msg);
            
            // 외부 시스템 연동
            if (redis_client_) {
                publishToRedis(event);
            }
            
            // 🔥 RabbitMQ 주석 처리
            // if (mq_client_) {
            //     sendToMessageQueue(event);
            // }
            
            enhanced_events.push_back(event);
        }
        
        total_evaluations_ += msg.points.size();
        return enhanced_events;
        
    } catch (const std::exception& e) {
        logger_.Error("AlarmManager::evaluateForMessage failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 🔥 비즈니스 로직 - 알람 이벤트 강화
// =============================================================================

void AlarmManager::enhanceAlarmEvent(AlarmEvent& event, const DeviceDataMessage& msg) {
    try {
        // 추가 메타데이터
        event.device_id = msg.device_id;
        event.tenant_id = msg.tenant_id;
        
        // 커스텀 메시지 생성
        auto rule = getAlarmRule(event.rule_id);
        if (rule.has_value()) {
            // 고급 메시지 템플릿 적용
            event.message = generateAdvancedMessage(rule.value(), event);
            
            // 알림 채널 설정
            if (!rule->notification_channels.empty()) {
                event.notification_channels = rule->notification_channels.get<std::vector<std::string>>();
            }
            
            // 우선순위 및 심각도 조정
            adjustSeverityAndPriority(event, rule.value());
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to enhance alarm event: " + std::string(e.what()));
    }
}

std::string AlarmManager::generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event) {
    // 🔥 기본 메시지는 이미 AlarmEngine에서 생성됨
    std::string base_message = event.message;
    
    // 🔥 AlarmManager는 추가적인 비즈니스 로직만 적용
    
    // 1. 다국어 지원 (향후 확장)
    if (rule.message_config.contains("locale")) {
        // TODO: 다국어 메시지 처리
    }
    
    // 2. 컨텍스트 정보 추가
    if (rule.message_config.contains("include_context") && 
        rule.message_config["include_context"].get<bool>()) {
        
        base_message += " [컨텍스트: " + event.point_id + "]";
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
    
    return base_message;
}

void AlarmManager::adjustSeverityAndPriority(AlarmEvent& event, const AlarmRule& rule) {
    // 시간대별 심각도 조정
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_time = std::localtime(&time_t);
    
    // 야간 시간(22:00-06:00)에는 심각도 한 단계 상향
    if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
        if (event.severity == "medium") {
            event.severity = "high";
        } else if (event.severity == "high") {
            event.severity = "critical";
        }
    }
    
    // 연속 알람 발생 시 심각도 상향
    // TODO: 연속 발생 횟수 추적 로직 추가
}

// =============================================================================
// 🔥 외부 시스템 연동 (AlarmEngine보다 고수준)
// =============================================================================

void AlarmManager::publishToRedis(const AlarmEvent& event) {
    if (!redis_client_) return;
    
    try {
        // 🔥 더 풍부한 Redis 이벤트 발송
        json alarm_json = {
            {"type", "alarm_event_enhanced"},
            {"occurrence_id", event.occurrence_id},
            {"rule_id", event.rule_id},
            {"tenant_id", event.tenant_id},
            {"device_id", event.device_id},
            {"point_id", event.point_id},
            {"severity", event.severity},
            {"state", event.state},
            {"message", event.message},
            {"notification_channels", event.notification_channels},
            {"timestamp", std::chrono::system_clock::to_time_t(event.occurrence_time)},
            {"manager_processed", true}  // AlarmManager에서 처리됨을 표시
        };
        
        // 다중 채널 발송
        std::string global_channel = "alarms:global";
        std::string tenant_channel = "tenant:" + std::to_string(event.tenant_id) + ":alarms";
        std::string device_channel = "device:" + std::to_string(event.device_id) + ":alarms";
        
        redis_client_->publish(global_channel, alarm_json.dump());
        redis_client_->publish(tenant_channel, alarm_json.dump());
        redis_client_->publish(device_channel, alarm_json.dump());
        
        // 심각도별 채널
        std::string severity_channel = "alarms:severity:" + event.severity;
        redis_client_->publish(severity_channel, alarm_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to publish enhanced alarm to Redis: " + std::string(e.what()));
    }
}

// =============================================================================
// 알람 규칙 로드 (기존 코드 유지)
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

std::optional<AlarmRule> AlarmManager::getAlarmRule(int rule_id) const {
    std::shared_lock<std::shared_mutex> lock(rules_mutex_);
    
    auto it = alarm_rules_.find(rule_id);
    if (it != alarm_rules_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// =============================================================================
// 🔥 비즈니스 레벨 평가 메서드들 (AlarmEngine 위임)
// =============================================================================

AlarmEvaluation AlarmManager::evaluateRule(const AlarmRule& rule, const DataValue& value) {
    // 🔥 실제 평가는 AlarmEngine에 위임
    auto& alarm_engine = AlarmEngine::getInstance();
    
    // AlarmRule을 AlarmRuleEntity로 변환
    auto rule_entity = convertToEntity(rule);
    
    return alarm_engine.evaluateRule(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateAnalogAlarm(const AlarmRule& rule, double value) {
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateAnalogAlarm(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateDigitalAlarm(const AlarmRule& rule, bool state) {
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateDigitalAlarm(rule_entity, state);
}

AlarmEvaluation AlarmManager::evaluateScriptAlarm(const AlarmRule& rule, const json& context) {
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateScriptAlarm(rule_entity, context);
}

// =============================================================================
// 🔥 AlarmRule ↔ AlarmRuleEntity 변환
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
// 🔥 JavaScript 엔진 (기존 코드 유지)
// =============================================================================

bool AlarmManager::initScriptEngine() {
    if (js_context_) {
        return true;  // 이미 초기화됨
    }
    
    // JavaScript 런타임 생성
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) {
        logger_.Error("Failed to create JS runtime for AlarmManager");
        return false;
    }
    
    // 메모리 제한 (8MB)
    JS_SetMemoryLimit(js_runtime_, 8 * 1024 * 1024);
    
    // Context 생성
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        logger_.Error("Failed to create JS context for AlarmManager");
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    
    logger_.Info("Script engine initialized for AlarmManager");
    return true;
}

void AlarmManager::cleanupScriptEngine() {
    if (js_context_) {
        JS_FreeContext(js_context_);
        js_context_ = nullptr;
    }
    if (js_runtime_) {
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
    }
}

// =============================================================================
// 기타 메서드들 (기존 코드 유지하되 AlarmEngine 위임)
// =============================================================================

std::optional<int64_t> AlarmManager::raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval) {
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    
    // TODO: eval에서 DataValue 생성
    DataValue dummy_value = 0.0;  // 실제로는 eval에서 추출
    
    return alarm_engine.raiseAlarm(rule_entity, eval, dummy_value);
}

bool AlarmManager::clearAlarm(int64_t occurrence_id, const DataValue& clear_value) {
    auto& alarm_engine = AlarmEngine::getInstance();
    return alarm_engine.clearAlarm(occurrence_id, clear_value);
}

json AlarmManager::getStatistics() const {
    auto& alarm_engine = AlarmEngine::getInstance();
    auto engine_stats = alarm_engine.getStatistics();
    
    // AlarmManager 고유 통계 추가
    std::shared_lock<std::shared_mutex> rules_lock(rules_mutex_);
    std::shared_lock<std::shared_mutex> active_lock(active_mutex_);
    
    json manager_stats = {
        {"manager_rules", alarm_rules_.size()},
        {"manager_active_alarms", active_alarms_.size()},
        {"manager_evaluations", total_evaluations_.load()},
        {"redis_connected", redis_client_ != nullptr},
        {"script_engine_active", js_context_ != nullptr}
    };
    
    // 통합 통계
    json combined_stats = engine_stats;
    combined_stats["manager"] = manager_stats;
    
    return combined_stats;
}

} // namespace Alarm
} // namespace PulseOne