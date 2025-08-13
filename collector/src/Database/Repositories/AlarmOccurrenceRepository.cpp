// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne AlarmOccurrenceRepository - DeviceRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 ScriptLibraryRepository 패턴 완전 적용:
 * - ExtendedSQLQueries.h 사용 (분리된 쿼리 파일)
 * - DatabaseAbstractionLayer 패턴
 * - 표준 LogManager 사용법
 * - 벌크 연산 SQL 최적화
 * - 캐시 관리 완전 구현
 * - 모든 IRepository 메서드 override
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/SQLQueries.h"
#include "Database/ExtendedSQLQueries.h"  // 🔥 분리된 쿼리 파일 사용
#include "Utils/LogManager.h"
#include "Alarm/AlarmTypes.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (ScriptLibraryRepository 패턴)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR, 
                                        "findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ALL);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "findAll - Found " + std::to_string(entities.size()) + " alarm occurrences");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                            "findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                        "findById - No alarm occurrence found for ID: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                    "findById - Found alarm occurrence ID: " + std::to_string(id));
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool AlarmOccurrenceRepository::save(AlarmOccurrenceEntity& entity) {
    try {
        auto& logger = LogManager::getInstance();
        logger.log("AlarmOccurrenceRepository", LogLevel::INFO, "🚀 save() 메서드 시작");
        
        if (!ensureTableExists()) {
            logger.log("AlarmOccurrenceRepository", LogLevel::ERROR, "❌ 테이블 존재 확인 실패");
            return false;
        }
        
        // 🔧 문자열 안전 처리
        std::string safe_message = entity.getAlarmMessage();
        std::replace(safe_message.begin(), safe_message.end(), '\'', '"');
        
        std::string safe_trigger_value = entity.getTriggerValue().empty() ? "NULL" : "'" + entity.getTriggerValue() + "'";
        std::string safe_trigger_condition = entity.getTriggerCondition().empty() ? "NULL" : "'" + entity.getTriggerCondition() + "'";
        std::string safe_context = entity.getContextData().empty() ? "'{}'" : "'" + entity.getContextData() + "'";
        std::string safe_source = entity.getSourceName().empty() ? "'Unknown'" : "'" + entity.getSourceName() + "'";
        std::string safe_location = entity.getLocation().empty() ? "'Unknown Location'" : "'" + entity.getLocation() + "'";
        
        // 🔧 enum을 문자열로 변환
        std::string severity_str = entity.getSeverityString();  // ✅ 수정된 부분
        
        std::string insert_query = 
            "INSERT INTO alarm_occurrences ("
            "rule_id, tenant_id, occurrence_time, trigger_value, "
            "trigger_condition, alarm_message, severity, state, "
            "context_data, source_name, location, "
            "created_at, updated_at"
            ") VALUES ("
            + std::to_string(entity.getRuleId()) + ", "
            + std::to_string(entity.getTenantId()) + ", "
            + "datetime('now'), "
            + safe_trigger_value + ", "
            + safe_trigger_condition + ", "
            + "'" + safe_message + "', "
            + "'" + severity_str + "', "  // ✅ 수정된 부분
            + "'active', "
            + safe_context + ", "
            + safe_source + ", "
            + safe_location + ", "
            + "datetime('now'), "
            + "datetime('now')"
            + ")";
        
        logger.log("AlarmOccurrenceRepository", LogLevel::INFO, 
                   "🔧 실행할 INSERT 쿼리: " + insert_query);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(insert_query);
        
        if (success) {
            logger.log("AlarmOccurrenceRepository", LogLevel::INFO, "✅ INSERT 쿼리 실행 성공");
            
            // 새로 생성된 ID 조회
            auto id_results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                int new_id = std::stoi(id_results[0].at("id"));
                entity.setId(new_id);
                logger.log("AlarmOccurrenceRepository", LogLevel::INFO,
                          "✅ 새 ID 설정 완료: " + std::to_string(new_id));
            }
            
            return true;
        } else {
            logger.log("AlarmOccurrenceRepository", LogLevel::ERROR, "❌ INSERT 쿼리 실행 실패");
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "❌ save() 메서드 예외 발생: " + std::string(e.what()));
        return false;
    }
}

bool AlarmOccurrenceRepository::update(const AlarmOccurrenceEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🔧 수정: std::map 방식으로 파라미터 생성
        std::map<std::string, std::string> params;
        params["trigger_value"] = escapeString(entity.getTriggerValue());
        params["trigger_condition"] = escapeString(entity.getTriggerCondition());
        params["alarm_message"] = escapeString(entity.getAlarmMessage());  // getMessage() → getAlarmMessage()
        params["severity"] = escapeString(PulseOne::Alarm::severityToString(entity.getSeverity()));
        params["state"] = escapeString(PulseOne::Alarm::stateToString(entity.getState()));
        params["context_data"] = escapeString(entity.getContextData());
        params["source_name"] = escapeString(entity.getSourceName());
        params["id"] = std::to_string(entity.getId());
        
        // 🔧 수정: 올바른 RepositoryHelpers 메서드 사용
        std::string query = SQL::AlarmOccurrence::UPDATE;
        query = RepositoryHelpers::replaceParametersInOrder(query, params);  // map 버전 사용
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                        "update - Updated alarm occurrence ID: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "update failed: " + std::string(e.what()));
        return false;
    }
}


bool AlarmOccurrenceRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                        "deleteById - Deleted alarm occurrence ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmOccurrenceRepository::exists(int id) {
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
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        return !results.empty() && std::stoi(results[0].at("count")) > 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산 (SQL 최적화된 구현) - ScriptLibraryRepository 패턴
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByIds(const std::vector<int>& ids) {
    std::vector<AlarmOccurrenceEntity> results;
    
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
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
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
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                    "findByIds - Found " + std::to_string(results.size()) + " alarm occurrences for " + std::to_string(ids.size()) + " IDs");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findByIds failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        if (!ensureTableExists()) {
            return results;
        }
        
        // 🎯 RepositoryHelpers를 사용한 동적 쿼리 구성
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
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
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                    "findByConditions - Found " + std::to_string(results.size()) + " alarm occurrences");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findByConditions failed: " + std::string(e.what()));
    }
    
    return results;
}

int AlarmOccurrenceRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 ExtendedSQLQueries.h 상수 + RepositoryHelpers 패턴
        std::string query = SQL::AlarmOccurrence::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    // 🎯 첫 번째 결과만 필요하므로 LIMIT 1 적용
    auto pagination = Pagination{0, 1}; // offset=0, limit=1
    auto results = findByConditions(conditions, std::nullopt, pagination);
    
    if (!results.empty()) {
        return results[0];
    }
    
    return std::nullopt;
}

int AlarmOccurrenceRepository::saveBulk(std::vector<AlarmOccurrenceEntity>& entities) {
    // 🔥 TODO: 실제 배치 INSERT 구현 필요 (성능 최적화)
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    return saved_count;
}

int AlarmOccurrenceRepository::updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) {
    // 🔥 TODO: 실제 배치 UPDATE 구현 필요 (성능 최적화)
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    return updated_count;
}

int AlarmOccurrenceRepository::deleteByIds(const std::vector<int>& ids) {
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
        
        std::string query = "DELETE FROM alarm_occurrences WHERE id IN (" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                        "deleteByIds - Deleted " + std::to_string(ids.size()) + " alarm occurrences");
            return static_cast<int>(ids.size());
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// AlarmOccurrence 전용 메서드들 - ExtendedSQLQueries.h 사용
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActive(std::optional<int> tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ACTIVE;
        
        if (tenant_id.has_value()) {
            // 🎯 RepositoryHelpers를 사용한 조건 추가
            size_t order_pos = query.find("ORDER BY");
            std::string tenant_clause = "AND tenant_id = " + std::to_string(tenant_id.value()) + " ";
            
            if (order_pos != std::string::npos) {
                query.insert(order_pos, tenant_clause);
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findActive - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "findActive - Found " + std::to_string(entities.size()) + " active alarm occurrences");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findActive failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id, bool active_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_BY_RULE_ID;
        
        // 🎯 RepositoryHelpers 패턴
        query = RepositoryHelpers::replaceParameter(query, std::to_string(rule_id));
        
        if (active_only) {
            size_t order_pos = query.find("ORDER BY");
            std::string active_clause = "AND state = 'active' ";
            
            if (order_pos != std::string::npos) {
                query.insert(order_pos, active_clause);
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findByRuleId - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "findByRuleId - Found " + std::to_string(entities.size()) + 
                                   " alarm occurrences for rule: " + std::to_string(rule_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findByRuleId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByTenant(int tenant_id, const std::string& state_filter) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // 🎯 WHERE 절 구성
        std::string where_clause = "WHERE tenant_id = " + std::to_string(tenant_id);
        
        if (!state_filter.empty()) {
            where_clause += " AND state = '" + state_filter + "'";
        }
        where_clause += " ";
        
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findByTenant - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "findByTenant - Found " + std::to_string(entities.size()) + 
                                   " alarm occurrences for tenant: " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByTimeRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time,
    std::optional<int> tenant_id) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // 🎯 WHERE 절 구성
        std::string start_str = timePointToString(start_time);
        std::string end_str = timePointToString(end_time);
        
        std::string where_clause = "WHERE occurrence_time >= '" + start_str + "' AND occurrence_time <= '" + end_str + "'";
        
        if (tenant_id.has_value()) {
            where_clause += " AND tenant_id = " + std::to_string(tenant_id.value());
        }
        where_clause += " ";
        
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findByTimeRange - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "findByTimeRange - Found " + std::to_string(entities.size()) + " alarm occurrences in time range");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findByTimeRange failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 알람 상태 관리 메서드들 - ExtendedSQLQueries.h 사용
// =============================================================================

bool AlarmOccurrenceRepository::acknowledge(int64_t occurrence_id, int acknowledged_by, const std::string& comment) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = SQL::AlarmOccurrence::ACKNOWLEDGE;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(acknowledged_by));
        query = RepositoryHelpers::replaceParameter(query, escapeString(comment));
        query = RepositoryHelpers::replaceParameter(query, std::to_string(occurrence_id));
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(occurrence_id));
            }
            
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                        "acknowledge - Acknowledged alarm occurrence ID: " + std::to_string(occurrence_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "acknowledge failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmOccurrenceRepository::clear(int64_t occurrence_id, const std::string& cleared_value, const std::string& comment) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = SQL::AlarmOccurrence::CLEAR;
        query = RepositoryHelpers::replaceParameter(query, escapeString(cleared_value));
        query = RepositoryHelpers::replaceParameter(query, escapeString(comment));
        query = RepositoryHelpers::replaceParameter(query, std::to_string(occurrence_id));
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(occurrence_id));
            }
            
            LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                        "clear - Cleared alarm occurrence ID: " + std::to_string(occurrence_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "clear failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 통계 및 분석 메서드들 - SQL 최적화
// =============================================================================

std::map<std::string, int> AlarmOccurrenceRepository::getAlarmStatistics(int tenant_id) {
    std::map<std::string, int> stats;
    
    try {
        if (!ensureTableExists()) {
            return stats;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 한 번의 쿼리로 모든 통계 조회 (성능 최적화)
        std::string query = R"(
            SELECT 
                COUNT(*) as total,
                COUNT(CASE WHEN state = 'active' THEN 1 END) as active,
                COUNT(CASE WHEN state = 'acknowledged' THEN 1 END) as acknowledged,
                COUNT(CASE WHEN state = 'cleared' THEN 1 END) as cleared
            FROM alarm_occurrences 
            WHERE tenant_id = )" + std::to_string(tenant_id);
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            const auto& row = results[0];
            stats["total"] = std::stoi(row.at("total"));
            stats["active"] = std::stoi(row.at("active"));
            stats["acknowledged"] = std::stoi(row.at("acknowledged"));
            stats["cleared"] = std::stoi(row.at("cleared"));
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "getAlarmStatistics - Retrieved statistics for tenant: " + std::to_string(tenant_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "getAlarmStatistics failed: " + std::string(e.what()));
    }
    
    return stats;
}

// =============================================================================
// 내부 헬퍼 메서드들 - AlarmTypes.h 올바른 사용
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmOccurrenceEntity entity;
        auto it = row.end();
        
        // 기본 정보
        it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoll(it->second));
        }
        
        it = row.find("rule_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setRuleId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        // 발생 시간
        it = row.find("occurrence_time");
        if (it != row.end() && !it->second.empty()) {
            entity.setOccurrenceTime(stringToTimePoint(it->second));
        }
        
        // 트리거 정보
        it = row.find("trigger_value");
        if (it != row.end()) {
            entity.setTriggerValue(it->second);
        }
        
        it = row.find("alarm_message");
        if (it != row.end()) {
            entity.setAlarmMessage(it->second);
        }
        
        // 🎯 AlarmSeverity 변환 - PulseOne::Alarm 네임스페이스 사용
        it = row.find("severity");
        if (it != row.end() && !it->second.empty()) {
            entity.setSeverity(PulseOne::Alarm::stringToSeverity(it->second));
        }
        
        // 🎯 AlarmState 변환 - PulseOne::Alarm 네임스페이스 사용
        it = row.find("state");
        if (it != row.end() && !it->second.empty()) {
            entity.setState(PulseOne::Alarm::stringToState(it->second));
        }
        
        // Acknowledge 정보
        it = row.find("acknowledged_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setAcknowledgedTime(stringToTimePoint(it->second));
        }
        
        it = row.find("acknowledged_by");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setAcknowledgedBy(std::stoi(it->second));
        }
        
        // Clear 정보
        it = row.find("cleared_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setClearedTime(stringToTimePoint(it->second));
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> AlarmOccurrenceRepository::entityToParams(const AlarmOccurrenceEntity& entity) {
    std::map<std::string, std::string> params;
    
    // 🔥 INSERT 쿼리와 정확히 일치하는 8개 파라미터만!
    // INSERT: rule_id, tenant_id, trigger_value, trigger_condition, alarm_message, severity, context_data, source_name
    
    params["rule_id"] = std::to_string(entity.getRuleId());
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["trigger_value"] = escapeString(entity.getTriggerValue());
    params["trigger_condition"] = escapeString(entity.getTriggerCondition());
    params["alarm_message"] = escapeString(entity.getAlarmMessage());
    
    // 🎯 AlarmSeverity 변환 - PulseOne::Alarm 네임스페이스 사용
    params["severity"] = escapeString(PulseOne::Alarm::severityToString(entity.getSeverity()));
    
    // 추가 컬럼들
    params["context_data"] = escapeString(entity.getContextData());
    params["source_name"] = escapeString(entity.getSourceName());
    
    return params;
}


bool AlarmOccurrenceRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;

        // 먼저 테이블 존재 여부 확인
        std::string check_query = "SELECT name FROM sqlite_master WHERE type='table' AND name='alarm_occurrences'";
        auto results = db_layer.executeQuery(check_query);
        
        if (!results.empty()) {
            return true;  // 이미 존재하면 성공
        }

        // 🔥 ExtendedSQLQueries.h 사용
        return db_layer.executeNonQuery(SQL::AlarmOccurrence::CREATE_TABLE);
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmOccurrenceRepository::validateAlarmOccurrence(const AlarmOccurrenceEntity& entity) {
    // 기본 검증
    if (entity.getRuleId() <= 0) {
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        return false;
    }
    
    if (entity.getAlarmMessage().empty()) {
        return false;
    }
    
    return true;
}

std::string AlarmOccurrenceRepository::escapeString(const std::string& str) {
    std::string escaped = str;
    
    // 작은따옴표 이스케이프
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    
    return "'" + escaped + "'";
}

std::string AlarmOccurrenceRepository::timePointToString(const std::chrono::system_clock::time_point& time_point) const {
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::chrono::system_clock::time_point AlarmOccurrenceRepository::stringToTimePoint(const std::string& time_str) const {
    std::tm tm = {};
    std::istringstream ss(time_str);
    
    // ISO 8601 형식 파싱: "YYYY-MM-DD HH:MM:SS.mmm"
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // 밀리초 파싱 (있으면)
    if (ss.peek() == '.') {
        ss.ignore(1); // '.' 건너뛰기
        int ms;
        ss >> ms;
        time_point += std::chrono::milliseconds(ms);
    }
    
    return time_point;
}


std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActiveByRuleId(int rule_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        std::string query = R"(
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at
            FROM alarm_occurrences 
            WHERE rule_id = ? AND state = 'active'
            ORDER BY occurrence_time DESC
        )";
        
        // 파라미터 치환
        query = RepositoryHelpers::replaceParameter(query, std::to_string(rule_id));
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::WARN,
                                            "findActiveByRuleId - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                    "findActiveByRuleId - Found " + std::to_string(entities.size()) + 
                                   " active alarms for rule: " + std::to_string(rule_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findActiveByRuleId failed: " + std::string(e.what()));
        return {};
    }
}

int AlarmOccurrenceRepository::findMaxId() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🔥 수정: ExtendedSQLQueries.h 상수 사용 (프로젝트 표준)
        auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_MAX_ID);
        
        if (!results.empty() && results[0].find("max_id") != results[0].end()) {
            const std::string& max_id_str = results[0].at("max_id");
            if (!max_id_str.empty() && max_id_str != "NULL") {
                return std::stoi(max_id_str);
            }
        }
        
        return 0; // 테이블이 비어있으면 0 반환
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::ERROR,
                                    "findMaxId failed: " + std::string(e.what()));
        return 0; // 에러 시에도 0 반환
    }
}


} // namespace Repositories
} // namespace Database
} // namespace PulseOne