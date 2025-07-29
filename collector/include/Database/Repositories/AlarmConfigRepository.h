#ifndef ALARM_CONFIG_REPOSITORY_H
#define ALARM_CONFIG_REPOSITORY_H

/**
 * @file AlarmConfigRepository.h
 * @brief PulseOne AlarmConfig Repository - 알람 설정 관리 Repository (DeviceRepository 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

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

// 🔥 네임스페이스 수정 - PulseOne::Database:: 제거
using AlarmConfigEntity = Entities::AlarmConfigEntity;

/**
 * @brief 알람 설정 Repository 클래스 (IRepository 템플릿 상속)
 */
class AlarmConfigRepository : public IRepository<AlarmConfigEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (DeviceRepository 패턴)
    // =======================================================================
    
    // 🔥 생성자에서 initializeDependencies() 호출
    AlarmConfigRepository() : IRepository<AlarmConfigEntity>("AlarmConfigRepository") {
        initializeDependencies();
        if (logger_) {
            logger_->Info("🚨 AlarmConfigRepository initialized with IRepository caching system");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~AlarmConfigRepository() = default;
    
    // =======================================================================
    // 캐시 관리 메서드들 (DeviceRepository 패턴) - override → final
    // =======================================================================
    
    void setCacheEnabled(bool enabled) final;
    bool isCacheEnabled() const final;
    void clearCache() final;
    void clearCacheForId(int id) final;
    std::map<std::string, int> getCacheStats() const final;
    
    // =======================================================================
    // IRepository 인터페이스 구현 - override → final
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findAll() final;
    std::optional<AlarmConfigEntity> findById(int id) final;
    bool save(AlarmConfigEntity& entity) final;
    bool update(const AlarmConfigEntity& entity) final;
    bool deleteById(int id) final;
    bool exists(int id) final;
    std::vector<AlarmConfigEntity> findByIds(const std::vector<int>& ids) final;
    int saveBulk(std::vector<AlarmConfigEntity>& entities) final;
    int updateBulk(const std::vector<AlarmConfigEntity>& entities) final;
    int deleteByIds(const std::vector<int>& ids) final;
    
    std::vector<AlarmConfigEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) final;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) final;
    int getTotalCount() final;
    std::string getRepositoryName() const final { return "AlarmConfigRepository"; }

    // =======================================================================
    // 알람 설정 전용 조회 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    // 기본 조회 메서드들
    std::vector<AlarmConfigEntity> findByDataPoint(int data_point_id);
    std::vector<AlarmConfigEntity> findByVirtualPoint(int virtual_point_id);
    std::vector<AlarmConfigEntity> findByTenant(int tenant_id);
    std::vector<AlarmConfigEntity> findBySite(int site_id);
    std::vector<AlarmConfigEntity> findBySeverity(AlarmConfigEntity::Severity severity);
    std::vector<AlarmConfigEntity> findByConditionType(AlarmConfigEntity::ConditionType condition_type);
    std::vector<AlarmConfigEntity> findActiveAlarms();
    std::vector<AlarmConfigEntity> findAutoAcknowledgeAlarms();
    std::vector<AlarmConfigEntity> findByNamePattern(const std::string& name_pattern);
    
    // 이름/조회 메서드들
    std::optional<AlarmConfigEntity> findByName(const std::string& name, int tenant_id = 0);
    std::vector<AlarmConfigEntity> findActiveConfigs();
    std::vector<AlarmConfigEntity> findByDevice(int device_id);
    std::vector<AlarmConfigEntity> findByPriorityRange(int min_priority, int max_priority);
    
    // 관리 메서드들
    bool isAlarmNameTaken(const std::string& name, int tenant_id = 0, int exclude_id = 0);
    bool enableConfig(int config_id, bool enabled);
    bool updateThresholds(int config_id, double low_threshold, double high_threshold);
    bool updatePriority(int config_id, int new_priority);
    bool updateSeverity(int config_id, AlarmConfigEntity::Severity new_severity);
    
    // 통계 메서드들
    int countByTenant(int tenant_id);
    int countByDataPoint(int data_point_id);
    std::map<std::string, int> getTypeStats();
    std::map<std::string, int> getSeverityStats();
    std::map<std::string, int> getStatusStats();
    std::vector<AlarmConfigEntity> findTopPriorityConfigs(int limit = 10);
    std::vector<AlarmConfigEntity> findRecentConfigs(int limit = 10);
    
    // 디바이스 연동 메서드들
    bool deployToDevice(int config_id, int device_id);
    bool syncWithDevice(int config_id);
    std::vector<AlarmConfigEntity> findConfigsNeedingSync();
    bool markSyncCompleted(int config_id);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    bool validateAlarmConfig(const AlarmConfigEntity& config) const;
    QueryCondition buildSeverityCondition(AlarmConfigEntity::Severity severity) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_REPOSITORY_H