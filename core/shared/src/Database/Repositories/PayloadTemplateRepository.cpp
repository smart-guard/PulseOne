/**
 * @file PayloadTemplateRepository.cpp
 * @brief Payload Template Repository Implementation
 * @version 1.1.0
 * 
 * 🔧 주요 수정사항:
 * 1. DatabaseAbstractionLayer API 올바른 사용 (RepositoryHelpers 활용)
 * 2. 벌크 연산 메소드명 수정: bulkSave->saveBulk, bulkUpdate->updateBulk, bulkDelete->deleteByIds
 * 3. 벌크 연산 반환 타입: bool -> int
 * 4. getLastInsertId() 대신 SQL::Common::GET_LAST_INSERT_ID 쿼리 사용
 */

#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/ExtendedSQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Utils/LogManager.h"
#include <stdexcept>

namespace PulseOne {
namespace Database {
namespace Repositories {

using namespace Entities;

// =============================================================================
// SQL 쿼리 상수 정의
// =============================================================================

namespace {
    const std::string TABLE_NAME = "payload_templates";
    
    const std::string FIND_ALL = 
        "SELECT id, name, system_type, description, template_json, is_active, "
        "created_at, updated_at FROM " + TABLE_NAME + " ORDER BY name";
    
    const std::string FIND_BY_ID = 
        "SELECT id, name, system_type, description, template_json, is_active, "
        "created_at, updated_at FROM " + TABLE_NAME + " WHERE id = ?";
    
    const std::string INSERT = 
        "INSERT INTO " + TABLE_NAME + " "
        "(name, system_type, description, template_json, is_active) "
        "VALUES (?, ?, ?, ?, ?)";
    
    const std::string UPDATE = 
        "UPDATE " + TABLE_NAME + " SET "
        "name = ?, system_type = ?, description = ?, template_json = ?, is_active = ? "
        "WHERE id = ?";
    
    const std::string DELETE_BY_ID = 
        "DELETE FROM " + TABLE_NAME + " WHERE id = ?";
    
    const std::string EXISTS_BY_ID = 
        "SELECT COUNT(*) as count FROM " + TABLE_NAME + " WHERE id = ?";
    
    const std::string FIND_BY_NAME = 
        "SELECT id, name, system_type, description, template_json, is_active, "
        "created_at, updated_at FROM " + TABLE_NAME + " WHERE name = ?";
    
    const std::string FIND_BY_SYSTEM_TYPE = 
        "SELECT id, name, system_type, description, template_json, is_active, "
        "created_at, updated_at FROM " + TABLE_NAME + " WHERE system_type = ?";
    
    const std::string FIND_ACTIVE = 
        "SELECT id, name, system_type, description, template_json, is_active, "
        "created_at, updated_at FROM " + TABLE_NAME + " WHERE is_active = 1";
    
    const std::string CREATE_TABLE =
        "CREATE TABLE IF NOT EXISTS " + TABLE_NAME + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL UNIQUE, "
        "system_type TEXT NOT NULL, "
        "description TEXT, "
        "template_json TEXT NOT NULL, "
        "is_active INTEGER DEFAULT 1, "
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP, "
        "updated_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")";
}

// =============================================================================
// IRepository 기본 인터페이스 구현
// =============================================================================

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findAll() {
    std::vector<PayloadTemplateEntity> result;
    
    try {
        if (!ensureTableExists()) {
            return result;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_ALL);
        
        for (const auto& row : results) {
            result.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Info("PayloadTemplateRepository::findAll - Found " + 
                         std::to_string(result.size()) + " templates");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findAll failed: " + std::string(e.what()));
        }
    }
    
    return result;
}

std::optional<PayloadTemplateEntity> PayloadTemplateRepository::findById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return std::nullopt;
        }
        
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                return cached;
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ RepositoryHelpers 사용하여 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool PayloadTemplateRepository::save(PayloadTemplateEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // ✅ RepositoryHelpers::replaceParametersInOrder 사용
        std::string query = RepositoryHelpers::replaceParametersInOrder(INSERT, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // ✅ SQLite에서 마지막 삽입 ID 가져오기
            auto id_results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoll(id_results[0].at("id")));
            }
            
            if (logger_) {
                logger_->Info("PayloadTemplateRepository::save - Saved template: " + 
                             entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::update(const PayloadTemplateEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId());
        
        // ✅ RepositoryHelpers::replaceParametersInOrder 사용
        std::string query = RepositoryHelpers::replaceParametersInOrder(UPDATE, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(entity.getId());
            }
            
            if (logger_) {
                logger_->Info("PayloadTemplateRepository::update - Updated template ID: " + 
                             std::to_string(entity.getId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ RepositoryHelpers 사용하여 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            if (logger_) {
                logger_->Info("PayloadTemplateRepository::deleteById - Deleted template ID: " + 
                             std::to_string(id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::exists(int id) {
    try {
        if (id <= 0) {
            return false;
        }
        
        // 캐시 확인
        if (isCacheEnabled() && getCachedEntity(id).has_value()) {
            return true;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ RepositoryHelpers 사용
        std::string query = RepositoryHelpers::replaceParameter(EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산 (IRepository 인터페이스 구현)
// ✅ 메소드명 수정: bulkSave->saveBulk, bulkUpdate->updateBulk, bulkDelete->deleteByIds
// ✅ 반환 타입: bool -> int
// =============================================================================

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findByIds(const std::vector<int>& ids) {
    std::vector<PayloadTemplateEntity> results;
    results.reserve(ids.size());
    
    for (int id : ids) {
        auto entity = findById(id);
        if (entity.has_value()) {
            results.push_back(entity.value());
        }
    }
    
    return results;
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // TODO: 조건부 쿼리 구현
    if (logger_) {
        logger_->Warn("PayloadTemplateRepository::findByConditions - Not implemented yet");
    }
    return {};
}

int PayloadTemplateRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // TODO: 조건부 카운트 구현
    if (logger_) {
        logger_->Warn("PayloadTemplateRepository::countByConditions - Not implemented yet");
    }
    return 0;
}

int PayloadTemplateRepository::saveBulk(std::vector<PayloadTemplateEntity>& entities) {
    int saved_count = 0;
    
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::saveBulk - Saved " + 
                     std::to_string(saved_count) + " templates");
    }
    
    return saved_count;
}

int PayloadTemplateRepository::updateBulk(const std::vector<PayloadTemplateEntity>& entities) {
    int updated_count = 0;
    
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::updateBulk - Updated " + 
                     std::to_string(updated_count) + " templates");
    }
    
    return updated_count;
}

int PayloadTemplateRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::deleteByIds - Deleted " + 
                     std::to_string(deleted_count) + " templates");
    }
    
    return deleted_count;
}

// =============================================================================
// 커스텀 쿼리 메소드들
// =============================================================================

std::optional<PayloadTemplateEntity> PayloadTemplateRepository::findByName(const std::string& name) {
    try {
        if (name.empty() || !ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ RepositoryHelpers 사용 (문자열은 따옴표 필요)
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(FIND_BY_NAME, name);
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findByName failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findBySystemType(
    const std::string& system_type) {
    
    std::vector<PayloadTemplateEntity> result;
    
    try {
        if (system_type.empty() || !ensureTableExists()) {
            return result;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ RepositoryHelpers 사용
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            FIND_BY_SYSTEM_TYPE, system_type);
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            result.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Info("PayloadTemplateRepository::findBySystemType - Found " + 
                         std::to_string(result.size()) + " templates");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findBySystemType failed: " + 
                          std::string(e.what()));
        }
    }
    
    return result;
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findActive() {
    std::vector<PayloadTemplateEntity> result;
    
    try {
        if (!ensureTableExists()) {
            return result;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_ACTIVE);
        
        for (const auto& row : results) {
            result.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Info("PayloadTemplateRepository::findActive - Found " + 
                         std::to_string(result.size()) + " active templates");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findActive failed: " + 
                          std::string(e.what()));
        }
    }
    
    return result;
}

// =============================================================================
// 헬퍼 메소드들
// =============================================================================

bool PayloadTemplateRepository::ensureTableExists() {
    static bool table_checked = false;
    
    if (table_checked) {
        return true;
    }
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        if (!db_layer.executeCreateTable(CREATE_TABLE)) {
            if (logger_) {
                logger_->Error("PayloadTemplateRepository - Failed to create table");
            }
            return false;
        }
        
        table_checked = true;
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::ensureTableExists failed: " + 
                          std::string(e.what()));
        }
        return false;
    }
}

PayloadTemplateEntity PayloadTemplateRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    PayloadTemplateEntity entity;
    
    if (row.find("id") != row.end()) {
        entity.setId(std::stoll(row.at("id")));
    }
    if (row.find("name") != row.end()) {
        entity.setName(row.at("name"));
    }
    if (row.find("system_type") != row.end()) {
        entity.setSystemType(row.at("system_type"));
    }
    if (row.find("description") != row.end()) {
        entity.setDescription(row.at("description"));
    }
    if (row.find("template_json") != row.end()) {
        entity.setTemplateJson(row.at("template_json"));
    }
    if (row.find("is_active") != row.end()) {
        entity.setActive(row.at("is_active") == "1" || row.at("is_active") == "true");
    }
    // created_at, updated_at은 BaseEntity에서 자동 관리되므로 설정하지 않음
    
    return entity;
}

std::map<std::string, std::string> PayloadTemplateRepository::entityToParams(
    const PayloadTemplateEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    params["name"] = entity.getName();
    params["system_type"] = entity.getSystemType();
    params["description"] = entity.getDescription();
    params["template_json"] = entity.getTemplateJson();
    params["is_active"] = entity.isActive() ? "1" : "0";
    
    return params;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne