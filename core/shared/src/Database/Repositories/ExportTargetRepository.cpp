/**
 * @file ExportTargetRepository.cpp
 * @brief Export Target Repository 구현부 (통계 필드 제거 버전)
 * @version 3.0.0 - 통계 필드 완전 제거
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/src/Database/Repositories/ExportTargetRepository.cpp
 * 
 * 주요 변경사항:
 *   - mapRowToEntity: 통계 필드 파싱 코드 제거
 *   - entityToParams: 통계 필드 변환 코드 제거
 *   - updateStatistics 메서드 삭제
 */

#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Utils/LogManager.h"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// CRUD 기본 연산
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findAll() {
    try {
        if (!ensureTableExists()) {
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
                if (logger_) {
                    logger_->Warn("Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Debug("findAll: mapped " + std::to_string(entities.size()) + 
                          " of " + std::to_string(results.size()) + " rows");
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findById(int id) {
    try {
        // 캐시 확인
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
        
        // 캐시 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findById failed: " + std::string(e.what()));
        }
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
        
        // INSERT 쿼리의 컬럼 순서대로 파라미터 치환
        std::vector<std::string> insert_order = {
            "profile_id",
            "name",
            "target_type",
            "description",
            "is_enabled",
            "config",
            "export_mode",
            "export_interval",
            "batch_size"
        };
        
        for (const auto& key : insert_order) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                std::string value = params[key];
                // SQL Injection 방지: 작은따옴표 이스케이프
                size_t escape_pos = 0;
                while ((escape_pos = value.find('\'', escape_pos)) != std::string::npos) {
                    value.replace(escape_pos, 1, "''");
                    escape_pos += 2;
                }
                query.replace(pos, 1, "'" + value + "'");
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 생성된 ID 가져오기
            auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!results.empty() && results[0].find("id") != results[0].end()) {
                entity.setId(std::stoi(results[0].at("id")));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("save failed: " + std::string(e.what()));
        }
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
        
        // UPDATE 쿼리의 SET 절 순서대로 파라미터 치환
        std::vector<std::string> update_order = {
            "profile_id",
            "name",
            "target_type",
            "description",
            "is_enabled",
            "config",
            "export_mode",
            "export_interval",
            "batch_size"
        };
        
        for (const auto& key : update_order) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                std::string value = params[key];
                // SQL Injection 방지
                size_t escape_pos = 0;
                while ((escape_pos = value.find('\'', escape_pos)) != std::string::npos) {
                    value.replace(escape_pos, 1, "''");
                    escape_pos += 2;
                }
                query.replace(pos, 1, "'" + value + "'");
            }
        }
        
        // WHERE id = ? 치환
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("update failed: " + std::string(e.what()));
        }
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("deleteById failed: " + std::string(e.what()));
        }
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByIds(
    const std::vector<int>& ids) {
    
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        std::string query = "SELECT * FROM export_targets WHERE id IN (" + 
                           ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByIds: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByIds failed: " + std::string(e.what()));
        }
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
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByConditions: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int ExportTargetRepository::countByConditions(
    const std::vector<QueryCondition>& conditions) {
    
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
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

// =============================================================================
// 전용 조회 메서드
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByEnabled(bool enabled) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_ENABLED,
            enabled ? "1" : "0"
        );
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByEnabled: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByEnabled failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByTargetType(
    const std::string& target_type) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            SQL::ExportTarget::FIND_BY_TARGET_TYPE, target_type);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByTargetType: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByTargetType failed: " + std::string(e.what()));
        }
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
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByProfileId: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByProfileId failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findByName(
    const std::string& name) {
    
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            SQL::ExportTarget::FIND_BY_NAME, name);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByName failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

// =============================================================================
// 카운트 및 통계 (설정 기반만)
// =============================================================================

int ExportTargetRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getTotalCount failed: " + std::string(e.what()));
        }
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
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getActiveCount failed: " + std::string(e.what()));
        }
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
            try {
                auto type_it = row.find("target_type");
                auto count_it = row.find("count");
                
                if (type_it != row.end() && count_it != row.end()) {
                    counts[type_it->second] = std::stoi(count_it->second);
                }
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to parse count by type row: " + 
                                 std::string(e.what()));
                }
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getCountByType failed: " + std::string(e.what()));
        }
    }
    
    return counts;
}

// =============================================================================
// 캐시 관리
// =============================================================================

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

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

ExportTargetEntity ExportTargetRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportTargetEntity entity;
    
    try {
        auto it = row.end();
        
        // ID
        it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
        
        // profile_id
        it = row.find("profile_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setProfileId(std::stoi(it->second));
        }
        
        // name
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        // target_type
        it = row.find("target_type");
        if (it != row.end()) {
            entity.setTargetType(it->second);
        }
        
        // description
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        // is_enabled
        it = row.find("is_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setEnabled(std::stoi(it->second) != 0);
        }
        
        // config
        it = row.find("config");
        if (it != row.end()) {
            entity.setConfig(it->second);
        }
        
        // export_mode
        it = row.find("export_mode");
        if (it != row.end()) {
            entity.setExportMode(it->second);
        }
        
        // export_interval
        it = row.find("export_interval");
        if (it != row.end() && !it->second.empty()) {
            entity.setExportInterval(std::stoi(it->second));
        }
        
        // batch_size
        it = row.find("batch_size");
        if (it != row.end() && !it->second.empty()) {
            entity.setBatchSize(std::stoi(it->second));
        }
        
        // ❌ 통계 필드 파싱 코드 완전 제거
        // total_exports, successful_exports, failed_exports 등
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to map row to ExportTargetEntity: " + 
                                std::string(e.what()));
    }
    
    return entity;
}

std::map<std::string, std::string> ExportTargetRepository::entityToParams(
    const ExportTargetEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    // 설정 정보만 변환 (통계 필드 제거됨)
    params["profile_id"] = std::to_string(entity.getProfileId());
    params["name"] = entity.getName();
    params["target_type"] = entity.getTargetType();
    params["description"] = entity.getDescription();
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["config"] = entity.getConfig();
    params["export_mode"] = entity.getExportMode();
    params["export_interval"] = std::to_string(entity.getExportInterval());
    params["batch_size"] = std::to_string(entity.getBatchSize());
    
    // ❌ 통계 필드 변환 코드 완전 제거
    
    return params;
}

bool ExportTargetRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(SQL::ExportTarget::CREATE_TABLE);
        
        if (success) {
            if (logger_) {
                logger_->Debug("Table export_targets is ready");
            }
        } else {
            if (logger_) {
                logger_->Error("Failed to ensure table exists");
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne