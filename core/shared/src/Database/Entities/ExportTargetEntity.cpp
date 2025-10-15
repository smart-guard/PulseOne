/**
 * @file ExportTargetEntity.cpp
 * @brief Export Target Entity 구현 - SiteEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Entities/ExportTargetEntity.cpp
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
// Repository 패턴 지원 (BaseEntity 통합)
// =============================================================================

std::shared_ptr<Repositories::ExportTargetRepository> ExportTargetEntity::getRepository() const {
    return RepositoryFactory::getInstance().getExportTargetRepository();
}

// =============================================================================
// DB 연산 (BaseEntity 패턴)
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
// JSON 직렬화 (SiteEntity 패턴)
// =============================================================================

json ExportTargetEntity::toJson() const {
    json j;
    
    // 기본 필드
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
    
    // 통계
    j["total_exports"] = total_exports_;
    j["successful_exports"] = successful_exports_;
    j["failed_exports"] = failed_exports_;
    j["success_rate"] = getSuccessRate();
    j["last_error"] = last_error_;
    j["avg_export_time_ms"] = avg_export_time_ms_;
    
    // 타임스탬프
    if (last_export_at_.has_value()) {
        j["last_export_at"] = std::chrono::system_clock::to_time_t(last_export_at_.value());
    }
    if (last_success_at_.has_value()) {
        j["last_success_at"] = std::chrono::system_clock::to_time_t(last_success_at_.value());
    }
    if (last_error_at_.has_value()) {
        j["last_error_at"] = std::chrono::system_clock::to_time_t(last_error_at_.value());
    }
    if (created_at_.time_since_epoch().count() > 0) {
        j["created_at"] = std::chrono::system_clock::to_time_t(created_at_);
    }
    if (updated_at_.time_since_epoch().count() > 0) {
        j["updated_at"] = std::chrono::system_clock::to_time_t(updated_at_);
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
// 유효성 검증
// =============================================================================

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
    if (target_type_ != "HTTP" && target_type_ != "HTTPS" && 
        target_type_ != "S3" && target_type_ != "FILE" && target_type_ != "MQTT") {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - invalid target_type: " + target_type_);
        return false;
    }
    
    // export_mode 검증
    if (export_mode_ != "on_change" && export_mode_ != "periodic" && export_mode_ != "both") {
        LogManager::getInstance().Warn("ExportTargetEntity::validate - invalid export_mode: " + export_mode_);
        return false;
    }
    
    return true;
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

void ExportTargetEntity::recordExportSuccess(int processing_time_ms) {
    total_exports_++;
    successful_exports_++;
    last_export_at_ = std::chrono::system_clock::now();
    last_success_at_ = std::chrono::system_clock::now();
    
    // 평균 처리 시간 갱신 (이동 평균)
    if (avg_export_time_ms_ == 0) {
        avg_export_time_ms_ = processing_time_ms;
    } else {
        avg_export_time_ms_ = (avg_export_time_ms_ * 9 + processing_time_ms) / 10;
    }
    
    markModified();
}

void ExportTargetEntity::recordExportFailure(const std::string& error_message) {
    total_exports_++;
    failed_exports_++;
    last_export_at_ = std::chrono::system_clock::now();
    last_error_ = error_message;
    last_error_at_ = std::chrono::system_clock::now();
    
    markModified();
}

void ExportTargetEntity::resetStatistics() {
    total_exports_ = 0;
    successful_exports_ = 0;
    failed_exports_ = 0;
    avg_export_time_ms_ = 0;
    last_error_.clear();
    
    markModified();
}

std::string ExportTargetEntity::getEntityTypeName() const {
    return "ExportTarget";
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne