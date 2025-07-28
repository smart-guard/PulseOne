#ifndef SITE_REPOSITORY_H
#define SITE_REPOSITORY_H

/**
 * @file SiteRepository.h
 * @brief PulseOne Site Repository - 사이트 관리 Repository (TenantRepository 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/SiteEntity.h"
#include "Utils/LogManager.h"
#include <vector>
#include <string>
#include <optional>
#include <memory>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 기존 패턴 준수 - using 선언 필수
using SiteEntity = PulseOne::Database::Entities::SiteEntity;

/**
 * @brief 사이트 Repository 클래스 (IRepository 템플릿 상속)
 */
class SiteRepository : public IRepository<SiteEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (TenantRepository 패턴)
    // =======================================================================
    
    SiteRepository();
    virtual ~SiteRepository() = default;

    // =======================================================================
    // 캐시 관리 메서드들 (TenantRepository 패턴)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // IRepository 인터페이스 구현 (TenantRepository 패턴)
    // =======================================================================
    
    std::vector<SiteEntity> findAll() override;
    std::optional<SiteEntity> findById(int id) override;
    bool save(SiteEntity& entity) override;
    bool update(const SiteEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<SiteEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<SiteEntity>& entities) override;
    int updateBulk(const std::vector<SiteEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    std::vector<SiteEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;
    std::string getRepositoryName() const override { return "SiteRepository"; }

    // =======================================================================
    // 사이트 전용 조회 메서드들 (TenantRepository 패턴)
    // =======================================================================
    
    std::vector<SiteEntity> findByTenant(int tenant_id);
    std::vector<SiteEntity> findByParentSite(int parent_site_id);
    std::vector<SiteEntity> findBySiteType(SiteEntity::SiteType site_type);
    std::optional<SiteEntity> findByName(const std::string& name, int tenant_id);
    std::optional<SiteEntity> findByCode(const std::string& code, int tenant_id);
    std::vector<SiteEntity> findByLocation(const std::string& location);
    std::vector<SiteEntity> findByTimezone(const std::string& timezone);
    std::vector<SiteEntity> findActiveSites(int tenant_id = 0);
    std::vector<SiteEntity> findRootSites(int tenant_id);
    std::vector<SiteEntity> findByHierarchyLevel(int level, int tenant_id = 0);
    std::vector<SiteEntity> findByNamePattern(const std::string& name_pattern, int tenant_id = 0);
    std::vector<SiteEntity> findSitesWithGPS(int tenant_id = 0);

    // =======================================================================
    // 사이트 비즈니스 로직 메서드들 (TenantRepository 패턴)
    // =======================================================================
    
    bool isSiteNameTaken(const std::string& name, int tenant_id, int exclude_id = 0);
    bool isSiteCodeTaken(const std::string& code, int tenant_id, int exclude_id = 0);
    bool hasChildSites(int parent_site_id);
    json getSiteHierarchy(int tenant_id);
    std::vector<SiteEntity> getAllChildSites(int parent_site_id);
    json getSiteStatistics(int tenant_id);

private:
    // =======================================================================
    // private 헬퍼 메서드들 (TenantRepository 패턴)
    // =======================================================================
    
    bool validateSite(const SiteEntity& site) const;
    QueryCondition buildSiteTypeCondition(SiteEntity::SiteType site_type) const;
    QueryCondition buildTenantCondition(int tenant_id) const;
    QueryCondition buildActiveCondition(bool active = true) const;
    json buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // SITE_REPOSITORY_H