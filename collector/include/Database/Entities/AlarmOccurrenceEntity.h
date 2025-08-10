#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

/**
 * @file AlarmOccurrenceEntity.h
 * @brief PulseOne 알람 발생 이력 엔티티 - BaseEntity 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🔥 BaseEntity 패턴 100% 준수:
 * - BaseEntity<AlarmOccurrenceEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일 (markDirty() 아님!)
 * - JSON 직렬화/역직렬화 (json 타입 사용)
 * - DeviceEntity/DataPointEntity와 동일한 패턴
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
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
namespace Database {
namespace Entities {

/**
 * @brief 알람 발생 이력 엔티티 클래스 (BaseEntity 템플릿 상속)
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 알람 심각도 열거형 (alarm_rules 테이블과 일치)
    // =======================================================================
    
    enum class Severity {
        LOW,      // 낮음
        MEDIUM,   // 보통  
        HIGH,     // 높음
        CRITICAL  // 치명적
    };
    
    // =======================================================================
    // 알람 상태 열거형
    // =======================================================================
    
    enum class State {
        ACTIVE,        // 활성 (발생 중)
        ACKNOWLEDGED,  // 인지됨
        CLEARED        // 해제됨
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 알람 발생)
     */
    AlarmOccurrenceEntity();
    
    /**
     * @brief ID로 생성자 (기존 발생 로드)
     * @param occurrence_id 발생 ID
     */
    explicit AlarmOccurrenceEntity(int occurrence_id);
    
    /**
     * @brief 완전 생성자 (새 알람 발생 생성)
     * @param rule_id 규칙 ID
     * @param tenant_id 테넌트 ID
     * @param trigger_value 트리거 값
     * @param alarm_message 알람 메시지
     * @param severity 심각도
     */
    AlarmOccurrenceEntity(int rule_id, int tenant_id, const std::string& trigger_value, 
                         const std::string& alarm_message, Severity severity);
    
    virtual ~AlarmOccurrenceEntity() = default;
    
    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (필수!)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;
    
    /**
     * @brief 테이블 이름 반환 (필수 구현)
     * @return "alarm_occurrences"
     */
    std::string getTableName() const override {
        return "alarm_occurrences";
    }
    
    /**
     * @brief JSON 객체로 변환 (BaseEntity 패턴)
     * @return json 객체 (string 아님!)
     */
    json toJson() const override;
    
    /**
     * @brief JSON 객체에서 로드 (BaseEntity 패턴)
     * @param data json 객체 (string 아님!)
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override;
    
    /**
     * @brief 표시용 문자열 생성 (필수 구현)
     * @return 사람이 읽기 쉬운 형태의 문자열
     */
    std::string toString() const override;
    
    /**
     * @brief 유효성 검증 (필수 구현)
     * @return 유효하면 true
     */
    bool isValid() const override;
    
    // =======================================================================
    // 🎯 기본 정보 getter/setter (markModified 사용)
    // =======================================================================
    
    // 규칙 연관
    int getRuleId() const { return rule_id_; }
    void setRuleId(int rule_id) { rule_id_ = rule_id; markModified(); }
    
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    
    // 발생 정보
    std::chrono::system_clock::time_point getOccurrenceTime() const { return occurrence_time_; }
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { 
        occurrence_time_ = time; markModified(); 
    }
    
    std::string getTriggerValue() const { return trigger_value_; }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markModified(); }
    
    std::string getTriggerCondition() const { return trigger_condition_; }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    
    std::string getAlarmMessage() const { return alarm_message_; }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markModified(); }
    
    Severity getSeverity() const { return severity_; }
    void setSeverity(Severity severity) { severity_ = severity; markModified(); }
    
    // 상태
    State getState() const { return state_; }
    void setState(State state) { state_ = state; markModified(); }
    
    // =======================================================================
    // 🎯 Acknowledge 정보 getter/setter
    // =======================================================================
    
    std::optional<std::chrono::system_clock::time_point> getAcknowledgedTime() const { 
        return acknowledged_time_; 
    }
    void setAcknowledgedTime(const std::chrono::system_clock::time_point& time) { 
        acknowledged_time_ = time; markModified(); 
    }
    
    std::optional<int> getAcknowledgedBy() const { return acknowledged_by_; }
    void setAcknowledgedBy(int user_id) { acknowledged_by_ = user_id; markModified(); }
    
    std::string getAcknowledgeComment() const { return acknowledge_comment_; }
    void setAcknowledgeComment(const std::string& comment) { 
        acknowledge_comment_ = comment; markModified(); 
    }
    
    // =======================================================================
    // 🎯 Clear 정보 getter/setter
    // =======================================================================
    
    std::optional<std::chrono::system_clock::time_point> getClearedTime() const { 
        return cleared_time_; 
    }
    void setClearedTime(const std::chrono::system_clock::time_point& time) { 
        cleared_time_ = time; markModified(); 
    }
    
    std::string getClearedValue() const { return cleared_value_; }
    void setClearedValue(const std::string& value) { cleared_value_ = value; markModified(); }
    
    std::string getClearComment() const { return clear_comment_; }
    void setClearComment(const std::string& comment) { clear_comment_ = comment; markModified(); }
    
    // =======================================================================
    // 🎯 알림 정보 getter/setter
    // =======================================================================
    
    bool isNotificationSent() const { return notification_sent_; }
    void setNotificationSent(bool sent) { notification_sent_ = sent; markModified(); }
    
    std::optional<std::chrono::system_clock::time_point> getNotificationTime() const { 
        return notification_time_; 
    }
    void setNotificationTime(const std::chrono::system_clock::time_point& time) { 
        notification_time_ = time; markModified(); 
    }
    
    int getNotificationCount() const { return notification_count_; }
    void setNotificationCount(int count) { notification_count_ = count; markModified(); }
    
    std::string getNotificationResult() const { return notification_result_; }
    void setNotificationResult(const std::string& result) { notification_result_ = result; markModified(); }
    
    // =======================================================================
    // 🎯 추가 컨텍스트 정보
    // =======================================================================
    
    std::string getContextData() const { return context_data_; }
    void setContextData(const std::string& data) { context_data_ = data; markModified(); }
    
    std::string getSourceName() const { return source_name_; }
    void setSourceName(const std::string& name) { source_name_ = name; markModified(); }
    
    std::string getLocation() const { return location_; }
    void setLocation(const std::string& location) { location_ = location; markModified(); }
    
    // =======================================================================
    // 🎯 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 인지 처리
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 시 true
     */
    bool acknowledge(int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 해제 처리
     * @param cleared_value 해제 시점의 값
     * @param comment 해제 코멘트
     * @return 성공 시 true
     */
    bool clear(const std::string& cleared_value, const std::string& comment = "");
    
    /**
     * @brief 상태 변경
     * @param new_state 새로운 상태
     * @return 성공 시 true
     */
    bool changeState(State new_state);
    
    /**
     * @brief 알림 전송 완료 마킹
     * @param result_json 전송 결과 JSON
     */
    void markNotificationSent(const std::string& result_json = "");
    
    /**
     * @brief 알림 횟수 증가
     */
    void incrementNotificationCount();
    
    // =======================================================================
    // 🎯 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 발생 후 경과 시간 (초)
     * @return 경과 시간 (초)
     */
    long long getElapsedSeconds() const;
    
    /**
     * @brief 활성 상태인지 확인
     * @return 활성 상태면 true
     */
    bool isActive() const { return state_ == State::ACTIVE; }
    
    /**
     * @brief 인지된 상태인지 확인
     * @return 인지 상태면 true
     */
    bool isAcknowledged() const { return state_ == State::ACKNOWLEDGED; }
    
    /**
     * @brief 해제된 상태인지 확인
     * @return 해제 상태면 true
     */
    bool isCleared() const { return state_ == State::CLEARED; }
    
    // =======================================================================
    // 🎯 정적 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief Severity 열거형을 문자열로 변환
     */
    static std::string severityToString(Severity severity);
    
    /**
     * @brief 문자열을 Severity 열거형으로 변환
     */
    static Severity stringToSeverity(const std::string& str);
    
    /**
     * @brief State 열거형을 문자열로 변환
     */
    static std::string stateToString(State state);
    
    /**
     * @brief 문자열을 State 열거형으로 변환
     */
    static State stringToState(const std::string& str);

private:
    // =======================================================================
    // 🎯 멤버 변수들 (alarm_occurrences 테이블 컬럼 매핑)
    // =======================================================================
    
    // 기본 정보
    int rule_id_;                                               // rule_id
    int tenant_id_;                                            // tenant_id
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time_;   // occurrence_time
    std::string trigger_value_;                               // trigger_value (JSON)
    std::string trigger_condition_;                           // trigger_condition
    std::string alarm_message_;                               // alarm_message
    Severity severity_;                                       // severity
    
    // 상태
    State state_;                                             // state
    
    // Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time_;  // acknowledged_time
    std::optional<int> acknowledged_by_;                      // acknowledged_by
    std::string acknowledge_comment_;                         // acknowledge_comment
    
    // Clear 정보
    std::optional<std::chrono::system_clock::time_point> cleared_time_;       // cleared_time
    std::string cleared_value_;                               // cleared_value (JSON)
    std::string clear_comment_;                               // clear_comment
    
    // 알림 정보
    bool notification_sent_;                                  // notification_sent
    std::optional<std::chrono::system_clock::time_point> notification_time_;  // notification_time
    int notification_count_;                                  // notification_count
    std::string notification_result_;                         // notification_result (JSON)
    
    // 추가 컨텍스트
    std::string context_data_;                                // context_data (JSON)
    std::string source_name_;                                 // source_name
    std::string location_;                                    // location
    
    // =======================================================================
    // 🎯 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 시간을 문자열로 변환
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 시간으로 변환
     */
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H