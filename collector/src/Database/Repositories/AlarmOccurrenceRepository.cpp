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
#include "Database/RepositoryHelpers.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmOccurrenceRepository::AlarmOccurrenceRepository() 
    : IRepository<Entities::AlarmOccurrenceEntity>("AlarmOccurrenceRepository") {
    
    if (logger_) {
        logger_->Info("AlarmOccurrenceRepository - Constructor");
    }
}

// =============================================================================
// IRepository 인터페이스 구현 (필수)
// =============================================================================

std::vector<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ALL;
        
        auto results = db_layer_.executeQuery(query);
        
        std::vector<Entities::AlarmOccurrenceEntity> entities;
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

std::optional<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findById(int id) {
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
        
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_ID, std::to_string(id));
        
        auto results = db_layer_.executeQuery(query);
        
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

bool AlarmOccurrenceRepository::save(Entities::AlarmOccurrenceEntity& entity) {
    try {
        if (!entity.isValid() || !ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::save - Invalid entity or table");
            }
            return false;
        }
        
        // INSERT 쿼리 생성 (간단화)
        std::ostringstream query;
        query << "INSERT INTO alarm_occurrences (";
        query << "rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition, ";
        query << "alarm_message, severity, state, acknowledge_comment, cleared_value, ";
        query << "clear_comment, notification_sent, notification_count, notification_result, ";
        query << "context_data, source_name, location";
        query << ") VALUES (";
        query << entity.getRuleId() << ", ";
        query << entity.getTenantId() << ", ";
        query << "'" << timePointToString(entity.getOccurrenceTime()) << "', ";
        query << "'" << entity.getTriggerValue() << "', ";
        query << "'" << entity.getTriggerCondition() << "', ";
        query << "'" << entity.getAlarmMessage() << "', ";
        query << "'" << severityToString(entity.getSeverity()) << "', ";
        query << "'" << stateToString(entity.getState()) << "', ";
        query << "'" << entity.getAcknowledgeComment() << "', ";
        query << "'" << entity.getClearedValue() << "', ";
        query << "'" << entity.getClearComment() << "', ";
        query << (entity.isNotificationSent() ? 1 : 0) << ", ";
        query << entity.getNotificationCount() << ", ";
        query << "'" << entity.getNotificationResult() << "', ";
        query << "'" << entity.getContextData() << "', ";
        query << "'" << entity.getSourceName() << "', ";
        query << "'" << entity.getLocation() << "'";
        query << ")";
        
        bool success = db_layer_.executeNonQuery(query.str());
        
        if (success) {
            // 생성된 ID 가져오기
            auto last_id = db_layer_.getLastInsertId();
            if (last_id > 0) {
                entity.setId(static_cast<int>(last_id));
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

bool AlarmOccurrenceRepository::update(const Entities::AlarmOccurrenceEntity& entity) {
    try {
        if (entity.getId() <= 0 || !entity.isValid() || !ensureTableExists()) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceRepository::update - Invalid entity or table");
            }
            return false;
        }
        
        std::ostringstream query;
        query << "UPDATE alarm_occurrences SET ";
        query << "rule_id = " << entity.getRuleId() << ", ";
        query << "tenant_id = " << entity.getTenantId() << ", ";
        query << "occurrence_time = '" << timePointToString(entity.getOccurrenceTime()) << "', ";
        query << "trigger_value = '" << entity.getTriggerValue() << "', ";
        query << "trigger_condition = '" << entity.getTriggerCondition() << "', ";
        query << "alarm_message = '" << entity.getAlarmMessage() << "', ";
        query << "severity = '" << severityToString(entity.getSeverity()) << "', ";
        query << "state = '" << stateToString(entity.getState()) << "', ";
        query << "acknowledge_comment = '" << entity.getAcknowledgeComment() << "', ";
        query << "cleared_value = '" << entity.getClearedValue() << "', ";
        query << "clear_comment = '" << entity.getClearComment() << "', ";
        query << "notification_sent = " << (entity.isNotificationSent() ? 1 : 0) << ", ";
        query << "notification_count = " << entity.getNotificationCount() << ", ";
        query << "notification_result = '" << entity.getNotificationResult() << "', ";
        query << "context_data = '" << entity.getContextData() << "', ";
        query << "source_name = '" << entity.getSourceName() << "', ";
        query << "location = '" << entity.getLocation() << "'";
        
        // Optional 필드들
        if (entity.getAcknowledgedTime().has_value()) {
            query << ", acknowledged_time = '" << timePointToString(entity.getAcknowledgedTime().value()) << "'";
        } else {
            query << ", acknowledged_time = NULL";
        }
        
        if (entity.getAcknowledgedBy().has_value()) {
            query << ", acknowledged_by = " << entity.getAcknowledgedBy().value();
        } else {
            query << ", acknowledged_by = NULL";
        }
        
        if (entity.getClearedTime().has_value()) {
            query << ", cleared_time = '" << timePointToString(entity.getClearedTime().value()) << "'";
        } else {
            query << ", cleared_time = NULL";
        }
        
        if (entity.getNotificationTime().has_value()) {
            query << ", notification_time = '" << timePointToString(entity.getNotificationTime().value()) << "'";
        } else {
            query << ", notification_time = NULL";
        }
        
        query << " WHERE id = " << entity.getId();
        
        bool success = db_layer_.executeNonQuery(query.str());
        
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
        
        std::string query = "DELETE FROM alarm_occurrences WHERE id = " + std::to_string(id);
        
        bool success = db_layer_.executeNonQuery(query);
        
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
        
        std::string query = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE id = " + std::to_string(id);
        
        auto results = db_layer_.executeQuery(query);
        
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
// 벌크 연산 구현
// =============================================================================

std::vector<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByIds(const std::vector<int>& ids) {
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
        
        std::string query = "SELECT * FROM alarm_occurrences WHERE id IN (" + ids_ss.str() + ") ORDER BY occurrence_time DESC";
        
        auto results = db_layer_.executeQuery(query);
        
        std::vector<Entities::AlarmOccurrenceEntity> entities;
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

std::vector<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination
) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::ostringstream query;
        query << "SELECT * FROM alarm_occurrences";
        
        // WHERE 절 추가
        std::string where_clause = buildWhereClause(conditions);
        if (!where_clause.empty()) {
            query << " WHERE " << where_clause;
        }
        
        // ORDER BY 절 추가
        std::string order_clause = buildOrderByClause(order_by);
        if (!order_clause.empty()) {
            query << " " << order_clause;
        } else {
            query << " ORDER BY occurrence_time DESC"; // 기본 정렬
        }
        
        // LIMIT/OFFSET 절 추가
        std::string pagination_clause = buildPaginationClause(pagination);
        if (!pagination_clause.empty()) {
            query << " " << pagination_clause;
        }
        
        auto results = db_layer_.executeQuery(query.str());
        
        std::vector<Entities::AlarmOccurrenceEntity> entities;
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
// 알람 발생 전용 메서드들
// =============================================================================

std::vector<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActive() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_ACTIVE, "");
        
        auto results = db_layer_.executeQuery(query);
        
        std::vector<Entities::AlarmOccurrenceEntity> entities;
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

std::vector<Entities::AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    try {
        if (rule_id <= 0 || !ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(SQL::AlarmOccurrence::FIND_BY_RULE_ID, std::to_string(rule_id));
        
        auto results = db_layer_.executeQuery(query);
        
        std::vector<Entities::AlarmOccurrenceEntity> entities;
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

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

Entities::AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
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
            if (it->second == "low") entity.setSeverity(AlarmOccurrenceEntity::Severity::LOW);
            else if (it->second == "medium") entity.setSeverity(AlarmOccurrenceEntity::Severity::MEDIUM);
            else if (it->second == "high") entity.setSeverity(AlarmOccurrenceEntity::Severity::HIGH);
            else if (it->second == "critical") entity.setSeverity(AlarmOccurrenceEntity::Severity::CRITICAL);
        }
        
        it = row.find("state");
        if (it != row.end()) {
            if (it->second == "active") entity.setState(AlarmOccurrenceEntity::State::ACTIVE);
            else if (it->second == "acknowledged") entity.setState(AlarmOccurrenceEntity::State::ACKNOWLEDGED);
            else if (it->second == "cleared") entity.setState(AlarmOccurrenceEntity::State::CLEARED);
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

std::string AlarmOccurrenceRepository::severityToString(Entities::AlarmOccurrenceEntity::Severity severity) const {
    switch (severity) {
        case Entities::AlarmOccurrenceEntity::Severity::LOW: return "low";
        case Entities::AlarmOccurrenceEntity::Severity::MEDIUM: return "medium";
        case Entities::AlarmOccurrenceEntity::Severity::HIGH: return "high";
        case Entities::AlarmOccurrenceEntity::Severity::CRITICAL: return "critical";
        default: return "medium";
    }
}

std::string AlarmOccurrenceRepository::stateToString(Entities::AlarmOccurrenceEntity::State state) const {
    switch (state) {
        case Entities::AlarmOccurrenceEntity::State::ACTIVE: return "active";
        case Entities::AlarmOccurrenceEntity::State::ACKNOWLEDGED: return "acknowledged";
        case Entities::AlarmOccurrenceEntity::State::CLEARED: return "cleared";
        default: return "active";
    }
}

std::string AlarmOccurrenceRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) {
        return "";
    }
    
    std::ostringstream where_clause;
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) {
            where_clause << " AND ";
        }
        
        const auto& condition = conditions[i];
        where_clause << condition.field << " " << condition.operator_ << " ";
        
        if (condition.operator_ == "IN" || condition.operator_ == "NOT IN") {
            where_clause << "(" << condition.value << ")";
        } else {
            where_clause << "'" << condition.value << "'";
        }
    }
    
    return where_clause.str();
}

std::string AlarmOccurrenceRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) {
        return "";
    }
    
    return "ORDER BY " + order_by->field + " " + order_by->direction;
}

std::string AlarmOccurrenceRepository::buildPaginationClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) {
        return "";
    }
    
    return "LIMIT " + std::to_string(pagination->limit) + " OFFSET " + std::to_string(pagination->offset);
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOnesent");
        if (it != row.end()) {
            entity.setNotificationSent(it->second == "1" || it->second == "true");
        }
        
        it = row.find("notification_time");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setNotificationTime(stringToTimePoint(it->second));
        }
        
        it = row.find("notification_