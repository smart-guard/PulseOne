// =============================================================================
// collector/src/Database/Repositories/SiteRepository.cpp
// PulseOne SiteRepository 구현 - TenantRepository 패턴 100% 준수
// =============================================================================

/**
 * @file SiteRepository.cpp
 * @brief PulseOne SiteRepository 구현 - TenantRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/SiteRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 기존 패턴 준수 - using 선언 필수 (cpp에도 필요)
using SiteEntity = PulseOne::Database::Entities::SiteEntity;

// =======================================================================
// 생성자 및 초기화 (TenantRepository 패턴)
// =======================================================================

SiteRepository::SiteRepository() 
    : IRepository<SiteEntity>("SiteRepository") {
    logger_->Info("🏭 SiteRepository initialized with IRepository caching system");
    logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
}

// =======================================================================
// 캐시 관리 메서드들 (TenantRepository 패턴)
// =======================================================================

void SiteRepository::setCacheEnabled(bool enabled) {
    IRepository<SiteEntity>::setCacheEnabled(enabled);
    logger_->Info("SiteRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool SiteRepository::isCacheEnabled() const {
    return IRepository<SiteEntity>::isCacheEnabled();
}

void SiteRepository::clearCache() {
    IRepository<SiteEntity>::clearCache();
    logger_->Info("SiteRepository cache cleared");
}

void SiteRepository::clearCacheForId(int id) {
    IRepository<SiteEntity>::clearCacheForId(id);
    logger_->Debug("SiteRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> SiteRepository::getCacheStats() const {
    return IRepository<SiteEntity>::getCacheStats();
}

// =======================================================================
// IRepository 인터페이스 구현 (TenantRepository 패턴)
// =======================================================================

std::vector<SiteEntity> SiteRepository::findAll() {
    logger_->Debug("🔍 SiteRepository::findAll() - Fetching all sites");
    
    return findByConditions({}, OrderBy("name", "ASC"));
}

std::optional<SiteEntity> SiteRepository::findById(int id) {
    logger_->Debug("🔍 SiteRepository::findById(" + std::to_string(id) + ")");
    
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("✅ Cache HIT for site ID: " + std::to_string(id));
        return cached;
    }
    
    auto sites = findByConditions({QueryCondition("id", "=", std::to_string(id))});
    if (!sites.empty()) {
        logger_->Debug("✅ Site found: " + sites[0].getName());
        return sites[0];
    }
    
    logger_->Debug("❌ Site not found: " + std::to_string(id));
    return std::nullopt;
}

bool SiteRepository::save(SiteEntity& entity) {
    logger_->Debug("💾 SiteRepository::save() - " + entity.getName());
    
    if (!validateSite(entity)) {
        logger_->Error("❌ SiteRepository::save - Invalid site data");
        return false;
    }
    
    if (isSiteNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ SiteRepository::save - Site name already taken: " + entity.getName());
        return false;
    }
    
    if (isSiteCodeTaken(entity.getCode(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ SiteRepository::save - Site code already taken: " + entity.getCode());
        return false;
    }
    
    try {
        bool success = entity.saveToDatabase();
        
        if (success) {
            cacheEntity(entity);
            logger_->Info("✅ SiteRepository::save - Saved and cached site: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::update(const SiteEntity& entity) {
    logger_->Debug("🔄 SiteRepository::update() - " + entity.getName());
    
    if (!validateSite(entity)) {
        logger_->Error("❌ SiteRepository::update - Invalid site data");
        return false;
    }
    
    if (isSiteNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ SiteRepository::update - Site name already taken: " + entity.getName());
        return false;
    }
    
    if (isSiteCodeTaken(entity.getCode(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ SiteRepository::update - Site code already taken: " + entity.getCode());
        return false;
    }
    
    try {
        SiteEntity& mutable_entity = const_cast<SiteEntity&>(entity);
        bool success = mutable_entity.updateToDatabase();
        
        if (success) {
            clearCacheForId(entity.getId());
            logger_->Info("✅ SiteRepository::update - Updated site and cleared cache: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::deleteById(int id) {
    logger_->Debug("🗑️ SiteRepository::deleteById(" + std::to_string(id) + ")");
    
    if (hasChildSites(id)) {
        logger_->Error("❌ SiteRepository::deleteById - Cannot delete site with child sites: " + std::to_string(id));
        return false;
    }
    
    try {
        SiteEntity entity(id);
        bool success = entity.deleteFromDatabase();
        
        if (success) {
            clearCacheForId(id);
            logger_->Info("✅ SiteRepository::deleteById - Deleted site and cleared cache: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool SiteRepository::exists(int id) {
    logger_->Debug("🔍 SiteRepository::exists(" + std::to_string(id) + ")");
    
    return findById(id).has_value();
}

std::vector<SiteEntity> SiteRepository::findByIds(const std::vector<int>& ids) {
    logger_->Debug("🔍 SiteRepository::findByIds() - " + std::to_string(ids.size()) + " sites");
    
    return IRepository<SiteEntity>::findByIds(ids);
}

int SiteRepository::saveBulk(std::vector<SiteEntity>& entities) {
    logger_->Info("💾 SiteRepository::saveBulk() - " + std::to_string(entities.size()) + " sites");
    
    int valid_count = 0;
    for (const auto& entity : entities) {
        if (validateSite(entity)) {
            valid_count++;
        }
    }
    
    if (valid_count != entities.size()) {
        logger_->Warn("⚠️ SiteRepository::saveBulk - Valid: " + 
                      std::to_string(valid_count) + "/" + std::to_string(entities.size()));
    }
    
    return IRepository<SiteEntity>::saveBulk(entities);
}

int SiteRepository::updateBulk(const std::vector<SiteEntity>& entities) {
    logger_->Info("🔄 SiteRepository::updateBulk() - " + std::to_string(entities.size()) + " sites");
    
    return IRepository<SiteEntity>::updateBulk(entities);
}

int SiteRepository::deleteByIds(const std::vector<int>& ids) {
    logger_->Info("🗑️ SiteRepository::deleteByIds() - " + std::to_string(ids.size()) + " sites");
    
    return IRepository<SiteEntity>::deleteByIds(ids);
}

std::vector<SiteEntity> SiteRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    return IRepository<SiteEntity>::findByConditions(conditions, order_by, pagination);
}

int SiteRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    return IRepository<SiteEntity>::countByConditions(conditions);
}

int SiteRepository::getTotalCount() {
    return countByConditions({});
}

// =======================================================================
// 사이트 전용 조회 메서드들 (TenantRepository 패턴)
// =======================================================================

std::vector<SiteEntity> SiteRepository::findByTenant(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findByTenant(" + std::to_string(tenant_id) + ")");
    
    return findByConditions({buildTenantCondition(tenant_id)}, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByParentSite(int parent_site_id) {
    logger_->Debug("🔍 SiteRepository::findByParentSite(" + std::to_string(parent_site_id) + ")");
    
    return findByConditions({QueryCondition("parent_site_id", "=", std::to_string(parent_site_id))}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findBySiteType(SiteEntity::SiteType site_type) {
    logger_->Debug("🔍 SiteRepository::findBySiteType(" + SiteEntity::siteTypeToString(site_type) + ")");
    
    return findByConditions({buildSiteTypeCondition(site_type)}, OrderBy("name", "ASC"));
}

std::optional<SiteEntity> SiteRepository::findByName(const std::string& name, int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findByName(" + name + ", " + std::to_string(tenant_id) + ")");
    
    auto sites = findByConditions({
        QueryCondition("name", "=", name),
        buildTenantCondition(tenant_id)
    });
    
    return sites.empty() ? std::nullopt : std::make_optional(sites[0]);
}

std::optional<SiteEntity> SiteRepository::findByCode(const std::string& code, int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findByCode(" + code + ", " + std::to_string(tenant_id) + ")");
    
    auto sites = findByConditions({
        QueryCondition("code", "=", code),
        buildTenantCondition(tenant_id)
    });
    
    return sites.empty() ? std::nullopt : std::make_optional(sites[0]);
}

std::vector<SiteEntity> SiteRepository::findByLocation(const std::string& location) {
    logger_->Debug("🔍 SiteRepository::findByLocation(" + location + ")");
    
    return findByConditions({QueryCondition("location", "LIKE", "%" + location + "%")}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByTimezone(const std::string& timezone) {
    logger_->Debug("🔍 SiteRepository::findByTimezone(" + timezone + ")");
    
    return findByConditions({QueryCondition("timezone", "=", timezone)}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findActiveSites(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findActiveSites(" + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {buildActiveCondition(true)};
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findRootSites(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findRootSites(" + std::to_string(tenant_id) + ")");
    
    return findByConditions({
        buildTenantCondition(tenant_id),
        QueryCondition("parent_site_id", "IS", "NULL")
    }, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByHierarchyLevel(int level, int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findByHierarchyLevel(" + std::to_string(level) + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("hierarchy_level", "=", std::to_string(level))
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByNamePattern(const std::string& name_pattern, int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findByNamePattern(" + name_pattern + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "LIKE", "%" + name_pattern + "%")
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findSitesWithGPS(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::findSitesWithGPS(" + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("latitude", "!=", "0"),
        QueryCondition("longitude", "!=", "0")
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

// =======================================================================
// 사이트 비즈니스 로직 메서드들 (TenantRepository 패턴)
// =======================================================================

bool SiteRepository::isSiteNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    logger_->Debug("🔍 SiteRepository::isSiteNameTaken(" + name + ", " + std::to_string(tenant_id) + ", " + std::to_string(exclude_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "=", name),
        buildTenantCondition(tenant_id)
    };
    
    if (exclude_id > 0) {
        conditions.push_back(QueryCondition("id", "!=", std::to_string(exclude_id)));
    }
    
    int count = countByConditions(conditions);
    return count > 0;
}

bool SiteRepository::isSiteCodeTaken(const std::string& code, int tenant_id, int exclude_id) {
    logger_->Debug("🔍 SiteRepository::isSiteCodeTaken(" + code + ", " + std::to_string(tenant_id) + ", " + std::to_string(exclude_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("code", "=", code),
        buildTenantCondition(tenant_id)
    };
    
    if (exclude_id > 0) {
        conditions.push_back(QueryCondition("id", "!=", std::to_string(exclude_id)));
    }
    
    int count = countByConditions(conditions);
    return count > 0;
}

bool SiteRepository::hasChildSites(int parent_site_id) {
    logger_->Debug("🔍 SiteRepository::hasChildSites(" + std::to_string(parent_site_id) + ")");
    
    int count = countByConditions({
        QueryCondition("parent_site_id", "=", std::to_string(parent_site_id))
    });
    
    return count > 0;
}

json SiteRepository::getSiteHierarchy(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::getSiteHierarchy(" + std::to_string(tenant_id) + ")");
    
    auto all_sites = findByTenant(tenant_id);
    
    json hierarchy;
    hierarchy["tenant_id"] = tenant_id;
    hierarchy["total_sites"] = all_sites.size();
    hierarchy["hierarchy"] = buildHierarchyRecursive(all_sites, 0);
    
    return hierarchy;
}

std::vector<SiteEntity> SiteRepository::getAllChildSites(int parent_site_id) {
    logger_->Debug("🔍 SiteRepository::getAllChildSites(" + std::to_string(parent_site_id) + ")");
    
    std::vector<SiteEntity> all_children;
    
    auto direct_children = findByParentSite(parent_site_id);
    
    for (const auto& child : direct_children) {
        all_children.push_back(child);
        
        auto sub_children = getAllChildSites(child.getId());
        all_children.insert(all_children.end(), sub_children.begin(), sub_children.end());
    }
    
    return all_children;
}

json SiteRepository::getSiteStatistics(int tenant_id) {
    logger_->Debug("🔍 SiteRepository::getSiteStatistics(" + std::to_string(tenant_id) + ")");
    
    json stats;
    stats["tenant_id"] = tenant_id;
    
    stats["total_sites"] = countByConditions({buildTenantCondition(tenant_id)});
    
    stats["active_sites"] = countByConditions({
        buildTenantCondition(tenant_id),
        buildActiveCondition(true)
    });
    
    json type_stats;
    for (int type = 0; type <= 7; type++) {
        auto site_type = static_cast<SiteEntity::SiteType>(type);
        std::string type_name = SiteEntity::siteTypeToString(site_type);
        
        int count = countByConditions({
            buildTenantCondition(tenant_id),
            buildSiteTypeCondition(site_type)
        });
        
        type_stats[type_name] = count;
    }
    stats["by_type"] = type_stats;
    
    json level_stats;
    for (int level = 1; level <= 5; level++) {
        int count = countByConditions({
            buildTenantCondition(tenant_id),
            QueryCondition("hierarchy_level", "=", std::to_string(level))
        });
        
        if (count > 0) {
            level_stats["level_" + std::to_string(level)] = count;
        }
    }
    stats["by_hierarchy_level"] = level_stats;
    
    stats["sites_with_gps"] = countByConditions({
        buildTenantCondition(tenant_id),
        QueryCondition("latitude", "!=", "0"),
        QueryCondition("longitude", "!=", "0")
    });
    
    return stats;
}

// =======================================================================
// private 헬퍼 메서드들 (TenantRepository 패턴)
// =======================================================================

bool SiteRepository::validateSite(const SiteEntity& site) const {
    if (!site.isValid()) {
        return false;
    }
    
    if (site.getTenantId() <= 0) {
        return false;
    }
    
    if (site.getName().empty() || site.getName().length() > 100) {
        return false;
    }
    
    if (site.getCode().empty() || site.getCode().length() > 20) {
        return false;
    }
    
    if (site.hasGpsCoordinates()) {
        if (site.getLatitude() < -90.0 || site.getLatitude() > 90.0) {
            return false;
        }
        if (site.getLongitude() < -180.0 || site.getLongitude() > 180.0) {
            return false;
        }
    }
    
    return true;
}

QueryCondition SiteRepository::buildSiteTypeCondition(SiteEntity::SiteType site_type) const {
    return QueryCondition("site_type", "=", SiteEntity::siteTypeToString(site_type));
}

QueryCondition SiteRepository::buildTenantCondition(int tenant_id) const {
    return QueryCondition("tenant_id", "=", std::to_string(tenant_id));
}

QueryCondition SiteRepository::buildActiveCondition(bool active) const {
    return QueryCondition("is_active", "=", active ? "1" : "0");
}

json SiteRepository::buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const {
    json children = json::array();
    
    for (const auto& site : sites) {
        if (site.getParentSiteId() == parent_id) {
            json node;
            node["id"] = site.getId();
            node["name"] = site.getName();
            node["code"] = site.getCode();
            node["type"] = SiteEntity::siteTypeToString(site.getSiteType());
            node["level"] = site.getHierarchyLevel();
            node["active"] = site.isActive();
            
            node["children"] = buildHierarchyRecursive(sites, site.getId());
            
            children.push_back(node);
        }
    }
    
    return children;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne