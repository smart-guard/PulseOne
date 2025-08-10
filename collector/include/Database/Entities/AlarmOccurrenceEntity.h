// =============================================================================
// collector/include/Database/Entities/AlarmOccurrenceEntity.h
// PulseOne AlarmOccurrenceEntity 헤더 - AlarmRuleEntity 패턴 100% 적용
// =============================================================================

#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
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
 * @brief AlarmOccurrenceEntity 클래스 - AlarmRuleEntity 패턴 100% 적용
 * @details BaseEntity 패턴 완전 준수, getTableName() 구현 포함
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 열거형 정의 (AlarmRuleEntity 패턴)
    // =======================================================================
    
    enum class Severity {
        CRITICAL = 0,       // 치명적
        HIGH = 1,           // 높음
        MEDIUM = 2,         // 보통
        LOW = 3,            // 낮음
        INFO = 4            // 정보
    };
    
    enum class State {
        ACTIVE = 0,         // 활성
        ACKNOWLEDGED = 1,   // 인지됨
        CLEARED = 2,        // 해제됨
        SUPPRESSED = 3      // 억제됨
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmOccurrenceEntity();
    explicit AlarmOccurrenceEntity(int occurrence_id);
    virtual ~AlarmOccurrenceEntity() = default;

    // =======================================================================
    // 🔥 BaseEntity 순수 가상 함수 구현 (필수!)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;
    
    // 🔥 getTableName() 구현 (컴파일 에러 해결)
    std::string getTableName() const override {
        return "alarm_occurrences";
    }

    // =======================================================================
    // JSON 직렬화/역직렬화 (AlarmRuleEntity 패턴)
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& j) override;  // 🔥 bool 반환타입으로 수정

    // =======================================================================
    // 유효성 검사
    // =======================================================================
    
    bool isValid() const override;

    // =======================================================================
    // Getter 메서드들 (AlarmRuleEntity 패턴)
    // =======================================================================
    
    // 기본 필드
    int getRuleId() const { return rule_id_; }
    int getTenantId() const { return tenant_id_; }
    
    const std::chrono::system_clock::time_point& getOccurrenceTime() const { return occurrence_time_; }
    const std::string& getTriggerValue() const { return trigger_value_; }
    const std::string& getTriggerCondition() const { return trigger_condition_; }
    const std::string& getAlarmMessage() const { return alarm_message_; }
    Severity getSeverity() const { return severity_; }
    State getState() const { return state_; }
    
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
    // Setter 메서드들 (AlarmRuleEntity 패턴)
    // =======================================================================
    
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { occurrence_time_ = time; markModified(); }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    void setSeverity(Severity severity) { severity_ = severity; markModified(); }
    void setState(State state) { state_ = state; markModified(); }
    
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
    bool isActive() const { return state_ == State::ACTIVE; }
    bool isAcknowledged() const { return state_ == State::ACKNOWLEDGED; }
    bool isCleared() const { return state_ == State::CLEARED; }
    bool isSuppressed() const { return state_ == State::SUPPRESSED; }
    
    // 경과 시간 계산
    double getElapsedSeconds() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - occurrence_time_);
        return duration.count();
    }

    // =======================================================================
    // 문자열 변환 메서드들 (static - AlarmRuleEntity 패턴)
    // =======================================================================
    
    static std::string severityToString(Severity severity);
    static Severity stringToSeverity(const std::string& str);
    static std::string stateToString(State state);
    static State stringToState(const std::string& str);
    
    // Getter용 편의 메서드들
    std::string getSeverityString() const { return severityToString(severity_); }
    std::string getStateString() const { return stateToString(state_); }
    
    // toString 메서드
    std::string toString() const;

private:
    // =======================================================================
    // 멤버 변수들 (AlarmRuleEntity 패턴)
    // =======================================================================
    
    // 기본 필드
    int rule_id_ = 0;
    int tenant_id_ = 0;
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;
    std::string trigger_value_;
    std::string trigger_condition_;
    std::string alarm_message_;
    Severity severity_ = Severity::MEDIUM;
    State state_ = State::ACTIVE;
    
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

    // Forward declarations (순환 참조 방지)
    friend class PulseOne::Database::Repositories::AlarmOccurrenceRepository;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H