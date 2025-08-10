// =============================================================================
// collector/include/Database/Repositories/AlarmOccurrenceRepository.h
// PulseOne AlarmOccurrenceRepository - DeviceRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.h
 * @brief PulseOne AlarmOccurrence Repository - DeviceRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - SQLQueries.h 상수 활용
 * - mapRowToEntity Repository에서 구현
 * - 에러 처리 및 로깅
 * - 캐싱 지원
 */

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

// 타입 별칭 정의 (DeviceRepository 패턴)
using AlarmOccurrenceEntity = PulseOne::Database::Entities::AlarmOccurrenceEntity;

/**
 * @brief Alarm Occurrence Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 알람 발생 이력별 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmOccurrenceRepository : public IRepository<AlarmOccurrenceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    AlarmOccurrenceRepository() : IRepository<AlarmOccurrenceEntity>("AlarmOccurrenceRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🚨 AlarmOccurrenceRepository initialized with DatabaseAbstractionLayer");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
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
    // 벌크 연산
    // =======================================================================
    
    std::vector<AlarmOccurrenceEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmOccurrenceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    // 🆕 AlarmOccurrenceRepository 전용 메서드 (override 없음)
    std::optional<AlarmOccurrenceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    int saveBulk(std::vector<AlarmOccurrenceEntity>& entities) override;
    int updateBulk(const std::vector<AlarmOccurrenceEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // AlarmOccurrence 전용 메서드들 (SQLQueries.h 상수 사용)
    // =======================================================================
    
    /**
     * @brief 활성 상태의 알람 발생들 조회
     * @return 활성 알람 발생 목록 (심각도 및 시간 순 정렬)
     */
    std::vector<AlarmOccurrenceEntity> findActive();
    
    /**
     * @brief 특정 알람 규칙의 발생 이력 조회
     * @param rule_id 알람 규칙 ID
     * @return 해당 규칙의 발생 이력 목록 (시간 역순)
     */
    std::vector<AlarmOccurrenceEntity> findByRuleId(int rule_id);
    
    /**
     * @brief 테넌트별 알람 발생들 조회
     * @param tenant_id 테넌트 ID
     * @param state_filter 상태 필터 (빈 문자열이면 모든 상태)
     * @return 해당 테넌트의 알람 발생 목록
     */
    std::vector<AlarmOccurrenceEntity> findByTenant(int tenant_id, const std::string& state_filter = "");
    
    /**
     * @brief 상태별 알람 발생들 조회
     * @param state 상태 ("active", "acknowledged", "cleared" 등)
     * @return 해당 상태의 알람 발생 목록
     */
    std::vector<AlarmOccurrenceEntity> findByState(const std::string& state);
    
    /**
     * @brief 심각도별 알람 발생들 조회
     * @param severity 심각도 ("critical", "high", "medium", "low", "info")
     * @return 해당 심각도의 알람 발생 목록
     */
    std::vector<AlarmOccurrenceEntity> findBySeverity(const std::string& severity);
    
    /**
     * @brief 시간 범위별 알람 발생들 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 해당 시간 범위의 알람 발생 목록
     */
    std::vector<AlarmOccurrenceEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );
    
    /**
     * @brief 최근 알람 발생들 조회
     * @param hours 시간 (기본값: 24시간)
     * @param limit 최대 개수 (기본값: 100)
     * @return 최근 알람 발생 목록
     */
    std::vector<AlarmOccurrenceEntity> findRecent(int hours = 24, int limit = 100);
    
    /**
     * @brief 특정 사용자가 인지한 알람들 조회
     * @param user_id 사용자 ID
     * @return 해당 사용자가 인지한 알람 목록
     */
    std::vector<AlarmOccurrenceEntity> findAcknowledgedBy(int user_id);
    
    // =======================================================================
    // 상태 변경 특화 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 발생 인지 처리 (단일)
     * @param occurrence_id 알람 발생 ID
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 시 true
     */
    bool acknowledgeOccurrence(int occurrence_id, int user_id, const std::string& comment = "");
    
    /**
     * @brief 알람 발생 해제 처리 (단일)
     * @param occurrence_id 알람 발생 ID
     * @param cleared_value 해제 시점 값
     * @param comment 해제 코멘트
     * @return 성공 시 true
     */
    bool clearOccurrence(int occurrence_id, const std::string& cleared_value = "", const std::string& comment = "");
    
    /**
     * @brief 여러 알람 발생 일괄 인지 처리
     * @param occurrence_ids 알람 발생 ID 목록
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 처리된 개수
     */
    int bulkAcknowledge(const std::vector<int>& occurrence_ids, int user_id, const std::string& comment = "");
    
    /**
     * @brief 여러 알람 발생 일괄 해제 처리
     * @param occurrence_ids 알람 발생 ID 목록
     * @param comment 해제 코멘트
     * @return 처리된 개수
     */
    int bulkClear(const std::vector<int>& occurrence_ids, const std::string& comment = "");
    
    // =======================================================================
    // 통계 및 분석 메서드들
    // =======================================================================
    
    /**
     * @brief 상태별 알람 발생 개수 조회
     * @return 상태별 개수 맵 {"active": 10, "acknowledged": 5, ...}
     */
    std::map<std::string, int> getStateDistribution();
    
    /**
     * @brief 심각도별 알람 발생 개수 조회
     * @return 심각도별 개수 맵 {"critical": 2, "high": 8, ...}
     */
    std::map<std::string, int> getSeverityDistribution();
    
    /**
     * @brief 시간대별 알람 발생 통계 (최근 24시간)
     * @return 시간별 통계 {"hour": "2025-08-10 14:00:00", "count": 5, "critical_count": 1, ...}
     */
    std::vector<std::map<std::string, std::string>> getHourlyStats();
    
    /**
     * @brief 알람 규칙별 발생 빈도 조회
     * @param limit 상위 N개 (기본값: 10)
     * @return 규칙별 발생 빈도 목록
     */
    std::vector<std::pair<int, int>> getTopRulesByFrequency(int limit = 10);
    
    /**
     * @brief 평균 해결 시간 계산 (활성→해제)
     * @param rule_id 특정 규칙 ID (0이면 전체)
     * @return 평균 해결 시간 (초)
     */
    double getAverageResolutionTime(int rule_id = 0);
    
    /**
     * @brief 활성 알람 개수 조회
     * @return 현재 활성 상태인 알람 개수
     */
    int getActiveCount();
    
    /**
     * @brief 전체 알람 발생 개수 조회
     * @return 전체 알람 발생 개수
     */
    int getTotalCount() override;
    
    // =======================================================================
    // 유지보수 및 관리 메서드들
    // =======================================================================
    
    /**
     * @brief 오래된 알람 발생 이력 정리
     * @param days_to_keep 보관 기간 (일 단위)
     * @return 삭제된 개수
     */
    int cleanupOldOccurrences(int days_to_keep = 90);
    
    /**
     * @brief 특정 규칙의 모든 발생 이력 삭제
     * @param rule_id 알람 규칙 ID
     * @return 삭제된 개수
     */
    int deleteAllByRuleId(int rule_id);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    AlarmOccurrenceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 SQL 파라미터 맵으로 변환
     * @param entity 변환할 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief alarm_occurrences 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 알람 발생 엔티티 유효성 검증
     * @param entity 검증할 엔티티
     * @return 유효하면 true
     */
    bool validateOccurrence(const AlarmOccurrenceEntity& entity);
    
    /**
     * @brief 문자열 이스케이프 (SQL 인젝션 방지)
     * @param str 원본 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str);
    
    /**
     * @brief 타임스탬프를 데이터베이스 형식 문자열로 변환
     * @param tp 타임포인트
     * @return 데이터베이스 형식 문자열
     */
    std::string timestampToDbString(const std::chrono::system_clock::time_point& tp);
    
    /**
     * @brief 데이터베이스 문자열을 타임스탬프로 변환
     * @param db_str 데이터베이스 형식 문자열
     * @return 타임포인트
     */
    std::chrono::system_clock::time_point dbStringToTimestamp(const std::string& db_str);
    
    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 캐시에서 엔티티 설정 (IRepository 메서드 활용)
     * @param id 엔티티 ID
     * @param entity 엔티티
     */
    void setCachedEntity([[maybe_unused]] int id, [[maybe_unused]] const AlarmOccurrenceEntity& entity) {
        // IRepository의 protected 메서드들 활용
        if (isCacheEnabled()) {
            if (logger_) {
                logger_->Debug("AlarmOccurrenceRepository::setCachedEntity - Entity cached for ID: " + std::to_string(id));
            }
        }
    }
    
    /**
     * @brief 캐시에서 엔티티 조회 (IRepository 메서드 활용)
     * @param id 엔티티 ID
     * @return 캐시된 엔티티 (있으면)
     */
    std::optional<AlarmOccurrenceEntity> getCachedEntity(int id) {
        // IRepository의 protected 메서드 활용
        return IRepository<AlarmOccurrenceEntity>::getCachedEntity(id);
    }
    
    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // 테이블 생성 쿼리 (SQLQueries.h에 없는 경우만)
    // =======================================================================
    
    static constexpr const char* CREATE_TABLE_QUERY = R"(
        CREATE TABLE IF NOT EXISTS alarm_occurrences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id INTEGER NOT NULL,
            tenant_id INTEGER NOT NULL,
            occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            trigger_value TEXT,
            trigger_condition TEXT,
            alarm_message TEXT,
            severity VARCHAR(20),
            state VARCHAR(20) DEFAULT 'active',
            acknowledged_time DATETIME,
            acknowledged_by INTEGER,
            acknowledge_comment TEXT,
            cleared_time DATETIME,
            cleared_value TEXT,
            clear_comment TEXT,
            notification_sent INTEGER DEFAULT 0,
            notification_time DATETIME,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            context_data TEXT,
            source_name VARCHAR(100),
            location VARCHAR(200),
            FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (acknowledged_by) REFERENCES users(id)
        )
    )";
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_OCCURRENCE_REPOSITORY_H