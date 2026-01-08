// =============================================================================
// collector/src/Database/Entities/VirtualPointEntity.cpp
// PulseOne VirtualPointEntity 구현 - DB 스키마와 완전 동기화
// =============================================================================

#include "Database/Entities/VirtualPointEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Logging/LogManager.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// BaseEntity 순수 가상 함수 구현
// =============================================================================

bool VirtualPointEntity::loadFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().Error("VirtualPointEntity::loadFromDatabase - Invalid virtual point ID: " + std::to_string(getId()));
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            LogManager::getInstance().Error("VirtualPointEntity::loadFromDatabase - Repository not available");
            markError();
            return false;
        }
        
        auto loaded = repo->findById(getId());
        if (loaded.has_value()) {
            *this = loaded.value();
            markSaved();
            LogManager::getInstance().Info("VirtualPointEntity::loadFromDatabase - Loaded virtual point: " + name_);
            return true;
        }
        
        LogManager::getInstance().Warn("VirtualPointEntity::loadFromDatabase - Virtual point not found: " + std::to_string(getId()));
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool VirtualPointEntity::saveToDatabase() {
    if (!validate()) {
        LogManager::getInstance().Error("VirtualPointEntity::saveToDatabase - Invalid virtual point data");
        markError();
        return false;
    }
    
    try {
        // 저장 전 타임스탬프 설정
        auto now = std::chrono::system_clock::now();
        if (getId() <= 0) {
            created_at_ = now;
        }
        updated_at_ = now;
        
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            LogManager::getInstance().Error("VirtualPointEntity::saveToDatabase - Repository not available");
            markError();
            return false;
        }
        
        bool success = false;
        if (getId() <= 0) {
            success = repo->save(*this);
        } else {
            success = repo->update(*this);
        }
        
        if (success) {
            markSaved();
            LogManager::getInstance().Info("VirtualPointEntity::saveToDatabase - Saved virtual point: " + name_ + 
                                         " (ID: " + std::to_string(getId()) + ")");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool VirtualPointEntity::updateToDatabase() {
    if (getId() <= 0 || !validate()) {
        LogManager::getInstance().Error("VirtualPointEntity::updateToDatabase - Invalid virtual point data or ID");
        markError();
        return false;
    }
    
    try {
        // 업데이트 타임스탬프 갱신
        updated_at_ = std::chrono::system_clock::now();
        
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            LogManager::getInstance().Error("VirtualPointEntity::updateToDatabase - Repository not available");
            markError();
            return false;
        }
        
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();
            LogManager::getInstance().Info("VirtualPointEntity::updateToDatabase - Updated virtual point: " + name_ + 
                                         " (ID: " + std::to_string(getId()) + ")");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool VirtualPointEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().Error("VirtualPointEntity::deleteFromDatabase - Invalid virtual point ID");
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            LogManager::getInstance().Error("VirtualPointEntity::deleteFromDatabase - Repository not available");
            markError();
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            LogManager::getInstance().Info("VirtualPointEntity::deleteFromDatabase - Deleted virtual point: " + name_ + 
                                         " (ID: " + std::to_string(getId()) + ")");
            setId(-1); // 삭제 후 ID 리셋
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

// =============================================================================
// JSON 변환 - 실제 DB 스키마 기반
// =============================================================================

json VirtualPointEntity::toJson() const {
    json j;
    
    try {
        // 기본 필드
        j["id"] = getId();
        j["tenant_id"] = tenant_id_;
        j["scope_type"] = scope_type_;
        
        if (site_id_.has_value()) {
            j["site_id"] = site_id_.value();
        } else {
            j["site_id"] = nullptr;
        }
        
        if (device_id_.has_value()) {
            j["device_id"] = device_id_.value();
        } else {
            j["device_id"] = nullptr;
        }
        
        j["name"] = name_;
        j["description"] = description_;
        j["formula"] = formula_;
        j["data_type"] = data_type_;
        j["unit"] = unit_;
        
        // 계산 설정
        j["calculation_interval"] = calculation_interval_;
        j["calculation_trigger"] = calculation_trigger_;
        j["is_enabled"] = is_enabled_;
        j["category"] = category_;
        
        // 태그 JSON 파싱
        if (!tags_.empty() && tags_ != "[]") {
            try {
                j["tags"] = json::parse(tags_);
            } catch (...) {
                j["tags"] = json::array();
            }
        } else {
            j["tags"] = json::array();
        }
        
        // v3.0.0 확장 필드들
        j["execution_type"] = execution_type_;
        
        if (!dependencies_.empty() && dependencies_ != "[]") {
            try {
                j["dependencies"] = json::parse(dependencies_);
            } catch (...) {
                j["dependencies"] = json::array();
            }
        } else {
            j["dependencies"] = json::array();
        }
        
        j["cache_duration_ms"] = cache_duration_ms_;
        j["error_handling"] = error_handling_;
        j["last_error"] = last_error_;
        j["execution_count"] = execution_count_;
        j["avg_execution_time_ms"] = avg_execution_time_ms_;
        
        if (last_execution_time_.has_value()) {
            j["last_execution_time"] = timestampToString(last_execution_time_.value());
        } else {
            j["last_execution_time"] = nullptr;
        }
        
        // 스크립트 라이브러리 연동
        if (script_library_id_.has_value()) {
            j["script_library_id"] = script_library_id_.value();
        } else {
            j["script_library_id"] = nullptr;
        }
        
        // 성능 추적 설정
        j["performance_tracking_enabled"] = performance_tracking_enabled_;
        j["log_calculations"] = log_calculations_;
        j["log_errors"] = log_errors_;
        
        // 알람 연동
        j["alarm_enabled"] = alarm_enabled_;
        
        if (high_limit_.has_value()) {
            j["high_limit"] = high_limit_.value();
        } else {
            j["high_limit"] = nullptr;
        }
        
        if (low_limit_.has_value()) {
            j["low_limit"] = low_limit_.value();
        } else {
            j["low_limit"] = nullptr;
        }
        
        j["deadband"] = deadband_;
        
        // 감사 필드
        if (created_by_.has_value()) {
            j["created_by"] = created_by_.value();
        } else {
            j["created_by"] = nullptr;
        }
        
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::toJson failed: " + std::string(e.what()));
    }
    
    return j;
}

bool VirtualPointEntity::fromJson(const json& j) {
    try {
        // 기본 필드
        if (j.contains("id")) setId(j["id"].get<int>());
        if (j.contains("tenant_id")) tenant_id_ = j["tenant_id"].get<int>();
        if (j.contains("scope_type")) scope_type_ = j["scope_type"].get<std::string>();
        
        if (j.contains("site_id") && !j["site_id"].is_null()) {
            site_id_ = j["site_id"].get<int>();
        }
        
        if (j.contains("device_id") && !j["device_id"].is_null()) {
            device_id_ = j["device_id"].get<int>();
        }
        
        if (j.contains("name")) name_ = j["name"].get<std::string>();
        if (j.contains("description")) description_ = j["description"].get<std::string>();
        if (j.contains("formula")) formula_ = j["formula"].get<std::string>();
        if (j.contains("data_type")) data_type_ = j["data_type"].get<std::string>();
        if (j.contains("unit")) unit_ = j["unit"].get<std::string>();
        
        // 계산 설정
        if (j.contains("calculation_interval")) calculation_interval_ = j["calculation_interval"].get<int>();
        if (j.contains("calculation_trigger")) calculation_trigger_ = j["calculation_trigger"].get<std::string>();
        if (j.contains("is_enabled")) is_enabled_ = j["is_enabled"].get<bool>();
        if (j.contains("category")) category_ = j["category"].get<std::string>();
        
        if (j.contains("tags")) {
            if (j["tags"].is_string()) {
                tags_ = j["tags"].get<std::string>();
            } else {
                tags_ = j["tags"].dump();
            }
        }
        
        // v3.0.0 확장 필드들
        if (j.contains("execution_type")) execution_type_ = j["execution_type"].get<std::string>();
        
        if (j.contains("dependencies")) {
            if (j["dependencies"].is_string()) {
                dependencies_ = j["dependencies"].get<std::string>();
            } else {
                dependencies_ = j["dependencies"].dump();
            }
        }
        
        if (j.contains("cache_duration_ms")) cache_duration_ms_ = j["cache_duration_ms"].get<int>();
        if (j.contains("error_handling")) error_handling_ = j["error_handling"].get<std::string>();
        if (j.contains("last_error")) last_error_ = j["last_error"].get<std::string>();
        if (j.contains("execution_count")) execution_count_ = j["execution_count"].get<int>();
        if (j.contains("avg_execution_time_ms")) avg_execution_time_ms_ = j["avg_execution_time_ms"].get<double>();
        
        // 스크립트 라이브러리 연동
        if (j.contains("script_library_id") && !j["script_library_id"].is_null()) {
            script_library_id_ = j["script_library_id"].get<int>();
        }
        
        // 성능 추적 설정
        if (j.contains("performance_tracking_enabled")) performance_tracking_enabled_ = j["performance_tracking_enabled"].get<bool>();
        if (j.contains("log_calculations")) log_calculations_ = j["log_calculations"].get<bool>();
        if (j.contains("log_errors")) log_errors_ = j["log_errors"].get<bool>();
        
        // 알람 연동
        if (j.contains("alarm_enabled")) alarm_enabled_ = j["alarm_enabled"].get<bool>();
        
        if (j.contains("high_limit") && !j["high_limit"].is_null()) {
            high_limit_ = j["high_limit"].get<double>();
        }
        
        if (j.contains("low_limit") && !j["low_limit"].is_null()) {
            low_limit_ = j["low_limit"].get<double>();
        }
        
        if (j.contains("deadband")) deadband_ = j["deadband"].get<double>();
        
        // 감사 필드
        if (j.contains("created_by") && !j["created_by"].is_null()) {
            created_by_ = j["created_by"].get<int>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string VirtualPointEntity::toString() const {
    std::ostringstream oss;
    oss << "VirtualPointEntity[";
    oss << "id=" << getId();
    oss << ", tenant_id=" << tenant_id_;
    oss << ", name=" << name_;
    oss << ", scope_type=" << scope_type_;
    oss << ", formula=" << formula_.substr(0, 50);
    if (formula_.length() > 50) oss << "...";
    oss << ", enabled=" << (is_enabled_ ? "true" : "false");
    oss << ", execution_count=" << execution_count_;
    oss << "]";
    return oss.str();
}

// =============================================================================
// 유틸리티 메서드 구현
// =============================================================================

std::vector<std::string> VirtualPointEntity::getTagList() const {
    std::vector<std::string> result;
    
    if (tags_.empty() || tags_ == "[]") {
        return result;
    }
    
    try {
        auto tags_array = json::parse(tags_);
        if (tags_array.is_array()) {
            for (const auto& tag : tags_array) {
                if (tag.is_string()) {
                    result.push_back(tag.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("VirtualPointEntity::getTagList - JSON parsing failed: " + std::string(e.what()));
    }
    
    return result;
}

bool VirtualPointEntity::hasTag(const std::string& tag) const {
    auto tags = getTagList();
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

bool VirtualPointEntity::validate() const {
    // 필수 필드 검증
    if (name_.empty()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Name is empty");
        return false;
    }
    
    if (formula_.empty()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Formula is empty");
        return false;
    }
    
    if (tenant_id_ <= 0) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid tenant_id: " + std::to_string(tenant_id_));
        return false;
    }
    
    // scope_type 검증
    if (scope_type_ != "tenant" && scope_type_ != "site" && scope_type_ != "device") {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid scope_type: " + scope_type_);
        return false;
    }
    
    // scope 일관성 검증
    if (scope_type_ == "tenant" && (site_id_.has_value() || device_id_.has_value())) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Tenant scope cannot have site_id or device_id");
        return false;
    }
    
    if (scope_type_ == "site" && (!site_id_.has_value() || device_id_.has_value())) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Site scope must have site_id but no device_id");
        return false;
    }
    
    if (scope_type_ == "device" && (!site_id_.has_value() || !device_id_.has_value())) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Device scope must have both site_id and device_id");
        return false;
    }
    
    // data_type 검증 (DB 제약조건)
    const std::vector<std::string> valid_data_types = {"bool", "int", "float", "double", "string"};
    if (std::find(valid_data_types.begin(), valid_data_types.end(), data_type_) == valid_data_types.end()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid data_type: " + data_type_);
        return false;
    }
    
    // calculation_trigger 검증 (DB 제약조건)
    const std::vector<std::string> valid_triggers = {"timer", "onchange", "manual", "event"};
    if (std::find(valid_triggers.begin(), valid_triggers.end(), calculation_trigger_) == valid_triggers.end()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid calculation_trigger: " + calculation_trigger_);
        return false;
    }
    
    // execution_type 검증 (DB 제약조건)
    const std::vector<std::string> valid_execution_types = {"javascript", "formula", "aggregation", "external"};
    if (std::find(valid_execution_types.begin(), valid_execution_types.end(), execution_type_) == valid_execution_types.end()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid execution_type: " + execution_type_);
        return false;
    }
    
    // error_handling 검증 (DB 제약조건)
    const std::vector<std::string> valid_error_handling = {"return_null", "return_zero", "return_previous", "throw_error"};
    if (std::find(valid_error_handling.begin(), valid_error_handling.end(), error_handling_) == valid_error_handling.end()) {
        LogManager::getInstance().Debug("VirtualPointEntity::validate - Invalid error_handling: " + error_handling_);
        return false;
    }
    
    return true;
}

// =============================================================================
// Private 헬퍼 메서드
// =============================================================================

std::string VirtualPointEntity::timestampToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::chrono::system_clock::time_point VirtualPointEntity::stringToTimestamp(const std::string& str) const {
    // 간단한 ISO 타임스탬프 파싱 (실제로는 더 정교한 구현 필요)
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne