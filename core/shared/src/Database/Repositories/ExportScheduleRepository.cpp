/**
 * @file ExportScheduleRepository.cpp
 * @brief Export Schedule Repository 구현 (DatabaseAbstractionLayer API 준수)
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.1.0
 */

#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 인터페이스 구현
// =============================================================================

std::vector<ExportScheduleEntity> ExportScheduleRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ExportScheduleRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportSchedule::FIND_ALL);
        
        std::vector<ExportScheduleEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("ExportScheduleRepository::findAll - Found " + 
                     std::to_string(entities.size()) + " schedules");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportScheduleEntity> ExportScheduleRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                return cached;
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportSchedule::FIND_BY_ID,
            std::to_string(id)
        );
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            auto entity = mapRowToEntity(results[0]);
            
            // 캐시에 저장
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            return entity;
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ExportScheduleRepository::save(ExportScheduleEntity& entity) {
    try {
        if (!ensureTableExists()) {
            logger_->Error("Table creation failed");
            return false;
        }
        
        if (!validateEntity(entity)) {
            logger_->Error("Invalid ExportScheduleEntity");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // next_run_at 추가
        params["next_run_at"] = formatTimestamp(std::chrono::system_clock::now());
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = SQL::ExportSchedule::INSERT;
        query = RepositoryHelpers::replaceParameter(query, params["profile_id"]);
        query = RepositoryHelpers::replaceParameter(query, params["target_id"]);
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["schedule_name"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["description"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["cron_expression"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["timezone"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["data_range"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, params["lookback_periods"]);
        query = RepositoryHelpers::replaceParameter(query, params["is_enabled"]);
        query = RepositoryHelpers::replaceParameter(query, "'" + params["next_run_at"] + "'");
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // ✅ 마지막 삽입된 ID 조회
            auto results = db_layer.executeQuery(
                "SELECT id FROM export_schedules WHERE schedule_name = '" + 
                RepositoryHelpers::escapeString(params["schedule_name"]) + 
                "' ORDER BY id DESC LIMIT 1"
            );
            
            if (!results.empty()) {
                int new_id = std::stoi(results[0].at("id"));
                entity.setId(new_id);
                entity.markSaved();
                
                if (isCacheEnabled()) {
                    cacheEntity(entity);
                }
            }
            
            logger_->Info("ExportSchedule saved: " + entity.getScheduleName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportScheduleRepository::update(const ExportScheduleEntity& entity) {
    try {
        if (entity.getId() <= 0) {
            logger_->Error("Invalid ID for update");
            return false;
        }
        
        if (!validateEntity(entity)) {
            logger_->Error("Invalid ExportScheduleEntity");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = SQL::ExportSchedule::UPDATE;
        query = RepositoryHelpers::replaceParameter(query, params["profile_id"]);
        query = RepositoryHelpers::replaceParameter(query, params["target_id"]);
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["schedule_name"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["description"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["cron_expression"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["timezone"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + RepositoryHelpers::escapeString(params["data_range"]) + "'");
        query = RepositoryHelpers::replaceParameter(query, params["lookback_periods"]);
        query = RepositoryHelpers::replaceParameter(query, params["is_enabled"]);
        query = RepositoryHelpers::replaceParameter(query, std::to_string(entity.getId()));
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            logger_->Info("ExportSchedule updated: " + entity.getScheduleName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportScheduleRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportSchedule::DELETE_BY_ID,
            std::to_string(id)
        );
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                // ✅ IRepository의 clearCacheForId 사용 (cache_ 직접 접근 금지)
                clearCache();
            }
            logger_->Info("ExportSchedule deleted: id=" + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportScheduleRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportSchedule::EXISTS_BY_ID,
            std::to_string(id)
        );
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<ExportScheduleEntity> ExportScheduleRepository::findByIds(const std::vector<int>& ids) {
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // ID 리스트를 문자열로 변환
        std::stringstream id_list;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) id_list << ",";
            id_list << ids[i];
        }
        
        std::string query = "SELECT * FROM export_schedules WHERE id IN (" + 
                           id_list.str() + ")";
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportScheduleEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportScheduleEntity> ExportScheduleRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // WHERE 절 구성
        std::stringstream where_clause;
        bool first = true;
        for (const auto& condition : conditions) {
            if (!first) where_clause << " AND ";
            // ✅ QueryCondition의 정확한 필드명 사용: field, operation, value
            where_clause << condition.field << " " << condition.operation << " ";
            
            // 값 타입에 따라 처리
            if (condition.value.find_first_not_of("0123456789.-") == std::string::npos) {
                where_clause << condition.value;
            } else {
                where_clause << "'" << RepositoryHelpers::escapeString(condition.value) << "'";
            }
            first = false;
        }
        
        // 전체 쿼리 구성
        std::stringstream query;
        query << "SELECT * FROM export_schedules";
        if (!conditions.empty()) {
            query << " WHERE " << where_clause.str();
        }
        
        if (order_by.has_value()) {
            // ✅ OrderBy의 정확한 필드명 사용: field, ascending
            query << " ORDER BY " << order_by->field << " " 
                  << (order_by->ascending ? "ASC" : "DESC");
        }
        
        if (pagination.has_value()) {
            query << " LIMIT " << pagination->limit 
                  << " OFFSET " << pagination->offset;
        }
        
        auto results = db_layer.executeQuery(query.str());
        
        std::vector<ExportScheduleEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findByConditions failed: " + 
                      std::string(e.what()));
        return {};
    }
}

int ExportScheduleRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // WHERE 절 구성
        std::stringstream where_clause;
        bool first = true;
        for (const auto& condition : conditions) {
            if (!first) where_clause << " AND ";
            // ✅ QueryCondition의 정확한 필드명 사용
            where_clause << condition.field << " " << condition.operation << " ";
            
            if (condition.value.find_first_not_of("0123456789.-") == std::string::npos) {
                where_clause << condition.value;
            } else {
                where_clause << "'" << RepositoryHelpers::escapeString(condition.value) << "'";
            }
            first = false;
        }
        
        std::stringstream query;
        query << "SELECT COUNT(*) as count FROM export_schedules";
        if (!conditions.empty()) {
            query << " WHERE " << where_clause.str();
        }
        
        auto results = db_layer.executeQuery(query.str());
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::countByConditions failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// ExportSchedule 특화 조회 메서드
// =============================================================================

std::vector<ExportScheduleEntity> ExportScheduleRepository::findEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        // ✅ ExportSQLQueries 사용
        auto results = db_layer.executeQuery(SQL::ExportSchedule::FIND_ENABLED);
        
        std::vector<ExportScheduleEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findEnabled failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportScheduleEntity> ExportScheduleRepository::findByTargetId(int target_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportSchedule::FIND_BY_TARGET_ID,
            std::to_string(target_id)
        );
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportScheduleEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findByTargetId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportScheduleEntity> ExportScheduleRepository::findPending() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        // ✅ ExportSQLQueries 사용
        auto results = db_layer.executeQuery(SQL::ExportSchedule::FIND_PENDING);
        
        std::vector<ExportScheduleEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::findPending failed: " + std::string(e.what()));
        return {};
    }
}

int ExportScheduleRepository::countEnabled() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        // ✅ COUNT_ENABLED 쿼리가 없으므로 직접 작성
        std::string query = "SELECT COUNT(*) as count FROM export_schedules WHERE is_enabled = 1";
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::countEnabled failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 스케줄 실행 상태 업데이트
// =============================================================================

bool ExportScheduleRepository::updateRunStatus(
    int schedule_id, 
    bool success, 
    const std::chrono::system_clock::time_point& next_run) {
    
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        auto now = std::chrono::system_clock::now();
        std::string last_run_at = formatTimestamp(now);
        std::string last_status = success ? "success" : "failed";
        std::string next_run_at = formatTimestamp(next_run);
        
        // ✅ ExportSQLQueries 사용 + RepositoryHelpers로 파라미터 치환
        std::string query = SQL::ExportSchedule::UPDATE_RUN_STATUS;
        query = RepositoryHelpers::replaceParameter(query, "'" + last_run_at + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + last_status + "'");
        query = RepositoryHelpers::replaceParameter(query, "'" + next_run_at + "'");
        query = RepositoryHelpers::replaceParameter(query, success ? "1" : "0");
        query = RepositoryHelpers::replaceParameter(query, success ? "0" : "1");
        query = RepositoryHelpers::replaceParameter(query, std::to_string(schedule_id));
        
        bool result = db_layer.executeNonQuery(query);
        
        if (result && isCacheEnabled()) {
            // ✅ IRepository의 clearCache 사용
            clearCache();
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::updateRunStatus failed: " + 
                      std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Private Helper 메서드
// =============================================================================

bool ExportScheduleRepository::ensureTableExists() {
    static bool table_checked = false;
    
    if (table_checked) {
        return true;
    }
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        if (!db_layer.executeCreateTable(SQL::ExportSchedule::CREATE_TABLE)) {
            logger_->Error("Failed to create export_schedules table");
            return false;
        }
        
        // 인덱스 생성 (executeNonQuery 사용)
        std::vector<std::string> indexes = {
            "CREATE INDEX IF NOT EXISTS idx_export_schedules_target_id ON export_schedules(target_id)",
            "CREATE INDEX IF NOT EXISTS idx_export_schedules_is_enabled ON export_schedules(is_enabled)",
            "CREATE INDEX IF NOT EXISTS idx_export_schedules_next_run_at ON export_schedules(next_run_at)"
        };
        
        for (const auto& index : indexes) {
            db_layer.executeNonQuery(index);
        }
        
        table_checked = true;
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::ensureTableExists failed: " + 
                      std::string(e.what()));
        return false;
    }
}

ExportScheduleEntity ExportScheduleRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportScheduleEntity entity;
    
    try {
        // 필수 필드
        entity.setId(std::stoi(row.at("id")));
        entity.setProfileId(std::stoi(row.at("profile_id")));
        entity.setTargetId(std::stoi(row.at("target_id")));
        entity.setScheduleName(row.at("schedule_name"));
        entity.setCronExpression(row.at("cron_expression"));
        entity.setTimezone(row.at("timezone"));
        entity.setDataRange(row.at("data_range"));
        entity.setLookbackPeriods(std::stoi(row.at("lookback_periods")));
        // ✅ setEnabled 사용 (setIsEnabled 아님)
        entity.setEnabled(row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
        
        // 옵션 필드
        auto it = row.find("description");
        if (it != row.end() && !it->second.empty()) {
            entity.setDescription(it->second);
        }
        
        // ✅ created_at, updated_at은 Entity 생성 시 자동 설정되므로 별도 setter 불필요
        // (ExportScheduleEntity에 setCreatedAt/setUpdatedAt 없음)
        
        // 실행 관련 필드
        it = row.find("next_run_at");
        if (it != row.end() && !it->second.empty()) {
            entity.setNextRunAt(parseTimestamp(it->second));
        }
        
        it = row.find("last_run_at");
        if (it != row.end() && !it->second.empty()) {
            entity.setLastRunAt(parseTimestamp(it->second));
        }
        
        it = row.find("last_status");
        if (it != row.end() && !it->second.empty()) {
            entity.setLastStatus(it->second);
        }
        
        it = row.find("consecutive_failures");
        if (it != row.end() && !it->second.empty()) {
            // consecutive_failures는 ExportScheduleEntity에 필드가 없음
            // total_runs, successful_runs, failed_runs으로 대체됨
        }
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::mapRowToEntity failed: " + 
                      std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> ExportScheduleRepository::entityToParams(
    const ExportScheduleEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    params["profile_id"] = std::to_string(entity.getProfileId());
    params["target_id"] = std::to_string(entity.getTargetId());
    params["schedule_name"] = entity.getScheduleName();
    params["description"] = entity.getDescription();  // ✅ std::string 반환
    params["cron_expression"] = entity.getCronExpression();
    params["timezone"] = entity.getTimezone();
    params["data_range"] = entity.getDataRange();
    params["lookback_periods"] = std::to_string(entity.getLookbackPeriods());
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";  // ✅ isEnabled() 사용
    
    return params;
}

// ✅ const 추가 (헤더 파일의 선언과 일치)
bool ExportScheduleRepository::validateEntity(const ExportScheduleEntity& entity) const {
    if (entity.getScheduleName().empty()) {
        logger_->Error("Schedule name cannot be empty");
        return false;
    }
    
    if (entity.getCronExpression().empty()) {
        logger_->Error("Cron expression cannot be empty");
        return false;
    }
    
    if (entity.getTimezone().empty()) {
        logger_->Error("Timezone cannot be empty");
        return false;
    }
    
    if (entity.getDataRange().empty()) {
        logger_->Error("Data range cannot be empty");
        return false;
    }
    
    if (entity.getLookbackPeriods() < 0) {
        logger_->Error("Lookback periods cannot be negative");
        return false;
    }
    
    return true;
}

std::string ExportScheduleRepository::formatTimestamp(
    const std::chrono::system_clock::time_point& tp) {
    
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point ExportScheduleRepository::parseTimestamp(
    const std::string& timestamp_str) {
    
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    auto time_t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t);
}

std::map<std::string, int> ExportScheduleRepository::getCacheStats() const {
    return IRepository<ExportScheduleEntity>::getCacheStats();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne