// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository 구현 - DeviceRepository/DataPointRepository 패턴 100% 적용
// =============================================================================

#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (DeviceRepository 패턴)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("VirtualPointRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::VirtualPoint::FIND_ALL);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("VirtualPointRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("VirtualPointRepository::findAll - Found " + std::to_string(entities.size()) + " virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<VirtualPointEntity> VirtualPointRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("VirtualPointRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::VirtualPoint::FIND_BY_ID, std::to_string(id)
        );
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool VirtualPointRepository::save(VirtualPointEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQLQueries.h 상수 사용
        std::string query = SQL::VirtualPoint::INSERT;
        
        // 파라미터 치환 (15개 파라미터)
        std::vector<std::string> params = {
            std::to_string(entity.getTenantId()),
            entity.getSiteId() ? std::to_string(*entity.getSiteId()) : "NULL",
            entity.getDeviceId() ? std::to_string(*entity.getDeviceId()) : "NULL",
            entity.getName(),
            entity.getDescription(),
            entity.getFormula(),
            entity.getDataType(),
            entity.getUnit(),
            std::to_string(entity.getCalculationInterval()),
            entity.getCalculationTrigger(),
            entity.getIsEnabled() ? "1" : "0",
            entity.getCategory(),
            entity.getTags(),
            entity.getScopeType(),
            entity.getCreatedBy()
        };
        
        // 파라미터 치환
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                if (param == "NULL") {
                    query.replace(pos, 1, "NULL");
                } else {
                    query.replace(pos, 1, "'" + param + "'");
                }
            }
        }
        
        if (db_layer.executeNonQuery(query)) {
            // 새로 생성된 ID 가져오기
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty()) {
                entity.setId(std::stoi(id_results[0].at("id")));
                entity.markSaved();
                
                // 캐시에 추가
                if (isCacheEnabled()) {
                    cacheEntity(entity);
                }
                
                logger_->Info("VirtualPointRepository::save - Saved virtual point with ID: " + 
                            std::to_string(entity.getId()));
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::update(const VirtualPointEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQLQueries.h 상수 사용
        std::string query = SQL::VirtualPoint::UPDATE;
        
        // 파라미터 치환 (16개 파라미터 - 마지막이 id)
        std::vector<std::string> params = {
            std::to_string(entity.getTenantId()),
            entity.getSiteId() ? std::to_string(*entity.getSiteId()) : "NULL",
            entity.getDeviceId() ? std::to_string(*entity.getDeviceId()) : "NULL",
            entity.getName(),
            entity.getDescription(),
            entity.getFormula(),
            entity.getDataType(),
            entity.getUnit(),
            std::to_string(entity.getCalculationInterval()),
            entity.getCalculationTrigger(),
            entity.getIsEnabled() ? "1" : "0",
            entity.getCategory(),
            entity.getTags(),
            entity.getScopeType(),
            std::to_string(entity.getId()) // WHERE 절의 id
        };
        
        // 파라미터 치환
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                if (param == "NULL") {
                    query.replace(pos, 1, "NULL");
                } else {
                    query.replace(pos, 1, "'" + param + "'");
                }
            }
        }
        
        if (db_layer.executeNonQuery(query)) {
            // 캐시 업데이트
            if (isCacheEnabled()) {
                clearCacheForId(entity.getId());
                cacheEntity(entity);
            }
            
            logger_->Info("VirtualPointRepository::update - Updated virtual point ID: " + std::to_string(entity.getId()));
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::VirtualPoint::DELETE_BY_ID, std::to_string(id)
        );
        
        if (db_layer.executeNonQuery(query)) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("VirtualPointRepository::deleteById - Deleted virtual point ID: " + std::to_string(id));
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::exists(int id) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::VirtualPoint::EXISTS_BY_ID, std::to_string(id)
        );
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산 (DataPointRepository 패턴)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // IN 절 구성
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        // 기본 쿼리에 WHERE 절 추가
        std::string query = SQL::VirtualPoint::FIND_ALL;
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("VirtualPointRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("VirtualPointRepository::findByIds - Found " + std::to_string(entities.size()) + " virtual points for " + std::to_string(ids.size()) + " IDs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // 기본 쿼리 사용 후 조건 추가
        std::string query = SQL::VirtualPoint::FIND_ALL;
        
        // ORDER BY 제거 후 조건 추가
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query = query.substr(0, order_pos);
        }
        
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("VirtualPointRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM " + getTableName();
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::saveBulk(std::vector<VirtualPointEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    logger_->Info("VirtualPointRepository::saveBulk - Saved " + std::to_string(saved_count) + " virtual points");
    return saved_count;
}

int VirtualPointRepository::updateBulk(const std::vector<VirtualPointEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    logger_->Info("VirtualPointRepository::updateBulk - Updated " + std::to_string(updated_count) + " virtual points");
    return updated_count;
}

int VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return 0;
    }
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        int deleted_count = 0;
        
        for (int id : ids) {
            if (deleteById(id)) {
                deleted_count++;
            }
        }
        
        logger_->Info("VirtualPointRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " virtual points");
        return deleted_count;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 캐시 관리 (IRepository에서 상속받은 메서드들 위임)
// =============================================================================

void VirtualPointRepository::clearCache() {
    IRepository<VirtualPointEntity>::clearCache();
    logger_->Info("VirtualPointRepository cache cleared");
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    return IRepository<VirtualPointEntity>::getCacheStats();
}

// =============================================================================
// VirtualPoint 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByTenant(int tenant_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findBySite(int site_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("site_id", "=", std::to_string(site_id))
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByDevice(int device_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("device_id", "=", std::to_string(device_id))
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findEnabled() {
    std::vector<QueryCondition> conditions = {
        QueryCondition("is_enabled", "=", "1")
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByCategory(const std::string& category) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("category", "=", category)
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByExecutionType(const std::string& execution_type) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("execution_type", "=", execution_type)
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByTag(const std::string& tag) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("tags", "LIKE", "%" + tag + "%")
    };
    return findByConditions(conditions);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findDependents(int point_id) {
    std::string point_ref = "\"point_id\":" + std::to_string(point_id);
    std::vector<QueryCondition> conditions = {
        QueryCondition("dependencies", "LIKE", "%" + point_ref + "%")
    };
    return findByConditions(conditions);
}

bool VirtualPointRepository::updateExecutionStats(int id, double value, double execution_time_ms) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "UPDATE " + getTableName() + 
                          " SET last_value = " + std::to_string(value) +
                          ", execution_count = execution_count + 1" +
                          ", avg_execution_time_ms = ((avg_execution_time_ms * execution_count) + " + 
                          std::to_string(execution_time_ms) + ") / (execution_count + 1)" +
                          ", last_error = ''" +
                          ", updated_at = datetime('now')" +
                          " WHERE id = " + std::to_string(id);
        
        if (db_layer.executeNonQuery(query)) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::updateExecutionStats failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::updateError(int id, const std::string& error_message) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "UPDATE " + getTableName() + 
                          " SET last_error = '" + RepositoryHelpers::escapeString(error_message) + "'" +
                          ", updated_at = datetime('now')" +
                          " WHERE id = " + std::to_string(id);
        
        if (db_layer.executeNonQuery(query)) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::updateError failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::setEnabled(int id, bool enabled) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "UPDATE " + getTableName() + 
                          " SET is_enabled = " + (enabled ? "1" : "0") +
                          ", updated_at = datetime('now')" +
                          " WHERE id = " + std::to_string(id);
        
        if (db_layer.executeNonQuery(query)) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::setEnabled failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Protected 메서드 구현 (DataPointRepository 패턴)
// =============================================================================

VirtualPointEntity VirtualPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        VirtualPointEntity entity;
        
        // 기본 필드
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("scope_type");
        if (it != row.end()) {
            entity.setScopeType(it->second);
        }
        
        // 선택적 필드
        it = row.find("site_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setSiteId(std::stoi(it->second));
        }
        
        it = row.find("device_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setDeviceId(std::stoi(it->second));
        }
        
        // 텍스트 필드
        it = row.find("name");
        if (it != row.end()) entity.setName(it->second);
        
        it = row.find("description");
        if (it != row.end()) entity.setDescription(it->second);
        
        it = row.find("formula");
        if (it != row.end()) entity.setFormula(it->second);
        
        it = row.find("data_type");
        if (it != row.end()) entity.setDataType(it->second);
        
        it = row.find("unit");
        if (it != row.end()) entity.setUnit(it->second);
        
        // 숫자 필드
        it = row.find("calculation_interval");
        if (it != row.end()) entity.setCalculationInterval(std::stoi(it->second));
        
        it = row.find("cache_duration_ms");
        if (it != row.end()) entity.setCacheDurationMs(std::stoi(it->second));
        
        // 트리거
        it = row.find("calculation_trigger");
        if (it != row.end()) entity.setCalculationTrigger(it->second);
        
        // 불린 필드
        it = row.find("is_enabled");
        if (it != row.end()) entity.setIsEnabled(it->second == "1");
        
        // 메타데이터
        it = row.find("category");
        if (it != row.end()) entity.setCategory(it->second);
        
        it = row.find("tags");
        if (it != row.end()) entity.setTags(it->second);
        
        // 실행 통계
        it = row.find("execution_count");
        if (it != row.end()) entity.setExecutionCount(std::stoi(it->second));
        
        it = row.find("last_value");
        if (it != row.end()) entity.setLastValue(std::stod(it->second));
        
        it = row.find("last_error");
        if (it != row.end()) entity.setLastError(it->second);
        
        it = row.find("avg_execution_time_ms");
        if (it != row.end()) entity.setAvgExecutionTimeMs(std::stod(it->second));
        
        it = row.find("created_by");
        if (it != row.end()) entity.setCreatedBy(it->second);
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> VirtualPointRepository::mapEntityToRow(const VirtualPointEntity& entity) {
    std::map<std::string, std::string> row;
    
    row["id"] = std::to_string(entity.getId());
    row["tenant_id"] = std::to_string(entity.getTenantId());
    row["scope_type"] = entity.getScopeType();
    
    if (entity.getSiteId()) {
        row["site_id"] = std::to_string(*entity.getSiteId());
    } else {
        row["site_id"] = "NULL";
    }
    
    if (entity.getDeviceId()) {
        row["device_id"] = std::to_string(*entity.getDeviceId());
    } else {
        row["device_id"] = "NULL";
    }
    
    row["name"] = entity.getName();
    row["description"] = entity.getDescription();
    row["formula"] = entity.getFormula();
    row["data_type"] = entity.getDataType();
    row["unit"] = entity.getUnit();
    row["calculation_interval"] = std::to_string(entity.getCalculationInterval());
    row["calculation_trigger"] = entity.getCalculationTrigger();
    row["cache_duration_ms"] = std::to_string(entity.getCacheDurationMs());
    row["is_enabled"] = entity.getIsEnabled() ? "1" : "0";
    row["category"] = entity.getCategory();
    row["tags"] = entity.getTags();
    row["execution_count"] = std::to_string(entity.getExecutionCount());
    row["last_value"] = std::to_string(entity.getLastValue());
    row["last_error"] = entity.getLastError();
    row["avg_execution_time_ms"] = std::to_string(entity.getAvgExecutionTimeMs());
    row["created_by"] = entity.getCreatedBy();
    
    return row;
}

bool VirtualPointRepository::ensureTableExists() {
    return createTable();
}

// =============================================================================
// Private 메서드
// =============================================================================

bool VirtualPointRepository::createTable() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeCreateTable(SQL::VirtualPoint::CREATE_TABLE);
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::createTable failed: " + std::string(e.what()));
        return false;
    }
}

std::string VirtualPointRepository::generateCacheKey(int id) const {
    return "virtualpoint:" + std::to_string(id);
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne