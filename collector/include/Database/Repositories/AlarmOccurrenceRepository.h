#ifndef ALARM_OCCURRENCE_REPOSITORY_H
#define ALARM_OCCURRENCE_REPOSITORY_H

/**
 * @file AlarmOccurrenceRepository.h
 * @brief PulseOne 알람 발생 이력 Repository - DeviceRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🔥 DeviceRepository 패턴 100% 준수:
 * - IRepository<AlarmOccurrenceEntity> 상속
 * - DatabaseAbstractionLayer 사용
 * - SQL::AlarmOccurrence 네임스페이스 사용
 * - 캐싱 지원
 * - 에러 처리 및 로깅
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/QueryTypes.h"
#include <vector>
#include <optional>
#include <string>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

/**
 * @brief 알람 발생 이력 Repository 클래스
 */
class AlarmOccurrenceRepository : public IRepository<Entities::AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    AlarmOccurrenceRepository();
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~AlarmOccurrenceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수)
    // =======================================================================
    
    /**
     * @brief 모든 알람 발생 조회
     * @return 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findAll() override;
    
    /**
     * @brief ID로 알람 발생 조회
     * @param id 알람 발생 ID
     * @return 알람 발생 (없으면 nullopt)
     */
    std::optional<Entities::AlarmOccurrenceEntity> findById(int id) override;
    
    /**
     * @brief 알람 발생 저장
     * @param entity 알람 발생 엔티티
     * @return 성공 시 true
     */
    bool save(Entities::AlarmOccurrenceEntity& entity) override;
    
    /**
     * @brief 알람 발생 업데이트
     * @param entity 알람 발생 엔티티
     * @return 성공 시 true
     */
    bool update(const Entities::AlarmOccurrenceEntity& entity) override;
    
    /**
     * @brief ID로 알람 발생 삭제
     * @param id 알람 발생 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 알람 발생 존재 여부 확인
     * @param id 알람 발생 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (IRepository 확장)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 알람 발생 조회
     * @param ids ID 목록
     * @return 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 검색
     * @param conditions 검색 조건
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이징 조건 (optional)
     * @return 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;

    // =======================================================================
    // 🎯 알람 발생 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 첫 번째 조건 매칭 알람 발생 조회
     * @param conditions 검색 조건
     * @return 첫 번째 알람 발생 (없으면 nullopt)
     */
    std::optional<Entities::AlarmOccurrenceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    /**
     * @brief 활성 상태 알람 발생 조회
     * @return 활성 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findActive();
    
    /**
     * @brief 규칙 ID로 알람 발생 조회
     * @param rule_id 알람 규칙 ID
     * @return 해당 규칙의 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByRuleId(int rule_id);
    
    /**
     * @brief 테넌트 ID로 알람 발생 조회
     * @param tenant_id 테넌트 ID
     * @return 해당 테넌트의 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByTenantId(int tenant_id);
    
    /**
     * @brief 심각도별 알람 발생 조회
     * @param severity 심각도
     * @return 해당 심각도의 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findBySeverity(
        Entities::AlarmOccurrenceEntity::Severity severity
    );
    
    /**
     * @brief 상태별 알람 발생 조회
     * @param state 상태
     * @return 해당 상태의 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByState(
        Entities::AlarmOccurrenceEntity::State state
    );
    
    /**
     * @brief 기간별 알람 발생 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 해당 기간의 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );
    
    /**
     * @brief 인지되지 않은 알람 발생 조회
     * @return 인지되지 않은 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findUnacknowledged();
    
    /**
     * @brief 해제되지 않은 알람 발생 조회
     * @return 해제되지 않은 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findUncleared();
    
    /**
     * @brief 알림이 전송되지 않은 알람 발생 조회
     * @return 알림 미전송 알람 발생 목록
     */
    std::vector<Entities::AlarmOccurrenceEntity> findPendingNotification();

    // =======================================================================
    // 🎯 통계 및 집계 메서드들
    // =======================================================================
    
    /**
     * @brief 규칙별 알람 발생 횟수
     * @param rule_id 알람 규칙 ID
     * @return 발생 횟수
     */
    int countByRuleId(int rule_id);
    
    /**
     * @brief 테넌트별 알람 발생 횟수
     * @param tenant_id 테넌트 ID
     * @return 발생 횟수
     */
    int countByTenantId(int tenant_id);
    
    /**
     * @brief 심각도별 알람 발생 횟수
     * @param severity 심각도
     * @return 발생 횟수
     */
    int countBySeverity(Entities::AlarmOccurrenceEntity::Severity severity);
    
    /**
     * @brief 활성 알람 발생 횟수
     * @return 활성 알람 발생 횟수
     */
    int countActive();
    
    /**
     * @brief 기간별 알람 발생 횟수
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 해당 기간의 알람 발생 횟수
     */
    int countByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );

    // =======================================================================
    // 🎯 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 발생 인지 처리
     * @param occurrence_id 알람 발생 ID
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 시 true
     */
    bool acknowledgeOccurrence(int occurrence_id, int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 발생 해제 처리
     * @param occurrence_id 알람 발생 ID
     * @param cleared_value 해제 시점의 값
     * @param comment 해제 코멘트
     * @return 성공 시 true
     */
    bool clearOccurrence(int occurrence_id, const std::string& cleared_value, const std::string& comment = "");
    
    /**
     * @brief 알림 전송 상태 업데이트
     * @param occurrence_id 알람 발생 ID
     * @param result_json 전송 결과 JSON
     * @return 성공 시 true
     */
    bool updateNotificationStatus(int occurrence_id, const std::string& result_json);
    
    /**
     * @brief 여러 알람 발생 벌크 인지 처리
     * @param occurrence_ids 알람 발생 ID 목록
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공한 개수
     */
    int acknowledgeBulk(const std::vector<int>& occurrence_ids, int user_id, const std::string& comment = "");
    
    /**
     * @brief 여러 알람 발생 벌크 해제 처리
     * @param occurrence_ids 알람 발생 ID 목록
     * @param cleared_value 해제 시점의 값
     * @param comment 해제 코멘트
     * @return 성공한 개수
     */
    int clearBulk(const std::vector<int>& occurrence_ids, const std::string& cleared_value, const std::string& comment = "");
    
    /**
     * @brief 오래된 알람 발생 정리
     * @param retention_days 보관 일수
     * @return 삭제된 개수
     */
    int cleanupOldOccurrences(int retention_days);

    // =======================================================================
    // 🎯 테이블 관리 (IRepository 확장)
    // =======================================================================
    
    /**
     * @brief 테이블 존재 여부 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists() override;
    
    /**
     * @brief 테이블 스키마 업데이트
     * @return 성공 시 true
     */
    bool updateTableSchema();

private:
    // =======================================================================
    // 🎯 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief DB 행을 엔티티로 변환
     * @param row DB 행 데이터
     * @return 알람 발생 엔티티
     */
    Entities::AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 시간을 문자열로 변환
     * @param tp 시간
     * @return 문자열
     */
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 시간으로 변환
     * @param str 문자열
     * @return 시간
     */
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) const;
    
    /**
     * @brief Severity 열거형을 문자열로 변환
     * @param severity 심각도
     * @return 문자열
     */
    std::string severityToString(Entities::AlarmOccurrenceEntity::Severity severity) const;
    
    /**
     * @brief State 열거형을 문자열로 변환
     * @param state 상태
     * @return 문자열
     */
    std::string stateToString(Entities::AlarmOccurrenceEntity::State state) const;
    
    /**
     * @brief WHERE 절 조건 생성
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
     * @brief LIMIT/OFFSET 절 생성
     * @param pagination 페이징 조건
     * @return LIMIT/OFFSET 절 문자열
     */
    std::string buildPaginationClause(const std::optional<Pagination>& pagination) const;

    // =======================================================================
    // 🎯 멤버 변수들
    // =======================================================================
    
    DatabaseAbstractionLayer db_layer_;  // DB 추상화 계층
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H