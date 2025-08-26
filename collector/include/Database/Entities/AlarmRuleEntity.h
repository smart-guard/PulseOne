// =============================================================================
// collector/include/Database/Entities/AlarmRuleEntity.h
// PulseOne AlarmRuleEntity - 스키마 완전 동기화 버전
// =============================================================================

#ifndef ALARM_RULE_ENTITY_H
#define ALARM_RULE_ENTITY_H

/**
 * @file AlarmRuleEntity.h
 * @brief PulseOne AlarmRuleEntity - DB 스키마와 완전 동기화
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 스키마 동기화 완료:
 * - category VARCHAR(50) 추가
 * - tags TEXT (JSON) 추가
 * - escalation_enabled INTEGER 추가
 * - escalation_max_level INTEGER 추가
 * - escalation_rules TEXT (JSON) 추가
 * - 템플릿 관련 필드들 추가
 */

#include "Database/Entities/BaseEntity.h"
#include "Alarm/AlarmTypes.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
struct json {
    template<typename T> T get() const { return T{}; }
    bool contains(const std::string&) const { return false; }
    std::string dump() const { return "{}"; }
    static json parse(const std::string&) { return json{}; }
    static json object() { return json{}; }
    static json array() { return json{}; }
};
#endif

namespace PulseOne {

namespace Database {
namespace Repositories {
    class AlarmRuleRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 알람 규칙 엔티티 클래스 - DB 스키마 완전 동기화
 */
class AlarmRuleEntity : public BaseEntity<AlarmRuleEntity> {
public:
    // AlarmTypes.h 타입 별칭
    using AlarmType = PulseOne::Alarm::AlarmType;
    using AlarmSeverity = PulseOne::Alarm::AlarmSeverity;
    using TargetType = PulseOne::Alarm::TargetType;
    using DigitalTrigger = PulseOne::Alarm::DigitalTrigger;

