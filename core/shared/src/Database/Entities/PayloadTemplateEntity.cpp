/**
 * @file PayloadTemplateEntity.cpp
 * @brief Payload Template Entity 구현
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Entities/PayloadTemplateEntity.cpp
 */

#include "Database/Entities/PayloadTemplateEntity.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

PayloadTemplateEntity::PayloadTemplateEntity()
    : BaseEntity<PayloadTemplateEntity>()
    , is_active_(true) {
}

PayloadTemplateEntity::PayloadTemplateEntity(int id)
    : PayloadTemplateEntity() {
    setId(id);
}

PayloadTemplateEntity::~PayloadTemplateEntity() {
}

// =============================================================================
// Repository 패턴
// =============================================================================

std::shared_ptr<Repositories::PayloadTemplateRepository> 
PayloadTemplateEntity::getRepository() const {
    return RepositoryFactory::getInstance().getPayloadTemplateRepository();
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

json PayloadTemplateEntity::parseTemplate() const {
    try {
        if (template_json_.empty()) {
            return json::object();
        }
        return json::parse(template_json_);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "PayloadTemplateEntity::parseTemplate failed: " + std::string(e.what()));
        return json::object();
    }
}

bool PayloadTemplateEntity::validate() const {
    if (name_.empty()) return false;
    if (system_type_.empty()) return false;
    if (template_json_.empty()) return false;
    
    // JSON 유효성 검사
    try {
        json::parse(template_json_);
    } catch (...) {
        return false;
    }
    
    return true;
}

std::string PayloadTemplateEntity::getEntityTypeName() const {
    return "PayloadTemplateEntity";
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json PayloadTemplateEntity::toJson() const {
    json j;
    
    try {
        j["id"] = getId();
        j["name"] = name_;
        j["system_type"] = system_type_;
        j["description"] = description_;
        j["template_json"] = template_json_;
        j["is_active"] = is_active_;
        j["created_at"] = timestampToString(getCreatedAt());
        j["updated_at"] = timestampToString(getUpdatedAt());
        
    } catch (const std::exception&) {
        // JSON 생성 실패 시 기본 객체 반환
    }
    
    return j;
}

bool PayloadTemplateEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) {
            setId(data["id"].get<int>());
        }
        if (data.contains("name")) {
            name_ = data["name"].get<std::string>();
        }
        if (data.contains("system_type")) {
            system_type_ = data["system_type"].get<std::string>();
        }
        if (data.contains("description")) {
            description_ = data["description"].get<std::string>();
        }
        if (data.contains("template_json")) {
            template_json_ = data["template_json"].get<std::string>();
        }
        if (data.contains("is_active")) {
            is_active_ = data["is_active"].get<bool>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string PayloadTemplateEntity::toString() const {
    std::ostringstream oss;
    oss << "PayloadTemplateEntity[";
    oss << "id=" << getId();
    oss << ", name=" << name_;
    oss << ", system_type=" << system_type_;
    oss << ", active=" << (is_active_ ? "true" : "false");
    oss << "]";
    return oss.str();
}

// =============================================================================
// DB 연산
// =============================================================================

bool PayloadTemplateEntity::loadFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto repo = getRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        markError();
        return false;
    }
}

bool PayloadTemplateEntity::saveToDatabase() {
    try {
        auto repo = getRepository();
        if (repo) {
            bool success = false;
            if (getId() <= 0) {
                success = repo->save(*this);
            } else {
                success = repo->update(*this);
            }
            
            if (success) {
                markSaved();
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        markError();
        return false;
    }
}

bool PayloadTemplateEntity::updateToDatabase() {
    return saveToDatabase();
}

bool PayloadTemplateEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto repo = getRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        markError();
        return false;
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne