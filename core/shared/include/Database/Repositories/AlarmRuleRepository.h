// =============================================================================
// collector/include/Database/Repositories/AlarmRuleRepository.h
// PulseOne AlarmRuleRepository - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

#ifndef ALARM_RULE_REPOSITORY_H
#define ALARM_RULE_REPOSITORY_H

/**
 * @file AlarmRuleRepository.h
 * @brief PulseOne AlarmRuleRepository - ScriptLibraryRepository 패턴 완전 적용
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
#include "Database/Entities/AlarmRuleEntity.h"
#include "Alarm/AlarmTypes.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (ScriptLibraryRepository 패턴)
using AlarmRuleEntity = PulseOne::Database::Entities::AlarmRuleEntity;

/**
 * @brief Alarm Rule Repository 클래스 (ScriptLibraryRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 알람 규칙 관리
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class AlarmRuleRepository : public IRepository<AlarmRuleEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    AlarmRuleRepository() : IRepository<AlarmRuleEntity>("AlarmRuleRepository") {
        initializeDependencies();
        
        // ✅ 표준 LogManager 사용법
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "🚨 AlarmRuleRepository initialized with ScriptLibraryRepository pattern");
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
    }
    
    virtual ~AlarmRuleRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수!)
    // =======================================================================
    
    /**
     * @brief 모든 알람 규칙 조회
     * @return 모든 AlarmRuleEntity 목록
     */
    std::vector<AlarmRuleEntity> findAll() override;
    
    /**
     * @brief ID로 알람 규칙 조회
     * @param id 알람 규칙 ID
     * @return AlarmRuleEntity (optional)
     */
    std::optional<AlarmRuleEntity> findById(int id) override;
    
    /**
     * @brief 알람 규칙 저장
     * @param entity AlarmRuleEntity (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(AlarmRuleEntity& entity) override;
    
    /**
     * @brief 알람 규칙 업데이트
     * @param entity AlarmRuleEntity
     * @return 성공 시 true
     */
    bool update(const AlarmRuleEntity& entity) override;
    
    /**
     * @brief ID로 알람 규칙 삭제
     * @param id 알람 규칙 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 알람 규칙 존재 여부 확인
     * @param id 알람 규칙 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (IRepository 상속) - 모든 메서드 구현파일에서 구현됨
    // =======================================================================
    
    /**
     * @brief 여러 ID로 알람 규칙들 조회
     * @param ids 알람 규칙 ID 목록
     * @return AlarmRuleEntity 목록
     */
    std::vector<AlarmRuleEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건에 맞는 알람 규칙들 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이징 조건 (optional)
     * @return AlarmRuleEntity 목록
     */
    std::vector<AlarmRuleEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    /**
     * @brief 조건에 맞는 알람 규칙 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 알람 규칙 일괄 저장
     * @param entities 저장할 알람 규칙들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     */
    int saveBulk(std::vector<AlarmRuleEntity>& entities) override;
    
    /**
     * @brief 여러 알람 규칙 일괄 업데이트
     * @param entities 업데이트할 알람 규칙들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<AlarmRuleEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // AlarmRule 전용 메서드들 (구현파일에서 구현됨)
    // =======================================================================
    
    /**
     * @brief 조건에 맞는 첫 번째 알람 규칙 조회
     * @param conditions 쿼리 조건들
     * @return AlarmRuleEntity (optional)
     */
    std::optional<AlarmRuleEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions
    );
    
    /**
     * @brief 특정 대상에 대한 알람 규칙들 조회
     * @param target_type 대상 타입 ('data_point', 'virtual_point', 'group')
     * @param target_id 대상 ID
     * @param enabled_only 활성화된 것만 조회 (기본값: true)
     * @return 해당 대상의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByTarget(const std::string& target_type, int target_id, bool enabled_only = true);
    
    /**
     * @brief 테넌트별 알람 규칙들 조회
     * @param tenant_id 테넌트 ID
     * @param enabled_only 활성화된 것만 조회
     * @return 해당 테넌트의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByTenant(int tenant_id, bool enabled_only = false);
    
    /**
     * @brief 심각도별 알람 규칙들 조회
     * @param severity 심각도 문자열
     * @param enabled_only 활성화된 것만 조회
     * @return 해당 심각도의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findBySeverity(const std::string& severity, bool enabled_only = false);
    
    /**
     * @brief 알람 타입별 규칙들 조회
     * @param alarm_type 알람 타입 문자열
     * @param enabled_only 활성화된 것만 조회
     * @return 해당 타입의 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findByAlarmType(const std::string& alarm_type, bool enabled_only = false);
    
    /**
     * @brief 활성화된 모든 알람 규칙들 조회
     * @return 활성화된 알람 규칙 목록
     */
    std::vector<AlarmRuleEntity> findAllEnabled();
    
    /**
     * @brief 이름으로 알람 규칙 조회
     * @param name 알람 규칙 이름
     * @param tenant_id 테넌트 ID
     * @param exclude_id 제외할 ID (기본값: 0)
     * @return AlarmRuleEntity (optional)
     */
    std::optional<AlarmRuleEntity> findByName(const std::string& name, int tenant_id, int exclude_id = 0);
    
    /**
     * @brief 알람 규칙 이름 중복 확인
     * @param name 알람 규칙 이름
     * @param tenant_id 테넌트 ID
     * @param exclude_id 제외할 ID (기본값: 0)
     * @return 중복이면 true
     */
    bool isNameTaken(const std::string& name, int tenant_id, int exclude_id = 0);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행 맵
     * @return AlarmRuleEntity
     */
    AlarmRuleEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     * @param entity AlarmRuleEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const AlarmRuleEntity& entity);
    
    /**
     * @brief 테이블 존재 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 알람 규칙 유효성 검사
     * @param entity AlarmRuleEntity
     * @return 유효하면 true
     */
    bool validateAlarmRule(const AlarmRuleEntity& entity);
    
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

#endif // ALARM_RULE_REPOSITORY_H