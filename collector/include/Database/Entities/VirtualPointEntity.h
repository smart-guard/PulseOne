// =============================================================================
// collector/include/Database/Entities/VirtualPointEntity.h
// PulseOne VirtualPointEntity - BaseEntity 패턴 준수 (컴파일 에러 수정)
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
    // =======================================================================
    // 생성자
    // =======================================================================
    VirtualPointEntity();
    explicit VirtualPointEntity(int id);
    VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula);
    
    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (모두 구현 필수!)
    // =======================================================================
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;  // 🔥 추가 필요
    bool validate() const;  // 🔥 override 제거 (BaseEntity에 없음)
    std::string getTableName() const override { return "virtual_points"; }
    
    // 🔥 BaseEntity에서 요구하는 추가 순수 가상 함수들
    bool fromJson(const json& data) override;  // 🔥 static이 아닌 virtual
    json toJson() const override;
    std::string toString() const override;  // 🔥 추가 필요
    
    // =======================================================================
    // Getters
    // =======================================================================
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
    const std::string& getCalculationTrigger() const { return calculation_trigger_; }
    ExecutionType getExecutionType() const { return execution_type_; }
    ErrorHandling getErrorHandling() const { return error_handling_; }
    const std::string& getInputMappings() const { return input_mappings_; }
    const std::string& getDependencies() const { return dependencies_; }
    int getCacheDurationMs() const { return cache_duration_ms_; }
    bool getIsEnabled() const { return is_enabled_; }
    const std::string& getCategory() const { return category_; }
    const std::string& getTags() const { return tags_; }
    int getExecutionCount() const { return execution_count_; }
    double getLastValue() const { return last_value_; }
    const std::string& getLastError() const { return last_error_; }
    double getAvgExecutionTimeMs() const { return avg_execution_time_ms_; }
    const std::string& getCreatedBy() const { return created_by_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setScopeType(const std::string& scope_type) { scope_type_ = scope_type; markModified(); }
    void setSiteId(std::optional<int> site_id) { site_id_ = site_id; markModified(); }
    void setDeviceId(std::optional<int> device_id) { device_id_ = device_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setFormula(const std::string& formula) { formula_ = formula; markModified(); }
    void setDataType(const std::string& data_type) { data_type_ = data_type; markModified(); }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    void setCalculationInterval(int interval) { calculation_interval_ = interval; markModified(); }
    void setCalculationTrigger(const std::string& trigger) { calculation_trigger_ = trigger; markModified(); }
    void setExecutionType(ExecutionType type) { execution_type_ = type; markModified(); }
    void setErrorHandling(ErrorHandling handling) { error_handling_ = handling; markModified(); }
    void setInputMappings(const std::string& mappings) { input_mappings_ = mappings; markModified(); }
    void setDependencies(const std::string& deps) { dependencies_ = deps; markModified(); }
    void setCacheDurationMs(int duration) { cache_duration_ms_ = duration; markModified(); }
    void setIsEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setCategory(const std::string& category) { category_ = category; markModified(); }
    void setTags(const std::string& tags) { tags_ = tags; markModified(); }
    void setExecutionCount(int count) { execution_count_ = count; markModified(); }
    void setLastValue(double value) { last_value_ = value; markModified(); }
    void setLastError(const std::string& error) { last_error_ = error; markModified(); }
    void setAvgExecutionTimeMs(double time) { avg_execution_time_ms_ = time; markModified(); }
    void setCreatedBy(const std::string& created_by) { created_by_ = created_by; markModified(); }
    
    // =======================================================================
    // 헬퍼 메서드
    // =======================================================================
    std::vector<std::string> getTagList() const;
    nlohmann::json getParametersJson() const;
    bool hasTag(const std::string& tag) const;
    
private:
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    int tenant_id_ = 0;
    std::string scope_type_ = "site";
    std::optional<int> site_id_;
    std::optional<int> device_id_;
    std::string name_;
    std::string description_;
    std::string formula_;
    std::string data_type_ = "float";
    std::string unit_;
    int calculation_interval_ = 1;  // seconds
    std::string calculation_trigger_ = "timer";
    ExecutionType execution_type_ = ExecutionType::JAVASCRIPT;
    ErrorHandling error_handling_ = ErrorHandling::RETURN_NULL;
    std::string input_mappings_;  // JSON string
    std::string dependencies_;    // JSON string
    int cache_duration_ms_ = 0;
    bool is_enabled_ = true;
    std::string category_;
    std::string tags_;  // JSON array string
    int execution_count_ = 0;
    double last_value_ = 0.0;
    std::string last_error_;
    double avg_execution_time_ms_ = 0.0;
    std::string created_by_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H