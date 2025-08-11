// =============================================================================
// collector/src/Database/Repositories/ScriptLibraryRepository.cpp
// PulseOne ScriptLibraryRepository 구현 - DeviceRepository 패턴 100% 적용
// =============================================================================

/**
 * @file ScriptLibraryRepository.cpp
 * @brief PulseOne ScriptLibraryRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - RepositoryHelpers 활용
 * - 캐시 관리 구현
 */

#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (DeviceRepository 패턴)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ScriptLibraryRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ScriptLibraryRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("ScriptLibraryRepository::findAll - Found " + std::to_string(entities.size()) + " scripts");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ScriptLibraryEntity> ScriptLibraryRepository::findById(int id) {
    try {
        // 캐시 확인 먼저
        if (isCacheEnabled()) {
            if (auto cached = getCachedEntity(id)) {
                logger_->Debug("ScriptLibraryRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached;
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE id = )" + std::to_string(id);
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("ScriptLibraryRepository::findById - Script not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("ScriptLibraryRepository::findById - Found script: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ScriptLibraryRepository::save(ScriptLibraryEntity& entity) {
    try {
        if (!validateEntity(entity)) {
            logger_->Error("ScriptLibraryRepository::save - Validation failed");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // Insert 쿼리 생성
        std::string columns, values;
        for (const auto& [key, value] : params) {
            if (key == "id") continue;  // ID는 자동 생성
            
            if (!columns.empty()) {
                columns += ", ";
                values += ", ";
            }
            columns += key;
            
            if (value == "NULL") {
                values += value;
            } else {
                values += "'" + RepositoryHelpers::escapeString(value) + "'";
            }
        }
        
        const std::string query = "INSERT INTO script_library (" + columns + ") VALUES (" + values + ")";
        
        int new_id = db_layer.executeInsert(query);
        
        if (new_id > 0) {
            entity.setId(new_id);
            entity.markSaved();
            
            // 캐시에 추가
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("ScriptLibraryRepository::save - Script saved with ID: " + std::to_string(new_id));
            return true;
        }
        
        logger_->Error("ScriptLibraryRepository::save - Failed to save script");
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::update(const ScriptLibraryEntity& entity) {
    try {
        if (!validateEntity(entity)) {
            logger_->Error("ScriptLibraryRepository::update - Validation failed");
            return false;
        }
        
        if (entity.getId() <= 0) {
            logger_->Error("ScriptLibraryRepository::update - Invalid ID");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // Update 쿼리 생성
        std::string set_clause;
        for (const auto& [key, value] : params) {
            if (key == "id" || key == "created_at") continue;  // ID와 created_at은 업데이트 안함
            
            if (!set_clause.empty()) {
                set_clause += ", ";
            }
            
            if (value == "NULL") {
                set_clause += key + " = NULL";
            } else {
                set_clause += key + " = '" + RepositoryHelpers::escapeString(value) + "'";
            }
        }
        
        const std::string query = "UPDATE script_library SET " + set_clause + 
                                " WHERE id = " + std::to_string(entity.getId());
        
        bool success = db_layer.executeNonQuery(query) > 0;
        
        if (success) {
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("ScriptLibraryRepository::update - Script updated: " + entity.getName());
        } else {
            logger_->Error("ScriptLibraryRepository::update - Failed to update script: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::deleteById(int id) {
    try {
        if (id <= 0) {
            logger_->Error("ScriptLibraryRepository::deleteById - Invalid ID");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = "DELETE FROM script_library WHERE id = " + std::to_string(id);
        
        bool success = db_layer.executeNonQuery(query) > 0;
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                removeCachedEntity(id);
            }
            
            logger_->Info("ScriptLibraryRepository::deleteById - Script deleted: " + std::to_string(id));
        } else {
            logger_->Warn("ScriptLibraryRepository::deleteById - Script not found: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::exists(int id) {
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
        
        const std::string query = "SELECT COUNT(*) as count FROM script_library WHERE id = " + std::to_string(id);
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 특화 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByCategory(const std::string& category) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE category = ')" + RepositoryHelpers::escapeString(category) + "'" +
            " ORDER BY name";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::findByCategory - Found " + 
                     std::to_string(entities.size()) + " scripts for category: " + category);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findByCategory failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByTenantId(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE tenant_id = )" + std::to_string(tenant_id) + 
            " OR is_system = 1 ORDER BY name";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::findByTenantId - Found " + 
                     std::to_string(entities.size()) + " scripts for tenant: " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findByTenantId failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ScriptLibraryEntity> ScriptLibraryRepository::findByName(int tenant_id, const std::string& name) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE (tenant_id = )" + std::to_string(tenant_id) + 
            " OR is_system = 1) AND name = '" + RepositoryHelpers::escapeString(name) + "'";
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("ScriptLibraryRepository::findByName - Script not found: " + name);
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        logger_->Debug("ScriptLibraryRepository::findByName - Found script: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findByName failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findSystemScripts() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE is_system = 1
            ORDER BY category, name
        )";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::findSystemScripts - Found " + 
                     std::to_string(entities.size()) + " system scripts");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findSystemScripts failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByTags(const std::vector<std::string>& tags) {
    try {
        if (!ensureTableExists() || tags.empty()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 태그 조건 생성
        std::string tag_conditions;
        for (const auto& tag : tags) {
            if (!tag_conditions.empty()) {
                tag_conditions += " AND ";
            }
            tag_conditions += "tags LIKE '%" + RepositoryHelpers::escapeString(tag) + "%'";
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE )" + tag_conditions + " ORDER BY usage_count DESC, name";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::findByTags - Found " + 
                     std::to_string(entities.size()) + " scripts with tags");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findByTags failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::search(const std::string& keyword) {
    try {
        if (!ensureTableExists() || keyword.empty()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string escaped_keyword = RepositoryHelpers::escapeString(keyword);
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE name LIKE '%)" + escaped_keyword + "%'" +
            " OR display_name LIKE '%" + escaped_keyword + "%'" +
            " OR description LIKE '%" + escaped_keyword + "%'" +
            " OR tags LIKE '%" + escaped_keyword + "%'" +
            " ORDER BY usage_count DESC, name";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::search - Found " + 
                     std::to_string(entities.size()) + " scripts matching: " + keyword);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::search failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findTopUsed(int limit) {
    try {
        if (!ensureTableExists() || limit <= 0) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, name, display_name, description, category,
                script_code, parameters, return_type, tags, example_usage,
                is_system, is_template, usage_count, rating, version,
                author, license, dependencies, created_at, updated_at
            FROM script_library 
            WHERE usage_count > 0
            ORDER BY usage_count DESC, rating DESC
            LIMIT )" + std::to_string(limit);
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities = mapResultToEntities(results);
        
        logger_->Info("ScriptLibraryRepository::findTopUsed - Found " + 
                     std::to_string(entities.size()) + " top used scripts");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::findTopUsed failed: " + std::string(e.what()));
        return {};
    }
}

bool ScriptLibraryRepository::incrementUsageCount(int script_id) {
    try {
        if (script_id <= 0) {
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            UPDATE script_library 
            SET usage_count = usage_count + 1,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = )" + std::to_string(script_id);
        
        bool success = db_layer.executeNonQuery(query) > 0;
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                removeCachedEntity(script_id);
            }
            
            logger_->Debug("ScriptLibraryRepository::incrementUsageCount - Updated count for script: " + 
                          std::to_string(script_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::incrementUsageCount failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::recordUsage(int script_id, int virtual_point_id, int tenant_id, const std::string& context) {
    try {
        if (script_id <= 0) {
            return false;
        }
        
        // 사용 횟수 증가
        incrementUsageCount(script_id);
        
        // 사용 이력 테이블이 있다면 여기에 기록
        // 현재는 사용 횟수만 증가
        
        logger_->Debug("ScriptLibraryRepository::recordUsage - Recorded usage for script: " + 
                      std::to_string(script_id) + " by virtual point: " + std::to_string(virtual_point_id));
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::recordUsage failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::saveVersion(int script_id, const std::string& version, 
                                         const std::string& code, const std::string& change_log) {
    try {
        if (script_id <= 0) {
            return false;
        }
        
        // 버전 관리 테이블이 있다면 여기에 저장
        // 현재는 메인 테이블의 버전과 코드만 업데이트
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            UPDATE script_library 
            SET version = ')" + RepositoryHelpers::escapeString(version) + "', " +
            "script_code = '" + RepositoryHelpers::escapeString(code) + "', " +
            "updated_at = CURRENT_TIMESTAMP " +
            "WHERE id = " + std::to_string(script_id);
        
        bool success = db_layer.executeNonQuery(query) > 0;
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                removeCachedEntity(script_id);
            }
            
            logger_->Info("ScriptLibraryRepository::saveVersion - Saved version " + version + 
                         " for script: " + std::to_string(script_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::saveVersion failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::map<std::string, std::string>> ScriptLibraryRepository::getTemplates(const std::string& category) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = R"(
            SELECT 
                id, name, display_name, description, category,
                script_code, parameters, return_type, example_usage
            FROM script_library 
            WHERE is_template = 1)";
        
        if (!category.empty()) {
            query += " AND category = '" + RepositoryHelpers::escapeString(category) + "'";
        }
        
        query += " ORDER BY category, name";
        
        auto results = db_layer.executeQuery(query);
        
        logger_->Info("ScriptLibraryRepository::getTemplates - Found " + 
                     std::to_string(results.size()) + " templates");
        return results;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::getTemplates failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<std::map<std::string, std::string>> ScriptLibraryRepository::getTemplateById(int template_id) {
    try {
        if (template_id <= 0 || !ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        const std::string query = R"(
            SELECT 
                id, name, display_name, description, category,
                script_code, parameters, return_type, example_usage
            FROM script_library 
            WHERE is_template = 1 AND id = )" + std::to_string(template_id);
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("ScriptLibraryRepository::getTemplateById - Template not found: " + 
                          std::to_string(template_id));
            return std::nullopt;
        }
        
        logger_->Debug("ScriptLibraryRepository::getTemplateById - Found template: " + 
                      results[0].at("name"));
        return results[0];
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::getTemplateById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

nlohmann::json ScriptLibraryRepository::getUsageStatistics(int tenant_id) {
    try {
        nlohmann::json stats;
        
        if (!ensureTableExists()) {
            return stats;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 전체 통계
        std::string query = R"(
            SELECT 
                COUNT(*) as total_scripts,
                SUM(usage_count) as total_usage,
                AVG(rating) as avg_rating,
                MAX(usage_count) as max_usage
            FROM script_library)";
        
        if (tenant_id > 0) {
            query += " WHERE tenant_id = " + std::to_string(tenant_id) + " OR is_system = 1";
        }
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            stats["total_scripts"] = std::stoi(results[0].at("total_scripts"));
            stats["total_usage"] = results[0].at("total_usage").empty() ? 0 : std::stoi(results[0].at("total_usage"));
            stats["avg_rating"] = results[0].at("avg_rating").empty() ? 0.0 : std::stod(results[0].at("avg_rating"));
            stats["max_usage"] = results[0].at("max_usage").empty() ? 0 : std::stoi(results[0].at("max_usage"));
        }
        
        // 카테고리별 통계
        query = R"(
            SELECT 
                category,
                COUNT(*) as count,
                SUM(usage_count) as usage
            FROM script_library)";
        
        if (tenant_id > 0) {
            query += " WHERE tenant_id = " + std::to_string(tenant_id) + " OR is_system = 1";
        }
        
        query += " GROUP BY category";
        
        results = db_layer.executeQuery(query);
        
        nlohmann::json categories = nlohmann::json::array();
        for (const auto& row : results) {
            nlohmann::json cat;
            cat["category"] = row.at("category");
            cat["count"] = std::stoi(row.at("count"));
            cat["usage"] = row.at("usage").empty() ? 0 : std::stoi(row.at("usage"));
            categories.push_back(cat);
        }
        stats["categories"] = categories;
        
        logger_->Debug("ScriptLibraryRepository::getUsageStatistics - Generated statistics for tenant: " + 
                      std::to_string(tenant_id));
        return stats;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::getUsageStatistics failed: " + std::string(e.what()));
        return nlohmann::json::object();
    }
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

ScriptLibraryEntity ScriptLibraryRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    ScriptLibraryEntity entity;
    
    try {
        // 기본 필드
        if (row.find("id") != row.end() && !row.at("id").empty()) {
            entity.setId(std::stoi(row.at("id")));
        }
        
        if (row.find("tenant_id") != row.end() && !row.at("tenant_id").empty()) {
            entity.setTenantId(std::stoi(row.at("tenant_id")));
        }
        
        if (row.find("name") != row.end()) {
            entity.setName(row.at("name"));
        }
        
        if (row.find("display_name") != row.end()) {
            entity.setDisplayName(row.at("display_name"));
        }
        
        if (row.find("description") != row.end()) {
            entity.setDescription(row.at("description"));
        }
        
        // 카테고리 매핑
        if (row.find("category") != row.end() && !row.at("category").empty()) {
            std::string cat = row.at("category");
            if (cat == "FUNCTION" || cat == "0") {
                entity.setCategory(ScriptLibraryEntity::Category::FUNCTION);
            } else if (cat == "FORMULA" || cat == "1") {
                entity.setCategory(ScriptLibraryEntity::Category::FORMULA);
            } else if (cat == "TEMPLATE" || cat == "2") {
                entity.setCategory(ScriptLibraryEntity::Category::TEMPLATE);
            } else {
                entity.setCategory(ScriptLibraryEntity::Category::CUSTOM);
            }
        }
        
        if (row.find("script_code") != row.end()) {
            entity.setScriptCode(row.at("script_code"));
        }
        
        // JSON 필드 파싱
        if (row.find("parameters") != row.end() && !row.at("parameters").empty()) {
            try {
                entity.setParameters(nlohmann::json::parse(row.at("parameters")));
            } catch (...) {
                entity.setParameters(nlohmann::json::object());
            }
        }
        
        // 반환 타입 매핑
        if (row.find("return_type") != row.end() && !row.at("return_type").empty()) {
            std::string rt = row.at("return_type");
            if (rt == "FLOAT" || rt == "0") {
                entity.setReturnType(ScriptLibraryEntity::ReturnType::FLOAT);
            } else if (rt == "STRING" || rt == "1") {
                entity.setReturnType(ScriptLibraryEntity::ReturnType::STRING);
            } else if (rt == "BOOLEAN" || rt == "2") {
                entity.setReturnType(ScriptLibraryEntity::ReturnType::BOOLEAN);
            } else if (rt == "OBJECT" || rt == "3") {
                entity.setReturnType(ScriptLibraryEntity::ReturnType::OBJECT);
            }
        }
        
        // 태그 파싱
        if (row.find("tags") != row.end() && !row.at("tags").empty()) {
            try {
                auto tags_json = nlohmann::json::parse(row.at("tags"));
                if (tags_json.is_array()) {
                    entity.setTags(tags_json.get<std::vector<std::string>>());
                }
            } catch (...) {
                entity.setTags({});
            }
        }
        
        if (row.find("example_usage") != row.end()) {
            entity.setExampleUsage(row.at("example_usage"));
        }
        
        // Boolean 필드
        if (row.find("is_system") != row.end()) {
            entity.setIsSystem(row.at("is_system") == "1" || row.at("is_system") == "true");
        }
        
        if (row.find("is_template") != row.end()) {
            entity.setIsTemplate(row.at("is_template") == "1" || row.at("is_template") == "true");
        }
        
        // 숫자 필드
        if (row.find("usage_count") != row.end() && !row.at("usage_count").empty()) {
            entity.setUsageCount(std::stoi(row.at("usage_count")));
        }
        
        if (row.find("rating") != row.end() && !row.at("rating").empty()) {
            entity.setRating(std::stod(row.at("rating")));
        }
        
        if (row.find("version") != row.end()) {
            entity.setVersion(row.at("version"));
        }
        
        if (row.find("author") != row.end()) {
            entity.setAuthor(row.at("author"));
        }
        
        if (row.find("license") != row.end()) {
            entity.setLicense(row.at("license"));
        }
        
        // 의존성 파싱
        if (row.find("dependencies") != row.end() && !row.at("dependencies").empty()) {
            try {
                auto deps_json = nlohmann::json::parse(row.at("dependencies"));
                if (deps_json.is_array()) {
                    entity.setDependencies(deps_json.get<std::vector<std::string>>());
                }
            } catch (...) {
                entity.setDependencies({});
            }
        }
        
        // 타임스탬프
        if (row.find("created_at") != row.end() && !row.at("created_at").empty()) {
            entity.setCreatedAt(RepositoryHelpers::parseTimestamp(row.at("created_at")));
        }
        
        if (row.find("updated_at") != row.end() && !row.at("updated_at").empty()) {
            entity.setUpdatedAt(RepositoryHelpers::parseTimestamp(row.at("updated_at")));
        }
        
        entity.markLoaded();
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::mapRowToEntity failed: " + std::string(e.what()));
    }
    
    return entity;
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<ScriptLibraryEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("ScriptLibraryRepository::mapResultToEntities - Failed to map row: " + 
                         std::string(e.what()));
        }
    }
    
    return entities;
}

std::map<std::string, std::string> ScriptLibraryRepository::entityToParams(const ScriptLibraryEntity& entity) {
    std::map<std::string, std::string> params;
    
    DatabaseAbstractionLayer db_layer;
    
    // 기본 필드
    params["id"] = std::to_string(entity.getId());
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["name"] = entity.getName();
    params["display_name"] = entity.getDisplayName();
    params["description"] = entity.getDescription();
    
    // 카테고리 변환
    params["category"] = entity.getCategoryString();
    
    params["script_code"] = entity.getScriptCode();
    
    // JSON 필드 직렬화
    params["parameters"] = entity.getParameters().dump();
    
    // 반환 타입 변환
    params["return_type"] = entity.getReturnTypeString();
    
    // 태그 직렬화
    nlohmann::json tags_json = entity.getTags();
    params["tags"] = tags_json.dump();
    
    params["example_usage"] = entity.getExampleUsage();
    params["is_system"] = entity.isSystem() ? "1" : "0";
    params["is_template"] = entity.isTemplate() ? "1" : "0";
    params["usage_count"] = std::to_string(entity.getUsageCount());
    params["rating"] = std::to_string(entity.getRating());
    params["version"] = entity.getVersion();
    params["author"] = entity.getAuthor();
    params["license"] = entity.getLicense();
    
    // 의존성 직렬화
    nlohmann::json deps_json = entity.getDependencies();
    params["dependencies"] = deps_json.dump();
    
    // 타임스탬프
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool ScriptLibraryRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        const std::string create_table_query = R"(
            CREATE TABLE IF NOT EXISTS script_library (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                name VARCHAR(100) NOT NULL,
                display_name VARCHAR(100),
                description TEXT,
                category VARCHAR(50) DEFAULT 'CUSTOM',
                script_code TEXT NOT NULL,
                parameters TEXT,
                return_type VARCHAR(50) DEFAULT 'FLOAT',
                tags TEXT,
                example_usage TEXT,
                is_system INTEGER DEFAULT 0,
                is_template INTEGER DEFAULT 0,
                usage_count INTEGER DEFAULT 0,
                rating REAL DEFAULT 0.0,
                version VARCHAR(20) DEFAULT '1.0.0',
                author VARCHAR(100),
                license VARCHAR(100),
                dependencies TEXT,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                UNIQUE(tenant_id, name)
            )
        )";
        
        bool success = db_layer.executeCreateTable(create_table_query);
        
        if (success) {
            logger_->Debug("ScriptLibraryRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("ScriptLibraryRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ScriptLibraryRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::validateEntity(const ScriptLibraryEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("ScriptLibraryRepository::validateEntity - Invalid entity");
        return false;
    }
    
    if (!entity.validate()) {
        logger_->Warn("ScriptLibraryRepository::validateEntity - Entity validation failed");
        return false;
    }
    
    return true;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne