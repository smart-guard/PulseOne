// =============================================================================
// collector/include/Database/Repositories/AlarmRuleRepository.h
// PulseOne AlarmRuleRepository - DeviceRepository/DataPointRepository 패턴 100% 준수
// =============================================================================

#ifndef ALARM_RULE_REPOSITORY_H
#define ALARM_RULE_REPOSITORY_H

/**
 * @file AlarmRuleRepository.h
 * @brief PulseOne AlarmRuleRepository - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🔥 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - IRepository<AlarmRuleEntity> 상속
 * - SQLQueries.h 상수 사용
 * - 캐싱 및 벌크 연산 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/DatabaseManager.h"
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
using AlarmRuleEntity = PulseOne::Database::Entities::AlarmRuleEntity;

/**
 * @brief AlarmRule Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 대상별/중요도별 알람 규칙 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmRuleRepository : public IRepository<AlarmRuleEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmRuleRepository() : IRepository<AlarmRuleEntity>("AlarmRuleRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🚨 AlarmRuleRepository initialized with BaseEntity pattern");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~AlarmRuleRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<AlarmRuleEntity> findAll() override;
    std::optional<AlarmRuleEntity> findById(int id) override;
    bool save(AlarmRuleEntity& entity) override;
    bool update(const AlarmRuleEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<AlarmRuleEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmRuleEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    std::optional<AlarmRuleEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    ) override;
    
    int saveBulk(const std::vector<AlarmRuleEntity>& entities) override;
    int updateBulk(const std::vector<AlarmRuleEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // AlarmRule 전용 메서드들 (SQLQueries.h 상수 사용)
    // =======================================================================
    
    /**
     * @brief 특정 대상에 대한 알람 규칙들 조회
     * @param target_type 대상 타입 ('data_point', 'virtual_point', 'group')
     * @param target_id 대상 ID
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 해당 대상의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByTarget(const std::string& target_type, int target_id, bool enabled_only = true);
    
    /**
     * @brief 테넌트별 알람 규칙들 조회
     * @param tenant_id 테넌트 ID
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 테넌트의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByTenant(int tenant_id, bool enabled_only = true);
    
    /**
     * @brief 중요도별 알람 규칙들 조회
     * @param severity 중요도 ('critical', 'high', 'medium', 'low', 'info')
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 해당 중요도의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findBySeverity(const std::string& severity, bool enabled_only = true);
    
    /**
     * @brief 알람 타입별 알람 규칙들 조회
     * @param alarm_type 알람 타입 ('analog', 'digital', 'script')
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 해당 타입의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByAlarmType(const std::string& alarm_type, bool enabled_only = true);
    
    /**
     * @brief 활성화된 모든 알람 규칙들 조회 (우선순위 정렬)
     * @return 활성화된 알람 규칙 목록 (우선순위 DESC, 중요도, 이름 순)
     */
    std::vector<AlarmRuleEntity> findAllEnabled();
    
    /**
     * @brief 여러 대상에 대한 알람 규칙들 조회 (성능 최적화)
     * @param target_type 대상 타입
     * @param target_ids 대상 ID 목록
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 해당 대상들의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByTargets(const std::string& target_type, const std::vector<int>& target_ids, bool enabled_only = true);
    
    /**
     * @brief 특정 이름의 알람 규칙 조회 (중복 체크용)
     * @param name 알람 규칙 이름
     * @param tenant_id 테넌트 ID
     * @param exclude_id 제외할 ID (업데이트 시 자기 자신 제외용, 기본값: -1)
     * @return 해당 이름의 알람 규칙 (없으면 nullopt)
     */
    std::optional<AlarmRuleEntity> findByName(const std::string& name, int tenant_id, int exclude_id = -1);
    
    // =======================================================================
    // 통계 및 집계 메서드들
    // =======================================================================
    
    /**
     * @brief 중요도별 알람 규칙 수 조회
     * @return 중요도별 개수 맵
     */
    std::map<std::string, int> getSeverityDistribution();
    
    /**
     * @brief 알람 타입별 알람 규칙 수 조회
     * @return 알람 타입별 개수 맵
     */
    std::map<std::string, int> getAlarmTypeDistribution();
    
    /**
     * @brief 테넌트별 알람 규칙 수 조회
     * @return 테넌트별 개수 맵
     */
    std::map<int, int> getTenantDistribution();
    
    /**
     * @brief 활성화/비활성화 알람 규칙 수 조회
     * @return {enabled_count, disabled_count}
     */
    std::pair<int, int> getEnabledDisabledCount();
    
    // =======================================================================
    // 캐시 관리 메서드들 (IRepository 확장)
    // =======================================================================
    
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 규칙 이름 중복 체크
     * @param name 체크할 이름
     * @param tenant_id 테넌트 ID
     * @param exclude_id 제외할 ID (업데이트 시, 기본값: -1)
     * @return 중복되면 true
     */
    bool isNameTaken(const std::string& name, int tenant_id, int exclude_id = -1);
    
    /**
     * @brief 알람 규칙 활성화/비활성화
     * @param id 알람 규칙 ID
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool updateStatus(int id, bool enabled);
    
    /**
     * @brief 여러 알람 규칙 상태 일괄 변경
     * @param ids 알람 규칙 ID 목록
     * @param enabled 활성화 여부
     * @return 변경된 개수
     */
    int bulkUpdateStatus(const std::vector<int>& ids, bool enabled);
    
    /**
     * @brief 알람 규칙 유효성 검증
     * @param entity 검증할 알람 규칙
     * @return 유효하면 true
     */
    bool validateAlarmRule(const AlarmRuleEntity& entity);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    AlarmRuleEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     * @param entity 변환할 엔티티
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const AlarmRuleEntity& entity);
    
    /**
     * @brief 여러 행을 엔티티 벡터로 변환
     * @param result 쿼리 결과
     * @return 엔티티 목록
     */
    std::vector<AlarmRuleEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    // =======================================================================
    // 데이터베이스 헬퍼 메서드들
    // =======================================================================
    
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
    
    /**
     * @brief 쿼리 조건을 WHERE 절로 변환
     * @param conditions 조건 목록
     * @return WHERE 절 문자열
     */
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    
    /**
     * @brief 정렬 조건을 ORDER BY 절로 변환
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    
    /**
     * @brief 페이징 정보를 LIMIT 절로 변환
     * @param pagination 페이징 정보
     * @return LIMIT 절 문자열
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_RULE_REPOSITORY_H