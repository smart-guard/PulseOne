/**
 * @file UserEntity.cpp
 * @brief PulseOne UserEntity 구현 (DeviceEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - 모든 DB 작업은 Repository로 위임
 */

#include "Database/Entities/UserEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/UserRepository.h"
#include <algorithm>
#include <random>
#include <functional>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

UserEntity::UserEntity() 
    : BaseEntity<UserEntity>()
    , tenant_id_(0)
    , username_("")
    , email_("")
    , password_hash_("")
    , full_name_("")
    , role_("viewer")
    , is_enabled_(true)
    , phone_number_("")
    , department_("")
    , permissions_()
    , last_login_at_(std::chrono::system_clock::now())
    , login_count_(0)
    , notes_("")
    , password_salt_("")
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

UserEntity::UserEntity(int user_id) 
    : UserEntity() {  // 위임 생성자 사용
    setId(user_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool UserEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("UserEntity::loadFromDatabase - Invalid user ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getUserRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("UserEntity::loadFromDatabase - Loaded user: " + username_);
                }
                return true;
            }
        }
        
        if (logger_) {
            logger_->Warn("UserEntity::loadFromDatabase - User not found: " + std::to_string(getId()));
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool UserEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("UserEntity::saveToDatabase - Invalid user data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getUserRepository();
        if (repo) {
            // Repository의 save 메서드가 ID를 자동으로 설정함
            bool success = repo->save(*this);
            
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("UserEntity::saveToDatabase - Saved user: " + username_);
                }
            }
            
            return success;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool UserEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("UserEntity::updateToDatabase - Invalid user data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getUserRepository();
        if (repo) {
            bool success = repo->update(*this);
            
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("UserEntity::updateToDatabase - Updated user: " + username_);
                }
            }
            
            return success;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool UserEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("UserEntity::deleteFromDatabase - Invalid user ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getUserRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("UserEntity::deleteFromDatabase - Deleted user: " + username_);
                }
            }
            
            return success;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool UserEntity::isValid() const {
    // 기본 유효성 검사
    if (username_.empty()) {
        return false;
    }
    
    if (email_.empty()) {
        return false;
    }
    
    if (tenant_id_ <= 0) {
        return false;
    }
    
    // 역할 검사
    if (role_ != "admin" && role_ != "engineer" && role_ != "operator" && role_ != "viewer") {
        return false;
    }
    
    // 이메일 형식 간단 검사
    if (email_.find("@") == std::string::npos) {
        return false;
    }
    
    return true;
}

// =============================================================================
// 비즈니스 로직 메서드들 (Repository 패턴 통합)
// =============================================================================

void UserEntity::setPassword(const std::string& password) {
    password_hash_ = hashPassword(password);
    markModified();
}

bool UserEntity::verifyPassword(const std::string& password) const {
    if (password_hash_.empty()) {
        return false;
    }
    
    std::string hashed = hashPassword(password);
    return hashed == password_hash_;
}

void UserEntity::updateLastLogin() {
    last_login_at_ = std::chrono::system_clock::now();
    login_count_++;
    updated_at_ = std::chrono::system_clock::now();
    markModified();
}

bool UserEntity::hasPermission(const std::string& permission) const {
    return std::find(permissions_.begin(), permissions_.end(), permission) != permissions_.end();
}

void UserEntity::addPermission(const std::string& permission) {
    if (!hasPermission(permission)) {
        permissions_.push_back(permission);
        markModified();
    }
}

void UserEntity::removePermission(const std::string& permission) {
    auto it = std::find(permissions_.begin(), permissions_.end(), permission);
    if (it != permissions_.end()) {
        permissions_.erase(it);
        markModified();
    }
}

// =============================================================================
// 고급 기능 (DeviceEntity 패턴)
// =============================================================================

json UserEntity::extractConfiguration() const {
    json config = {
        {"basic", {
            {"username", username_},
            {"email", email_},
            {"full_name", full_name_},
            {"role", role_},
            {"is_enabled", is_enabled_}
        }},
        {"contact", {
            {"phone_number", phone_number_},
            {"department", department_}
        }},
        {"permissions", permissions_},
        {"tenant", {
            {"tenant_id", tenant_id_}
        }}
    };
    
    return config;
}

json UserEntity::getAuthContext() const {
    json context;
    context["user_id"] = getId();
    context["username"] = username_;
    context["role"] = role_;
    context["tenant_id"] = tenant_id_;
    context["is_enabled"] = is_enabled_;
    context["permissions"] = permissions_;
    
    return context;
}

json UserEntity::getProfileInfo() const {
    json profile;
    profile["user_id"] = getId();
    profile["username"] = username_;
    profile["email"] = email_;
    profile["full_name"] = full_name_;
    profile["department"] = department_;
    profile["phone_number"] = phone_number_;
    profile["login_count"] = login_count_;
    profile["last_login_at"] = timestampToString(last_login_at_);
    
    return profile;
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceEntity 패턴)
// =============================================================================

std::string UserEntity::hashPassword(const std::string& password) const {
    // 간단한 해싱 (실제 환경에서는 bcrypt 사용 권장)
    // salt 추가로 보안 강화
    std::string salted_password = password + "pulseone_salt_2025";
    std::hash<std::string> hasher;
    size_t hashed = hasher(salted_password);
    return std::to_string(hashed);
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne