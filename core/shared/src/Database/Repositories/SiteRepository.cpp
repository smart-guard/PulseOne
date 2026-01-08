/**
 * @file SiteRepository.cpp
 * @brief PulseOne SiteRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DbLib::DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 컴파일 에러 완전 해결
 * - formatTimestamp, ensureTableExists 문제 해결
 */

#include "Database/Repositories/SiteRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "DatabaseAbstractionLayer.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (DeviceRepository 패턴)
// =============================================================================

std::vector<SiteEntity> SiteRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("SiteRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            ORDER BY hierarchy_level, name
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("SiteRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("SiteRepository::findAll - Found " + std::to_string(entities.size()) + " sites");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<SiteEntity> SiteRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("SiteRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE id = )" + std::to_string(id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("SiteRepository::findById - Site not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("SiteRepository::findById - Found site: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool SiteRepository::save(SiteEntity& entity) {
    try {
        if (!validateSite(entity)) {
            logger_->Error("SiteRepository::save - Invalid site: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        
        // entityToParams 메서드 사용하여 맵 생성
        std::map<std::string, std::string> data = entityToParams(entity);
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("sites", data, primary_keys);
        
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
            
            logger_->Info("SiteRepository::save - Saved site: " + entity.getName());
        } else {
            logger_->Error("SiteRepository::save - Failed to save site: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::update(const SiteEntity& entity) {
    SiteEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool SiteRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        // 하위 사이트가 있는지 확인
        if (hasChildSites(id)) {
            logger_->Error("SiteRepository::deleteById - Cannot delete site with children: " + std::to_string(id));
            return false;
        }
        
        const std::string query = "DELETE FROM sites WHERE id = " + std::to_string(id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("SiteRepository::deleteById - Deleted site ID: " + std::to_string(id));
        } else {
            logger_->Error("SiteRepository::deleteById - Failed to delete site ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM sites WHERE id = " + std::to_string(id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<SiteEntity> SiteRepository::findByIds(const std::vector<int>& ids) {
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
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE id IN ()" + ids_ss.str() + ")";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("SiteRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("SiteRepository::findByIds - Found " + std::to_string(entities.size()) + " sites for " + std::to_string(ids.size()) + " IDs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<SiteEntity> SiteRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites
        )";
        
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("SiteRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Debug("SiteRepository::findByConditions - Found " + std::to_string(entities.size()) + " sites");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int SiteRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM sites";
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// Site 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::vector<SiteEntity> SiteRepository::findByTenant(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"(
            ORDER BY hierarchy_level, name
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities = mapResultToEntities(results);
        
        logger_->Info("SiteRepository::findByTenant - Found " + std::to_string(entities.size()) + " sites for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<SiteEntity> SiteRepository::findByParentSite(int parent_site_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE parent_site_id = )" + std::to_string(parent_site_id) + R"(
            ORDER BY name
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities = mapResultToEntities(results);
        
        logger_->Info("SiteRepository::findByParentSite - Found " + std::to_string(entities.size()) + " child sites for parent " + std::to_string(parent_site_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findByParentSite failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<SiteEntity> SiteRepository::findBySiteType(SiteEntity::SiteType site_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE site_type = ')" + RepositoryHelpers::escapeString(SiteEntity::siteTypeToString(site_type)) + R"(' AND is_active = 1 
            ORDER BY name
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("SiteRepository::findBySiteType - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("SiteRepository::findBySiteType - Found " + std::to_string(entities.size()) + " sites for type: " + SiteEntity::siteTypeToString(site_type));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findBySiteType failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<SiteEntity> SiteRepository::findActiveSites(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE is_active = 1
        )";
        
        if (tenant_id > 0) {
            query += " AND tenant_id = " + std::to_string(tenant_id);
        }
        query += " ORDER BY hierarchy_level, name";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities = mapResultToEntities(results);
        
        logger_->Info("SiteRepository::findActiveSites - Found " + std::to_string(entities.size()) + " active sites");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findActiveSites failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<SiteEntity> SiteRepository::findRootSites(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"( 
            AND (parent_site_id IS NULL OR parent_site_id = 0)
            ORDER BY name
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<SiteEntity> entities = mapResultToEntities(results);
        
        logger_->Info("SiteRepository::findRootSites - Found " + std::to_string(entities.size()) + " root sites for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findRootSites failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<SiteEntity> SiteRepository::findByCode(const std::string& code, int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, parent_site_id, name, code, site_type, description,
                location, timezone, address, city, country, latitude, longitude,
                hierarchy_level, hierarchy_path, is_active, contact_name,
                contact_email, contact_phone, created_at, updated_at
            FROM sites 
            WHERE code = ')" + RepositoryHelpers::escapeString(code) + R"(' AND tenant_id = )" + std::to_string(tenant_id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("SiteRepository::findByCode - Site not found: " + code);
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        logger_->Debug("SiteRepository::findByCode - Found site: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::findByCode failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::map<std::string, std::vector<SiteEntity>> SiteRepository::groupBySiteType() {
    std::map<std::string, std::vector<SiteEntity>> grouped;
    
    try {
        auto sites = findAll();
        for (const auto& site : sites) {
            grouped[SiteEntity::siteTypeToString(site.getSiteType())].push_back(site);
        }
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::groupBySiteType failed: " + std::string(e.what()));
    }
    
    return grouped;
}

// =============================================================================
// 벌크 연산 (DeviceRepository 패턴)
// =============================================================================

int SiteRepository::saveBulk(std::vector<SiteEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    logger_->Info("SiteRepository::saveBulk - Saved " + std::to_string(saved_count) + " sites");
    return saved_count;
}

int SiteRepository::updateBulk(const std::vector<SiteEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    logger_->Info("SiteRepository::updateBulk - Updated " + std::to_string(updated_count) + " sites");
    return updated_count;
}

int SiteRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    logger_->Info("SiteRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " sites");
    return deleted_count;
}

// =============================================================================
// 실시간 사이트 관리
// =============================================================================

bool SiteRepository::activateSite(int site_id) {
    return updateSiteStatus(site_id, true);
}

bool SiteRepository::deactivateSite(int site_id) {
    return updateSiteStatus(site_id, false);
}

bool SiteRepository::updateSiteStatus(int site_id, bool is_active) {
    try {
        const std::string query = R"(
            UPDATE sites 
            SET is_active = )" + std::string(is_active ? "1" : "0") + R"(,
                updated_at = ')" + RepositoryHelpers::formatTimestamp(std::chrono::system_clock::now()) + R"('
            WHERE id = )" + std::to_string(site_id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(site_id);
            }
            logger_->Info("SiteRepository::updateSiteStatus - " + 
                         std::string(is_active ? "Activated" : "Deactivated") + 
                         " site ID: " + std::to_string(site_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::updateSiteStatus failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::updateHierarchyPath(SiteEntity& entity) {
    try {
        if (entity.isRootSite()) {
            // 루트 사이트
            entity.setHierarchyPath("/" + std::to_string(entity.getId()));
            entity.setHierarchyLevel(1);
        } else {
            // 하위 사이트 - 부모 사이트 정보 조회
            auto parent = findById(entity.getParentSiteId());
            if (parent.has_value()) {
                entity.setHierarchyPath(parent->getHierarchyPath() + "/" + std::to_string(entity.getId()));
                entity.setHierarchyLevel(parent->getHierarchyLevel() + 1);
            } else {
                // 부모를 찾을 수 없으면 루트로 처리
                entity.setHierarchyPath("/" + std::to_string(entity.getId()));
                entity.setHierarchyLevel(1);
            }
        }
        
        // DB 업데이트
        const std::string query = R"(
            UPDATE sites 
            SET hierarchy_path = ')" + RepositoryHelpers::escapeString(entity.getHierarchyPath()) + R"(',
                hierarchy_level = )" + std::to_string(entity.getHierarchyLevel()) + R"(,
                updated_at = ')" + RepositoryHelpers::formatTimestamp(std::chrono::system_clock::now()) + R"('
            WHERE id = )" + std::to_string(entity.getId());
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(entity.getId());
            }
            logger_->Info("SiteRepository::updateHierarchyPath - Updated hierarchy for site: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::updateHierarchyPath failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::hasChildSites(int site_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM sites WHERE parent_site_id = " + std::to_string(site_id);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::hasChildSites failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 통계 및 분석
// =============================================================================

std::string SiteRepository::getSiteStatistics() const {
    return "{ \"error\": \"Statistics not implemented\" }";
}

std::vector<SiteEntity> SiteRepository::findInactiveSites() const {
    // 임시 구현
    return {};
}

std::map<std::string, int> SiteRepository::getSiteTypeDistribution() const {
    std::map<std::string, int> distribution;
    
    try {
        const std::string query = R"(
            SELECT site_type, COUNT(*) as count 
            FROM sites 
            GROUP BY site_type
            ORDER BY count DESC
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            if (row.find("site_type") != row.end() && row.find("count") != row.end()) {
                distribution[row.at("site_type")] = std::stoi(row.at("count"));
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::getSiteTypeDistribution failed: " + std::string(e.what()));
        }
    }
    
    return distribution;
}

int SiteRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

SiteEntity SiteRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    SiteEntity entity;
    
    try {
        DbLib::DatabaseAbstractionLayer db_layer;
        
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("parent_site_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setParentSiteId(std::stoi(it->second));
        }
        
        // 사이트 기본 정보
        if ((it = row.find("name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("code")) != row.end()) entity.setCode(it->second);
        if ((it = row.find("site_type")) != row.end()) {
            entity.setSiteType(SiteEntity::stringToSiteType(it->second));
        }
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        if ((it = row.find("location")) != row.end()) entity.setLocation(it->second);
        if ((it = row.find("timezone")) != row.end()) entity.setTimezone(it->second);
        
        // 주소 정보
        if ((it = row.find("address")) != row.end()) entity.setAddress(it->second);
        if ((it = row.find("city")) != row.end()) entity.setCity(it->second);
        if ((it = row.find("country")) != row.end()) entity.setCountry(it->second);
        
        it = row.find("latitude");
        if (it != row.end() && !it->second.empty()) {
            entity.setLatitude(std::stod(it->second));
        }
        
        it = row.find("longitude");
        if (it != row.end() && !it->second.empty()) {
            entity.setLongitude(std::stod(it->second));
        }
        
        // 계층 정보
        it = row.find("hierarchy_level");
        if (it != row.end() && !it->second.empty()) {
            entity.setHierarchyLevel(std::stoi(it->second));
        }
        
        if ((it = row.find("hierarchy_path")) != row.end()) entity.setHierarchyPath(it->second);
        
        // 상태 정보
        it = row.find("is_active");
        if (it != row.end()) {
            entity.setActive(db_layer.parseBoolean(it->second));
        }
        
        // 담당자 정보
        if ((it = row.find("contact_name")) != row.end()) entity.setContactName(it->second);
        if ((it = row.find("contact_email")) != row.end()) entity.setContactEmail(it->second);
        if ((it = row.find("contact_phone")) != row.end()) entity.setContactPhone(it->second);
        
        // 타임스탬프는 기본값 사용 (실제 구현에서는 파싱 필요)
        entity.setCreatedAt(std::chrono::system_clock::now());
        entity.setUpdatedAt(std::chrono::system_clock::now());
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::vector<SiteEntity> SiteRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<SiteEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("SiteRepository::mapResultToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

std::map<std::string, std::string> SiteRepository::entityToParams(const SiteEntity& entity) {
    DbLib::DatabaseAbstractionLayer db_layer;
    
    std::map<std::string, std::string> params;
    
    // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
    params["tenant_id"] = std::to_string(entity.getTenantId());
    
    if (entity.getParentSiteId() > 0) {
        params["parent_site_id"] = std::to_string(entity.getParentSiteId());
    } else {
        params["parent_site_id"] = "NULL";
    }
    
    // 사이트 정보
    params["name"] = entity.getName();
    params["code"] = entity.getCode();
    params["site_type"] = SiteEntity::siteTypeToString(entity.getSiteType());
    params["description"] = entity.getDescription();
    params["location"] = entity.getLocation();
    params["timezone"] = entity.getTimezone();
    
    // 주소 정보
    params["address"] = entity.getAddress();
    params["city"] = entity.getCity();
    params["country"] = entity.getCountry();
    params["latitude"] = std::to_string(entity.getLatitude());
    params["longitude"] = std::to_string(entity.getLongitude());
    
    // 계층 정보
    params["hierarchy_level"] = std::to_string(entity.getHierarchyLevel());
    params["hierarchy_path"] = entity.getHierarchyPath();
    
    // 상태 정보
    params["is_active"] = db_layer.formatBoolean(entity.isActive());
    
    // 담당자 정보
    params["contact_name"] = entity.getContactName();
    params["contact_email"] = entity.getContactEmail();
    params["contact_phone"] = entity.getContactPhone();
    
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool SiteRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS sites (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                parent_site_id INTEGER,
                
                -- 사이트 기본 정보
                name VARCHAR(100) NOT NULL,
                code VARCHAR(20) NOT NULL,
                site_type VARCHAR(20) NOT NULL,
                description TEXT,
                location VARCHAR(255),
                timezone VARCHAR(50) DEFAULT 'UTC',
                
                -- 주소 정보
                address TEXT,
                city VARCHAR(100),
                country VARCHAR(100),
                latitude REAL DEFAULT 0.0,
                longitude REAL DEFAULT 0.0,
                
                -- 계층 정보
                hierarchy_level INTEGER DEFAULT 1,
                hierarchy_path VARCHAR(255),
                
                -- 상태 정보
                is_active INTEGER DEFAULT 1,
                
                -- 담당자 정보
                contact_name VARCHAR(100),
                contact_email VARCHAR(255),
                contact_phone VARCHAR(50),
                
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
                FOREIGN KEY (parent_site_id) REFERENCES sites(id) ON DELETE CASCADE,
                UNIQUE(tenant_id, code)
            )
        )";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            logger_->Debug("SiteRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("SiteRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::validateSite(const SiteEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("SiteRepository::validateSite - Invalid site: " + entity.getName());
        return false;
    }
    
    if (entity.getName().empty()) {
        logger_->Warn("SiteRepository::validateSite - Site name is empty");
        return false;
    }
    
    if (entity.getCode().empty()) {
        logger_->Warn("SiteRepository::validateSite - Site code is empty");
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        logger_->Warn("SiteRepository::validateSite - Invalid tenant ID for: " + entity.getName());
        return false;
    }
    
    return true;
}


} // namespace Repositories
} // namespace Database
} // namespace PulseOne