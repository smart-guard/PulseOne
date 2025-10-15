/**
 * @file ExportTargetEntity.cpp
 * @version 2.0.0
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
// DB 연산
// =============================================================================

bool ExportTargetEntity::loadFromDatabase() {
    return loadViaRepository();
}

bool ExportTargetEntity::saveToDatabase() {
    return saveViaRepository();
}

bool ExportTargetEntity::updateToDatabase() {
    return updateViaRepository();
}

bool ExportTargetEntity::deleteFromDatabase() {
    return deleteViaRepository();
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json ExportTargetEntity::toJson() const {
    json j;
    
    j["id"] = id_;
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
    j["success_rate"] = getSuccessRate();
    j["last_error"] = last_error_;
    j["avg_export_time_ms"] = avg_export_time_ms_;
    
    if (last_export_at_.has_value()) {
        j["last_export_at"] = std::chrono::system_clock::to_time_t(last_export_at_.value());
    }
    if (last_success_at_.has_value()) {
        j["last_success_at"] = std::chrono::system_clock::to_time_t(last_success_at_.value());
    }
    if (last_error_at_.has_value()) {
        j["last_error_at"] = std::chrono::system_clock::to_time_t(last_error_at_.value());
    }
    
    return j;
}

bool ExportTargetEntity::fromJson(const json& data) {
    try {
        if (data.contains("id") && data["id"].is_number()) {
            id_ = data["id"].get<int>();
        }
        
        if (data.contains("profile_id") && data["profile_id"].is_number()) {
            profile_id_ = data["profile_id"].get<int>();
        }
        
        if (data.contains("name") && data["name"].is_string()) {
            name_ = data["name"].get<std::string>();
        }
        
        if (data.contains("target_type") && data["target_type"].is_string()) {
            target_type_ = data["target_type"].get<std::string>();
        }
        
        if (data.contains("description") && data["description"].is_string()) {
            description_ = data["description"].get<std::string>();
        }
        
        if (data.contains("is_enabled") && data["is_enabled"].is_boolean()) {
            is_enabled_ = data["is_enabled"].get<bool>();
        }
        
        if (data.contains("config") && data["config"].is_string()) {
            config_ = data["config"].get<std::string>();
        }
        
        if (data.contains("export_mode") && data["export_mode"].is_string()) {
            export_mode_ = data["export_mode"].get<std::string>();
        }
        
        if (data.contains("export_interval") && data["export_interval"].is_number()) {
            export_interval_ = data["export_interval"].get<int>();
        }
        
        if (data.contains("batch_size") && data["batch_size"].is_number()) {
            batch_size_ = data["batch_size"].get<int>();
        }
        
        if (data.contains("total_exports") && data["total_exports"].is_number()) {
            total_exports_ = data["total_exports"].get<uint64_t>();
        }
        
        if (data.contains("successful_exports") && data["successful_exports"].is_number()) {
            successful_exports_ = data["successful_exports"].get<uint64_t>();
        }
        
        if (data.contains("failed_exports") && data["failed_exports"].is_number()) {
            failed_exports_ = data["failed_exports"].get<uint64_t>();
        }
        
        if (data.contains("last_error") && data["last_error"].is_string()) {
            last_error_ = data["last_error"].get<std::string>();
        }
        
        if (data.contains("avg_export_time_ms") && data["avg_export_time_ms"].is_number()) {
            avg_export_time_ms_ = data["avg_export_time_ms"].get<int>();
        }
        
        markModified();
        return true;
        
    } catch (const json::exception& e) {
        LogManager::getInstance().Error("ExportTargetEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string ExportTargetEntity::toString() const {
    return toJson().dump(2);
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

double ExportTargetEntity::getSuccessRate() const {
    if (total_exports_ == 0) return 0.0;
    return (static_cast<double>(successful_exports_) / total_exports_) * 100.0;
}

bool ExportTargetEntity::isHealthy() const {
    return is_enabled_ && (getSuccessRate() > 50.0);
}

bool ExportTargetEntity::validate() const {
    if (name_.empty()) {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - name is empty");
        return false;
    }
    
    if (target_type_.empty()) {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - target_type is empty");
        return false;
    }
    
    if (config_.empty()) {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - config is empty");
        return false;
    }
    
    // target_type 검증
    if (target_type_ != "HTTP" && target_type_ != "S3" && 
        target_type_ != "FILE" && target_type_ != "MQTT") {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - invalid target_type: " + target_type_);
        return false;
    }
    
    return true;
}

std::string ExportTargetEntity::getEntityTypeName() const {
    return "ExportTarget";
}

}
}
}