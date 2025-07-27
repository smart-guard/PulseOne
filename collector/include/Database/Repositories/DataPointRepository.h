#ifndef DATA_POINT_REPOSITORY_H
#define DATA_POINT_REPOSITORY_H

/**
 * @file DataPointRepository.h
 * @brief PulseOne 데이터포인트 Repository - 최종 수정 버전
 * @author PulseOne Development Team
 * @date 2025-07-27
 * 
 * 🔥 수정사항:
 * - 메서드 오버라이드 충돌 해결
 * - 존재하지 않는 메서드 제거
 * - 모든 순수 가상 함수 구현
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
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

// 🔥 타입 별칭 정의
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;
using QueryCondition = PulseOne::Database::QueryCondition;
using OrderBy = PulseOne::Database::OrderBy;
using Pagination = PulseOne::Database::Pagination;

/**
 * @brief DataPoint Repository 클래스 (INTEGER ID 기반)
 */
class DataPointRepository : public IRepository<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DataPointRepository();
    virtual ~DataPointRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (🔥 정확한 시그니처)
    // =======================================================================
    
    /**
     * @brief 모든 데이터포인트 조회 (IRepository 인터페이스)
     */
    std::vector<DataPointEntity> findAll() override;
    
    /**
     * @brief ID로 데이터포인트 조회
     */
    std::optional<DataPointEntity> findById(int id) override;
    
    /**
     * @brief 데이터포인트 저장
     */
    bool save(DataPointEntity& entity) override;
    
    /**
     * @brief 데이터포인트 업데이트
     */
    bool update(const DataPointEntity& entity) override;
    
    /**
     * @brief ID로 데이터포인트 삭제
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 데이터포인트 존재 여부 확인
     */
    bool exists(int id) override;
    
    /**
     * @brief 여러 ID로 데이터포인트들 조회
     */
    std::vector<DataPointEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 조회
     */
    std::vector<DataPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    /**
     * @brief 조건으로 첫 번째 데이터포인트 조회
     */
    std::optional<DataPointEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 조건에 맞는 데이터포인트 개수 조회
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 데이터포인트 일괄 저장
     */
    int saveBulk(std::vector<DataPointEntity>& entities) override;
    
    /**
     * @brief 여러 데이터포인트 일괄 업데이트
     */
    int updateBulk(const std::vector<DataPointEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     */
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // 유틸리티
    // =======================================================================
    
    int getTotalCount() override;
    std::string getRepositoryName() const override;

    // =======================================================================
    // DataPoint 전용 메서드들 (🔥 오버라이드 없음)
    // =======================================================================
    
    /**
     * @brief 제한된 개수로 데이터포인트 조회
     */
    std::vector<DataPointEntity> findAllWithLimit(size_t limit = 0);
    
    /**
     * @brief 디바이스별 데이터포인트 조회
     */
    std::vector<DataPointEntity> findByDeviceId(int device_id, bool enabled_only = true);
    
    /**
     * @brief 여러 디바이스의 데이터포인트 조회
     */
    std::vector<DataPointEntity> findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only = true);
    
    /**
     * @brief 쓰기 가능한 데이터포인트 조회
     */
    std::vector<DataPointEntity> findWritablePoints();
    
    /**
     * @brief 특정 데이터 타입의 데이터포인트 조회
     */
    std::vector<DataPointEntity> findByDataType(const std::string& data_type);
    
    /**
     * @brief Worker용 데이터포인트 조회
     */
    std::vector<DataPointEntity> findDataPointsForWorkers(const std::vector<int>& device_ids = {});
    
    /**
     * @brief 디바이스와 주소로 데이터포인트 조회
     */
    std::optional<DataPointEntity> findByDeviceAndAddress(int device_id, int address);
    
    /**
     * @brief 태그로 데이터포인트 조회
     */
    std::vector<DataPointEntity> findByTag(const std::string& tag);
    
    /**
     * @brief 비활성 데이터포인트 조회
     */
    std::vector<DataPointEntity> findDisabledPoints();
    
    /**
     * @brief 최근 생성된 데이터포인트 조회
     */
    std::vector<DataPointEntity> findRecentlyCreated(int days = 7);

    // =======================================================================
    // 관계 데이터 사전 로딩
    // =======================================================================
    
    void preloadDeviceInfo(std::vector<DataPointEntity>& data_points);
    void preloadCurrentValues(std::vector<DataPointEntity>& data_points);
    void preloadAlarmConfigs(std::vector<DataPointEntity>& data_points);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::map<int, int> getPointCountByDevice();
    std::map<std::string, int> getPointCountByDataType();

private:
    // =======================================================================
    // 내부 멤버 변수들
    // =======================================================================
    
    DatabaseManager& db_manager_;
    ConfigManager& config_manager_;
    LogManager& logger_;
    
    // 캐싱 관련
    mutable std::mutex cache_mutex_;
    bool cache_enabled_;
    std::map<int, DataPointEntity> entity_cache_;
    
    // 캐시 통계
    mutable std::map<std::string, int> cache_stats_;
    
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    DataPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::optional<DataPointEntity> getFromCache(int id) const;
    void putToCache(const DataPointEntity& entity);
    std::vector<DataPointEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    void updateCacheStats(const std::string& operation) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_REPOSITORY_H