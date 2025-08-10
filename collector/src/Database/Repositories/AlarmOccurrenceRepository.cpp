// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - SQLQueries.h 사용 (AlarmRuleRepository 패턴)
// =============================================================================

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 (AlarmRuleRepository 패턴)
using AlarmOccurrenceEntity = Entities::AlarmOccurrenceEntity;

// =============================================================================
// IRepository 인터페이스 구현 (SQLQueries.h 사용)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ALL);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            results.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findAll - Found " + std::to_string(results.size()) + " occurrences");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findAll failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findById(int id) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_BY_ID, {std::to_string(id)});
        
        if (!rows.empty()) {
            auto entity = mapRowToEntity(rows[0]);
            
            // 캐시에 저장
            setCachedEntity(id, entity);
            
            if (logger_) {
                logger_->Debug("AlarmOccurrenceRepository::findById - Found occurrence: " + std::to_string(id));
            }
            
            return entity;
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findById - Occurrence not found: " + std::to_string(id));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findById failed: " + std::string(e.what()));
        }
    }
    
    return std::nullopt;
}

bool AlarmOccurrenceRepository::save(AlarmOccurrenceEntity& entity) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // INSERT 쿼리 실행
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            entity.getTriggerValue(),
            entity.getTriggerCondition(),
            entity.getAlarmMessage(),
            entity.getSeverityString(),
            entity.getStateString(),
            entity.getAcknowledgeComment(),
            entity.getClearedValue(),
            entity.getClearComment(),
            entity.isNotificationSent() ? "1" : "0",
            std::to_string(entity.getNotificationCount()),
            entity.getNotificationResult(),
            entity.getContextData(),
            entity.getSourceName(),
            entity.getLocation()
        };
        
        bool success = db_layer.executeUpdate(SQL::AlarmOccurrence::INSERT, params);
        
        if (success) {
            // 새로 생성된 ID 가져오기
            auto id_rows = db_layer.executeQuery(SQL::AlarmOccurrence::GET_LAST_INSERT_ID);
            if (!id_rows.empty() && id_rows[0].count("id")) {
                entity.setId(std::stoi(id_rows[0].at("id")));
            }
            
            // 캐시에 저장
            setCachedEntity(entity.getId(), entity);
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::save - Saved occurrence: " + std::to_string(entity.getId()));
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
        DatabaseAbstractionLayer db_layer;
        
        // UPDATE 쿼리 실행
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            entity.getTriggerValue(),
            entity.getTriggerCondition(),
            entity.getAlarmMessage(),
            entity.getSeverityString(),
            entity.getStateString(),
            entity.getAcknowledgeComment(),
            entity.getClearedValue(),
            entity.getClearComment(),
            entity.isNotificationSent() ? "1" : "0",
            std::to_string(entity.getNotificationCount()),
            entity.getNotificationResult(),
            entity.getContextData(),
            entity.getSourceName(),
            entity.getLocation(),
            std::to_string(entity.getId())  // WHERE 조건
        };
        
        bool success = db_layer.executeUpdate(SQL::AlarmOccurrence::UPDATE, params);
        
        if (success) {
            // 캐시 업데이트
            setCachedEntity(entity.getId(), entity);
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::update - Updated occurrence: " + std::to_string(entity.getId()));
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
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeUpdate(SQL::AlarmOccurrence::DELETE_BY_ID, {std::to_string(id)});
        
        if (success) {
            // 캐시에서 제거
            clearCacheForId(id);
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::deleteById - Deleted occurrence: " + std::to_string(id));
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
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::EXISTS_BY_ID, {std::to_string(id)});
        
        if (!rows.empty() && rows[0].count("count")) {
            return std::stoi(rows[0].at("count")) > 0;
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::exists failed: " + std::string(e.what()));
        }
    }
    
    return false;
}

// =============================================================================
// 벌크 연산 (IRepository 기본 구현 사용)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByIds(const std::vector<int>& ids) {
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::findByIds(ids);
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::findByConditions(conditions, order_by, pagination);
}

int AlarmOccurrenceRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::countByConditions(conditions);
}

int AlarmOccurrenceRepository::saveBulk(std::vector<AlarmOccurrenceEntity>& entities) {
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::saveBulk(entities);
}

int AlarmOccurrenceRepository::updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) {
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::updateBulk(entities);
}

int AlarmOccurrenceRepository::deleteByIds(const std::vector<int>& ids) {
    // IRepository 기본 구현 사용
    return IRepository<AlarmOccurrenceEntity>::deleteByIds(ids);
}

// =============================================================================
// AlarmOccurrence 전용 메서드들
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActive() {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ACTIVE);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            results.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findActive - Found " + std::to_string(results.size()) + " active occurrences");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findActive failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_BY_RULE_ID, {std::to_string(rule_id)});
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            results.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findByRuleId - Found " + std::to_string(results.size()) + " occurrences for rule " + std::to_string(rule_id));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findByRuleId failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findActiveByTenantId(int tenant_id) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ACTIVE_BY_TENANT_ID, {std::to_string(tenant_id)});
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            results.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findActiveByTenantId - Found " + std::to_string(results.size()) + " active occurrences for tenant " + std::to_string(tenant_id));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findActiveByTenantId failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findBySeverity(AlarmOccurrenceEntity::Severity severity) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_BY_SEVERITY, {severityToString(severity)});
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            results.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findBySeverity - Found " + std::to_string(results.size()) + " occurrences with severity " + severityToString(severity));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findBySeverity failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

bool AlarmOccurrenceRepository::acknowledge(int occurrence_id, int user_id, const std::string& comment) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            "acknowledged",  // state
            timePointToString(std::chrono::system_clock::now()),  // acknowledged_time
            std::to_string(user_id),  // acknowledged_by
            comment,  // acknowledge_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        bool success = db_layer.executeUpdate(SQL::AlarmOccurrence::ACKNOWLEDGE, params);
        
        if (success) {
            // 캐시 클리어 (다음 조회시 DB에서 최신 데이터 가져오기)
            clearCacheForId(occurrence_id);
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::acknowledge - Acknowledged occurrence " + std::to_string(occurrence_id) + " by user " + std::to_string(user_id));
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

bool AlarmOccurrenceRepository::clear(int occurrence_id, const std::string& cleared_value, const std::string& comment) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            "cleared",  // state
            timePointToString(std::chrono::system_clock::now()),  // cleared_time
            cleared_value,  // cleared_value
            comment,  // clear_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        bool success = db_layer.executeUpdate(SQL::AlarmOccurrence::CLEAR, params);
        
        if (success) {
            // 캐시 클리어 (다음 조회시 DB에서 최신 데이터 가져오기)
            clearCacheForId(occurrence_id);
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceRepository::clear - Cleared occurrence " + std::to_string(occurrence_id));
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

std::optional<int64_t> AlarmOccurrenceRepository::findMaxId() {
    try {
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery("SELECT MAX(id) as max_id FROM alarm_occurrences");
        
        if (!rows.empty() && rows[0].count("max_id") && !rows[0].at("max_id").empty()) {
            return std::stoll(rows[0].at("max_id"));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findMaxId failed: " + std::string(e.what()));
        }
    }
    
    return std::nullopt;
}

// =============================================================================
// 테이블 관리
// =============================================================================

bool AlarmOccurrenceRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
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

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        AlarmOccurrenceEntity entity;
        
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
        if (it != row.end()) {
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
        if (it != row.end() && !it->second.empty()) {
            entity.setAcknowledgedTime(stringToTimePoint(it->second));
        }
        
        it = row.find("acknowledged_by");
        if (it != row.end() && !it->second.empty()) {
            entity.setAcknowledgedBy(std::stoi(it->second));
        }
        
        it = row.find("acknowledge_comment");
        if (it != row.end()) {
            entity.setAcknowledgeComment(it->second);
        }
        
        it = row.find("cleared_time");
        if (it != row.end() && !it->second.empty()) {
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
        if (it != row.end()) {
            entity.setNotificationSent(it->second == "1");
        }
        
        it = row.find("notification_time");
        if (it != row.end() && !it->second.empty()) {
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

} // namespace Repositories
} // namespace Database
} // namespace PulseOne