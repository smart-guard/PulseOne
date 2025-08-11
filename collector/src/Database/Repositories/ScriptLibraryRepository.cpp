// =============================================================================
// collector/src/Database/Repositories/ScriptLibraryRepository.cpp
// PulseOne ScriptLibraryRepository 구현 - SQLQueries.h 패턴 100% 적용
// =============================================================================

/**
 * @file ScriptLibraryRepository.cpp
 * @brief PulseOne ScriptLibraryRepository 구현 - SQLQueries.h 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 SQLQueries.h 패턴 완전 적용:
 * - 모든 쿼리는 SQLQueries.h에서 관리
 * - RepositoryHelpers::replaceParameter 사용
 * - DeviceRepository와 동일한 패턴
 * - 인라인 쿼리 완전 제거
 */

#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/SQLQueries.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (SQLQueries.h 패턴)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ScriptLibraryRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ScriptLibrary::FIND_ALL);
        
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_ID, std::to_string(id));
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
        
        // 🎯 executeUpsert 사용 (SQLQueries.h 패턴)
        std::vector<std::string> primary_keys = {"id"};
        bool success = db_layer.executeUpsert("script_library", params, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회
            if (entity.getId() <= 0) {
                auto id_result = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
                if (!id_result.empty()) {
                    entity.setId(std::stoi(id_result[0].at("id")));
                }
            }
            
            entity.markSaved();
            
            // 캐시에 추가
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("ScriptLibraryRepository::save - Script saved with ID: " + std::to_string(entity.getId()));
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
        
        // 🎯 SQLQueries.h 상수 사용 + RepositoryHelpers 파라미터 치환
        std::vector<std::string> params = {
            std::to_string(entity.getTenantId()),
            entity.getName(),
            entity.getDisplayName(),
            entity.getDescription(),
            entity.getCategoryString(),
            entity.getScriptCode(),
            entity.getParameters().dump(),
            entity.getReturnTypeString(),
            nlohmann::json(entity.getTags()).dump(),
            entity.getExampleUsage(),
            entity.isSystem() ? "1" : "0",
            entity.isTemplate() ? "1" : "0",
            std::to_string(entity.getUsageCount()),
            std::to_string(entity.getRating()),
            entity.getVersion(),
            entity.getAuthor(),
            entity.getLicense(),
            nlohmann::json(entity.getDependencies()).dump(),
            std::to_string(entity.getId()) // WHERE 절의 id
        };
        
        std::string query = SQL::ScriptLibrary::UPDATE_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::EXISTS_BY_ID, std::to_string(id));
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
// 특화 메서드들 (SQLQueries.h 패턴)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByCategory(const std::string& category) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_CATEGORY, 
                                                              RepositoryHelpers::escapeString(category));
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_TENANT_ID, 
                                                              std::to_string(tenant_id));
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
        
        // 🎯 SQLQueries.h 상수 사용 + 두 개 파라미터 치환
        std::string query = RepositoryHelpers::replaceTwoParameters(SQL::ScriptLibrary::FIND_BY_NAME,
                                                                  std::to_string(tenant_id),
                                                                  RepositoryHelpers::escapeString(name));
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
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::ScriptLibrary::FIND_SYSTEM_SCRIPTS);
        
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
        
        // 태그 조건 생성 (동적)
        std::string tag_conditions;
        for (const auto& tag : tags) {
            if (!tag_conditions.empty()) {
                tag_conditions += " AND ";
            }
            tag_conditions += "tags LIKE '%" + RepositoryHelpers::escapeString(tag) + "%'";
        }
        
        // 🎯 SQLQueries.h 상수 사용하되 동적 조건 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_TAGS, tag_conditions);
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
        
        std::string escaped_keyword = "%" + RepositoryHelpers::escapeString(keyword) + "%";
        
        // 🎯 SQLQueries.h 상수 사용 + 네 개 파라미터 치환 (LIKE 조건들)
        std::vector<std::string> params = {escaped_keyword, escaped_keyword, escaped_keyword, escaped_keyword};
        std::string query = SQL::ScriptLibrary::SEARCH_SCRIPTS;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_TOP_USED, 
                                                              std::to_string(limit));
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::INCREMENT_USAGE_COUNT, 
                                                              std::to_string(script_id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(script_id);
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 세 개 파라미터 치환
        std::vector<std::string> params = {
            RepositoryHelpers::escapeString(version),
            RepositoryHelpers::escapeString(code),
            std::to_string(script_id)
        };
        
        std::string query = SQL::ScriptLibrary::UPDATE_VERSION;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(script_id);
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
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (category.empty()) {
            // 🎯 SQLQueries.h 상수 사용
            results = db_layer.executeQuery(SQL::ScriptLibrary::FIND_TEMPLATES);
        } else {
            // 🎯 SQLQueries.h 상수 사용 + 카테고리 파라미터
            std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_TEMPLATES_BY_CATEGORY,
                                                                  RepositoryHelpers::escapeString(category));
            results = db_layer.executeQuery(query);
        }
        
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_TEMPLATE_BY_ID,
                                                              std::to_string(template_id));
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::vector<std::map<std::string, std::string>> results;
        
        if (tenant_id > 0) {
            std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::GET_USAGE_STATS_BY_TENANT,
                                                                  std::to_string(tenant_id));
            results = db_layer.executeQuery(query);
        } else {
            results = db_layer.executeQuery(SQL::ScriptLibrary::GET_USAGE_STATS);
        }
        
        if (!results.empty()) {
            stats["total_scripts"] = std::stoi(results[0].at("total_scripts"));
            stats["total_usage"] = results[0].at("total_usage").empty() ? 0 : std::stoi(results[0].at("total_usage"));
            stats["avg_rating"] = results[0].at("avg_rating").empty() ? 0.0 : std::stod(results[0].at("avg_rating"));
            stats["max_usage"] = results[0].at("max_usage").empty() ? 0 : std::stoi(results[0].at("max_usage"));
        }
        
        // 카테고리별 통계
        if (tenant_id > 0) {
            std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::GET_CATEGORY_STATS_BY_TENANT,
                                                                  std::to_string(tenant_id));
            results = db_layer.executeQuery(query);
        } else {
            results = db_layer.executeQuery(SQL::ScriptLibrary::GET_CATEGORY_STATS);
        }
        
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
// 내부 헬퍼 메서드 (기존과 동일)
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
        
        entity.markSaved();
        
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
    
    return params;
}

bool ScriptLibraryRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        bool success = db_layer.executeCreateTable(SQL::ScriptLibrary::CREATE_TABLE);
        
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