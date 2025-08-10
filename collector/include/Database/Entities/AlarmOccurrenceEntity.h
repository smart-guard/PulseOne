#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

/**
 * @file AlarmOccurrenceEntity.h
 * @brief PulseOne AlarmOccurrenceEntity - DeviceEntity/DataPointEntity 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🔥 DeviceEntity/DataPointEntity 패턴 완전 적용:
 * - BaseEntity<AlarmOccurrenceEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화 인라인
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * 
 * 🎯 DB 스키마 (alarm_occurrences 테이블):
 * - id, rule_id, tenant_id
 * - occurrence_time, trigger_value, trigger_condition
 * - alarm_message, severity, state
 * - acknowledged_time, acknowledged_by, acknowledge_comment
 * - cleared_time, cleared_value, clear_comment
 * - notification_sent, notification_time, notification_count, notification_result
 * - context_data, source_name, location
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 알람 발생 이력 엔티티 클래스 (BaseEntity 템플릿 상속)
 * 
 * SQLQueries::AlarmOccurrence 테이블과 1:1 매칭:
 * CREATE TABLE alarm_occurrences (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     rule_id INTEGER NOT NULL,
 *     tenant_id INTEGER NOT NULL,
 *     occurrence_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 *     trigger_value TEXT,
 *     trigger_condition TEXT,
 *     alarm_message TEXT NOT NULL,
 *     severity TEXT NOT NULL DEFAULT 'medium',
 *     state TEXT NOT NULL DEFAULT 'active',
 *     acknowledged_time TIMESTAMP NULL,
 *     acknowledged_by INTEGER NULL,
 *     acknowledge_comment TEXT,
 *     cleared_time TIMESTAMP NULL,
 *     cleared_value TEXT,
 *     clear_comment TEXT,
 *     notification_sent BOOLEAN DEFAULT 0,
 *     notification_time TIMESTAMP NULL,
 *     notification_count INTEGER DEFAULT 0,
 *     notification_result TEXT,
 *     context_data TEXT,
 *     source_name TEXT,
 *     location TEXT
 * );
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 열거형 정의 (DB 스키마와 일치)
    // =======================================================================
    
    /**
     * @brief 알람 심각도 (severity 컬럼)
     */
    enum class Severity {
        LOW,        // 낮음
        MEDIUM,     // 중간
        HIGH,       // 높음
        CRITICAL    // 긴급
    };
    
    /**
     * @brief 알람 상태 (state 컬럼)
     */
    enum class State {
        ACTIVE,         // 활성
        ACKNOWLEDGED,   // 확인됨
        CLEARED         // 해제됨
    };

    // =======================================================================
    // 생성자 및 소멸자 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 알람 발생)
     */
    AlarmOccurrenceEntity();
    
    /**
     * @brief ID로 생성자 (기존 알람 발생 로드)
     * @param id 알람 발생 ID
     */
    explicit AlarmOccurrenceEntity(int id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~AlarmOccurrenceEntity() = default;

    // =======================================================================
    // 추가 생성자 (기존 구현과 일치)
    // =======================================================================
    
    /**
     * @brief 알람 생성 헬퍼 생성자
     * @param rule_id 알람 규칙 ID
     * @param tenant_id 테넌트 ID
     * @param trigger_value 트리거 값
     * @param alarm_message 알람 메시지
     * @param severity 심각도
     */
    AlarmOccurrenceEntity(int rule_id, int tenant_id, const std::string& trigger_value, 
                         const std::string& alarm_message, Severity severity);

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    /**
     * @brief DB에서 엔티티 로드
     * @return 성공 시 true
     */
    bool loadFromDatabase() override;
    
    /**
     * @brief DB에 엔티티 저장
     * @return 성공 시 true
     */
    bool saveToDatabase() override;
    
    /**
     * @brief DB에서 엔티티 삭제
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override;
    
    /**
     * @brief DB에 엔티티 업데이트
     * @return 성공 시 true
     */
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현 - DeviceEntity 패턴)
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            // 기본 식별자
            j["id"] = getId();
            j["rule_id"] = rule_id_;
            j["tenant_id"] = tenant_id_;
            
            // 발생 정보
            j["occurrence_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                occurrence_time_.time_since_epoch()).count();
            j["trigger_value"] = trigger_value_;
            j["trigger_condition"] = trigger_condition_;
            j["alarm_message"] = alarm_message_;
            j["severity"] = severityToString(severity_);
            j["state"] = stateToString(state_);
            
            // Optional 필드들
            if (acknowledged_time_.has_value()) {
                j["acknowledged_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    acknowledged_time_.value().time_since_epoch()).count();
            }
            if (acknowledged_by_.has_value()) {
                j["acknowledged_by"] = acknowledged_by_.value();
            }
            j["acknowledge_comment"] = acknowledge_comment_;
            
            if (cleared_time_.has_value()) {
                j["cleared_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    cleared_time_.value().time_since_epoch()).count();
            }
            j["cleared_value"] = cleared_value_;
            j["clear_comment"] = clear_comment_;
            
            // 알림 정보
            j["notification_sent"] = notification_sent_;
            if (notification_time_.has_value()) {
                j["notification_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    notification_time_.value().time_since_epoch()).count();
            }
            j["notification_count"] = notification_count_;
            j["notification_result"] = notification_result_;
            
            // 컨텍스트 정보
            j["context_data"] = context_data_;
            j["source_name"] = source_name_;
            j["location"] = location_;
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::toJson failed: " + std::string(e.what()));
            }
        }
        return j;
    }
    
    void fromJson(const json& j) override {
        try {
            // 기본 식별자
            if (j.contains("id")) setId(j["id"]);
            if (j.contains("rule_id")) rule_id_ = j["rule_id"];
            if (j.contains("tenant_id")) tenant_id_ = j["tenant_id"];
            
            // 발생 정보
            if (j.contains("occurrence_time")) {
                auto ms = std::chrono::milliseconds(j["occurrence_time"]);
                occurrence_time_ = std::chrono::system_clock::time_point(ms);
            }
            if (j.contains("trigger_value")) trigger_value_ = j["trigger_value"];
            if (j.contains("trigger_condition")) trigger_condition_ = j["trigger_condition"];
            if (j.contains("alarm_message")) alarm_message_ = j["alarm_message"];
            if (j.contains("severity")) severity_ = stringToSeverity(j["severity"]);
            if (j.contains("state")) state_ = stringToState(j["state"]);
            
            // Optional 필드들
            if (j.contains("acknowledged_time") && !j["acknowledged_time"].is_null()) {
                auto ms = std::chrono::milliseconds(j["acknowledged_time"]);
                acknowledged_time_ = std::chrono::system_clock::time_point(ms);
            }
            if (j.contains("acknowledged_by") && !j["acknowledged_by"].is_null()) {
                acknowledged_by_ = j["acknowledged_by"];
            }
            if (j.contains("acknowledge_comment")) acknowledge_comment_ = j["acknowledge_comment"];
            
            if (j.contains("cleared_time") && !j["cleared_time"].is_null()) {
                auto ms = std::chrono::milliseconds(j["cleared_time"]);
                cleared_time_ = std::chrono::system_clock::time_point(ms);
            }
            if (j.contains("cleared_value")) cleared_value_ = j["cleared_value"];
            if (j.contains("clear_comment")) clear_comment_ = j["clear_comment"];
            
            // 알림 정보
            if (j.contains("notification_sent")) notification_sent_ = j["notification_sent"];
            if (j.contains("notification_time") && !j["notification_time"].is_null()) {
                auto ms = std::chrono::milliseconds(j["notification_time"]);
                notification_time_ = std::chrono::system_clock::time_point(ms);
            }
            if (j.contains("notification_count")) notification_count_ = j["notification_count"];
            if (j.contains("notification_result")) notification_result_ = j["notification_result"];
            
            // 컨텍스트 정보
            if (j.contains("context_data")) context_data_ = j["context_data"];
            if (j.contains("source_name")) source_name_ = j["source_name"];
            if (j.contains("location")) location_ = j["location"];
            
            markModified();
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::fromJson failed: " + std::string(e.what()));
            }
        }
    }

    // =======================================================================
    // 유효성 검사 (DeviceEntity 패턴)
    // =======================================================================
    
    bool isValid() const override {
        return getId() > 0 && 
               rule_id_ > 0 && 
               tenant_id_ > 0 && 
               !alarm_message_.empty();
    }

    // =======================================================================
    // Getter 메서드들 (기본 필드)
    // =======================================================================
    
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
    // Setter 메서드들 (markModified 호출)
    // =======================================================================
    
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { 
        occurrence_time_ = time; markModified(); 
    }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    void setSeverity(Severity severity) { severity_ = severity; markModified(); }
    void setState(State state) { state_ = state; markModified(); }
    
    // Optional 필드들
    void setAcknowledgedTime(const std::chrono::system_clock::time_point& time) { 
        acknowledged_time_ = time; markModified(); 
    }
    void setAcknowledgedBy(int user_id) { acknowledged_by_ = user_id; markModified(); }
    void setAcknowledgeComment(const std::string& comment) { 
        acknowledge_comment_ = comment; markModified(); 
    }
    
    void setClearedTime(const std::chrono::system_clock::time_point& time) { 
        cleared_time_ = time; markModified(); 
    }
    void setClearedValue(const std::string& value) { cleared_value_ = value; markModified(); }
    void setClearComment(const std::string& comment) { clear_comment_ = comment; markModified(); }
    
    // 알림 정보
    void setNotificationSent(bool sent) { notification_sent_ = sent; markModified(); }
    void setNotificationTime(const std::chrono::system_clock::time_point& time) { 
        notification_time_ = time; markModified(); 
    }
    void setNotificationCount(int count) { notification_count_ = count; markModified(); }
    void setNotificationResult(const std::string& result) { 
        notification_result_ = result; markModified(); 
    }
    
    // 컨텍스트 정보
    void setContextData(const std::string& data) { context_data_ = data; markModified(); }
    void setSourceName(const std::string& name) { source_name_ = name; markModified(); }
    void setLocation(const std::string& location) { location_ = location; markModified(); }

    // =======================================================================
    // 헬퍼 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 활성 상태인지 확인
     */
    bool isActive() const { return state_ == State::ACTIVE; }
    
    /**
     * @brief 확인된 상태인지 확인
     */
    bool isAcknowledged() const { return state_ == State::ACKNOWLEDGED; }
    
    /**
     * @brief 해제된 상태인지 확인
     */
    bool isCleared() const { return state_ == State::CLEARED; }
    
    /**
     * @brief 알람을 확인 상태로 변경 (DB 즉시 반영)
     */
    bool acknowledge(int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람을 해제 상태로 변경 (DB 즉시 반영)
     */
    bool clear(const std::string& cleared_value = "", const std::string& comment = "");
    
    // =======================================================================
    // 추가 메서드들 (기존 구현과 일치)
    // =======================================================================
    
    /**
     * @brief 문자열 표현 반환
     */
    std::string toString() const;
    
    /**
     * @brief 알람 발생 후 경과 시간 (초)
     */
    long long getElapsedSeconds() const;
    
    /**
     * @brief 알림 전송 처리 완료 마킹
     */
    void markNotificationSent(const std::string& result_json = "");
    
    /**
     * @brief 알림 카운트 증가
     */
    void incrementNotificationCount();
    
    /**
     * @brief 상태 변경 (DB 즉시 반영)
     */
    bool changeState(State new_state);

    // =======================================================================
    // 정적 유틸리티 메서드들 (기존 구현과 일치)
    // =======================================================================
    
    static std::string severityToString(Severity severity);
    static Severity stringToSeverity(const std::string& str);
    static std::string stateToString(State state);
    static State stringToState(const std::string& str);

private:
    // =======================================================================
    // 헬퍼 메서드들 (기존 구현과 일치)
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;

private:
    // =======================================================================
    // 멤버 변수들 (DB 스키마와 1:1 매칭)
    // =======================================================================
    
    // 기본 식별자
    int rule_id_;                   // NOT NULL
    int tenant_id_;                 // NOT NULL
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;     // NOT NULL
    std::string trigger_value_;                                 // TEXT
    std::string trigger_condition_;                             // TEXT
    std::string alarm_message_;                                 // NOT NULL
    Severity severity_;                                         // NOT NULL, DEFAULT 'medium'
    State state_;                                               // NOT NULL, DEFAULT 'active'
    
    // Acknowledge 정보 (Optional)
    std::optional<std::chrono::system_clock::time_point> acknowledged_time_;    // NULL
    std::optional<int> acknowledged_by_;                                        // NULL
    std::string acknowledge_comment_;                                           // TEXT
    
    // Clear 정보 (Optional)
    std::optional<std::chrono::system_clock::time_point> cleared_time_;         // NULL
    std::string cleared_value_;                                                 // TEXT
    std::string clear_comment_;                                                 // TEXT
    
    // 알림 정보
    bool notification_sent_;                                                    // DEFAULT 0
    std::optional<std::chrono::system_clock::time_point> notification_time_;    // NULL
    int notification_count_;                                                    // DEFAULT 0
    std::string notification_result_;                                           // TEXT
    
    // 컨텍스트 정보
    std::string context_data_;                                                  // TEXT (JSON)
    std::string source_name_;                                                   // TEXT
    std::string location_;                                                      // TEXT
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H