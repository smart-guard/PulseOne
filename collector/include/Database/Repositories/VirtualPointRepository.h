// =============================================================================
// collector/include/Database/Repositories/VirtualPointRepository.h
// PulseOne VirtualPointRepository - DeviceRepository 패턴 100% 적용 (완성본)
// =============================================================================

#ifndef VIRTUAL_POINT_REPOSITORY_H
#define VIRTUAL_POINT_REPOSITORY_H

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
    // 벌크 연산 (IRepository 시그니처에 맞춤)
    // =======================================================================
    
    std::vector<VirtualPointEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<VirtualPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    // ✅ IRepository에 실제로 있는 벌크 메서드들
    int saveBulk(std::vector<VirtualPointEntity>& entities) override;
    int updateBulk(const std::vector<VirtualPointEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 캐시 관리 (IRepository에서 상속)
    // =======================================================================
    
    void clearCache() override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // VirtualPoint 전용 메서드
    // =======================================================================
    
    /**
     * @brief 테넌트별 가상포인트 조회
     */
    std::vector<VirtualPointEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 사이트별 가상포인트 조회
     */
    std::vector<VirtualPointEntity> findBySite(int site_id);
    
    /**
     * @brief 디바이스별 가상포인트 조회
     */
    std::vector<VirtualPointEntity> findByDevice(int device_id);
    
    /**
     * @brief 활성화된 가상포인트만 조회
     */
    std::vector<VirtualPointEntity> findEnabled();
    
    /**
     * @brief 카테고리별 가상포인트 조회
     */
    std::vector<VirtualPointEntity> findByCategory(const std::string& category);
    
    /**
     * @brief 실행 타입별 가상포인트 조회
     */
    std::vector<VirtualPointEntity> findByExecutionType(const std::string& execution_type);
    
    /**
     * @brief 태그로 가상포인트 검색
     */
    std::vector<VirtualPointEntity> findByTag(const std::string& tag);
    
    /**
     * @brief 수식에 특정 포인트를 참조하는 가상포인트 찾기
     */
    std::vector<VirtualPointEntity> findDependents(int point_id);
    
    /**
     * @brief 가상포인트 실행 통계 업데이트
     */
    bool updateExecutionStats(int id, double value, double execution_time_ms);
    
    /**
     * @brief 가상포인트 에러 상태 업데이트
     */
    bool updateError(int id, const std::string& error_message);
    
    /**
     * @brief 가상포인트 활성화/비활성화
     */
    bool setEnabled(int id, bool enabled);

protected:
    // =======================================================================
    // 내부 헬퍼 메서드들 (override 제거)
    // =======================================================================
    
    VirtualPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> mapEntityToRow(const VirtualPointEntity& entity);
    std::string getTableName() const { return "virtual_points"; }
    std::string getIdFieldName() const { return "id"; }
    bool ensureTableExists();

private:
    // =======================================================================
    // Private 헬퍼 메서드
    // =======================================================================
    
    /**
     * @brief 테이블 생성 쿼리 실행
     */
    bool createTable();
    
    /**
     * @brief JSON 문자열 필드 파싱 헬퍼
     */
    std::string parseJsonField(const std::string& json_str, const std::string& default_value = "[]") const;
    
    /**
     * @brief 캐시 키 생성
     */
    std::string generateCacheKey(int id) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_REPOSITORY_H