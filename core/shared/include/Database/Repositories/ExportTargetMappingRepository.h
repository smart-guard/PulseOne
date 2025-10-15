/**
 * @file ExportTargetMappingRepository.h
 * @brief Export Target Mapping Repository
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/Repositories/ExportTargetMappingRepository.h
 */

#ifndef EXPORT_TARGET_MAPPING_REPOSITORY_H
#define EXPORT_TARGET_MAPPING_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using ExportTargetMappingEntity = PulseOne::Database::Entities::ExportTargetMappingEntity;

/**
 * @brief Export Target Mapping Repository 클래스
 * 
 * 기능:
 * - 타겟별 포인트 매핑 관리
 * - 포인트별 타겟 매핑 조회
 * - 변환 설정 관리
 */
class ExportTargetMappingRepository : public IRepository<ExportTargetMappingEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportTargetMappingRepository() 
        : IRepository<ExportTargetMappingEntity>("ExportTargetMappingRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🔗 ExportTargetMappingRepository initialized");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~ExportTargetMappingRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<ExportTargetMappingEntity> findAll() override;
    std::optional<ExportTargetMappingEntity> findById(int id) override;
    bool save(ExportTargetMappingEntity& entity) override;
    bool update(const ExportTargetMappingEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<ExportTargetMappingEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<ExportTargetMappingEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Mapping 전용 조회 메서드
    // =======================================================================
    
    /**
     * @brief 타겟별 매핑 조회
     * @param target_id 타겟 ID
     * @return 매핑 목록
     */
    std::vector<ExportTargetMappingEntity> findByTargetId(int target_id);
    
    /**
     * @brief 포인트별 매핑 조회
     * @param point_id 포인트 ID
     * @return 매핑 목록
     */
    std::vector<ExportTargetMappingEntity> findByPointId(int point_id);
    
    /**
     * @brief 타겟+포인트 조합으로 매핑 조회
     * @param target_id 타겟 ID
     * @param point_id 포인트 ID
     * @return 매핑 (optional)
     */
    std::optional<ExportTargetMappingEntity> findByTargetAndPoint(int target_id, int point_id);
    
    /**
     * @brief 활성화된 매핑만 조회
     * @param target_id 타겟 ID
     * @return 매핑 목록
     */
    std::vector<ExportTargetMappingEntity> findEnabledByTargetId(int target_id);
    
    /**
     * @brief 타겟 삭제 시 관련 매핑 모두 삭제
     * @param target_id 타겟 ID
     * @return 삭제된 개수
     */
    int deleteByTargetId(int target_id);
    
    /**
     * @brief 포인트 삭제 시 관련 매핑 모두 삭제
     * @param point_id 포인트 ID
     * @return 삭제된 개수
     */
    int deleteByPointId(int point_id);

    // =======================================================================
    // 통계
    // =======================================================================
    
    /**
     * @brief 타겟별 매핑 개수
     */
    int countByTargetId(int target_id);
    
    /**
     * @brief 포인트별 매핑 개수
     */
    int countByPointId(int point_id);

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    ExportTargetMappingEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const ExportTargetMappingEntity& entity);
    bool validateMapping(const ExportTargetMappingEntity& entity);
    bool ensureTableExists();
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_TARGET_MAPPING_REPOSITORY_H