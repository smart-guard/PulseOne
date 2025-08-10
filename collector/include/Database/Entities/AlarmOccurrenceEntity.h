// =============================================================================
// collector/include/Database/Entities/AlarmOccurrenceEntity.h
// PulseOne AlarmOccurrenceEntity - 알람 발생 이력 엔티티 (BaseEntity 패턴)
// =============================================================================

/**
 * @file AlarmOccurrenceEntity.h
 * @brief PulseOne 알람 발생 이력 엔티티 - BaseEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🎯 알람 발생 이력 관리:
 * - alarm_occurrences 테이블과 1:1 매핑
 * - BaseEntity<AlarmOccurrenceEntity> 상속
 * - JSON 직렬화/역직렬화 지원
 * - 상태 전환 (active → acknowledged → cleared)
 * - 알림 관리 (notification tracking)
 */

#ifndef ALARM_OCCURRENCE_ENTITY_H
#define ALARM_OCCURRENCE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <map>
#include <vector>

// Forward declarations
class LogManager;
class ConfigManager;

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 알람 발생 이력 엔티티 클래스
 * 
 * alarm_occurrences 테이블과 1:1 대응:
 * - 알람 발생 시점 데이터 저장
 * - 상태 전환 추적 (active/acknowledged/cleared)
 * - 알림 전송 이력 관리
 * - JSON 기반 확장 데이터 지원
 */
class AlarmOccurrenceEntity : public BaseEntity<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 🎯 열거형 정의들
    // =======================================================================
    
    /**
     * @brief 알람 발생 상태
     */
    enum class State {
        ACTIVE = 0,        // 활성 상태 (발생)
        ACKNOWLEDGED,      // 인지됨 (사용자가 확인)
        CLEARED,          // 해제됨 (조건 정상화)
        SUPPRESSED,       // 억제됨 (자동 억제)
        SHELVED          // 보류됨 (일시적 비활성화)
    };
    
    /**
     * @brief 알람 심각도 (AlarmRuleEntity와 동일)
     */
    enum class Severity {
        CRITICAL = 0,     // 긴급
        HIGH,            // 높음
        MEDIUM,          // 보통
        LOW,             // 낮음
        INFO             // 정보
    };
    
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    AlarmOccurrenceEntity();
    
    /**
     * @brief ID로 생성자
     * @param occurrence_id 알람 발생 이력 ID
     */
    explicit AlarmOccurrenceEntity(int occurrence_id);
    
    /**
     * @brief 알람 규칙으로 생성자 (새 발생 생성 시)
     * @param rule_id 알람 규칙 ID
     * @param tenant_id 테넌트 ID
     * @param trigger_value 트리거 값
     * @param alarm_message 알람 메시지
     * @param severity 심각도
     */
    AlarmOccurrenceEntity(int rule_id, int tenant_id, const std::string& trigger_value, 
                         const std::string& alarm_message, Severity severity);
    
    virtual ~AlarmOccurrenceEntity() = default;
    
    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;
    
    // =======================================================================
    // 🎯 기본 정보 getter/setter
    // =======================================================================
    
    // 규칙 연관
    int getRuleId() const { return rule_id_; }
    void setRuleId(int rule_id) { rule_id_ = rule_id; markDirty(); }
    
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markDirty(); }
    
    // 발생 정보
    std::chrono::system_clock::time_point getOccurrenceTime() const { return occurrence_time_; }
    void setOccurrenceTime(const std::chrono::system_clock::time_point& time) { 
        occurrence_time_ = time; markDirty(); 
    }
    
    std::string getTriggerValue() const { return trigger_value_; }
    void setTriggerValue(const std::string& value) { trigger_value_ = value; markDirty(); }
    
    std::string getTriggerCondition() const { return trigger_condition_; }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markDirty(); }
    
    std::string getAlarmMessage() const { return alarm_message_; }
    void setAlarmMessage(const std::string& message) { alarm_message_ = message; markDirty(); }
    
    Severity getSeverity() const { return severity_; }
    void setSeverity(Severity severity) { severity_ = severity; markDirty(); }
    
    // 상태
    State getState() const { return state_; }
    void setState(State state) { state_ = state; markDirty(); }
    
    // =======================================================================
    // 🎯 Acknowledge 정보 getter/setter
    // =======================================================================
    
    std::optional<std::chrono::system_clock::time_point> getAcknowledgedTime() const { 
        return acknowledged_time_; 
    }
    void setAcknowledgedTime(const std::chrono::system_clock::time_point& time) { 
        acknowledged_time_ = time; markDirty(); 
    }
    
    std::optional<int> getAcknowledgedBy() const { return acknowledged_by_; }
    void setAcknowledgedBy(int user_id) { acknowledged_by_ = user_id; markDirty(); }
    
    std::string getAcknowledgeComment() const { return acknowledge_comment_; }
    void setAcknowledgeComment(const std::string& comment) { 
        acknowledge_comment_ = comment; markDirty(); 
    }
    
    // =======================================================================
    // 🎯 Clear 정보 getter/setter
    // =======================================================================
    
    std::optional<std::chrono::system_clock::time_point> getClearedTime() const { 
        return cleared_time_; 
    }
    void setClearedTime(const std::chrono::system_clock::time_point& time) { 
        cleared_time_ = time; markDirty(); 
    }
    
    std::string getClearedValue() const { return cleared_value_; }
    void setClearedValue(const std::string& value) { cleared_value_ = value; markDirty(); }
    
    std::string getClearComment() const { return clear_comment_; }
    void setClearComment(const std::string& comment) { clear_comment_ = comment; markDirty(); }
    
    // =======================================================================
    // 🎯 알림 정보 getter/setter
    // =======================================================================
    
    bool isNotificationSent() const { return notification_sent_; }
    void setNotificationSent(bool sent) { notification_sent_ = sent; markDirty(); }
    
    std::optional<std::chrono::system_clock::time_point> getNotificationTime() const { 
        return notification_time_; 
    }
    void setNotificationTime(const std::chrono::system_clock::time_point& time) { 
        notification_time_ = time; markDirty(); 
    }
    
    int getNotificationCount() const { return notification_count_; }
    void setNotificationCount(int count) { notification_count_ = count; markDirty(); }
    
    std::string getNotificationResult() const { return notification_result_; }
    void setNotificationResult(const std::string& result) { notification_result_ = result; markDirty(); }
    
    // =======================================================================
    // 🎯 추가 컨텍스트 getter/setter
    // =======================================================================
    
    std::string getContextData() const { return context_data_; }
    void setContextData(const std::string& data) { context_data_ = data; markDirty(); }
    
    std::string getSourceName() const { return source_name_; }
    void setSourceName(const std::string& name) { source_name_ = name; markDirty(); }
    
    std::string getLocation() const { return location_; }
    void setLocation(const std::string& location) { location_ = location; markDirty(); }
    
    // =======================================================================
    // 🎯 열거형 변환 유틸리티 메서드들
    // =======================================================================
    
    static std::string stateToString(State state);
    static State stringToState(const std::string& state_str);
    static std::vector<std::string> getAllStateStrings();
    
    static std::string severityToString(Severity severity);
    static Severity stringToSeverity(const std::string& severity_str);
    static std::vector<std::string> getAllSeverityStrings();
    
    // =======================================================================
    // 🎯 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 발생 인지 처리
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 시 true
     */
    bool acknowledge(int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 발생 해제 처리
     * @param cleared_value 해제 시점 값
     * @param comment 해제 코멘트
     * @return 성공 시 true
     */
    bool clear(const std::string& cleared_value = "", const std::string& comment = "");
    
    /**
     * @brief 알람 상태 변경
     * @param new_state 새로운 상태
     * @return 성공 시 true
     */
    bool changeState(State new_state);
    
    /**
     * @brief 알림 전송 완료 기록
     * @param result_json 전송 결과 JSON
     */
    void markNotificationSent(const std::string& result_json = "");
    
    /**
     * @brief 알림 재전송 카운트 증가
     */
    void incrementNotificationCount();
    
    /**
     * @brief 발생 경과 시간 계산 (초 단위)
     * @return 경과 시간 (초)
     */
    long long getElapsedSeconds() const;
    
    /**
     * @brief 상태별 지속 시간 계산
     * @return 현재 상태 지속 시간 (초)
     */
    long long getStateDurationSeconds() const;
    
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
    // 🎯 JSON 직렬화/역직렬화 (BaseEntity 확장)
    // =======================================================================
    
    /**
     * @brief JSON 문자열로 변환
     * @return JSON 문자열
     */
    std::string toJson() const override;
    
    /**
     * @brief JSON 문자열에서 로드
     * @param json_str JSON 문자열
     * @return 성공 시 true
     */
    bool fromJson(const std::string& json_str) override;
    
    /**
     * @brief 간단한 정보만 포함하는 JSON (성능 최적화)
     * @return 축약된 JSON 문자열
     */
    std::string toSummaryJson() const;
    
    // =======================================================================
    // 🎯 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 표시용 문자열 생성
     * @return 사람이 읽기 쉬운 형태의 문자열
     */
    std::string getDisplayString() const;
    
    /**
     * @brief 로그용 문자열 생성
     * @return 로그에 적합한 형태의 문자열
     */
    std::string getLogString() const;
    
    /**
     * @brief 유효성 검증
     * @return 유효하면 true
     */
    bool isValid() const override;

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
    std::optional<std::chrono::system_clock::time_point> notification_time_; // notification_time
    int notification_count_;                                  // notification_count
    std::string notification_result_;                         // notification_result (JSON)
    
    // 추가 컨텍스트
    std::string context_data_;                                // context_data (JSON)
    std::string source_name_;                                 // source_name
    std::string location_;                                    // location
    
    // 🎯 의존성 (BaseEntity 추가)
    mutable LogManager* logger_;
    mutable ConfigManager* config_manager_;
    
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 의존성 초기화
     */
    void initializeDependencies() const;
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     * @param tp 타임포인트
     * @return ISO 8601 형식 문자열
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 타임스탬프로 변환
     * @param timestamp_str ISO 8601 형식 문자열
     * @return 타임포인트
     */
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& timestamp_str) const;
    
    /**
     * @brief JSON 값 파싱 헬퍼
     * @param json_str JSON 문자열
     * @return 파싱된 맵
     */
    std::map<std::string, std::string> parseJsonToMap(const std::string& json_str) const;
    
    /**
     * @brief 맵을 JSON으로 변환 헬퍼
     * @param data 데이터 맵
     * @return JSON 문자열
     */
    std::string mapToJson(const std::map<std::string, std::string>& data) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_ENTITY_H