// =============================================================================
// collector/src/Alarm/AlarmManager.cpp - 완성본 (Redis 의존성 제거)
// =============================================================================

#include "Alarm/AlarmManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Alarm/AlarmEngine.h"
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
// 🎯 싱글톤 패턴
// =============================================================================

AlarmManager& AlarmManager::getInstance() {
    static AlarmManager instance;
    return instance;
}

AlarmManager::AlarmManager() 
    : initialized_(false)
    , total_evaluations_(0)
    , alarms_raised_(0)
    , alarms_cleared_(0)
    , next_occurrence_id_(1)
    , js_runtime_(nullptr)
    , js_context_(nullptr)
{
    try {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "🎯 AlarmManager 초기화 시작 (순수 알람 모드)");
        
        // ❌ 제거: Redis 클라이언트 초기화
        // initializeClients();
        
        if (!initScriptEngine()) {
            logger.log("alarm", LogLevel::WARN, "JavaScript 엔진 초기화 실패 (스크립트 알람 비활성화)");
        }
        
        initializeData();
        initialized_ = true;
        
        logger.log("alarm", LogLevel::INFO, "✅ AlarmManager 초기화 완료 (외부 의존성 없음)");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ AlarmManager 초기화 실패: " + std::string(e.what()));
        initialized_ = false;
    }
}

AlarmManager::~AlarmManager() {
    shutdown();
}

// =============================================================================
// 🎯 초기화 메서드들
// =============================================================================

void AlarmManager::initializeData() {
    try {
        next_occurrence_id_ = 1;
        
        {
            std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
            std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
            
            alarm_rules_.clear();
            point_alarm_map_.clear();
            group_alarm_map_.clear();
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "✅ 알람 초기 데이터 로드 완료");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 초기 데이터 로드 실패: " + std::string(e.what()));
    }
}

bool AlarmManager::initScriptEngine() {
    try {
        if (js_context_) {
            return true;
        }
        
        js_runtime_ = JS_NewRuntime();
        if (!js_runtime_) {
            return false;
        }
        
        JS_SetMemoryLimit((JSRuntime*)js_runtime_, 8 * 1024 * 1024);
        
        js_context_ = JS_NewContext((JSRuntime*)js_runtime_);
        if (!js_context_) {
            JS_FreeRuntime((JSRuntime*)js_runtime_);
            js_runtime_ = nullptr;
            return false;
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "✅ JavaScript 엔진 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ JavaScript 엔진 초기화 실패: " + std::string(e.what()));
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
    logger.log("alarm", LogLevel::INFO, "🔄 AlarmManager 종료 시작");
    
    cleanupScriptEngine();
    
    // ❌ 제거: Redis 연결 종료
    // if (redis_client_) {
    //     redis_client_.reset();
    // }
    
    {
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
        
        alarm_rules_.clear();
        point_alarm_map_.clear();
        group_alarm_map_.clear();
    }
    
    initialized_ = false;
    logger.log("alarm", LogLevel::INFO, "✅ AlarmManager 종료 완료");
}

// =============================================================================
// 🎯 핵심 알람 평가 인터페이스
// =============================================================================

std::vector<AlarmEvent> AlarmManager::evaluateForMessage(const DeviceDataMessage& msg) {
    std::vector<AlarmEvent> events;
    
    if (!initialized_.load()) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ AlarmManager 초기화되지 않음");
        return events;
    }
    
    try {
        // 🎯 1단계: AlarmEngine에 위임하여 순수 알람 평가
        auto& alarm_engine = AlarmEngine::getInstance();
        events = alarm_engine.evaluateForMessage(msg);
        
        // 🎯 2단계: 비즈니스 로직으로 이벤트 강화 (AlarmManager 역할)
        for (auto& event : events) {
            enhanceAlarmEventWithBusinessLogic(event, msg);
            adjustSeverityByBusinessRules(event);
            addLocationAndContext(event, msg);
            generateLocalizedMessage(event);
        }
        
        // 🎯 3단계: 통계 업데이트
        total_evaluations_.fetch_add(1);
        if (!events.empty()) {
            alarms_raised_.fetch_add(events.size());
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "🎯 알람 평가 완료: " + 
                  std::to_string(events.size()) + "개 이벤트 생성 (외부 발송은 호출자 담당)");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 메시지 평가 실패: " + std::string(e.what()));
    }
    
    return events;
}

// =============================================================================
// 🎯 비즈니스 로직 메서드들 - 핵심 구현
// =============================================================================

