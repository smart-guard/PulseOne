/**
 * @file UserRepository.h - vtable 에러 완전 해결
 * @brief PulseOne UserRepository - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔧 수정사항:
 * - IRepository 모든 순수 가상 함수 override 선언 추가
 * - 캐시 관리 메서드들 추가
 * - DeviceRepository.h와 완전 동일한 구조
 */

#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/UserEntity.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
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

using UserEntity = PulseOne::Database::Entities::UserEntity;

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
    // IRepository 인터페이스 구현 (필수!)
    // =======================================================================
    std::map<std::string, int> getCacheStats() const override;
    std::vector<UserEntity> findAll() override;
    std::optional<UserEntity> findById(int id) override;
    bool save(UserEntity& entity) override;
    bool update(const UserEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (IRepository 상속)
    // =======================================================================
    
    std::vector<UserEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<UserEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // User 특화 메서드들
    // =======================================================================
    
    std::optional<UserEntity> findByUsername(const std::string& username);
    std::optional<UserEntity> findByEmail(const std::string& email);
    std::vector<UserEntity> findByTenant(int tenant_id);
    std::vector<UserEntity> findByRole(const std::string& role);
    std::vector<UserEntity> findByEnabled(bool enabled);
    std::vector<UserEntity> findByDepartment(const std::string& department);
    std::vector<UserEntity> findByPermission(const std::string& permission);

    // =======================================================================
    // 통계 메서드들
    // =======================================================================
    
    int countByTenant(int tenant_id);
    int countByRole(const std::string& role);
    int countActiveUsers();

    // =======================================================================
    // 인증 관련 메서드들
    // =======================================================================
    
    bool recordLogin(int user_id);
    bool isUsernameTaken(const std::string& username);
    bool isEmailTaken(const std::string& email);

    // =======================================================================
    // 고급 쿼리 메서드들
    // =======================================================================
    
    std::vector<UserEntity> searchUsers(
        const std::optional<int>& tenant_id = std::nullopt,
        const std::optional<std::string>& role = std::nullopt,
        const std::optional<bool>& enabled = std::nullopt,
        const std::optional<std::string>& department = std::nullopt
    );
    
    std::vector<UserEntity> findInactiveUsers(int days_ago);

    // =======================================================================
    // 벌크 연산 (DeviceRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<UserEntity>& entities);
    int updateBulk(const std::vector<UserEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 캐시 관리 (DeviceRepository 패턴 - 핵심!)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<UserEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("UserRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<UserEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<UserEntity>::clearCache();
        if (logger_) {
            logger_->Info("UserRepository cache cleared");
        }
    }

    // =======================================================================
    // 통계 메서드 (DeviceRepository 패턴)
    // =======================================================================
    
    int getTotalCount();

private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    
    DbLib::DatabaseManager* db_manager_;
    LogManager* logger_;
    
    void initializeDependencies() {
        db_manager_ = &DbLib::DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceRepository 패턴)
    // =======================================================================
    
    UserEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const UserEntity& entity);
    bool ensureTableExists();
    bool validateEntity(const UserEntity& entity) const;

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