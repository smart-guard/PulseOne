// =============================================================================
// collector/src/Database/Entities/VirtualPointEntity.cpp
// PulseOne VirtualPointEntity 구현 - 단순한 데이터 구조체
// =============================================================================

#include "Database/Entities/VirtualPointEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자
// =============================================================================

VirtualPointEntity::VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula)
    : BaseEntity()
    , tenant_id_(tenant_id)
    , name_(name)
    , formula_(formula) {
    // 필수 필드로 생성
}

// =============================================================================
// JSON 변환
// =============================================================================

bool VirtualPointEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) id_ = data["id"];
        if (data.contains("tenant_id")) tenant_id_ = data["tenant_id"];
        if (data.contains("scope_type")) scope_type_ = data["scope_type"];
        if (data.contains("site_id") && !data["site_id"].is_null()) {
            site_id_ = data["site_id"];
        }
        if (data.contains("device_id") && !data["device_id"].is_null()) {
            device_id_ = data["device_id"];
        }
        if (data.contains("name")) name_ = data["name"];
        if (data.contains("description")) description_ = data["description"];
        if (data.contains("formula")) formula_ = data["formula"];
        if (data.contains("data_type")) data_type_ = data["data_type"];
        if (data.contains("unit")) unit_ = data["unit"];
        if (data.contains("calculation_interval")) calculation_interval_ = data["calculation_interval"];
        if (data.contains("calculation_trigger")) calculation_trigger_ = data["calculation_trigger"];
        
        if (data.contains("execution_type")) {
            execution_type_ = stringToExecutionType(data["execution_type"]);
        }
        if (data.contains("error_handling")) {
            error_handling_ = stringToErrorHandling(data["error_handling"]);
        }
        
        if (data.contains("input_mappings")) {
            if (data["input_mappings"].is_string()) {
                input_mappings_ = data["input_mappings"];
            } else {
                input_mappings_ = data["input_mappings"].dump();
            }
        }
        if (data.contains("dependencies")) {
            if (data["dependencies"].is_string()) {
                dependencies_ = data["dependencies"];
            } else {
                dependencies_ = data["dependencies"].dump();
            }
        }
        if (data.contains("cache_duration_ms")) cache_duration_ms_ = data["cache_duration_ms"];
        if (data.contains("is_enabled")) is_enabled_ = data["is_enabled"];
        if (data.contains("category")) category_ = data["category"];
        if (data.contains("tags")) {
            if (data["tags"].is_string()) {
                tags_ = data["tags"];
            } else {
                tags_ = data["tags"].dump();
            }
        }
        
        if (data.contains("execution_count")) execution_count_ = data["execution_count"];
        if (data.contains("last_value")) last_value_ = data["last_value"];
        if (data.contains("last_error")) last_error_ = data["last_error"];
        if (data.contains("avg_execution_time_ms")) avg_execution_time_ms_ = data["avg_execution_time_ms"];
        if (data.contains("created_by")) created_by_ = data["created_by"];
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to parse VirtualPointEntity from JSON: " + std::string(e.what()));
        }
        return false;
    }
}

