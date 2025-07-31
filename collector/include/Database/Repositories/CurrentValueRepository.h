#ifndef CURRENT_VALUE_REPOSITORY_H
#define CURRENT_VALUE_REPOSITORY_H

/**
 * @file CurrentValueRepository.h
 * @brief PulseOne Current Value Repository - DataPointRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DataPointRepository 패턴 완전 적용:
 * - IRepository<CurrentValueEntity> 상속으로 캐시 자동 획득
 * - INTEGER ID 기반 CRUD 연산 (point_id)
 * - 동일한 구조/네이밍 패턴
 * - saveBulk/updateBulk/deleteByIds 메서드 사용 (Batch 아님)
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
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

// 🔥 타입 별칭 정의 (DataPointRepository 패턴)
using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;

/**
 * @brief Current Value Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산 (point_id)
 * - Redis ↔ RDB 양방향 저장
 * - 실시간 데이터 버퍼링
 * - 주기적/변경감지 저장
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class CurrentValueRepository : public IRepository<CurrentValueEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    CurrentValueRepository() : IRepository<CurrentValueEntity>("CurrentValueRepository") {
        // 🔥 의존성 초기화를 여기서 호출
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 CurrentValueRepository initialized with IRepository caching system");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~CurrentValueRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (DataPointRepository와 동일)
    // =======================================================================
    
    /**
     * @brief 모든 현재값 조회
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findAll() override;
    
    /**
     * @brief ID로 현재값 조회
     * @param id 데이터포인트 ID (point_id)
     * @return 현재값 (없으면 nullopt)
     */
    std::optional<CurrentValueEntity> findById(int id) override;
    
    /**
     * @brief 현재값 저장 (upsert)
     * @param entity 저장할 현재값 (참조로 전달)
     * @return 성공 시 true
     */
    bool save(CurrentValueEntity& entity) override;
    
    /**
     * @brief 현재값 업데이트
     * @param entity 업데이트할 현재값
     * @return 성공 시 true
     */
    bool update(const CurrentValueEntity& entity) override;
    
    /**
     * @brief ID로 현재값 삭제
     * @param id 삭제할 데이터포인트 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 현재값 존재 여부 확인
     * @param id 확인할 데이터포인트 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    // =======================================================================
    // 벌크 연산 (성능 최적화) - DataPointRepository 패턴
    // =======================================================================
    
    /**
     * @brief 여러 ID로 현재값들 조회
     * @param ids 데이터포인트 ID 목록
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이징 정보 (선택사항)
     * @return 조건에 맞는 현재값 목록
     */
    std::vector<CurrentValueEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    /**
     * @brief 조건으로 개수 조회
     * @param conditions 쿼리 조건들
     * @return 조건 만족 현재값 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 현재값 일괄 저장 (DataPointRepository 패턴)
     * @param entities 저장할 현재값들
     * @return 저장된 개수
     */
    int saveBulk(std::vector<CurrentValueEntity>& entities) override;
    
    /**
     * @brief 여러 현재값 일괄 업데이트 (DataPointRepository 패턴)
     * @param entities 업데이트할 현재값들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<CurrentValueEntity>& entities) override;
    
    /**
     * @brief 여러 ID로 일괄 삭제 (DataPointRepository 패턴)
     * @param ids 삭제할 데이터포인트 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // 캐시 관리 (IRepository에서 자동 제공) - DataPointRepository 패턴
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
     * @brief 특정 현재값 캐시 삭제
     * @param id 데이터포인트 ID
     */
    void clearCacheForId(int id) override;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 (hits, misses, size 등)
     */
    std::map<std::string, int> getCacheStats() const override;
    
    /**
     * @brief 전체 현재값 개수 조회
     * @return 전체 개수
     */
    int getTotalCount() override;
    
    /**
     * @brief Repository 이름 반환
     * @return Repository 이름
     */
    std::string getRepositoryName() const override { return "CurrentValueRepository"; }

    // =======================================================================
    // CurrentValue 전용 메서드들 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 여러 데이터포인트의 현재값 조회
     * @param point_ids 데이터포인트 ID 목록
     * @return 해당 포인트들의 현재값 목록
     */
    std::vector<CurrentValueEntity> findByDataPointIds(const std::vector<int>& point_ids);
    
    std::optional<CurrentValueEntity> findByDataPointId(int data_point_id);
    /**
     * @brief 특정 품질의 현재값들 조회
     * @param quality 데이터 품질
     * @return 해당 품질의 현재값 목록
     */
    std::vector<CurrentValueEntity> findByQuality(PulseOne::Enums::DataQuality quality);
    
    /**
     * @brief 시간 범위로 현재값 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 해당 시간 범위의 현재값 목록
     */
    std::vector<CurrentValueEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);
    
    /**
     * @brief 최근 업데이트된 현재값들 조회
     * @param minutes 몇 분 이내
     * @return 최근 업데이트된 현재값 목록
     */
    std::vector<CurrentValueEntity> findRecentlyUpdated(int minutes = 10);
    
    /**
     * @brief 오래된 현재값들 조회 (정리용)
     * @param hours 몇 시간 이전
     * @return 오래된 현재값 목록
     */
    std::vector<CurrentValueEntity> findStaleValues(int hours = 24);
    
    /**
     * @brief 나쁜 품질의 현재값들 조회
     * @return BAD/UNCERTAIN/NOT_CONNECTED 품질의 현재값 목록
     */
    std::vector<CurrentValueEntity> findBadQualityValues();
    
    /**
     * @brief 현재값 통계 정보
     * @return 통계 정보 맵
     */
    std::map<std::string, int> getStatistics();
    
    /**
     * @brief 현재값 대량 upsert (성능 최적화)
     * @param entities 저장할 현재값들
     * @return 성공 개수
     */
    int bulkUpsert(std::vector<CurrentValueEntity>& entities);

private:
    // =======================================================================
    // 의존성 초기화 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 의존성 초기화
     */
    void initializeDependencies() {
        // 싱글톤 인스턴스들 초기화
        db_manager_ = &DatabaseManager::getInstance();
        config_manager_ = &ConfigManager::getInstance();
        logger_ = &LogManager::getInstance();
        
        // 테이블 존재 여부 확인 및 생성
        ensureTableExists();
    }
    
    // =======================================================================
    // Private 헬퍼 메서드들 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 테이블 존재 여부 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 현재값 엔티티 유효성 검사
     * @param entity 검사할 엔티티
     * @return 유효하면 true
     */
    bool validateCurrentValue(const CurrentValueEntity& entity) const;
    
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
    // 데이터 매핑 헬퍼 메서드들 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    CurrentValueEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 행을 엔티티 벡터로 변환
     * @param result 쿼리 결과
     * @return 엔티티 목록
     */
    std::vector<CurrentValueEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     * @param entity 변환할 엔티티
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const CurrentValueEntity& entity);
    
    // =======================================================================
    // SQL 빌더 헬퍼 메서드들 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief WHERE 절 생성
     * @param conditions 조건 목록
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
     * @param pagination 페이징 조건
     * @return LIMIT 절 문자열
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
    
    // =======================================================================
    // 유틸리티 헬퍼 메서드들 (DataPointRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 문자열 이스케이프
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const;

    // =======================================================================
    // 데이터 멤버들 (DataPointRepository 패턴)
    // =======================================================================
    
    // 외부 의존성들 (포인터)
    DatabaseManager* db_manager_ = nullptr;
    ConfigManager* config_manager_ = nullptr;
    LogManager* logger_ = nullptr;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_REPOSITORY_H