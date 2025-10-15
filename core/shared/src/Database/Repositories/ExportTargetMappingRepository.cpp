/**
 * @file ExportTargetMappingRepository.cpp
 * @brief Export Target Mapping Repository 구현
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Repositories/ExportTargetMappingRepository.cpp
 */

#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Database/SQLQueries.h"
#include "Utils/LogManager.h"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ExportTargetMappingRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_ALL);
        
        std::vector<ExportTargetMappingEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetMappingRepository::findAll - Failed to map row");
            }
        }
        
        logger_->Info("ExportTargetMappingRepository::findAll - Found " + 
                     std::to_string(entities.size()) + " mappings");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportTargetMappingEntity> ExportTargetMappingRepository::findById(int id) {
    try {
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("ExportTargetMappingRepository::findById - Cache hit");
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_BY_ID, 
                                            {std::to_string(id)});
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ExportTargetMappingRepository::save(ExportTargetMappingEntity& entity) {
    try {
        if (!validateMapping(entity)) {
            logger_->Error("ExportTargetMappingRepository::save - Invalid mapping");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            std::to_string(entity.getTargetId()),
            std::to_string(entity.getPointId()),
            entity.getTargetFieldName(),
            entity.getTargetDescription(),
            entity.getConversionConfig(),
            entity.isEnabled() ? "1" : "0"
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportTargetMapping::INSERT, params);
        
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
            
            logger_->Info("ExportTargetMappingRepository::save - Saved mapping ID: " + 
                         std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetMappingRepository::update(const ExportTargetMappingEntity& entity) {
    try {
        if (!validateMapping(entity)) {
            logger_->Error("ExportTargetMappingRepository::update - Invalid mapping");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            std::to_string(entity.getTargetId()),
            std::to_string(entity.getPointId()),
            entity.getTargetFieldName(),
            entity.getTargetDescription(),
            entity.getConversionConfig(),
            entity.isEnabled() ? "1" : "0",
            std::to_string(entity.getId())
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportTargetMapping::UPDATE, params);
        
        if (success && isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetMappingRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportTargetMapping::DELETE_BY_ID, 
                                               {std::to_string(id)});
        
        if (success && isCacheEnabled()) {
            invalidateCache(id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportTargetMappingRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::EXISTS_BY_ID, 
                                            {std::to_string(id)});
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    std::vector<ExportTargetMappingEntity> entities;
    entities.reserve(ids.size());
    
    for (int id : ids) {
        auto entity = findById(id);
        if (entity.has_value()) {
            entities.push_back(entity.value());
        }
    }
    
    return entities;
}

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    return IRepository<ExportTargetMappingEntity>::findByConditions(conditions, order_by, pagination);
}

int ExportTargetMappingRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    return IRepository<ExportTargetMappingEntity>::countByConditions(conditions);
}

// =============================================================================
// Mapping 전용 조회 메서드
// =============================================================================

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findByTargetId(int target_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_BY_TARGET_ID, 
                                            {std::to_string(target_id)});
        
        std::vector<ExportTargetMappingEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetMappingRepository::findByTargetId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findByTargetId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findByPointId(int point_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_BY_POINT_ID, 
                                            {std::to_string(point_id)});
        
        std::vector<ExportTargetMappingEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetMappingRepository::findByPointId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findByPointId failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportTargetMappingEntity> ExportTargetMappingRepository::findByTargetAndPoint(
    int target_id, int point_id) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_BY_TARGET_AND_POINT,
                                            {std::to_string(target_id), std::to_string(point_id)});
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findByTargetAndPoint failed: " + 
                      std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<ExportTargetMappingEntity> ExportTargetMappingRepository::findEnabledByTargetId(int target_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_ENABLED_BY_TARGET_ID, 
                                            {std::to_string(target_id)});
        
        std::vector<ExportTargetMappingEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportTargetMappingRepository::findEnabledByTargetId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::findEnabledByTargetId failed: " + 
                      std::string(e.what()));
        return {};
    }
}

int ExportTargetMappingRepository::deleteByTargetId(int target_id) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 먼저 개수 확인
        int count = countByTargetId(target_id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportTargetMapping::DELETE_BY_TARGET_ID, 
                                               {std::to_string(target_id)});
        
        if (success) {
            logger_->Info("ExportTargetMappingRepository::deleteByTargetId - Deleted " + 
                         std::to_string(count) + " mappings");
            return count;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::deleteByTargetId failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

int ExportTargetMappingRepository::deleteByPointId(int point_id) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        int count = countByPointId(point_id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportTargetMapping::DELETE_BY_POINT_ID, 
                                               {std::to_string(point_id)});
        
        if (success) {
            logger_->Info("ExportTargetMappingRepository::deleteByPointId - Deleted " + 
                         std::to_string(count) + " mappings");
            return count;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::deleteByPointId failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 통계
// =============================================================================

int ExportTargetMappingRepository::countByTargetId(int target_id) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::COUNT_BY_TARGET_ID, 
                                            {std::to_string(target_id)});
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::countByTargetId failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

int ExportTargetMappingRepository::countByPointId(int point_id) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTargetMapping::COUNT_BY_POINT_ID, 
                                            {std::to_string(point_id)});
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::countByPointId failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 캐시 관리
// =============================================================================

std::map<std::string, int> ExportTargetMappingRepository::getCacheStats() const {
    return IRepository<ExportTargetMappingEntity>::getCacheStats();
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

ExportTargetMappingEntity ExportTargetMappingRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportTargetMappingEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("target_id");
        if (it != row.end()) {
            entity.setTargetId(std::stoi(it->second));
        }
        
        it = row.find("point_id");
        if (it != row.end()) {
            entity.setPointId(std::stoi(it->second));
        }
        
        if ((it = row.find("target_field_name")) != row.end()) {
            entity.setTargetFieldName(it->second);
        }
        
        if ((it = row.find("target_description")) != row.end()) {
            entity.setTargetDescription(it->second);
        }
        
        if ((it = row.find("conversion_config")) != row.end()) {
            entity.setConversionConfig(it->second);
        }
        
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::mapRowToEntity failed: " + 
                      std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> ExportTargetMappingRepository::entityToParams(
    const ExportTargetMappingEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    params["id"] = std::to_string(entity.getId());
    params["target_id"] = std::to_string(entity.getTargetId());
    params["point_id"] = std::to_string(entity.getPointId());
    params["target_field_name"] = entity.getTargetFieldName();
    params["target_description"] = entity.getTargetDescription();
    params["conversion_config"] = entity.getConversionConfig();
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    
    return params;
}

bool ExportTargetMappingRepository::validateMapping(const ExportTargetMappingEntity& entity) {
    return entity.validate();
}

bool ExportTargetMappingRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::ExportTargetMapping::CREATE_TABLE);
    } catch (const std::exception& e) {
        logger_->Error("ExportTargetMappingRepository::ensureTableExists failed: " + 
                      std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne