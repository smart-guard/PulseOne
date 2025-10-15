/**
 * @file ExportTargetRepository.cpp
 * @brief Export Target Repository 구현 - SiteRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Repositories/ExportTargetRepository.cpp
 */

#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Database/SQLQueries.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ExportTargetRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_ALL);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("ExportTargetRepository::findAll - Found " + std::to_string(entities.size()) + " targets");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("ExportTargetRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_BY_ID, {std::to_string(id)});
        
        if (results.empty()) {
            logger_->Debug("ExportTargetRepository::findById - Target not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("ExportTargetRepository::findById - Found target: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ExportTargetRepository::save(ExportTargetEntity& entity) {
    try {
        if (!validateTarget(entity)) {
            logger_->Error("ExportTargetRepository::save - Invalid target: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            std::to_string(entity.getProfileId()),
            entity.getName(),
            entity.getTargetType(),
            entity.getDescription(),
            entity.isEnabled() ? "1" : "0",
            entity.getConfig(),
            entity.getExportMode(),
            std::to_string(entity.getExportInterval()),
            std::to_string(entity.getBatchSize())
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportTarget::INSERT, params);
        
        if (success && entity.getId() <= 0) {
            auto id_result = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_result.empty() && !id_result[0].empty()) {
                entity.setId(std::stoi(id_result[0].at("id")));
            }
        }
        
        if (success) {
            entity.markSaved();
            
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("ExportTargetRepository::save - Saved target: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetRepository::update(const ExportTargetEntity& entity) {
    try {
        if (!validateTarget(entity)) {
            logger_->Error("ExportTargetRepository::update - Invalid target");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            std::to_string(entity.getProfileId()),
            entity.getName(),
            entity.getTargetType(),
            entity.getDescription(),
            entity.isEnabled() ? "1" : "0",
            entity.getConfig(),
            entity.getExportMode(),
            std::to_string(entity.getExportInterval()),
            std::to_string(entity.getBatchSize()),
            std::to_string(entity.getId())
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportTarget::UPDATE, params);
        
        if (success && isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        if (success) {
            logger_->Info("ExportTargetRepository::update - Updated target ID: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportTarget::DELETE_BY_ID, {std::to_string(id)});
        
        if (success && isCacheEnabled()) {
            invalidateCache(id);
        }
        
        if (success) {
            logger_->Info("ExportTargetRepository::deleteById - Deleted target ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::EXISTS_BY_ID, {std::to_string(id)});
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    std::vector<ExportTargetEntity> entities;
    entities.reserve(ids.size());
    
    for (int id : ids) {
        auto entity = findById(id);
        if (entity.has_value()) {
            entities.push_back(entity.value());
        }
    }
    
    return entities;
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // IRepository의 기본 구현 활용
    return IRepository<ExportTargetEntity>::findByConditions(conditions, order_by, pagination);
}

int ExportTargetRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // IRepository의 기본 구현 활용
    return IRepository<ExportTargetEntity>::countByConditions(conditions);
}

// =============================================================================
// Export Target 전용 조회 메서드
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByEnabled(bool enabled) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_BY_ENABLED, 
                                            {enabled ? "1" : "0"});
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetRepository::findByEnabled - Failed to map row");
            }
        }
        
        logger_->Debug("ExportTargetRepository::findByEnabled - Found " + 
                      std::to_string(entities.size()) + " targets");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findByEnabled failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByTargetType(const std::string& target_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_BY_TARGET_TYPE, {target_type});
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetRepository::findByTargetType - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findByTargetType failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByProfileId(int profile_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_BY_PROFILE_ID, 
                                            {std::to_string(profile_id)});
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetRepository::findByProfileId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findByProfileId failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findByName(const std::string& name) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_BY_NAME, {name});
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::findByName failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findHealthyTargets() {
    auto all_targets = findByEnabled(true);
    
    std::vector<ExportTargetEntity> healthy_targets;
    for (const auto& target : all_targets) {
        if (target.isHealthy()) {
            healthy_targets.push_back(target);
        }
    }
    
    return healthy_targets;
}

std::vector<ExportTargetEntity> ExportTargetRepository::findRecentErrorTargets(int hours) {
    auto all_targets = findAll();
    auto now = std::chrono::system_clock::now();
    auto threshold = now - std::chrono::hours(hours);
    
    std::vector<ExportTargetEntity> error_targets;
    for (const auto& target : all_targets) {
        auto last_error_at = target.getLastErrorAt();
        if (last_error_at.has_value() && last_error_at.value() >= threshold) {
            error_targets.push_back(target);
        }
    }
    
    return error_targets;
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

int ExportTargetRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_ALL);
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

int ExportTargetRepository::getActiveCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_ACTIVE);
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::getActiveCount failed: " + std::string(e.what()));
        return 0;
    }
}

std::map<std::string, int> ExportTargetRepository::getCountByType() {
    std::map<std::string, int> counts;
    
    try {
        if (!ensureTableExists()) {
            return counts;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_BY_TYPE);
        
        for (const auto& row : results) {
            if (!row.empty()) {
                std::string type = row.at("target_type");
                int count = std::stoi(row.at("count"));
                counts[type] = count;
            }
        }
        
        return counts;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::getCountByType failed: " + std::string(e.what()));
        return counts;
    }
}

bool ExportTargetRepository::updateStatistics(int target_id, bool success, 
                                             int processing_time_ms,
                                             const std::string& error_message) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            success ? "1" : "0",  // successful_exports increment
            success ? "0" : "1",  // failed_exports increment
            success ? "1" : "0",  // update last_success_at
            error_message,
            success ? "1" : "0",  // check for error update
            std::to_string(processing_time_ms),
            std::to_string(target_id)
        };
        
        bool result = db_layer.executeNonQuery(SQL::ExportTarget::UPDATE_STATISTICS, params);
        
        if (result && isCacheEnabled()) {
            invalidateCache(target_id);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::updateStatistics failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 캐시 관리
// =============================================================================

std::map<std::string, int> ExportTargetRepository::getCacheStats() const {
    return IRepository<ExportTargetEntity>::getCacheStats();
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

ExportTargetEntity ExportTargetRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    ExportTargetEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("profile_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setProfileId(std::stoi(it->second));
        }
        
        if ((it = row.find("name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("target_type")) != row.end()) entity.setTargetType(it->second);
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        if ((it = row.find("config")) != row.end()) entity.setConfig(it->second);
        if ((it = row.find("export_mode")) != row.end()) entity.setExportMode(it->second);
        
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("export_interval");
        if (it != row.end() && !it->second.empty()) {
            entity.setExportInterval(std::stoi(it->second));
        }
        
        it = row.find("batch_size");
        if (it != row.end() && !it->second.empty()) {
            entity.setBatchSize(std::stoi(it->second));
        }
        
        // 통계 필드들
        it = row.find("total_exports");
        if (it != row.end() && !it->second.empty()) {
            // 직접 설정 (내부 접근 필요)
        }
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> ExportTargetRepository::entityToParams(const ExportTargetEntity& entity) {
    std::map<std::string, std::string> params;
    
    params["id"] = std::to_string(entity.getId());
    params["profile_id"] = std::to_string(entity.getProfileId());
    params["name"] = entity.getName();
    params["target_type"] = entity.getTargetType();
    params["description"] = entity.getDescription();
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["config"] = entity.getConfig();
    params["export_mode"] = entity.getExportMode();
    params["export_interval"] = std::to_string(entity.getExportInterval());
    params["batch_size"] = std::to_string(entity.getBatchSize());
    
    return params;
}

bool ExportTargetRepository::validateTarget(const ExportTargetEntity& entity) {
    return entity.validate();
}

bool ExportTargetRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::ExportTarget::CREATE_TABLE);
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne