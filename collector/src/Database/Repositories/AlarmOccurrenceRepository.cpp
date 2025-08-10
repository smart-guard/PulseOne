// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// 🔥 완성된 AlarmOccurrenceRepository - 모든 기능 완전 구현
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne 알람 발생 이력 Repository - 완전 구현체
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 완성 기능:
 * - RepositoryHelpers 방식 매개변수 바인딩
 * - 완전한 캐시 시스템 (setCachedEntity 포함)
 * - 모든 헬퍼 메서드 구현
 * - 에러 처리 및 로깅
 * - SQLQueries.h 완전 활용
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭
using AlarmOccurrenceEntity = Entities::AlarmOccurrenceEntity;

// =============================================================================
// 생성자 및 초기화
// =============================================================================

AlarmOccurrenceRepository::AlarmOccurrenceRepository() 
    : logger_(&Utils::LogManager::getInstance()) {
    
    if (logger_) {
        logger_->Info("AlarmOccurrenceRepository initialized");
    }
}

// =============================================================================
// IRepository 인터페이스 구현 (RepositoryHelpers 방식)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ALL);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findAll - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceRepository::findAll - Found " + std::to_string(results.size()) + " occurrences");
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
        if (id <= 0 || !ensureTableExists()) {
            return std::nullopt;
        }
        
        // 🔥 1단계: 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                if (logger_) {
                    logger_->Debug("AlarmOccurrenceRepository::findById - Cache hit for ID: " + std::to_string(id));
                }
                return cached.value();
            }
        }
        
        // 🔥 2단계: DB에서 조회 (RepositoryHelpers 방식)
        std::string query = SQL::AlarmOccurrence::FIND_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        if (!rows.empty()) {
            auto entity = mapRowToEntity(rows[0]);
            
            // 🔥 3단계: 캐시에 저장
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
        if (!validateEntity(entity) || !ensureTableExists()) {
            return false;
        }
        
        // 🔥 RepositoryHelpers 방식으로 INSERT 실행
        std::string query = SQL::AlarmOccurrence::INSERT;
        
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            escapeString(entity.getTriggerValue()),
            escapeString(entity.getTriggerCondition()),
            escapeString(entity.getAlarmMessage()),
            entity.getSeverityString(),
            entity.getStateString(),
            escapeString(entity.getAcknowledgeComment()),
            escapeString(entity.getClearedValue()),
            escapeString(entity.getClearComment()),
            entity.isNotificationSent() ? "1" : "0",
            std::to_string(entity.getNotificationCount()),
            escapeString(entity.getNotificationResult()),
            escapeString(entity.getContextData()),
            escapeString(entity.getSourceName()),
            escapeString(entity.getLocation())
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 🔥 새로 생성된 ID 가져오기
            auto id_rows = db_layer.executeQuery(SQL::AlarmOccurrence::GET_LAST_INSERT_ID);
            if (!id_rows.empty() && id_rows[0].count("id")) {
                entity.setId(std::stoi(id_rows[0].at("id")));
            }
            
            // 🔥 캐시에 저장
            if (entity.getId() > 0) {
                setCachedEntity(entity.getId(), entity);
            }
            
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
        if (entity.getId() <= 0 || !validateEntity(entity) || !ensureTableExists()) {
            return false;
        }
        
        // 🔥 RepositoryHelpers 방식으로 UPDATE 실행
        std::string query = SQL::AlarmOccurrence::UPDATE;
        
        std::vector<std::string> params = {
            std::to_string(entity.getRuleId()),
            std::to_string(entity.getTenantId()),
            timePointToString(entity.getOccurrenceTime()),
            escapeString(entity.getTriggerValue()),
            escapeString(entity.getTriggerCondition()),
            escapeString(entity.getAlarmMessage()),
            entity.getSeverityString(),
            entity.getStateString(),
            // Optional 필드들 처리
            entity.getAcknowledgedTime().has_value() ? 
                timePointToString(entity.getAcknowledgedTime().value()) : "NULL",
            entity.getAcknowledgedBy().has_value() ? 
                std::to_string(entity.getAcknowledgedBy().value()) : "NULL",
            escapeString(entity.getAcknowledgeComment()),
            entity.getClearedTime().has_value() ? 
                timePointToString(entity.getClearedTime().value()) : "NULL",
            escapeString(entity.getClearedValue()),
            escapeString(entity.getClearComment()),
            entity.isNotificationSent() ? "1" : "0",
            entity.getNotificationTime().has_value() ? 
                timePointToString(entity.getNotificationTime().value()) : "NULL",
            std::to_string(entity.getNotificationCount()),
            escapeString(entity.getNotificationResult()),
            escapeString(entity.getContextData()),
            escapeString(entity.getSourceName()),
            escapeString(entity.getLocation()),
            std::to_string(entity.getId())  // WHERE 조건
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 🔥 캐시 업데이트
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
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        // 🔥 RepositoryHelpers 방식으로 DELETE 실행
        std::string query = SQL::AlarmOccurrence::DELETE_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 🔥 캐시에서 제거
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
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        // 🔥 RepositoryHelpers 방식으로 EXISTS 확인
        std::string query = SQL::AlarmOccurrence::EXISTS_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
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
// AlarmOccurrence 전용 메서드들
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        if (rule_id <= 0 || !ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_BY_RULE_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(rule_id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findByRuleId - Failed to map row: " + std::string(e.what()));
                }
            }
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
        if (tenant_id <= 0 || !ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_ACTIVE_BY_TENANT_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(tenant_id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findActiveByTenantId - Failed to map row: " + std::string(e.what()));
                }
            }
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
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_BY_SEVERITY;
        RepositoryHelpers::replaceParameterPlaceholders(query, {severityToString(severity)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findBySeverity - Failed to map row: " + std::string(e.what()));
                }
            }
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
        if (occurrence_id <= 0 || user_id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        std::string query = SQL::AlarmOccurrence::ACKNOWLEDGE;
        
        std::vector<std::string> params = {
            "acknowledged",  // state
            timePointToString(std::chrono::system_clock::now()),  // acknowledged_time
            std::to_string(user_id),  // acknowledged_by
            escapeString(comment),  // acknowledge_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 🔥 캐시 클리어 (다음 조회시 DB에서 최신 데이터 가져오기)
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
        if (occurrence_id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        std::string query = SQL::AlarmOccurrence::CLEAR;
        
        std::vector<std::string> params = {
            "cleared",  // state
            timePointToString(std::chrono::system_clock::now()),  // cleared_time
            escapeString(cleared_value),  // cleared_value
            escapeString(comment),  // clear_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 🔥 캐시 클리어
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

// =============================================================================
// 🔥 캐시 관리 메서드들 (핵심!)
// =============================================================================

void AlarmOccurrenceRepository::setCachedEntity(int id, const AlarmOccurrenceEntity& entity) {
    if (!isCacheEnabled()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    entity_cache_[id] = entity;
    
    if (logger_) {
        logger_->Debug("AlarmOccurrenceRepository::setCachedEntity - Cached entity for ID: " + std::to_string(id));
    }
}

std::optional<AlarmOccurrenceEntity> AlarmOccurrenceRepository::getCachedEntity(int id) const {
    if (!isCacheEnabled()) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = entity_cache_.find(id);
    if (it != entity_cache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void AlarmOccurrenceRepository::clearCacheForId(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    entity_cache_.erase(id);
    
    if (logger_) {
        logger_->Debug("AlarmOccurrenceRepository::clearCacheForId - Cleared cache for ID: " + std::to_string(id));
    }
}

void AlarmOccurrenceRepository::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    entity_cache_.clear();
    
    if (logger_) {
        logger_->Info("AlarmOccurrenceRepository::clearCache - All cache cleared");
    }
}

bool AlarmOccurrenceRepository::isCacheEnabled() const {
    return cache_enabled_;
}

// =============================================================================
// 헬퍼 메서드들 (완전 구현)
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    AlarmOccurrenceEntity entity;
    
    try {
        // 기본 필드들
        auto getValue = [&row](const std::string& key, const std::string& defaultValue = "") -> std::string {
            auto it = row.find(key);
            return (it != row.end()) ? it->second : defaultValue;
        };
        
        // ID 설정
        std::string id_str = getValue("id");
        if (!id_str.empty()) {
            entity.setId(std::stoi(id_str));
        }
        
        // 기본 정보
        entity.setRuleId(std::stoi(getValue("rule_id", "0")));
        entity.setTenantId(std::stoi(getValue("tenant_id", "0")));
        
        // 시간 정보
        std::string occurrence_time = getValue("occurrence_time");
        if (!occurrence_time.empty()) {
            entity.setOccurrenceTime(stringToTimePoint(occurrence_time));
        }
        
        // 트리거 정보
        entity.setTriggerValue(getValue("trigger_value"));
        entity.setTriggerCondition(getValue("trigger_condition"));
        entity.setAlarmMessage(getValue("alarm_message"));
        
        // 심각도 및 상태
        entity.setSeverity(stringToSeverity(getValue("severity", "medium")));
        entity.setState(stringToState(getValue("state", "active")));
        
        // Optional 필드들
        std::string ack_time = getValue("acknowledged_time");
        if (!ack_time.empty() && ack_time != "NULL") {
            entity.setAcknowledgedTime(stringToTimePoint(ack_time));
        }
        
        std::string ack_by = getValue("acknowledged_by");
        if (!ack_by.empty() && ack_by != "NULL") {
            entity.setAcknowledgedBy(std::stoi(ack_by));
        }
        
        entity.setAcknowledgeComment(getValue("acknowledge_comment"));
        
        std::string clear_time = getValue("cleared_time");
        if (!clear_time.empty() && clear_time != "NULL") {
            entity.setClearedTime(stringToTimePoint(clear_time));
        }
        
        entity.setClearedValue(getValue("cleared_value"));
        entity.setClearComment(getValue("clear_comment"));
        
        // 알림 정보
        entity.setNotificationSent(getValue("notification_sent") == "1");
        
        std::string notif_time = getValue("notification_time");
        if (!notif_time.empty() && notif_time != "NULL") {
            entity.setNotificationTime(stringToTimePoint(notif_time));
        }
        
        entity.setNotificationCount(std::stoi(getValue("notification_count", "0")));
        entity.setNotificationResult(getValue("notification_result"));
        
        // 컨텍스트 정보
        entity.setContextData(getValue("context_data", "{}"));
        entity.setSourceName(getValue("source_name"));
        entity.setLocation(getValue("location"));
        
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

bool AlarmOccurrenceRepository::validateEntity(const AlarmOccurrenceEntity& entity) const {
    if (entity.getRuleId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::validateEntity - Invalid rule_id: " + std::to_string(entity.getRuleId()));
        }
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::validateEntity - Invalid tenant_id: " + std::to_string(entity.getTenantId()));
        }
        return false;
    }
    
    if (entity.getAlarmMessage().empty()) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::validateEntity - Empty alarm_message");
        }
        return false;
    }
    
    return true;
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

std::string AlarmOccurrenceRepository::escapeString(const std::string& str) const {
    std::string escaped = str;
    
    // SQL 인젝션 방지를 위한 기본적인 이스케이프
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    
    return escaped;
}

// =============================================================================
// 통계 및 관리 메서드들
// =============================================================================

int AlarmOccurrenceRepository::getActiveCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(SQL::AlarmOccurrence::COUNT_ACTIVE);
        
        if (!rows.empty() && rows[0].count("count")) {
            return std::stoi(rows[0].at("count"));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::getActiveCount failed: " + std::string(e.what()));
        }
    }
    
    return 0;
}

int AlarmOccurrenceRepository::getCountBySeverity(AlarmOccurrenceEntity::Severity severity) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = SQL::AlarmOccurrence::COUNT_BY_SEVERITY;
        RepositoryHelpers::replaceParameterPlaceholders(query, {severityToString(severity)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        if (!rows.empty() && rows[0].count("count")) {
            return std::stoi(rows[0].at("count"));
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::getCountBySeverity failed: " + std::string(e.what()));
        }
    }
    
    return 0;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findRecentOccurrences(int limit) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        if (limit <= 0 || !ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::AlarmOccurrence::FIND_RECENT;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(limit)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);
        
        results.reserve(rows.size());
        for (const auto& row : rows) {
            try {
                results.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("AlarmOccurrenceRepository::findRecentOccurrences - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Debug("AlarmOccurrenceRepository::findRecentOccurrences - Found " + std::to_string(results.size()) + " recent occurrences");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceRepository::findRecentOccurrences failed: " + std::string(e.what()));
        }
    }
    
    return results;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne