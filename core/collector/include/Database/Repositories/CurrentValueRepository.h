// =============================================================================
// collector/include/Database/Repositories/CurrentValueRepository.h
// 기존 코드 패턴에 맞춰 깔끔하게 정리한 완성본
// =============================================================================

#ifndef CURRENT_VALUE_REPOSITORY_H
#define CURRENT_VALUE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Common/Enums.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;

/**
 * @brief Current Value Repository 클래스
 * 기존 패턴을 따라 IRepository 상속, 깔끔한 인터페이스
 */
class CurrentValueRepository : public IRepository<CurrentValueEntity> {
public:
    // =======================================================================
    // 생성자
    // =======================================================================
    CurrentValueRepository() : IRepository<CurrentValueEntity>("CurrentValueRepository") {}
    virtual ~CurrentValueRepository() = default;

    // =======================================================================
    // IRepository 필수 구현 (CRUD)
    // =======================================================================
    std::vector<CurrentValueEntity> findAll() override;
    std::optional<CurrentValueEntity> findById(int id) override;
    bool save(CurrentValueEntity& entity) override;
    bool update(const CurrentValueEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<CurrentValueEntity> findByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 캐시 관리 (IRepository 상속)
    // =======================================================================
    void clearCache() override;
    void clearCacheForId(int id) override;
    bool isCacheEnabled() const override;
    std::map<std::string, int> getCacheStats() const override;
    int getTotalCount() override;

    // =======================================================================
    // CurrentValue 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 디바이스 ID로 현재값들 조회 (WorkerManager용)
     */
    std::vector<CurrentValueEntity> findByDeviceId(int device_id);
    
    /**
     * @brief DataPoint ID로 조회 (호환성)
     */
    std::optional<CurrentValueEntity> findByDataPointId(int data_point_id);
    
    /**
     * @brief 품질별 조회
     */
    std::vector<CurrentValueEntity> findByQuality(PulseOne::Enums::DataQuality quality);
    
    /**
     * @brief 통계 카운터 증가
     */
    bool incrementReadCount(int point_id);
    bool incrementWriteCount(int point_id);
    bool incrementErrorCount(int point_id);

protected:
    // =======================================================================
    // IRepository 추상 메서드 구현
    // =======================================================================
    bool ensureTableExists();
    CurrentValueEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const CurrentValueEntity& entity);
    bool validateEntity(const CurrentValueEntity& entity) const;

private:
    bool validateCurrentValue(const CurrentValueEntity& entity) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_REPOSITORY_H