// =============================================================================
// collector/src/Database/Repositories/ScriptLibraryRepository.cpp
// PulseOne ScriptLibraryRepository 구현 - DeviceRepository 패턴 100% 적용
// =============================================================================

/**
 * @file ScriptLibraryRepository.cpp
 * @brief PulseOne ScriptLibraryRepository 완전 구현 - DeviceRepository 패턴 준수
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - ExtendedSQLQueries.h 사용 (분리된 쿼리 파일)
 * - DatabaseAbstractionLayer 패턴
 * - RepositoryHelpers 활용
 * - 벌크 연산 SQL 최적화
 * - 캐시 관리 완전 구현
 * - 모든 IRepository 메서드 override
 */

#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExtendedSQLQueries.h"  // 🔥 새로운 분리된 쿼리 파일
#include "Utils/LogManager.h"
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
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR, 
                                        "findAll - Table creation failed");
            return {};  // 🔥 수정: 올바른 빈 벡터 반환
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        auto results = db_layer.executeQuery(SQL::ScriptLibrary::FIND_ALL);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findAll - Found " + std::to_string(entities.size()) + " scripts");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ScriptLibraryEntity> ScriptLibraryRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                            "findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                        "findById - Script not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                    "findById - Found script: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ScriptLibraryRepository::save(ScriptLibraryEntity& entity) {
    try {
        if (!entity.isValid()) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "save - Invalid script entity");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 매개변수 배열 생성 (ExtendedSQLQueries.h INSERT 순서와 일치)
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
            nlohmann::json(entity.getDependencies()).dump()
        };
        
        // 🎯 RepositoryHelpers를 사용한 파라미터 치환
        std::string query = SQL::ScriptLibrary::INSERT;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 마지막 삽입 ID 가져오기
            auto id_results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!id_results.empty()) {
                entity.setId(std::stoi(id_results[0].at("id")));
                entity.markSaved();
                
                // 캐시에 저장
                if (isCacheEnabled()) {
                    cacheEntity(entity);
                }
                
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                            "save - Script saved: " + entity.getName());
            }
        } else {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "save - Failed to save script: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "save failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::update(const ScriptLibraryEntity& entity) {
    try {
        if (entity.getId() <= 0 || !entity.isValid()) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "update - Invalid script entity");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 UPDATE 매개변수 배열 (ExtendedSQLQueries.h UPDATE_BY_ID 순서와 일치)
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
            
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                        "update - Script updated: " + entity.getName());
        } else {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "update - Failed to update script: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "update failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::deleteById(int id) {
    try {
        if (id <= 0) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "deleteById - Invalid ID");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                        "deleteById - Script deleted: " + std::to_string(id));
        } else {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                        "deleteById - Script not found: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "deleteById failed: " + std::string(e.what()));
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
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        return !results.empty() && std::stoi(results[0].at("count")) > 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산 (SQL 최적화된 구현)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByIds(const std::vector<int>& ids) {
    std::vector<ScriptLibraryEntity> results;
    
    if (ids.empty()) {
        return results;
    }
    
    try {
        if (!ensureTableExists()) {
            return results;
        }
        
        // 🎯 SQL IN 절로 한 번에 조회 (성능 최적화)
        std::stringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ",";
            ids_ss << ids[i];
        }
        
        // 🎯 기본 쿼리에 WHERE 절 추가
        std::string query = SQL::ScriptLibrary::FIND_ALL;
        // ORDER BY 앞에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
        }
        
        DatabaseAbstractionLayer db_layer;
        auto query_results = db_layer.executeQuery(query);
        
        results.reserve(query_results.size());
        
        for (const auto& row : query_results) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findByIds - Found " + std::to_string(results.size()) + 
                                    " scripts for " + std::to_string(ids.size()) + " IDs");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByIds failed: " + std::string(e.what()));
    }
    
    return results;
}

int ScriptLibraryRepository::saveBulk(std::vector<ScriptLibraryEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    int saved_count = 0;
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 트랜잭션으로 배치 처리
        db_layer.executeNonQuery("BEGIN TRANSACTION");
        
        for (auto& entity : entities) {
            if (save(entity)) {
                saved_count++;
            }
        }
        
        db_layer.executeNonQuery("COMMIT");
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "saveBulk - Saved " + std::to_string(saved_count) + 
                                    " out of " + std::to_string(entities.size()) + " scripts");
        
    } catch (const std::exception& e) {
        // 롤백
        try {
            DatabaseAbstractionLayer db_layer;
            db_layer.executeNonQuery("ROLLBACK");
        } catch (...) {}
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "saveBulk failed: " + std::string(e.what()));
    }
    
    return saved_count;
}

int ScriptLibraryRepository::updateBulk(const std::vector<ScriptLibraryEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    int updated_count = 0;
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 트랜잭션으로 배치 처리
        db_layer.executeNonQuery("BEGIN TRANSACTION");
        
        for (const auto& entity : entities) {
            if (update(entity)) {
                updated_count++;
            }
        }
        
        db_layer.executeNonQuery("COMMIT");
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "updateBulk - Updated " + std::to_string(updated_count) + 
                                    " out of " + std::to_string(entities.size()) + " scripts");
        
    } catch (const std::exception& e) {
        // 롤백
        try {
            DatabaseAbstractionLayer db_layer;
            db_layer.executeNonQuery("ROLLBACK");
        } catch (...) {}
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "updateBulk failed: " + std::string(e.what()));
    }
    
    return updated_count;
}

int ScriptLibraryRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return 0;
    }
    
    int deleted_count = 0;
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 SQL IN 절로 한 번에 삭제 (성능 최적화)
        std::stringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ",";
            ids_ss << ids[i];
        }
        
        std::string query = "DELETE FROM script_library WHERE id IN (" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 영향받은 행 수 확인 (SQLite에서는 changes() 사용)
            auto result = db_layer.executeQuery("SELECT changes() as deleted_count");
            if (!result.empty()) {
                deleted_count = std::stoi(result[0].at("deleted_count"));
            }
            
            // 캐시에서 제거
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                        "deleteByIds - Deleted " + std::to_string(deleted_count) + " scripts");
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "deleteByIds failed: " + std::string(e.what()));
    }
    
    return deleted_count;
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    std::vector<ScriptLibraryEntity> results;
    
    try {
        if (!ensureTableExists()) {
            return results;
        }
        
        // 🎯 기본 쿼리 사용 후 조건 추가 (RepositoryHelpers 활용)
        std::string query = SQL::ScriptLibrary::FIND_ALL;
        
        // ORDER BY 제거 후 조건 추가
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query = query.substr(0, order_pos);
        }
        
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto query_results = db_layer.executeQuery(query);
        
        results.reserve(query_results.size());
        
        for (const auto& row : query_results) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                    "findByConditions - Found " + std::to_string(results.size()) + " scripts");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByConditions failed: " + std::string(e.what()));
    }
    
    return results;
}

int ScriptLibraryRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = SQL::ScriptLibrary::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// ScriptLibrary 전용 메서드들 (ExtendedSQLQueries.h 사용)
// =============================================================================

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByCategory(const std::string& category) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(SQL::ScriptLibrary::FIND_BY_CATEGORY, category);
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findByCategory - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findByCategory - Found " + std::to_string(entities.size()) + 
                                    " scripts in category: " + category);
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByCategory failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByTenantId(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_BY_TENANT_ID, std::to_string(tenant_id));
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findByTenantId - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findByTenantId - Found " + std::to_string(entities.size()) + 
                                    " scripts for tenant: " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByTenantId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findSystemScripts() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::ScriptLibrary::FIND_SYSTEM_SCRIPTS);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findSystemScripts - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findSystemScripts - Found " + std::to_string(entities.size()) + " system scripts");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findSystemScripts failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findTopUsed(int limit) {
    try {
        if (!ensureTableExists() || limit <= 0) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::FIND_TOP_USED, std::to_string(limit));
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findTopUsed - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findTopUsed - Found " + std::to_string(entities.size()) + " top used scripts");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findTopUsed failed: " + std::string(e.what()));
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
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::ScriptLibrary::INCREMENT_USAGE_COUNT, std::to_string(script_id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(script_id);
            }
            
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                        "incrementUsageCount - Updated count for script: " + std::to_string(script_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "incrementUsageCount failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

bool ScriptLibraryRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 상수 사용
        bool success = db_layer.executeCreateTable(SQL::ScriptLibrary::CREATE_TABLE);
        
        if (success) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                        "ensureTableExists - Table creation/check completed");
        } else {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

ScriptLibraryEntity ScriptLibraryRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    ScriptLibraryEntity entity;
    
    try {
        // 기본 필드
        entity.setId(std::stoi(row.at("id")));
        entity.setTenantId(std::stoi(row.at("tenant_id")));
        entity.setName(row.at("name"));
        entity.setDisplayName(row.at("display_name"));
        entity.setDescription(row.at("description"));
        entity.setScriptCode(row.at("script_code"));
        entity.setExampleUsage(row.at("example_usage"));
        entity.setIsSystem(row.at("is_system") == "1");
        entity.setIsTemplate(row.at("is_template") == "1");
        entity.setUsageCount(std::stoi(row.at("usage_count")));
        entity.setRating(std::stod(row.at("rating")));
        entity.setVersion(row.at("version"));
        entity.setAuthor(row.at("author"));
        entity.setLicense(row.at("license"));
        
        // JSON 필드들
        try {
            auto params = nlohmann::json::parse(row.at("parameters"));
            entity.setParameters(params);
        } catch (...) {
            entity.setParameters(nlohmann::json::object());
        }
        
        try {
            auto tags = nlohmann::json::parse(row.at("tags"));
            entity.setTags(tags.get<std::vector<std::string>>());
        } catch (...) {
            entity.setTags({});
        }
        
        try {
            auto deps = nlohmann::json::parse(row.at("dependencies"));
            entity.setDependencies(deps.get<std::vector<std::string>>());
        } catch (...) {
            entity.setDependencies({});
        }
        
        // Enum 필드들 (문자열에서 enum으로 변환)
        std::string category_str = row.at("category");
        if (category_str == "FUNCTION") entity.setCategory(ScriptLibraryEntity::Category::FUNCTION);
        else if (category_str == "FORMULA") entity.setCategory(ScriptLibraryEntity::Category::FORMULA);
        else if (category_str == "TEMPLATE") entity.setCategory(ScriptLibraryEntity::Category::TEMPLATE);
        else entity.setCategory(ScriptLibraryEntity::Category::CUSTOM);
        
        std::string return_type_str = row.at("return_type");
        if (return_type_str == "FLOAT") entity.setReturnType(ScriptLibraryEntity::ReturnType::FLOAT);
        else if (return_type_str == "STRING") entity.setReturnType(ScriptLibraryEntity::ReturnType::STRING);
        else if (return_type_str == "BOOLEAN") entity.setReturnType(ScriptLibraryEntity::ReturnType::BOOLEAN);
        else if (return_type_str == "OBJECT") entity.setReturnType(ScriptLibraryEntity::ReturnType::OBJECT);
        else entity.setReturnType(ScriptLibraryEntity::ReturnType::FLOAT);
        
        entity.markSaved(); // 데이터베이스에서 로드됨을 표시
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "mapRowToEntity failed: " + std::string(e.what()));
        throw; // 상위로 전파
    }
    
    return entity;
}

// =============================================================================
// 헤더에 선언된 추가 메서드들 구현
// =============================================================================

std::optional<ScriptLibraryEntity> ScriptLibraryRepository::findByName(int tenant_id, const std::string& name) {
    try {
        if (name.empty()) {
            return std::nullopt;
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 패턴 (두 개 매개변수)
        std::vector<std::string> params = {std::to_string(tenant_id), name};
        std::string query = SQL::ScriptLibrary::FIND_BY_NAME;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByName failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::findByTags(const std::vector<std::string>& tags) {
    if (tags.empty()) {
        return {};
    }
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 태그 검색을 위한 LIKE 조건 생성
        std::string where_clause = " WHERE (";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) where_clause += " OR ";
            where_clause += "tags LIKE '%" + tags[i] + "%'";
        }
        where_clause += ")";
        
        std::string query = SQL::ScriptLibrary::FIND_ALL;
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "findByTags - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "findByTags - Found " + std::to_string(entities.size()) + " scripts");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "findByTags failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ScriptLibraryEntity> ScriptLibraryRepository::search(const std::string& keyword) {
    if (keyword.empty()) {
        return {};
    }
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 키워드 검색 (이름, 설명, 태그에서 검색)
        std::string search_pattern = "%" + keyword + "%";
        std::string where_clause = " WHERE (name LIKE '" + search_pattern + 
                                  "' OR display_name LIKE '" + search_pattern + 
                                  "' OR description LIKE '" + search_pattern + 
                                  "' OR tags LIKE '" + search_pattern + "')";
        
        std::string query = SQL::ScriptLibrary::FIND_ALL;
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "search - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "search - Found " + std::to_string(entities.size()) + 
                                    " scripts for keyword: " + keyword);
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "search failed: " + std::string(e.what()));
        return {};
    }
}

bool ScriptLibraryRepository::recordUsage(int script_id, int virtual_point_id, int tenant_id, const std::string& context) {
    try {
        if (script_id <= 0 || virtual_point_id <= 0) {
            return false;
        }
        
        // 사용 횟수 증가
        incrementUsageCount(script_id);
        
        // 사용 이력 로깅 (실제 테이블이 있다면 여기에 INSERT)
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                    "recordUsage - Script " + std::to_string(script_id) + 
                                    " used by VP " + std::to_string(virtual_point_id) + 
                                    " in context: " + context);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "recordUsage failed: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryRepository::saveVersion(int script_id, const std::string& version, 
                                         const std::string& code, const std::string& change_log) {
    try {
        if (script_id <= 0 || version.empty() || code.empty()) {
            return false;
        }
        
        // 기본 구현: 스크립트 코드와 버전 업데이트
        auto script_opt = findById(script_id);
        if (!script_opt) {
            return false;
        }
        
        auto script = script_opt.value();
        script.setVersion(version);
        script.setScriptCode(code);
        
        bool success = update(script);
        
        if (success) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                        "saveVersion - Script " + std::to_string(script_id) + 
                                        " updated to version: " + version);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "saveVersion failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::map<std::string, std::string>> ScriptLibraryRepository::getTemplates(const std::string& category) {
    std::vector<std::map<std::string, std::string>> templates;
    
    try {
        if (!ensureTableExists()) {
            return templates;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query;
        if (category.empty()) {
            query = SQL::ScriptLibrary::FIND_TEMPLATES;
        } else {
            query = RepositoryHelpers::replaceParameterWithQuotes(
                SQL::ScriptLibrary::FIND_TEMPLATES_BY_CATEGORY, category);
        }
        
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            std::map<std::string, std::string> template_info;
            template_info["id"] = row.at("id");
            template_info["name"] = row.at("name");
            template_info["display_name"] = row.at("display_name");
            template_info["description"] = row.at("description");
            template_info["category"] = row.at("category");
            template_info["script_code"] = row.at("script_code");
            template_info["parameters"] = row.at("parameters");
            template_info["return_type"] = row.at("return_type");
            template_info["example_usage"] = row.at("example_usage");
            
            templates.push_back(template_info);
        }
        
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "getTemplates - Found " + std::to_string(templates.size()) + " templates");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "getTemplates failed: " + std::string(e.what()));
    }
    
    return templates;
}

std::optional<std::map<std::string, std::string>> ScriptLibraryRepository::getTemplateById(int template_id) {
    try {
        if (template_id <= 0) {
            return std::nullopt;
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = RepositoryHelpers::replaceParameter(
        SQL::ScriptLibrary::FIND_TEMPLATE_BY_ID, std::to_string(template_id));
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        const auto& row = results[0];
        std::map<std::string, std::string> template_info;
        template_info["id"] = row.at("id");
        template_info["name"] = row.at("name");
        template_info["display_name"] = row.at("display_name");
        template_info["description"] = row.at("description");
        template_info["category"] = row.at("category");
        template_info["script_code"] = row.at("script_code");
        template_info["parameters"] = row.at("parameters");
        template_info["return_type"] = row.at("return_type");
        template_info["example_usage"] = row.at("example_usage");
        
        return template_info;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                    "getTemplateById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

nlohmann::json ScriptLibraryRepository::getUsageStatistics(int tenant_id) {
   nlohmann::json stats = nlohmann::json::object();
   
   try {
       if (!ensureTableExists()) {
           return stats;
       }
       
       DatabaseAbstractionLayer db_layer;
       
       // 🎯 ExtendedSQLQueries.h 사용 - 총 스크립트 수
       std::string count_query;
       if (tenant_id > 0) {
           count_query = RepositoryHelpers::replaceParameter(
               SQL::ScriptLibrary::COUNT_BY_TENANT, std::to_string(tenant_id));
       } else {
           count_query = SQL::ScriptLibrary::COUNT_ALL_SCRIPTS;
       }
       
       auto count_results = db_layer.executeQuery(count_query);
       if (!count_results.empty()) {
           stats["total_scripts"] = std::stoi(count_results[0].at("total_scripts"));
       }
       
       // 🎯 ExtendedSQLQueries.h 사용 - 카테고리별 통계
       std::string category_query;
       if (tenant_id > 0) {
           category_query = RepositoryHelpers::replaceParameter(
               SQL::ScriptLibrary::GROUP_BY_CATEGORY_AND_TENANT, std::to_string(tenant_id));
       } else {
           category_query = SQL::ScriptLibrary::GROUP_BY_CATEGORY;
       }
       
       auto category_results = db_layer.executeQuery(category_query);
       nlohmann::json category_stats = nlohmann::json::object();
       for (const auto& row : category_results) {
           category_stats[row.at("category")] = std::stoi(row.at("count"));
       }
       stats["by_category"] = category_stats;
       
       // 🎯 ExtendedSQLQueries.h 사용 - 사용량 통계
       std::string usage_query;
       if (tenant_id > 0) {
           usage_query = RepositoryHelpers::replaceParameter(
               SQL::ScriptLibrary::USAGE_STATISTICS_BY_TENANT, std::to_string(tenant_id));
       } else {
           usage_query = SQL::ScriptLibrary::USAGE_STATISTICS;
       }
       
       auto usage_results = db_layer.executeQuery(usage_query);
       if (!usage_results.empty()) {
           stats["total_usage"] = std::stoi(usage_results[0].at("total_usage"));
           stats["average_usage"] = std::stod(usage_results[0].at("avg_usage"));
       }
       
       LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::DEBUG,
                                   "getUsageStatistics - Generated statistics for tenant: " + std::to_string(tenant_id));
       
   } catch (const std::exception& e) {
       LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                   "getUsageStatistics failed: " + std::string(e.what()));
       stats["error"] = e.what();
   }
   
   return stats;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne