#ifndef SITE_REPOSITORY_H
#define SITE_REPOSITORY_H

/**
 * @file SiteRepository.h
 * @brief PulseOne Site Repository - 사이트 관리 Repository (생성자 문제 해결)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 수정 사항:
 * - 생성자에서 initializeDependencies() 호출
 * - override → final 변경
 * - 네임스페이스 수정
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

// 🔥 네임스페이스 수정 - 중복 제거
using SiteEntity = Entities::SiteEntity;

/**
 * @brief 사이트 Repository 클래스 (IRepository 템플릿 상속)
 */
class SiteRepository : public IRepository<SiteEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (수정됨)
    // =======================================================================
    
    SiteRepository() : IRepository<SiteEntity>("SiteRepository") {
        // 🔥 의존성 초기화를 여기서 호출
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 SiteRepository initialized with IRepository caching system");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~SiteRepository() = default;

    // =======================================================================
    // 캐시 관리 메서드들 (override → final)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) final {
        IRepository<SiteEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("SiteRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const final {
        return IRepository<SiteEntity>::isCacheEnabled();
    }
    
    void clearCache() final {
        IRepository<SiteEntity>::clearCache();
    }
    
    void clearCacheForId(int id) final {
        IRepository<SiteEntity>::clearCacheForId(id);
    }
    
    std::map<std::string, int> getCacheStats() const final {
        return IRepository<SiteEntity>::getCacheStats();
    }

    // =======================================================================
    // IRepository 인터페이스 구현 (override → final)
    // =======================================================================
    
    std::vector<SiteEntity> findAll() final;
    std::optional<SiteEntity> findById(int id) final;
    bool save(SiteEntity& entity) final;
    bool update(const SiteEntity& entity) final;
    bool deleteById(int id) final;
    bool exists(int id) final;
    std::vector<SiteEntity> findByIds(const std::vector<int>& ids) final;
    int saveBulk(std::vector<SiteEntity>& entities) final;
    int updateBulk(const std::vector<SiteEntity>& entities) final;
    int deleteByIds(const std::vector<int>& ids) final;
    
    std::vector<SiteEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) final;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) final;
    int getTotalCount() final;
    
    std::string getRepositoryName() const final { 
        return "SiteRepository"; 
    }

    // =======================================================================
    // 사이트 전용 조회 메서드들
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
    // 사이트 비즈니스 로직 메서드들
    // =======================================================================
    
    bool isSiteNameTaken(const std::string& name, int tenant_id, int exclude_id = 0);
    bool isSiteCodeTaken(const std::string& code, int tenant_id, int exclude_id = 0);
    bool hasChildSites(int parent_site_id);
    
    // 🔥 JSON 관련 메서드들 (조건부 컴파일)
#ifdef HAVE_NLOHMANN_JSON
    nlohmann::json getSiteHierarchy(int tenant_id);
    nlohmann::json getSiteStatistics(int tenant_id);
#else
    std::string getSiteHierarchy(int tenant_id);      // JSON 문자열 반환
    std::string getSiteStatistics(int tenant_id);    // JSON 문자열 반환
#endif
    
    std::vector<SiteEntity> getAllChildSites(int parent_site_id);

private:
    // =======================================================================
    // private 헬퍼 메서드들
    // =======================================================================
    
    bool validateSite(const SiteEntity& site) const;
    QueryCondition buildSiteTypeCondition(SiteEntity::SiteType site_type) const;
    QueryCondition buildTenantCondition(int tenant_id) const;
    QueryCondition buildActiveCondition(bool active = true) const;
    
    // 🔥 JSON 관련 헬퍼 메서드들 (조건부 컴파일)
#ifdef HAVE_NLOHMANN_JSON
    nlohmann::json buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const;
#else
    std::string buildHierarchyRecursive(const std::vector<SiteEntity>& sites, int parent_id) const;
#endif

    // =======================================================================
    // 데이터베이스 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    SiteEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 행을 엔티티 벡터로 변환
     * @param result 쿼리 결과
     * @return 엔티티 목록
     */
    std::vector<SiteEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief 통합 데이터베이스 쿼리 실행 (SELECT)
     * @param sql SQL 쿼리
     * @return 결과 맵의 벡터
     */
    std::vector<std::map<std::string, std::string>> executeDatabaseQuery(const std::string& sql);
    
    /**
     * @brief 통합 데이터베이스 비쿼리 실행 (INSERT/UPDATE/DELETE)
     * @param sql SQL 쿼리
     * @return 성공 시 true
     */
    bool executeDatabaseNonQuery(const std::string& sql);
    
    /**
     * @brief 현재 타임스탬프를 문자열로 반환
     * @return ISO 형식 타임스탬프
     */
    std::string getCurrentTimestamp();
    
    /**
     * @brief SQL 문자열 이스케이프
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str);

    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;

};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // SITE_REPOSITORY_H