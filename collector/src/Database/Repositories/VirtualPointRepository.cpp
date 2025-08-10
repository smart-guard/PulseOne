// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository 구현 - SQLQueries.h 패턴 적용
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
// IRepository 기본 CRUD 구현
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
        
        std::string query = RepositoryHelpers::replaceParameterMarkers(
            SQL::VirtualPoint::FIND_BY_ID, {std::to_string(id)}
        );
        
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(id, entity);
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
        
        auto row = mapEntityToRow(entity);
        
        // id 제거 (자동 증가)
        row.erase("id");
        
        // INSERT 쿼리 생성
        std::vector<std::string> columns;
        std::vector<std::string> values;
        
        for (const auto& [col, val] : row) {
            columns.push_back(col);
            values.push_back("'" + val + "'");
        }
        
        std::string query = "INSERT INTO " + getTableName() + " (" +
                          RepositoryHelpers::join(columns, ", ") + ") VALUES (" +
                          RepositoryHelpers::join(values, ", ") + ")";
        
        if (db_layer.executeNonQuery(query)) {
            // 새로 생성된 ID 가져오기
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty()) {
                entity.setId(std::stoi(id_results[0].at("id")));
                entity.markSaved();
                
                // 캐시에 추가
                if (isCacheEnabled()) {
                    cacheEntity(entity.getId(), entity);
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
        
        auto row = mapEntityToRow(entity);
        int id = entity.getId();
        
        // id 제거 (UPDATE에서는 WHERE 절에서 사용)
        row.erase("id");
        
        // UPDATE 쿼리 생성
        std::vector<std::string> set_clauses;
        for (const auto& [col, val] : row) {
            set_clauses.push_back(col + " = '" + val + "'");
        }
        
        std::string query = "UPDATE " + getTableName() + " SET " +
                          RepositoryHelpers::join(set_clauses, ", ") +
                          " WHERE id = " + std::to_string(id);
        
        if (db_layer.executeNonQuery(query)) {
            // 캐시 업데이트
            if (isCacheEnabled()) {
                invalidateCachedEntity(id);
                cacheEntity(id, entity);
            }
            
            logger_->Info("VirtualPointRepository::update - Updated virtual point ID: " + std::to_string(id));
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
        
        std::string query = RepositoryHelpers::replaceParameterMarkers(
            SQL::VirtualPoint::DELETE_BY_ID, {std::to_string(id)}
        );
        
        if (db_layer.executeNonQuery(query)) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                invalidateCachedEntity(id);
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
        
        std::string query = RepositoryHelpers::replaceParameterMarkers(
            SQL::VirtualPoint::EXISTS_BY_ID, {std::to_string(id)}
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
// 벌크 연산
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> id_strings;
        for (int id : ids) {
            id_strings.push_back(std::to_string(id));
        }
        
        std::string query = "SELECT * FROM " + getTableName() + 
                          " WHERE id IN (" + RepositoryHelpers::join(id_strings, ",") + ")";
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
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
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "SELECT * FROM " + getTableName();
        
        // WHERE 절 추가
        if (!conditions.empty()) {
            query += " WHERE ";
            std::vector<std::string> where_clauses;
            for (const auto& cond : conditions) {
                where_clauses.push_back(cond.field + " " + cond.operation + " '" + cond.value + "'");
            }
            query += RepositoryHelpers::join(where_clauses, " AND ");
        }
        
        // ORDER BY 절 추가
        if (order_by) {
            query += " ORDER BY " + order_by->toSql();
        }
        
        // LIMIT/OFFSET 추가
        if (pagination) {
            query += " LIMIT " + std::to_string(pagination->limit);
            if (pagination->offset > 0) {
                query += " OFFSET " + std::to_string(pagination->offset);
            }
        }
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        std::string query = "SELECT COUNT(*) as count FROM " + getTableName();
        
        if (!conditions.empty()) {
            query += " WHERE ";
            std::vector<std::string> where_clauses;
            for (const auto& cond : conditions) {
                where_clauses.push_back(cond.field + " " + cond.operation + " '" + cond.value + "'");
            }
            query += RepositoryHelpers::join(where_clauses, " AND ");
        }
        
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

bool VirtualPointRepository::saveAll(std::vector<VirtualPointEntity>& entities) {
    bool all_success = true;
    for (auto& entity : entities) {
        if (!save(entity)) {
            all_success = false;
        }
    }
    return all_success;
}

bool VirtualPointRepository::updateAll(const std::vector<VirtualPointEntity>& entities) {
    bool all_success = true;
    for (const auto& entity : entities) {
        if (!update(entity)) {
            all_success = false;
        }
    }
    return all_success;
}

bool VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    bool all_success = true;
    for (int id : ids) {
        if (!deleteById(id)) {
            all_success = false;
        }
    }
    return all_success;
}

// =============================================================================
// 캐시 관리
// =============================================================================

void VirtualPointRepository::clearCache() {
    if (cache_) {
        cache_->clear();
        logger_->Info("VirtualPointRepository cache cleared");
    }
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    if (cache_) {
        stats["size"] = cache_->size();
        stats["hits"] = cache_->getHits();
        stats["misses"] = cache_->getMisses();
    }
    return stats;
}

// =============================================================================
// VirtualPoint 전용 메서드
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByTenant(int tenant_id) {
    return findByConditions({QueryCondition::Equal("tenant_id", std::to_string(tenant_id))});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findBySite(int site_id) {
    return findByConditions({QueryCondition::Equal("site_id", std::to_string(site_id))});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByDevice(int device_id) {
    return findByConditions({QueryCondition::Equal("device_id", std::to_string(device_id))});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findEnabled() {
    return findByConditions({QueryCondition::Equal("is_enabled", "1")});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByCategory(const std::string& category) {
    return findByConditions({QueryCondition::Equal("category", category)});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByExecutionType(const std::string& execution_type) {
    return findByConditions({QueryCondition::Equal("execution_type", execution_type)});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByTag(const std::string& tag) {
    return findByConditions({QueryCondition::Like("tags", "%" + tag + "%")});
}

std::vector<VirtualPointEntity> VirtualPointRepository::findDependents(int point_id) {
    std::string point_ref = "\"point_id\":" + std::to_string(point_id);
    return findByConditions({QueryCondition::Like("dependencies", "%" + point_ref + "%")});
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
                invalidateCachedEntity(id);
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
                          " SET last_error = '" + error_message + "'" +
                          ", updated_at = datetime('now')" +
                          " WHERE id = " + std::to_string(id);
        
        if (db_layer.executeNonQuery(query)) {
            if (isCacheEnabled()) {
                invalidateCachedEntity(id);
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
                invalidateCachedEntity(id);
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
// Protected 메서드 구현
// =============================================================================

VirtualPointEntity VirtualPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    VirtualPointEntity entity;
    
    // 필수 필드
    if (row.count("id")) entity.setId(std::stoi(row.at("id")));
    if (row.count("tenant_id")) entity.setTenantId(std::stoi(row.at("tenant_id")));
    if (row.count("scope_type")) entity.setScopeType(row.at("scope_type"));
    
    // 선택 필드
    if (row.count("site_id") && !row.at("site_id").empty() && row.at("site_id") != "NULL") {
        entity.setSiteId(std::stoi(row.at("site_id")));
    }
    if (row.count("device_id") && !row.at("device_id").empty() && row.at("device_id") != "NULL") {
        entity.setDeviceId(std::stoi(row.at("device_id")));
    }
    
    // 텍스트 필드
    if (row.count("name")) entity.setName(row.at("name"));
    if (row.count("description")) entity.setDescription(row.at("description"));
    if (row.count("formula")) entity.setFormula(row.at("formula"));
    if (row.count("data_type")) entity.setDataType(row.at("data_type"));
    if (row.count("unit")) entity.setUnit(row.at("unit"));
    
    // 숫자 필드
    if (row.count("calculation_interval")) entity.setCalculationInterval(std::stoi(row.at("calculation_interval")));
    if (row.count("cache_duration_ms")) entity.setCacheDurationMs(std::stoi(row.at("cache_duration_ms")));
    
    // 트리거 및 타입
    if (row.count("calculation_trigger")) entity.setCalculationTrigger(row.at("calculation_trigger"));
    
    if (row.count("execution_type")) {
        std::string exec_type = row.at("execution_type");
        if (exec_type == "javascript") entity.setExecutionType(VirtualPointEntity::ExecutionType::JAVASCRIPT);
        else if (exec_type == "formula") entity.setExecutionType(VirtualPointEntity::ExecutionType::FORMULA);
        else if (exec_type == "aggregate") entity.setExecutionType(VirtualPointEntity::ExecutionType::AGGREGATE);
        else entity.setExecutionType(VirtualPointEntity::ExecutionType::REFERENCE);
    }
    
    if (row.count("error_handling")) {
        std::string error_handling = row.at("error_handling");
        if (error_handling == "return_null") entity.setErrorHandling(VirtualPointEntity::ErrorHandling::RETURN_NULL);
        else if (error_handling == "return_last") entity.setErrorHandling(VirtualPointEntity::ErrorHandling::RETURN_LAST);
        else if (error_handling == "return_zero") entity.setErrorHandling(VirtualPointEntity::ErrorHandling::RETURN_ZERO);
        else entity.setErrorHandling(VirtualPointEntity::ErrorHandling::RETURN_DEFAULT);
    }
    
    // JSON 필드
    if (row.count("input_mappings")) entity.setInputMappings(row.at("input_mappings"));
    if (row.count("dependencies")) entity.setDependencies(row.at("dependencies"));
    if (row.count("tags")) entity.setTags(row.at("tags"));
    
    // 불린 필드
    if (row.count("is_enabled")) entity.setIsEnabled(row.at("is_enabled") == "1");
    
    // 메타데이터
    if (row.count("category")) entity.setCategory(row.at("category"));
    
    // 실행 통계
    if (row.count("execution_count")) entity.setExecutionCount(std::stoi(row.at("execution_count")));
    if (row.count("last_value")) entity.setLastValue(std::stod(row.at("last_value")));
    if (row.count("last_error")) entity.setLastError(row.at("last_error"));
    if (row.count("avg_execution_time_ms")) entity.setAvgExecutionTimeMs(std::stod(row.at("avg_execution_time_ms")));
    
    if (row.count("created_by")) entity.setCreatedBy(row.at("created_by"));
    
    entity.markSaved();
    return entity;
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
    
    // Enum 변환
    switch (entity.getExecutionType()) {
        case VirtualPointEntity::ExecutionType::JAVASCRIPT: row["execution_type"] = "javascript"; break;
        case VirtualPointEntity::ExecutionType::FORMULA: row["execution_type"] = "formula"; break;
        case VirtualPointEntity::ExecutionType::AGGREGATE: row["execution_type"] = "aggregate"; break;
        case VirtualPointEntity::ExecutionType::REFERENCE: row["execution_type"] = "reference"; break;
    }
    
    switch (entity.getErrorHandling()) {
        case VirtualPointEntity::ErrorHandling::RETURN_NULL: row["error_handling"] = "return_null"; break;
        case VirtualPointEntity::ErrorHandling::RETURN_LAST: row["error_handling"] = "return_last"; break;
        case VirtualPointEntity::ErrorHandling::RETURN_ZERO: row["error_handling"] = "return_zero"; break;
        case VirtualPointEntity::ErrorHandling::RETURN_DEFAULT: row["error_handling"] = "return_default"; break;
    }
    
    row["input_mappings"] = entity.getInputMappings();
    row["dependencies"] = entity.getDependencies();
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