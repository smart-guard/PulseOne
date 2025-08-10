// =============================================================================
// collector/include/Database/Entities/VirtualPointEntity.h
// PulseOne VirtualPointEntity - BaseEntity 패턴 적용
// =============================================================================

#ifndef VIRTUAL_POINT_ENTITY_H
#define VIRTUAL_POINT_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;

/**
 * @brief VirtualPointEntity - 가상포인트 엔티티
 * 
 * 🎯 BaseEntity 패턴 적용:
 * - 단순한 데이터 구조체 역할
 * - DB 작업은 VirtualPointRepository에서 처리
 * - BaseEntity<VirtualPointEntity> 상속으로 기본 기능 자동 획득
 */
class VirtualPointEntity : public BaseEntity<VirtualPointEntity> {
public:
    // =======================================================================
    // Enum 정의 (DB 스키마와 일치)
    // =======================================================================
    
    enum class ExecutionType {
        JAVASCRIPT,
        FORMULA,
        AGGREGATE,
        REFERENCE
    };
    
    enum class ErrorHandling {
        RETURN_NULL,
        RETURN_LAST,
        RETURN_ZERO,
        RETURN_DEFAULT
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    VirtualPointEntity() = default;
    explicit VirtualPointEntity(int id) : BaseEntity(id) {}
    VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula);
    virtual ~VirtualPointEntity() = default;

    // =======================================================================
    // Getter/Setter - DB 스키마 필드들
    // =======================================================================
    
    // 필수 필드
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int id) { tenant_id_ = id; markModified(); }
    
    const std::string& getScopeType() const { return scope_type_; }
    void setScopeType(const std::string& type) { scope_type_ = type; markModified(); }
    
    const std::optional<int>& getSiteId() const { return site_id_; }
    void setSiteId(const std::optional<int>& id) { site_id_ = id; markModified(); }
    
    const std::optional<int>& getDeviceId() const { return device_id_; }
    void setDeviceId(const std::optional<int>& id) { device_id_ = id; markModified(); }
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; markModified(); }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    
    const std::string& getFormula() const { return formula_; }
    void setFormula(const std::string& formula) { formula_ = formula; markModified(); }
    
    const std::string& getDataType() const { return data_type_; }
    void setDataType(const std::string& type) { data_type_ = type; markModified(); }
    
    const std::string& getUnit() const { return unit_; }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    
    int getCalculationInterval() const { return calculation_interval_; }
    void setCalculationInterval(int interval) { calculation_interval_ = interval; markModified(); }
    
    const std::string& getCalculationTrigger() const { return calculation_trigger_; }
    void setCalculationTrigger(const std::string& trigger) { calculation_trigger_ = trigger; markModified(); }
    
    ExecutionType getExecutionType() const { return execution_type_; }
    void setExecutionType(ExecutionType type) { execution_type_ = type; markModified(); }
    
    ErrorHandling getErrorHandling() const { return error_handling_; }
    void setErrorHandling(ErrorHandling handling) { error_handling_ = handling; markModified(); }
    
    const std::string& getInputMappings() const { return input_mappings_; }
    void setInputMappings(const std::string& mappings) { input_mappings_ = mappings; markModified(); }
    
    const std::string& getDependencies() const { return dependencies_; }
    void setDependencies(const std::string& deps) { dependencies_ = deps; markModified(); }
    
    int getCacheDurationMs() const { return cache_duration_ms_; }
    void setCacheDurationMs(int duration) { cache_duration_ms_ = duration; markModified(); }
    
    bool getIsEnabled() const { return is_enabled_; }
    void setIsEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    
    const std::string& getCategory() const { return category_; }
    void setCategory(const std::string& cat) { category_ = cat; markModified(); }
    
    const std::string& getTags() const { return tags_; }
    void setTags(const std::string& tags) { tags_ = tags; markModified(); }
    
    // 실행 통계
    int getExecutionCount() const { return execution_count_; }
    void setExecutionCount(int count) { execution_count_ = count; markModified(); }
    
    double getLastValue() const { return last_value_; }
    void setLastValue(double value) { last_value_ = value; markModified(); }
    
    const std::string& getLastError() const { return last_error_; }
    void setLastError(const std::string& error) { last_error_ = error; markModified(); }
    
    double getAvgExecutionTimeMs() const { return avg_execution_time_ms_; }
    void setAvgExecutionTimeMs(double time) { avg_execution_time_ms_ = time; markModified(); }
    
    const std::string& getCreatedBy() const { return created_by_; }
    void setCreatedBy(const std::string& user) { created_by_ = user; markModified(); }

    // =======================================================================
    // JSON 변환
    // =======================================================================
    
    bool fromJson(const json& j);
    json toJson() const;
    
    // =======================================================================
    // 헬퍼 메서드
    // =======================================================================
    
    std::vector<std::string> getTagList() const;
    bool hasTag(const std::string& tag) const;
    bool validate() const;
    std::string toString() const;

private:
    // =======================================================================
    // 멤버 변수 - DB 스키마와 일치
    // =======================================================================
    
    int tenant_id_ = 0;
    std::string scope_type_ = "tenant";
    std::optional<int> site_id_;
    std::optional<int> device_id_;
    
    std::string name_;
    std::string description_;
    std::string formula_;
    std::string data_type_ = "float";
    std::string unit_;
    
    int calculation_interval_ = 1000;
    std::string calculation_trigger_ = "timer";
    ExecutionType execution_type_ = ExecutionType::JAVASCRIPT;
    ErrorHandling error_handling_ = ErrorHandling::RETURN_NULL;
    
    std::string input_mappings_ = "[]";  // JSON string
    std::string dependencies_ = "[]";     // JSON string
    int cache_duration_ms_ = 0;
    bool is_enabled_ = true;
    
    std::string category_;
    std::string tags_ = "[]";  // JSON string
    
    // 실행 통계
    int execution_count_ = 0;
    double last_value_ = 0.0;
    std::string last_error_;
    double avg_execution_time_ms_ = 0.0;
    
    std::string created_by_;
    
    // =======================================================================
    // 헬퍼 메서드
    // =======================================================================
    
    std::string executionTypeToString(ExecutionType type) const;
    std::string errorHandlingToString(ErrorHandling handling) const;
    ExecutionType stringToExecutionType(const std::string& str) const;
    ErrorHandling stringToErrorHandling(const std::string& str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H