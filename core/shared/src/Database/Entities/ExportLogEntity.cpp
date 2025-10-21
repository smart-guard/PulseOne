/**
 * @file ExportLogEntity.cpp
 * @brief Export Log 엔티티 구현부
 * @version 3.0.0 - 완전 리팩토링
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/src/Database/Entities/ExportLogEntity.cpp
 */

#include "Database/Entities/ExportLogEntity.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

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

std::shared_ptr<Repositories::ExportLogRepository> 
ExportLogEntity::getRepository() const {
    return RepositoryFactory::getInstance().getExportLogRepository();
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

bool ExportLogEntity::validate() const {
    // 기본 유효성 검사
    if (!BaseEntity<ExportLogEntity>::isValid()) {
        return false;
    }
    
    // 필수 필드 검사
    if (log_type_.empty()) {
        return false;
    }
    
    if (status_.empty()) {
        return false;
    }
    
    // status 값 검증 ('success', 'failed', 'timeout' 등)
    if (status_ != "success" && 
        status_ != "failed" && 
        status_ != "timeout" &&
        status_ != "pending") {
        return false;
    }
    
    // target_id는 0이면 안됨 (최소한 타겟은 있어야 함)
    if (target_id_ <= 0) {
        return false;
    }
    
    return true;
}

bool ExportLogEntity::isSuccess() const {
    return status_ == "success";
}

bool ExportLogEntity::isFailure() const {
    return status_ == "failed" || status_ == "timeout";
}

std::string ExportLogEntity::getEntityTypeName() const {
    return "ExportLogEntity";
}

// =============================================================================
// DB 연산 - Repository 위임
// =============================================================================

bool ExportLogEntity::loadFromDatabase() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto repo = getRepository();
        if (!repo) {
            return false;
        }
        
        auto loaded = repo->findById(getId());
        if (loaded.has_value()) {
            *this = loaded.value();
            markSaved();
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        markError();
        return false;
    }
}

bool ExportLogEntity::saveToDatabase() {
    try {
        auto repo = getRepository();
        if (!repo) {
            return false;
        }
        
        bool success = false;
        
        if (getId() <= 0) {
            // 신규 저장 (INSERT)
            // ExportLog는 주로 INSERT만 사용
            success = repo->save(*this);
        } else {
            // 업데이트 (UPDATE) - 거의 사용 안 함
            success = repo->update(*this);
        }
        
        if (success) {
            markSaved();
        }
        
        return success;
        
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
        auto repo = getRepository();
        if (!repo) {
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            markDeleted();
        }
        
        return success;
        
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
        // 기본 정보
        j["id"] = getId();
        j["log_type"] = log_type_;
        j["service_id"] = service_id_;
        j["target_id"] = target_id_;
        j["mapping_id"] = mapping_id_;
        j["point_id"] = point_id_;
        
        // 데이터
        j["source_value"] = source_value_;
        j["converted_value"] = converted_value_;
        
        // 결과
        j["status"] = status_;
        j["http_status_code"] = http_status_code_;
        
        // 에러 정보
        j["error_message"] = error_message_;
        j["error_code"] = error_code_;
        j["response_data"] = response_data_;
        
        // 성능 정보
        j["processing_time_ms"] = processing_time_ms_;
        
        // 메타 정보
        j["client_info"] = client_info_;
        
        // 타임스탬프 (epoch seconds)
        j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            timestamp_.time_since_epoch()).count();
        
        j["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            getCreatedAt().time_since_epoch()).count();
        
        j["updated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            getUpdatedAt().time_since_epoch()).count();
        
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
        
        // 타임스탬프 파싱
        if (data.contains("timestamp")) {
            if (data["timestamp"].is_number()) {
                // epoch seconds
                auto seconds = data["timestamp"].get<int64_t>();
                timestamp_ = std::chrono::system_clock::from_time_t(seconds);
            }
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
    
    if (!error_message_.empty()) {
        oss << ", error=" << error_message_;
    }
    
    oss << ", time=" << processing_time_ms_ << "ms";
    oss << "]";
    
    return oss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne