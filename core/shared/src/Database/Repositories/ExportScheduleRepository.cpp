/**
 * @file ExportScheduleRepository.cpp
 * @brief Export Schedule Repository 구현 (올바른 패턴 적용)
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
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
        auto results = db_layer.executeQuery(
            SQL::ExportSchedule::FIND_BY_ID,
            {std::to_string(id)}
        );
        
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
        
        bool success = db_layer.executeInsert(
            SQL::ExportSchedule::INSERT,
            {
                params["profile_id"],
                params["target_id"],
                params["schedule_name"],
                params["description"],
                params["cron_expression"],
                params["timezone"],
                params["data_range"],
                params["lookback_periods"],
                params["is_enabled"],
                params["next_run_at"]
            }
        );
        
        if (success) {
            int new_id = db_layer.getLastInsertId();
            entity.setId(new_id);
            entity.markSaved();
            
            if (isCacheEnabled()) {
                cacheEntity(entity);
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
        
        bool success = db_layer.executeUpdate(
            SQL::ExportSchedule::UPDATE,
            {
                params["profile_id"],
                params["target_id"],
                params["schedule_name"],
                params["description"],
                params["cron_expression"],
                params["timezone"],
                params["data_range"],
                params["lookback_periods"],
                params["is_enabled"],
                std::to_string(entity.getId())
            }
        );
        
        if (success) {
            if (isCacheEnabled()) {
                updateCache(entity);
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
        bool success = db_layer.executeDelete(
            SQL::ExportSchedule::DELETE_BY_ID,
            {std::to_string(id)}
        );
        
        if (success) {
            if (isCacheEnabled()) {
                removeFromCache(id);
            }
            logger_->Info("ExportSchedule deleted: ID=" + std::to_string(id));
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
        auto results = db_layer.executeQuery(
            SQL::ExportSchedule::EXISTS_BY_ID,
            {std::to_string(id)}
        );
        
        if (!results.empty() && !results[0].empty()) {
            auto it = results[0].find("count");
            if (it != results[0].end()) {
                return std::stoi(it->second) > 0;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportScheduleEntity> ExportScheduleRepository::findByIds(const std::vector<int>& ids) {
    return RepositoryHelpers::findByIdsBatch<ExportScheduleEntity>(
        *this, ids, 100
    );
}

std::vector<ExportScheduleEntity> ExportScheduleRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    return RepositoryHelpers::findByConditionsGeneric<ExportScheduleEntity>(
        *db_manager_, "export_schedules", conditions, order_by, pagination,
        [this](const std::map<std::string, std::string>& row) {
            return this->mapRowToEntity(row);
        }
    );
}

int ExportScheduleRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    return RepositoryHelpers::countByConditionsGeneric(
        *db_manager_, "export_schedules", conditions
    );
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
        auto results = db_layer.executeQuery(
            SQL::ExportSchedule::FIND_BY_TARGET_ID,
            {std::to_string(target_id)}
        );
        
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
        auto results = db_layer.executeQuery(SQL::ExportSchedule::COUNT_ENABLED);
        
        if (!results.empty() && !results[0].empty()) {
            auto it = results[0].find("count");
            if (it != results[0].end()) {
                return std::stoi(it->second);
            }
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
        
        bool result = db_layer.executeUpdate(
            SQL::ExportSchedule::UPDATE_RUN_STATUS,
            {
                last_run_at,
                last_status,
                next_run_at,
                success ? "1" : "0",
                success ? "0" : "1",
                std::to_string(schedule_id)
            }
        );
        
        if (result && isCacheEnabled()) {
            removeFromCache(schedule_id);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::updateRunStatus failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 캐시 통계
// =============================================================================

std::map<std::string, int> ExportScheduleRepository::getCacheStats() const {
    return IRepository<ExportScheduleEntity>::getCacheStats();
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

bool ExportScheduleRepository::ensureTableExists() {
    if (table_created_.load()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    if (table_created_.load()) {
        return true;
    }
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        bool success = db_layer.executeCreateTable(SQL::ExportSchedule::CREATE_TABLE);
        
        if (success) {
            db_layer.executeRaw(SQL::ExportSchedule::CREATE_INDEXES);
            table_created_ = true;
            logger_->Info("export_schedules table created successfully");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportScheduleRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

ExportScheduleEntity ExportScheduleRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportScheduleEntity entity;
    
    try {
        auto get_int = [&](const std::string& key) -> int {
            auto it = row.find(key);
            return (it != row.end() && !it->second.empty()) ? std::stoi(it->second) : 0;
        };
        
        auto get_string = [&](const std::string& key) -> std::string {
            auto it = row.find(key);
            return (it != row.end()) ? it->second : "";
        };
        
        auto get_bool = [&](const std::string& key) -> bool {
            auto it = row.find(key);
            return (it != row.end() && !it->second.empty()) ? (it->second == "1" || it->second == "true") : false;
        };
        
        entity.setId(get_int("id"));
        entity.setProfileId(get_int("profile_id"));
        entity.setTargetId(get_int("target_id"));
        entity.setScheduleName(get_string("schedule_name"));
        entity.setDescription(get_string("description"));
        entity.setCronExpression(get_string("cron_expression"));
        entity.setTimezone(get_string("timezone"));
        entity.setDataRange(get_string("data_range"));
        entity.setLookbackPeriods(get_int("lookback_periods"));
        entity.setEnabled(get_bool("is_enabled"));
        
        std::string last_run_at_str = get_string("last_run_at");
        if (!last_run_at_str.empty()) {
            entity.setLastRunAt(parseTimestamp(last_run_at_str));
        }
        
        entity.setLastStatus(get_string("last_status"));
        
        std::string next_run_at_str = get_string("next_run_at");
        if (!next_run_at_str.empty()) {
            entity.setNextRunAt(parseTimestamp(next_run_at_str));
        }
        
        entity.setTotalRuns(get_int("total_runs"));
        entity.setSuccessfulRuns(get_int("successful_runs"));
        entity.setFailedRuns(get_int("failed_runs"));
        
        std::string created_at_str = get_string("created_at");
        if (!created_at_str.empty()) {
            entity.setCreatedAt(parseTimestamp(created_at_str));
        }
        
        std::string updated_at_str = get_string("updated_at");
        if (!updated_at_str.empty()) {
            entity.setUpdatedAt(parseTimestamp(updated_at_str));
        }
        
        entity.markSaved();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to map row to ExportScheduleEntity: " + 
                                std::string(e.what()));
    }
    
    return entity;
}

std::map<std::string, std::string> ExportScheduleRepository::entityToParams(
    const ExportScheduleEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    params["profile_id"] = entity.getProfileId() > 0 ? 
        std::to_string(entity.getProfileId()) : "";
    params["target_id"] = std::to_string(entity.getTargetId());
    params["schedule_name"] = entity.getScheduleName();
    params["description"] = entity.getDescription();
    params["cron_expression"] = entity.getCronExpression();
    params["timezone"] = entity.getTimezone();
    params["data_range"] = entity.getDataRange();
    params["lookback_periods"] = std::to_string(entity.getLookbackPeriods());
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    
    return params;
}

bool ExportScheduleRepository::validateEntity(const ExportScheduleEntity& entity) const {
    return entity.validate();
}

std::chrono::system_clock::time_point ExportScheduleRepository::parseTimestamp(
    const std::string& time_str) {
    
    if (time_str.empty()) {
        return std::chrono::system_clock::time_point{};
    }
    
    try {
        std::tm tm = {};
        std::istringstream ss(time_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        
        if (ss.fail()) {
            return std::chrono::system_clock::time_point{};
        }
        
        auto time_t = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(time_t);
        
    } catch (const std::exception&) {
        return std::chrono::system_clock::time_point{};
    }
}

std::string ExportScheduleRepository::formatTimestamp(
    const std::chrono::system_clock::time_point& tp) {
    
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = {};
    
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne