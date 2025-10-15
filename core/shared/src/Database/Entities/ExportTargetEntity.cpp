/**
 * @file ExportTargetEntity.cpp
 * @version 2.0.1 - loadViaRepository 제거, 직접 구현
 */

#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Repositories/ExportTargetRepository.h"
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

ExportTargetEntity::ExportTargetEntity()
    : BaseEntity()
    , profile_id_(0)
    , is_enabled_(true)
    , export_mode_("on_change")
    , export_interval_(0)
    , batch_size_(100)
    , total_exports_(0)
    , successful_exports_(0)
    , failed_exports_(0)
    , avg_export_time_ms_(0) {
}

ExportTargetEntity::ExportTargetEntity(int id)
    : BaseEntity(id)
    , profile_id_(0)
    , is_enabled_(true)
    , export_mode_("on_change")
    , export_interval_(0)
    , batch_size_(100)
    , total_exports_(0)
    , successful_exports_(0)
    , failed_exports_(0)
    , avg_export_time_ms_(0) {
}

ExportTargetEntity::~ExportTargetEntity() {
}

// =============================================================================
// Repository 패턴 지원
// =============================================================================

std::shared_ptr<Repositories::ExportTargetRepository> ExportTargetEntity::getRepository() const {
    return RepositoryFactory::getInstance().getExportTargetRepository();
}

// =============================================================================
// DB 연산 - 직접 구현 (loadViaRepository 사용 안 함)
// =============================================================================

bool ExportTargetEntity::loadFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetRepository();
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

bool ExportTargetEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetRepository();
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

bool ExportTargetEntity::updateToDatabase() {
    return saveToDatabase();
}

bool ExportTargetEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportTargetRepository();
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

json ExportTargetEntity::toJson() const {
    json j;
    
    try {
        j["id"] = getId();
        j["profile_id"] = profile_id_;
        j["name"] = name_;
        j["target_type"] = target_type_;
        j["description"] = description_;
        j["is_enabled"] = is_enabled_;
        j["config"] = config_;
        j["export_mode"] = export_mode_;
        j["export_interval"] = export_interval_;
        j["batch_size"] = batch_size_;
        j["total_exports"] = total_exports_;
        j["successful_exports"] = successful_exports_;
        j["failed_exports"] = failed_exports_;
        
        if (last_export_at_.has_value()) {
            j["last_export_at"] = timestampToString(last_export_at_.value());
        }
        
        j["avg_export_time_ms"] = avg_export_time_ms_;
        j["created_at"] = timestampToString(getCreatedAt());
        j["updated_at"] = timestampToString(getUpdatedAt());
        
    } catch (const std::exception&) {
        // JSON 생성 실패 시 기본 객체 반환
    }
    
    return j;
}

bool ExportTargetEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) {
            setId(data["id"].get<int>());
        }
        if (data.contains("profile_id")) {
            profile_id_ = data["profile_id"].get<int>();
        }
        if (data.contains("name")) {
            name_ = data["name"].get<std::string>();
        }
        if (data.contains("target_type")) {
            target_type_ = data["target_type"].get<std::string>();
        }
        if (data.contains("description")) {
            description_ = data["description"].get<std::string>();
        }
        if (data.contains("is_enabled")) {
            is_enabled_ = data["is_enabled"].get<bool>();
        }
        if (data.contains("config")) {
            config_ = data["config"].get<std::string>();
        }
        if (data.contains("export_mode")) {
            export_mode_ = data["export_mode"].get<std::string>();
        }
        if (data.contains("export_interval")) {
            export_interval_ = data["export_interval"].get<int>();
        }
        if (data.contains("batch_size")) {
            batch_size_ = data["batch_size"].get<int>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ExportTargetEntity::toString() const {
    std::ostringstream oss;
    oss << "ExportTargetEntity[";
    oss << "id=" << getId();
    oss << ", name=" << name_;
    oss << ", type=" << target_type_;
    oss << ", enabled=" << (is_enabled_ ? "true" : "false");
    oss << "]";
    return oss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne