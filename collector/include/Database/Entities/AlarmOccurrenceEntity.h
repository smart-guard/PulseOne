// =============================================================================
// collector/include/Database/Entities/AlarmOccurrenceEntity.h
// PulseOne AlarmOccurrenceEntity 헤더 - AlarmTypes.h 통합 적용 완료
// =============================================================================

#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include "Alarm/AlarmTypes.h"  // 🔥 AlarmTypes.h 추가!
#include "Utils/LogManager.h"
#include <chrono>
#include <string>
#include <optional>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief AlarmOccurrenceEntity 클래스 - AlarmTypes.h 통합 완료
 * @details 공통 타입 시스템 사용, 중복 enum 제거
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 🔥 AlarmTypes.h 타입 별칭 (자체 enum 제거!)
    // =======================================================================
    using AlarmSeverity = PulseOne::Alarm::AlarmSeverity;
    using AlarmState = PulseOne::Alarm::AlarmState;

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmOccurrenceEntity();
    explicit AlarmOccurrenceEntity(int occurrence_id);
    virtual ~AlarmOccurrenceEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;
    
    std::string getTableName() const override {
        return "alarm_occurrences";
    }

    // =======================================================================
    // JSON 직렬화/역직렬화
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& j) override;

    // =======================================================================
    // 유효성 검사
    // =======================================================================
    
    bool isValid() const override;

    // =======================================================================
    // Getter 메서드들
    // =======================================================================
    
    // 기본 필드
    int getRuleId() const { return rule_id_; }
    int getTenantId() const { return tenant_id_; }
    
    const std::chrono::system_clock::time_point& getOccurrenceTime() const { return occurrence_time_; }
    const std::string& getTriggerValue() const { return trigger_value_; }
    const std::string& getTriggerCondition() const { return trigger_condition_; }
    const std::string& getAlarmMessage() const { return alarm_message_; }
    AlarmSeverity getSeverity() const { return severity_; }
    AlarmState getState() const { return state_; }
    
    // Optional 필드들
    const std::optional<std::chrono::system_clock::time_point>& getAcknowledgedTime() const { return acknowledged_time_; }
    const std::optional<int>& getAcknowledgedBy() const { return acknowledged_by_; }
    const std::string& getAcknowledgeComment() const { return acknowledge_comment_; }
    
    const std::optional<std::chrono::system_clock::time_point>& getClearedTime() const { return cleared_time_; }
    const std::string& getClearedValue() const { return cleared_value_; }
    const std::string& getClearComment() const { return clear_comment_; }
    
    // 알림 정보
    bool isNotificationSent() const { return notification_sent_; }
    const std::optional<std::chrono::system_clock::time_point>& getNotificationTime() const { return notification_time_; }
    int getNotificationCount() const { return notification_count_; }
    const std::string& getNotificationResult() const { return notification_result_; }
    
    // 컨텍스트 정보
    const std::string& getContextData() const { return context_data_; }
    const std::string& getSourceName() const { return source_name_; }
    const std::string& getLocation() const { return location_; }

    // =======================================================================
    // Setter 메서드들
    // =======================================================================
    
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { occurrence_time_ = time; markModified(); }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    void setSeverity(AlarmSeverity severity) { severity_ = severity; markModified(); }
    void setState(AlarmState state) { state_ = state; markModified(); }
    
    // Optional 필드들
    void setAcknowledgedTime(const std::chrono::system_clock::time_point& time) { acknowledged_time_ = time; markModified(); }
    void setAcknowledgedBy(int user_id) { acknowledged_by_ = user_id; markModified(); }
    void setAcknowledgeComment(const std::string& comment) { acknowledge_comment_ = comment; markModified(); }
    
    void setClearedTime(const std::chrono::system_clock::time_point& time) { cleared_time_ = time; markModified(); }
    void setClearedValue(const std::string& value) { cleared_value_ = value; markModified(); }
    void setClearComment(const std::string& comment) { clear_comment_ = comment; markModified(); }
    
    // 알림 정보
    void setNotificationSent(bool sent) { notification_sent_ = sent; markModified(); }
    void setNotificationTime(const std::chrono::system_clock::time_point& time) { notification_time_ = time; markModified(); }
    void setNotificationCount(int count) { notification_count_ = count; markModified(); }
    void setNotificationResult(const std::string& result) { notification_result_ = result; markModified(); }
    
    // 컨텍스트 정보
    void setContextData(const std::string& data) { context_data_ = data; markModified(); }
    void setSourceName(const std::string& name) { source_name_ = name; markModified(); }
    void setLocation(const std::string& location) { location_ = location; markModified(); }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    bool acknowledge(int user_id, const std::string& comment = "");
    bool clear(const std::string& cleared_value = "", const std::string& comment = "");
    
    // 상태 확인
    bool isActive() const { return state_ == AlarmState::ACTIVE; }
    bool isAcknowledged() const { return state_ == AlarmState::ACKNOWLEDGED; }
    bool isCleared() const { return state_ == AlarmState::CLEARED; }
    bool isSuppressed() const { return state_ == AlarmState::SUPPRESSED; }
    
    // 경과 시간 계산
    double getElapsedSeconds() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - occurrence_time_);
        return duration.count();
    }
   
    // 🔥 Getter용 편의 메서드들 - AlarmTypes.h 함수 사용
    std::string getSeverityString() const { 
        return PulseOne::Alarm::severityToString(severity_); 
    }
    std::string getStateString() const { 
        return PulseOne::Alarm::stateToString(state_); 
    }
    
    // toString 메서드
    std::string toString() const override;

private:
    // =======================================================================
    // 멤버 변수들 - AlarmTypes.h 타입 사용
    // =======================================================================
    
    // 기본 필드
    int rule_id_ = 0;
    int tenant_id_ = 0;
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;
    std::string trigger_value_;
    std::string trigger_condition_;
    std::string alarm_message_;
    AlarmSeverity severity_ = AlarmSeverity::MEDIUM;  // 🔥 AlarmTypes.h 사용
    AlarmState state_ = AlarmState::ACTIVE;           // 🔥 AlarmTypes.h 사용
    
    // Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time_;
    std::optional<int> acknowledged_by_;
    std::string acknowledge_comment_;
    
    // Clear 정보
    std::optional<std::chrono::system_clock::time_point> cleared_time_;
    std::string cleared_value_;
    std::string clear_comment_;
    
    // 알림 정보
    bool notification_sent_ = false;
    std::optional<std::chrono::system_clock::time_point> notification_time_;
    int notification_count_ = 0;
    std::string notification_result_;
    
    // 컨텍스트 정보
    std::string context_data_ = "{}";
    std::string source_name_;
    std::string location_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H