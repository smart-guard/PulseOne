// =============================================================================
// collector/src/Database/Entities/ScriptLibraryEntity.cpp
// PulseOne ScriptLibraryEntity 구현 - DeviceEntity 패턴 100% 적용
// =============================================================================

/**
 * @file ScriptLibraryEntity.cpp
 * @brief PulseOne ScriptLibraryEntity 구현 - DeviceEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - entityToParams 등은 Repository로 이동
 */

#include "Database/Entities/ScriptLibraryEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ScriptLibraryRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

ScriptLibraryEntity::ScriptLibraryEntity() 
    : BaseEntity<ScriptLibraryEntity>()
    , tenant_id_(0)
    , name_("")
    , display_name_("")
    , description_("")
    , category_(Category::CUSTOM)
    , script_code_("")
    , parameters_(nlohmann::json::object())
    , return_type_(ReturnType::FLOAT)
    , tags_({})
    , example_usage_("")
    , is_system_(false)
    , is_template_(false)
    , usage_count_(0)
    , rating_(0.0)
    , version_("1.0.0")
    , author_("")
    , license_("")
    , dependencies_({}) {
}

ScriptLibraryEntity::ScriptLibraryEntity(int id) 
    : ScriptLibraryEntity() {  // 위임 생성자 사용
    setId(id);
}

ScriptLibraryEntity::ScriptLibraryEntity(int tenant_id, const std::string& name, const std::string& script_code)
    : ScriptLibraryEntity() {  // 위임 생성자 사용
    tenant_id_ = tenant_id;
    name_ = name;
    script_code_ = script_code;
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool ScriptLibraryEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::loadFromDatabase - Invalid script ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getRepository<ScriptLibraryRepository>("ScriptLibraryRepository");
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::loadFromDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        auto result = repo->findById(getId());
        if (!result.has_value()) {
            if (logger_) {
                logger_->Warn("ScriptLibraryEntity::loadFromDatabase - Script not found: " + std::to_string(getId()));
            }
            markError();
            return false;
        }
        
        // 로드된 데이터로 현재 객체 업데이트
        *this = result.value();
        markLoaded();
        
        if (logger_) {
            logger_->Debug("ScriptLibraryEntity::loadFromDatabase - Loaded script: " + name_);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::loadFromDatabase - Exception: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ScriptLibraryEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getRepository<ScriptLibraryRepository>("ScriptLibraryRepository");
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::saveToDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        bool success = repo->save(*this);
        
        if (success) {
            markSaved();
            if (logger_) {
                logger_->Debug("ScriptLibraryEntity::saveToDatabase - Saved script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::saveToDatabase - Failed to save script: " + name_);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::saveToDatabase - Exception: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ScriptLibraryEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Invalid script ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getRepository<ScriptLibraryRepository>("ScriptLibraryRepository");
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            if (logger_) {
                logger_->Info("ScriptLibraryEntity::deleteFromDatabase - Deleted script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Failed to delete script: " + name_);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Exception: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ScriptLibraryEntity::updateToDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::updateToDatabase - Invalid script ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getRepository<ScriptLibraryRepository>("ScriptLibraryRepository");
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::updateToDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();
            if (logger_) {
                logger_->Debug("ScriptLibraryEntity::updateToDatabase - Updated script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::updateToDatabase - Failed to update script: " + name_);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::updateToDatabase - Exception: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 문자열 표현 구현
// =============================================================================

std::string ScriptLibraryEntity::toString() const {
    std::stringstream ss;
    ss << "ScriptLibraryEntity["
       << "id=" << getId()
       << ", tenant_id=" << tenant_id_
       << ", name=" << name_
       << ", display_name=" << display_name_
       << ", category=" << getCategoryString()
       << ", return_type=" << getReturnTypeString()
       << ", is_system=" << (is_system_ ? "true" : "false")
       << ", is_template=" << (is_template_ ? "true" : "false")
       << ", usage_count=" << usage_count_
       << ", version=" << version_
       << "]";
    return ss.str();
}

// =============================================================================
// 유효성 검증
// =============================================================================

bool ScriptLibraryEntity::validate() const {
    if (name_.empty()) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Name is empty");
        }
        return false;
    }
    
    if (script_code_.empty()) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Script code is empty for: " + name_);
        }
        return false;
    }
    
    if (tenant_id_ < 0) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Invalid tenant_id for: " + name_);
        }
        return false;
    }
    
    if (rating_ < 0.0 || rating_ > 5.0) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Invalid rating for: " + name_);
        }
        return false;
    }
    
    return true;
}

// =============================================================================
// JSON 직렬화/역직렬화
// =============================================================================

nlohmann::json ScriptLibraryEntity::toJson() const {
    nlohmann::json j;
    
    try {
        // 기본 식별자
        j["id"] = getId();
        j["tenant_id"] = tenant_id_;
        
        // 스크립트 정보
        j["name"] = name_;
        j["display_name"] = display_name_;
        j["description"] = description_;
        j["category"] = static_cast<int>(category_);
        j["category_name"] = getCategoryString();
        j["script_code"] = script_code_;
        j["parameters"] = parameters_;
        j["return_type"] = static_cast<int>(return_type_);
        j["return_type_name"] = getReturnTypeString();
        
        // 메타데이터
        j["tags"] = tags_;
        j["example_usage"] = example_usage_;
        j["is_system"] = is_system_;
        j["is_template"] = is_template_;
        j["usage_count"] = usage_count_;
        j["rating"] = rating_;
        j["version"] = version_;
        j["author"] = author_;
        j["license"] = license_;
        j["dependencies"] = dependencies_;
        
        // 타임스탬프
        j["created_at"] = std::chrono::system_clock::to_time_t(getCreatedAt());
        j["updated_at"] = std::chrono::system_clock::to_time_t(getUpdatedAt());
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::toJson - Exception: " + std::string(e.what()));
        }
    }
    
    return j;
}

bool ScriptLibraryEntity::fromJson(const nlohmann::json& j) {
    try {
        // 기본 식별자
        if (j.contains("id")) {
            setId(j["id"].get<int>());
        }
        if (j.contains("tenant_id")) {
            tenant_id_ = j["tenant_id"].get<int>();
        }
        
        // 스크립트 정보
        if (j.contains("name")) {
            name_ = j["name"].get<std::string>();
        }
        if (j.contains("display_name")) {
            display_name_ = j["display_name"].get<std::string>();
        }
        if (j.contains("description")) {
            description_ = j["description"].get<std::string>();
        }
        if (j.contains("category")) {
            category_ = static_cast<Category>(j["category"].get<int>());
        }
        if (j.contains("script_code")) {
            script_code_ = j["script_code"].get<std::string>();
        }
        if (j.contains("parameters")) {
            parameters_ = j["parameters"];
        }
        if (j.contains("return_type")) {
            return_type_ = static_cast<ReturnType>(j["return_type"].get<int>());
        }
        
        // 메타데이터
        if (j.contains("tags")) {
            tags_ = j["tags"].get<std::vector<std::string>>();
        }
        if (j.contains("example_usage")) {
            example_usage_ = j["example_usage"].get<std::string>();
        }
        if (j.contains("is_system")) {
            is_system_ = j["is_system"].get<bool>();
        }
        if (j.contains("is_template")) {
            is_template_ = j["is_template"].get<bool>();
        }
        if (j.contains("usage_count")) {
            usage_count_ = j["usage_count"].get<int>();
        }
        if (j.contains("rating")) {
            rating_ = j["rating"].get<double>();
        }
        if (j.contains("version")) {
            version_ = j["version"].get<std::string>();
        }
        if (j.contains("author")) {
            author_ = j["author"].get<std::string>();
        }
        if (j.contains("license")) {
            license_ = j["license"].get<std::string>();
        }
        if (j.contains("dependencies")) {
            dependencies_ = j["dependencies"].get<std::vector<std::string>>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::fromJson - Exception: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 유틸리티 메서드
// =============================================================================

std::string ScriptLibraryEntity::getCategoryString() const {
    switch (category_) {
        case Category::FUNCTION:
            return "FUNCTION";
        case Category::FORMULA:
            return "FORMULA";
        case Category::TEMPLATE:
            return "TEMPLATE";
        case Category::CUSTOM:
        default:
            return "CUSTOM";
    }
}

std::string ScriptLibraryEntity::getReturnTypeString() const {
    switch (return_type_) {
        case ReturnType::FLOAT:
            return "FLOAT";
        case ReturnType::STRING:
            return "STRING";
        case ReturnType::BOOLEAN:
            return "BOOLEAN";
        case ReturnType::OBJECT:
            return "OBJECT";
        default:
            return "UNKNOWN";
    }
}

bool ScriptLibraryEntity::hasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

void ScriptLibraryEntity::addTag(const std::string& tag) {
    if (!hasTag(tag)) {
        tags_.push_back(tag);
        markModified();
    }
}

void ScriptLibraryEntity::removeTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it != tags_.end()) {
        tags_.erase(it);
        markModified();
    }
}

void ScriptLibraryEntity::incrementUsageCount() {
    usage_count_++;
    markModified();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne