// =============================================================================
// collector/include/Database/Entities/AlarmRuleEntity.h
// PulseOne AlarmRuleEntity - 기존 패턴 100% 준수
// =============================================================================

#ifndef ALARM_RULE_ENTITY_H
#define ALARM_RULE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class AlarmRuleEntity : public BaseEntity<AlarmRuleEntity> {
public:
    // 알람 타입
    enum class AlarmType {
        ANALOG = 0,
        DIGITAL = 1,
        SCRIPT = 2
    };
    
    // 심각도
    enum class Severity {
        CRITICAL = 0,
        HIGH = 1,
        MEDIUM = 2,
        LOW = 3,
        INFO = 4
    };
    
    // 디지털 트리거
    enum class DigitalTrigger {
        ON_TRUE = 0,
        ON_FALSE = 1,
        ON_CHANGE = 2,
        ON_RISING = 3,
        ON_FALLING = 4
    };
    
    // 대상 타입
    enum class TargetType {
        DATA_POINT = 0,
        VIRTUAL_POINT = 1,
        GROUP = 2
    };

public:
    // 생성자
    AlarmRuleEntity();
    explicit AlarmRuleEntity(int id);
    AlarmRuleEntity(int tenant_id, const std::string& name, AlarmType type);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "alarm_rules"; }
    
    // Getters - 기본 정보
    int getTenantId() const { return tenant_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    // Getters - 대상 정보
    TargetType getTargetType() const { return target_type_; }
    std::optional<int> getTargetId() const { return target_id_; }
    const std::string& getTargetGroup() const { return target_group_; }
    
    // Getters - 알람 타입
    AlarmType getAlarmType() const { return alarm_type_; }
    
    // Getters - 아날로그 설정
    std::optional<double> getHighHighLimit() const { return high_high_limit_; }
    std::optional<double> getHighLimit() const { return high_limit_; }
    std::optional<double> getLowLimit() const { return low_limit_; }
    std::optional<double> getLowLowLimit() const { return low_low_limit_; }
    double getDeadband() const { return deadband_; }
    double getRateOfChange() const { return rate_of_change_; }
    
    // Getters - 디지털 설정
    DigitalTrigger getTriggerCondition() const { return trigger_condition_; }
    
    // Getters - 스크립트 설정
    const std::string& getConditionScript() const { return condition_script_; }
    const std::string& getMessageScript() const { return message_script_; }
    
    // Getters - 메시지
    const nlohmann::json& getMessageConfig() const { return message_config_; }
    const std::string& getMessageTemplate() const { return message_template_; }
    
    // Getters - 우선순위
    Severity getSeverity() const { return severity_; }
    int getPriority() const { return priority_; }
    
    // Getters - 자동 처리
    bool isAutoAcknowledge() const { return auto_acknowledge_; }
    int getAcknowledgeTimeout() const { return acknowledge_timeout_min_; }
    bool isAutoClear() const { return auto_clear_; }
    
    // Getters - 억제
    const nlohmann::json& getSuppressionRules() const { return suppression_rules_; }
    
    // Getters - 알림
    bool isNotificationEnabled() const { return notification_enabled_; }
    int getNotificationDelay() const { return notification_delay_sec_; }
    int getNotificationRepeatInterval() const { return notification_repeat_interval_min_; }
    const nlohmann::json& getNotificationChannels() const { return notification_channels_; }
    const nlohmann::json& getNotificationRecipients() const { return notification_recipients_; }
    
    // Getters - 상태
    bool isEnabled() const { return is_enabled_; }
    bool isLatched() const { return is_latched_; }
    
    // Setters - 기본 정보
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    
    // Setters - 대상 정보
    void setTargetType(TargetType type) { target_type_ = type; markModified(); }
    void setTargetId(std::optional<int> id) { target_id_ = id; markModified(); }
    void setTargetGroup(const std::string& group) { target_group_ = group; markModified(); }
    
    // Setters - 알람 타입
    void setAlarmType(AlarmType type) { alarm_type_ = type; markModified(); }
    
    // Setters - 아날로그 설정
    void setHighHighLimit(std::optional<double> limit) { high_high_limit_ = limit; markModified(); }
    void setHighLimit(std::optional<double> limit) { high_limit_ = limit; markModified(); }
    void setLowLimit(std::optional<double> limit) { low_limit_ = limit; markModified(); }
    void setLowLowLimit(std::optional<double> limit) { low_low_limit_ = limit; markModified(); }
    void setDeadband(double deadband) { deadband_ = deadband; markModified(); }
    void setRateOfChange(double rate) { rate_of_change_ = rate; markModified(); }
    
    // Setters - 디지털 설정
    void setTriggerCondition(DigitalTrigger trigger) { trigger_condition_ = trigger; markModified(); }
    
    // Setters - 스크립트 설정
    void setConditionScript(const std::string& script) { condition_script_ = script; markModified(); }
    void setMessageScript(const std::string& script) { message_script_ = script; markModified(); }
    
    // Setters - 메시지
    void setMessageConfig(const nlohmann::json& config) { message_config_ = config; markModified(); }
    void setMessageTemplate(const std::string& tmpl) { message_template_ = tmpl; markModified(); }
    
    // Setters - 우선순위
    void setSeverity(Severity severity) { severity_ = severity; markModified(); }
    void setPriority(int priority) { priority_ = priority; markModified(); }
    
    // Setters - 자동 처리
    void setAutoAcknowledge(bool auto_ack) { auto_acknowledge_ = auto_ack; markModified(); }
    void setAcknowledgeTimeout(int timeout) { acknowledge_timeout_min_ = timeout; markModified(); }
    void setAutoClear(bool auto_clear) { auto_clear_ = auto_clear; markModified(); }
    
    // Setters - 억제
    void setSuppressionRules(const nlohmann::json& rules) { suppression_rules_ = rules; markModified(); }
    
    // Setters - 알림
    void setNotificationEnabled(bool enabled) { notification_enabled_ = enabled; markModified(); }
    void setNotificationDelay(int delay) { notification_delay_sec_ = delay; markModified(); }
    void setNotificationRepeatInterval(int interval) { notification_repeat_interval_min_ = interval; markModified(); }
    void setNotificationChannels(const nlohmann::json& channels) { notification_channels_ = channels; markModified(); }
    void setNotificationRecipients(const nlohmann::json& recipients) { notification_recipients_ = recipients; markModified(); }
    
    // Setters - 상태
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setLatched(bool latched) { is_latched_ = latched; markModified(); }
    
    // 비즈니스 로직
    std::string generateMessage(double value, const std::string& unit = "") const;
    std::string generateDigitalMessage(bool state) const;
    bool isInAlarmState(double value) const;
    bool checkSuppressionRules(const nlohmann::json& context) const;
    int getSeverityLevel() const;
    
    // JSON 변환
    nlohmann::json toJson() const;
    static AlarmRuleEntity fromJson(const nlohmann::json& j);
    
    // 헬퍼 메서드
    static std::string alarmTypeToString(AlarmType type);
    static AlarmType stringToAlarmType(const std::string& str);
    static std::string severityToString(Severity severity);
    static Severity stringToSeverity(const std::string& str);
    static std::string digitalTriggerToString(DigitalTrigger trigger);
    static DigitalTrigger stringToDigitalTrigger(const std::string& str);
    static std::string targetTypeToString(TargetType type);
    static TargetType stringToTargetType(const std::string& str);
    
private:
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
    
    // 메시지 설정
    nlohmann::json message_config_;
    std::string message_template_;
    
    // 우선순위
    Severity severity_ = Severity::MEDIUM;
    int priority_ = 100;
    
    // 자동 처리
    bool auto_acknowledge_ = false;
    int acknowledge_timeout_min_ = 0;
    bool auto_clear_ = true;
    
    // 억제 규칙
    nlohmann::json suppression_rules_;
    
    // 알림 설정
    bool notification_enabled_ = true;
    int notification_delay_sec_ = 0;
    int notification_repeat_interval_min_ = 0;
    nlohmann::json notification_channels_;
    nlohmann::json notification_recipients_;
    
    // 상태
    bool is_enabled_ = true;
    bool is_latched_ = false;
    
    // 타임스탬프
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    int created_by_ = 0;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::string interpolateTemplate(const std::string& tmpl, double value, const std::string& unit) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_RULE_ENTITY_H