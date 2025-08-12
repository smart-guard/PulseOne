// =============================================================================
// collector/include/Database/Repositories/AlarmOccurrenceRepository.h
// PulseOne AlarmOccurrenceRepository - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

#ifndef ALARM_OCCURRENCE_REPOSITORY_H
#define ALARM_OCCURRENCE_REPOSITORY_H

/**
 * @file AlarmOccurrenceRepository.h
 * @brief PulseOne AlarmOccurrenceRepository - ScriptLibraryRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 ScriptLibraryRepository 패턴 100% 적용:
 * - ExtendedSQLQueries.h 사용
 * - 표준 LogManager 사용법
 * - IRepository 상속 관계 정확히 준수
 * - 불필요한 의존성 제거
 * - 모든 구현 메서드 헤더 선언
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/DatabaseTypes.h"
#include "Alarm/AlarmTypes.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (ScriptLibraryRepository 패턴)
using AlarmOccurrenceEntity = PulseOne::Database::Entities::AlarmOccurrenceEntity;

/**
 * @brief Alarm Occurrence Repository 클래스 (ScriptLibraryRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 알람 발생 이력 관리
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    AlarmOccurrenceRepository() : IRepository<AlarmOccurrenceEntity>("AlarmOccurrenceRepository") {
        initializeDependencies();
        
        // ✅ 표준 LogManager 사용법
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "🚨 AlarmOccurrenceRepository initialized with ScriptLibraryRepository pattern");
        LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
    }
    
    virtual ~AlarmOccurrenceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수!)
    // =======================================================================
    
    /**
     * @brief 모든 알람 발생 조회
     * @return 모든 AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findAll() override;
    
    /**
     * @brief ID로 알람 발생 조회
     * @param id 알람 발생 ID
     * @return AlarmOccurrenceEntity (optional)
     */
    std::optional<AlarmOccurrenceEntity> findById(int id) override;
    
    /**
     * @brief 알람 발생 저장
     * @param entity AlarmOccurrenceEntity (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(AlarmOccurrenceEntity& entity) override;
    
    /**
     * @brief 알람 발생 업데이트
     * @param entity AlarmOccurrenceEntity
     * @return 성공 시 true
     */
    bool update(const AlarmOccurrenceEntity& entity) override;
    
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
    // 벌크 연산 (IRepository 상속) - 모든 메서드 구현파일에서 구현됨
    // =======================================================================
    
    /**
     * @brief 여러 ID로 알람 발생들 조회
     * @param ids 알람 발생 ID 목록
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건에 맞는 알람 발생들 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이징 조건 (optional)
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    /**
     * @brief 조건에 맞는 알람 발생 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 알람 발생 일괄 저장
     * @param entities 저장할 알람 발생들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     */
    int saveBulk(std::vector<AlarmOccurrenceEntity>& entities) override;
    
    /**
     * @brief 여러 알람 발생 일괄 업데이트
     * @param entities 업데이트할 알람 발생들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // AlarmOccurrence 전용 메서드들 (구현파일에서 구현됨)
    // =======================================================================
    
    /**
     * @brief 조건에 맞는 첫 번째 알람 발생 조회
     * @param conditions 쿼리 조건들
     * @return AlarmOccurrenceEntity (optional)
     */
    std::optional<AlarmOccurrenceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    /**
     * @brief 활성 알람 발생들 조회
     * @param tenant_id 테넌트 ID (optional)
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findActive(std::optional<int> tenant_id = std::nullopt);
    
    /**
     * @brief 규칙 ID로 알람 발생 조회
     * @param rule_id 알람 규칙 ID
     * @param active_only 활성 알람만 조회할지 여부
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id, bool active_only = false);
    
    /**
     * @brief 테넌트별 알람 발생 조회
     * @param tenant_id 테넌트 ID
     * @param state_filter 상태 필터 (빈 문자열이면 모든 상태)
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findByTenant(int tenant_id, const std::string& state_filter = "");
    
    /**
     * @brief 심각도별 알람 발생 조회
     * @param severity 심각도 문자열
     * @param active_only 활성 알람만 조회할지 여부
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(const std::string& severity, bool active_only = false);
    
    /**
     * @brief 시간 범위별 알람 발생 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @param tenant_id 테넌트 ID (optional)
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time,
        std::optional<int> tenant_id = std::nullopt
    );
    
    /**
     * @brief 최근 알람 발생 조회
     * @param limit 조회할 개수 (기본값: 100)
     * @param tenant_id 테넌트 ID (optional)
     * @return AlarmOccurrenceEntity 목록
     */
    std::vector<AlarmOccurrenceEntity> findRecent(int limit = 100, std::optional<int> tenant_id = std::nullopt);

    // =======================================================================
    // 알람 상태 관리 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 승인
     * @param occurrence_id 알람 발생 ID
     * @param acknowledged_by 승인자 ID
     * @param comment 승인 코멘트
     * @return 성공 시 true
     */
    bool acknowledge(int64_t occurrence_id, int acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 알람 해제
     * @param occurrence_id 알람 발생 ID
     * @param cleared_value 해제 시점 값
     * @param comment 해제 코멘트
     * @return 성공 시 true
     */
    bool clear(int64_t occurrence_id, const std::string& cleared_value, const std::string& comment = "");
    
    /**
     * @brief 알람 억제
     * @param occurrence_id 알람 발생 ID
     * @param comment 억제 코멘트
     * @return 성공 시 true
     */
    bool suppress(int64_t occurrence_id, const std::string& comment = "");
    
    /**
     * @brief 벌크 알람 승인
     * @param occurrence_ids 알람 발생 ID 목록
     * @param acknowledged_by 승인자 ID
     * @param comment 승인 코멘트
     * @return 승인된 개수
     */
    int acknowledgeBulk(const std::vector<int64_t>& occurrence_ids, int acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 벌크 알람 해제
     * @param occurrence_ids 알람 발생 ID 목록
     * @param comment 해제 코멘트
     * @return 해제된 개수
     */
    int clearBulk(const std::vector<int64_t>& occurrence_ids, const std::string& comment = "");

    // =======================================================================
    // 통계 및 분석 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 통계 조회
     * @param tenant_id 테넌트 ID (0이면 전체)
     * @return 통계 맵 (total, active, acknowledged, cleared)
     */
    std::map<std::string, int> getAlarmStatistics(int tenant_id = 0);
    
    /**
     * @brief 심각도별 활성 알람 개수 조회
     * @param tenant_id 테넌트 ID (0이면 전체)
     * @return 심각도별 개수 맵
     */
    std::map<std::string, int> getActiveAlarmsBySeverity(int tenant_id = 0);
    
    /**
     * @brief 오래된 해제된 알람 정리
     * @param older_than_days 보관 일수 (기본값: 30일)
     * @return 정리된 개수
     */
    int cleanupOldClearedAlarms(int older_than_days = 30);

    // =======================================================================
    // 유틸리티 메서드들 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 시간 포인트를 문자열로 변환
     * @param time_point 시간 포인트
     * @return 시간 문자열
     */
    std::string timePointToString(const std::chrono::system_clock::time_point& time_point) const;
    
    /**
     * @brief 문자열을 시간 포인트로 변환
     * @param time_str 시간 문자열
     * @return 시간 포인트
     */
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& time_str) const;
    
    /**
     * @brief 최대 ID 조회 (테스트용)
     * @return 최대 ID (optional)
     */
    int findMaxId();;
    std::vector<AlarmOccurrenceEntity> findActiveByRuleId(int rule_id);
private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행 맵
     * @return AlarmOccurrenceEntity
     */
    AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     * @param entity AlarmOccurrenceEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief 테이블 존재 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 알람 발생 유효성 검사
     * @param entity AlarmOccurrenceEntity
     * @return 유효하면 true
     */
    bool validateAlarmOccurrence(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief SQL 이스케이프 처리
     * @param str 원본 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str);

    // =======================================================================
    // ✅ ScriptLibraryRepository 패턴 - 멤버 변수 제거
    // =======================================================================
    // - db_layer_: IRepository에서 자동 관리
    // - logger_: LogManager::getInstance() 사용
    // - config_: ConfigManager::getInstance() 사용  
    // - 캐시 관련: IRepository에서 자동 관리
    // - 뮤텍스: IRepository에서 자동 관리
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H