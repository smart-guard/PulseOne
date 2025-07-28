#ifndef TENANT_REPOSITORY_H
#define TENANT_REPOSITORY_H

/**
 * @file TenantRepository.h
 * @brief PulseOne TenantRepository - IRepository 기반 테넌트 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

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

// 타입 별칭 정의
using TenantEntity = PulseOne::Database::Entities::TenantEntity;

/**
 * @brief Tenant Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 다중 테넌트 관리
 * - 구독 및 제한 관리
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class TenantRepository : public IRepository<TenantEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (IRepository 초기화 포함)
     */
    TenantRepository();
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~TenantRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (자동 캐시 적용)
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
    // 테넌트 전용 조회 메서드들
    // =======================================================================
    
    /**
     * @brief 도메인으로 테넌트 조회
     * @param domain 도메인 (예: acme.pulseone.com)
     * @return 테넌트 엔티티 (없으면 nullopt)
     */
    std::optional<TenantEntity> findByDomain(const std::string& domain);
    
    /**
     * @brief 테넌트명으로 테넌트 조회
     * @param name 테넌트명
     * @return 테넌트 엔티티 (없으면 nullopt)
     */
    std::optional<TenantEntity> findByName(const std::string& name);
    
    /**
     * @brief 상태별 테넌트 조회
     * @param status 테넌트 상태
     * @return 테넌트 목록
     */
    std::vector<TenantEntity> findByStatus(TenantEntity::Status status);
    
    /**
     * @brief 활성 테넌트 목록 조회
     * @return 활성 테넌트 목록
     */
    std::vector<TenantEntity> findActiveTenants();
    
    /**
     * @brief 구독 만료 임박 테넌트 조회
     * @param days_before_expiry 만료 전 일수 (기본값: 30일)
     * @return 구독 만료 임박 테넌트 목록
     */
    std::vector<TenantEntity> findTenantsExpiringSoon(int days_before_expiry = 30);
    
    /**
     * @brief 만료된 테넌트 조회
     * @return 만료된 테넌트 목록
     */
    std::vector<TenantEntity> findExpiredTenants();
    
    /**
     * @brief 시험 계정 테넌트 조회
     * @return 시험 계정 테넌트 목록
     */
    std::vector<TenantEntity> findTrialTenants();
    
    /**
     * @brief 테넌트명 패턴으로 검색
     * @param name_pattern 테넌트명 패턴 (LIKE 검색)
     * @return 테넌트 목록
     */
    std::vector<TenantEntity> findByNamePattern(const std::string& name_pattern);

    // =======================================================================
    // 테넌트 관리 특화 메서드들
    // =======================================================================
    
    /**
     * @brief 도메인 중복 확인
     * @param domain 확인할 도메인
     * @param exclude_id 제외할 테넌트 ID (수정 시 사용)
     * @return 중복이면 true
     */
    bool isDomainTaken(const std::string& domain, int exclude_id = 0);
    
    /**
     * @brief 테넌트명 중복 확인
     * @param name 확인할 테넌트명
     * @param exclude_id 제외할 테넌트 ID (수정 시 사용)
     * @return 중복이면 true
     */
    bool isNameTaken(const std::string& name, int exclude_id = 0);
    
    /**
     * @brief 테넌트별 사용량 제한 확인
     * @param tenant_id 테넌트 ID
     * @return 제한 상태 맵 {users: limit, devices: limit, data_points: limit}
     */
    std::map<std::string, int> checkLimits(int tenant_id);
    
    /**
     * @brief 구독 연장
     * @param tenant_id 테넌트 ID
     * @param additional_days 연장할 일수
     * @return 성공 시 true
     */
    bool extendSubscription(int tenant_id, int additional_days);
    
    /**
     * @brief 테넌트 상태 변경
     * @param tenant_id 테넌트 ID
     * @param new_status 새로운 상태
     * @return 성공 시 true
     */
    bool updateStatus(int tenant_id, TenantEntity::Status new_status);
    
    /**
     * @brief 테넌트 제한 업데이트
     * @param tenant_id 테넌트 ID
     * @param limits 제한 맵 {max_users, max_devices, max_data_points}
     * @return 성공 시 true
     */
    bool updateLimits(int tenant_id, const std::map<std::string, int>& limits);

    // =======================================================================
    // 테넌트 통계 및 분석 메서드들
    // =======================================================================
    
    /**
     * @brief 테넌트별 사용량 통계 조회
     * @param tenant_id 테넌트 ID
     * @return 사용량 통계 맵
     */
    std::map<std::string, int> getTenantUsageStats(int tenant_id);
    
    /**
     * @brief 전체 테넌트 통계 조회
     * @return 상태별 테넌트 수 맵
     */
    std::map<std::string, int> getTenantStatusStats();
    
    /**
     * @brief 구독 만료 일정 조회
     * @param days_ahead 앞으로 확인할 일수 (기본값: 90일)
     * @return 만료 일정 리스트 {tenant_id, days_remaining}
     */
    std::vector<std::pair<int, int>> getExpirationSchedule(int days_ahead = 90);
    
    /**
     * @brief 상위 사용률 테넌트 조회
     * @param limit 조회할 개수 (기본값: 10)
     * @return 사용률 높은 테넌트 목록
     */
    std::vector<TenantEntity> findTopUsageTenants(int limit = 10);
    
    /**
     * @brief 최근 생성된 테넌트 목록
     * @param limit 조회할 개수 (기본값: 10)
     * @return 최근 생성 테넌트 목록
     */
    std::vector<TenantEntity> findRecentTenants(int limit = 10);

    // =======================================================================
    // 백업 및 복원 메서드들
    // =======================================================================
    
    /**
     * @brief 테넌트 데이터 백업
     * @param tenant_id 테넌트 ID
     * @return 백업 데이터 JSON 문자열
     */
    std::string exportTenantData(int tenant_id);
    
    /**
     * @brief 테넌트 데이터 복원
     * @param backup_data 백업 데이터 JSON 문자열
     * @return 성공 시 생성된 테넌트 ID
     */
    std::optional<int> importTenantData(const std::string& backup_data);
    
    /**
     * @brief 테넌트 설정 복사
     * @param source_tenant_id 소스 테넌트 ID
     * @param target_tenant_id 타겟 테넌트 ID
     * @param copy_users 사용자도 복사할지 여부
     * @return 성공 시 true
     */
    bool cloneTenantConfig(int source_tenant_id, int target_tenant_id, bool copy_users = false);

    // =======================================================================
    // 캐시 관리 (IRepository에서 자동 제공)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 구독 만료 확인 내부 로직
     * @param tenant 테넌트 엔티티
     * @return 만료되었으면 true
     */
    bool isSubscriptionExpiredInternal(const TenantEntity& tenant) const;
    
    /**
     * @brief 테넌트 유효성 검사
     * @param tenant 테넌트 엔티티
     * @return 유효하면 true
     */
    bool validateTenant(const TenantEntity& tenant) const;
    
    /**
     * @brief SQL WHERE 조건 빌드 (상태별)
     * @param status 테넌트 상태
     * @return QueryCondition 객체
     */
    QueryCondition buildStatusCondition(TenantEntity::Status status) const;
    
    /**
     * @brief SQL WHERE 조건 빌드 (날짜 범위)
     * @param field_name 필드명
     * @param days_from_now 현재부터 며칠
     * @param is_before true면 이전, false면 이후
     * @return QueryCondition 객체
     */
    QueryCondition buildDateRangeCondition(const std::string& field_name, 
                                         int days_from_now, 
                                         bool is_before) const;

    // =======================================================================
    // 멤버 변수들 (IRepository에서 상속받은 것들 제외)
    // =======================================================================
    
    // IRepository에서 상속받은 멤버들:
    // - logger_ (LogManager 참조)
    // - db_manager_ (DatabaseManager 참조)
    // - cache_enabled_ (캐시 활성화 상태)
    // - entity_cache_ (엔티티 캐시)
    // - cache_stats_ (캐시 통계)
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // TENANT_REPOSITORY_H