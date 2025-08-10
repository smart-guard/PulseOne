// =============================================================================
// collector/src/Database/Entities/VirtualPointEntity.cpp
// PulseOne VirtualPointEntity 구현
// =============================================================================

#include "Database/Entities/VirtualPointEntity.h"
#include "Database/DatabaseManager.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자
// =============================================================================

VirtualPointEntity::VirtualPointEntity() : BaseEntity() {
    // 기본 생성자
}

VirtualPointEntity::VirtualPointEntity(int id) : BaseEntity(id) {
    // ID로 생성
}

VirtualPointEntity::VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula)
    : BaseEntity()
    , tenant_id_(tenant_id)
    , name_(name)
    , formula_(formula) {
    // 필수 필드로 생성
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현
// =============================================================================

bool VirtualPointEntity::loadFromDatabase() {
    if (id_ <= 0) return false;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::string query = R"(
            SELECT tenant_id, scope_type, site_id, device_id, name, description,
                   formula, data_type, unit, calculation_interval, calculation_trigger,
                   execution_type, error_handling, input_mappings, dependencies,
                   cache_duration_ms, is_enabled, category, tags, execution_count,
                   last_value, last_error, avg_execution_time_ms, created_by,
                   created_at, updated_at
            FROM virtual_points
            WHERE id = ?
        )";
        
        auto results = db.executeQuery(query, {std::to_string(id_)});
        
        if (results.empty()) return false;
        
        const auto& row = results[0];
        
        // 필수 필드
        tenant_id_ = std::stoi(row.at("tenant_id"));
        scope_type_ = row.at("scope_type");
        name_ = row.at("name");
        description_ = row.at("description");
        formula_ = row.at("formula");
        data_type_ = row.at("data_type");
        unit_ = row.at("unit");
        calculation_interval_ = std::stoi(row.at("calculation_interval"));
        calculation_trigger_ = row.at("calculation_trigger");
        
        // 선택 필드
        if (!row.at("site_id").empty()) {
            site_id_ = std::stoi(row.at("site_id"));
        }
        if (!row.at("device_id").empty()) {
            device_id_ = std::stoi(row.at("device_id"));
        }
        
        // Enum 변환
        std::string exec_type = row.at("execution_type");
        if (exec_type == "javascript") execution_type_ = ExecutionType::JAVASCRIPT;
        else if (exec_type == "formula") execution_type_ = ExecutionType::FORMULA;
        else if (exec_type == "aggregate") execution_type_ = ExecutionType::AGGREGATE;
        else execution_type_ = ExecutionType::REFERENCE;
        
        std::string error_handling = row.at("error_handling");
        if (error_handling == "return_null") error_handling_ = ErrorHandling::RETURN_NULL;
        else if (error_handling == "return_last") error_handling_ = ErrorHandling::RETURN_LAST;
        else if (error_handling == "return_zero") error_handling_ = ErrorHandling::RETURN_ZERO;
        else error_handling_ = ErrorHandling::RETURN_DEFAULT;
        
        // JSON 필드
        input_mappings_ = row.at("input_mappings");
        dependencies_ = row.at("dependencies");
        tags_ = row.at("tags");
        
        // 숫자 필드
        cache_duration_ms_ = std::stoi(row.at("cache_duration_ms"));
        execution_count_ = std::stoi(row.at("execution_count"));
        last_value_ = std::stod(row.at("last_value"));
        avg_execution_time_ms_ = std::stod(row.at("avg_execution_time_ms"));
        
        // 불린 필드
        is_enabled_ = row.at("is_enabled") == "1";
        
        // 기타
        category_ = row.at("category");
        last_error_ = row.at("last_error");
        created_by_ = row.at("created_by");
        
        state_ = EntityState::LOADED;
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load VirtualPointEntity: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::saveToDatabase() {
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::string query = R"(
            INSERT INTO virtual_points 
            (tenant_id, scope_type, site_id, device_id, name, description,
             formula, data_type, unit, calculation_interval, calculation_trigger,
             execution_type, error_handling, input_mappings, dependencies,
             cache_duration_ms, is_enabled, category, tags, created_by)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        std::vector<std::string> params = {
            std::to_string(tenant_id_),
            scope_type_,
            site_id_ ? std::to_string(*site_id_) : "NULL",
            device_id_ ? std::to_string(*device_id_) : "NULL",
            name_,
            description_,
            formula_,
            data_type_,
            unit_,
            std::to_string(calculation_interval_),
            calculation_trigger_,
            executionTypeToString(execution_type_),
            errorHandlingToString(error_handling_),
            input_mappings_,
            dependencies_,
            std::to_string(cache_duration_ms_),
            is_enabled_ ? "1" : "0",
            category_,
            tags_,
            created_by_
        };
        
        if (db.executeUpdate(query, params)) {
            // 새로 생성된 ID 가져오기
            auto results = db.executeQuery("SELECT last_insert_rowid() as id");
            if (!results.empty()) {
                id_ = std::stoi(results[0].at("id"));
                state_ = EntityState::LOADED;
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to save VirtualPointEntity: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::updateToDatabase() {
    if (id_ <= 0) return false;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::string query = R"(
            UPDATE virtual_points SET
                tenant_id = ?, scope_type = ?, site_id = ?, device_id = ?,
                name = ?, description = ?, formula = ?, data_type = ?,
                unit = ?, calculation_interval = ?, calculation_trigger = ?,
                execution_type = ?, error_handling = ?, input_mappings = ?,
                dependencies = ?, cache_duration_ms = ?, is_enabled = ?,
                category = ?, tags = ?, execution_count = ?, last_value = ?,
                last_error = ?, avg_execution_time_ms = ?, updated_at = datetime('now')
            WHERE id = ?
        )";
        
        std::vector<std::string> params = {
            std::to_string(tenant_id_),
            scope_type_,
            site_id_ ? std::to_string(*site_id_) : "NULL",
            device_id_ ? std::to_string(*device_id_) : "NULL",
            name_,
            description_,
            formula_,
            data_type_,
            unit_,
            std::to_string(calculation_interval_),
            calculation_trigger_,
            executionTypeToString(execution_type_),
            errorHandlingToString(error_handling_),
            input_mappings_,
            dependencies_,
            std::to_string(cache_duration_ms_),
            is_enabled_ ? "1" : "0",
            category_,
            tags_,
            std::to_string(execution_count_),
            std::to_string(last_value_),
            last_error_,
            std::to_string(avg_execution_time_ms_),
            std::to_string(id_)
        };
        
        if (db.executeUpdate(query, params)) {
            state_ = EntityState::LOADED;
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to update VirtualPointEntity: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::deleteFromDatabase() {
    if (id_ <= 0) return false;
    
    try {
        auto& db = DatabaseManager::getInstance();
        
        std::string query = "DELETE FROM virtual_points WHERE id = ?";
        
        if (db.executeUpdate(query, {std::to_string(id_)})) {
            state_ = EntityState::DELETED;
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to delete VirtualPointEntity: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::validate() const {
    // 필수 필드 검증
    if (name_.empty()) return false;
    if (formula_.empty()) return false;
    if (tenant_id_ <= 0) return false;
    
    return true;
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
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to parse VirtualPointEntity from JSON: " + std::string(e.what()));
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
        if (!input_mappings_.empty()) {
            j["input_mappings"] = json::parse(input_mappings_);
        } else {
            j["input_mappings"] = json::array();
        }
    } catch (...) {
        j["input_mappings"] = input_mappings_;
    }
    
    try {
        if (!dependencies_.empty()) {
            j["dependencies"] = json::parse(dependencies_);
        } else {
            j["dependencies"] = json::array();
        }
    } catch (...) {
        j["dependencies"] = dependencies_;
    }
    
    try {
        if (!tags_.empty()) {
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
        if (!tags_.empty()) {
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

} // namespace Entities
} // namespace Database
} // namespace PulseOne