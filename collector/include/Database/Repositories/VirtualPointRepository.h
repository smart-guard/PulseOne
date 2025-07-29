#ifndef VIRTUAL_POINT_REPOSITORY_H
#define VIRTUAL_POINT_REPOSITORY_H

/**
 * @file VirtualPointRepository.h
 * @brief PulseOne VirtualPoint Repository - 가상포인트 관리 Repository (SiteRepository 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 데이터베이스 스키마 (virtual_points 테이블):
 * - id: PRIMARY KEY AUTOINCREMENT
 * - tenant_id: INTEGER NOT NULL (FK to tenants)
 * - site_id: INTEGER (FK to sites, nullable)
 * - name: VARCHAR(100) NOT NULL
 * - description: TEXT
 * - formula: TEXT NOT NULL (JavaScript 계산식)
 * - data_type: VARCHAR(20) DEFAULT 'float'
 * - unit: VARCHAR(20)
 * - calculation_interval: INTEGER DEFAULT 1000 (ms)
 * - is_enabled: BOOLEAN DEFAULT true
 * - created_at: DATETIME DEFAULT CURRENT_TIMESTAMP
 * - updated_at: DATETIME DEFAULT CURRENT_TIMESTAMP
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/VirtualPointEntity.h"
#include "Database/DatabaseTypes.h"
#include "Utils/LogManager.h"
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <map>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 기존 패턴 준수 - using 선언 필수
using VirtualPointEntity = Entities::VirtualPointEntity;

/**
 * @brief VirtualPoint Repository 클래스 (IRepository 템플릿 상속)
 */
class VirtualPointRepository : public IRepository<VirtualPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (SiteRepository 패턴)
    // =======================================================================
    
    VirtualPointRepository();
    virtual ~VirtualPointRepository() = default;

    // =======================================================================
    // 캐시 관리 메서드들 (SiteRepository 패턴)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // IRepository 인터페이스 구현 (SiteRepository 패턴)
    // =======================================================================
    
    std::vector<VirtualPointEntity> findAll() override;
    std::optional<VirtualPointEntity> findById(int id) override;
    bool save(VirtualPointEntity& entity) override;
    bool update(const VirtualPointEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<VirtualPointEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<VirtualPointEntity>& entities) override;
    int updateBulk(const std::vector<VirtualPointEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    std::vector<VirtualPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;

    // =======================================================================
    // VirtualPoint 전용 메서드들 (SiteRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 테넌트별 가상포인트 조회
     * @param tenant_id 테넌트 ID
     * @return 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 사이트별 가상포인트 조회
     * @param site_id 사이트 ID
     * @return 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findBySite(int site_id);
    
    /**
     * @brief 활성화된 가상포인트 조회
     * @param tenant_id 테넌트 ID (0이면 모든 테넌트)
     * @return 활성화된 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findEnabledPoints(int tenant_id = 0);
    
    /**
     * @brief 이름으로 가상포인트 조회
     * @param name 가상포인트 이름
     * @param tenant_id 테넌트 ID
     * @return 가상포인트 (없으면 nullopt)
     */
    std::optional<VirtualPointEntity> findByName(const std::string& name, int tenant_id);
    
    /**
     * @brief 카테고리별 가상포인트 조회
     * @param category 카테고리
     * @param tenant_id 테넌트 ID (0이면 모든 테넌트)
     * @return 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findByCategory(const std::string& category, int tenant_id = 0);
    
    /**
     * @brief 데이터 타입별 가상포인트 조회
     * @param data_type 데이터 타입
     * @param tenant_id 테넌트 ID (0이면 모든 테넌트)
     * @return 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findByDataType(VirtualPointEntity::DataType data_type, int tenant_id = 0);
    
    /**
     * @brief 이름 패턴으로 가상포인트 조회
     * @param name_pattern 이름 패턴 (LIKE 연산자 사용)
     * @param tenant_id 테넌트 ID (0이면 모든 테넌트)
     * @return 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findByNamePattern(const std::string& name_pattern, int tenant_id = 0);

    // =======================================================================
    // 비즈니스 로직 메서드들 (VirtualPoint 전용)
    // =======================================================================
    
    /**
     * @brief 가상포인트 이름 중복 확인
     * @param name 가상포인트 이름
     * @param tenant_id 테넌트 ID
     * @param exclude_id 제외할 ID (업데이트 시 사용)
     * @return 중복이면 true
     */
    bool isPointNameTaken(const std::string& name, int tenant_id, int exclude_id = 0);
    
    /**
     * @brief 계산이 필요한 가상포인트들 조회
     * @param limit 최대 개수 (0이면 제한 없음)
     * @return 계산이 필요한 가상포인트 목록
     */
    std::vector<VirtualPointEntity> findPointsNeedingCalculation(int limit = 0);
    
    /**
     * @brief 수식 유효성 검사
     * @param entity 검사할 가상포인트
     * @return 유효하면 true
     */
    bool validateFormula(const VirtualPointEntity& entity);

    // =======================================================================
    // 계산 관련 메서드들 (VirtualPoint 전용)
    // =======================================================================
    
    /**
     * @brief 가상포인트 계산 실행
     * @param point_id 가상포인트 ID
     * @param force_calculation 강제 계산 여부
     * @return 계산 결과값 (실패 시 nullopt)
     */
    std::optional<double> executeCalculation(int point_id, bool force_calculation = false);
    
    /**
     * @brief 계산된 값 업데이트
     * @param point_id 가상포인트 ID
     * @param value 계산 결과값
     * @param quality 데이터 품질 (기본값: "GOOD")
     * @return 성공 시 true
     */
    bool updateCalculatedValue(int point_id, double value, const std::string& quality = "GOOD");

private:
    // =======================================================================
    // private 헬퍼 메서드들 (SiteRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 가상포인트 유효성 검사
     * @param point 검사할 가상포인트
     * @return 유효하면 true
     */
    bool validateVirtualPoint(const VirtualPointEntity& point) const;
    
    /**
     * @brief 테넌트 조건 생성
     * @param tenant_id 테넌트 ID
     * @return QueryCondition 객체
     */
    QueryCondition buildTenantCondition(int tenant_id) const;
    
    /**
     * @brief 사이트 조건 생성
     * @param site_id 사이트 ID
     * @return QueryCondition 객체
     */
    QueryCondition buildSiteCondition(int site_id) const;
    
    /**
     * @brief 활성화 조건 생성
     * @param enabled 활성화 여부
     * @return QueryCondition 객체
     */
    QueryCondition buildEnabledCondition(bool enabled) const;
    
    /**
     * @brief 데이터 타입 조건 생성
     * @param data_type 데이터 타입
     * @return QueryCondition 객체
     */
    QueryCondition buildDataTypeCondition(VirtualPointEntity::DataType data_type) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_REPOSITORY_H