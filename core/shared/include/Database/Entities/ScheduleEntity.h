// =============================================================================
// collector/include/Database/Entities/ScheduleEntity.h
// PulseOne ScheduleEntity - 스케줄 관리
// =============================================================================

#ifndef SCHEDULE_ENTITY_H
#define SCHEDULE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class ScheduleEntity : public BaseEntity<ScheduleEntity> {
public:
    // 스케줄 타입
    enum class ScheduleType {
        TIME_BASED = 0,      // Cron 표현식 기반
        EVENT_BASED = 1,     // 이벤트 트리거
        CONDITION_BASED = 2  // 조건 기반
    };
    
    // 액션 타입
    enum class ActionType {
        WRITE_VALUE = 0,     // 값 쓰기
        EXECUTE_RECIPE = 1,  // 레시피 실행
        RUN_SCRIPT = 2,      // 스크립트 실행
        GENERATE_REPORT = 3, // 리포트 생성
        SEND_NOTIFICATION = 4 // 알림 전송
    };

public:
    // 생성자
    ScheduleEntity();
    explicit ScheduleEntity(int id);
    ScheduleEntity(int tenant_id, const std::string& name, 
                   ScheduleType type, ActionType action);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "schedules"; }
    
    // Getters - 기본 정보
    int getTenantId() const { return tenant_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    
    // Getters - 스케줄 설정
    ScheduleType getScheduleType() const { return schedule_type_; }
    ActionType getActionType() const { return action_type_; }
    const nlohmann::json& getActionConfig() const { return action_config_; }
    const std::string& getCronExpression() const { return cron_expression_; }
    const std::string& getTriggerCondition() const { return trigger_condition_; }
    
    // Getters - 실행 옵션
    bool isRetryOnFailure() const { return retry_on_failure_; }
    int getMaxRetries() const { return max_retries_; }
    int getTimeoutSeconds() const { return timeout_seconds_; }
    
    // Getters - 상태
    bool isEnabled() const { return is_enabled_; }
    std::optional<std::chrono::system_clock::time_point> getLastExecutionTime() const { 
        return last_execution_time_; 
    }
    std::optional<std::chrono::system_clock::time_point> getNextExecutionTime() const { 
        return next_execution_time_; 
    }
    
    // Getters - 타임스탬프
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }
    
    // Setters - 기본 정보
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    
    // Setters - 스케줄 설정
    void setScheduleType(ScheduleType type) { schedule_type_ = type; markModified(); }
    void setActionType(ActionType type) { action_type_ = type; markModified(); }
    void setActionConfig(const nlohmann::json& config) { action_config_ = config; markModified(); }
    void setCronExpression(const std::string& cron) { cron_expression_ = cron; markModified(); }
    void setTriggerCondition(const std::string& condition) { trigger_condition_ = condition; markModified(); }
    
    // Setters - 실행 옵션
    void setRetryOnFailure(bool retry) { retry_on_failure_ = retry; markModified(); }
    void setMaxRetries(int retries) { max_retries_ = retries; markModified(); }
    void setTimeoutSeconds(int timeout) { timeout_seconds_ = timeout; markModified(); }
    
    // Setters - 상태
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setLastExecutionTime(const std::chrono::system_clock::time_point& time) { 
        last_execution_time_ = time; 
        markModified(); 
    }
    void setNextExecutionTime(const std::chrono::system_clock::time_point& time) { 
        next_execution_time_ = time; 
        markModified(); 
    }
    
    // 비즈니스 로직
    bool shouldExecuteNow() const;
    std::chrono::system_clock::time_point calculateNextExecutionTime() const;
    bool isValidCronExpression() const;
    bool execute();  // 스케줄 실행
    
    // 액션 설정 헬퍼
    void setWriteValueAction(int point_id, double value);
    void setExecuteRecipeAction(int recipe_id);
    void setRunScriptAction(const std::string& script);
    void setGenerateReportAction(int template_id);
    
    // JSON 변환
    nlohmann::json toJson() const;
    static ScheduleEntity fromJson(const nlohmann::json& j);
    
    // 헬퍼 메서드
    static std::string scheduleTypeToString(ScheduleType type);
    static ScheduleType stringToScheduleType(const std::string& str);
    static std::string actionTypeToString(ActionType type);
    static ActionType stringToActionType(const std::string& str);
    
private:
    // 기본 정보
    int tenant_id_ = 0;
    std::string name_;
    std::string description_;
    
    // 스케줄 타입
    ScheduleType schedule_type_ = ScheduleType::TIME_BASED;
    
    // 실행 대상
    ActionType action_type_ = ActionType::WRITE_VALUE;
    nlohmann::json action_config_;
    /* 예시:
    {
        "type": "write_value",
        "point_id": 123,
        "value": 100
    }
    또는
    {
        "type": "execute_recipe",
        "recipe_id": 456
    }
    */
    
    // 스케줄 설정
    std::string cron_expression_;  // time_based인 경우
    std::string trigger_condition_;  // condition_based인 경우
    
    // 실행 옵션
    bool retry_on_failure_ = true;
    int max_retries_ = 3;
    int timeout_seconds_ = 300;
    
    // 상태
    bool is_enabled_ = true;
    std::optional<std::chrono::system_clock::time_point> last_execution_time_;
    std::optional<std::chrono::system_clock::time_point> next_execution_time_;
    
    // 타임스탬프
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    bool parseCronExpression(const std::string& cron);
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SCHEDULE_ENTITY_H