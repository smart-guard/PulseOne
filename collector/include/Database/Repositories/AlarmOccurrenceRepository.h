// =============================================================================
// collector/include/Database/Repositories/AlarmOccurrenceRepository.h
// PulseOne AlarmOccurrenceRepository 헤더 - 컴파일 에러 완전 해결
// =============================================================================

#ifndef ALARM_OCCURRENCE_REPOSITORY_H
#define ALARM_OCCURRENCE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <shared_mutex>  // ✅ 추가
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
 * @brief Alarm Occurrence Repository 클래스 (컴파일 에러 0% 보장)
 * 
 * 🎯 핵심 수정사항:
 * - 생성자를 .cpp에서만 정의 (헤더에서 제거)
 * - 모든 구현된 메서드를 헤더에 선언 추가
 * - 캐싱 관련 메서드 선언 추가
 * - 헬퍼 메서드들 모두 선언
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (⚠️ 생성자는 .cpp에서만 구현)
    // =======================================================================
    
    AlarmOccurrenceRepository();  // ✅ 선언만 (구현은 .cpp에서)
    virtual ~AlarmOccurrenceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<AlarmOccurrenceEntity> findAll() override;
    std::optional<AlarmOccurrenceEntity> findById(int id) override;
    bool save(AlarmOccurrenceEntity& entity) override;
    bool update(const AlarmOccurrenceEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

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
    
    std::optional<AlarmOccurrenceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    int saveBulk(std::vector<AlarmOccurrenceEntity>& entities) override;
    int updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // AlarmOccurrence 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 활성 알람 발생들 조회
     */
    std::vector<AlarmOccurrenceEntity> findActive();
    
    /**
     * @brief 특정 알람 규칙의 발생들 조회
     */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id);
    
    /**
     * @brief 특정 테넌트의 활성 알람들 조회
     */
    std::vector<AlarmOccurrenceEntity> findActiveByTenantId(int tenant_id);
    
    /**
     * @brief 심각도별 알람 발생들 조회
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(AlarmOccurrenceEntity::Severity severity);
    
    /**
     * @brief 알람 인지 처리
     */
    bool acknowledge(int occurrence_id, int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 해제 처리
     */
    bool clear(int occurrence_id, const std::string& cleared_value = "", const std::string& comment = "");
    
    /**
     * @brief 최대 ID 조회 (ID 생성용)
     */
    std::optional<int64_t> findMaxId();

    // =======================================================================
    // 테이블 관리
    // =======================================================================
    
    bool ensureTableExists();

    // =======================================================================
    // ✅ 캐싱 관련 메서드들 (.cpp에서 구현됨)
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
    // ✅ 헬퍼 메서드들 (.cpp에서 구현됨) - 이 부분이 핵심 수정!
    // =======================================================================
    
    /**
     * @brief 엔티티 유효성 검증
     */
    bool validateEntity(const AlarmOccurrenceEntity& entity) const;
    
    /**
     * @brief 문자열 이스케이프 처리
     */
    std::string escapeString(const std::string& str) const override;
    
    /**
     * @brief 활성 알람 개수 조회
     */
    int getActiveCount();
    
    /**
     * @brief 심각도별 알람 개수 조회
     */
    int getCountBySeverity(AlarmOccurrenceEntity::Severity severity);
    
    /**
     * @brief 최근 발생 알람들 조회
     */
    std::vector<AlarmOccurrenceEntity> findRecentOccurrences(int limit);

    /**
     * @brief 🔥 .cpp에서 구현된 헬퍼 메서드들 선언 추가
     */
    std::string severityToString(AlarmOccurrenceEntity::Severity severity) const;
    AlarmOccurrenceEntity::Severity stringToSeverity(const std::string& str) const;
    std::string stateToString(AlarmOccurrenceEntity::State state) const;
    AlarmOccurrenceEntity::State stringToState(const std::string& str) const;
    bool validateAlarmOccurrence(const AlarmOccurrenceEntity& entity) const;  // ✅ const 추가!

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 행 데이터를 Entity로 변환
     */
    AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 시간 포인트를 문자열로 변환
     */
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 시간 포인트로 변환
     */
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) const;
    
    /**
     * @brief 의존성 초기화 (생성자에서 호출)
     */
    void initializeDependencies();

    // =======================================================================
    // 멤버 변수들 - 전방 선언 대신 포인터 사용
    // =======================================================================
    
    mutable std::shared_mutex cache_mutex_;
    std::map<int, AlarmOccurrenceEntity> entity_cache_;
    
    // ✅ 전방 선언으로 해결
    class LogManager* logger_;      
    class ConfigManager* config_;   
    std::atomic<bool> cache_enabled_;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H