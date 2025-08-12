// =============================================================================
// collector/src/Database/Repositories/AlarmRuleRepository.cpp
// PulseOne AlarmRuleRepository 구현 - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmRuleRepository.cpp
 * @brief PulseOne AlarmRuleRepository - ScriptLibraryRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 ScriptLibraryRepository 패턴 100% 적용:
 * - ExtendedSQLQueries.h 사용 (분리된 쿼리 파일)
 * - DatabaseAbstractionLayer 패턴
 * - 표준 LogManager 사용법
 * - 벌크 연산 SQL 최적화
 * - 캐시 관리 완전 구현
 * - 모든 IRepository 메서드 override
 */

#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExtendedSQLQueries.h"  // 🔥 분리된 쿼리 파일 사용
#include "Database/SQLQueries.h"          // 🔥 SQL::Common 네임스페이스용
#include "Utils/LogManager.h"
#include "Alarm/AlarmTypes.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (ScriptLibraryRepository 패턴)
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR, 
                                        "findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        auto results = db_layer.executeQuery(SQL::Alarm::Rule::FIND_ALL);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                            "findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "findAll - Found " + std::to_string(entities.size()) + " alarm rules");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                            "findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::Alarm::Rule::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                        "findById - No alarm rule found for ID: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                    "findById - Found alarm rule ID: " + std::to_string(id));
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool AlarmRuleRepository::save(AlarmRuleEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmRule(entity)) {
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                        "save - Invalid alarm rule data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        auto params = entityToParams(entity);
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::Alarm::Rule::INSERT, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 새로 생성된 ID 조회
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoll(id_results[0].at("id")));
            }
            
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                        "save - Saved alarm rule: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "save failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmRuleRepository::update(const AlarmRuleEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmRule(entity)) {
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                        "update - Invalid alarm rule data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId()); // WHERE 절용
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::Alarm::Rule::UPDATE, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(entity.getId()));
            }
            
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                        "update - Updated alarm rule ID: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "update failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmRuleRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::Alarm::Rule::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                        "deleteById - Deleted alarm rule ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmRuleRepository::exists(int id) {
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
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::Alarm::Rule::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        return !results.empty() && std::stoi(results[0].at("count")) > 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산 (SQL 최적화된 구현) - ScriptLibraryRepository 패턴
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByIds(const std::vector<int>& ids) {
    std::vector<AlarmRuleEntity> results;
    
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
        std::string query = SQL::Alarm::Rule::FIND_ALL;
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
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                            "findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                    "findByIds - Found " + std::to_string(results.size()) + " alarm rules for " + std::to_string(ids.size()) + " IDs");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByIds failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    std::vector<AlarmRuleEntity> results;
    
    try {
        if (!ensureTableExists()) {
            return results;
        }
        
        // 🎯 RepositoryHelpers를 사용한 동적 쿼리 구성
        std::string query = SQL::Alarm::Rule::FIND_ALL;
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
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                            "findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                    "findByConditions - Found " + std::to_string(results.size()) + " alarm rules");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByConditions failed: " + std::string(e.what()));
    }
    
    return results;
}

int AlarmRuleRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 ExtendedSQLQueries.h 상수 + RepositoryHelpers 패턴
        std::string query = SQL::Alarm::Rule::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    // 🎯 첫 번째 결과만 필요하므로 LIMIT 1 적용
    auto pagination = Pagination{0, 1}; // offset=0, limit=1
    auto results = findByConditions(conditions, std::nullopt, pagination);
    
    if (!results.empty()) {
        return results[0];
    }
    
    return std::nullopt;
}

int AlarmRuleRepository::saveBulk(std::vector<AlarmRuleEntity>& entities) {
    // 🔥 TODO: 실제 배치 INSERT 구현 필요 (성능 최적화)
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    return saved_count;
}

int AlarmRuleRepository::updateBulk(const std::vector<AlarmRuleEntity>& entities) {
    // 🔥 TODO: 실제 배치 UPDATE 구현 필요 (성능 최적화)
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    return updated_count;
}

int AlarmRuleRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return 0;
    }
    
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
        
        std::string query = "DELETE FROM alarm_rules WHERE id IN (" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                        "deleteByIds - Deleted " + std::to_string(ids.size()) + " alarm rules");
            return static_cast<int>(ids.size());
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// AlarmRule 전용 메서드들 - ExtendedSQLQueries.h 사용
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByTarget(const std::string& target_type, int target_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::Alarm::Rule::FIND_BY_TARGET;
        
        // 🎯 RepositoryHelpers로 파라미터 치환
        query = RepositoryHelpers::replaceParameter(query, "'" + target_type + "'");
        query = RepositoryHelpers::replaceParameter(query, std::to_string(target_id));
        
        if (!enabled_only) {
            // enabled 조건 제거
            size_t enabled_pos = query.find("AND is_enabled = 1");
            if (enabled_pos != std::string::npos) {
                query.erase(enabled_pos, 20); // "AND is_enabled = 1" 제거
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                            "findByTarget - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "findByTarget - Found " + std::to_string(entities.size()) + 
                                   " alarm rules for " + target_type + ":" + std::to_string(target_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByTarget failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByTenant(int tenant_id, bool enabled_only) {
    try {
        std::vector<QueryCondition> conditions = {
            {"tenant_id", "=", std::to_string(tenant_id)}
        };
        
        if (enabled_only) {
            conditions.push_back({"is_enabled", "=", "1"});
        }
        
        return findByConditions(conditions);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findBySeverity(const std::string& severity, bool enabled_only) {
    try {
        std::vector<QueryCondition> conditions = {
            {"severity", "=", severity}
        };
        
        if (enabled_only) {
            conditions.push_back({"is_enabled", "=", "1"});
        }
        
        return findByConditions(conditions);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findBySeverity failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByAlarmType(const std::string& alarm_type, bool enabled_only) {
    try {
        std::vector<QueryCondition> conditions = {
            {"condition_type", "=", alarm_type}
        };
        
        if (enabled_only) {
            conditions.push_back({"is_enabled", "=", "1"});
        }
        
        return findByConditions(conditions);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByAlarmType failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAllEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        auto results = db_layer.executeQuery(SQL::Alarm::Rule::FIND_ENABLED);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                            "findAllEnabled - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "findAllEnabled - Found " + std::to_string(entities.size()) + " enabled alarm rules");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findAllEnabled failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findByName(const std::string& name, int tenant_id, int exclude_id) {
    try {
        std::vector<QueryCondition> conditions = {
            {"name", "=", name},
            {"tenant_id", "=", std::to_string(tenant_id)}
        };
        
        if (exclude_id > 0) {
            conditions.push_back({"id", "!=", std::to_string(exclude_id)});
        }
        
        return findFirstByConditions(conditions);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "findByName failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool AlarmRuleRepository::isNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    try {
        auto found = findByName(name, tenant_id, exclude_id);
        return found.has_value();
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "isNameTaken failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 - AlarmTypes.h 올바른 사용
// =============================================================================

AlarmRuleEntity AlarmRuleRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmRuleEntity entity;
        auto it = row.end();
        
        // 기본 정보
        it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoll(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        // 대상 정보
        it = row.find("target_type");
        if (it != row.end()) {
            entity.setTargetType(it->second);
        }
        
        it = row.find("target_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setTargetId(std::stoi(it->second));
        }
        
        // 조건 정보
        it = row.find("condition_type");
        if (it != row.end()) {
            entity.setConditionType(it->second);
        }
        
        it = row.find("threshold_value");
        if (it != row.end() && !it->second.empty()) {
            entity.setThresholdValue(std::stod(it->second));
        }
        
        // 심각도
        it = row.find("severity");
        if (it != row.end()) {
            entity.setSeverity(it->second);
        }
        
        // 우선순위
        it = row.find("priority");
        if (it != row.end() && !it->second.empty()) {
            entity.setPriority(std::stoi(it->second));
        }
        
        // 플래그들
        it = row.find("is_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setEnabled(it->second == "1");
        }
        
        it = row.find("auto_clear");
        if (it != row.end() && !it->second.empty()) {
            entity.setAutoClear(it->second == "1");
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> AlarmRuleRepository::entityToParams(const AlarmRuleEntity& entity) {
    std::map<std::string, std::string> params;
    
    // 기본 정보
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["name"] = escapeString(entity.getName());
    params["description"] = escapeString(entity.getDescription());
    
    // 대상 정보
    params["target_type"] = escapeString(entity.getTargetType());
    params["target_id"] = std::to_string(entity.getTargetId());
    
    // 조건 정보
    params["condition_type"] = escapeString(entity.getConditionType());
    params["threshold_value"] = std::to_string(entity.getThresholdValue());
    
    // 심각도 및 우선순위
    params["severity"] = escapeString(entity.getSeverity());
    params["priority"] = std::to_string(entity.getPriority());
    
    // 플래그들
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["auto_clear"] = entity.isAutoClear() ? "1" : "0";
    
    return params;
}

bool AlarmRuleRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        // 🔥 ExtendedSQLQueries.h 사용
        return db_layer.executeNonQuery(SQL::Alarm::Rule::CREATE_TABLE);
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::ERROR,
                                    "ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmRuleRepository::validateAlarmRule(const AlarmRuleEntity& entity) {
    // 기본 검증
    if (entity.getName().empty()) {
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        return false;
    }
    
    if (entity.getTargetType().empty()) {
        return false;
    }
    
    return true;
}

std::string AlarmRuleRepository::escapeString(const std::string& str) {
    std::string escaped = str;
    
    // 작은따옴표 이스케이프
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    
    return "'" + escaped + "'";
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne