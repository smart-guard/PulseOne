// =============================================================================
// collector/include/Database/Entities/VirtualPointEntity.h
// PulseOne VirtualPointEntity - 기존 패턴 100% 준수
// =============================================================================

#ifndef VIRTUAL_POINT_ENTITY_H
#define VIRTUAL_POINT_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include "Common/Enums.h"
#include <string>
#include <chrono>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class VirtualPointEntity : public BaseEntity<VirtualPointEntity> {
public:
    // 실행 타입
    enum class ExecutionType {
        JAVASCRIPT = 0,
        FORMULA = 1,
        AGGREGATE = 2,
        REFERENCE = 3
    };
    
    // 계산 트리거
    enum class CalculationTrigger {
        TIMER = 0,
        ON_CHANGE = 1,
        ON_DEMAND = 2,
        EVENT = 3
    };
    
    // 에러 처리
    enum class ErrorHandling {
        RETURN_NULL = 0,
        RETURN_LAST = 1,
        RETURN_ZERO = 2,
        RETURN_DEFAULT = 3
    };

public:
    // 생성자
    VirtualPointEntity();
    explicit VirtualPointEntity(int id);
    VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "virtual_points"; }
    
    // Getters
    int getTenantId() const { return tenant_id_; }
    const std::string& getScopeType() const { return scope_type_; }
    std::optional<int> getSiteId() const { return site_id_; }
    std::optional<int> getDeviceId() const { return device_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getFormula() const { return formula_; }
    const std::string& getDataType() const { return data_type_; }
    const std::string& getUnit() const { return unit_; }
    
    int getCalculationInterval() const { return calculation_interval_; }
    ExecutionType getExecutionType() const { return execution_type_; }
    CalculationTrigger getCalculationTrigger() const { return calculation_trigger_; }
    ErrorHandling getErrorHandling() const { return error_handling_; }
    
    const nlohmann::json& getDependencies() const { return dependencies_; }
    int getCacheDuration() const { return cache_duration_ms_; }
    bool isEnabled() const { return is_enabled_; }
    
    const std::string& getCategory() const { return category_; }
    const nlohmann::json& getTags() const { return tags_; }
    
    // 실행 정보
    const std::string& getLastError() const { return last_error_; }
    int getExecutionCount() const { return execution_count_; }
    double getAvgExecutionTime() const { return avg_execution_time_ms_; }
    std::chrono::system_clock::time_point getLastExecutionTime() const { return last_execution_time_; }
    
    // Setters
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setScopeType(const std::string& type) { scope_type_ = type; markModified(); }
    void setSiteId(std::optional<int> site_id) { site_id_ = site_id; markModified(); }
    void setDeviceId(std::optional<int> device_id) { device_id_ = device_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    void setFormula(const std::string& formula) { formula_ = formula; markModified(); }
    void setDataType(const std::string& type) { data_type_ = type; markModified(); }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    
    void setCalculationInterval(int interval) { calculation_interval_ = interval; markModified(); }
    void setExecutionType(ExecutionType type) { execution_type_ = type; markModified(); }
    void setCalculationTrigger(CalculationTrigger trigger) { calculation_trigger_ = trigger; markModified(); }
    void setErrorHandling(ErrorHandling handling) { error_handling_ = handling; markModified(); }
    
    void setDependencies(const nlohmann::json& deps) { dependencies_ = deps; markModified(); }
    void setCacheDuration(int duration) { cache_duration_ms_ = duration; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    
    void setCategory(const std::string& category) { category_ = category; markModified(); }
    void setTags(const nlohmann::json& tags) { tags_ = tags; markModified(); }
    
    // 실행 정보 업데이트
    void updateExecutionInfo(bool success, double execution_time_ms, const std::string& error = "");
    
    // 비즈니스 로직
    bool hasCircularDependency() const;
    std::vector<int> getDependentPointIds() const;
    bool dependsOn(int point_id, const std::string& point_type) const;
    
    // JSON 변환
    nlohmann::json toJson() const;
    static VirtualPointEntity fromJson(const nlohmann::json& j);
    
    // 헬퍼 메서드
    static std::string executionTypeToString(ExecutionType type);
    static ExecutionType stringToExecutionType(const std::string& str);
    static std::string calculationTriggerToString(CalculationTrigger trigger);
    static CalculationTrigger stringToCalculationTrigger(const std::string& str);
    static std::string errorHandlingToString(ErrorHandling handling);
    static ErrorHandling stringToErrorHandling(const std::string& str);
    
private:
    // 기본 정보
    int tenant_id_ = 0;
    std::string scope_type_ = "tenant";  // tenant, site, device
    std::optional<int> site_id_;
    std::optional<int> device_id_;
    
    std::string name_;
    std::string description_;
    std::string formula_;
    std::string data_type_ = "float";
    std::string unit_;
    
    // 계산 설정
    int calculation_interval_ = 1000;  // ms
    ExecutionType execution_type_ = ExecutionType::JAVASCRIPT;
    CalculationTrigger calculation_trigger_ = CalculationTrigger::TIMER;
    ErrorHandling error_handling_ = ErrorHandling::RETURN_NULL;
    
    // 의존성 및 캐싱
    nlohmann::json dependencies_;  // 의존 포인트 목록
    int cache_duration_ms_ = 0;
    
    // 상태
    bool is_enabled_ = true;
    
    // 메타데이터
    std::string category_;
    nlohmann::json tags_;
    
    // 실행 정보
    std::string last_error_;
    int execution_count_ = 0;
    double avg_execution_time_ms_ = 0.0;
    std::chrono::system_clock::time_point last_execution_time_;
    
    // 타임스탬프
    int created_by_ = 0;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H