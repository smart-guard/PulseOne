/**
 * @file ExportTargetMappingEntity.cpp
 * @version 2.0.1 - loadViaRepository 제거, 직접 구현
 */

#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportTargetMappingEntity::ExportTargetMappingEntity()
    : BaseEntity()
    , target_id_(0)
    , point_id_(0)
    , is_enabled_(true) {
}

ExportTargetMappingEntity::ExportTargetMappingEntity(int id)
    : BaseEntity(id)
    , target_id_(0)
    , point_id_(0)
    , is_enabled_(true) {
}

ExportTargetMappingEntity::~ExportTargetMappingEntity() {
}

// =============================================================================
// Repository 패턴 지원
// =============================================================================

std::shared_ptr<Repositories::ExportTargetMappingRepository> ExportTargetMappingEntity::getRepository() const {
    return RepositoryFactory::getInstance().getExportTargetMappingRepository();
}

// =============================================================================
// DB 연산 - 직접 구현 (loadViaRepository 사용 안 함)
// =============================================================================

bool ExportTargetMappingEntity::loadFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetMappingRepository();
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

bool ExportTargetMappingEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetMappingRepository();
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

bool ExportTargetMappingEntity::updateToDatabase() {
    return saveToDatabase();
}

bool ExportTargetMappingEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetMappingRepository();
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

// =============================================================================
// JSON 직렬화
// =============================================================================

json ExportTargetMappingEntity::toJson() const {
    json j;
    
    try {
        j["id"] = getId();
        j["target_id"] = target_id_;
        j["point_id"] = point_id_;
        j["target_field_name"] = target_field_name_;
        j["target_description"] = target_description_;
        j["conversion_config"] = conversion_config_;
        j["is_enabled"] = is_enabled_;
        j["created_at"] = timestampToString(getCreatedAt());
        j["updated_at"] = timestampToString(getUpdatedAt());
        
    } catch (const std::exception&) {
        // JSON 생성 실패 시 기본 객체 반환
    }
    
    return j;
}

bool ExportTargetMappingEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) {
            setId(data["id"].get<int>());
        }
        if (data.contains("target_id")) {
            target_id_ = data["target_id"].get<int>();
        }
        if (data.contains("point_id")) {
            point_id_ = data["point_id"].get<int>();
        }
        if (data.contains("target_field_name")) {
            target_field_name_ = data["target_field_name"].get<std::string>();
        }
        if (data.contains("target_description")) {
            target_description_ = data["target_description"].get<std::string>();
        }
        if (data.contains("conversion_config")) {
            conversion_config_ = data["conversion_config"].get<std::string>();
        }
        if (data.contains("is_enabled")) {
            is_enabled_ = data["is_enabled"].get<bool>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ExportTargetMappingEntity::toString() const {
    std::ostringstream oss;
    oss << "ExportTargetMappingEntity[";
    oss << "id=" << getId();
    oss << ", target_id=" << target_id_;
    oss << ", point_id=" << point_id_;
    oss << ", enabled=" << (is_enabled_ ? "true" : "false");
    oss << "]";
    return oss.str();
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

bool ExportTargetMappingEntity::hasConversion() const {
    return !conversion_config_.empty() && conversion_config_ != "{}";
}

bool ExportTargetMappingEntity::validate() const {
    if (target_id_ <= 0) return false;
    if (point_id_ <= 0) return false;
    if (target_field_name_.empty()) return false;
    return true;
}

std::string ExportTargetMappingEntity::getEntityTypeName() const {
    return "ExportTargetMappingEntity";
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne