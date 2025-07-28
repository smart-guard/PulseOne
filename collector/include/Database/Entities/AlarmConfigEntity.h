#ifndef ALARM_CONFIG_ENTITY_H
#define ALARM_CONFIG_ENTITY_H

/**
 * @file AlarmConfigEntity.h
 * @brief PulseOne AlarmConfig Entity - 알람 설정 엔티티
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <chrono>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 알람 설정 엔티티 클래스
 */
class AlarmConfigEntity : public BaseEntity {
public:
    // 알람 심각도 열거형
    enum class Severity {
        LOW = 1,
        MEDIUM = 2,
        HIGH = 3,
        CRITICAL = 4
    };
    
    // 알람 조건 타입 열거형
    enum class ConditionType {
        THRESHOLD,      // 임계값
        RANGE,          // 범위
        CHANGE,         // 변화량
        TIMEOUT,        // 타임아웃
        DISCRETE        // 이산값
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 알람 설정)
     */
    AlarmConfigEntity();
    
    /**
     * @brief ID로 생성자 (기존 알람 설정 로드)
     * @param id 알람 설정 ID
     */
    explicit AlarmConfigEntity(int id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~AlarmConfigEntity() = default;

    // =======================================================================
    // BaseEntity 인터페이스 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    std::string getTableName() const override { return "alarm_definitions"; }
    bool isValid() const override;

    // =======================================================================
    // Getter 메서드들
    // =======================================================================
    
    int getTenantId() const { return tenant_id_; }
    int getSiteId() const { return site_id_; }
    int getDataPointId() const { return data_point_id_; }
    int getVirtualPointId() const { return virtual_point_id_; }
    const std::string& getAlarmName() const { return alarm_name_; }
    const std::string& getDescription() const { return description_; }
    Severity getSeverity() const { return severity_; }
    ConditionType getConditionType() const { return condition_type_; }
    double getThresholdValue() const { return threshold_value_; }
    double getHighLimit() const { return high_limit_; }
    double getLowLimit() const { return low_limit_; }
    int getTimeoutSeconds() const { return timeout_seconds_; }
    bool isEnabled() const { return is_enabled_; }
    bool isAutoAcknowledge() const { return auto_acknowledge_; }
    int getDelaySeconds() const { return delay_seconds_; }
    const std::string& getMessageTemplate() const { return message_template_; }

    // =======================================================================
    // Setter 메서드들
    // =======================================================================
    
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setSiteId(int site_id) { site_id_ = site_id; markModified(); }
    void setDataPointId(int data_point_id) { data_point_id_ = data_point_id; markModified(); }
    void setVirtualPointId(int virtual_point_id) { virtual_point_id_ = virtual_point_id; markModified(); }
    void setAlarmName(const std::string& name) { alarm_name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    void setSeverity(Severity severity) { severity_ = severity; markModified(); }
    void setConditionType(ConditionType type) { condition_type_ = type; markModified(); }
    void setThresholdValue(double value) { threshold_value_ = value; markModified(); }
    void setHighLimit(double value) { high_limit_ = value; markModified(); }
    void setLowLimit(double value) { low_limit_ = value; markModified(); }
    void setTimeoutSeconds(int seconds) { timeout_seconds_ = seconds; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setAutoAcknowledge(bool auto_ack) { auto_acknowledge_ = auto_ack; markModified(); }
    void setDelaySeconds(int seconds) { delay_seconds_ = seconds; markModified(); }
    void setMessageTemplate(const std::string& template_str) { message_template_ = template_str; markModified(); }

    // =======================================================================
    // 알람 관련 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 값이 알람 조건을 만족하는지 확인
     * @param value 확인할 값
     * @return 알람 조건 만족 시 true
     */
    bool checkAlarmCondition(double value) const;
    
    /**
     * @brief 심각도를 문자열로 변환
     * @return 심각도 문자열
     */
    std::string severityToString() const;
    
    /**
     * @brief 조건 타입을 문자열로 변환
     * @return 조건 타입 문자열
     */
    std::string conditionTypeToString() const;
    
    /**
     * @brief 알람 메시지 생성 (템플릿 기반)
     * @param current_value 현재 값
     * @param point_name 포인트 이름
     * @return 생성된 알람 메시지
     */
    std::string generateAlarmMessage(double current_value, const std::string& point_name) const;
    
    /**
     * @brief 알람 설정 복사
     * @return 복사된 알람 설정 (ID는 0)
     */
    AlarmConfigEntity clone() const;

    // =======================================================================
    // 정적 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 문자열을 심각도로 변환
     * @param severity_str 심각도 문자열
     * @return 심각도 열거형
     */
    static Severity stringToSeverity(const std::string& severity_str);
    
    /**
     * @brief 문자열을 조건 타입으로 변환
     * @param condition_str 조건 타입 문자열
     * @return 조건 타입 열거형
     */
    static ConditionType stringToConditionType(const std::string& condition_str);

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                          // 외래키 (tenants.id)
    int site_id_;                            // 외래키 (sites.id) - 선택사항
    int data_point_id_;                      // 외래키 (data_points.id) - 선택사항
    int virtual_point_id_;                   // 외래키 (virtual_points.id) - 선택사항
    
    // 알람 기본 정보
    std::string alarm_name_;
    std::string description_;
    Severity severity_;
    
    // 조건 설정
    ConditionType condition_type_;
    double threshold_value_;                 // 임계값
    double high_limit_;                      // 상한값
    double low_limit_;                       // 하한값
    int timeout_seconds_;                    // 타임아웃 (초)
    
    // 알람 동작
    bool is_enabled_;
    bool auto_acknowledge_;                  // 자동 확인 여부
    int delay_seconds_;                      // 지연 시간 (초)
    
    // 메시지 설정
    std::string message_template_;           // 메시지 템플릿
    
    // 시간 정보 (BaseEntity에서 상속)
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 멤버 변수로 매핑
     * @param row 데이터베이스 행
     */
    void mapRowToMembers(const std::map<std::string, std::string>& row);
    
    /**
     * @brief INSERT SQL 쿼리 생성
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 쿼리 생성
     */
    std::string buildUpdateSQL() const;
    
    /**
     * @brief 메시지 템플릿에서 변수 치환
     * @param template_str 템플릿 문자열
     * @param value 현재 값
     * @param point_name 포인트 이름
     * @return 치환된 메시지
     */
    std::string replaceTemplateVariables(const std::string& template_str, 
                                       double value, 
                                       const std::string& point_name) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_ENTITY_H