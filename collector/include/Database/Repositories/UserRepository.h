#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

/**
 * @file UserRepository.h
 * @brief PulseOne UserRepository - IRepository 기반 사용자 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/UserEntity.h"
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
using UserEntity = PulseOne::Database::Entities::UserEntity;

/**
 * @brief User Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 사용자 인증 및 권한 관리
 * - 테넌트별 사용자 조회
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class UserRepository : public IRepository<UserEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (IRepository 초기화 포함)
     */
    UserRepository();
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~UserRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (자동 캐시 적용)
    // =======================================================================
    
    std::vector<UserEntity> findAll() override;
    std::optional<UserEntity> findById(int id) override;
    bool save(UserEntity& entity) override;
    bool update(const UserEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    std::vector<UserEntity> findByIds(const std::vector<int>& ids) override;
    int saveBulk(std::vector<UserEntity>& entities) override;
    int updateBulk(const std::vector<UserEntity>& entities) override;
    int deleteByIds(const std::vector<int>& ids) override;
    
    std::vector<UserEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    int getTotalCount() override;
    std::string getRepositoryName() const override { return "UserRepository"; }

    // =======================================================================
    // 사용자 전용 조회 메서드들
    // =======================================================================
    
    /**
     * @brief 사용자명으로 사용자 조회
     * @param username 사용자명
     * @return 사용자 엔티티 (없으면 nullopt)
     */
    std::optional<UserEntity> findByUsername(const std::string& username);
    
    /**
     * @brief 이메일로 사용자 조회
     * @param email 이메일 주소
     * @return 사용자 엔티티 (없으면 nullopt)
     */
    std::optional<UserEntity> findByEmail(const std::string& email);
    
    /**
     * @brief 테넌트별 사용자 목록 조회
     * @param tenant_id 테넌트 ID
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 역할별 사용자 목록 조회
     * @param role 역할 (admin, user, viewer, operator)
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByRole(const std::string& role);
    
    /**
     * @brief 활성 사용자 목록 조회
     * @return 활성 사용자 목록
     */
    std::vector<UserEntity> findActiveUsers();
    
    /**
     * @brief 특정 권한을 가진 사용자 목록 조회
     * @param permission 권한명
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByPermission(const std::string& permission);
    
    /**
     * @brief 마지막 로그인 기간별 사용자 조회
     * @param days 일수 (예: 30일 이내)
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByLastLoginDays(int days);

    // =======================================================================
    // 사용자 인증 및 보안 메서드들
    // =======================================================================
    
    /**
     * @brief 사용자 인증 (로그인)
     * @param username 사용자명
     * @param password 비밀번호
     * @return 인증 성공 시 사용자 엔티티
     */
    std::optional<UserEntity> authenticate(const std::string& username, const std::string& password);
    
    /**
     * @brief 사용자명 중복 확인
     * @param username 확인할 사용자명
     * @param exclude_id 제외할 사용자 ID (수정 시 사용)
     * @return 중복이면 true
     */
    bool isUsernameTaken(const std::string& username, int exclude_id = 0);
    
    /**
     * @brief 이메일 중복 확인
     * @param email 확인할 이메일
     * @param exclude_id 제외할 사용자 ID (수정 시 사용)
     * @return 중복이면 true
     */
    bool isEmailTaken(const std::string& email, int exclude_id = 0);
    
    /**
     * @brief 비밀번호 재설정
     * @param user_id 사용자 ID
     * @param new_password 새 비밀번호
     * @return 성공 시 true
     */
    bool resetPassword(int user_id, const std::string& new_password);

    // =======================================================================
    // 사용자 통계 및 분석 메서드들
    // =======================================================================
    
    /**
     * @brief 테넌트별 사용자 수 조회
     * @param tenant_id 테넌트 ID
     * @return 사용자 수
     */
    int countUsersByTenant(int tenant_id);
    
    /**
     * @brief 역할별 사용자 수 조회
     * @return 역할별 사용자 수 맵
     */
    std::map<std::string, int> getUserCountByRole();
    
    /**
     * @brief 활성/비활성 사용자 수 조회
     * @return {active: 수, inactive: 수}
     */
    std::map<std::string, int> getUserStatusStats();
    
    /**
     * @brief 최근 가입한 사용자 목록
     * @param limit 조회할 개수 (기본값: 10)
     * @return 최근 가입 사용자 목록
     */
    std::vector<UserEntity> findRecentUsers(int limit = 10);

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
     * @brief SQL 결과를 UserEntity 목록으로 변환
     * @param results SQL 실행 결과
     * @return UserEntity 목록
     */
    std::vector<UserEntity> mapResultsToEntities(
        const std::vector<std::map<std::string, std::string>>& results);
    
    /**
     * @brief 데이터베이스 행을 UserEntity로 변환
     * @param row 데이터베이스 행
     * @return 변환된 UserEntity
     */
    UserEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
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

#endif // USER_REPOSITORY_H