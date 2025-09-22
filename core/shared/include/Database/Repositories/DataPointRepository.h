#ifndef DATA_POINT_REPOSITORY_H
#define DATA_POINT_REPOSITORY_H

/**
 * @file DataPointRepository.h
 * @brief PulseOne DataPointRepository - 품질/알람 필드 완전 지원
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 현재 스키마 완전 반영:
 * - 품질 관리: quality_check_enabled, range_check_enabled, rate_of_change_limit
 * - 알람 관리: alarm_enabled, alarm_priority
 * - DatabaseTypes.h 사용으로 타입 경로 수정
 * - 네임스페이스 일관성 확보
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

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Database {
namespace Repositories {
    class CurrentValueRepository;

// 타입 별칭 정의
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;

/**
 * @brief DataPoint Repository 클래스 (품질/알람 필드 완전 지원)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 디바이스별 데이터포인트 조회
 * - Worker용 최적화 메서드들
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 * - 품질 관리 및 알람 필드 완전 지원
 */
class DataPointRepository : public IRepository<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DataPointRepository() : IRepository<DataPointEntity>("DataPointRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("DataPointRepository initialized with quality/alarm support");
            logger_->Info("Cache enabled: " + std::string(isCacheEnabled() ? "true" : "false"));
        }
    }
    
    virtual ~DataPointRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수)
    // =======================================================================
    
    /**
     * @brief 모든 데이터포인트 조회
     * @return 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findAll() override;
    
    /**
     * @brief ID로 데이터포인트 조회
     * @param id 데이터포인트 ID
     * @return 데이터포인트 (없으면 nullopt)
     */
    std::optional<DataPointEntity> findById(int id) override;
    
    /**
     * @brief 데이터포인트 저장/업데이트
     * @param entity 저장할 데이터포인트 (ID가 설정됨)
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
     * @param id 확인할 데이터포인트 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    /**
     * @brief 여러 ID로 데이터포인트 조회
     * @param ids ID 벡터
     * @return 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 데이터포인트 조회
     * @param conditions 조건 벡터
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이지네이션 (선택사항)
     * @return 조건에 맞는 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    /**
     * @brief 조건별 데이터포인트 개수 조회
     * @param conditions 조건 벡터
     * @return 조건에 맞는 데이터포인트 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // DataPoint 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 디바이스별 데이터포인트 조회
     * @param device_id 디바이스 ID
     * @param enabled_only true면 활성화된 것만 조회
     * @return 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByDeviceId(int device_id, bool enabled_only = false);
    
    /**
     * @brief 여러 디바이스의 데이터포인트 조회
     * @param device_ids 디바이스 ID 벡터
     * @param enabled_only true면 활성화된 것만 조회
     * @return 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only = false);
    
    /**
     * @brief 디바이스와 주소로 데이터포인트 조회
     * @param device_id 디바이스 ID
     * @param address 주소
     * @return 데이터포인트 (없으면 nullopt)
     */
    std::optional<DataPointEntity> findByDeviceAndAddress(int device_id, int address);
    
    /**
     * @brief 쓰기 가능한 데이터포인트 조회
     * @return 쓰기 가능한 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findWritablePoints();
    
    /**
     * @brief 데이터 타입별 데이터포인트 조회
     * @param data_type 데이터 타입
     * @return 해당 타입의 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByDataType(const std::string& data_type);
    
    /**
     * @brief 태그별 데이터포인트 조회
     * @param tag 태그명
     * @return 해당 태그가 있는 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByTag(const std::string& tag);
    
    /**
     * @brief 비활성화된 데이터포인트 조회
     * @return 비활성화된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findDisabledPoints();
    
    /**
     * @brief 그룹별 데이터포인트 조회
     * @param group_name 그룹명
     * @return 해당 그룹의 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByGroup(const std::string& group_name);

    // =======================================================================
    // 품질 관리 관련 메서드들 (새로 추가)
    // =======================================================================
    
    /**
     * @brief 품질 체크가 활성화된 데이터포인트 조회
     * @return 품질 체크 활성화된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findQualityCheckEnabledPoints();
    
    /**
     * @brief 범위 체크가 활성화된 데이터포인트 조회
     * @return 범위 체크 활성화된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findRangeCheckEnabledPoints();
    
    /**
     * @brief 변화율 제한이 설정된 데이터포인트 조회
     * @return 변화율 제한이 0보다 큰 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findRateOfChangeLimitedPoints();

    // =======================================================================
    // 알람 관련 메서드들 (새로 추가)
    // =======================================================================
    
    /**
     * @brief 알람이 활성화된 데이터포인트 조회
     * @return 알람 활성화된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findAlarmEnabledPoints();
    
    /**
     * @brief 알람 우선순위별 데이터포인트 조회
     * @param priority 알람 우선순위 (low, medium, high, critical)
     * @return 해당 우선순위의 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findByAlarmPriority(const std::string& priority);
    
    /**
     * @brief 고위험 알람 포인트 조회 (high, critical)
     * @return 고위험 알람 포인트 벡터
     */
    std::vector<DataPointEntity> findHighPriorityAlarmPoints();

    // =======================================================================
    // Worker 시스템 통합
    // =======================================================================
    
    /**
     * @brief Worker용 데이터포인트 조회 (현재값 포함)
     * @param device_id 디바이스 ID
     * @param enabled_only true면 활성화된 것만 조회
     * @return Structs::DataPoint 목록
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
     * @brief 전체 데이터포인트 개수
     * @return 전체 개수
     */
    int getTotalCount();
    
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
    
    /**
     * @brief 품질 관리 통계
     * @return 품질 관리 관련 통계 맵
     */
    std::map<std::string, int> getQualityManagementStats();
    
    /**
     * @brief 알람 관련 통계
     * @return 알람 관련 통계 맵
     */
    std::map<std::string, int> getAlarmStats();

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    /**
     * @brief 여러 데이터포인트 일괄 저장
     * @param entities 저장할 데이터포인트들
     * @return 저장된 개수
     */
    int saveBulk(std::vector<DataPointEntity>& entities);
    
    /**
     * @brief 여러 데이터포인트 일괄 업데이트
     * @param entities 업데이트할 데이터포인트들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<DataPointEntity>& entities);
    
    /**
     * @brief 여러 ID로 데이터포인트 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 캐시 관리 (IRepository에서 상속받은 메서드들 재정의)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 테이블 존재 확인/생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 데이터포인트 유효성 검사 (품질/알람 필드 포함)
     * @param entity 검사할 데이터포인트
     * @return 유효하면 true
     */
    bool validateDataPoint(const DataPointEntity& entity) const;
    
    /**
     * @brief DB 행을 Entity로 변환 (품질/알람 필드 포함)
     * @param row DB 행 데이터
     * @return DataPointEntity
     */
    DataPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief Entity를 DB 파라미터로 변환 (품질/알람 필드 포함)
     * @param entity 변환할 Entity
     * @return DB 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const DataPointEntity& entity);
    
    /**
     * @brief Entity와 현재값을 Struct으로 변환
     * @param entity DataPointEntity
     * @param current_values_row 현재값 DB 행
     * @return Structs::DataPoint
     */
    PulseOne::Structs::DataPoint mapToStructDataPoint(
        const DataPointEntity& entity, 
        const std::map<std::string, std::string>& current_values_row = {});
    
    /**
     * @brief JSON 문자열을 문자열 맵으로 파싱
     * @param json_str JSON 문자열
     * @param field_name 필드명 (로깅용)
     * @return 문자열 맵
     */
    std::map<std::string, std::string> parseJsonToStringMap(
        const std::string& json_str, 
        const std::string& field_name) const;

    // =======================================================================
    // 추가 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 최대 개수로 제한된 전체 조회
     * @param limit 최대 개수
     * @return 제한된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findAllWithLimit(size_t limit);
    
    /**
     * @brief Worker용 데이터포인트 조회
     * @param device_ids 디바이스 ID들 (비어있으면 모든 활성 포인트)
     * @return Worker용 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findDataPointsForWorkers(const std::vector<int>& device_ids = {});
    
    /**
     * @brief 최근 생성된 데이터포인트 조회
     * @param days 최근 일수
     * @return 최근 생성된 데이터포인트 벡터
     */
    std::vector<DataPointEntity> findRecentlyCreated(int days = 7);

    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    /// CurrentValueRepository 참조 (의존성 주입)
    std::shared_ptr<CurrentValueRepository> current_value_repo_;
    
    /// 스레드 안전성을 위한 뮤텍스
    mutable std::mutex repo_mutex_;
    
    /// 통계 캐시
    mutable std::map<std::string, int> stats_cache_;
    mutable std::chrono::steady_clock::time_point stats_cache_time_;
    static constexpr std::chrono::minutes STATS_CACHE_DURATION{5};
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_REPOSITORY_H