// =============================================================================
// collector/src/Database/Repositories/SiteRepository.cpp
// PulseOne SiteRepository 구현 - 생성자 문제 해결
// =============================================================================

/**
 * @file SiteRepository.cpp
 * @brief PulseOne SiteRepository 구현 - 생성자 문제 해결
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 수정 사항:
 * - 생성자 구현 제거 (헤더에서 인라인 처리)
 * - 네임스페이스 수정
 * - 캐시 메서드들 간소화
 */

#include "Database/Repositories/SiteRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 네임스페이스 수정 - 중복 제거
using SiteEntity = Entities::SiteEntity;

// 🔥 생성자는 헤더에서 인라인으로 처리하므로 여기서는 제거

// =======================================================================
// 캐시 관리 메서드들 (헤더에서 인라인 처리하므로 구현 제거)
// =======================================================================

// setCacheEnabled, isCacheEnabled, clearCache 등은 
// 헤더에서 IRepository를 호출하도록 인라인 처리했으므로 구현 불필요

// =======================================================================
// IRepository 기본 CRUD 구현 (기존 로직 유지, 캐시 호출 수정)
// =======================================================================

std::vector<SiteEntity> SiteRepository::findAll() {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findAll() - Fetching all sites");
    }
    
    try {
        return findByConditions({}, OrderBy("name", "ASC"));
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<SiteEntity> SiteRepository::findById(int id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findById(" + std::to_string(id) + ")");
    }
    
    if (id <= 0) {
        if (logger_) {
            logger_->Warn("SiteRepository::findById - Invalid ID: " + std::to_string(id));
        }
        return std::nullopt;
    }
    
    // 🔥 IRepository의 캐시 자동 확인
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        if (logger_) {
            logger_->Debug("✅ Cache HIT for site ID: " + std::to_string(id));
        }
        return cached;
    }
    
    try {
        auto sites = findByConditions({QueryCondition("id", "=", std::to_string(id))});
        if (!sites.empty()) {
            // 🔥 캐시에 저장
            cacheEntity(sites[0]);
            if (logger_) {
                logger_->Debug("✅ Site found: " + sites[0].getName());
            }
            return sites[0];
        }
        
        if (logger_) {
            logger_->Debug("❌ Site not found: " + std::to_string(id));
        }
        return std::nullopt;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool SiteRepository::save(SiteEntity& entity) {
    if (logger_) {
        logger_->Debug("💾 SiteRepository::save() - " + entity.getName());
    }
    
    if (!validateSite(entity)) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::save - Invalid site data");
        }
        return false;
    }
    
    if (isSiteNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::save - Site name already taken: " + entity.getName());
        }
        return false;
    }
    
    if (isSiteCodeTaken(entity.getCode(), entity.getTenantId(), entity.getId())) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::save - Site code already taken: " + entity.getCode());
        }
        return false;
    }
    
    try {
        bool success = entity.saveToDatabase();
        
        if (success) {
            cacheEntity(entity);
            if (logger_) {
                logger_->Info("✅ SiteRepository::save - Saved and cached site: " + entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool SiteRepository::update(const SiteEntity& entity) {
    if (logger_) {
        logger_->Debug("🔄 SiteRepository::update() - " + entity.getName());
    }
    
    if (!validateSite(entity)) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::update - Invalid site data");
        }
        return false;
    }
    
    if (isSiteNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::update - Site name already taken: " + entity.getName());
        }
        return false;
    }
    
    if (isSiteCodeTaken(entity.getCode(), entity.getTenantId(), entity.getId())) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::update - Site code already taken: " + entity.getCode());
        }
        return false;
    }
    
    try {
        SiteEntity& mutable_entity = const_cast<SiteEntity&>(entity);
        bool success = mutable_entity.updateToDatabase();
        
        if (success) {
            clearCacheForId(entity.getId());
            if (logger_) {
                logger_->Info("✅ SiteRepository::update - Updated site and cleared cache: " + entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool SiteRepository::deleteById(int id) {
    if (logger_) {
        logger_->Debug("🗑️ SiteRepository::deleteById(" + std::to_string(id) + ")");
    }
    
    if (hasChildSites(id)) {
        if (logger_) {
            logger_->Error("❌ SiteRepository::deleteById - Cannot delete site with child sites: " + std::to_string(id));
        }
        return false;
    }
    
    try {
        SiteEntity entity(id);
        bool success = entity.deleteFromDatabase();
        
        if (success) {
            clearCacheForId(id);
            if (logger_) {
                logger_->Info("✅ SiteRepository::deleteById - Deleted site and cleared cache: " + std::to_string(id));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool SiteRepository::exists(int id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::exists(" + std::to_string(id) + ")");
    }
    
    return findById(id).has_value();
}

std::vector<SiteEntity> SiteRepository::findByIds(const std::vector<int>& ids) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByIds() - " + std::to_string(ids.size()) + " sites");
    }
    
    return IRepository<SiteEntity>::findByIds(ids);
}

int SiteRepository::saveBulk(std::vector<SiteEntity>& entities) {
    if (logger_) {
        logger_->Info("💾 SiteRepository::saveBulk() - " + std::to_string(entities.size()) + " sites");
    }
    
    int valid_count = 0;
    for (const auto& entity : entities) {
        if (validateSite(entity)) {
            valid_count++;
        }
    }
    
    if (valid_count != static_cast<int>(entities.size())) {
        if (logger_) {
            logger_->Warn("⚠️ SiteRepository::saveBulk - Valid: " + 
                          std::to_string(valid_count) + "/" + std::to_string(entities.size()));
        }
    }
    
    return IRepository<SiteEntity>::saveBulk(entities);
}

int SiteRepository::updateBulk(const std::vector<SiteEntity>& entities) {
    if (logger_) {
        logger_->Info("🔄 SiteRepository::updateBulk() - " + std::to_string(entities.size()) + " sites");
    }
    
    return IRepository<SiteEntity>::updateBulk(entities);
}

int SiteRepository::deleteByIds(const std::vector<int>& ids) {
    if (logger_) {
        logger_->Info("🗑️ SiteRepository::deleteByIds() - " + std::to_string(ids.size()) + " sites");
    }
    
    return IRepository<SiteEntity>::deleteByIds(ids);
}

std::vector<SiteEntity> SiteRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        // 🔥 실제 데이터베이스 쿼리 구현 (기존 로직)
        std::string sql = "SELECT * FROM sites";
        
        // WHERE 절 추가
        if (!conditions.empty()) {
            sql += buildWhereClause(conditions);
        }
        
        // ORDER BY 절 추가
        if (order_by.has_value()) {
            sql += buildOrderByClause(order_by);
        }
        
        // LIMIT 절 추가
        if (pagination.has_value()) {
            sql += buildLimitClause(pagination);
        }
        
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int SiteRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM sites";
        
        if (!conditions.empty()) {
            sql += buildWhereClause(conditions);
        }
        
        auto result = executeDatabaseQuery(sql);
        if (!result.empty() && result[0].find("count") != result[0].end()) {
            return std::stoi(result[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteRepository::countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

int SiteRepository::getTotalCount() {
    return countByConditions({});
}

// =======================================================================
// 사이트 전용 조회 메서드들 (기존 로직 유지)
// =======================================================================

std::vector<SiteEntity> SiteRepository::findByTenant(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByTenant(" + std::to_string(tenant_id) + ")");
    }
    
    return findByConditions({buildTenantCondition(tenant_id)}, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByParentSite(int parent_site_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByParentSite(" + std::to_string(parent_site_id) + ")");
    }
    
    return findByConditions({QueryCondition("parent_site_id", "=", std::to_string(parent_site_id))}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findBySiteType(SiteEntity::SiteType site_type) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findBySiteType(" + SiteEntity::siteTypeToString(site_type) + ")");
    }
    
    return findByConditions({buildSiteTypeCondition(site_type)}, OrderBy("name", "ASC"));
}

std::optional<SiteEntity> SiteRepository::findByName(const std::string& name, int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByName(" + name + ", " + std::to_string(tenant_id) + ")");
    }
    
    auto sites = findByConditions({
        QueryCondition("name", "=", name),
        buildTenantCondition(tenant_id)
    });
    
    return sites.empty() ? std::nullopt : std::make_optional(sites[0]);
}

std::optional<SiteEntity> SiteRepository::findByCode(const std::string& code, int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByCode(" + code + ", " + std::to_string(tenant_id) + ")");
    }
    
    auto sites = findByConditions({
        QueryCondition("code", "=", code),
        buildTenantCondition(tenant_id)
    });
    
    return sites.empty() ? std::nullopt : std::make_optional(sites[0]);
}

std::vector<SiteEntity> SiteRepository::findByLocation(const std::string& location) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByLocation(" + location + ")");
    }
    
    return findByConditions({QueryCondition("location", "LIKE", "%" + location + "%")}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByTimezone(const std::string& timezone) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByTimezone(" + timezone + ")");
    }
    
    return findByConditions({QueryCondition("timezone", "=", timezone)}, 
                           OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findActiveSites(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findActiveSites(" + std::to_string(tenant_id) + ")");
    }
    
    std::vector<QueryCondition> conditions = {buildActiveCondition(true)};
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findRootSites(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findRootSites(" + std::to_string(tenant_id) + ")");
    }
    
    return findByConditions({
        buildTenantCondition(tenant_id),
        QueryCondition("parent_site_id", "IS", "NULL")
    }, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByHierarchyLevel(int level, int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByHierarchyLevel(" + std::to_string(level) + ", " + std::to_string(tenant_id) + ")");
    }
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("hierarchy_level", "=", std::to_string(level))
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findByNamePattern(const std::string& name_pattern, int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findByNamePattern(" + name_pattern + ", " + std::to_string(tenant_id) + ")");
    }
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "LIKE", "%" + name_pattern + "%")
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<SiteEntity> SiteRepository::findSitesWithGPS(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::findSitesWithGPS(" + std::to_string(tenant_id) + ")");
    }
    
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
// 사이트 비즈니스 로직 메서드들
// =======================================================================

bool SiteRepository::isSiteNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::isSiteNameTaken(" + name + ", " + std::to_string(tenant_id) + ", " + std::to_string(exclude_id) + ")");
    }
    
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
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::isSiteCodeTaken(" + code + ", " + std::to_string(tenant_id) + ", " + std::to_string(exclude_id) + ")");
    }
    
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
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::hasChildSites(" + std::to_string(parent_site_id) + ")");
    }
    
    int count = countByConditions({
        QueryCondition("parent_site_id", "=", std::to_string(parent_site_id))
    });
    
    return count > 0;
}

// 🔥 JSON 메서드들 - 조건부 컴파일
#ifdef HAVE_NLOHMANN_JSON
nlohmann::json SiteRepository::getSiteHierarchy(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::getSiteHierarchy(" + std::to_string(tenant_id) + ")");
    }
    
    auto all_sites = findByTenant(tenant_id);
    
    nlohmann::json hierarchy;
    hierarchy["tenant_id"] = tenant_id;
    hierarchy["total_sites"] = all_sites.size();
    hierarchy["hierarchy"] = buildHierarchyRecursive(all_sites, 0);
    
    return hierarchy;
}

nlohmann::json SiteRepository::getSiteStatistics(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::getSiteStatistics(" + std::to_string(tenant_id) + ")");
    }
    
    nlohmann::json stats;
    stats["tenant_id"] = tenant_id;
    
    stats["total_sites"] = countByConditions({buildTenantCondition(tenant_id)});
    
    stats["active_sites"] = countByConditions({
        buildTenantCondition(tenant_id),
        buildActiveCondition(true)
    });
    
    // 타입별 통계
    nlohmann::json type_stats;
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
    
    return stats;
}

nlohmann::json SiteRepository::buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const {
    nlohmann::json children = nlohmann::json::array();
    
    for (const auto& site : sites) {
        if (site.getParentSiteId() == parent_id) {
            nlohmann::json node;
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

#else
// JSON 라이브러리가 없는 경우 문자열 반환
std::string SiteRepository::getSiteHierarchy(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::getSiteHierarchy(" + std::to_string(tenant_id) + ") - JSON not available");
    }
    
    auto all_sites = findByTenant(tenant_id);
    return "{\"tenant_id\":" + std::to_string(tenant_id) + 
           ",\"total_sites\":" + std::to_string(all_sites.size()) + "}";
}

std::string SiteRepository::getSiteStatistics(int tenant_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::getSiteStatistics(" + std::to_string(tenant_id) + ") - JSON not available");
    }
    
    int total = countByConditions({buildTenantCondition(tenant_id)});
    int active = countByConditions({buildTenantCondition(tenant_id), buildActiveCondition(true)});
    
    return "{\"tenant_id\":" + std::to_string(tenant_id) + 
           ",\"total_sites\":" + std::to_string(total) + 
           ",\"active_sites\":" + std::to_string(active) + "}";
}

std::string SiteRepository::buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const {
    return "[]";  // 단순 문자열 반환
}
#endif

std::vector<SiteEntity> SiteRepository::getAllChildSites(int parent_site_id) {
    if (logger_) {
        logger_->Debug("🔍 SiteRepository::getAllChildSites(" + std::to_string(parent_site_id) + ")");
    }
    
    std::vector<SiteEntity> all_children;
    
    auto direct_children = findByParentSite(parent_site_id);
    
    for (const auto& child : direct_children) {
        all_children.push_back(child);
        
        auto sub_children = getAllChildSites(child.getId());
        all_children.insert(all_children.end(), sub_children.begin(), sub_children.end());
    }
    
    return all_children;
}

// =======================================================================
// private 헬퍼 메서드들
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

// =======================================================================
// 데이터베이스 헬퍼 메서드들 (임시 구현)
// =======================================================================

SiteEntity SiteRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    // 임시 구현 - 실제로는 row 데이터를 SiteEntity로 변환
    SiteEntity entity;
    // TODO: 실제 매핑 로직 구현
    return entity;
}

std::vector<SiteEntity> SiteRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<SiteEntity> entities;
    for (const auto& row : result) {
        entities.push_back(mapRowToEntity(row));
    }
    return entities;
}

std::vector<std::map<std::string, std::string>> SiteRepository::executeDatabaseQuery(const std::string& sql) {
    try {
        if (!db_manager_) return {};
        
        // 데이터베이스 타입에 따른 쿼리 실행
        if (config_manager_) {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
            if (db_type == "POSTGRESQL") {
                auto result = db_manager_->executeQueryPostgres(sql);
                
                // PostgreSQL 결과를 벡터<맵>으로 변환
                std::vector<std::map<std::string, std::string>> rows;
                for (const auto& row : result) {
                    std::map<std::string, std::string> row_map;
                    for (size_t i = 0; i < row.size(); ++i) {
                        std::string column_name = result.column_name(i);
                        std::string value = row[static_cast<int>(i)].is_null() ? "" : row[static_cast<int>(i)].c_str();
                        row_map[column_name] = value;
                    }
                    rows.push_back(row_map);
                }
                return rows;
                
            } else {
                // SQLite는 콜백 함수가 필요하므로 임시로 빈 결과 반환
                logger_->Warn("SQLite query execution not implemented in executeDatabaseQuery");
                return {};
                
                // 실제 SQLite 구현이 필요하다면:
                /*
                std::vector<std::map<std::string, std::string>> rows;
                auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
                    auto* results = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
                    std::map<std::string, std::string> row;
                    for (int i = 0; i < argc; i++) {
                        row[azColName[i]] = argv[i] ? argv[i] : "";
                    }
                    results->push_back(row);
                    return 0;
                };
                db_manager_->executeQuerySQLite(sql, callback, &rows);
                return rows;
                */
            }
        }
        return {};
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("executeDatabaseQuery failed: " + std::string(e.what()));
        }
        return {};
    }
}

bool SiteRepository::executeDatabaseNonQuery(const std::string& sql) {
    try {
        if (!db_manager_) return false;
        
        if (config_manager_) {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
            if (db_type == "POSTGRESQL") {
                return db_manager_->executeNonQueryPostgres(sql);
            } else {
                return db_manager_->executeNonQuerySQLite(sql);
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("executeDatabaseNonQuery failed: " + std::string(e.what()));
        }
        return false;
    }
}

std::string SiteRepository::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string SiteRepository::escapeString(const std::string& str) {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

std::string SiteRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) {
        return "";
    }
    
    std::string where_clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) {
            where_clause += " AND ";
        }
        where_clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return where_clause;
}

/**
 * @brief ORDER BY 절 생성
 */
std::string SiteRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) {
        return "";
    }
    
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

/**
 * @brief LIMIT 절 생성
 */
std::string SiteRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) {
        return "";
    }
    
    std::string limit_clause = " LIMIT " + std::to_string(pagination->limit);
    if (pagination->offset > 0) {
        limit_clause += " OFFSET " + std::to_string(pagination->offset);
    }
    return limit_clause;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne