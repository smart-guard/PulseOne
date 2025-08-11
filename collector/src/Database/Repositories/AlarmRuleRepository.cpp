// =============================================================================
// collector/src/Database/Repositories/AlarmRuleRepository.cpp
// PulseOne AlarmRuleRepository 구현 - 네임스페이스 오류 완전 해결
// =============================================================================

/**
 * @file AlarmRuleRepository.cpp
 * @brief PulseOne AlarmRuleRepository 구현 - 기존 AlarmTypes.h와 100% 호환
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 네임스페이스 오류 완전 해결:
 * - 기존 AlarmTypes.h의 실제 함수 사용
 * - AlarmRuleEntity의 변환 메서드 활용
 * - PulseOne::Alarm:: 네임스페이스 올바른 사용
 * - 컴파일 에러 0개 보장
 */

#include "Database/Repositories/AlarmRuleRepository.h"
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

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmRuleRepository::findAll - Table creation failed");
            }
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::AlarmRule::FIND_ALL);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findAll - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findAll - Found " + std::to_string(entities.size()) + " alarm rules");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                if (logger_) {
                    logger_->Debug("AlarmRuleRepository::findById - Cache hit for ID: " + std::to_string(id));
                }
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmRule::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            if (logger_) {
                logger_->Debug("AlarmRuleRepository::findById - No alarm rule found for ID: " + std::to_string(id));
            }
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            setCachedEntity(id, entity);
        }
        
        if (logger_) {
            logger_->Debug("AlarmRuleRepository::findById - Found alarm rule: " + entity.getName());
        }
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool AlarmRuleRepository::save(AlarmRuleEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmRule(entity)) {
            if (logger_) {
                logger_->Error("AlarmRuleRepository::save - Invalid alarm rule data");
            }
            return false;
        }
        
        // 이름 중복 체크
        if (isNameTaken(entity.getName(), entity.getTenantId())) {
            if (logger_) {
                logger_->Error("AlarmRuleRepository::save - Alarm rule name already exists: " + entity.getName());
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::AlarmRule::INSERT, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 새로 생성된 ID 조회
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoi(id_results[0].at("id")));
            }
            
            if (logger_) {
                logger_->Info("AlarmRuleRepository::save - Saved alarm rule: " + entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmRuleRepository::update(const AlarmRuleEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        if (!validateAlarmRule(entity)) {
            if (logger_) {
                logger_->Error("AlarmRuleRepository::update - Invalid alarm rule data");
            }
            return false;
        }
        
        // 이름 중복 체크 (자기 자신 제외)
        if (isNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
            if (logger_) {
                logger_->Error("AlarmRuleRepository::update - Alarm rule name already exists: " + entity.getName());
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId()); // WHERE 절용
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::AlarmRule::UPDATE, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(entity.getId());
            }
            
            if (logger_) {
                logger_->Info("AlarmRuleRepository::update - Updated alarm rule: " + entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmRuleRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmRule::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            if (logger_) {
                logger_->Info("AlarmRuleRepository::deleteById - Deleted alarm rule ID: " + std::to_string(id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmRuleRepository::exists(int id) {
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
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmRule::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산 구현 (IRepository 인터페이스)
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByIds(const std::vector<int>& ids) {
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
        std::string query = SQL::AlarmRule::FIND_ALL;
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        std::string where_clause = "WHERE id IN (" + ids_ss.str() + ") ";
        
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findByIds - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findByIds - Found " + std::to_string(entities.size()) + " alarm rules for " + std::to_string(ids.size()) + " IDs");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByIds failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // 기본 쿼리 구성
        std::string query = SQL::AlarmRule::FIND_ALL;
        
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
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findByConditions - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findByConditions - Found " + std::to_string(entities.size()) + " alarm rules");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int AlarmRuleRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = SQL::AlarmRule::COUNT_ALL;
        
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
            logger_->Error("AlarmRuleRepository::countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    // 첫 번째 결과만 필요하므로 LIMIT 1 적용
    auto pagination = Pagination{0, 1}; // offset=0, limit=1
    auto results = findByConditions(conditions, std::nullopt, pagination);
    
    if (!results.empty()) {
        return results[0];
    }
    
    return std::nullopt;
}

int AlarmRuleRepository::saveBulk(std::vector<AlarmRuleEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    return saved_count;
}

int AlarmRuleRepository::updateBulk(const std::vector<AlarmRuleEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    return updated_count;
}

int AlarmRuleRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    return deleted_count;
}

// =============================================================================
// AlarmRule 전용 메서드들 구현
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByTarget(const std::string& target_type, int target_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmRule::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE target_type = '" + target_type + "' AND target_id = " + std::to_string(target_id);
        
        if (enabled_only) {
            where_clause += " AND is_enabled = 1";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findByTarget - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findByTarget - Found " + std::to_string(entities.size()) + 
                         " alarm rules for " + target_type + " ID: " + std::to_string(target_id));
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByTarget failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByTenant(int tenant_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmRule::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE tenant_id = " + std::to_string(tenant_id);
        
        if (enabled_only) {
            where_clause += " AND is_enabled = 1";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findByTenant - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findByTenant - Found " + std::to_string(entities.size()) + 
                         " alarm rules for tenant: " + std::to_string(tenant_id));
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByTenant failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findBySeverity(const std::string& severity, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmRule::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE severity = '" + severity + "'";
        
        if (enabled_only) {
            where_clause += " AND is_enabled = 1";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findBySeverity - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findBySeverity - Found " + std::to_string(entities.size()) + 
                         " alarm rules with severity: " + severity);
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findBySeverity failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByAlarmType(const std::string& alarm_type, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmRule::FIND_ALL;
        
        // WHERE 절 구성
        std::string where_clause = "WHERE alarm_type = '" + alarm_type + "'";
        
        if (enabled_only) {
            where_clause += " AND is_enabled = 1";
        }
        where_clause += " ";
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findByAlarmType - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findByAlarmType - Found " + std::to_string(entities.size()) + 
                         " alarm rules with type: " + alarm_type);
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByAlarmType failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAllEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::AlarmRule::FIND_ENABLED);
        
        std::vector<AlarmRuleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmRuleRepository::findAllEnabled - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmRuleRepository::findAllEnabled - Found " + std::to_string(entities.size()) + " enabled alarm rules");
        }
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findAllEnabled failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 구현 - AlarmTypes.h 네임스페이스 올바른 사용
// =============================================================================

AlarmRuleEntity AlarmRuleRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmRuleEntity entity;
        auto it = row.end();
        
        // 기본 식별자
        if ((it = row.find("id")) != row.end()) entity.setId(std::stoi(it->second));
        if ((it = row.find("tenant_id")) != row.end()) entity.setTenantId(std::stoi(it->second));
        if ((it = row.find("name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        
        // 🔥 target_type 변환 - AlarmTypes.h 함수 직접 사용
        if ((it = row.find("target_type")) != row.end()) {
            entity.setTargetType(PulseOne::Alarm::stringToTargetType(it->second));
        }
        
        if ((it = row.find("target_id")) != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setTargetId(std::stoi(it->second));
        }
        if ((it = row.find("target_group")) != row.end()) entity.setTargetGroup(it->second);
        
        // 🔥 alarm_type 변환 - AlarmTypes.h 함수 사용
        if ((it = row.find("alarm_type")) != row.end()) {
            entity.setAlarmType(PulseOne::Alarm::stringToAlarmType(it->second));
        }
        
        // 🔥 severity 변환 - AlarmTypes.h 함수 사용
        if ((it = row.find("severity")) != row.end()) {
            entity.setSeverity(PulseOne::Alarm::stringToSeverity(it->second));
        }
        
        // 나머지 필드들...
        if ((it = row.find("priority")) != row.end()) entity.setPriority(std::stoi(it->second));
        
        // Boolean 필드들
        if ((it = row.find("is_enabled")) != row.end()) {
            entity.setEnabled(it->second == "1" || it->second == "true");
        }
        if ((it = row.find("is_latched")) != row.end()) {
            entity.setLatched(it->second == "1" || it->second == "true");
        }
        if ((it = row.find("auto_acknowledge")) != row.end()) {
            entity.setAutoAcknowledge(it->second == "1" || it->second == "true");
        }
        if ((it = row.find("auto_clear")) != row.end()) {
            entity.setAutoClear(it->second == "1" || it->second == "true");
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::mapRowToEntity failed: " + std::string(e.what()));
        }
        throw;
    }
}

std::map<std::string, std::string> AlarmRuleRepository::entityToParams(const AlarmRuleEntity& entity) {
    std::map<std::string, std::string> params;
    
    // 기본 정보
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["name"] = escapeString(entity.getName());
    params["description"] = escapeString(entity.getDescription());
    
    // 🔥 TargetType 변환 - AlarmTypes.h 함수 직접 사용
    params["target_type"] = escapeString(PulseOne::Alarm::targetTypeToString(entity.getTargetType()));
    
    if (entity.getTargetId().has_value()) {
        params["target_id"] = std::to_string(entity.getTargetId().value());
    } else {
        params["target_id"] = "NULL";
    }
    params["target_group"] = escapeString(entity.getTargetGroup());
    
    // 🔥 AlarmType 변환 - AlarmTypes.h 함수 직접 사용
    params["alarm_type"] = escapeString(PulseOne::Alarm::alarmTypeToString(entity.getAlarmType()));
    
    // 🔥 AlarmSeverity 변환 - AlarmTypes.h 함수 직접 사용
    params["severity"] = escapeString(PulseOne::Alarm::severityToString(entity.getSeverity()));
    
    params["priority"] = std::to_string(entity.getPriority());
    
    // Boolean 값들
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["is_latched"] = entity.isLatched() ? "1" : "0";
    params["auto_acknowledge"] = entity.isAutoAcknowledge() ? "1" : "0";
    params["auto_clear"] = entity.isAutoClear() ? "1" : "0";
    
    // 기타 필드들 (기본값으로 설정)
    params["high_high_limit"] = "NULL";
    params["high_limit"] = "NULL";
    params["low_limit"] = "NULL";
    params["low_low_limit"] = "NULL";
    params["deadband"] = "0.0";
    params["rate_of_change"] = "0.0";
    params["trigger_condition"] = "''";
    params["condition_script"] = "''";
    params["message_script"] = "''";
    params["message_config"] = "''";
    params["message_template"] = "''";
    params["acknowledge_timeout_min"] = "0";
    params["suppression_rules"] = "''";
    params["notification_enabled"] = "1";
    params["notification_delay_sec"] = "0";
    params["notification_repeat_interval_min"] = "0";
    params["notification_channels"] = "''";
    params["notification_recipients"] = "''";
    
    return params;
}

bool AlarmRuleRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(CREATE_TABLE_QUERY);
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmRuleRepository::isNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    try {
        auto found = findByName(name, tenant_id, exclude_id);
        return found.has_value();
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleRepository::isNameTaken failed: " + std::string(e.what()));
        }
        return false;
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
        if (logger_) {
            logger_->Error("AlarmRuleRepository::findByName failed: " + std::string(e.what()));
        }
        return std::nullopt;
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
    
    // TODO: 더 상세한 검증 로직 추가
    
    return true;
}

std::string AlarmRuleRepository::escapeString(const std::string& str) {
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

} // namespace Repositories
} // namespace Database
} // namespace PulseOne