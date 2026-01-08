#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

/**
 * @file AlarmOccurrenceEntity.h
 * @brief PulseOne AlarmOccurrenceEntity - 실제 DB 스키마 완전 호환
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🎯 실제 alarm_occurrences 테이블 스키마 완전 반영:
 * - device_id (INTEGER) - 정수형으로 수정
 * - cleared_by 필드 제거 (실제 DB에 없음)
 * - point_id (INTEGER), category, tags 추가
 * - created_at, updated_at 추가
 */

#include "Database/Entities/BaseEntity.h"
#include "Alarm/AlarmTypes.h"
#include "Logging/LogManager.h"
#include <chrono>
#include <string>
#include <optional>
#include <sstream>
#include <iomanip>
#include <vector>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief AlarmOccurrenceEntity 클래스 - 실제 DB 스키마 완전 호환
 * 
 * 🎯 실제 DB 스키마 매핑:
 * CREATE TABLE alarm_occurrences (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     rule_id INTEGER NOT NULL,
 *     tenant_id INTEGER NOT NULL,
 *     occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     trigger_value TEXT,
 *     trigger_condition TEXT,
 *     alarm_message TEXT,
 *     severity VARCHAR(20),
 *     state VARCHAR(20) DEFAULT 'active',
 *     acknowledged_time DATETIME,
 *     acknowledged_by INTEGER,
 *     acknowledge_comment TEXT,
 *     cleared_time DATETIME,
 *     cleared_value TEXT,
 *     clear_comment TEXT,
 *     -- cleared_by 필드는 실제 DB에 없음
 *     notification_sent INTEGER DEFAULT 0,
 *     notification_time DATETIME,
 *     notification_count INTEGER DEFAULT 0,
 *     notification_result TEXT,
 *     context_data TEXT,
 *     source_name VARCHAR(100),
 *     location VARCHAR(200),
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     device_id INTEGER,                   -- 정수형!
 *     point_id INTEGER,
 *     category VARCHAR(50) DEFAULT NULL,
 *     tags TEXT DEFAULT NULL
 * );
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // AlarmTypes.h 타입 별칭
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
    // Getter 메서드들 (실제 DB 스키마와 완전 일치)
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
    
    // Acknowledge 필드들
    const std::optional<std::chrono::system_clock::time_point>& getAcknowledgedTime() const { return acknowledged_time_; }
    const std::optional<int>& getAcknowledgedBy() const { return acknowledged_by_; }
    const std::string& getAcknowledgeComment() const { return acknowledge_comment_; }
    
    // Clear 필드들 (cleared_by 추가 - 논리적으로 필요)
    const std::optional<std::chrono::system_clock::time_point>& getClearedTime() const { return cleared_time_; }
    const std::string& getClearedValue() const { return cleared_value_; }
    const std::string& getClearComment() const { return clear_comment_; }
    const std::optional<int>& getClearedBy() const { return cleared_by_; }  // 감사 추적을 위해 필요
    
    // 알림 정보
    bool isNotificationSent() const { return notification_sent_; }
    const std::optional<std::chrono::system_clock::time_point>& getNotificationTime() const { return notification_time_; }
    int getNotificationCount() const { return notification_count_; }
    const std::string& getNotificationResult() const { return notification_result_; }
    
    // 컨텍스트 정보
    const std::string& getContextData() const { return context_data_; }
    const std::string& getSourceName() const { return source_name_; }
    const std::string& getLocation() const { return location_; }
    
    // 시간 정보
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    
    // 디바이스/포인트 정보 (수정됨: device_id INTEGER)
    const std::optional<int>& getDeviceId() const { return device_id_; }  // 정수형으로 변경!
    const std::optional<int>& getPointId() const { return point_id_; }
    
    // 분류 시스템
    const std::optional<std::string>& getCategory() const { return category_; }
    const std::vector<std::string>& getTags() const { return tags_; }

    // =======================================================================
    // Setter 메서드들 (실제 DB 스키마와 완전 일치)
    // =======================================================================
    
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { occurrence_time_ = time; markModified(); }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    void setSeverity(AlarmSeverity severity) { severity_ = severity; markModified(); }
    void setState(AlarmState state) { state_ = state; markModified(); }
    
    // Acknowledge 필드들
    void setAcknowledgedTime(const std::chrono::system_clock::time_point& time) { acknowledged_time_ = time; markModified(); }
    void setAcknowledgedBy(int user_id) { acknowledged_by_ = user_id; markModified(); }
    void setAcknowledgeComment(const std::string& comment) { acknowledge_comment_ = comment; markModified(); }
    
    // Clear 필드들 (cleared_by 추가 - 감사 추적을 위해 필요)
    void setClearedTime(const std::chrono::system_clock::time_point& time) { cleared_time_ = time; markModified(); }
    void setClearedValue(const std::string& value) { cleared_value_ = value; markModified(); }
    void setClearComment(const std::string& comment) { clear_comment_ = comment; markModified(); }
    void setClearedBy(int user_id) { cleared_by_ = user_id; markModified(); }
    
    // 알림 정보
    void setNotificationSent(bool sent) { notification_sent_ = sent; markModified(); }
    void setNotificationTime(const std::chrono::system_clock::time_point& time) { notification_time_ = time; markModified(); }
    void setNotificationCount(int count) { notification_count_ = count; markModified(); }
    void setNotificationResult(const std::string& result) { notification_result_ = result; markModified(); }
    
    // 컨텍스트 정보
    void setContextData(const std::string& data) { context_data_ = data; markModified(); }
    void setSourceName(const std::string& name) { source_name_ = name; markModified(); }
    void setLocation(const std::string& location) { location_ = location; markModified(); }
    
    // 시간 정보
    void setCreatedAt(const std::chrono::system_clock::time_point& time) { created_at_ = time; markModified(); }
    void setUpdatedAt(const std::chrono::system_clock::time_point& time) { updated_at_ = time; markModified(); }
    
    // 디바이스/포인트 정보 (수정됨: device_id INTEGER)
    void setDeviceId(int device_id) { device_id_ = device_id; markModified(); }  // 정수형으로 변경!
    void setPointId(int point_id) { point_id_ = point_id; markModified(); }
    
    // 분류 시스템
    void setCategory(const std::string& category) { category_ = category; markModified(); }
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; markModified(); }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    bool acknowledge(int user_id, const std::string& comment = "");
    bool clear(int user_id, const std::string& cleared_value = "", const std::string& comment = "");  // cleared_by 파라미터 복원
    
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
    
    // 태그 관리
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    
    // 편의 메서드들
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
    // 멤버 변수들 (실제 DB 스키마와 1:1 매핑)
    // =======================================================================
    
    // 기본 필드
    int rule_id_ = 0;
    int tenant_id_ = 0;
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;
    std::string trigger_value_;
    std::string trigger_condition_;
    std::string alarm_message_;
    AlarmSeverity severity_ = AlarmSeverity::MEDIUM;
    AlarmState state_ = AlarmState::ACTIVE;
    
    // Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time_;
    std::optional<int> acknowledged_by_;
    std::string acknowledge_comment_;
    
    // Clear 정보 (cleared_by 복원 - 감사 추적을 위해 필요)
    std::optional<std::chrono::system_clock::time_point> cleared_time_;
    std::string cleared_value_;
    std::string clear_comment_;
    std::optional<int> cleared_by_;  // 감사 추적을 위해 필요한 필드
    
    // 알림 정보
    bool notification_sent_ = false;
    std::optional<std::chrono::system_clock::time_point> notification_time_;
    int notification_count_ = 0;
    std::string notification_result_;
    
    // 컨텍스트 정보
    std::string context_data_ = "{}";
    std::string source_name_;
    std::string location_;
    
    // 시간 정보
    std::chrono::system_clock::time_point created_at_ = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point updated_at_ = std::chrono::system_clock::now();
    
    // 디바이스/포인트 정보 (수정됨)
    std::optional<int> device_id_;  // 정수형으로 변경!
    std::optional<int> point_id_;
    
    // 분류 시스템
    std::optional<std::string> category_;
    std::vector<std::string> tags_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
    void updateTimestamp();
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H