    // 생성자 및 소멸자
    AlarmRuleEntity();
    explicit AlarmRuleEntity(int alarm_id);
    virtual ~AlarmRuleEntity() = default;

    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // JSON 직렬화/역직렬화
    json toJson() const override {
        json j;
        
        try {
            // 기본 식별자
            j["id"] = getId();
            j["tenant_id"] = tenant_id_;
            j["name"] = name_;
            j["description"] = description_;
            
            // 대상 정보
            j["target_type"] = PulseOne::Alarm::targetTypeToString(target_type_);
            if (target_id_.has_value()) {
                j["target_id"] = target_id_.value();
            }
            j["target_group"] = target_group_;
            
            // 알람 타입
            j["alarm_type"] = PulseOne::Alarm::alarmTypeToString(alarm_type_);
            
            // 아날로그 설정
            if (high_high_limit_.has_value()) j["high_high_limit"] = high_high_limit_.value();
            if (high_limit_.has_value()) j["high_limit"] = high_limit_.value();
            if (low_limit_.has_value()) j["low_limit"] = low_limit_.value();
            if (low_low_limit_.has_value()) j["low_low_limit"] = low_low_limit_.value();
            j["deadband"] = deadband_;
            j["rate_of_change"] = rate_of_change_;
            
            // 디지털 설정
            j["trigger_condition"] = PulseOne::Alarm::digitalTriggerToString(trigger_condition_);
            
            // 스크립트 설정
            j["condition_script"] = condition_script_;
            j["message_script"] = message_script_;
            
            // 메시지 설정
            if (!message_config_.empty()) {
                try {
                    j["message_config"] = json::parse(message_config_);
                } catch (...) {
                    j["message_config"] = json::object();
                }
            }
            j["message_template"] = message_template_;
            
            // 우선순위
            j["severity"] = PulseOne::Alarm::severityToString(severity_);
            j["priority"] = priority_;
            
            // 자동 처리
            j["auto_acknowledge"] = auto_acknowledge_;
            j["acknowledge_timeout_min"] = acknowledge_timeout_min_;
            j["auto_clear"] = auto_clear_;
            
            // 억제 규칙
            if (!suppression_rules_.empty()) {
                try {
                    j["suppression_rules"] = json::parse(suppression_rules_);
                } catch (...) {
                    j["suppression_rules"] = json::object();
                }
            }
            
            // 알림 설정
            j["notification_enabled"] = notification_enabled_;
            j["notification_delay_sec"] = notification_delay_sec_;
            j["notification_repeat_interval_min"] = notification_repeat_interval_min_;
            
            if (!notification_channels_.empty()) {
                try {
                    j["notification_channels"] = json::parse(notification_channels_);
                } catch (...) {
                    j["notification_channels"] = json::array();
                }
            }
            
            if (!notification_recipients_.empty()) {
                try {
                    j["notification_recipients"] = json::parse(notification_recipients_);
                } catch (...) {
                    j["notification_recipients"] = json::array();
                }
            }
            
            // 상태
            j["is_enabled"] = is_enabled_;
            j["is_latched"] = is_latched_;
            
            // 템플릿 관련 필드들 추가
            if (template_id_.has_value()) j["template_id"] = template_id_.value();
            j["rule_group"] = rule_group_;
            j["created_by_template"] = created_by_template_;
            if (last_template_update_.has_value()) {
                j["last_template_update"] = timestampToString(last_template_update_.value());
            }
            
            // 에스컬레이션 설정 추가
            j["escalation_enabled"] = escalation_enabled_;
            j["escalation_max_level"] = escalation_max_level_;
            if (!escalation_rules_.empty()) {
                try {
                    j["escalation_rules"] = json::parse(escalation_rules_);
                } catch (...) {
                    j["escalation_rules"] = json::object();
                }
            }
            
            // 분류 및 태깅 시스템 추가
            j["category"] = category_;
            if (!tags_.empty()) {
                try {
                    j["tags"] = json::parse(tags_);
                } catch (...) {
                    j["tags"] = json::array();
                }
            }
            
            // 타임스탬프
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            j["created_by"] = created_by_;
            
        } catch (const std::exception& e) {
            // JSON 변환 실패 시 기본 객체 반환
        }
        
        return j;
    }
    
    bool fromJson(const json& j) override {
        try {
            // 기본 정보
            if (j.contains("id")) setId(j["id"].get<int>());
            if (j.contains("tenant_id")) tenant_id_ = j["tenant_id"].get<int>();
            if (j.contains("name")) name_ = j["name"].get<std::string>();
            if (j.contains("description")) description_ = j["description"].get<std::string>();
            
            // 대상 정보
            if (j.contains("target_type")) {
                target_type_ = PulseOne::Alarm::stringToTargetType(j["target_type"].get<std::string>());
            }
            if (j.contains("target_id") && !j["target_id"].is_null()) {
                target_id_ = j["target_id"].get<int>();
            }
            if (j.contains("target_group")) target_group_ = j["target_group"].get<std::string>();
            
            // 알람 타입
            if (j.contains("alarm_type")) {
                alarm_type_ = PulseOne::Alarm::stringToAlarmType(j["alarm_type"].get<std::string>());
            }
            
            // 아날로그 설정
            if (j.contains("high_high_limit") && !j["high_high_limit"].is_null()) {
                high_high_limit_ = j["high_high_limit"].get<double>();
            }
            if (j.contains("high_limit") && !j["high_limit"].is_null()) {
                high_limit_ = j["high_limit"].get<double>();
            }
            if (j.contains("low_limit") && !j["low_limit"].is_null()) {
                low_limit_ = j["low_limit"].get<double>();
            }
            if (j.contains("low_low_limit") && !j["low_low_limit"].is_null()) {
                low_low_limit_ = j["low_low_limit"].get<double>();
            }
            if (j.contains("deadband")) deadband_ = j["deadband"].get<double>();
            if (j.contains("rate_of_change")) rate_of_change_ = j["rate_of_change"].get<double>();
            
            // 디지털 설정
            if (j.contains("trigger_condition")) {
                trigger_condition_ = PulseOne::Alarm::stringToDigitalTrigger(j["trigger_condition"].get<std::string>());
            }
            
            // 스크립트 설정
            if (j.contains("condition_script")) condition_script_ = j["condition_script"].get<std::string>();
            if (j.contains("message_script")) message_script_ = j["message_script"].get<std::string>();
            
            // 메시지 설정
            if (j.contains("message_config")) message_config_ = j["message_config"].dump();
            if (j.contains("message_template")) message_template_ = j["message_template"].get<std::string>();
            
            // 우선순위
            if (j.contains("severity")) {
                severity_ = PulseOne::Alarm::stringToSeverity(j["severity"].get<std::string>());
            }
            if (j.contains("priority")) priority_ = j["priority"].get<int>();
            
            // 자동 처리
            if (j.contains("auto_acknowledge")) auto_acknowledge_ = j["auto_acknowledge"].get<bool>();
            if (j.contains("acknowledge_timeout_min")) acknowledge_timeout_min_ = j["acknowledge_timeout_min"].get<int>();
            if (j.contains("auto_clear")) auto_clear_ = j["auto_clear"].get<bool>();
            
            // 억제 규칙
            if (j.contains("suppression_rules")) suppression_rules_ = j["suppression_rules"].dump();
            
            // 알림 설정
            if (j.contains("notification_enabled")) notification_enabled_ = j["notification_enabled"].get<bool>();
            if (j.contains("notification_delay_sec")) notification_delay_sec_ = j["notification_delay_sec"].get<int>();
            if (j.contains("notification_repeat_interval_min")) notification_repeat_interval_min_ = j["notification_repeat_interval_min"].get<int>();
            if (j.contains("notification_channels")) notification_channels_ = j["notification_channels"].dump();
            if (j.contains("notification_recipients")) notification_recipients_ = j["notification_recipients"].dump();
            
            // 상태
            if (j.contains("is_enabled")) is_enabled_ = j["is_enabled"].get<bool>();
            if (j.contains("is_latched")) is_latched_ = j["is_latched"].get<bool>();
            
            // 템플릿 관련 필드들
            if (j.contains("template_id") && !j["template_id"].is_null()) {
                template_id_ = j["template_id"].get<int>();
            }
            if (j.contains("rule_group")) rule_group_ = j["rule_group"].get<std::string>();
            if (j.contains("created_by_template")) created_by_template_ = j["created_by_template"].get<bool>();
            
            // 에스컬레이션 설정
            if (j.contains("escalation_enabled")) escalation_enabled_ = j["escalation_enabled"].get<bool>();
            if (j.contains("escalation_max_level")) escalation_max_level_ = j["escalation_max_level"].get<int>();
            if (j.contains("escalation_rules")) escalation_rules_ = j["escalation_rules"].dump();
            
            // 분류 및 태깅 시스템
            if (j.contains("category")) category_ = j["category"].get<std::string>();
            if (j.contains("tags")) tags_ = j["tags"].dump();
            
            // 타임스탬프
            if (j.contains("created_by")) created_by_ = j["created_by"].get<int>();
            
            markModified();
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "AlarmRuleEntity[";
        oss << "id=" << getId();
        oss << ", tenant_id=" << tenant_id_;
        oss << ", name=" << name_;
        oss << ", alarm_type=" << PulseOne::Alarm::alarmTypeToString(alarm_type_);
        oss << ", severity=" << PulseOne::Alarm::severityToString(severity_);
        oss << ", category=" << category_;
        oss << ", enabled=" << (is_enabled_ ? "true" : "false");
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "alarm_rules"; 
    }

    // =======================================================================
    // 기존 접근자들 (유지)
    // =======================================================================
    
    // 기본 정보
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified();
    }
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { 
        name_ = name; 
        markModified();
    }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified();
    }
    
    // 대상 정보
    TargetType getTargetType() const { return target_type_; }
    void setTargetType(TargetType target_type) { 
        target_type_ = target_type; 
        markModified();
    }
    
    std::optional<int> getTargetId() const { return target_id_; }
    void setTargetId(const std::optional<int>& target_id) { 
        target_id_ = target_id; 
        markModified();
    }
    void setTargetId(int target_id) { 
        target_id_ = target_id; 
        markModified();
    }
    
    const std::string& getTargetGroup() const { return target_group_; }
    void setTargetGroup(const std::string& target_group) { 
        target_group_ = target_group; 
        markModified();
    }
    
    // 알람 타입
    AlarmType getAlarmType() const { return alarm_type_; }
    void setAlarmType(AlarmType alarm_type) { 
        alarm_type_ = alarm_type; 
        markModified();
    }
    
    // 아날로그 설정
    std::optional<double> getHighHighLimit() const { return high_high_limit_; }
    void setHighHighLimit(const std::optional<double>& limit) { 
        high_high_limit_ = limit; 
        markModified();
    }
    void setHighHighLimit(double limit) { 
        high_high_limit_ = limit; 
        markModified();
    }
    
    std::optional<double> getHighLimit() const { return high_limit_; }
    void setHighLimit(const std::optional<double>& limit) { 
        high_limit_ = limit; 
        markModified();
    }
    void setHighLimit(double limit) { 
        high_limit_ = limit; 
        markModified();
    }
    
    std::optional<double> getLowLimit() const { return low_limit_; }
    void setLowLimit(const std::optional<double>& limit) { 
        low_limit_ = limit; 
        markModified();
    }
    void setLowLimit(double limit) { 
        low_limit_ = limit; 
        markModified();
    }
    
    std::optional<double> getLowLowLimit() const { return low_low_limit_; }
    void setLowLowLimit(const std::optional<double>& limit) { 
        low_low_limit_ = limit; 
        markModified();
    }
    void setLowLowLimit(double limit) { 
        low_low_limit_ = limit; 
        markModified();
    }
    
    double getDeadband() const { return deadband_; }
    void setDeadband(double deadband) { 
        deadband_ = deadband; 
        markModified();
    }
    
    double getRateOfChange() const { return rate_of_change_; }
    void setRateOfChange(double rate) { 
        rate_of_change_ = rate; 
        markModified();
    }
    
    // 디지털 설정
    DigitalTrigger getTriggerCondition() const { return trigger_condition_; }
    void setTriggerCondition(DigitalTrigger trigger) { 
        trigger_condition_ = trigger; 
        markModified();
    }
    
    // 스크립트 설정
    const std::string& getConditionScript() const { return condition_script_; }
    void setConditionScript(const std::string& script) { 
        condition_script_ = script; 
        markModified();
    }
    
    const std::string& getMessageScript() const { return message_script_; }
    void setMessageScript(const std::string& script) { 
        message_script_ = script; 
        markModified();
    }
    
    // 메시지 설정
    const std::string& getMessageConfig() const { return message_config_; }
    void setMessageConfig(const std::string& config) { 
        message_config_ = config; 
        markModified();
    }
    
    const std::string& getMessageTemplate() const { return message_template_; }
    void setMessageTemplate(const std::string& template_str) { 
        message_template_ = template_str; 
        markModified();
    }
    
    // 우선순위
    AlarmSeverity getSeverity() const { return severity_; }
    void setSeverity(AlarmSeverity severity) { 
        severity_ = severity; 
        markModified();
    }
    
    int getPriority() const { return priority_; }
    void setPriority(int priority) { 
        priority_ = priority; 
        markModified();
    }
    
    // 자동 처리
    bool isAutoAcknowledge() const { return auto_acknowledge_; }
    void setAutoAcknowledge(bool auto_acknowledge) { 
        auto_acknowledge_ = auto_acknowledge; 
        markModified();
    }
    
    int getAcknowledgeTimeoutMin() const { return acknowledge_timeout_min_; }
    void setAcknowledgeTimeoutMin(int timeout) { 
        acknowledge_timeout_min_ = timeout; 
        markModified();
    }
    
    bool isAutoClear() const { return auto_clear_; }
    void setAutoClear(bool auto_clear) { 
        auto_clear_ = auto_clear; 
        markModified();
    }
    
    // 억제 규칙
    const std::string& getSuppressionRules() const { return suppression_rules_; }
    void setSuppressionRules(const std::string& rules) { 
        suppression_rules_ = rules; 
        markModified();
    }
    
    // 알림 설정
    bool isNotificationEnabled() const { return notification_enabled_; }
    void setNotificationEnabled(bool enabled) { 
        notification_enabled_ = enabled; 
        markModified();
    }
    
    int getNotificationDelaySec() const { return notification_delay_sec_; }
    void setNotificationDelaySec(int delay) { 
        notification_delay_sec_ = delay; 
        markModified();
    }
    
    int getNotificationRepeatIntervalMin() const { return notification_repeat_interval_min_; }
    void setNotificationRepeatIntervalMin(int interval) { 
        notification_repeat_interval_min_ = interval; 
        markModified();
    }
    
    const std::string& getNotificationChannels() const { return notification_channels_; }
    void setNotificationChannels(const std::string& channels) { 
        notification_channels_ = channels; 
        markModified();
    }
    
    const std::string& getNotificationRecipients() const { return notification_recipients_; }
    void setNotificationRecipients(const std::string& recipients) { 
        notification_recipients_ = recipients; 
        markModified();
    }
    
    // 상태
    bool isEnabled() const { return is_enabled_; }
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified();
    }
    
    bool isLatched() const { return is_latched_; }
    void setLatched(bool latched) { 
        is_latched_ = latched; 
        markModified();
    }

    // =======================================================================
    // 새로 추가된 필드들의 접근자
    // =======================================================================
    
    // 템플릿 관련
    std::optional<int> getTemplateId() const { return template_id_; }
    void setTemplateId(const std::optional<int>& template_id) { 
        template_id_ = template_id; 
        markModified();
    }
    void setTemplateId(int template_id) { 
        template_id_ = template_id; 
        markModified();
    }
    
    const std::string& getRuleGroup() const { return rule_group_; }
    void setRuleGroup(const std::string& rule_group) { 
        rule_group_ = rule_group; 
        markModified();
    }
    
    bool isCreatedByTemplate() const { return created_by_template_; }
    void setCreatedByTemplate(bool created_by_template) { 
        created_by_template_ = created_by_template; 
        markModified();
    }
    
    std::optional<std::chrono::system_clock::time_point> getLastTemplateUpdate() const { 
        return last_template_update_; 
    }
    void setLastTemplateUpdate(const std::optional<std::chrono::system_clock::time_point>& update_time) { 
        last_template_update_ = update_time; 
        markModified();
    }
    
    // 에스컬레이션 설정
    bool isEscalationEnabled() const { return escalation_enabled_; }
    void setEscalationEnabled(bool enabled) { 
        escalation_enabled_ = enabled; 
        markModified();
    }
    
    int getEscalationMaxLevel() const { return escalation_max_level_; }
    void setEscalationMaxLevel(int max_level) { 
        escalation_max_level_ = max_level; 
        markModified();
    }
    
    const std::string& getEscalationRules() const { return escalation_rules_; }
    void setEscalationRules(const std::string& rules) { 
        escalation_rules_ = rules; 
        markModified();
    }
    
    // 분류 및 태깅 시스템
    const std::string& getCategory() const { return category_; }
    void setCategory(const std::string& category) { 
        category_ = category; 
        markModified();
    }
    
