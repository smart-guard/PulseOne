#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

/**
 * @file UserRepository.h
 * @brief PulseOne UserRepository - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔥 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - IRepository 상속으로 캐싱 자동 지원
 * - executeQuery/executeNonQuery 패턴
 * - 컴파일 에러 완전 해결
 * - BaseEntity 상속 패턴 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/UserEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseTypes.h"  // QueryCondition, OrderBy, Pagination이 여기에 정의됨
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
using UserEntity = PulseOne::Database::Entities::UserEntity;

/**
 * @brief User Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 사용자명/이메일별 조회
 * - 권한별 사용자 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class UserRepository : public IRepository<UserEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    UserRepository() : IRepository<UserEntity>("UserRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 UserRepository initialized with BaseEntity pattern");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~UserRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    std::map<std::string, int> getCacheStats() const override;
    std::vector<UserEntity> findAll() override;
    std::optional<UserEntity> findById(int id) override;
    bool save(UserEntity& entity) override;
    bool update(const UserEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (DeviceRepository 패턴 정확히 따름)
    // =======================================================================
    
    std::vector<UserEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<UserEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    // 🔥 DeviceRepository 패턴: override 제거 (IRepository에 없는 메서드들)
    int saveBulk(std::vector<UserEntity>& entities);
    int updateBulk(const std::vector<UserEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // User 특화 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 사용자명으로 조회
     * @param username 사용자명
     * @return 사용자 엔티티 (없으면 nullopt)
     */
    std::optional<UserEntity> findByUsername(const std::string& username);
    
    /**
     * @brief 이메일로 조회
     * @param email 이메일 주소
     * @return 사용자 엔티티 (없으면 nullopt)
     */
    std::optional<UserEntity> findByEmail(const std::string& email);
    
    /**
     * @brief 테넌트별 사용자 조회
     * @param tenant_id 테넌트 ID
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 역할별 사용자 조회
     * @param role 역할 (admin, engineer, operator, viewer)
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByRole(const std::string& role);
    
    /**
     * @brief 활성 사용자 조회
     * @param enabled 활성 상태
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByEnabled(bool enabled);
    
    /**
     * @brief 부서별 사용자 조회
     * @param department 부서명
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByDepartment(const std::string& department);
    
    /**
     * @brief 권한을 가진 사용자 조회
     * @param permission 권한명
     * @return 사용자 목록
     */
    std::vector<UserEntity> findByPermission(const std::string& permission);

    // =======================================================================
    // 통계 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 테넌트별 사용자 수 조회
     * @param tenant_id 테넌트 ID
     * @return 사용자 수
     */
    int countByTenant(int tenant_id);
    
    /**
     * @brief 역할별 사용자 수 조회
     * @param role 역할
     * @return 사용자 수
     */
    int countByRole(const std::string& role);
    
    /**
     * @brief 활성 사용자 수 조회
     * @return 활성 사용자 수
     */
    int countActiveUsers();

    // =======================================================================
    // 인증 관련 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 로그인 시도 기록
     * @param user_id 사용자 ID
     * @return 성공 시 true
     */
    bool recordLogin(int user_id);
    
    /**
     * @brief 사용자명 중복 확인
     * @param username 사용자명
     * @return 중복되면 true
     */
    bool isUsernameTaken(const std::string& username);
    
    /**
     * @brief 이메일 중복 확인
     * @param email 이메일
     * @return 중복되면 true
     */
    bool isEmailTaken(const std::string& email);

    // =======================================================================
    // 고급 쿼리 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 복합 조건으로 사용자 검색
     * @param tenant_id 테넌트 ID (선택)
     * @param role 역할 (선택)
     * @param enabled 활성 상태 (선택)
     * @param department 부서 (선택)
     * @return 사용자 목록
     */
    std::vector<UserEntity> searchUsers(
        const std::optional<int>& tenant_id = std::nullopt,
        const std::optional<std::string>& role = std::nullopt,
        const std::optional<bool>& enabled = std::nullopt,
        const std::optional<std::string>& department = std::nullopt
    );
    
    /**
     * @brief 마지막 로그인 기준 사용자 조회
     * @param days_ago 며칠 전
     * @return 사용자 목록
     */
    std::vector<UserEntity> findInactiveUsers(int days_ago);

protected:
    // =======================================================================
    // IRepository 헬퍼 메서드 구현 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 테이블 존재 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 엔티티 유효성 검사
     * @param entity 검사할 엔티티
     * @return 유효하면 true
     */
    bool validateEntity(const UserEntity& entity) const;
    
    /**
     * @brief DB 행을 엔티티로 매핑
     * @param row DB 행 데이터
     * @return 매핑된 엔티티
     */
    UserEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 DB 파라미터로 변환
     * @param entity 엔티티
     * @return DB 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const UserEntity& entity);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    /**
     * @brief CREATE TABLE SQL 생성
     * @return CREATE TABLE SQL
     */
    std::string getCreateTableSQL() const;
    
    /**
     * @brief 권한 JSON 파싱
     * @param permissions_json JSON 문자열
     * @return 권한 벡터
     */
    std::vector<std::string> parsePermissions(const std::string& permissions_json) const;
    
    /**
     * @brief 권한 JSON 직렬화
     * @param permissions 권한 벡터
     * @return JSON 문자열
     */
    std::string serializePermissions(const std::vector<std::string>& permissions) const;
    
    /**
     * @brief 타임스탬프 파싱
     * @param timestamp_str 타임스탬프 문자열
     * @return 타임스탬프
     */
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp_str) const;
    
    /**
     * @brief 타임스탬프 직렬화
     * @param timestamp 타임스탬프
     * @return 문자열
     */
    std::string serializeTimestamp(const std::chrono::system_clock::time_point& timestamp) const;

    // =======================================================================
    // 스레드 안전성 (DeviceRepository 패턴)
    // =======================================================================
    
    mutable std::mutex repository_mutex_;
    std::atomic<bool> table_created_{false};
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // USER_REPOSITORY_H