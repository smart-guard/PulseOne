/**
 * @file ExportTargetMappingEntity.cpp
 * @version 2.0.0
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
// DB 연산
// =============================================================================

bool ExportTargetMappingEntity::loadFromDatabase() {
    return loadViaRepository();
}

bool ExportTargetMappingEntity::saveToDatabase() {
    return saveViaRepository();
}

bool ExportTargetMappingEntity::updateToDatabase() {
    return updateViaRepository();
}

bool ExportTargetMappingEntity::deleteFromDatabase() {
    return deleteViaRepository();
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json ExportTargetMappingEntity::toJson() const {
    json j;
    
    j["id"] = id_;
    j["target_id"] = target_id_;
    j["point_id"] = point_id_;
    j["target_field_name"] = target_field_name_;
    j["target_description"] = target_description_;
    j["conversion_config"] = conversion_config_;
    j["is_enabled"] = is_enabled_;
    
    return j;
}

bool ExportTargetMappingEntity::fromJson(const json& data) {
    try {
        if (data.contains("id") && data["id"].is_number()) {
            id_ = data["id"].get<int>();
        }
        
        if (data.contains("target_id") && data["target_id"].is_number()) {
            target_id_ = data["target_id"].get<int>();
        }
        
        if (data.contains("point_id") && data["point_id"].is_number()) {
            point_id_ = data["point_id"].get<int>();
        }
        
        if (data.contains("target_field_name") && data["target_field_name"].is_string()) {
            target_field_name_ = data["target_field_name"].get<std::string>();
        }
        
        if (data.contains("target_description") && data["target_description"].is_string()) {
            target_description_ = data["target_description"].get<std::string>();
        }
        
        if (data.contains("conversion_config") && data["conversion_config"].is_string()) {
            conversion_config_ = data["conversion_config"].get<std::string>();
        }
        
        if (data.contains("is_enabled") && data["is_enabled"].is_boolean()) {
            is_enabled_ = data["is_enabled"].get<bool>();
        }
        
        markModified();
        return true;
        
    } catch (const json::exception& e) {
        LogManager::getInstance().Error("ExportTargetMappingEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string ExportTargetMappingEntity::toString() const {
    return toJson().dump(2);
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

bool ExportTargetMappingEntity::hasConversion() const {
    return !conversion_config_.empty() && conversion_config_ != "{}";
}

bool ExportTargetMappingEntity::validate() const {
    if (target_id_ <= 0) {
        LogManager::getInstance().Warn("ExportTargetMappingEntity::validate - target_id is invalid");
        return false;
    }
    
    if (point_id_ <= 0) {
        LogManager::getInstance().Warn("ExportTargetMappingEntity::validate - point_id is invalid");
        return false;
    }
    
    return true;
}

std::string ExportTargetMappingEntity::getEntityTypeName() const {
    return "ExportTargetMapping";
}

}
}
}