    const std::string& getTags() const { return tags_; }
    void setTags(const std::string& tags) { 
        tags_ = tags; 
        markModified();
    }
    
    // 타임스탬프
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    int getCreatedBy() const { return created_by_; }
    void setCreatedBy(int created_by) { 
        created_by_ = created_by; 
        markModified();
    }

    // =======================================================================
    // 비즈니스 로직 메서드 (CPP에서 구현)
    // =======================================================================
    
    std::string generateMessage(double value, const std::string& unit = "") const;
    std::string generateDigitalMessage(bool state) const;
    bool isInAlarmState(double value) const;
    bool checkSuppressionRules(const std::string& context_json) const;
    int getSeverityLevel() const;
    
    // 새로 추가된 비즈니스 로직 메서드들
    bool hasEscalationRules() const;
    std::vector<std::string> getTagsList() const;
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    std::string getCategoryDisplayName() const;

private:
    // =======================================================================
    // 기존 멤버 변수들 (유지)
    // =======================================================================
    
    // 기본 정보
    int tenant_id_ = 0;
    std::string name_;
    std::string description_;
    
    // 대상 정보
    TargetType target_type_ = TargetType::DATA_POINT;
    std::optional<int> target_id_;
    std::string target_group_;
    
    // 알람 타입
    AlarmType alarm_type_ = AlarmType::ANALOG;
    
    // 아날로그 설정
    std::optional<double> high_high_limit_;
    std::optional<double> high_limit_;
    std::optional<double> low_limit_;
    std::optional<double> low_low_limit_;
    double deadband_ = 0.0;
    double rate_of_change_ = 0.0;
    
    // 디지털 설정
    DigitalTrigger trigger_condition_ = DigitalTrigger::ON_CHANGE;
    
    // 스크립트 설정
    std::string condition_script_;
    std::string message_script_;
    
    // 메시지 설정 (JSON으로 저장)
    std::string message_config_ = "{}";
    std::string message_template_;
    
    // 우선순위
    AlarmSeverity severity_ = AlarmSeverity::MEDIUM;
    int priority_ = 100;
    
    // 자동 처리
    bool auto_acknowledge_ = false;
    int acknowledge_timeout_min_ = 0;
    bool auto_clear_ = true;
    
    // 억제 규칙 (JSON으로 저장)
    std::string suppression_rules_ = "{}";
    
    // 알림 설정 (JSON으로 저장)
    bool notification_enabled_ = true;
    int notification_delay_sec_ = 0;
    int notification_repeat_interval_min_ = 0;
    std::string notification_channels_ = "[]";
    std::string notification_recipients_ = "[]";
    
    // 상태
    bool is_enabled_ = true;
    bool is_latched_ = false;
    
    // 타임스탬프
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    int created_by_ = 0;

    // =======================================================================
    // 새로 추가된 멤버 변수들
    // =======================================================================
    
    // 템플릿 관련 필드들
    std::optional<int> template_id_;                                    // template_id INTEGER
    std::string rule_group_;                                           // rule_group VARCHAR(36)
    bool created_by_template_ = false;                                 // created_by_template INTEGER DEFAULT 0
    std::optional<std::chrono::system_clock::time_point> last_template_update_; // last_template_update DATETIME
    
    // 에스컬레이션 설정
    bool escalation_enabled_ = false;                                  // escalation_enabled INTEGER DEFAULT 0
    int escalation_max_level_ = 3;                                    // escalation_max_level INTEGER DEFAULT 3
    std::string escalation_rules_ = "{}";                             // escalation_rules TEXT (JSON)
    
    // 분류 및 태깅 시스템
    std::string category_;                                             // category VARCHAR(50) DEFAULT NULL
    std::string tags_ = "[]";                                         // tags TEXT (JSON 배열)

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::string interpolateTemplate(const std::string& tmpl, double value, const std::string& unit) const;

    friend class PulseOne::Database::Repositories::AlarmRuleRepository;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_RULE_ENTITY_H