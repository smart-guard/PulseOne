// =============================================================================
// collector/include/Database/Entities/ScriptLibraryEntity.h - override 수정
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
    // 🔥 BaseEntity 순수 가상 함수 구현 - override 키워드 제거
    // =======================================================================
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;  // 🔥 구현 필요
    std::string toString() const override;  // 🔥 구현 필요
    
    // 🔥 validate 메서드 - override 제거 (BaseEntity에서 순수가상함수가 아님)
    bool validate() const;  // override 제거
    
    // Repository access
    std::shared_ptr<Repositories::ScriptLibraryRepository> getRepository() const;
    
    std::string getTableName() const override { return "script_library"; }
    
    // =======================================================================
    // JSON 직렬화/역직렬화
    // =======================================================================
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& data) override;
    
    // =======================================================================
    // Getters
    // =======================================================================
    int getTenantId() const { return tenant_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDisplayName() const { return display_name_; }
    const std::string& getDescription() const { return description_; }
    Category getCategory() const { return category_; }
    const std::string& getScriptCode() const { return script_code_; }
    const nlohmann::json& getParameters() const { return parameters_; }
    ReturnType getReturnType() const { return return_type_; }
    const std::vector<std::string>& getTags() const { return tags_; }
    const std::string& getExampleUsage() const { return example_usage_; }
    bool isSystem() const { return is_system_; }
    bool isTemplate() const { return is_template_; }
    int getUsageCount() const { return usage_count_; }
    double getRating() const { return rating_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getAuthor() const { return author_; }
    const std::string& getLicense() const { return license_; }
    const std::vector<std::string>& getDependencies() const { return dependencies_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified();
    }
    
    void setName(const std::string& name) { 
        name_ = name; 
        markModified();
    }
    
    void setDisplayName(const std::string& display_name) { 
        display_name_ = display_name; 
        markModified();
    }
    
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified();
    }
    
    void setCategory(Category category) { 
        category_ = category; 
        markModified();
    }
    
    void setScriptCode(const std::string& script_code) { 
        script_code_ = script_code; 
        markModified();
    }
    
    void setParameters(const nlohmann::json& parameters) { 
        parameters_ = parameters; 
        markModified();
    }
    
    void setReturnType(ReturnType return_type) { 
        return_type_ = return_type; 
        markModified();
    }
    
    void setTags(const std::vector<std::string>& tags) { 
        tags_ = tags; 
        markModified();
    }
    
    void setExampleUsage(const std::string& example_usage) { 
        example_usage_ = example_usage; 
        markModified();
    }
    
    void setIsSystem(bool is_system) { 
        is_system_ = is_system; 
        markModified();
    }
    
    void setIsTemplate(bool is_template) { 
        is_template_ = is_template; 
        markModified();
    }
    
    void setUsageCount(int usage_count) { 
        usage_count_ = usage_count; 
        markModified();
    }
    
    void setRating(double rating) { 
        rating_ = rating; 
        markModified();
    }
    
    void setVersion(const std::string& version) { 
        version_ = version; 
        markModified();
    }
    
    void setAuthor(const std::string& author) { 
        author_ = author; 
        markModified();
    }
    
    void setLicense(const std::string& license) { 
        license_ = license; 
        markModified();
    }
    
    void setDependencies(const std::vector<std::string>& dependencies) { 
        dependencies_ = dependencies; 
        markModified();
    }

    // =======================================================================
    // 유틸리티 메서드
    // =======================================================================
    std::string getCategoryString() const;
    std::string getReturnTypeString() const;
    bool hasTag(const std::string& tag) const;
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    void incrementUsageCount();

private:
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    int tenant_id_;
    std::string name_;
    std::string display_name_;
    std::string description_;
    Category category_;
    std::string script_code_;
    nlohmann::json parameters_;
    ReturnType return_type_;
    std::vector<std::string> tags_;
    std::string example_usage_;
    bool is_system_;
    bool is_template_;
    int usage_count_;
    double rating_;
    std::string version_;
    std::string author_;
    std::string license_;
    std::vector<std::string> dependencies_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_ENTITY_H