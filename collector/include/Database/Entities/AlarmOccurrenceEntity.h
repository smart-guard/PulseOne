// =============================================================================
// collector/include/Database/Entities/AlarmOccurrenceEntity.h
// PulseOne AlarmOccurrenceEntity - 알람 발생 이력
// =============================================================================

#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // 알람 상태
    enum class AlarmState {
        ACTIVE = 0,
        ACKNOWLEDGED = 1,
        CLEARED = 2,
        SUPPRESSED = 3,
        SHELVED = 4
    };

public:
    // 생성자
    AlarmOccurrenceEntity();
    explicit AlarmOccurrenceEntity(int64_t id);
    AlarmOccurrenceEntity(int rule_id, int tenant_id);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "alarm_occurrences"; }
    
    // Getters - 기본 정보
    int64_t getOccurrenceId() const { return occurrence_id_; }
    int getRuleId() const { return rule_id_; }
    int getTenantId() const { return tenant_id_; }
    
    // Getters - 발생 정보
    std::chrono::system_clock::time_point getOccurrenceTime() const { return occurrence_time_; }
    const nlohmann::json& getTriggerValue() const { return trigger_value_; }
    const std::string& getTriggerCondition() const { return trigger_condition_; }
    const std::string& getAlarmMessage() const { return alarm_message_; }
    const std::string& getSeverity() const { return severity_; }
    
    // Getters - 상태
    AlarmState getState() const { return state_; }
    
    // Getters - Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> getAcknowledgedTime() const { return acknowledged_time_; }
    std::optional<int> getAcknowledgedBy() const { return acknowledged_by_; }
    const std::string& getAcknowledgeComment() const { return acknowledge_comment_; }
    
    // Getters - Clear 정보
    std::optional<std::chrono::system_clock::time_point> getClearedTime() const { return cleared_time_; }
    const nlohmann::json& getClearedValue() const { return cleared_value_; }
    const std::string& getClearComment() const { return clear_comment_; }
    
    // Getters - 알림 정보
    bool isNotificationSent() const { return notification_sent_; }
    std::optional<std::chrono::system_clock::time_point> getNotificationTime() const { return notification_time_; }
    int getNotificationCount() const { return notification_count_; }
    const nlohmann::json& getNotificationResult() const { return notification_result_; }
    
    // Getters - 컨텍스트
    const nlohmann::json& getContextData() const { return context_data_; }
    const std::string& getSourceName() const { return source_name_; }
    const std::string& getLocation() const { return location_; }
    
    // Setters - 기본 정보
    void setOccurrenceId(int64_t id) { occurrence_id_ = id; setId(id); markModified(); }
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    // Setters - 발생 정보
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { 
        occurrence_time_ = time; markModified(); 
    }
    void setTriggerValue(const nlohmann::json& value) { trigger_value_ = value; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    void setSeverity(const std::string& severity) { severity_ = severity; markModified(); }
    
    // Setters - 상태
    void setState(AlarmState state) { state_ = state; markModified(); }
    
    // Setters - Acknowledge
    void acknowledge(int user_id, const std::string& comment = "");
    
    // Setters - Clear
    void clear(const nlohmann::json& clear_value = {}, const std::string& comment = "");
    
    // Setters - 알림
    void setNotificationSent(bool sent) { notification_sent_ = sent; markModified(); }
    void incrementNotificationCount() { notification_count_++; markModified(); }
    void setNotificationResult(const nlohmann::json& result) { notification_result_ = result; markModified(); }
    
    // Setters - 컨텍스트
    void setContextData(const nlohmann::json& data) { context_data_ = data; markModified(); }
    void setSourceName(const std::string& name) { source_name_ = name; markModified(); }
    void setLocation(const std::string& location) { location_ = location; markModified(); }
    
    // 비즈니스 로직
    bool isActive() const { return state_ == AlarmState::ACTIVE; }
    bool isAcknowledged() const { return state_ == AlarmState::ACKNOWLEDGED; }
    bool isCleared() const { return state_ == AlarmState::CLEARED; }
    bool canAcknowledge() const { return state_ == AlarmState::ACTIVE; }
    bool canClear() const { return state_ == AlarmState::ACTIVE || state_ == AlarmState::ACKNOWLEDGED; }
    
    // JSON 변환
    nlohmann::json toJson() const;
    static AlarmOccurrenceEntity fromJson(const nlohmann::json& j);
    
    // 헬퍼 메서드
    static std::string stateToString(AlarmState state);
    static AlarmState stringToState(const std::string& str);
    
private:
    // 기본 정보
    int64_t occurrence_id_ = 0;
    int rule_id_ = 0;
    int tenant_id_ = 0;
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;
    nlohmann::json trigger_value_;
    std::string trigger_condition_;
    std::string alarm_message_;
    std::string severity_;
    
    // 상태
    AlarmState state_ = AlarmState::ACTIVE;
    
    // Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time_;
    std::optional<int> acknowledged_by_;
    std::string acknowledge_comment_;
    
    // Clear 정보
    std::optional<std::chrono::system_clock::time_point> cleared_time_;
    nlohmann::json cleared_value_;
    std::string clear_comment_;
    
    // 알림 정보
    bool notification_sent_ = false;
    std::optional<std::chrono::system_clock::time_point> notification_time_;
    int notification_count_ = 0;
    nlohmann::json notification_result_;
    
    // 컨텍스트
    nlohmann::json context_data_;
    std::string source_name_;
    std::string location_;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H