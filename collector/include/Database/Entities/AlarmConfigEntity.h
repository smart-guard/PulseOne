#ifndef ALARM_CONFIG_ENTITY_H
#define ALARM_CONFIG_ENTITY_H

/**
 * @file AlarmConfigEntity.h
 * @brief PulseOne AlarmConfig Entity - 알람 설정 엔티티 (DeviceEntity 패턴 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 DeviceEntity/DataPointEntity 패턴 100% 준수:
 * - BaseEntity<AlarmConfigEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화
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
 * @brief 알람 설정 엔티티 클래스 (BaseEntity 템플릿 상속)
 */
class AlarmConfigEntity : public BaseEntity<AlarmConfigEntity> {
public:
    // =======================================================================
    // 알람 관련 열거형
    // =======================================================================
    
    enum class Severity {
        LOW,        // 낮음
        MEDIUM,     // 보통
        HIGH,       // 높음
        CRITICAL    // 치명적
    };
    
    enum class ConditionType {
        GREATER_THAN,       // >
        LESS_THAN,          // <
        EQUAL,              // ==
        NOT_EQUAL,          // !=
        BETWEEN,            // 범위 내
        OUT_OF_RANGE,       // 범위 밖
        RATE_OF_CHANGE      // 변화율
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
    // BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
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
     * @brief DB에 엔티티 업데이트
     * @return 성공 시 true
     */
    bool updateToDatabase() override;
    
    /**
     * @brief DB에서 엔티티 삭제
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override;
    
    /**
     * @brief 테이블명 반환
     * @return 테이블명
     */
    std::string getTableName() const override { return "alarm_definitions"; }
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool isValid() const override;
    // =======================================================================
    // JSON 직렬화 (BaseEntity 순수 가상 함수)
    // =======================================================================
    
    /**
     * @brief JSON으로 변환
     * @return JSON 객체
     */
    json toJson() const override;
    
    /**
     * @brief JSON에서 로드
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override;
    
    /**
     * @brief 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;

    // =======================================================================
    // Getter 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    int getTenantId() const { return tenant_id_; }
    int getSiteId() const { return site_id_; }
    int getDataPointId() const { return data_point_id_; }
    int getVirtualPointId() const { return virtual_point_id_; }
    const std::string& getName() const { return alarm_name_; }
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
    // Setter 메서드들 (markModified 패턴 통일)
    // =======================================================================
    
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified(); 
    }
    
    void setSiteId(int site_id) { 
        site_id_ = site_id; 
        markModified(); 
    }
    
    void setDataPointId(int data_point_id) { 
        data_point_id_ = data_point_id; 
        markModified(); 
    }
    
    void setVirtualPointId(int virtual_point_id) { 
        virtual_point_id_ = virtual_point_id; 
        markModified(); 
    }
    
    void setName(const std::string& name) { 
        alarm_name_ = name; 
        markModified(); 
    }
    
    void setDescription(const std::string& desc) { 
        description_ = desc; 
        markModified(); 
    }
    
    void setSeverity(Severity severity) { 
        severity_ = severity; 
        markModified(); 
    }
    
    void setConditionType(ConditionType type) { 
        condition_type_ = type; 
        markModified(); 
    }
    
    void setThresholdValue(double value) { 
        threshold_value_ = value; 
        markModified(); 
    }
    
    void setHighLimit(double value) { 
        high_limit_ = value; 
        markModified(); 
    }
    
    void setLowLimit(double value) { 
        low_limit_ = value; 
        markModified(); 
    }
    
    void setTimeoutSeconds(int seconds) { 
        timeout_seconds_ = seconds; 
        markModified(); 
    }
    
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified(); 
    }
    
    void setAutoAcknowledge(bool auto_ack) { 
        auto_acknowledge_ = auto_ack; 
        markModified(); 
    }
    
    void setDelaySeconds(int seconds) { 
        delay_seconds_ = seconds; 
        markModified(); 
    }
    
    void setMessageTemplate(const std::string& template_str) { 
        message_template_ = template_str; 
        markModified(); 
    }

    // =======================================================================
    // 비즈니스 로직 메서드들 (DataPointEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 알람 조건 평가
     * @param value 현재 값
     * @return 알람 조건에 맞으면 true
     */
    bool evaluateCondition(double value) const;
    
    /**
     * @brief 알람 메시지 생성
     * @param current_value 현재 값
     * @return 생성된 알람 메시지
     */
    std::string generateAlarmMessage(double current_value) const;
    
    /**
     * @brief 심각도 레벨 숫자 반환
     * @return 심각도 레벨 (0-3)
     */
    int getSeverityLevel() const;
    
    /**
     * @brief 심각도 문자열 변환
     * @param severity 심각도 열거형
     * @return 심각도 문자열
     */
    static std::string severityToString(Severity severity);
    
    /**
     * @brief 문자열을 심각도로 변환
     * @param severity_str 심각도 문자열
     * @return 심각도 열거형
     */
    static Severity stringToSeverity(const std::string& severity_str);
    
    /**
     * @brief 조건 타입 문자열 변환
     * @param condition 조건 타입 열거형
     * @return 조건 타입 문자열
     */
    static std::string conditionTypeToString(ConditionType condition);
    
    /**
     * @brief 문자열을 조건 타입으로 변환
     * @param condition_str 조건 타입 문자열
     * @return 조건 타입 열거형
     */
    static ConditionType stringToConditionType(const std::string& condition_str);

    // =======================================================================
    // 고급 기능 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 알람 설정을 JSON으로 추출
     * @return 설정 JSON
     */
    json extractConfiguration() const;
    
    /**
     * @brief 알람 평가용 컨텍스트 조회
     * @return 평가 컨텍스트
     */
    json getEvaluationContext() const;
    
    /**
     * @brief 알람 정보 조회
     * @return 알람 정보
     */
    json getAlarmInfo() const;

private:
    // =======================================================================
    // 멤버 변수들 (DeviceEntity 패턴)
    // =======================================================================
    
    int tenant_id_;                 // 테넌트 ID
    int site_id_;                   // 사이트 ID (선택사항)
    int data_point_id_;             // 데이터포인트 ID (실제 포인트)
    int virtual_point_id_;          // 가상포인트 ID (가상 포인트)
    
    std::string alarm_name_;        // 알람명
    std::string description_;       // 설명
    Severity severity_;             // 심각도
    ConditionType condition_type_;  // 조건 타입
    
    double threshold_value_;        // 임계값
    double high_limit_;             // 상한값
    double low_limit_;              // 하한값
    int timeout_seconds_;           // 타임아웃 (초)
    
    bool is_enabled_;               // 활성화 여부
    bool auto_acknowledge_;         // 자동 확인 여부
    int delay_seconds_;             // 지연 시간 (초)
    std::string message_template_;  // 메시지 템플릿

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief DB 행을 엔티티로 매핑
     * @param row DB 행 데이터
     * @return 성공 시 true
     */
    bool mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief INSERT SQL 생성
     * @return INSERT SQL 문
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 생성
     * @return UPDATE SQL 문
     */
    std::string buildUpdateSQL() const;
    
    /**
     * @brief 메시지 템플릿 변수 치환
     * @param template_str 템플릿 문자열
     * @param value 현재 값
     * @return 치환된 메시지
     */
    std::string replaceTemplateVariables(const std::string& template_str, double value) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_ENTITY_H