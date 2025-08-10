// =============================================================================
// collector/include/Database/Repositories/AlarmOccurrenceRepository.h
// PulseOne AlarmOccurrenceRepository 헤더 - AlarmRuleRepository 패턴 100% 적용
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
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (AlarmRuleRepository 패턴)
using AlarmOccurrenceEntity = PulseOne::Database::Entities::AlarmOccurrenceEntity;

/**
 * @brief Alarm Occurrence Repository 클래스 (AlarmRuleRepository 패턴 적용)
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmOccurrenceRepository() : IRepository<AlarmOccurrenceEntity>("AlarmOccurrenceRepository") {
        initializeDependencies();
    }
    
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
    
    // ❌ override 제거 - IRepository에 없는 메서드
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
     * @return 활성 상태인 알람 발생들
     */
    std::vector<AlarmOccurrenceEntity> findActive();
    
    /**
     * @brief 특정 알람 규칙의 발생들 조회
     * @param rule_id 알람 규칙 ID
     * @return 해당 규칙의 알람 발생들
     */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id);
    
    /**
     * @brief 특정 테넌트의 활성 알람들 조회
     * @param tenant_id 테넌트 ID
     * @return 해당 테넌트의 활성 알람들
     */
    std::vector<AlarmOccurrenceEntity> findActiveByTenantId(int tenant_id);
    
    /**
     * @brief 심각도별 알람 발생들 조회
     * @param severity 심각도
     * @return 해당 심각도의 알람 발생들
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(AlarmOccurrenceEntity::Severity severity);
    
    /**
     * @brief 알람 인지 처리
     * @param occurrence_id 발생 ID
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 여부
     */
    bool acknowledge(int occurrence_id, int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 해제 처리
     * @param occurrence_id 발생 ID
     * @param cleared_value 해제 값
     * @param comment 해제 코멘트
     * @return 성공 여부
     */
    bool clear(int occurrence_id, const std::string& cleared_value = "", const std::string& comment = "");
    
    /**
     * @brief 최대 ID 조회 (ID 생성용)
     * @return 최대 ID (없으면 nullopt)
     */
    std::optional<int64_t> findMaxId();

    // =======================================================================
    // 테이블 관리 (BaseEntity에 있음 - override 제거)
    // =======================================================================
    
    bool ensureTableExists();

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 행 데이터를 Entity로 변환
     * @param row 행 데이터
     * @return AlarmOccurrenceEntity 객체
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
     * @brief 심각도 enum을 문자열로 변환
     */
    std::string severityToString(AlarmOccurrenceEntity::Severity severity) const;
    
    /**
     * @brief 문자열을 심각도 enum으로 변환
     */
    AlarmOccurrenceEntity::Severity stringToSeverity(const std::string& str) const;
    
    /**
     * @brief 상태 enum을 문자열로 변환
     */
    std::string stateToString(AlarmOccurrenceEntity::State state) const;
    
    /**
     * @brief 문자열을 상태 enum으로 변환
     */
    AlarmOccurrenceEntity::State stringToState(const std::string& str) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H