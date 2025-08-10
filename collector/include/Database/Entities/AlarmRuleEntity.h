// =============================================================================
// collector/include/Database/Entities/AlarmRuleEntity.h
// PulseOne AlarmRuleEntity - DeviceEntity 패턴 100% 준수 (수정완료)
// =============================================================================

#ifndef ALARM_RULE_ENTITY_H
#define ALARM_RULE_ENTITY_H

/**
 * @file AlarmRuleEntity.h
 * @brief PulseOne AlarmRuleEntity - DeviceEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - BaseEntity<AlarmRuleEntity> 상속 (CRTP)
 * - alarm_rules 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
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
};
#endif

namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class AlarmRuleRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 알람 규칙 엔티티 클래스 (BaseEntity 상속, 정규화된 스키마)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE alarm_rules (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     tenant_id INTEGER NOT NULL,
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     
 *     -- 대상 정보
 *     target_type VARCHAR(20) NOT NULL,  -- 'data_point', 'virtual_point', 'group'
 *     target_id INTEGER,
 *     target_group VARCHAR(100),
 *     
 *     -- 알람 타입
 *     alarm_type VARCHAR(20) NOT NULL,  -- 'analog', 'digital', 'script'
 *     
 *     -- 아날로그 알람 설정
 *     high_high_limit REAL,
 *     high_limit REAL,
 *     low_limit REAL,
 *     low_low_limit REAL,
 *     deadband REAL DEFAULT 0,
 *     rate_of_change REAL DEFAULT 0,
 *     
 *     -- 디지털 알람 설정
 *     trigger_condition VARCHAR(20),  -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
 *     
 *     -- 스크립트 기반 알람
 *     condition_script TEXT,
 *     message_script TEXT,
 *     
 *     -- 메시지 커스터마이징
 *     message_config TEXT,  -- JSON 형태
 *     message_template TEXT,
 *     
 *     -- 우선순위
 *     severity VARCHAR(20) DEFAULT 'medium',  -- 'critical', 'high', 'medium', 'low', 'info'
 *     priority INTEGER DEFAULT 100,
 *     
 *     -- 자동 처리
 *     auto_acknowledge INTEGER DEFAULT 0,
 *     acknowledge_timeout_min INTEGER DEFAULT 0,
 *     auto_clear INTEGER DEFAULT 1,
 *     
 *     -- 억제 규칙
 *     suppression_rules TEXT,  -- JSON 형태
 *     
 *     -- 알림 설정
 *     notification_enabled INTEGER DEFAULT 1,
 *     notification_delay_sec INTEGER DEFAULT 0,
 *     notification_repeat_interval_min INTEGER DEFAULT 0,
 *     notification_channels TEXT,  -- JSON 배열
 *     notification_recipients TEXT,  -- JSON 배열
 *     
 *     -- 상태
 *     is_enabled INTEGER DEFAULT 1,
 *     is_latched INTEGER DEFAULT 0,
 *     
 *     -- 타임스탬프
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     created_by INTEGER,
 *     
 *     FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
 *     FOREIGN KEY (created_by) REFERENCES users(id)
 * );
 */
class AlarmRuleEntity : public BaseEntity<AlarmRuleEntity> {
public:
    // =======================================================================
    // 알람 타입 열거형 (DB 스키마와 일치)
    // =======================================================================
    
    enum class AlarmType {
        ANALOG = 0,         // 아날로그 값 기반 알람
        DIGITAL = 1,        // 디지털 값 기반 알람
        SCRIPT = 2          // 스크립트 기반 알람
    };
    
    enum class Severity {
        CRITICAL = 0,       // 치명적
        HIGH = 1,           // 높음
        MEDIUM = 2,         // 보통
        LOW = 3,            // 낮음
        INFO = 4            // 정보
    };
    
    enum class DigitalTrigger {
        ON_TRUE = 0,        // true로 변경 시
        ON_FALSE = 1,       // false로 변경 시
        ON_CHANGE = 2,      // 값 변경 시
        ON_RISING = 3,      // 상승 엣지
        ON_FALLING = 4      // 하강 엣지
    };
    
    enum class TargetType {
        DATA_POINT = 0,     // 데이터 포인트
        VIRTUAL_POINT = 1,  // 가상 포인트
        GROUP = 2           // 그룹
    };

public:
    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    AlarmRuleEntity();
    explicit AlarmRuleEntity(int alarm_id);
    virtual ~AlarmRuleEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현)
    // =======================================================================
    
    json toJson() const override {
        json j;
        
        try {
            // 기본 식별자
            j["id"] = getId();
            j["tenant_id"] = tenant_id_;
            j["name"] = name_;
            j["description"] = description_;
            
            // 대상 정보
            j["target_type"] = targetTypeToString(target_type_);
            if (target_id_.has_value()) {
                j["target_id"] = target_id_.value();
            }
            j["target_group"] = target_group_;
            
            // 알람 타입
            j["alarm_type"] = alarmTypeToString(alarm_type_);
            
            // 아날로그 설정
            if (high_high_limit_.has_value()) j["high_high_limit"] = high_high_limit_.value();
            if (high_limit_.has_value()) j["high_limit"] = high_limit_.value();
            if (low_limit_.has_value()) j["low_limit"] = low_limit_.value();
            if (low_low_limit_.has_value()) j["low_low_limit"] = low_low_limit_.value();
            j["deadband"] = deadband_;
            j["rate_of_change"] = rate_of_change_;
            
            // 디지털 설정
            j["trigger_condition"] = digitalTriggerToString(trigger_condition_);
            
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
            j["severity"] = severityToString(severity_);
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
            if (j.contains("target_type")) target_type_ = stringToTargetType(j["target_type"].get<std::string>());
            if (j.contains("target_id") && !j["target_id"].is_null()) target_id_ = j["target_id"].get<int>();
            if (j.contains("target_group")) target_group_ = j["target_group"].get<std::string>();
            
            // 알람 타입
            if (j.contains("alarm_type")) alarm_type_ = stringToAlarmType(j["alarm_type"].get<std::string>());
            
            // 아날로그 설정
            if (j.contains("high_high_limit") && !j["high_high_limit"].is_null()) high_high_limit_ = j["high_high_limit"].get<double>();
            if (j.contains("high_limit") && !j["high_limit"].is_null()) high_limit_ = j["high_limit"].get<double>();
            if (j.contains("low_limit") && !j["low_limit"].is_null()) low_limit_ = j["low_limit"].get<double>();
            if (j.contains("low_low_limit") && !j["low_low_limit"].is_null()) low_low_limit_ = j["low_low_limit"].get<double>();
            if (j.contains("deadband")) deadband_ = j["deadband"].get<double>();
            if (j.contains("rate_of_change")) rate_of_change_ = j["rate_of_change"].get<double>();
            
            // 디지털 설정
            if (j.contains("trigger_condition")) trigger_condition_ = stringToDigitalTrigger(j["trigger_condition"].get<std::string>());
            
            // 스크립트 설정
            if (j.contains("condition_script")) condition_script_ = j["condition_script"].get<std::string>();
            if (j.contains("message_script")) message_script_ = j["message_script"].get<std::string>();
            
            // 메시지 설정
            if (j.contains("message_config")) message_config_ = j["message_config"].dump();
            if (j.contains("message_template")) message_template_ = j["message_template"].get<std::string>();
            
            // 우선순위
            if (j.contains("severity")) severity_ = stringToSeverity(j["severity"].get<std::string>());
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
        oss << ", alarm_type=" << alarmTypeToString(alarm_type_);
        oss << ", severity=" << severityToString(severity_);
        oss << ", enabled=" << (is_enabled_ ? "true" : "false");
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "alarm_rules"; 
    }

    // =======================================================================
    // 기본 속성 접근자 (인라인)
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
    Severity getSeverity() const { return severity_; }
    void setSeverity(Severity severity) { 
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
    
    // =======================================================================
    // 헬퍼 메서드 (CPP에서 구현)
    // =======================================================================
    
    static std::string alarmTypeToString(AlarmType type);
    static AlarmType stringToAlarmType(const std::string& str);
    static std::string severityToString(Severity severity);
    static Severity stringToSeverity(const std::string& str);
    static std::string digitalTriggerToString(DigitalTrigger trigger);
    static DigitalTrigger stringToDigitalTrigger(const std::string& str);
    static std::string targetTypeToString(TargetType type);
    static TargetType stringToTargetType(const std::string& str);

private:
    // =======================================================================
    // 멤버 변수들 (alarm_rules 테이블과 1:1 대응)
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
    Severity severity_ = Severity::MEDIUM;
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
    // 내부 헬퍼 메서드들 (CPP에서 구현)
    // =======================================================================
    
    
    /**
     * @brief 내부 헬퍼 메서드: 타임스탬프를 문자열로 변환 (CPP에서 구현)
     * @param tp 타임스탬프
     * @return 문자열 형태의 타임스탬프
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 내부 헬퍼 메서드: 메시지 템플릿 보간 (CPP에서 구현)
     * @param tmpl 템플릿 문자열
     * @param value 값
     * @param unit 단위
     * @return 보간된 문자열
     */
    std::string interpolateTemplate(const std::string& tmpl, double value, const std::string& unit) const;

    // Forward declarations (순환 참조 방지)
    friend class PulseOne::Database::Repositories::AlarmRuleRepository;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_RULE_ENTITY_H