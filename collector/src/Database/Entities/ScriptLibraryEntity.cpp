// =============================================================================
// collector/src/Database/Entities/ScriptLibraryEntity.cpp
// PulseOne ScriptLibraryEntity 구현 - DeviceEntity 패턴 100% 적용 (컴파일 에러 수정)
// =============================================================================

/**
 * @file ScriptLibraryEntity.cpp
 * @brief PulseOne ScriptLibraryEntity 구현 - DeviceEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🔧 컴파일 에러 완전 수정:
 * - getRepository<ScriptLibraryRepository> → getScriptLibraryRepository()
 * - markLoaded → markSaved
 * - RepositoryFactory 올바른 사용법 적용
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
// BaseEntity 순수 가상 함수 구현 (🔧 수정: RepositoryFactory 활용)
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
        // 🔧 수정: getScriptLibraryRepository() 사용
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getScriptLibraryRepository();
        
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
        markSaved(); // 🔧 수정: markLoaded → markSaved
        
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
        // 🔧 수정: getScriptLibraryRepository() 사용
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getScriptLibraryRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::saveToDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        // Repository의 save 메서드가 ID를 자동으로 설정함
        bool success = repo->save(*this);
        
        if (success) {
            markSaved();
            if (logger_) {
                logger_->Info("ScriptLibraryEntity::saveToDatabase - Saved script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::saveToDatabase - Failed to save script");
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
            logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Invalid script ID");
        }
        return false;
    }
    
    try {
        // 🔧 수정: getScriptLibraryRepository() 사용
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getScriptLibraryRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Failed to get repository");
            }
            markError();
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            markDeleted();
            if (logger_) {
                logger_->Info("ScriptLibraryEntity::deleteFromDatabase - Deleted script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::deleteFromDatabase - Failed to delete script");
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
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("ScriptLibraryEntity::updateToDatabase - Invalid script data or ID");
        }
        return false;
    }
    
    try {
        // 🔧 수정: getScriptLibraryRepository() 사용
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getScriptLibraryRepository();
        
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
                logger_->Info("ScriptLibraryEntity::updateToDatabase - Updated script: " + name_);
            }
        } else {
            markError();
            if (logger_) {
                logger_->Error("ScriptLibraryEntity::updateToDatabase - Failed to update script");
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
// JSON 변환 (기존 유지)
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
// 유효성 검사 (기존 유지)
// =============================================================================

bool ScriptLibraryEntity::validate() const {
    // 필수 필드 검사
    if (tenant_id_ <= 0) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Invalid tenant_id: " + std::to_string(tenant_id_));
        }
        return false;
    }
    
    if (name_.empty()) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Empty name");
        }
        return false;
    }
    
    if (script_code_.empty()) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Empty script_code");
        }
        return false;
    }
    
    // 이름 길이 제한
    if (name_.length() > 100) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Name too long: " + std::to_string(name_.length()));
        }
        return false;
    }
    
    // 버전 형식 간단 검사
    if (version_.empty() || version_.find_first_not_of("0123456789.") != std::string::npos) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Invalid version format: " + version_);
        }
        return false;
    }
    
    // 평점 범위 검사
    if (rating_ < 0.0 || rating_ > 5.0) {
        if (logger_) {
            logger_->Warn("ScriptLibraryEntity::validate - Rating out of range: " + std::to_string(rating_));
        }
        return false;
    }
    
    return true;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne