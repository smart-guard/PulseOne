// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// 🔧 AlarmOccurrenceRepository 수정 - RepositoryHelpers 방식으로 매개변수 바인딩
// =============================================================================

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"  // 🔥 추가
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 (AlarmRuleRepository 패턴)
using AlarmOccurrenceEntity = Entities::AlarmOccurrenceEntity;

// =============================================================================
// IRepository 인터페이스 구현 (RepositoryHelpers 방식)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🔥 RepositoryHelpers 방식: 매개변수 없으므로 그대로 사용
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::FIND_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);  // 매개변수 1개만!
        
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
        
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::INSERT;
        
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
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);  // 매개변수 1개만!
        
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
        
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::UPDATE;
        
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
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        bool success = db_layer.executeNonQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::DELETE_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::EXISTS_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);  // 매개변수 1개만!
        
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
// AlarmOccurrence 전용 메서드들 (RepositoryHelpers 방식)
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByRuleId(int rule_id) {
    std::vector<AlarmOccurrenceEntity> results;
    
    try {
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::FIND_BY_RULE_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(rule_id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::FIND_ACTIVE_BY_TENANT_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(tenant_id)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::FIND_BY_SEVERITY;
        RepositoryHelpers::replaceParameterPlaceholders(query, {severityToString(severity)});
        
        DatabaseAbstractionLayer db_layer;
        auto rows = db_layer.executeQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::ACKNOWLEDGE;
        
        std::vector<std::string> params = {
            "acknowledged",  // state
            timePointToString(std::chrono::system_clock::now()),  // acknowledged_time
            std::to_string(user_id),  // acknowledged_by
            comment,  // acknowledge_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);  // 매개변수 1개만!
        
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
        // 🔥 RepositoryHelpers 방식: 미리 매개변수 바인딩
        std::string query = SQL::AlarmOccurrence::CLEAR;
        
        std::vector<std::string> params = {
            "cleared",  // state
            timePointToString(std::chrono::system_clock::now()),  // cleared_time
            cleared_value,  // cleared_value
            comment,  // clear_comment
            std::to_string(occurrence_id)  // WHERE id
        };
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);  // 매개변수 1개만!
        
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

// =============================================================================
// 헬퍼 메서드들 (나머지는 기존과 동일)
// =============================================================================

// mapRowToEntity, timePointToString, severityToString 등의 헬퍼 메서드들은
// 기존 AlarmOccurrenceRepository 구현과 동일하게 유지

} // namespace Repositories
} // namespace Database
} // namespace PulseOne