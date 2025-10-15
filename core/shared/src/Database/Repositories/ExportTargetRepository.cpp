/**
 * @file ExportTargetRepository.cpp
 * @brief Export Target Repository 구현 - 에러 수정 완료
 */

#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

std::vector<ExportTargetEntity> ExportTargetRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_ALL);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findById(int id) {
    try {
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
    } catch (...) {
        return std::nullopt;
    }
}

bool ExportTargetRepository::save(ExportTargetEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportTarget::INSERT;
        
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                query.replace(pos, 1, "'" + param.second + "'");
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!results.empty() && results[0].find("id") != results[0].end()) {
                entity.setId(std::stoi(results[0].at("id")));
            }
        }
        
        return success;
    } catch (...) {
        return false;
    }
}

bool ExportTargetRepository::update(const ExportTargetEntity& entity) {
    try {
        if (!ensureTableExists() || entity.getId() <= 0) {
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportTarget::UPDATE;
        
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                query.replace(pos, 1, "'" + param.second + "'");
            }
        }
        
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(entity.getId()));
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(entity.getId());
        }
        
        return success;
    } catch (...) {
        return false;
    }
}

bool ExportTargetRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::DELETE_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(id);
        }
        
        return success;
    } catch (...) {
        return false;
    }
}

bool ExportTargetRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::EXISTS_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
    } catch (...) {
        return false;
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByIds(const std::vector<int>& ids) {
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        std::string query = "SELECT * FROM export_targets WHERE id IN (" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = "SELECT * FROM export_targets";
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

int ExportTargetRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM export_targets";
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
    } catch (...) {
        return 0;
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByTargetType(const std::string& target_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            SQL::ExportTarget::FIND_BY_TARGET_TYPE, target_type);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_ENABLED);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByProfileId(int profile_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_PROFILE_ID, std::to_string(profile_id));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (...) {}
        }
        
        return entities;
    } catch (...) {
        return {};
    }
}

bool ExportTargetRepository::updateStatistics(int target_id, bool success, int processing_time_ms) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = SQL::ExportTarget::UPDATE_STATISTICS;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(processing_time_ms));
        query = RepositoryHelpers::replaceParameter(query, std::to_string(target_id));
        
        DatabaseAbstractionLayer db_layer;
        bool result = db_layer.executeNonQuery(query);
        
        if (result && isCacheEnabled()) {
            clearCacheForId(target_id);
        }
        
        return result;
    } catch (...) {
        return false;
    }
}

std::map<std::string, int> ExportTargetRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        stats["cache_enabled"] = 1;
        auto base_stats = IRepository<ExportTargetEntity>::getCacheStats();
        stats.insert(base_stats.begin(), base_stats.end());
    } else {
        stats["cache_enabled"] = 0;
    }
    
    return stats;
}

ExportTargetEntity ExportTargetRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportTargetEntity entity;
    
    try {
        auto it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("profile_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setProfileId(std::stoi(it->second));
        }
        
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        it = row.find("target_type");
        if (it != row.end()) {
            entity.setTargetType(it->second);
        }
        
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        it = row.find("is_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setEnabled(std::stoi(it->second) != 0);
        }
        
        it = row.find("config");
        if (it != row.end()) {
            entity.setConfig(it->second);
        }
        
        it = row.find("export_mode");
        if (it != row.end()) {
            entity.setExportMode(it->second);
        }
        
        it = row.find("export_interval");
        if (it != row.end() && !it->second.empty()) {
            entity.setExportInterval(std::stoi(it->second));
        }
        
        it = row.find("batch_size");
        if (it != row.end() && !it->second.empty()) {
            entity.setBatchSize(std::stoi(it->second));
        }
        
        it = row.find("total_exports");
        if (it != row.end() && !it->second.empty()) {
            entity.setTotalExports(std::stoull(it->second));
        }
        
        it = row.find("successful_exports");
        if (it != row.end() && !it->second.empty()) {
            entity.setSuccessfulExports(std::stoull(it->second));
        }
        
        it = row.find("failed_exports");
        if (it != row.end() && !it->second.empty()) {
            entity.setFailedExports(std::stoull(it->second));
        }
        
        it = row.find("avg_export_time_ms");
        if (it != row.end() && !it->second.empty()) {
            entity.setAvgExportTimeMs(std::stoi(it->second));
        }
        
    } catch (...) {
        throw std::runtime_error("Failed to map row to ExportTargetEntity");
    }
    
    return entity;
}

std::map<std::string, std::string> ExportTargetRepository::entityToParams(
    const ExportTargetEntity& entity) {
    
    std::map<std::string, std::string> params;
    
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

bool ExportTargetRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::ExportTarget::CREATE_TABLE);
    } catch (...) {
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne