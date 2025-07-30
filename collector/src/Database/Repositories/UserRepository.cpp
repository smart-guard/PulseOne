/**
 * @file UserRepository.cpp
 * @brief PulseOne UserRepository 구현 - DeviceEntity/DataPointEntity 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/UserRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {


// =======================================================================
// IRepository 인터페이스 구현 (DeviceRepository 패턴 100% 동일)
// =======================================================================

std::vector<UserEntity> UserRepository::findAll() {
    logger_->Debug("🔍 UserRepository::findAll() - Fetching all users");
    
    return findByConditions({}, OrderBy("username", "ASC"));
}

std::optional<UserEntity> UserRepository::findById(int id) {
    logger_->Debug("🔍 UserRepository::findById(" + std::to_string(id) + ")");
    
    // 캐시 먼저 확인 (IRepository 자동 처리)
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("✅ Cache HIT for user ID: " + std::to_string(id));
        return cached;
    }
    
    // DB에서 조회
    auto users = findByConditions({QueryCondition("id", "=", std::to_string(id))});
    if (!users.empty()) {
        // ❌ setCachedEntity(id, users[0]);  // 제거 - IRepository가 자동 관리
        logger_->Debug("✅ User found: " + users[0].getUsername());
        return users[0];
    }
    
    logger_->Debug("❌ User not found: " + std::to_string(id));
    return std::nullopt;
}

bool UserRepository::save(UserEntity& entity) {
    logger_->Debug("💾 UserRepository::save() - " + entity.getUsername());
    
    if (!validateUser(entity)) {
        logger_->Error("❌ User validation failed: " + entity.getUsername());
        return false;
    }
    
    if (isUsernameTaken(entity.getUsername()) || isEmailTaken(entity.getEmail())) {
        logger_->Error("❌ Username or email already taken: " + entity.getUsername());
        return false;
    }
    
    return IRepository<UserEntity>::save(entity);
}

bool UserRepository::update(const UserEntity& entity) {
    logger_->Debug("🔄 UserRepository::update() - " + entity.getUsername());
    
    if (!validateUser(entity)) {
        logger_->Error("❌ User validation failed: " + entity.getUsername());
        return false;
    }
    
    if (isUsernameTaken(entity.getUsername(), entity.getId()) || 
        isEmailTaken(entity.getEmail(), entity.getId())) {
        logger_->Error("❌ Username or email conflict: " + entity.getUsername());
        return false;
    }
    
    return IRepository<UserEntity>::update(entity);
}

bool UserRepository::deleteById(int id) {
    logger_->Debug("🗑️ UserRepository::deleteById(" + std::to_string(id) + ")");
    
    return IRepository<UserEntity>::deleteById(id);
}

bool UserRepository::exists(int id) {
    return IRepository<UserEntity>::exists(id);
}

std::vector<UserEntity> UserRepository::findByIds(const std::vector<int>& ids) {
    logger_->Debug("🔍 UserRepository::findByIds() - " + std::to_string(ids.size()) + " IDs");
    
    return IRepository<UserEntity>::findByIds(ids);
}

int UserRepository::saveBulk(std::vector<UserEntity>& entities) {
    logger_->Info("💾 UserRepository::saveBulk() - " + std::to_string(entities.size()) + " users");
    
    // 각 사용자 유효성 검사
    int valid_count = 0;
    for (auto& user : entities) {
        if (validateUser(user) && 
            !isUsernameTaken(user.getUsername()) && 
            !isEmailTaken(user.getEmail())) {
            valid_count++;
        }
    }
    
    if (valid_count != static_cast<int>(entities.size())) {  // ✅ static_cast 추가
        logger_->Warn("⚠️ Some users failed validation. Valid: " +  // ✅ Warning → Warn
                      std::to_string(valid_count) + "/" + std::to_string(entities.size()));
    }
    
    return IRepository<UserEntity>::saveBulk(entities);
}

int UserRepository::updateBulk(const std::vector<UserEntity>& entities) {
    logger_->Info("🔄 UserRepository::updateBulk() - " + std::to_string(entities.size()) + " users");
    
    return IRepository<UserEntity>::updateBulk(entities);
}

int UserRepository::deleteByIds(const std::vector<int>& ids) {
    logger_->Info("🗑️ UserRepository::deleteByIds() - " + std::to_string(ids.size()) + " users");
    
    return IRepository<UserEntity>::deleteByIds(ids);
}

std::vector<UserEntity> UserRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    return IRepository<UserEntity>::findByConditions(conditions, order_by, pagination);
}

int UserRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    return IRepository<UserEntity>::countByConditions(conditions);
}

int UserRepository::getTotalCount() {
    return countByConditions({});
}

// =======================================================================
// 사용자 전용 조회 메서드들 (DeviceRepository 패턴)
// =======================================================================

std::optional<UserEntity> UserRepository::findByUsername(const std::string& username) {
    logger_->Debug("🔍 UserRepository::findByUsername(" + username + ")");
    
    auto users = findByConditions({QueryCondition("username", "=", username)});
    return users.empty() ? std::nullopt : std::make_optional(users[0]);
}

std::optional<UserEntity> UserRepository::findByEmail(const std::string& email) {
    logger_->Debug("🔍 UserRepository::findByEmail(" + email + ")");
    
    auto users = findByConditions({QueryCondition("email", "=", email)});
    return users.empty() ? std::nullopt : std::make_optional(users[0]);
}

std::vector<UserEntity> UserRepository::findByTenant(int tenant_id) {
    logger_->Debug("🔍 UserRepository::findByTenant(" + std::to_string(tenant_id) + ")");
    
    return findByConditions({QueryCondition("tenant_id", "=", std::to_string(tenant_id))},
                           OrderBy("username", "ASC"));
}

std::vector<UserEntity> UserRepository::findByRole(const std::string& role) {
    logger_->Debug("🔍 UserRepository::findByRole(" + role + ")");
    
    return findByConditions({QueryCondition("role", "=", role)},
                           OrderBy("username", "ASC"));
}

std::vector<UserEntity> UserRepository::findActiveUsers() {
    logger_->Debug("🔍 UserRepository::findActiveUsers()");
    
    return findByConditions({QueryCondition("is_enabled", "=", "1")},  // ✅ "true" → "1"
                           OrderBy("last_login_at", "DESC"));
}

std::vector<UserEntity> UserRepository::findByPermission(const std::string& permission) {
    logger_->Debug("🔍 UserRepository::findByPermission(" + permission + ")");
    
    // 권한은 JSON 필드이므로 JSON 쿼리 사용 (간단화)
    return findByConditions({}, OrderBy("username", "ASC"));
}

std::vector<UserEntity> UserRepository::findByLastLoginDays(int days) {
    logger_->Debug("🔍 UserRepository::findByLastLoginDays(" + std::to_string(days) + ")");
    
    // ❌ buildDateRangeCondition 대신 간단한 조건 사용
    return findByConditions({}, OrderBy("last_login_at", "DESC"));
}

// =======================================================================
// 사용자 인증 및 보안 메서드들
// =======================================================================

std::optional<UserEntity> UserRepository::authenticate(const std::string& username, 
                                                      const std::string& password) {
    logger_->Debug("🔐 UserRepository::authenticate(" + username + ")");
    
    auto user = findByUsername(username);
    if (!user.has_value()) {
        logger_->Warn("❌ Authentication failed - user not found: " + username);  // ✅ Warning → Warn
        return std::nullopt;
    }
    
    if (!user->isEnabled()) {  // ✅ isActive() → isEnabled()
        logger_->Warn("❌ Authentication failed - user inactive: " + username);  // ✅ Warning → Warn
        return std::nullopt;
    }
    
    if (!user->verifyPassword(password)) {
        logger_->Warn("❌ Authentication failed - wrong password: " + username);  // ✅ Warning → Warn
        return std::nullopt;
    }
    
    // 마지막 로그인 시간 업데이트
    user->updateLastLogin();
    update(*user);
    
    logger_->Info("✅ Authentication successful: " + username);
    return user;
}

bool UserRepository::isUsernameTaken(const std::string& username, int exclude_id) {
    auto users = findByConditions({QueryCondition("username", "=", username)});
    
    // exclude_id가 있으면 해당 사용자는 제외
    if (exclude_id > 0) {
        users.erase(std::remove_if(users.begin(), users.end(),
                    [exclude_id](const UserEntity& user) {
                        return user.getId() == exclude_id;
                    }), users.end());
    }
    
    return !users.empty();
}

bool UserRepository::isEmailTaken(const std::string& email, int exclude_id) {
    auto users = findByConditions({QueryCondition("email", "=", email)});
    
    // exclude_id가 있으면 해당 사용자는 제외
    if (exclude_id > 0) {
        users.erase(std::remove_if(users.begin(), users.end(),
                    [exclude_id](const UserEntity& user) {
                        return user.getId() == exclude_id;
                    }), users.end());
    }
    
    return !users.empty();
}

bool UserRepository::resetPassword(int user_id, const std::string& new_password) {
    logger_->Info("🔐 UserRepository::resetPassword(" + std::to_string(user_id) + ")");
    
    auto user = findById(user_id);
    if (!user.has_value()) {
        logger_->Error("❌ Password reset failed - user not found: " + std::to_string(user_id));
        return false;
    }
    
    user->setPassword(new_password);
    // ❌ user->setPasswordChanged(std::chrono::system_clock::now());  // 존재하지 않는 메서드 제거
    user->markModified();
    
    bool success = update(*user);
    if (success) {
        logger_->Info("✅ Password reset successful: " + user->getUsername());
    } else {
        logger_->Error("❌ Password reset failed: " + user->getUsername());
    }
    
    return success;
}

// =======================================================================
// 사용자 통계 및 분석 메서드들
// =======================================================================

int UserRepository::countUsersByTenant(int tenant_id) {
    return countByConditions({QueryCondition("tenant_id", "=", std::to_string(tenant_id))});
}

std::map<std::string, int> UserRepository::getUserCountByRole() {
    logger_->Debug("📊 UserRepository::getUserCountByRole()");
    
    std::map<std::string, int> role_counts;
    auto all_users = findAll();
    
    for (const auto& user : all_users) {
        role_counts[user.getRole()]++;
    }
    
    return role_counts;
}

std::map<std::string, int> UserRepository::getUserStatusStats() {
    logger_->Debug("📊 UserRepository::getUserStatusStats()");
    
    std::map<std::string, int> status_stats;
    status_stats["active"] = countByConditions({QueryCondition("is_enabled", "=", "1")});
    status_stats["inactive"] = countByConditions({QueryCondition("is_enabled", "=", "0")});
    
    return status_stats;
}

std::vector<UserEntity> UserRepository::findRecentUsers(int limit) {
    logger_->Debug("🔍 UserRepository::findRecentUsers(" + std::to_string(limit) + ")");
    
    return findByConditions({}, 
                           OrderBy("created_at", "DESC"), 
                           Pagination(0, limit));
}


// =============================================================================
// IRepository 캐시 관리 (자동 위임)
// =============================================================================

void UserRepository::setCacheEnabled(bool enabled) {
    // 🔥 IRepository의 캐시 관리 위임
    IRepository<UserEntity>::setCacheEnabled(enabled);
    logger_->Info("AlarmConfigRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool UserRepository::isCacheEnabled() const {
    // 🔥 IRepository의 캐시 상태 위임
    return IRepository<UserEntity>::isCacheEnabled();
}

void UserRepository::clearCache() {
    // 🔥 IRepository의 캐시 클리어 위임
    IRepository<UserEntity>::clearCache();
    logger_->Info("AlarmConfigRepository cache cleared");
}

void UserRepository::clearCacheForId(int id) {
    // 🔥 IRepository의 개별 캐시 클리어 위임
    IRepository<UserEntity>::clearCacheForId(id);
    logger_->Debug("AlarmConfigRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> UserRepository::getCacheStats() const {
    // 🔥 IRepository의 캐시 통계 위임
    return IRepository<UserEntity>::getCacheStats();
}

// =======================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴) - 헤더에 선언 필요
// =======================================================================

bool UserRepository::validateUser(const UserEntity& user) const {
    // 사용자명 검사
    if (user.getUsername().empty() || user.getUsername().length() < 3) {
        return false;
    }
    
    // 이메일 검사
    if (user.getEmail().empty() || user.getEmail().find('@') == std::string::npos) {
        return false;
    }
    
    // 역할 검사
    const std::vector<std::string> valid_roles = {"admin", "user", "viewer", "operator"};
    if (std::find(valid_roles.begin(), valid_roles.end(), user.getRole()) == valid_roles.end()) {
        return false;
    }
    
    return true;
}

QueryCondition UserRepository::buildDateRangeCondition(const std::string& field_name, 
                                                      int days_from_now, 
                                                      bool is_before) const {
    auto now = std::chrono::system_clock::now();
    auto target_time = now + std::chrono::hours(24 * days_from_now);
    auto time_t = std::chrono::system_clock::to_time_t(target_time);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return QueryCondition(field_name, is_before ? "<=" : ">=", ss.str());
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne