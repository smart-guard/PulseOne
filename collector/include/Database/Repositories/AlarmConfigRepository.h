#ifndef ALARM_CONFIG_REPOSITORY_H
#define ALARM_CONFIG_REPOSITORY_H

/**
 * @file AlarmConfigRepository.h
 * @brief PulseOne AlarmConfig Repository - DeviceRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 컴파일 에러 완전 해결
 * - 깔끔하고 유지보수 가능한 코드
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmConfigEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (DeviceRepository 패턴)
using AlarmConfigEntity = PulseOne::Database::Entities::AlarmConfigEntity;

/**
 * @brief Alarm Config Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 알람 설정별 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmConfigRepository : public IRepository<AlarmConfigEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmConfigRepository() : IRepository<AlarmConfigEntity>("AlarmConfigRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🚨 AlarmConfigRepository initialized with DatabaseAbstractionLayer");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~AlarmConfigRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findAll() override;
    std::optional<AlarmConfigEntity> findById(int id) override;
    bool save(AlarmConfigEntity& entity) override;
    bool update(const AlarmConfigEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmConfigEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // 알람 설정 전용 메서드들
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findByDataPoint(int data_point_id);
    std::vector<AlarmConfigEntity> findByVirtualPoint(int virtual_point_id);
    std::vector<AlarmConfigEntity> findByTenant(int tenant_id);
    std::vector<AlarmConfigEntity> findBySite(int site_id);
    std::vector<AlarmConfigEntity> findBySeverity(AlarmConfigEntity::Severity severity);
    std::vector<AlarmConfigEntity> findByConditionType(AlarmConfigEntity::ConditionType condition_type);
    std::vector<AlarmConfigEntity> findActiveAlarms();
    std::vector<AlarmConfigEntity> findAutoAcknowledgeAlarms();
    std::vector<AlarmConfigEntity> findByNamePattern(const std::string& name_pattern);

    // =======================================================================
    // 벌크 연산 (DeviceRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<AlarmConfigEntity>& entities);
    int updateBulk(const std::vector<AlarmConfigEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    std::optional<AlarmConfigEntity> findByName(const std::string& name, int tenant_id = 0);
    std::vector<AlarmConfigEntity> findActiveConfigs();
    std::vector<AlarmConfigEntity> findByDevice(int device_id);
    std::vector<AlarmConfigEntity> findByPriorityRange(int min_priority, int max_priority);
    
    bool isAlarmNameTaken(const std::string& name, int tenant_id = 0, int exclude_id = 0);
    bool enableConfig(int config_id, bool enabled);
    bool updateThresholds(int config_id, double low_threshold, double high_threshold);
    bool updatePriority(int config_id, int new_priority);
    bool updateSeverity(int config_id, AlarmConfigEntity::Severity new_severity);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::string getAlarmStatistics() const;
    std::vector<AlarmConfigEntity> findInactiveConfigs() const;
    std::map<std::string, int> getSeverityDistribution() const;

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // Worker용 최적화 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    int getTotalCount();

private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    
    DatabaseManager* db_manager_;
    LogManager* logger_;
    ConfigManager* config_manager_;
    
    void initializeDependencies();

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 AlarmConfigEntity로 변환
     * @param row SQL 결과 행
     * @return AlarmConfigEntity
     */
    AlarmConfigEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 AlarmConfigEntity 벡터로 변환
     * @param result SQL 결과
     * @return AlarmConfigEntity 벡터
     */
    std::vector<AlarmConfigEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief AlarmConfigEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const AlarmConfigEntity& entity);
    
    /**
     * @brief alarm_definitions 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 알람 설정 검증
     * @param entity 검증할 알람 설정 엔티티
     * @return 유효하면 true
     */
    bool validateAlarmConfig(const AlarmConfigEntity& entity) const;
    
    /**
     * @brief 심각도 조건 생성
     * @param severity 심각도
     * @return QueryCondition
     */
    QueryCondition buildSeverityCondition(AlarmConfigEntity::Severity severity) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_REPOSITORY_H