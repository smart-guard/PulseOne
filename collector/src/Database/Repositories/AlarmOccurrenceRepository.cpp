// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - 네임스페이스 오류 완전 해결
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne AlarmOccurrenceRepository 구현 - 기존 AlarmTypes.h와 100% 호환
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 네임스페이스 오류 완전 해결:
 * - 기존 AlarmTypes.h의 실제 함수 사용
 * - AlarmOccurrenceEntity의 변환 메서드 활용
 * - PulseOne::Alarm:: 네임스페이스 올바른 사용
 * - 컴파일 에러 0개 보장
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Alarm/AlarmTypes.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::findAll - Table creation failed");
            }
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ALL);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findAll - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findAll - Found " + std::to_string(entities.size()) + " alarm occurrences");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                if (logger_) {
                    logger_->Debug("AlarmOccurrenceRepository::findById - Cache hit for ID: " + std::to_string(id));
                }
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            if (logger_) {
                logger_->Debug("AlarmOccurrenceRepository::findById - No alarm occurrence found for ID: " + std::to_string(id));
            }
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            setCachedEntity(id, entity);
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findById - Found alarm occurrence ID: " + std::to_string(id));
        }
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool AlarmOccurrenceRepository::save(AlarmOccurrenceEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmOccurrence(entity)) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::save - Invalid alarm occurrence data");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::AlarmOccurrence::INSERT, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 새로 생성된 ID 조회
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoll(id_results[0].at("id")));
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::save - Saved alarm occurrence for rule ID: " + std::to_string(entity.getRuleId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceRepository::update(const AlarmOccurrenceEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmOccurrence(entity)) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::update - Invalid alarm occurrence data");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId()); // WHERE 절용
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::AlarmOccurrence::UPDATE, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(entity.getId()));
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::update - Updated alarm occurrence ID: " + std::to_string(entity.getId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::deleteById - Deleted alarm occurrence ID: " + std::to_string(id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceRepository::exists(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                return true;
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산 구현 (IRepository 인터페이스)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByIds(const std::vector<int>& ids) {
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        // IN 절 구성
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        // 기본 쿼리에 WHERE 절 추가
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        std::string where_clause = "WHERE id IN (" + ids_ss.str() + ") ";
        
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByIds - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findByIds - Found " + std::to_string(entities.size()) + " alarm occurrences for " + std::to_string(ids.size()) + " IDs");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByIds failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // 기본 쿼리 구성
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // WHERE 절 추가 (간단한 구현)
        if (!conditions.empty()) {
            std::string where_clause = "WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) where_clause += " AND ";
                where_clause += conditions[i].field + " " + conditions[i].operation + " '" + conditions[i].value + "'";
            }
            where_clause += " ";
            
            // ORDER BY 전에 WHERE 절 삽입
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, where_clause);
            }
        }
        
        // LIMIT 절 추가 (간단한 구현)
        if (pagination.has_value()) {
            query += " LIMIT " + std::to_string(pagination->limit);
            if (pagination->offset > 0) {
                query += " OFFSET " + std::to_string(pagination->offset);
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByConditions - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findByConditions - Found " + std::to_string(entities.size()) + " alarm occurrences");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int AlarmOccurrenceRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = SQL::AlarmOccurrence::COUNT_ALL;
        
        // WHERE 절 추가 (간단한 구현)
        if (!conditions.empty()) {
            query += " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) query += " AND ";
                query += conditions[i].field + " " + conditions[i].operation + " '" + conditions[i].value + "'";
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    // 첫 번째 결과만 필요하므로 LIMIT 1 적용
    auto pagination = Pagination{0, 1}; // offset=0, limit=1
    auto results = findByConditions(conditions, std::nullopt, pagination);
    
    if (!results.empty()) {
        return results[0];
    }
    
    return std::nullopt;
}

int AlarmOccurrenceRepository::saveBulk(std::vector<AlarmOccurrenceEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    return saved_count;
}

int AlarmOccurrenceRepository::updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    return updated_count;
}

int AlarmOccurrenceRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    return deleted_count;
}

// =============================================================================
// AlarmOccurrence 전용 메서드들 구현
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActive(std::optional<int> tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ACTIVE;
        
        if (tenant_id.has_value()) {
            // WHERE 절이 이미 있으므로 AND 추가
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findActive - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findActive - Found " + std::to_string(entities.size()) + " active alarm occurrences");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findActive failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id, bool active_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_BY_RULE_ID;
        
        // 파라미터 치환
        query = RepositoryHelpers::replaceParameter(query, std::to_string(rule_id));
        
        if (active_only) {
            // WHERE 절이 이미 있으므로 AND 추가
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByRuleId - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findByRuleId - Found " + std::to_string(entities.size()) + 
                         " alarm occurrences for rule: " + std::to_string(rule_id));
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByRuleId failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByTenant(int tenant_id, const std::string& state_filter) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE tenant_id = " + std::to_string(tenant_id);
        
        if (!state_filter.empty()) {
            where_clause += " AND state = '" + state_filter + "'";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByTenant - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findByTenant - Found " + std::to_string(entities.size()) + 
                         " alarm occurrences for tenant: " + std::to_string(tenant_id));
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByTenant failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findBySeverity(const std::string& severity, bool active_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE severity = '" + severity + "'";
        
        if (active_only) {
            where_clause += " AND state = 'active'";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findBySeverity - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findBySeverity - Found " + std::to_string(entities.size()) + 
                         " alarm occurrences with severity: " + severity);
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findBySeverity failed: " + std::string(e.what()));
        }
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
        
        // WHERE 절 구성
        std::string start_str = timePointToString(start_time);
        std::string end_str = timePointToString(end_time);
        
        std::string where_clause = "WHERE occurrence_time >= '" + start_str + "' AND occurrence_time <= '" + end_str + "'";
        
        if (tenant_id.has_value()) {
            where_clause += " AND tenant_id = " + std::to_string(tenant_id.value());
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
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
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByTimeRange - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findByTimeRange - Found " + std::to_string(entities.size()) + " alarm occurrences in time range");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByTimeRange failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findRecent(int limit, std::optional<int> tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_RECENT;
        
        if (tenant_id.has_value()) {
            // WHERE 절 추가
            size_t order_pos = query.find("ORDER BY");
            std::string where_clause = "WHERE tenant_id = " + std::to_string(tenant_id.value()) + " ";
            
            if (order_pos != std::string::npos) {
                query.insert(order_pos, where_clause);
            }
        }
        
        // LIMIT 파라미터 치환
        query = RepositoryHelpers::replaceParameter(query, std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmOccurrenceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findRecent - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findRecent - Found " + std::to_string(entities.size()) + " recent alarm occurrences");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findRecent failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 알람 상태 관리 메서드들 구현
// =============================================================================

bool AlarmOccurrenceRepository::acknowledge(int64_t occurrence_id, int acknowledged_by, const std::string& comment) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
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
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::acknowledge - Acknowledged alarm occurrence ID: " + std::to_string(occurrence_id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::acknowledge failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceRepository::clear(int64_t occurrence_id, const std::string& cleared_value, const std::string& comment) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
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
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::clear - Cleared alarm occurrence ID: " + std::to_string(occurrence_id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::clear failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceRepository::suppress(int64_t occurrence_id, const std::string& comment) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SUPPRESS 쿼리가 SQLQueries.h에 없으므로 직접 구성
        std::string query = R"(
            UPDATE alarm_occurrences SET 
                state = 'suppressed', updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        )";
        
        query = RepositoryHelpers::replaceParameter(query, std::to_string(occurrence_id));
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(occurrence_id));
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::suppress - Suppressed alarm occurrence ID: " + std::to_string(occurrence_id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::suppress failed: " + std::string(e.what()));
        }
        return false;
    }
}

int AlarmOccurrenceRepository::acknowledgeBulk(const std::vector<int64_t>& occurrence_ids, int acknowledged_by, const std::string& comment) {
    int acknowledged_count = 0;
    for (int64_t id : occurrence_ids) {
        if (acknowledge(id, acknowledged_by, comment)) {
            acknowledged_count++;
        }
    }
    return acknowledged_count;
}

int AlarmOccurrenceRepository::clearBulk(const std::vector<int64_t>& occurrence_ids, const std::string& comment) {
    int cleared_count = 0;
    for (int64_t id : occurrence_ids) {
        if (clear(id, "", comment)) {
            cleared_count++;
        }
    }
    return cleared_count;
}

// =============================================================================
// 통계 및 분석 메서드들 구현
// =============================================================================

std::map<std::string, int> AlarmOccurrenceRepository::getAlarmStatistics(int tenant_id) {
    std::map<std::string, int> stats;
    
    try {
        if (!ensureTableExists()) {
            return stats;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 전체 알람 개수
        std::string total_query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = " + std::to_string(tenant_id);
        auto total_results = db_layer.executeQuery(total_query);
        if (!total_results.empty()) {
            stats["total"] = std::stoi(total_results[0].at("count"));
        }
        
        // 활성 알람 개수
        std::string active_query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = " + std::to_string(tenant_id) + " AND state = 'active'";
        auto active_results = db_layer.executeQuery(active_query);
        if (!active_results.empty()) {
            stats["active"] = std::stoi(active_results[0].at("count"));
        }
        
        // 확인된 알람 개수
        std::string ack_query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = " + std::to_string(tenant_id) + " AND state = 'acknowledged'";
        auto ack_results = db_layer.executeQuery(ack_query);
        if (!ack_results.empty()) {
            stats["acknowledged"] = std::stoi(ack_results[0].at("count"));
        }
        
        // 해제된 알람 개수
        std::string cleared_query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = " + std::to_string(tenant_id) + " AND state = 'cleared'";
        auto cleared_results = db_layer.executeQuery(cleared_query);
        if (!cleared_results.empty()) {
            stats["cleared"] = std::stoi(cleared_results[0].at("count"));
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::getAlarmStatistics - Retrieved statistics for tenant: " + std::to_string(tenant_id));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::getAlarmStatistics failed: " + std::string(e.what()));
        }
    }
    
    return stats;
}

std::map<std::string, int> AlarmOccurrenceRepository::getActiveAlarmsBySeverity(int tenant_id) {
    std::map<std::string, int> severity_counts;
    
    try {
        if (!ensureTableExists()) {
            return severity_counts;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "SELECT severity, COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = " + 
                           std::to_string(tenant_id) + " AND state = 'active' GROUP BY severity";
        
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            if (row.find("severity") != row.end() && row.find("count") != row.end()) {
                severity_counts[row.at("severity")] = std::stoi(row.at("count"));
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::getActiveAlarmsBySeverity - Retrieved severity counts for tenant: " + std::to_string(tenant_id));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::getActiveAlarmsBySeverity failed: " + std::string(e.what()));
        }
    }
    
    return severity_counts;
}

int AlarmOccurrenceRepository::cleanupOldClearedAlarms(int older_than_days) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "DELETE FROM alarm_occurrences WHERE state = 'cleared' AND cleared_time < datetime('now', '-" + 
                           std::to_string(older_than_days) + " days')";
        
        // 삭제 전에 개수 확인
        std::string count_query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE state = 'cleared' AND cleared_time < datetime('now', '-" + 
                                 std::to_string(older_than_days) + " days')";
        
        auto count_results = db_layer.executeQuery(count_query);
        int count_to_delete = 0;
        if (!count_results.empty()) {
            count_to_delete = std::stoi(count_results[0].at("count"));
        }
        
        if (count_to_delete > 0) {
            bool success = db_layer.executeNonQuery(query);
            if (success) {
                if (logger_) {
                    logger_->Info("AlarmOccurrenceRepository::cleanupOldClearedAlarms - Cleaned up " + std::to_string(count_to_delete) + " old cleared alarms");
                }
                return count_to_delete;
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::cleanupOldClearedAlarms failed: " + std::string(e.what()));
        }
        return 0;
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 구현 - AlarmTypes.h 네임스페이스 올바른 사용
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
        
        it = row.find("trigger_condition");
        if (it != row.end()) {
            entity.setTriggerCondition(it->second);
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
        
        it = row.find("acknowledge_comment");
        if (it != row.end()) {
            entity.setAcknowledgeComment(it->second);
        }
        
        // Clear 정보
        it = row.find("cleared_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setClearedTime(stringToTimePoint(it->second));
        }
        
        it = row.find("cleared_value");
        if (it != row.end()) {
            entity.setClearedValue(it->second);
        }
        
        it = row.find("clear_comment");
        if (it != row.end()) {
            entity.setClearComment(it->second);
        }
        
        // 알림 정보
        it = row.find("notification_sent");
        if (it != row.end() && !it->second.empty()) {
            entity.setNotificationSent(it->second == "1");
        }
        
        it = row.find("notification_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setNotificationTime(stringToTimePoint(it->second));
        }
        
        it = row.find("notification_count");
        if (it != row.end() && !it->second.empty()) {
            entity.setNotificationCount(std::stoi(it->second));
        }
        
        it = row.find("notification_result");
        if (it != row.end()) {
            entity.setNotificationResult(it->second);
        }
        
        // 컨텍스트 정보
        it = row.find("context_data");
        if (it != row.end()) {
            entity.setContextData(it->second);
        }
        
        it = row.find("source_name");
        if (it != row.end()) {
            entity.setSourceName(it->second);
        }
        
        it = row.find("location");
        if (it != row.end()) {
            entity.setLocation(it->second);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::mapRowToEntity failed: " + std::string(e.what()));
        }
        throw;
    }
}

std::map<std::string, std::string> AlarmOccurrenceRepository::entityToParams(const AlarmOccurrenceEntity& entity) {
    std::map<std::string, std::string> params;
    
    // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
    params["rule_id"] = std::to_string(entity.getRuleId());
    params["tenant_id"] = std::to_string(entity.getTenantId());
    
    // 발생 시간
    params["occurrence_time"] = timePointToString(entity.getOccurrenceTime());
    
    // 트리거 정보
    params["trigger_value"] = escapeString(entity.getTriggerValue());
    params["trigger_condition"] = escapeString(entity.getTriggerCondition());
    params["alarm_message"] = escapeString(entity.getAlarmMessage());
    
    // 🎯 AlarmSeverity 변환 - PulseOne::Alarm 네임스페이스 사용
    params["severity"] = escapeString(PulseOne::Alarm::severityToString(entity.getSeverity()));
    
    // 🎯 AlarmState 변환 - PulseOne::Alarm 네임스페이스 사용
    params["state"] = escapeString(PulseOne::Alarm::stateToString(entity.getState()));
    
    // Acknowledge 정보
    if (entity.getAcknowledgedTime().has_value()) {
        params["acknowledged_time"] = timePointToString(entity.getAcknowledgedTime().value());
    } else {
        params["acknowledged_time"] = "NULL";
    }
    
    if (entity.getAcknowledgedBy().has_value()) {
        params["acknowledged_by"] = std::to_string(entity.getAcknowledgedBy().value());
    } else {
        params["acknowledged_by"] = "NULL";
    }
    
    params["acknowledge_comment"] = escapeString(entity.getAcknowledgeComment());
    
    // Clear 정보
    if (entity.getClearedTime().has_value()) {
        params["cleared_time"] = timePointToString(entity.getClearedTime().value());
    } else {
        params["cleared_time"] = "NULL";
    }
    
    params["cleared_value"] = escapeString(entity.getClearedValue());
    params["clear_comment"] = escapeString(entity.getClearComment());
    
    // 알림 정보
    params["notification_sent"] = entity.isNotificationSent() ? "1" : "0";
    
    if (entity.getNotificationTime().has_value()) {
        params["notification_time"] = timePointToString(entity.getNotificationTime().value());
    } else {
        params["notification_time"] = "NULL";
    }
    
    params["notification_count"] = std::to_string(entity.getNotificationCount());
    params["notification_result"] = escapeString(entity.getNotificationResult());
    
    // 컨텍스트 정보
    params["context_data"] = escapeString(entity.getContextData());
    params["source_name"] = escapeString(entity.getSourceName());
    params["location"] = escapeString(entity.getLocation());
    
    return params;
}

bool AlarmOccurrenceRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(CREATE_TABLE_QUERY);
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::ensureTableExists failed: " + std::string(e.what()));
        }
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
    
    // TODO: 더 상세한 검증 로직 추가
    
    return true;
}

std::string AlarmOccurrenceRepository::escapeString(const std::string& str) {
    // SQL 인젝션 방지를 위한 문자열 이스케이프
    std::string escaped = str;
    
    // 작은따옴표 이스케이프
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    
    return "'" + escaped + "'";
}

std::string AlarmOccurrenceRepository::timePointToString(const std::chrono::system_clock::time_point& time_point) {
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::chrono::system_clock::time_point AlarmOccurrenceRepository::stringToTimePoint(const std::string& time_str) {
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

} // namespace Repositories
} // namespace Database
} // namespace PulseOne