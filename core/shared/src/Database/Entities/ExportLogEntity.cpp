/**
 * @file ExportLogEntity.cpp
 * @version 2.0.0
 */

#include "Database/Entities/ExportLogEntity.h"
#include "Database/Repositories/ExportLogRepository.h"
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

ExportLogEntity::ExportLogEntity()
    : BaseEntity()
    , service_id_(0)
    , target_id_(0)
    , mapping_id_(0)
    , point_id_(0)
    , http_status_code_(0)
    , processing_time_ms_(0)
    , timestamp_(std::chrono::system_clock::now()) {
}

ExportLogEntity::ExportLogEntity(int id)
    : BaseEntity(id)
    , service_id_(0)
    , target_id_(0)
    , mapping_id_(0)
    , point_id_(0)
    , http_status_code_(0)
    , processing_time_ms_(0)
    , timestamp_(std::chrono::system_clock::now()) {
}

ExportLogEntity::~ExportLogEntity() {
}

// =============================================================================
// Repository 패턴 지원
// =============================================================================

std::shared_ptr<Repositories::ExportLogRepository> ExportLogEntity::getRepository() const {
    return RepositoryFactory::getInstance().getExportLogRepository();
}

// =============================================================================
// DB 연산
// =============================================================================

bool ExportLogEntity::loadFromDatabase() {
    return loadViaRepository();
}

bool ExportLogEntity::saveToDatabase() {
    return saveViaRepository();
}

bool ExportLogEntity::updateToDatabase() {
    return updateViaRepository();
}

bool ExportLogEntity::deleteFromDatabase() {
    return deleteViaRepository();
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json ExportLogEntity::toJson() const {
    json j;
    
    j["id"] = id_;
    j["log_type"] = log_type_;
    j["service_id"] = service_id_;
    j["target_id"] = target_id_;
    j["mapping_id"] = mapping_id_;
    j["point_id"] = point_id_;
    j["source_value"] = source_value_;
    j["converted_value"] = converted_value_;
    j["status"] = status_;
    j["error_message"] = error_message_;
    j["error_code"] = error_code_;
    j["response_data"] = response_data_;
    j["http_status_code"] = http_status_code_;
    j["processing_time_ms"] = processing_time_ms_;
    j["timestamp"] = std::chrono::system_clock::to_time_t(timestamp_);
    j["client_info"] = client_info_;
    
    return j;
}

bool ExportLogEntity::fromJson(const json& data) {
    try {
        if (data.contains("id") && data["id"].is_number()) {
            id_ = data["id"].get<int>();
        }
        
        if (data.contains("log_type") && data["log_type"].is_string()) {
            log_type_ = data["log_type"].get<std::string>();
        }
        
        if (data.contains("service_id") && data["service_id"].is_number()) {
            service_id_ = data["service_id"].get<int>();
        }
        
        if (data.contains("target_id") && data["target_id"].is_number()) {
            target_id_ = data["target_id"].get<int>();
        }
        
        if (data.contains("mapping_id") && data["mapping_id"].is_number()) {
            mapping_id_ = data["mapping_id"].get<int>();
        }
        
        if (data.contains("point_id") && data["point_id"].is_number()) {
            point_id_ = data["point_id"].get<int>();
        }
        
        if (data.contains("source_value") && data["source_value"].is_string()) {
            source_value_ = data["source_value"].get<std::string>();
        }
        
        if (data.contains("converted_value") && data["converted_value"].is_string()) {
            converted_value_ = data["converted_value"].get<std::string>();
        }
        
        if (data.contains("status") && data["status"].is_string()) {
            status_ = data["status"].get<std::string>();
        }
        
        if (data.contains("error_message") && data["error_message"].is_string()) {
            error_message_ = data["error_message"].get<std::string>();
        }
        
        if (data.contains("error_code") && data["error_code"].is_string()) {
            error_code_ = data["error_code"].get<std::string>();
        }
        
        if (data.contains("response_data") && data["response_data"].is_string()) {
            response_data_ = data["response_data"].get<std::string>();
        }
        
        if (data.contains("http_status_code") && data["http_status_code"].is_number()) {
            http_status_code_ = data["http_status_code"].get<int>();
        }
        
        if (data.contains("processing_time_ms") && data["processing_time_ms"].is_number()) {
            processing_time_ms_ = data["processing_time_ms"].get<int>();
        }
        
        if (data.contains("client_info") && data["client_info"].is_string()) {
            client_info_ = data["client_info"].get<std::string>();
        }
        
        markModified();
        return true;
        
    } catch (const json::exception& e) {
        LogManager::getInstance().Error("ExportLogEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string ExportLogEntity::toString() const {
    return toJson().dump(2);
}

// =============================================================================
// 유효성 검증
// =============================================================================

bool ExportLogEntity::validate() const {
    if (log_type_.empty()) {
        LogManager::getInstance().Warn("ExportLogEntity::validate - log_type is empty");
        return false;
    }
    
    if (status_.empty()) {
        LogManager::getInstance().Warn("ExportLogEntity::validate - status is empty");
        return false;
    }
    
    if (log_type_ != "export" && log_type_ != "protocol" && log_type_ != "mapping") {
        LogManager::getInstance().Warn("ExportLogEntity::validate - invalid log_type: " + log_type_);
        return false;
    }
    
    if (status_ != "success" && status_ != "failed" && status_ != "retry") {
        LogManager::getInstance().Warn("ExportLogEntity::validate - invalid status: " + status_);
        return false;
    }
    
    return true;
}

bool ExportLogEntity::isSuccess() const {
    return status_ == "success";
}

std::string ExportLogEntity::getEntityTypeName() const {
    return "ExportLog";
}

}
}
}