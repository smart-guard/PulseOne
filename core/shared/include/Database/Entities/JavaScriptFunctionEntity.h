// =============================================================================
// collector/include/Database/Entities/JavaScriptFunctionEntity.h
// PulseOne JavaScriptFunctionEntity - JavaScript 함수 라이브러리
// =============================================================================

#ifndef JAVASCRIPT_FUNCTION_ENTITY_H
#define JAVASCRIPT_FUNCTION_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

class JavaScriptFunctionEntity : public BaseEntity<JavaScriptFunctionEntity> {
public:
    // 함수 카테고리
    enum class Category {
        MATH = 0,
        LOGIC = 1,
        ENGINEERING = 2,
        TIME = 3,
        STRING = 4,
        CUSTOM = 5
    };

public:
    // 생성자
    JavaScriptFunctionEntity();
    explicit JavaScriptFunctionEntity(int id);
    JavaScriptFunctionEntity(int tenant_id, const std::string& name, const std::string& code);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "javascript_functions"; }
    
    // Repository access
    std::shared_ptr<Repositories::DataPointRepository> getRepository() const; // Wait! JavaScriptFunction uses which repo?
    
    // Getters
    int getTenantId() const { return tenant_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    Category getCategory() const { return category_; }
    const std::string& getFunctionCode() const { return function_code_; }
    const nlohmann::json& getParameters() const { return parameters_; }
    const std::string& getReturnType() const { return return_type_; }
    bool isSystem() const { return is_system_; }
    bool isEnabled() const { return is_enabled_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    int getCreatedBy() const { return created_by_; }
    
    // Setters
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    void setCategory(Category category) { category_ = category; markModified(); }
    void setFunctionCode(const std::string& code) { function_code_ = code; markModified(); }
    void setParameters(const nlohmann::json& params) { parameters_ = params; markModified(); }
    void setReturnType(const std::string& type) { return_type_ = type; markModified(); }
    void setSystem(bool is_system) { is_system_ = is_system; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setCreatedBy(int user_id) { created_by_ = user_id; markModified(); }
    
    // 비즈니스 로직
    bool isValidJavaScript() const;
    std::string getFullFunction() const;  // 완전한 함수 코드 반환
    bool hasParameter(const std::string& param_name) const;
    
    // JSON 변환
    nlohmann::json toJson() const;
    static JavaScriptFunctionEntity fromJson(const nlohmann::json& j);
    
    // 헬퍼 메서드
    static std::string categoryToString(Category category);
    static Category stringToCategory(const std::string& str);
    
private:
    // 기본 정보
    int tenant_id_ = 0;
    std::string name_;
    std::string description_;
    Category category_ = Category::CUSTOM;
    
    // 함수 정보
    std::string function_code_;
    nlohmann::json parameters_;  // [{name: "value", type: "number", required: true}]
    std::string return_type_ = "number";
    
    // 상태
    bool is_system_ = false;
    bool is_enabled_ = true;
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;
    int created_by_ = 0;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // JAVASCRIPT_FUNCTION_ENTITY_H