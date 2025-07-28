#ifndef TENANT_REPOSITORY_H
#define TENANT_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/TenantEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

using TenantEntity = PulseOne::Database::Entities::TenantEntity;

class TenantRepository : public IRepository<TenantEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    TenantRepository();
    virtual ~TenantRepository() = default;
    
    // =======================================================================
    // 🔥 캐시 관리 메서드들 (한 곳에만 선언)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<TenantEntity> findAll() override;
    std::optional<TenantEntity> findById(int id) override;
    bool save(TenantEntity& entity) override;
    bool update(const TenantEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<TenantEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<TenantEntity>& entities) override;
    int updateBulk(const std::vector<TenantEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    std::vector<TenantEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;
    std::string getRepositoryName() const override { return "TenantRepository"; }

    // =======================================================================
    // 테넌트 전용 메서드들
    // =======================================================================
    
    std::optional<TenantEntity> findByDomain(const std::string& domain);
    std::optional<TenantEntity> findByName(const std::string& name);
    std::vector<TenantEntity> findByStatus(TenantEntity::Status status);
    std::vector<TenantEntity> findActiveTenants();
    std::vector<TenantEntity> findTenantsExpiringSoon(int days_before_expiry = 30);
    std::vector<TenantEntity> findExpiredTenants();
    std::vector<TenantEntity> findTrialTenants();
    std::vector<TenantEntity> findByNamePattern(const std::string& name_pattern);
    
    // 테넌트 관리
    bool isDomainTaken(const std::string& domain, int exclude_id = 0);
    bool isNameTaken(const std::string& name, int exclude_id = 0);
    std::map<std::string, int> checkLimits(int tenant_id);
    bool extendSubscription(int tenant_id, int additional_days);
    bool updateStatus(int tenant_id, TenantEntity::Status new_status);
    bool updateLimits(int tenant_id, const std::map<std::string, int>& limits);
    
    // 통계 및 분석
    std::map<std::string, int> getTenantUsageStats(int tenant_id);
    std::map<std::string, int> getTenantStatusStats();
    std::vector<std::pair<int, int>> getExpirationSchedule(int days_ahead = 90);
    std::vector<TenantEntity> findTopUsageTenants(int limit = 10);
    std::vector<TenantEntity> findRecentTenants(int limit = 10);
    
    // 백업 및 복원
    std::string exportTenantData(int tenant_id);
    std::optional<int> importTenantData(const std::string& backup_data);
    bool cloneTenantConfig(int source_tenant_id, int target_tenant_id, bool copy_users = false);

private:
    // 내부 헬퍼 메서드들
    bool isSubscriptionExpiredInternal(const TenantEntity& tenant) const;
    bool validateTenant(const TenantEntity& tenant) const;
    QueryCondition buildStatusCondition(TenantEntity::Status status) const;
    QueryCondition buildDateRangeCondition(const std::string& field_name, 
                                         int days_from_now, 
                                         bool is_before) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // TENANT_REPOSITORY_H