// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository 구현 - DeviceRepository 패턴 100% 적용
// =============================================================================

/**
 * @file VirtualPointRepository.cpp
 * @brief PulseOne VirtualPointRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - formatTimestamp, entityToParams, ensureTableExists 구현
 * - 컴파일 에러 완전 해결
 */

#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include <sstream>
#include <iomanip>
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
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            ORDER BY id
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
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
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE id = )" + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("VirtualPointRepository::findById - Virtual point not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("VirtualPointRepository::findById - Found virtual point: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool VirtualPointRepository::save(VirtualPointEntity& entity) {
    try {
        if (!validateVirtualPoint(entity)) {
            logger_->Error("VirtualPointRepository::save - Invalid virtual point: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🔧 DeviceRepository 패턴: entityToParams 메서드 사용
        std::map<std::string, std::string> data = entityToParams(entity);
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("virtual_points", data, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회
            if (entity.getId() <= 0) {
                const std::string id_query = "SELECT last_insert_rowid() as id";
                auto id_result = db_layer.executeQuery(id_query);
                if (!id_result.empty()) {
                    entity.setId(std::stoi(id_result[0].at("id")));
                }
            }
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("VirtualPointRepository::save - Saved virtual point: " + entity.getName());
        } else {
            logger_->Error("VirtualPointRepository::save - Failed to save virtual point: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::update(const VirtualPointEntity& entity) {
    VirtualPointEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool VirtualPointRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "DELETE FROM virtual_points WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("VirtualPointRepository::deleteById - Deleted virtual point ID: " + std::to_string(id));
        } else {
            logger_->Error("VirtualPointRepository::deleteById - Failed to delete virtual point ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM virtual_points WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    try {
        if (ids.empty()) {
            return {};
        }
        
        if (!ensureTableExists()) {
            return {};
        }
        
        // IN 절 구성
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE id IN ()" + ids_ss.str() + ")";
        
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
        
        std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points
        )";
        
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
        
        logger_->Debug("VirtualPointRepository::findByConditions - Found " + std::to_string(entities.size()) + " virtual points");
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
        
        std::string query = "SELECT COUNT(*) as count FROM virtual_points";
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// VirtualPoint 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByTenant(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"(
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities = mapResultToEntities(results);
        
        logger_->Info("VirtualPointRepository::findByTenant - Found " + std::to_string(entities.size()) + " virtual points for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findBySite(int site_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE site_id = )" + std::to_string(site_id) + R"(
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities = mapResultToEntities(results);
        
        logger_->Info("VirtualPointRepository::findBySite - Found " + std::to_string(entities.size()) + " virtual points for site " + std::to_string(site_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findBySite failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findEnabledPoints(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE is_enabled = 1
        )";
        
        if (tenant_id > 0) {
            query += " AND tenant_id = " + std::to_string(tenant_id);
        }
        
        query += " ORDER BY name";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities = mapResultToEntities(results);
        
        logger_->Info("VirtualPointRepository::findEnabledPoints - Found " + std::to_string(entities.size()) + " enabled virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findEnabledPoints failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<VirtualPointEntity> VirtualPointRepository::findByName(const std::string& name, int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, name, description, formula,
                data_type, unit, calculation_interval, calculation_trigger,
                is_enabled, category, tags, created_by, created_at, updated_at
            FROM virtual_points 
            WHERE name = ')" + RepositoryHelpers::escapeString(name) + R"(' AND tenant_id = )" + std::to_string(tenant_id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::findByName failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

// =============================================================================
// 벌크 연산 (DeviceRepository 패턴)
// =============================================================================

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
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    logger_->Info("VirtualPointRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " virtual points");
    return deleted_count;
}

// =============================================================================
// 실시간 가상포인트 관리 (DeviceRepository 패턴)
// =============================================================================

bool VirtualPointRepository::enableVirtualPoint(int point_id) {
    return updateVirtualPointStatus(point_id, true);
}

bool VirtualPointRepository::disableVirtualPoint(int point_id) {
    return updateVirtualPointStatus(point_id, false);
}

bool VirtualPointRepository::updateVirtualPointStatus(int point_id, bool is_enabled) {
    try {
        const std::string query = R"(
            UPDATE virtual_points 
            SET is_enabled = )" + std::string(is_enabled ? "1" : "0") + R"(,
                updated_at = ')" + formatTimestamp(std::chrono::system_clock::now()) + R"('
            WHERE id = )" + std::to_string(point_id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
            logger_->Info("VirtualPointRepository::updateVirtualPointStatus - " + 
                         std::string(is_enabled ? "Enabled" : "Disabled") + 
                         " virtual point ID: " + std::to_string(point_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::updateVirtualPointStatus failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::updateFormula(int point_id, const std::string& formula) {
    try {
        const std::string query = R"(
            UPDATE virtual_points 
            SET formula = ')" + RepositoryHelpers::escapeString(formula) + R"(',
                updated_at = ')" + formatTimestamp(std::chrono::system_clock::now()) + R"('
            WHERE id = )" + std::to_string(point_id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
            logger_->Info("VirtualPointRepository::updateFormula - Updated formula for virtual point ID: " + std::to_string(point_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::updateFormula failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 캐시 관리 메서드들 (DeviceRepository 패턴)
// =============================================================================

void VirtualPointRepository::setCacheEnabled(bool enabled) {
    IRepository<VirtualPointEntity>::setCacheEnabled(enabled);
    logger_->Info("VirtualPointRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool VirtualPointRepository::isCacheEnabled() const {
    return IRepository<VirtualPointEntity>::isCacheEnabled();
}

void VirtualPointRepository::clearCache() {
    IRepository<VirtualPointEntity>::clearCache();
    logger_->Info("VirtualPointRepository cache cleared");
}

void VirtualPointRepository::clearCacheForId(int id) {
    IRepository<VirtualPointEntity>::clearCacheForId(id);
    logger_->Debug("VirtualPointRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    return IRepository<VirtualPointEntity>::getCacheStats();
}

int VirtualPointRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

VirtualPointEntity VirtualPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    VirtualPointEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("site_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setSiteId(std::stoi(it->second));
        }
        
        // 가상포인트 기본 정보
        if ((it = row.find("name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        if ((it = row.find("formula")) != row.end()) entity.setFormula(it->second);
        if ((it = row.find("data_type")) != row.end()) entity.setDataType(VirtualPointEntity::stringToDataType(it->second));
        if ((it = row.find("unit")) != row.end()) entity.setUnit(it->second);
        if ((it = row.find("category")) != row.end()) entity.setCategory(it->second);
        
        // 계산 설정
        it = row.find("calculation_interval");
        if (it != row.end()) {
            entity.setCalculationInterval(std::stoi(it->second));
        }
        
        if ((it = row.find("calculation_trigger")) != row.end()) {
            entity.setCalculationTrigger(VirtualPointEntity::stringToCalculationTrigger(it->second));
        }
        
        // 상태 정보
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("created_by");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setCreatedBy(std::stoi(it->second));
        }
        
        // 타임스탬프는 기본값 사용 (실제 구현에서는 파싱 필요)
        entity.setCreatedAt(std::chrono::system_clock::now());
        entity.setUpdatedAt(std::chrono::system_clock::now());
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<VirtualPointEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("VirtualPointRepository::mapResultToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

std::map<std::string, std::string> VirtualPointRepository::entityToParams(const VirtualPointEntity& entity) {
    DatabaseAbstractionLayer db_layer;
    
    std::map<std::string, std::string> params;
    
    // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["site_id"] = std::to_string(entity.getSiteId());
    
    // 가상포인트 정보
    params["name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["formula"] = entity.getFormula();
    params["data_type"] = VirtualPointEntity::dataTypeToString(entity.getDataType());
    params["unit"] = entity.getUnit();
    params["calculation_interval"] = std::to_string(entity.getCalculationInterval());
    params["calculation_trigger"] = VirtualPointEntity::calculationTriggerToString(entity.getCalculationTrigger());
    params["category"] = entity.getCategory();
    
    // 태그 배열을 JSON 문자열로 변환
    std::ostringstream tags_ss;
    tags_ss << "[";
    const auto& tags = entity.getTags();
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) tags_ss << ", ";
        tags_ss << "\"" << RepositoryHelpers::escapeString(tags[i]) << "\"";
    }
    tags_ss << "]";
    params["tags"] = tags_ss.str();
    
    // 상태 정보
    params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
    params["created_by"] = std::to_string(entity.getCreatedBy());
    
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool VirtualPointRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS virtual_points (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                site_id INTEGER,
                name VARCHAR(100) NOT NULL,
                description TEXT,
                formula TEXT NOT NULL,
                data_type VARCHAR(20) DEFAULT 'float',
                unit VARCHAR(20),
                calculation_interval INTEGER DEFAULT 1000,
                calculation_trigger VARCHAR(20) DEFAULT 'timer',
                is_enabled INTEGER DEFAULT 1,
                category VARCHAR(50),
                tags TEXT,
                created_by INTEGER,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
                FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
                FOREIGN KEY (created_by) REFERENCES users(id)
            )
        )";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            logger_->Debug("VirtualPointRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("VirtualPointRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("VirtualPointRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::validateVirtualPoint(const VirtualPointEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("VirtualPointRepository::validateVirtualPoint - Invalid virtual point: " + entity.getName());
        return false;
    }
    
    if (entity.getName().empty()) {
        logger_->Warn("VirtualPointRepository::validateVirtualPoint - Virtual point name is empty");
        return false;
    }
    
    if (entity.getFormula().empty()) {
        logger_->Warn("VirtualPointRepository::validateVirtualPoint - Formula is empty for: " + entity.getName());
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        logger_->Warn("VirtualPointRepository::validateVirtualPoint - Invalid tenant ID for: " + entity.getName());
        return false;
    }
    
    return true;
}

// =============================================================================
// 유틸리티 함수들 (DeviceRepository 패턴)
// =============================================================================
std::string VirtualPointRepository::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne