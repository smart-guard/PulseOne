// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - DeviceRepository 패턴 100% 준수
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne 알람 발생 이력 Repository 구현
 * @author PulseOne Development Team
 * @date 2025-08-10
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/SQLQueries.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 인터페이스 구현 (DeviceRepository 패턴)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
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
        if (id <= 0 || !ensureTableExists()) {
            return std::nullopt;
        }
        
        // 캐시 확인 (IRepository 패턴)
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                if (logger_) {
                    logger_->Debug("AlarmOccurrenceRepository::findById - Cache hit for ID: " + std::to_string(id));
                }
                return cached.value();
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_ID, std::to_string(id));
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            if (logger_) {
                logger_->Debug("AlarmOccurrenceRepository::findById - No alarm occurrence found for ID: " + std::to_string(id));
            }
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장 (IRepository 패턴)
        if (isCacheEnabled()) {
            cacheEntity(id, entity);
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findById - Found alarm occurrence: " + std::to_string(id));
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
        if (!entity.isValid() || !ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::save - Invalid entity or table");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQLQueries.h의 INSERT 쿼리 사용 + 파라미터 바인딩
        std::string query = SQL::AlarmOccurrence::INSERT;
        
        // 파라미터 값들 준비
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            RepositoryHelpers::escapeString(entity.getTriggerValue()),
            RepositoryHelpers::escapeString(entity.getTriggerCondition()),
            RepositoryHelpers::escapeString(entity.getAlarmMessage()),
            AlarmOccurrenceEntity::severityToString(entity.getSeverity()),
            AlarmOccurrenceEntity::stateToString(entity.getState()),
            RepositoryHelpers::escapeString(entity.getAcknowledgeComment()),
            RepositoryHelpers::escapeString(entity.getClearedValue()),
            RepositoryHelpers::escapeString(entity.getClearComment()),
            entity.isNotificationSent() ? "1" : "0",
            std::to_string(entity.getNotificationCount()),
            RepositoryHelpers::escapeString(entity.getNotificationResult()),
            RepositoryHelpers::escapeString(entity.getContextData()),
            RepositoryHelpers::escapeString(entity.getSourceName()),
            RepositoryHelpers::escapeString(entity.getLocation())
        };
        
        // RepositoryHelpers로 파라미터 치환
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // SQLQueries.h의 GET_LAST_INSERT_ID 사용
            auto last_id_results = db_layer.executeQuery(SQL::AlarmOccurrence::GET_LAST_INSERT_ID);
            if (!last_id_results.empty() && last_id_results[0].find("id") != last_id_results[0].end()) {
                int last_id = std::stoi(last_id_results[0].at("id"));
                if (last_id > 0) {
                    entity.setId(last_id);
                }
            }
            
            // 캐시에 저장 (IRepository 패턴)
            if (isCacheEnabled() && entity.getId() > 0) {
                cacheEntity(entity.getId(), entity);
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::save - Saved alarm occurrence: " + std::to_string(entity.getId()));
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
        if (entity.getId() <= 0 || !entity.isValid() || !ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::update - Invalid entity or table");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQLQueries.h의 UPDATE 쿼리 사용
        std::string query = SQL::AlarmOccurrence::UPDATE;
        
        // 파라미터 값들 준비 (UPDATE 쿼리 순서에 맞게)
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            RepositoryHelpers::escapeString(entity.getTriggerValue()),
            RepositoryHelpers::escapeString(entity.getTriggerCondition()),
            RepositoryHelpers::escapeString(entity.getAlarmMessage()),
            AlarmOccurrenceEntity::severityToString(entity.getSeverity()),
            AlarmOccurrenceEntity::stateToString(entity.getState()),
            // Optional 필드들 처리
            entity.getAcknowledgedTime().has_value() ? 
                timePointToString(entity.getAcknowledgedTime().value()) : "NULL",
            entity.getAcknowledgedBy().has_value() ? 
                std::to_string(entity.getAcknowledgedBy().value()) : "NULL",
            RepositoryHelpers::escapeString(entity.getAcknowledgeComment()),
            entity.getClearedTime().has_value() ? 
                timePointToString(entity.getClearedTime().value()) : "NULL",
            RepositoryHelpers::escapeString(entity.getClearedValue()),
            RepositoryHelpers::escapeString(entity.getClearComment()),
            entity.isNotificationSent() ? "1" : "0",
            entity.getNotificationTime().has_value() ? 
                timePointToString(entity.getNotificationTime().value()) : "NULL",
            std::to_string(entity.getNotificationCount()),
            RepositoryHelpers::escapeString(entity.getNotificationResult()),
            RepositoryHelpers::escapeString(entity.getContextData()),
            RepositoryHelpers::escapeString(entity.getSourceName()),
            RepositoryHelpers::escapeString(entity.getLocation()),
            std::to_string(entity.getId())  // WHERE 절의 ID
        };
        
        // RepositoryHelpers로 파라미터 치환
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 업데이트 (IRepository 패턴)
            if (isCacheEnabled()) {
                cacheEntity(entity.getId(), entity);
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::update - Updated alarm occurrence: " + std::to_string(entity.getId()));
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
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::DELETE_BY_ID, std::to_string(id));
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거 (IRepository 패턴)
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::deleteById - Deleted alarm occurrence: " + std::to_string(id));
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
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::EXISTS_BY_ID, std::to_string(id));
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
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
// 벌크 연산 구현 (DeviceRepository 패턴)
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
        
        DatabaseAbstractionLayer db_layer;
        std::string query = "SELECT * FROM alarm_occurrences WHERE id IN (" + ids_ss.str() + ") ORDER BY occurrence_time DESC";
        
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
            logger_->Info("AlarmOccurrenceRepository::findByIds - Found " + std::to_string(entities.size()) + " alarm occurrences");
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
    const std::optional<Pagination>& pagination
) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::ostringstream query;
        query << "SELECT * FROM alarm_occurrences";
        
        // WHERE 절 추가 (RepositoryHelpers 사용)
        std::string where_clause = RepositoryHelpers::buildWhereClause(conditions);
        if (!where_clause.empty()) {
            query << where_clause;
        }
        
        // ORDER BY 절 추가 (RepositoryHelpers 사용)
        std::string order_clause = RepositoryHelpers::buildOrderByClause(order_by);
        if (!order_clause.empty()) {
            query << order_clause;
        } else {
            query << " ORDER BY occurrence_time DESC"; // 기본 정렬
        }
        
        // LIMIT/OFFSET 절 추가 (RepositoryHelpers 사용)
        std::string pagination_clause = RepositoryHelpers::buildLimitClause(pagination);
        if (!pagination_clause.empty()) {
            query << pagination_clause;
        }
        
        auto results = db_layer.executeQuery(query.str());
        
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

// =============================================================================
// AlarmOccurrence 전용 메서드들 (SQLQueries 상수 사용)
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

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    try {
        if (rule_id <= 0 || !ensureTableExists()) {
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
            logger_->Info("AlarmOccurrenceRepository::findByRuleId - Found " + std::to_string(entities.size()) + " alarm occurrences for rule " + std::to_string(rule_id));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByRuleId failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findBySeverity(AlarmOccurrenceEntity::Severity severity) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        // SQLQueries.h의 FIND_BY_SEVERITY 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_SEVERITY, 
                                                               AlarmOccurrenceEntity::severityToString(severity));
        
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
            logger_->Info("AlarmOccurrenceRepository::findBySeverity - Found " + std::to_string(entities.size()) + " alarm occurrences");
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findBySeverity failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    using namespace Entities;
    
    AlarmOccurrenceEntity entity;
    
    try {
        // 기본 필드들
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("rule_id");
        if (it != row.end()) {
            entity.setRuleId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("occurrence_time");
        if (it != row.end() && !it->second.empty()) {
            entity.setOccurrenceTime(stringToTimePoint(it->second));
        }
        
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
        
        it = row.find("severity");
        if (it != row.end()) {
            entity.setSeverity(stringToSeverity(it->second));
        }
        
        it = row.find("state");
        if (it != row.end()) {
            entity.setState(stringToState(it->second));
        }
        
        // Optional 필드들
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
        
        it = row.find("notification_sent");
        if (it != row.end()) {
            entity.setNotificationSent(it->second == "1" || it->second == "true");
        }
        
        it = row.find("notification_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setNotificationTime(stringToTimePoint(it->second));
        }
        
        it = row.find("notification_count");
        if (it != row.end()) {
            entity.setNotificationCount(std::stoi(it->second));
        }
        
        it = row.find("notification_result");
        if (it != row.end()) {
            entity.setNotificationResult(it->second);
        }
        
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

bool AlarmOccurrenceRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        // SQLQueries.h의 CREATE_TABLE 사용
        bool success = db_layer.executeCreateTable(SQL::AlarmOccurrence::CREATE_TABLE);
        
        if (success && logger_) {
            logger_->Debug("AlarmOccurrenceRepository::ensureTableExists - Table alarm_occurrences ready");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

std::string AlarmOccurrenceRepository::timePointToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point AlarmOccurrenceRepository::stringToTimePoint(const std::string& str) const {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    auto time_t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t);
}

std::string AlarmOccurrenceRepository::severityToString(AlarmOccurrenceEntity::Severity severity) const {
    return AlarmOccurrenceEntity::severityToString(severity);
}

AlarmOccurrenceEntity::Severity AlarmOccurrenceRepository::stringToSeverity(const std::string& str) const {
    return AlarmOccurrenceEntity::stringToSeverity(str);
}

std::string AlarmOccurrenceRepository::stateToString(AlarmOccurrenceEntity::State state) const {
    return AlarmOccurrenceEntity::stateToString(state);
}

AlarmOccurrenceEntity::State AlarmOccurrenceRepository::stringToState(const std::string& str) const {
    return AlarmOccurrenceEntity::stringToState(str);
}

