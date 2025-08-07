#ifndef DATA_POINT_REPOSITORY_H
#define DATA_POINT_REPOSITORY_H

/**
 * @file DataPointRepository.h
 * @brief PulseOne DataPointRepository - 타입 정의 문제 해결 완성본
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 타입 정의 문제 해결:
 * - DatabaseTypes.h 사용으로 타입 경로 수정
 * - 네임스페이스 일관성 확보 (PulseOne::Database 내에서 직접 사용)
 * - 불필요한 using 별칭 제거
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
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {
    class CurrentValueRepository;
// 🔥 타입 별칭 정의 수정 - Database 네임스페이스 내에서 직접 사용
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;

/**
 * @brief DataPoint Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 디바이스별 데이터포인트 조회
 * - Worker용 최적화 메서드들
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class DataPointRepository : public IRepository<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DataPointRepository() : IRepository<DataPointEntity>("DataPointRepository") {
        // 🔥 의존성 초기화를 여기서 호출
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 DataPointRepository initialized with IRepository caching system");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DataPointRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    /**
     * @brief 모든 데이터포인트 조회
     * @return 데이터포인트 목록
     */
    std::vector<DataPointEntity> findAll() override;
    
    /**
     * @brief ID로 데이터포인트 조회
     * @param id 데이터포인트 ID
     * @return 데이터포인트 (없으면 nullopt)
     */
    std::optional<DataPointEntity> findById(int id) override;
    
    /**
     * @brief 데이터포인트 저장
     * @param entity 저장할 데이터포인트 (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(DataPointEntity& entity) override;
    
    /**
     * @brief 데이터포인트 업데이트
     * @param entity 업데이트할 데이터포인트
     * @return 성공 시 true
     */
    bool update(const DataPointEntity& entity) override;
    
    /**
     * @brief ID로 데이터포인트 삭제
     * @param id 삭제할 데이터포인트 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 데이터포인트 존재 여부 확인
     * @param id 확인할 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    // =======================================================================
    // 벌크 연산 (성능 최적화)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 데이터포인트들 조회
     * @param ids ID 목록
     * @return 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이징 정보 (선택사항)
     * @return 조건에 맞는 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    /**
     * @brief 조건으로 첫 번째 데이터포인트 조회
     * @param conditions 쿼리 조건들
     * @return 첫 번째 매칭 데이터포인트 (없으면 nullopt)
     */
    std::optional<DataPointEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions);
    
    /**
     * @brief 조건에 맞는 데이터포인트 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 데이터포인트 일괄 저장
     * @param entities 저장할 데이터포인트들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     */
    int saveBulk(std::vector<DataPointEntity>& entities) override;
    
    /**
     * @brief 여러 데이터포인트 일괄 업데이트
     * @param entities 업데이트할 데이터포인트들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<DataPointEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // 캐시 관리 (IRepository에서 자동 제공 - override만 필요)
    // =======================================================================
    
    /**
     * @brief 캐시 활성화/비활성화
     * @param enabled 캐시 사용 여부
     */
    void setCacheEnabled(bool enabled) override;
    
    /**
     * @brief 캐시 상태 조회
     * @return 캐시 활성화 여부
     */
    bool isCacheEnabled() const override;
    
    /**
     * @brief 모든 캐시 삭제
     */
    void clearCache() override;
    
    /**
     * @brief 특정 데이터포인트 캐시 삭제
     * @param id 데이터포인트 ID
     */
    void clearCacheForId(int id) override;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 (hits, misses, size 등)
     */
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // 유틸리티
    // =======================================================================
    
    /**
     * @brief 전체 데이터포인트 개수 조회
     * @return 전체 개수
     */
    int getTotalCount() override;
    
    /**
     * @brief Repository 이름 조회 (디버깅용)
     * @return Repository 이름
     */
    std::string getRepositoryName() const override { return "DataPointRepository"; }

    // =======================================================================
    // DataPoint 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 제한된 개수로 데이터포인트 조회
     * @param limit 최대 개수 (0이면 전체)
     * @return 데이터포인트 목록
     */
    std::vector<DataPointEntity> findAllWithLimit(size_t limit = 0);
    
    /**
     * @brief 디바이스별 데이터포인트 조회
     * @param device_id 디바이스 ID
     * @param enabled_only 활성화된 것만 조회할지 여부
     * @return 해당 디바이스의 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByDeviceId(int device_id, bool enabled_only = true);
    
    /**
     * @brief 여러 디바이스의 데이터포인트 조회
     * @param device_ids 디바이스 ID 목록
     * @param enabled_only 활성화된 것만 조회할지 여부
     * @return 해당 디바이스들의 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only = true);
    
    /**
     * @brief 쓰기 가능한 데이터포인트 조회
     * @return 쓰기 가능한 데이터포인트 목록
     */
    std::vector<DataPointEntity> findWritablePoints();
    
    /**
     * @brief 특정 데이터 타입의 데이터포인트 조회
     * @param data_type 데이터 타입
     * @return 해당 타입의 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByDataType(const std::string& data_type);
    
    /**
     * @brief Worker용 데이터포인트 조회
     * @param device_ids 디바이스 ID 목록 (빈 경우 모든 활성 포인트)
     * @return Worker용 최적화된 데이터포인트 목록
     */
    std::vector<DataPointEntity> findDataPointsForWorkers(const std::vector<int>& device_ids = {});
    
    /**
     * @brief 디바이스와 주소로 데이터포인트 조회
     * @param device_id 디바이스 ID
     * @param address 주소
     * @return 해당 디바이스의 특정 주소 데이터포인트 (없으면 nullopt)
     */
    std::optional<DataPointEntity> findByDeviceAndAddress(int device_id, int address);
    
    /**
     * @brief 태그로 데이터포인트 조회
     * @param tag 검색할 태그
     * @return 해당 태그를 가진 데이터포인트 목록
     */
    std::vector<DataPointEntity> findByTag(const std::string& tag);
    
    /**
     * @brief 비활성 데이터포인트 조회
     * @return 비활성화된 데이터포인트 목록
     */
    std::vector<DataPointEntity> findDisabledPoints();
    
    /**
     * @brief 최근 생성된 데이터포인트 조회
     * @param days 며칠 이내 (기본 7일)
     * @return 최근 생성된 데이터포인트 목록
     */
    std::vector<DataPointEntity> findRecentlyCreated(int days = 7);
 
    /**
     * @brief 현재값이 포함된 완성된 DataPoint 조회 (WorkerFactory 전용)
     * @param device_id 디바이스 ID
     * @param enabled_only 활성화된 것만 조회할지 여부 (기본값: true)
     * @return 현재값이 포함된 완성된 DataPoint 목록
     */
    std::vector<PulseOne::Structs::DataPoint> getDataPointsWithCurrentValues(int device_id, bool enabled_only = true);
    
    /**
     * @brief CurrentValueRepository 의존성 주입
     * @param current_value_repo CurrentValueRepository 인스턴스
     */
    void setCurrentValueRepository(std::shared_ptr<CurrentValueRepository> current_value_repo);

    // =======================================================================
    // 관계 데이터 사전 로딩 (N+1 문제 해결)
    // =======================================================================
    
    /**
     * @brief 디바이스 정보 사전 로딩
     * @param data_points 데이터포인트들
     */
    void preloadDeviceInfo(std::vector<DataPointEntity>& data_points);
    
    /**
     * @brief 현재 값 사전 로딩
     * @param data_points 데이터포인트들
     */
    void preloadCurrentValues(std::vector<DataPointEntity>& data_points);
    
    /**
     * @brief 알람 설정 사전 로딩
     * @param data_points 데이터포인트들
     */
    void preloadAlarmConfigs(std::vector<DataPointEntity>& data_points);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    /**
     * @brief 디바이스별 데이터포인트 개수 통계
     * @return {device_id: count} 맵
     */
    std::map<int, int> getPointCountByDevice();
    
    /**
     * @brief 데이터 타입별 데이터포인트 개수 통계
     * @return {data_type: count} 맵
     */
    std::map<std::string, int> getPointCountByDataType();

private:
    // =======================================================================
    // 🔥 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 테이블 존재 확인/생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 데이터포인트 유효성 검사
     * @param entity 검사할 데이터포인트
     * @return 유효하면 true
     */
    bool validateDataPoint(const DataPointEntity& entity) const;

    // =======================================================================
    // 🔥 DatabaseManager 래퍼 메서드들 (핵심!)
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
      
    // =======================================================================
    // 데이터 매핑 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    DataPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const DataPointEntity& entity);
    /**
     * @brief 여러 행을 엔티티 벡터로 변환
     * @param result 쿼리 결과
     * @return 엔티티 목록
     */
    std::vector<DataPointEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    // CurrentValueRepository 의존성
    std::shared_ptr<CurrentValueRepository> current_value_repo_;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_REPOSITORY_H