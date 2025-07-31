#ifndef VIRTUAL_POINT_REPOSITORY_H
#define VIRTUAL_POINT_REPOSITORY_H

/**
 * @file VirtualPointRepository.h
 * @brief PulseOne VirtualPointRepository - DeviceRepository 패턴 100% 적용
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
#include "Database/Entities/VirtualPointEntity.h"
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
using VirtualPointEntity = PulseOne::Database::Entities::VirtualPointEntity;

/**
 * @brief VirtualPoint Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 수식별 가상포인트 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class VirtualPointRepository : public IRepository<VirtualPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    VirtualPointRepository() : IRepository<VirtualPointEntity>("VirtualPointRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 VirtualPointRepository initialized with BaseEntity pattern");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~VirtualPointRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<VirtualPointEntity> findAll() override;
    std::optional<VirtualPointEntity> findById(int id) override;
    bool save(VirtualPointEntity& entity) override;
    bool update(const VirtualPointEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<VirtualPointEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<VirtualPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // VirtualPoint 전용 메서드들
    // =======================================================================
    
    std::vector<VirtualPointEntity> findByTenant(int tenant_id);
    std::vector<VirtualPointEntity> findBySite(int site_id);
    std::vector<VirtualPointEntity> findEnabledPoints(int tenant_id = 0);
    std::optional<VirtualPointEntity> findByName(const std::string& name, int tenant_id);

    // =======================================================================
    // 벌크 연산 (DeviceRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<VirtualPointEntity>& entities);
    int updateBulk(const std::vector<VirtualPointEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 실시간 가상포인트 관리
    // =======================================================================
    
    bool enableVirtualPoint(int point_id);
    bool disableVirtualPoint(int point_id);
    bool updateVirtualPointStatus(int point_id, bool is_enabled);
    bool updateFormula(int point_id, const std::string& formula);

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
    
    void initializeDependencies() {
        db_manager_ = &DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 VirtualPointEntity로 변환
     * @param row SQL 결과 행
     * @return VirtualPointEntity
     */
    VirtualPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 VirtualPointEntity 벡터로 변환
     * @param result SQL 결과
     * @return VirtualPointEntity 벡터
     */
    std::vector<VirtualPointEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief VirtualPointEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const VirtualPointEntity& entity);
    
    /**
     * @brief virtual_points 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 가상포인트 검증
     * @param entity 검증할 가상포인트 엔티티
     * @return 유효하면 true
     */
    bool validateVirtualPoint(const VirtualPointEntity& entity) const;
    
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

#endif // VIRTUAL_POINT_REPOSITORY_H