json VirtualPointEntity::toJson() const {
    json j;
    j["id"] = id_;
    j["tenant_id"] = tenant_id_;
    j["scope_type"] = scope_type_;
    
    if (site_id_) j["site_id"] = *site_id_;
    else j["site_id"] = nullptr;
    
    if (device_id_) j["device_id"] = *device_id_;
    else j["device_id"] = nullptr;
    
    j["name"] = name_;
    j["description"] = description_;
    j["formula"] = formula_;
    j["data_type"] = data_type_;
    j["unit"] = unit_;
    j["calculation_interval"] = calculation_interval_;
    j["calculation_trigger"] = calculation_trigger_;
    j["execution_type"] = executionTypeToString(execution_type_);
    j["error_handling"] = errorHandlingToString(error_handling_);
    
    // JSON 문자열 필드들은 파싱해서 저장
    try {
        if (!input_mappings_.empty() && input_mappings_ != "[]") {
            j["input_mappings"] = json::parse(input_mappings_);
        } else {
            j["input_mappings"] = json::array();
        }
    } catch (...) {
        j["input_mappings"] = input_mappings_;
    }
    
    try {
        if (!dependencies_.empty() && dependencies_ != "[]") {
            j["dependencies"] = json::parse(dependencies_);
        } else {
            j["dependencies"] = json::array();
        }
    } catch (...) {
        j["dependencies"] = dependencies_;
    }
    
    try {
        if (!tags_.empty() && tags_ != "[]") {
            j["tags"] = json::parse(tags_);
        } else {
            j["tags"] = json::array();
        }
    } catch (...) {
        j["tags"] = tags_;
    }
    
    j["cache_duration_ms"] = cache_duration_ms_;
    j["is_enabled"] = is_enabled_;
    j["category"] = category_;
    j["execution_count"] = execution_count_;
    j["last_value"] = last_value_;
    j["last_error"] = last_error_;
    j["avg_execution_time_ms"] = avg_execution_time_ms_;
    j["created_by"] = created_by_;
    
    return j;
}

std::string VirtualPointEntity::toString() const {
    std::stringstream ss;
    ss << "VirtualPointEntity[id=" << id_ 
       << ", tenant_id=" << tenant_id_
       << ", name=" << name_
       << ", formula=" << formula_
       << ", enabled=" << (is_enabled_ ? "true" : "false")
       << ", execution_count=" << execution_count_
       << ", last_value=" << last_value_ << "]";
    return ss.str();
}

// =============================================================================
// 헬퍼 메서드
// =============================================================================

std::vector<std::string> VirtualPointEntity::getTagList() const {
    std::vector<std::string> tag_list;
    
    try {
        if (!tags_.empty() && tags_ != "[]") {
            json tags_json = json::parse(tags_);
            if (tags_json.is_array()) {
                for (const auto& tag : tags_json) {
                    if (tag.is_string()) {
                        tag_list.push_back(tag.get<std::string>());
                    }
                }
            }
        }
    } catch (...) {
        // 파싱 실패시 빈 리스트 반환
    }
    
    return tag_list;
}

bool VirtualPointEntity::hasTag(const std::string& tag) const {
    auto tag_list = getTagList();
    return std::find(tag_list.begin(), tag_list.end(), tag) != tag_list.end();
}

bool VirtualPointEntity::validate() const {
    // 필수 필드 검증
    if (name_.empty()) return false;
    if (formula_.empty()) return false;
    if (tenant_id_ <= 0) return false;
    
    // scope_type 검증
    if (scope_type_ != "tenant" && scope_type_ != "site" && scope_type_ != "device") {
        return false;
    }
    
    // scope 일관성 검증
    if (scope_type_ == "tenant" && (site_id_.has_value() || device_id_.has_value())) {
        return false;
    }
    if (scope_type_ == "site" && (!site_id_.has_value() || device_id_.has_value())) {
        return false;
    }
    if (scope_type_ == "device" && (!site_id_.has_value() || !device_id_.has_value())) {
        return false;
    }
    
    return true;
}

// =============================================================================
// Private 헬퍼 메서드
// =============================================================================

std::string VirtualPointEntity::executionTypeToString(ExecutionType type) const {
    switch (type) {
        case ExecutionType::JAVASCRIPT: return "javascript";
        case ExecutionType::FORMULA: return "formula";
        case ExecutionType::AGGREGATE: return "aggregate";
        case ExecutionType::REFERENCE: return "reference";
        default: return "javascript";
    }
}

std::string VirtualPointEntity::errorHandlingToString(ErrorHandling handling) const {
    switch (handling) {
        case ErrorHandling::RETURN_NULL: return "return_null";
        case ErrorHandling::RETURN_LAST: return "return_last";
        case ErrorHandling::RETURN_ZERO: return "return_zero";
        case ErrorHandling::RETURN_DEFAULT: return "return_default";
        default: return "return_null";
    }
}

VirtualPointEntity::ExecutionType VirtualPointEntity::stringToExecutionType(const std::string& str) const {
    if (str == "formula") return ExecutionType::FORMULA;
    if (str == "aggregate") return ExecutionType::AGGREGATE;
    if (str == "reference") return ExecutionType::REFERENCE;
    return ExecutionType::JAVASCRIPT;
}

VirtualPointEntity::ErrorHandling VirtualPointEntity::stringToErrorHandling(const std::string& str) const {
    if (str == "return_last") return ErrorHandling::RETURN_LAST;
    if (str == "return_zero") return ErrorHandling::RETURN_ZERO;
    if (str == "return_default") return ErrorHandling::RETURN_DEFAULT;
    return ErrorHandling::RETURN_NULL;
}

bool VirtualPointEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::loadFromDatabase - Invalid virtual point ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("VirtualPointEntity::loadFromDatabase - Repository not available");
            }
            markError();
            return false;
        }
        
        auto loaded_entity = repo->findById(getId());
        if (!loaded_entity) {
            if (logger_) {
                logger_->Warn("VirtualPointEntity::loadFromDatabase - Virtual point not found: " + std::to_string(getId()));
            }
            return false;
        }
        
        // 현재 엔티티에 로드된 데이터 복사
        *this = loaded_entity.value();
        markSaved(); // DeviceEntity 패턴
        
        if (logger_) {
            logger_->Info("VirtualPointEntity::loadFromDatabase - Loaded virtual point: " + getName());
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool VirtualPointEntity::saveToDatabase() {
    if (!validate()) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::saveToDatabase - Invalid virtual point data");
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("VirtualPointEntity::saveToDatabase - Repository not available");
            }
            markError();
            return false;
        }
        
        VirtualPointEntity temp_entity = *this; // 복사본 생성 (save는 비상수 참조 필요)
        bool success = repo->save(temp_entity);
        
        if (success) {
            // 생성된 ID를 현재 엔티티에 반영
            setId(temp_entity.getId());
            markSaved(); // DeviceEntity 패턴
            
            if (logger_) {
                logger_->Info("VirtualPointEntity::saveToDatabase - Saved virtual point: " + getName() + 
                            " (ID: " + std::to_string(getId()) + ")");
            }
        } else {
            markError();
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool VirtualPointEntity::updateToDatabase() {
    if (getId() <= 0 || !validate()) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::updateToDatabase - Invalid virtual point data or ID");
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("VirtualPointEntity::updateToDatabase - Repository not available");
            }
            markError();
            return false;
        }
        
        bool success = repo->update(*this);
        
        if (success) {
            markSaved(); // DeviceEntity 패턴
            
            if (logger_) {
                logger_->Info("VirtualPointEntity::updateToDatabase - Updated virtual point: " + getName() + 
                            " (ID: " + std::to_string(getId()) + ")");
            }
        } else {
            markError();
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool VirtualPointEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::deleteFromDatabase - Invalid virtual point ID");
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("VirtualPointEntity::deleteFromDatabase - Repository not available");
            }
            markError();
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            markDeleted(); // DeviceEntity 패턴
            
            if (logger_) {
                logger_->Info("VirtualPointEntity::deleteFromDatabase - Deleted virtual point: " + getName() + 
                            " (ID: " + std::to_string(getId()) + ")");
            }
        } else {
            markError();
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("VirtualPointEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

std::string VirtualPointEntity::preprocessFormula(const std::string& formula, int tenant_id) {
    try {
        auto& logger = LogManager::getInstance();
        
        // 실제 전처리 로직
        std::string processed = formula;
        // ... 전처리 작업 ...
        
        logger.Debug("Formula preprocessed for tenant " + std::to_string(tenant_id));
        return processed;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Warn("Formula preprocessing failed: " + std::string(e.what()));  // ✅ Warning → Warn
        return formula; // 원본 반환
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne