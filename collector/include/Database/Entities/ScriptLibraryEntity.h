// =============================================================================
// collector/include/Database/Entities/ScriptLibraryEntity.h
// PulseOne ScriptLibraryEntity - BaseEntity 패턴 준수
// =============================================================================

#ifndef SCRIPT_LIBRARY_ENTITY_H
#define SCRIPT_LIBRARY_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 스크립트 라이브러리 엔티티 클래스
 */
class ScriptLibraryEntity : public BaseEntity<ScriptLibraryEntity> {
public:
    // 카테고리 열거형
    enum class Category {
        FUNCTION = 0,
        FORMULA = 1,
        TEMPLATE = 2,
        CUSTOM = 3
    };
    
    // 반환 타입 열거형
    enum class ReturnType {
        FLOAT = 0,
        STRING = 1,
        BOOLEAN = 2,
        OBJECT = 3
    };

public:
    // =======================================================================
    // 생성자
    // =======================================================================
    ScriptLibraryEntity();
    explicit ScriptLibraryEntity(int id);
    ScriptLibraryEntity(int tenant_id, const std::string& name, const std::string& script_code);
    
    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "script_library"; }
    
    // =======================================================================
    // Getters
    // =======================================================================
    int getTenantId() const { return tenant_id_; }
    const std::string& getCategory() const { return category_; }
    const std::string& getName() const { return name_; }
    const std::string& getDisplayName() const { return display_name_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getScriptCode() const { return script_code_; }
    const std::string& getParameters() const { return parameters_; }
    const std::string& getReturnType() const { return return_type_; }
    const std::string& getTags() const { return tags_; }
    const std::string& getExampleUsage() const { return example_usage_; }
    bool getIsSystem() const { return is_system_; }
    bool getIsPublic() const { return is_public_; }
    const std::string& getCreatedBy() const { return created_by_; }
    int getUsageCount() const { return usage_count_; }
    double getRating() const { return rating_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setCategory(const std::string& category) { category_ = category; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDisplayName(const std::string& display_name) { display_name_ = display_name; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setScriptCode(const std::string& script_code) { script_code_ = script_code; markModified(); }
    void setParameters(const std::string& parameters) { parameters_ = parameters; markModified(); }
    void setReturnType(const std::string& return_type) { return_type_ = return_type; markModified(); }
    void setTags(const std::string& tags) { tags_ = tags; markModified(); }
    void setExampleUsage(const std::string& example_usage) { example_usage_ = example_usage; markModified(); }
    void setIsSystem(bool is_system) { is_system_ = is_system; markModified(); }
    void setIsPublic(bool is_public) { is_public_ = is_public; markModified(); }
    void setCreatedBy(const std::string& created_by) { created_by_ = created_by; markModified(); }
    void incrementUsageCount() { usage_count_++; markModified(); }
    void setRating(double rating) { rating_ = rating; markModified(); }
    
    // =======================================================================
    // JSON 변환
    // =======================================================================
    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& j);
    
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
    std::string category_ = "function";
    std::string name_;
    std::string display_name_;
    std::string description_;
    std::string script_code_;
    std::string parameters_;      // JSON string
    std::string return_type_ = "float";
    std::string tags_;            // JSON array string
    std::string example_usage_;
    bool is_system_ = false;
    bool is_public_ = true;
    std::string created_by_;
    int usage_count_ = 0;
    double rating_ = 0.0;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_ENTITY_H