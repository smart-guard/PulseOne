#ifndef CURRENT_VALUE_REPOSITORY_H
#define CURRENT_VALUE_REPOSITORY_H

/**
 * @file CurrentValueRepository.h
 * @brief PulseOne Current Value Repository - 실시간 데이터 현재값 Repository (기존 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 기존 패턴 100% 준수:
 * - IRepository<CurrentValueEntity> 상속으로 캐시 자동 획득
 * - DataPointRepository와 동일한 구조/네이밍
 * - Redis ↔ RDB 양방향 저장 로직
 * - 벌크 연산 및 성능 최적화
 * 
 * 핵심 기능:
 * - Redis 실시간 버퍼링
 * - 주기적 RDB 저장
 * - 변경 감지 저장
 * - 배치 처리 최적화
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Client/RedisClient.h"  // 🔥 추가
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <set>  // 🔥 추가
#include <atomic>  // 🔥 추가

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 타입 별칭 정의 (기존 패턴)
using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;

/**
 * @brief Current Value Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
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
    virtual ~CurrentValueRepository();
    
    // =======================================================================
    // 🔥 Redis 클라이언트 초기화 (기존 패턴)
    // =======================================================================
    
    /**
     * @brief Redis 클라이언트 초기화
     * @return 성공 시 true
     */
    bool initializeRedisClient();
    
    /**
     * @brief Redis 연결 상태 확인
     * @return 연결되어 있으면 true
     */
    bool isRedisConnected() const;

    // =======================================================================
    // IRepository 인터페이스 구현 (기존 패턴)
    // =======================================================================
    
    /**
     * @brief 모든 현재값 조회
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findAll() override;
    
    /**
     * @brief ID로 현재값 조회
     * @param id 현재값 ID
     * @return 현재값 (없으면 nullopt)
     */
    std::optional<CurrentValueEntity> findById(int id) override;
    
    /**
     * @brief 현재값 저장
     * @param entity 저장할 현재값 (참조로 전달하여 ID 업데이트)
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
     * @param id 삭제할 현재값 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 현재값 존재 여부 확인
     * @param id 확인할 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    // =======================================================================
    // 벌크 연산 (성능 최적화)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 현재값들 조회
     * @param ids ID 목록
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

    // =======================================================================
    // 🔥 CurrentValue 전용 조회 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터포인트 ID로 현재값 조회
     * @param data_point_id 데이터포인트 ID
     * @return 현재값 (없으면 nullopt)
     */
    std::optional<CurrentValueEntity> findByDataPointId(int data_point_id);
    
    /**
     * @brief 여러 데이터포인트 ID로 현재값들 조회
     * @param data_point_ids 데이터포인트 ID 목록
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findByDataPointIds(const std::vector<int>& data_point_ids);
    
    /**
     * @brief 가상포인트 ID로 현재값들 조회
     * @param virtual_point_id 가상포인트 ID
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findByVirtualPointId(int virtual_point_id);
    
    /**
     * @brief 데이터 품질별 현재값들 조회
     * @param quality 데이터 품질
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findByQuality(PulseOne::Enums::DataQuality quality);
    
    /**
     * @brief 시간 범위로 현재값들 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);

    // =======================================================================
    // 🔥 Redis 연동 핵심 메서드들
    // =======================================================================
    
    /**
     * @brief Redis에서 현재값 로드
     * @param data_point_id 데이터포인트 ID
     * @return 현재값 (없으면 nullopt)
     */
    std::optional<CurrentValueEntity> loadFromRedis(int data_point_id);
    
    /**
     * @brief Redis에 현재값 저장
     * @param entity 저장할 현재값
     * @param ttl_seconds TTL 설정 (초, 0이면 무제한)
     * @return 성공 시 true
     */
    bool saveToRedis(const CurrentValueEntity& entity, int ttl_seconds = 300);
    
    /**
     * @brief Redis에서 여러 현재값들 로드 (배치)
     * @param data_point_ids 데이터포인트 ID 목록
     * @return 현재값 목록
     */
    std::vector<CurrentValueEntity> loadMultipleFromRedis(const std::vector<int>& data_point_ids);
    
    /**
     * @brief Redis에 여러 현재값들 저장 (배치)
     * @param entities 저장할 현재값들
     * @param ttl_seconds TTL 설정 (초)
     * @return 성공한 개수
     */
    int saveMultipleToRedis(const std::vector<CurrentValueEntity>& entities, int ttl_seconds = 300);
    
    /**
     * @brief Redis와 RDB 동기화
     * @param force_sync 강제 동기화 여부
     * @return 성공 시 true
     */
    bool syncRedisToRDB(bool force_sync = false);

    // =======================================================================
    // 🔥 주기적 저장 시스템
    // =======================================================================
    
    /**
     * @brief 주기적 저장 스케줄러 시작
     * @param interval_seconds 저장 주기 (초)
     * @return 성공 시 true
     */
    bool startPeriodicSaver(int interval_seconds = 60);
    
    /**
     * @brief 주기적 저장 스케줄러 중지
     */
    void stopPeriodicSaver();
    
    /**
     * @brief 주기적 저장이 실행 중인지 확인
     * @return 실행 중이면 true
     */
    bool isPeriodicSaverRunning() const;
    
    /**
     * @brief 즉시 주기적 저장 실행
     * @return 저장된 엔티티 수
     */
    int executePeriodicSave();

    // =======================================================================
    // 🔥 배치 처리 최적화
    // =======================================================================
    
    /**
     * @brief 배치 저장 (UPSERT)
     * @param entities 저장할 현재값들
     * @return 성공한 개수
     */
    int saveBatch(const std::vector<CurrentValueEntity>& entities);
    
    /**
     * @brief 배치 업데이트
     * @param entities 업데이트할 현재값들
     * @return 성공한 개수
     */
    int updateBatch(const std::vector<CurrentValueEntity>& entities);
    
    /**
     * @brief 변경 감지 저장 (데드밴드 기반)
     * @param entities 확인할 현재값들
     * @param deadband 데드밴드 값
     * @return 저장된 개수
     */
    int saveOnChange(const std::vector<CurrentValueEntity>& entities, double deadband = 0.0);

    // =======================================================================
    // 캐시 관리 메서드들 (IRepository에서 상속, 오버라이드 가능)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // 🔥 통계 및 모니터링
    // =======================================================================
    
    /**
     * @brief 저장소 통계 조회
     * @return 통계 정보 JSON
     */
    json getRepositoryStats() const;
    
    /**
     * @brief Redis 통계 조회
     * @return Redis 통계 정보
     */
    json getRedisStats() const;
    
    /**
     * @brief 성능 메트릭 조회
     * @return 성능 메트릭 정보
     */
    json getPerformanceMetrics() const;

private:
    // =======================================================================
    // Redis 클라이언트 (기존 패턴)
    // =======================================================================
    
    std::unique_ptr<RedisClient> redis_client_;     // Redis 클라이언트
    bool redis_enabled_;                            // Redis 사용 여부
    std::string redis_prefix_;                      // Redis 키 접두어
    int default_ttl_seconds_;                       // 기본 TTL
    
    // =======================================================================
    // 주기적 저장 스레드
    // =======================================================================
    
    std::unique_ptr<std::thread> periodic_save_thread_;    // 주기적 저장 스레드
    std::atomic<bool> periodic_save_running_;              // 실행 상태
    std::condition_variable periodic_save_cv_;             // 조건 변수
    std::mutex periodic_save_mutex_;                       // 뮤텍스
    int periodic_save_interval_;                           // 저장 주기 (초)
    
    // =======================================================================
    // 성능 통계
    // =======================================================================
    
    mutable std::mutex stats_mutex_;                       // 통계 뮤텍스
    mutable std::atomic<int> redis_read_count_;            // Redis 읽기 횟수
    mutable std::atomic<int> redis_write_count_;           // Redis 쓰기 횟수
    mutable std::atomic<int> db_read_count_;               // DB 읽기 횟수
    mutable std::atomic<int> db_write_count_;              // DB 쓰기 횟수
    mutable std::atomic<int> batch_save_count_;            // 배치 저장 횟수

    // =======================================================================
    // 내부 헬퍼 메서드들 (기존 패턴)
    // =======================================================================
    
    /**
     * @brief DB 쿼리 실행 (SELECT)
     */
    std::vector<std::map<std::string, std::string>> executeDatabaseQuery(const std::string& sql);
    
    /**
     * @brief DB 쿼리 실행 (INSERT/UPDATE/DELETE)
     */
    bool executeDatabaseNonQuery(const std::string& sql);
    
    /**
     * @brief 결과 행을 엔티티로 변환
     */
    CurrentValueEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 결과 집합을 엔티티 목록으로 변환
     */
    std::vector<CurrentValueEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief WHERE 절 빌드
     */
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    
    /**
     * @brief ORDER BY 절 빌드
     */
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    
    /**
     * @brief LIMIT 절 빌드
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
    
    /**
     * @brief Redis 키 생성
     */
    std::string generateRedisKey(int data_point_id) const;
    
    /**
     * @brief 주기적 저장 워커 함수
     */
    void periodicSaveWorker();
    
    /**
     * @brief 설정값 로드
     */
    void loadConfiguration();
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_REPOSITORY_H