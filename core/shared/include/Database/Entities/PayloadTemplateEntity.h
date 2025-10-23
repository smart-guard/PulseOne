/**
 * @file PayloadTemplateEntity.h
 * @brief Payload Template Entity
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/Entities/PayloadTemplateEntity.h
 */

#ifndef PAYLOAD_TEMPLATE_ENTITY_H
#define PAYLOAD_TEMPLATE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Repositories {
    class PayloadTemplateRepository;
}
namespace Entities {

using json = nlohmann::json;

class PayloadTemplateEntity : public BaseEntity<PayloadTemplateEntity> {
public:
    PayloadTemplateEntity();
    explicit PayloadTemplateEntity(int id);
    virtual ~PayloadTemplateEntity();
    
    // Repository 패턴
    std::shared_ptr<Repositories::PayloadTemplateRepository> getRepository() const;
    
    // Getters (inline)
    std::string getName() const { return name_; }
    std::string getSystemType() const { return system_type_; }
    std::string getDescription() const { return description_; }
    std::string getTemplateJson() const { return template_json_; }
    bool isActive() const { return is_active_; }
    
    // Setters (inline)
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setSystemType(const std::string& system_type) { system_type_ = system_type; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setTemplateJson(const std::string& template_json) { template_json_ = template_json; markModified(); }
    void setActive(bool is_active) { is_active_ = is_active; markModified(); }
    
    // 비즈니스 로직
    json parseTemplate() const;
    bool validate() const;
    std::string getEntityTypeName() const;
    
    // BaseEntity 순수 가상 함수 구현
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    // DB 연산
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    
    std::string getTableName() const override { return "payload_templates"; }
    
private:
    std::string name_;
    std::string system_type_;
    std::string description_;
    std::string template_json_;
    bool is_active_ = true;
};

}
}
}

#endif // PAYLOAD_TEMPLATE_ENTITY_H