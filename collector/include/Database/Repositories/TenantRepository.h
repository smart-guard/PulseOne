#ifndef TENANT_REPOSITORY_H
#define TENANT_REPOSITORY_H

/**
 * @file TenantRepository.h
 * @brief PulseOne TenantRepository - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔥 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - 컴파일 에러 완전 해결
 * - BaseEntity 상속 패턴 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/TenantEntity.h"
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
using TenantEntity = PulseOne::Database::Entities::TenantEntity;

/**
 * @brief Tenant Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 상태별 테넌트 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class TenantRepository : public IRepository<TenantEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    TenantRepository() : IRepository<TenantEntity>("TenantRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 TenantRepository initialized with BaseEntity pattern");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~TenantRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    std::map<std::string, int> getCacheStats() const override;
    std::vector<TenantEntity> findAll() override;
    std::optional<TenantEntity> findById(int id) override;
    bool save(TenantEntity& entity) override;
    bool update(const TenantEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<TenantEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<TenantEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Tenant 전용 메서드들
    // =======================================================================
    
    std::optional<TenantEntity> findByDomain(const std::string& domain);
    std::vector<TenantEntity> findByStatus(TenantEntity::Status status);
    std::vector<TenantEntity> findActiveTenants();
    std::vector<TenantEntity> findExpiredTenants();
    std::vector<TenantEntity> findTrialTenants();
    std::optional<TenantEntity> findByName(const std::string& name);
    std::map<std::string, std::vector<TenantEntity>> groupByStatus();

    // =======================================================================
    // 벌크 연산 (DeviceRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<TenantEntity>& entities);
    int updateBulk(const std::vector<TenantEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 실시간 테넌트 관리
    // =======================================================================
    
    bool updateStatus(int tenant_id, TenantEntity::Status status);
    bool extendSubscription(int tenant_id, int additional_days);
    bool isDomainTaken(const std::string& domain, int exclude_id = 0);
    bool isNameTaken(const std::string& name, int exclude_id = 0);
    bool updateLimits(int tenant_id, const std::map<std::string, int>& limits);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::string getTenantStatistics() const;
    std::vector<TenantEntity> findInactiveTenants() const;
    std::map<std::string, int> getStatusDistribution() const;

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<TenantEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("TenantRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<TenantEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<TenantEntity>::clearCache();
        if (logger_) {
            logger_->Info("TenantRepository cache cleared");
        }
    }

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
    
    void initializeDependencies() {
        db_manager_ = &DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 TenantEntity로 변환
     * @param row SQL 결과 행
     * @return TenantEntity
     */
    TenantEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 TenantEntity 벡터로 변환
     * @param result SQL 결과
     * @return TenantEntity 벡터
     */
    std::vector<TenantEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief TenantEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const TenantEntity& entity);
    
    /**
     * @brief tenants 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 테넌트 검증
     * @param entity 검증할 테넌트 엔티티
     * @return 유효하면 true
     */
    bool validateTenant(const TenantEntity& entity) const;
    
    /**
     * @brief SQL 문자열 이스케이프 처리
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const;
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     * @param timestamp 타임스탬프
     * @return 문자열 형태의 타임스탬프
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    
    /**
     * @brief WHERE 절 생성
     * @param conditions 조건들
     * @return WHERE 절 문자열
     */
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    
    /**
     * @brief ORDER BY 절 생성
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    
    /**
     * @brief LIMIT 절 생성
     * @param pagination 페이지네이션
     * @return LIMIT 절 문자열
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // TENANT_REPOSITORY_H