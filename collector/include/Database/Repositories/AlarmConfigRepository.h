#ifndef ALARM_CONFIG_REPOSITORY_H
#define ALARM_CONFIG_REPOSITORY_H

/**
 * @file AlarmConfigRepository.h
 * @brief PulseOne AlarmConfigRepository - IRepository 기반 알람 설정 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/AlarmConfigEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using AlarmConfigEntity = PulseOne::Database::Entities::AlarmConfigEntity;

/**
 * @brief AlarmConfig Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 데이터포인트별 알람 설정 관리
 * - 심각도별, 상태별 알람 조회
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmConfigRepository : public IRepository<AlarmConfigEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (IRepository 초기화 포함)
     */
    AlarmConfigRepository();
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~AlarmConfigRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (자동 캐시 적용)
    // =======================================================================
    
    std::vector<AlarmConfigEntity> findAll() override;
    std::optional<AlarmConfigEntity> findById(int id) override;
    bool save(AlarmConfigEntity& entity) override;
    bool update(const AlarmConfigEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<AlarmConfigEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<AlarmConfigEntity>& entities) override;
    int updateBulk(const std::vector<AlarmConfigEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    std::vector<AlarmConfigEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;
    std::string getRepositoryName() const override { return "AlarmConfigRepository"; }

    // =======================================================================
    // 알람 설정 전용 조회 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터포인트별 알람 설정 조회
     * @param data_point_id 데이터포인트 ID
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findByDataPoint(int data_point_id);
    
    /**
     * @brief 가상 포인트별 알람 설정 조회
     * @param virtual_point_id 가상 포인트 ID
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findByVirtualPoint(int virtual_point_id);
    
    /**
     * @brief 테넌트별 알람 설정 조회
     * @param tenant_id 테넌트 ID
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 사이트별 알람 설정 조회
     * @param site_id 사이트 ID
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findBySite(int site_id);
    
    /**
     * @brief 심각도별 알람 설정 조회
     * @param severity 심각도
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findBySeverity(AlarmConfigEntity::Severity severity);
    
    /**
     * @brief 조건 타입별 알람 설정 조회
     * @param condition_type 조건 타입
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findByConditionType(AlarmConfigEntity::ConditionType condition_type);
    
    /**
     * @brief 활성 알람 설정 조회
     * @return 활성 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findActiveAlarms();
    
    /**
     * @brief 자동 확인 알람 설정 조회
     * @return 자동 확인 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findAutoAcknowledgeAlarms();
    
    /**
     * @brief 알람명으로 검색
     * @param name_pattern 알람명 패턴 (LIKE 검색)
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findByNamePattern(const std::string& name_pattern);

    // =======================================================================
    // 알람 설정 특화 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터포인트의 활성 알람 설정 조회
     * @param data_point_id 데이터포인트 ID
     * @return 활성 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findActiveAlarmsByDataPoint(int data_point_id);
    
    /**
     * @brief 알람 설정 이름 중복 확인
     * @param tenant_id 테넌트 ID
     * @param alarm_name 알람명
     * @param exclude_id 제외할 알람 설정 ID (수정 시 사용)
     * @return 중복이면 true
     */
    bool isAlarmNameTaken(int tenant_id, const std::string& alarm_name, int exclude_id = 0);
    
    /**
     * @brief 데이터포인트별 알람 설정 복사
     * @param source_data_point_id 원본 데이터포인트 ID
     * @param target_data_point_id 대상 데이터포인트 ID
     * @return 복사된 알람 설정 수
     */
    int copyAlarmsToDataPoint(int source_data_point_id, int target_data_point_id);
    
    /**
     * @brief 알람 설정 일괄 활성화/비활성화
     * @param alarm_ids 알람 설정 ID 목록
     * @param enabled 활성화 여부
     * @return 성공한 개수
     */
    int bulkToggleEnabled(const std::vector<int>& alarm_ids, bool enabled);
    
    /**
     * @brief 임계값 일괄 업데이트
     * @param alarm_ids 알람 설정 ID 목록
     * @param threshold_value 새 임계값
     * @return 성공한 개수
     */
    int bulkUpdateThreshold(const std::vector<int>& alarm_ids, double threshold_value);

    // =======================================================================
    // 알람 통계 및 분석 메서드들
    // =======================================================================
    
    /**
     * @brief 테넌트별 알람 설정 수 조회
     * @param tenant_id 테넌트 ID
     * @return 알람 설정 수
     */
    int countAlarmsByTenant(int tenant_id);
    
    /**
     * @brief 심각도별 알람 설정 수 조회
     * @return 심각도별 알람 설정 수 맵
     */
    std::map<std::string, int> getAlarmCountBySeverity();
    
    /**
     * @brief 조건 타입별 알람 설정 수 조회
     * @return 조건 타입별 알람 설정 수 맵
     */
    std::map<std::string, int> getAlarmCountByConditionType();
    
    /**
     * @brief 활성/비활성 알람 설정 수 조회
     * @return {enabled: 수, disabled: 수}
     */
    std::map<std::string, int> getAlarmStatusStats();
    
    /**
     * @brief 최근 생성된 알람 설정 목록
     * @param limit 조회할 개수 (기본값: 10)
     * @return 최근 생성 알람 설정 목록
     */
    std::vector<AlarmConfigEntity> findRecentAlarms(int limit = 10);

    // =======================================================================
    // 캐시 관리 (IRepository에서 자동 제공)
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
     * @brief SQL 결과를 AlarmConfigEntity 목록으로 변환
     * @param results SQL 실행 결과
     * @return AlarmConfigEntity 목록
     */
    std::vector<AlarmConfigEntity> mapResultsToEntities(
        const std::vector<std::map<std::string, std::string>>& results);
    
    /**
     * @brief 데이터베이스 행을 AlarmConfigEntity로 변환
     * @param row 데이터베이스 행
     * @return 변환된 AlarmConfigEntity
     */
    AlarmConfigEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief SELECT 쿼리 빌드
     * @param conditions 조건 목록
     * @param order_by 정렬 조건
     * @param pagination 페이징
     * @return 빌드된 SQL 쿼리
     */
    std::string buildSelectQuery(
        const std::vector<QueryCondition>& conditions = {},
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt);
    
    /**
     * @brief 통합 쿼리 실행 (PostgreSQL/SQLite 자동 선택)
     * @param sql SQL 쿼리
     * @return 실행 결과
     */
    std::vector<std::map<std::string, std::string>> executeDatabaseQuery(const std::string& sql);
    
    /**
     * @brief 통합 비쿼리 실행 (INSERT/UPDATE/DELETE)
     * @param sql SQL 쿼리
     * @return 성공 시 true
     */
    bool executeDatabaseNonQuery(const std::string& sql);
    
    /**
     * @brief 문자열 이스케이프 처리
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const;
    
    /**
     * @brief 현재 타임스탬프 생성
     * @return ISO 형식 타임스탬프
     */
    std::string getCurrentTimestamp() const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // ALARM_CONFIG_REPOSITORY_H