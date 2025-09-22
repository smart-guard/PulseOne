// =============================================================================
// collector/include/Database/Repositories/VirtualPointRepository.h
// PulseOne VirtualPointRepository - 완전한 메서드 선언
// =============================================================================

#ifndef VIRTUAL_POINT_REPOSITORY_H
#define VIRTUAL_POINT_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/VirtualPointEntity.h"
#include "Database/DatabaseTypes.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

using VirtualPointEntity = PulseOne::Database::Entities::VirtualPointEntity;

class VirtualPointRepository : public IRepository<VirtualPointEntity> {
public:
    VirtualPointRepository() : IRepository<VirtualPointEntity>("VirtualPointRepository") {
        LogManager::getInstance().Info("VirtualPointRepository initialized with ScriptLibraryRepository pattern");
        LogManager::getInstance().Info("Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
    }
    
    virtual ~VirtualPointRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수)
    // =======================================================================
    
    std::vector<VirtualPointEntity> findAll() override;
    std::optional<VirtualPointEntity> findById(int id) override;
    bool save(VirtualPointEntity& entity) override;
    bool update(const VirtualPointEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    int getTotalCount() override;

    // =======================================================================
    // 벌크 연산 (IRepository 상속)
    // =======================================================================
    
    std::vector<VirtualPointEntity> findByIds(const std::vector<int>& ids) override;
    std::vector<VirtualPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int saveBulk(std::vector<VirtualPointEntity>& entities) override;
    int updateBulk(const std::vector<VirtualPointEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void clearCache() override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // VirtualPoint 전용 조회 메서드들
    // =======================================================================
    
    std::vector<VirtualPointEntity> findByTenant(int tenant_id);
    std::vector<VirtualPointEntity> findBySite(int site_id);
    std::vector<VirtualPointEntity> findByDevice(int device_id);
    std::vector<VirtualPointEntity> findEnabled();
    std::vector<VirtualPointEntity> findByCategory(const std::string& category);
    std::vector<VirtualPointEntity> findByExecutionType(const std::string& execution_type);
    std::vector<VirtualPointEntity> findByTag(const std::string& tag);
    std::vector<VirtualPointEntity> findDependents(int point_id);

    // =======================================================================
    // VirtualPoint 전용 업데이트 메서드들
    // =======================================================================
    
    bool updateExecutionStats(int id, double last_value, double execution_time_ms);
    bool updateLastError(int id, const std::string& error_message);
    bool updateLastValue(int id, double last_value);
    bool setEnabled(int id, bool enabled);
    bool resetExecutionStats(int id);

protected:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    VirtualPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const VirtualPointEntity& entity);
    bool validateVirtualPoint(const VirtualPointEntity& entity);
    bool ensureTableExists();

private:
    // =======================================================================
    // 캐시 관련 메서드들
    // =======================================================================
    
    bool isCacheEnabled() const;
    std::optional<VirtualPointEntity> getCachedEntity(int id);
    void cacheEntity(int id, const VirtualPointEntity& entity);
    void clearCacheForId(int id);
    std::string generateCacheKey(int id) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_REPOSITORY_H