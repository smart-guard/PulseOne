// =============================================================================
// collector/include/Database/Entities/RecipeEntity.h
// PulseOne RecipeEntity - SCADA 레시피 관리
// =============================================================================

#ifndef RECIPE_ENTITY_H
#define RECIPE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class RecipeEntity : public BaseEntity<RecipeEntity> {
public:
    // 생성자
    RecipeEntity();
    explicit RecipeEntity(int id);
    RecipeEntity(int tenant_id, const std::string& name, const nlohmann::json& setpoints);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "recipes"; }
    
    // Getters
    int getTenantId() const { return tenant_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getCategory() const { return category_; }
    const nlohmann::json& getSetpoints() const { return setpoints_; }
    const nlohmann::json& getValidationRules() const { return validation_rules_; }
    int getVersion() const { return version_; }
    bool isActive() const { return is_active_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }
    int getCreatedBy() const { return created_by_; }
    
    // Setters
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    void setCategory(const std::string& category) { category_ = category; markModified(); }
    void setSetpoints(const nlohmann::json& setpoints) { 
        setpoints_ = setpoints; 
        markModified(); 
    }
    void setValidationRules(const nlohmann::json& rules) { 
        validation_rules_ = rules; 
        markModified(); 
    }
    void setActive(bool active) { is_active_ = active; markModified(); }
    void incrementVersion() { version_++; markModified(); }
    void setCreatedBy(int user_id) { created_by_ = user_id; markModified(); }
    
    // 비즈니스 로직
    bool addSetpoint(int point_id, double value, const std::string& unit = "");
    bool removeSetpoint(int point_id);
    bool updateSetpoint(int point_id, double value);
    std::optional<double> getSetpointValue(int point_id) const;
    
    bool validateSetpoints() const;  // 검증 규칙에 따라 setpoint 검증
    bool applyRecipe();  // 레시피 적용 (setpoint 쓰기)
    
    // 레시피 복사
    RecipeEntity clone(const std::string& new_name) const;
    
    // JSON 변환
    nlohmann::json toJson() const;
    static RecipeEntity fromJson(const nlohmann::json& j);
    
    // Setpoint 구조
    struct Setpoint {
        int point_id;
        double value;
        std::string unit;
    };
    
    std::vector<Setpoint> getSetpointList() const;
    
private:
    // 기본 정보
    int tenant_id_ = 0;
    std::string name_;
    std::string description_;
    std::string category_;
    
    // 레시피 데이터
    nlohmann::json setpoints_;
    /* 예시:
    {
        "points": [
            {"point_id": 1, "value": 100, "unit": "℃"},
            {"point_id": 2, "value": 50, "unit": "bar"}
        ]
    }
    */
    
    nlohmann::json validation_rules_;
    /* 예시:
    {
        "rules": [
            {"type": "range", "point_id": 1, "min": 0, "max": 200},
            {"type": "dependency", "if_point": 1, "value": ">100", "then_point": 2, "must_be": ">40"}
        ]
    }
    */
    
    // 메타데이터
    int version_ = 1;
    bool is_active_ = false;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    int created_by_ = 0;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // RECIPE_ENTITY_H