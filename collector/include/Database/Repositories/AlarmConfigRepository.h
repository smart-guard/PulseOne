#ifndef ALARM_CONFIG_REPOSITORY_H
#define ALARM_CONFIG_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmConfigEntity.h"
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

using AlarmConfigEntity = PulseOne::Database::Entities::AlarmConfigEntity;

class AlarmConfigRepository : public IRepository<AlarmConfigEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmConfigRepository();
    virtual ~AlarmConfigRepository() = default;
    
    // =======================================================================
    // 캐시 관리 메서드들 (한 곳에만 선언)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findAll() override;
    std::optional<AlarmConfigEntity> findById(int id) override;
    bool save(AlarmConfigEntity& entity) override;
    bool update(const AlarmConfigEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<AlarmConfigEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<AlarmConfigEntity>& entities) override;
    int updateBulk(const std::vector<AlarmConfigEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmConfigEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;
    std::string getRepositoryName() const override { return "AlarmConfigRepository"; }

    // =======================================================================
    // 🔥 구현 파일에 있는 모든 메서드들 선언 추가
    // =======================================================================
    
    // 기본 조회 메서드들
    std::vector<AlarmConfigEntity> findByDataPoint(int data_point_id);
    std::vector<AlarmConfigEntity> findByDevice(int device_id);
    std::vector<AlarmConfigEntity> findEnabledAlarms();
    std::vector<AlarmConfigEntity> findByAlarmType(const std::string& alarm_type);
    std::vector<AlarmConfigEntity> findBySeverity(const std::string& severity);
    
    // 🔥 구현 파일에만 있던 메서드들
    std::vector<AlarmConfigEntity> findByVirtualPoint(int virtual_point_id);
    std::vector<AlarmConfigEntity> findByTenant(int tenant_id);
    std::vector<AlarmConfigEntity> findBySite(int site_id);
    std::vector<AlarmConfigEntity> findBySeverity(AlarmConfigEntity::Severity severity);  // 오버로드
    std::vector<AlarmConfigEntity> findByConditionType(AlarmConfigEntity::ConditionType condition_type);
    std::vector<AlarmConfigEntity> findActiveAlarms();
    std::vector<AlarmConfigEntity> findAutoAcknowledgeAlarms();
    std::vector<AlarmConfigEntity> findByNamePattern(const std::string& name_pattern);
    
    // 이름/조회 메서드들
    std::optional<AlarmConfigEntity> findByName(const std::string& name, int tenant_id = 0);
    std::vector<AlarmConfigEntity> findActiveConfigs();
    std::vector<AlarmConfigEntity> findByPriorityRange(int min_priority, int max_priority);
    
    // 🔥 관리 메서드들 (누락된 선언)
    bool isAlarmNameTaken(const std::string& name, int tenant_id = 0, int exclude_id = 0);
    bool enableConfig(int config_id, bool enabled);
    bool updateThresholds(int config_id, double low_threshold, double high_threshold);
    bool updatePriority(int config_id, int new_priority);
    bool updateSeverity(int config_id, AlarmConfigEntity::Severity new_severity);
    
    // 🔥 통계 메서드들 (누락된 선언)
    int countByTenant(int tenant_id);
    int countByDataPoint(int data_point_id);
    std::map<std::string, int> getTypeStats();
    std::map<std::string, int> getSeverityStats();
    std::map<std::string, int> getStatusStats();
    std::vector<AlarmConfigEntity> findTopPriorityConfigs(int limit = 10);
    std::vector<AlarmConfigEntity> findRecentConfigs(int limit = 10);
    
    // 🔥 디바이스 연동 메서드들 (누락된 선언)
    bool deployToDevice(int config_id, int device_id);
    bool syncWithDevice(int config_id);
    std::vector<AlarmConfigEntity> findConfigsNeedingSync();
    bool markSyncCompleted(int config_id);

private:
    // =======================================================================
    // 🔥 내부 헬퍼 메서드들 (누락된 선언)
    // =======================================================================
    
    bool validateAlarmConfig(const AlarmConfigEntity& config) const;
    QueryCondition buildSeverityCondition(AlarmConfigEntity::Severity severity) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_REPOSITORY_H