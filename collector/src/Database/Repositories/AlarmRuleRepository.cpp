// =============================================================================
// collector/src/Database/Repositories/AlarmRuleRepository.cpp
// PulseOne AlarmRuleRepository 구현 - DeviceRepository/DataPointRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmRuleRepository.cpp
 * @brief PulseOne AlarmRuleRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - SQLQueries.h 상수 활용
 * - mapRowToEntity Repository에서 구현
 * - 에러 처리 및 로깅
 * - 캐싱 지원
 */

#include "Database/Repositories/AlarmRuleRepository.h"
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
// IRepository 기본 CRUD 구현 (DeviceRepository 패턴)
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
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::INSERT;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        query = RepositoryHelpers::replaceParametersInOrder(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 새로 생성된 ID 조회
            auto id_results = db_layer.executeQuery(SQL::AlarmRule::GET_LAST_INSERT_ID);
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::UPDATE;
        
        // 파라미터 치환
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId()); // WHERE 절용
        query = RepositoryHelpers::replaceParametersInOrder(query, params);
        
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
        
        // 🎯 SQLQueries.h 상수 사용
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
        
        // 🎯 SQLQueries.h 상수 사용
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
        
        // WHERE 절 추가
        if (!conditions.empty()) {
            // ORDER BY 전에 WHERE 절 삽입
            size_t order_pos = query.find("ORDER BY");
            std::string where_clause = RepositoryHelpers::buildWhereClause(conditions) + " ";
            
            if (order_pos != std::string::npos) {
                query.insert(order_pos, where_clause);
            }
        }
        
        // ORDER BY 절 커스터마이징 (필요한 경우)
        if (order_by.has_value()) {
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                std::string new_order = RepositoryHelpers::buildOrderByClause(order_by);
                size_t order_end = query.find('\n', order_pos);
                if (order_end == std::string::npos) order_end = query.length();
                query.replace(order_pos, order_end - order_pos, new_order);
            }
        }
        
        // LIMIT 절 추가
        if (pagination.has_value()) {
            query += " " + RepositoryHelpers::buildLimitClause(pagination);
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
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

// =============================================================================
// AlarmRule 전용 메서드들 구현
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByTarget(const std::string& target_type, int target_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::FIND_BY_TARGET;
        query = RepositoryHelpers::replaceTwoParameters(query, target_type, std::to_string(target_id));
        
        if (enabled_only) {
            // AND is_enabled = 1 추가
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, "AND is_enabled = 1 ");
            }
        }
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::FIND_BY_TENANT;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(tenant_id));
        
        if (enabled_only) {
            // AND is_enabled = 1 추가
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, "AND is_enabled = 1 ");
            }
        }
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::FIND_BY_SEVERITY;
        query = RepositoryHelpers::replaceParameterWithQuotes(query, severity);
        
        if (enabled_only) {
            // AND is_enabled = 1 추가
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, "AND is_enabled = 1 ");
            }
        }
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::AlarmRule::FIND_BY_ALARM_TYPE;
        query = RepositoryHelpers::replaceParameterWithQuotes(query, alarm_type);
        
        if (enabled_only) {
            // AND is_enabled = 1 추가
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, "AND is_enabled = 1 ");
            }
        }
        
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
        
        // 🎯 SQLQueries.h 상수 사용
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
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

AlarmRuleEntity AlarmRuleRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmRuleEntity entity;
        auto it = row.end();
        
        // 기본 정보
        it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
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
        if (it != row.end() && !it->second.empty()) {
            entity.setTargetType(AlarmRuleEntity::stringToTargetType(it->second));
        }
        
        it = row.find("target_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setTargetId(std::stoi(it->second));
        }
        
        it = row.find("target_group");
        if (it != row.end()) {
            entity.setTargetGroup(it->second);
        }
        
        // 알람 타입
        it = row.find("alarm_type");
        if (it != row.end() && !it->second.empty()) {
            entity.setAlarmType(AlarmRuleEntity::stringToAlarmType(it->second));
        }
        
        // 아날로그 설정
        it = row.find("high_high_limit");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setHighHighLimit(std::stod(it->second));
        }
        
        it = row.find("high_limit");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setHighLimit(std::stod(it->second));
        }
        
        it = row.find("low_limit");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setLowLimit(std::stod(it->second));
        }
        
        it = row.find("low_low_limit");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setLowLowLimit(std::stod(it->second));
        }
        
        it = row.find("deadband");
        if (it != row.end() && !it->second.empty()) {
            entity.setDeadband(std::stod(it->second));
        }
        
        it = row.find("rate_of_change");
        if (it != row.end() && !it->second.empty()) {
            entity.setRateOfChange(std::stod(it->second));
        }
        
        // 디지털 설정
        it = row.find("trigger_condition");
        if (it != row.end() && !it->second.empty()) {
            entity.setTriggerCondition(AlarmRuleEntity::stringToDigitalTrigger(it->second));
        }
        
        // 스크립트 설정
        it = row.find("condition_script");
        if (it != row.end()) {
            entity.setConditionScript(it->second);
        }
        
        it = row.find("message_script");
        if (it != row.end()) {
            entity.setMessageScript(it->second);
        }
        
        // 메시지 설정
        it = row.find("message_config");
        if (it != row.end()) {
            entity.setMessageConfig(it->second);
        }
        
        it = row.find("message_template");
        if (it != row.end()) {
            entity.setMessageTemplate(it->second);
        }
        
        // 우선순위
        it = row.find("severity");
        if (it != row.end() && !it->second.empty()) {
            entity.setSeverity(AlarmRuleEntity::stringToSeverity(it->second));
        }
        
        it = row.find("priority");
        if (it != row.end() && !it->second.empty()) {
            entity.setPriority(std::stoi(it->second));
        }
        
        // 자동 처리
        it = row.find("auto_acknowledge");
        if (it != row.end() && !it->second.empty()) {
            entity.setAutoAcknowledge(it->second == "1");
        }
        
        it = row.find("acknowledge_timeout_min");
        if (it != row.end() && !it->second.empty()) {
            entity.setAcknowledgeTimeoutMin(std::stoi(it->second));
        }
        
        it = row.find("auto_clear");
        if (it != row.end() && !it->second.empty()) {
            entity.setAutoClear(it->second == "1");
        }
        
        // 억제 규칙
        it = row.find("suppression_rules");
        if (it != row.end()) {
            entity.setSuppressionRules(it->second);
        }
        
        // 알림 설정
        it = row.find("notification_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setNotificationEnabled(it->second == "1");
        }
        
        it = row.find("notification_delay_sec");
        if (it != row.end() && !it->second.empty()) {
            entity.setNotificationDelaySec(std::stoi(it->second));
        }
        
        it = row.find("notification_repeat_interval_min");
        if (it != row.end() && !it->second.empty()) {
            entity.setNotificationRepeatIntervalMin(std::stoi(it->second));
        }
        
        it = row.find("notification_channels");
        if (it != row.end()) {
            entity.setNotificationChannels(it->second);
        }
        
        it = row.find("notification_recipients");
        if (it != row.end()) {
            entity.setNotificationRecipients(it->second);
        }
        
        // 상태
        it = row.find("is_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setEnabled(it->second == "1");
        }
        
        it = row.find("is_latched");
        if (it != row.end() && !it->second.empty()) {
            entity.setLatched(it->second == "1");
        }
        
        // 타임스탬프
        it = row.find("created_by");
        if (it != row.end() && !it->second.empty()) {
            entity.setCreatedBy(std::stoi(it->second));
        }
        
        // TODO: created_at, updated_at 파싱 (ISO 8601 형식)
        
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
    
    // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["name"] = escapeString(entity.getName());
    params["description"] = escapeString(entity.getDescription());
    
    // 대상 정보
    params["target_type"] = escapeString(AlarmRuleEntity::targetTypeToString(entity.getTargetType()));
    if (entity.getTargetId().has_value()) {
        params["target_id"] = std::to_string(entity.getTargetId().value());
    } else {
        params["target_id"] = "NULL";
    }
    params["target_group"] = escapeString(entity.getTargetGroup());
    
    // 알람 타입
    params["alarm_type"] = escapeString(AlarmRuleEntity::alarmTypeToString(entity.getAlarmType()));
    
    // 아날로그 설정
    if (entity.getHighHighLimit().has_value()) {
        params["high_high_limit"] = std::to_string(entity.getHighHighLimit().value());
    } else {
        params["high_high_limit"] = "NULL";
    }
    
    if (entity.getHighLimit().has_value()) {
        params["high_limit"] = std::to_string(entity.getHighLimit().value());
    } else {
        params["high_limit"] = "NULL";
    }
    
    if (entity.getLowLimit().has_value()) {
        params["low_limit"] = std::to_string(entity.getLowLimit().value());
    } else {
        params["low_limit"] = "NULL";
    }
    
    if (entity.getLowLowLimit().has_value()) {
        params["low_low_limit"] = std::to_string(entity.getLowLowLimit().value());
    } else {
        params["low_low_limit"] = "NULL";
    }
    
    params["deadband"] = std::to_string(entity.getDeadband());
    params["rate_of_change"] = std::to_string(entity.getRateOfChange());
    
    // 디지털 설정
    params["trigger_condition"] = escapeString(AlarmRuleEntity::digitalTriggerToString(entity.getTriggerCondition()));
    
    // 스크립트 설정
    params["condition_script"] = escapeString(entity.getConditionScript());
    params["message_script"] = escapeString(entity.getMessageScript());
    
    // 메시지 설정
    params["message_config"] = escapeString(entity.getMessageConfig());
    params["message_template"] = escapeString(entity.getMessageTemplate());
    
    // 우선순위
    params["severity"] = escapeString(AlarmRuleEntity::severityToString(entity.getSeverity()));
    params["priority"] = std::to_string(entity.getPriority());
    
    // 자동 처리
    params["auto_acknowledge"] = entity.isAutoAcknowledge() ? "1" : "0";
    params["acknowledge_timeout_min"] = std::to_string(entity.getAcknowledgeTimeoutMin());
    params["auto_clear"] = entity.isAutoClear() ? "1" : "0";
    
    // 억제 규칙
    params["suppression_rules"] = escapeString(entity.getSuppressionRules());
    
    // 알림 설정
    params["notification_enabled"] = entity.isNotificationEnabled() ? "1" : "0";
    params["notification_delay_sec"] = std::to_string(entity.getNotificationDelaySec());
    params["notification_repeat_interval_min"] = std::to_string(entity.getNotificationRepeatIntervalMin());
    params["notification_channels"] = escapeString(entity.getNotificationChannels());
    params["notification_recipients"] = escapeString(entity.getNotificationRecipients());
    
    // 상태
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["is_latched"] = entity.isLatched() ? "1" : "0";
    params["created_by"] = std::to_string(entity.getCreatedBy());
    
    return params;
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

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
    
    // 알람 타입별 검증
    if (entity.getAlarmType() == AlarmRuleEntity::AlarmType::ANALOG) {
        // 아날로그 알람은 최소한 하나의 리미트가 설정되어야 함
        if (!entity.getHighHighLimit().has_value() && 
            !entity.getHighLimit().has_value() && 
            !entity.getLowLimit().has_value() && 
            !entity.getLowLowLimit().has_value()) {
            return false;
        }
    }
    
    // 대상 검증
    if (entity.getTargetType() != AlarmRuleEntity::TargetType::GROUP) {
        if (!entity.getTargetId().has_value()) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

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

std::map<std::string, int> AlarmRuleRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        // IRepository에서 캐시 통계 조회
        auto base_stats = IRepository<AlarmRuleEntity>::getCacheStats();
        stats.insert(base_stats.begin(), base_stats.end());
    }
    
    return stats;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne