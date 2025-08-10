// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - DeviceRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne AlarmOccurrenceRepository 구현 - SQLQueries.h 활용
 * @author PulseOne Development Team
 * @date 2025-08-10
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"
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
                logger_->Debug("AlarmOccurrenceRepository::findById - No occurrence found for ID: " + std::to_string(id));
            }
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            setCachedEntity(id, entity);
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findById - Found occurrence: " + entity.getAlarmMessage());
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
        
        if (!validateOccurrence(entity)) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::save - Invalid occurrence data");
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
                entity.setId(std::stoi(id_results[0].at("id")));
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::save - Saved occurrence: " + entity.getAlarmMessage());
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
        
        if (!validateOccurrence(entity)) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::update - Invalid occurrence data");
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
                clearCacheForId(entity.getId());
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::update - Updated occurrence: " + entity.getAlarmMessage());
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
                logger_->Info("AlarmOccurrenceRepository::deleteById - Deleted occurrence ID: " + std::to_string(id));
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
// 벌크 연산 구현
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
            logger_->Info("AlarmOccurrenceRepository::findByIds - Found " + std::to_string(entities.size()) + " occurrences for " + std::to_string(ids.size()) + " IDs");
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
            logger_->Info("AlarmOccurrenceRepository::findByConditions - Found " + std::to_string(entities.size()) + " occurrences");
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

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActive() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ACTIVE);
        
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
            logger_->Info("AlarmOccurrenceRepository::findActive - Found " + std::to_string(entities.size()) + " active occurrences");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findActive failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_RULE_ID, std::to_string(rule_id));
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
            logger_->Info("AlarmOccurrenceRepository::findByRuleId - Found " + std::to_string(entities.size()) + " occurrences for rule " + std::to_string(rule_id));
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByRuleId failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 구현
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmOccurrenceEntity entity;
        auto it = row.end();
        
        // 기본 정보
        it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("rule_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setRuleId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("trigger_value");
        if (it != row.end()) {
            entity.setTriggerValue(it->second);
        }
        
        it = row.find("alarm_message");
        if (it != row.end()) {
            entity.setAlarmMessage(it->second);
        }
        
        it = row.find("severity");
        if (it != row.end() && !it->second.empty()) {
            entity.setSeverity(AlarmOccurrenceEntity::stringToSeverity(it->second));
        }
        
        it = row.find("state");
        if (it != row.end() && !it->second.empty()) {
            entity.setState(AlarmOccurrenceEntity::stringToState(it->second));
        }
        
        // TODO: 나머지 필드들 매핑 (타임스탬프, 인지 정보 등)
        
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
    params["trigger_value"] = escapeString(entity.getTriggerValue());
    params["trigger_condition"] = escapeString(entity.getTriggerCondition());
    params["alarm_message"] = escapeString(entity.getAlarmMessage());
    params["severity"] = escapeString(AlarmOccurrenceEntity::severityToString(entity.getSeverity()));
    params["state"] = escapeString(AlarmOccurrenceEntity::stateToString(entity.getState()));
    
    // 간단한 필드들만 먼저 (타임스탬프 등은 추후 추가)
    params["notification_sent"] = entity.isNotificationSent() ? "1" : "0";
    params["notification_count"] = std::to_string(entity.getNotificationCount());
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

bool AlarmOccurrenceRepository::validateOccurrence(const AlarmOccurrenceEntity& entity) {
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

int AlarmOccurrenceRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::AlarmOccurrence::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::getTotalCount failed: " + std::string(e.what()));
        }
        return 0;
    }
}

std::map<std::string, int> AlarmOccurrenceRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        // IRepository에서 캐시 통계 조회
        auto base_stats = IRepository<AlarmOccurrenceEntity>::getCacheStats();
        stats.insert(base_stats.begin(), base_stats.end());
    }
    
    return stats;
}

// TODO: 나머지 특화 메서드들 구현
// - findByTenant, findByState, findBySeverity
// - acknowledgeOccurrence, clearOccurrence
// - getStateDistribution, getSeverityDistribution
// - 등등...

} // namespace Repositories
} // namespace Database
} // namespace PulseOne