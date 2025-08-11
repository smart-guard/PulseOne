// =============================================================================
// Database/Repositories/AlarmOccurrenceRepository.h - 완전 수정 버전
// =============================================================================

#ifndef ALARM_OCCURRENCE_REPOSITORY_H
#define ALARM_OCCURRENCE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/DatabaseAbstractionLayer.h"  // 🔥 누락된 헤더 추가
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
 * @brief Alarm Occurrence Repository 클래스 - 완전 수정 버전
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 🔥 AlarmTypes.h 타입 별칭 (올바른 네임스페이스 사용)
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
    bool deleteById(int id) override;  // 🔥 remove → deleteById
    bool exists(int id) override;
    int count() override;

    // =======================================================================
    // 🔥 벌크 연산 (구현부와 일치하는 시그니처)
    // =======================================================================
    
    std::vector<AlarmOccurrenceEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmOccurrenceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    std::optional<AlarmOccurrenceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    int saveBulk(std::vector<AlarmOccurrenceEntity>& entities) override;
    int updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // 🔥 AlarmOccurrence 전용 메서드들 (구현부와 일치하는 시그니처)
    // =======================================================================
    
    /**
     * @brief 활성 알람 발생들 조회 (구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findActive(std::optional<int> tenant_id = std::nullopt);
    
    /**
     * @brief 특정 알람 규칙의 발생들 조회 (구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id, bool active_only = false);
    
    /**
     * @brief 특정 테넌트의 활성 알람들 조회
     */
    std::vector<AlarmOccurrenceEntity> findActiveByTenantId(int tenant_id);
    
    /**
     * @brief 심각도별 알람 발생들 조회 (AlarmSeverity enum 버전)
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(AlarmSeverity severity);
    
    /**
     * @brief 심각도별 알람 발생들 조회 (문자열 버전, 구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(const std::string& severity, bool active_only = false);
    
    /**
     * @brief 테넌트별 알람 조회 (구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findByTenant(int tenant_id, const std::string& state_filter = "");
    
    /**
     * @brief 시간 범위 내 알람 조회 (구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time,
        std::optional<int> tenant_id = std::nullopt);
    
    /**
     * @brief 최근 알람들 조회 (구현부와 일치)
     */
    std::vector<AlarmOccurrenceEntity> findRecent(int limit, std::optional<int> tenant_id = std::nullopt);
    
    /**
     * @brief 알람 인지 처리 (구현부와 일치 - int64_t 사용)
     */
    bool acknowledge(int64_t occurrence_id, int acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 알람 해제 처리 (구현부와 일치 - int64_t 사용)
     */
    bool clear(int64_t occurrence_id, const std::string& cleared_value = "", const std::string& comment = "");
    
    /**
     * @brief 알람 억제 처리 (구현부와 일치)
     */
    bool suppress(int64_t occurrence_id, const std::string& comment = "");
    
    /**
     * @brief 대량 알람 인지 처리 (구현부와 일치)
     */
    int acknowledgeBulk(const std::vector<int64_t>& occurrence_ids, int acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 대량 알람 해제 처리 (구현부와 일치)
     */
    int clearBulk(const std::vector<int64_t>& occurrence_ids, const std::string& comment = "");
    
    /**
     * @brief 알람 통계 조회 (구현부와 일치)
     */
    std::map<std::string, int> getAlarmStatistics(int tenant_id = 0);
    
    /**
     * @brief 심각도별 활성 알람 개수 조회 (구현부와 일치)
     */
    std::map<std::string, int> getActiveAlarmsBySeverity(int tenant_id = 0);
    
    /**
     * @brief 오래된 해제 알람 정리 (구현부와 일치)
     */
    int cleanupOldClearedAlarms(int older_than_days);
    
    /**
     * @brief 최대 ID 조회 (ID 생성용)
     */
    std::optional<int64_t> findMaxId();

    // =======================================================================
    // 테이블 관리
    // =======================================================================
    
    bool ensureTableExists() override;

    // =======================================================================
    // 캐싱 관련 메서드들
    // =======================================================================
    
    /**
     * @brief 엔티티를 캐시에 저장
     */
    void setCachedEntity(int id, const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief 캐시에서 엔티티 조회
     */
    std::optional<AlarmOccurrenceEntity> getCachedEntity(int id) const;
    
    /**
     * @brief 특정 ID의 캐시 삭제
     */
    void clearCacheForId(int id);
    
    /**
     * @brief 전체 캐시 삭제
     */
    void clearCache();
    
    /**
     * @brief 캐시 활성화 여부 확인
     */
    bool isCacheEnabled() const;

    // =======================================================================
    // 헬퍼 메서드들 - AlarmTypes.h 타입 사용
    // =======================================================================
    
    /**
     * @brief 엔티티 유효성 검증
     */
    bool validateEntity(const AlarmOccurrenceEntity& entity) const;
    
    /**
     * @brief 문자열 이스케이프 처리 (const 추가)
     */
    std::string escapeString(const std::string& str) const override;
    
    /**
     * @brief 활성 알람 개수 조회
     */
    int getActiveCount();
    
    /**
     * @brief 심각도별 알람 개수 조회 - AlarmTypes.h 타입 사용
     */
    int getCountBySeverity(AlarmSeverity severity);
    
    /**
     * @brief 최근 발생 알람들 조회
     */
    std::vector<AlarmOccurrenceEntity> findRecentOccurrences(int limit);

    /**
     * @brief AlarmTypes.h 타입 변환 헬퍼 메서드들 (const 추가)
     */
    std::string severityToString(AlarmSeverity severity) const;
    AlarmSeverity stringToSeverity(const std::string& str) const;
    std::string stateToString(AlarmState state) const;
    AlarmState stringToState(const std::string& str) const;
    bool validateAlarmOccurrence(const AlarmOccurrenceEntity& entity) const;  // 🔥 const 추가

private:
    // =======================================================================
    // 🔥 내부 헬퍼 메서드들 (const 추가)
    // =======================================================================
    
    /**
     * @brief 행 데이터를 Entity로 변환
     */
    AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row) override;
    
    /**
     * @brief Entity를 파라미터 맵으로 변환 (구현부에 있음)
     */
    std::map<std::string, std::string> entityToParams(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief 시간 포인트를 문자열로 변환 (const 추가)
     */
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 시간 포인트로 변환 (const 추가)
     */
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) const;
    
    /**
     * @brief 의존성 초기화 (생성자에서 호출)
     */
    void initializeDependencies();

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    mutable std::shared_mutex cache_mutex_;
    std::map<int, AlarmOccurrenceEntity> entity_cache_;
    
    // 전방 선언으로 해결
    class LogManager* logger_;      
    class ConfigManager* config_;   
    std::atomic<bool> cache_enabled_;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H