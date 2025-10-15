/**
 * @file ExportLogEntity.cpp
 * @version 2.0.1 - loadViaRepository 제거, 직접 구현
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
// DB 연산 - 직접 구현 (loadViaRepository 사용 안 함)
// =============================================================================

bool ExportLogEntity::loadFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportLogRepository();
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

bool ExportLogEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportLogRepository();
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

bool ExportLogEntity::updateToDatabase() {
    return saveToDatabase();
}

bool ExportLogEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getExportLogRepository();
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

json ExportLogEntity::toJson() const {
    json j;
    
    try {
        j["id"] = getId();
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
        j["timestamp"] = timestampToString(timestamp_);
        j["client_info"] = client_info_;
        j["created_at"] = timestampToString(getCreatedAt());
        j["updated_at"] = timestampToString(getUpdatedAt());
        
    } catch (const std::exception&) {
        // JSON 생성 실패 시 기본 객체 반환
    }
    
    return j;
}

bool ExportLogEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) {
            setId(data["id"].get<int>());
        }
        if (data.contains("log_type")) {
            log_type_ = data["log_type"].get<std::string>();
        }
        if (data.contains("service_id")) {
            service_id_ = data["service_id"].get<int>();
        }
        if (data.contains("target_id")) {
            target_id_ = data["target_id"].get<int>();
        }
        if (data.contains("mapping_id")) {
            mapping_id_ = data["mapping_id"].get<int>();
        }
        if (data.contains("point_id")) {
            point_id_ = data["point_id"].get<int>();
        }
        if (data.contains("source_value")) {
            source_value_ = data["source_value"].get<std::string>();
        }
        if (data.contains("converted_value")) {
            converted_value_ = data["converted_value"].get<std::string>();
        }
        if (data.contains("status")) {
            status_ = data["status"].get<std::string>();
        }
        if (data.contains("error_message")) {
            error_message_ = data["error_message"].get<std::string>();
        }
        if (data.contains("error_code")) {
            error_code_ = data["error_code"].get<std::string>();
        }
        if (data.contains("response_data")) {
            response_data_ = data["response_data"].get<std::string>();
        }
        if (data.contains("http_status_code")) {
            http_status_code_ = data["http_status_code"].get<int>();
        }
        if (data.contains("processing_time_ms")) {
            processing_time_ms_ = data["processing_time_ms"].get<int>();
        }
        if (data.contains("client_info")) {
            client_info_ = data["client_info"].get<std::string>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string ExportLogEntity::toString() const {
    std::ostringstream oss;
    oss << "ExportLogEntity[";
    oss << "id=" << getId();
    oss << ", type=" << log_type_;
    oss << ", target_id=" << target_id_;
    oss << ", status=" << status_;
    oss << "]";
    return oss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne