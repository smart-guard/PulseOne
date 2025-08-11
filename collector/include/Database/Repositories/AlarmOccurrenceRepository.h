// =============================================================================
// Database/Repositories/AlarmOccurrenceRepository.h - 컴파일 에러 수정 버전
// =============================================================================

#ifndef ALARM_OCCURRENCE_REPOSITORY_H
#define ALARM_OCCURRENCE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Alarm/AlarmTypes.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using AlarmOccurrenceEntity = PulseOne::Database::Entities::AlarmOccurrenceEntity;

/**
 * @brief Alarm Occurrence Repository 클래스 - 컴파일 에러 수정 버전
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // AlarmTypes.h 타입 별칭
    // =======================================================================
    using AlarmSeverity = PulseOne::Alarm::AlarmSeverity;
    using AlarmState = PulseOne::Alarm::AlarmState;

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmOccurrenceRepository();
    explicit AlarmOccurrenceRepository(std::shared_ptr<DatabaseAbstractionLayer> db_layer);
    virtual ~AlarmOccurrenceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (올바른 메서드명 사용)
    // =======================================================================
    
    std::vector<AlarmOccurrenceEntity> findAll() override;
    std::optional<AlarmOccurrenceEntity> findById(int id) override;
    bool save(AlarmOccurrenceEntity& entity) override;
    bool update(const AlarmOccurrenceEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 🔥 override 제거: 이 메서드들은 IRepository에 없음
    // =======================================================================
    
    // count() 메서드 - override 제거
    int getTotalCount();
    
    // ensureTableExists() 메서드 - override 제거  
    bool ensureTableExists();
    
    // mapRowToEntity() 메서드 - override 제거
    AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row);

    // =======================================================================
    // 벌크 연산 (IRepository에서 제공)
    // =======================================================================
    
    std::vector<AlarmOccurrenceEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmOccurrenceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    int saveBulk(std::vector<AlarmOccurrenceEntity>& entities) override;
    int updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // AlarmOccurrence 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 특정 알람 규칙의 활성 발생 조회
     */
    std::vector<AlarmOccurrenceEntity> findActiveByRuleId(int rule_id);
    
    /**
     * @brief 특정 시간 범위 내 발생 조회
     */
    std::vector<AlarmOccurrenceEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end
    );
    
    /**
     * @brief 심각도별 발생 조회
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(AlarmSeverity severity);
    
    /**
     * @brief 상태별 발생 조회
     */
    std::vector<AlarmOccurrenceEntity> findByState(AlarmState state);
    
    /**
     * @brief 특정 대상의 알람 발생 조회
     */
    std::vector<AlarmOccurrenceEntity> findByTarget(const std::string& target_type, int target_id);
    
    /**
     * @brief 알람 승인
     */
    bool acknowledge(int64_t occurrence_id, int acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 알람 해제
     */
    bool clear(int64_t occurrence_id, const std::string& cleared_value, const std::string& comment = "");
    
    /**
     * @brief 알람 억제
     */
    bool suppress(int64_t occurrence_id, const std::string& comment = "");
    
    /**
     * @brief 알람 shelve
     */
    bool shelve(int64_t occurrence_id, int shelved_by, int duration_minutes, const std::string& comment = "");

    // =======================================================================
    // 통계 및 집계
    // =======================================================================
    
    /**
     * @brief 활성 알람 개수
     */
    int getActiveAlarmCount();
    
    /**
     * @brief 심각도별 개수
     */
    std::map<AlarmSeverity, int> getCountBySeverity();
    
    /**
     * @brief 상태별 개수
     */
    std::map<AlarmState, int> getCountByState();
    
    /**
     * @brief 테넌트별 활성 알람 개수
     */
    std::map<int, int> getActiveCountByTenant();

    // =======================================================================
    // 캐시 관리 (IRepository에서 상속)
    // =======================================================================
    
    void enableCache(bool enable = true);
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;
    /**
    * @brief 조건에 맞는 첫 번째 알람 발생 조회
    */
    std::optional<AlarmOccurrenceEntity> findFirstByConditions(
    const std::vector<QueryCondition>& conditions
    );

    /**
    * @brief 활성 알람 발생들 조회
    */
    std::vector<AlarmOccurrenceEntity> findActive(std::optional<int> tenant_id = std::nullopt);

    /**
    * @brief 규칙 ID로 알람 발생 조회 (활성 필터 옵션)
    */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id, bool active_only = false);

    /**
    * @brief 테넌트별 알람 발생 조회
    */
    std::vector<AlarmOccurrenceEntity> findByTenant(int tenant_id, const std::string& state_filter = "");

    /**
    * @brief 심각도별 알람 발생 조회 (문자열 버전)
    */
    std::vector<AlarmOccurrenceEntity> findBySeverity(const std::string& severity, bool active_only = false);

    /**
    * @brief 시간 범위별 알람 발생 조회 (테넌트 필터 옵션)
    */
    std::vector<AlarmOccurrenceEntity> findByTimeRange(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end,
    std::optional<int> tenant_id = std::nullopt
    );

    /**
    * @brief 최근 알람 발생 조회
    */
    std::vector<AlarmOccurrenceEntity> findRecent(int limit = 100, std::optional<int> tenant_id = std::nullopt);

    /**
    * @brief 벌크 알람 승인
    */
    int acknowledgeBulk(const std::vector<int64_t>& occurrence_ids, int acknowledged_by, const std::string& comment = "");

    /**
    * @brief 벌크 알람 해제
    */
    int clearBulk(const std::vector<int64_t>& occurrence_ids, const std::string& comment = "");

    /**
    * @brief 알람 통계 조회
    */
    std::map<std::string, int> getAlarmStatistics(int tenant_id = 0);

    /**
    * @brief 심각도별 활성 알람 개수 조회
    */
    std::map<std::string, int> getActiveAlarmsBySeverity(int tenant_id = 0);

    /**
    * @brief 오래된 해제된 알람 정리
    */
    int cleanupOldClearedAlarms(int older_than_days = 30);

    /**
    * @brief 시간 변환 헬퍼 함수들
    */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point parseTimestamp(const std::string& str);
    std::string timePointToString(const std::chrono::system_clock::time_point& time_point) const;
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& time_str) const;
    std::optional<int> findMaxId();

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     */
    std::map<std::string, std::string> entityToParams(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief 알람 발생 유효성 검사
     */
    bool validateAlarmOccurrence(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief SQL 이스케이프 처리
     */
    std::string escapeString(const std::string& str);
    
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    std::shared_ptr<DatabaseAbstractionLayer> db_layer_;
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;
    
    // 성능 최적화용 뮤텍스
    mutable std::shared_mutex cache_mutex_;
    
    // 통계 캐시 (성능 최적화)
    mutable std::atomic<std::chrono::system_clock::time_point> stats_cache_time_;
    mutable std::map<AlarmSeverity, int> cached_severity_counts_;
    mutable std::map<AlarmState, int> cached_state_counts_;
    mutable std::atomic<int> cached_active_count_;
    
    static constexpr std::chrono::minutes STATS_CACHE_DURATION{5}; // 5분 캐시
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H