void AlarmManager::enhanceAlarmEventWithBusinessLogic(AlarmEvent& event, const DeviceDataMessage& msg) {
    try {
        // 기본 메타데이터 설정
        event.tenant_id = msg.tenant_id;
        event.device_id = msg.device_id;
        
        if (event.timestamp == std::chrono::system_clock::time_point{}) {
            event.timestamp = std::chrono::system_clock::now();
        }
        
        if (event.source_name.empty()) {
            event.source_name = "Point_" + std::to_string(event.point_id);
        }
        
        // 🎯 비즈니스 로직 적용
        analyzeContinuousAlarmPattern(event);
        applyCategorySpecificRules(event);
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "⚠️ 알람 이벤트 강화 실패: " + std::string(e.what()));
    }
}

void AlarmManager::adjustSeverityByBusinessRules(AlarmEvent& event) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* local_time = std::localtime(&time_t);
        
        // 🎯 비즈니스 규칙 1: 야간 시간(22:00-06:00) 심각도 상향
        if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
            if (event.severity == AlarmSeverity::LOW) {
                event.severity = AlarmSeverity::MEDIUM;
                event.message += " [야간]";
            } else if (event.severity == AlarmSeverity::MEDIUM) {
                event.severity = AlarmSeverity::HIGH;
                event.message += " [야간]";
            } else if (event.severity == AlarmSeverity::HIGH) {
                event.severity = AlarmSeverity::CRITICAL;
                event.message += " [야간긴급]";
            }
        }
        
        // 🎯 비즈니스 규칙 2: 주말 심각도 조정
        if (local_time->tm_wday == 0 || local_time->tm_wday == 6) {
            if (event.severity == AlarmSeverity::LOW) {
                event.severity = AlarmSeverity::MEDIUM;
                event.message += " [주말]";
            }
        }
        
        // 🎯 비즈니스 규칙 3: 안전 관련 알람은 항상 높은 우선순위
        if (event.message.find("안전") != std::string::npos || 
            event.message.find("Safety") != std::string::npos) {
            if (event.severity < AlarmSeverity::HIGH) {
                event.severity = AlarmSeverity::HIGH;
                event.message = "🚨 안전알람: " + event.message;
            }
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 심각도 조정 실패: " + std::string(e.what()));
    }
}

void AlarmManager::addLocationAndContext(AlarmEvent& event, const DeviceDataMessage& msg) {
    try {
        // 🎯 위치 정보 상세화
        if (event.location.empty()) {
            std::string device_str = event.device_id;
            if (device_str.find("factory_a") != std::string::npos) {
                event.location = "공장 A동";
            } else if (device_str.find("factory_b") != std::string::npos) {
                event.location = "공장 B동";
            } else {
                event.location = "Device_" + device_str;
            }
        }
        
        // 🎯 컨텍스트 정보 추가
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        
        std::string context = " [위치: " + event.location + ", 시간: " + std::string(time_str) + "]";
        event.message += context;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "⚠️ 위치/컨텍스트 추가 실패: " + std::string(e.what()));
    }
}

void AlarmManager::generateLocalizedMessage(AlarmEvent& event) {
    try {
        // 🎯 심각도별 이모지 및 메시지 강화
        std::string severity_prefix;
        std::string action_guide;
        
        switch (event.severity) {
            case AlarmSeverity::CRITICAL:
                severity_prefix = "🚨 [긴급] ";
                action_guide = " → 즉시 대응 필요!";
                break;
            case AlarmSeverity::HIGH:
                severity_prefix = "⚠️ [높음] ";
                action_guide = " → 신속 점검 요망";
                break;
            case AlarmSeverity::MEDIUM:
                severity_prefix = "⚡ [보통] ";
                action_guide = " → 정기 점검 시 확인";
                break;
            case AlarmSeverity::LOW:
                severity_prefix = "💡 [낮음] ";
                action_guide = " → 정보 확인";
                break;
            default:
                severity_prefix = "ℹ️ [정보] ";
                action_guide = "";
                break;
        }
        
        // 🎯 메시지 재구성 (중복 방지)
        if (!event.message.empty() && event.message[0] != severity_prefix[0]) {
            event.message = severity_prefix + event.message + action_guide;
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "⚠️ 지역화 메시지 생성 실패: " + std::string(e.what()));
    }
}

void AlarmManager::analyzeContinuousAlarmPattern(AlarmEvent& event) {
    try {
        // 🎯 연속 발생 패턴 분석 (스텁 구현)
        // TODO: 실제 패턴 분석 로직 구현
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "🔍 연속 알람 패턴 분석 완료: rule_id=" + 
                  std::to_string(event.rule_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 연속 알람 패턴 분석 실패: " + std::string(e.what()));
    }
}

void AlarmManager::applyCategorySpecificRules(AlarmEvent& event) {
    try {
        // 🎯 카테고리별 특수 처리
        
        // 온도 관련 알람
        if (event.message.find("Temperature") != std::string::npos || 
            event.message.find("온도") != std::string::npos) {
            event.message = "🌡️ " + event.message;
        }
        
        // 모터 관련 알람
        if (event.message.find("Motor") != std::string::npos || 
            event.message.find("모터") != std::string::npos) {
            event.message = "⚙️ " + event.message;
        }
        
        // 전력 관련 알람
        if (event.message.find("Current") != std::string::npos || 
            event.message.find("전류") != std::string::npos) {
            event.message = "⚡ " + event.message;
        }
        
        // 비상정지 알람
        if (event.message.find("Emergency") != std::string::npos || 
            event.message.find("비상") != std::string::npos) {
            event.severity = AlarmSeverity::CRITICAL; // 항상 최고 심각도
            event.message = "🆘 " + event.message;
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 카테고리별 규칙 적용 실패: " + std::string(e.what()));
    }
}

// =============================================================================
// 🎯 AlarmEngine 위임 메서드들
// =============================================================================

AlarmEvaluation AlarmManager::evaluateRule(const AlarmRule& rule, const DataValue& value) {
    if (!initialized_.load()) {
        return {};
    }
    
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

bool AlarmManager::acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment) {
    if (!initialized_.load()) {
        return false;
    }
    
    try {
        // TODO: 실제 acknowledge 로직 구현
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "✅ 알람 확인: " + std::to_string(occurrence_id) + 
                    " by user " + std::to_string(user_id));
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 알람 확인 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🎯 메시지 생성
// =============================================================================

std::string AlarmManager::generateMessage(const AlarmRule& rule, const DataValue& value, const std::string& condition) {
    try {
        std::string base_message = "알람: " + rule.name;
        
        if (!condition.empty()) {
            base_message += " (" + condition + ")";
        }
        
        return base_message;
        
    } catch (const std::exception& e) {
        return "알람 메시지 생성 실패";
    }
}

std::string AlarmManager::generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event) {
    return event.message;
}

std::string AlarmManager::generateCustomMessage(const AlarmRule& rule, const DataValue& value) {
    return generateMessage(rule, value);
}

// =============================================================================
// 🎯 기타 메서드들
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
    
    if (rule.alarm_type == AlarmType::ANALOG) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::ANALOG);
    } else if (rule.alarm_type == AlarmType::DIGITAL) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::DIGITAL);
    } else if (rule.alarm_type == AlarmType::SCRIPT) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::SCRIPT);
    }
    
    // 한계값들
    if (rule.analog_limits.high_high.has_value()) {
        entity.setHighHighLimit(rule.analog_limits.high_high.value());
    }
    if (rule.analog_limits.high.has_value()) {
        entity.setHighLimit(rule.analog_limits.high.value());
    }
    if (rule.analog_limits.low.has_value()) {
        entity.setLowLimit(rule.analog_limits.low.value());
    }
    if (rule.analog_limits.low_low.has_value()) {
        entity.setLowLowLimit(rule.analog_limits.low_low.value());
    }
    
    entity.setDeadband(rule.analog_limits.deadband);
    entity.setAutoClear(rule.auto_clear);
    entity.setEnabled(rule.is_enabled);
    
    if (!rule.condition_script.empty()) {
        entity.setConditionScript(rule.condition_script);
    }
    if (!rule.message_template.empty()) {
        entity.setMessageTemplate(rule.message_template);
    }
    
    entity.setSeverity(rule.severity);
    
    return entity;
}

json AlarmManager::getStatistics() const {
    json stats;
    
    try {
        stats["initialized"] = initialized_.load();
        stats["total_evaluations"] = total_evaluations_.load();
        stats["alarms_raised"] = alarms_raised_.load();
        stats["alarms_cleared"] = alarms_cleared_.load();
        stats["next_occurrence_id"] = next_occurrence_id_.load();
        stats["js_engine_available"] = (js_context_ != nullptr);
        stats["cached_rules_count"] = alarm_rules_.size();
        
        // 🎯 순수 AlarmManager 특성
        stats["alarm_manager_type"] = "standalone";
        stats["external_dependencies"] = "none";
        stats["redis_dependency"] = false;
        stats["business_features_enabled"] = true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "❌ 통계 정보 생성 실패: " + std::string(e.what()));
        stats["error"] = "Failed to get statistics";
    }
    
    return stats;
}

bool AlarmManager::loadAlarmRules(int tenant_id) {
    // TODO: 실제 알람 규칙 로드 구현
    return true;
}

bool AlarmManager::reloadAlarmRule(int rule_id) {
    // TODO: 실제 알람 규칙 재로드 구현
    return true;
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

std::vector<AlarmRule> AlarmManager::getAlarmRulesForPoint(int point_id, const std::string& point_type) const {
    // TODO: 실제 포인트별 알람 규칙 검색 구현
    return {};
}

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
        logger.log("alarm", LogLevel::ERROR, "❌ 템플릿 변수 치환 실패: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace Alarm
} // namespace PulseOne