#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

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

using UserEntity = PulseOne::Database::Entities::UserEntity;

class UserRepository : public IRepository<UserEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    UserRepository();
    virtual ~UserRepository() = default;
    
    // =======================================================================
    // 캐시 관리 메서드들 (한 곳에만 선언)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override;
    bool isCacheEnabled() const override;
    void clearCache() override;
    void clearCacheForId(int id) override;
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // IRepository 인터페이스 구현
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
    // 🔥 기본 사용자 조회 메서드들
    // =======================================================================
    
    std::optional<UserEntity> findByUsername(const std::string& username);
    std::optional<UserEntity> findByEmail(const std::string& email);
    std::vector<UserEntity> findByTenant(int tenant_id);
    std::vector<UserEntity> findByRole(const std::string& role);
    std::vector<UserEntity> findActiveUsers();
    std::vector<UserEntity> findByUsernamePattern(const std::string& username_pattern);
    
    // =======================================================================
    // 🔥 구현 파일에만 있던 메서드들 (누락된 선언 추가)
    // =======================================================================
    
    std::vector<UserEntity> findByPermission(const std::string& permission);
    std::vector<UserEntity> findByLastLoginDays(int days);
    std::optional<UserEntity> authenticate(const std::string& username, const std::string& password);
    
    // =======================================================================
    // 🔥 사용자 관리 메서드들 (누락된 선언 추가)
    // =======================================================================
    
    bool isUsernameTaken(const std::string& username, int exclude_id = 0);
    bool isEmailTaken(const std::string& email, int exclude_id = 0);
    bool verifyPassword(int user_id, const std::string& password);
    bool updatePassword(int user_id, const std::string& new_password);
    bool resetPassword(int user_id, const std::string& new_password);
    bool updateLastLogin(int user_id);
    bool updateFailedLogins(int user_id, int failed_count);
    
    // =======================================================================
    // 🔥 통계 및 분석 메서드들 (누락된 선언 추가)
    // =======================================================================
    
    std::map<std::string, int> getUserStats();
    std::vector<UserEntity> findRecentUsers(int limit = 10);
    std::vector<UserEntity> findActiveUsersByTenant(int tenant_id);
    int countUsersByTenant(int tenant_id);
    std::map<std::string, int> getUserCountByRole();
    std::map<std::string, int> getUserStatusStats();

private:
    // =======================================================================
    // 🔥 내부 헬퍼 메서드들 (누락된 선언 추가)
    // =======================================================================
    
    bool validateUser(const UserEntity& user) const;
    std::string hashPassword(const std::string& password) const;
    bool verifyPasswordHash(const std::string& password, const std::string& hash) const;
    QueryCondition buildDateRangeCondition(const std::string& field_name, 
                                         int days_from_now, 
                                         bool is_before) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // USER_REPOSITORY_H