/**
 * @file TenantEntity.cpp
 * @brief PulseOne TenantEntity 구현 (DeviceEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - 모든 비즈니스 로직은 Repository로 위임
 */

#include "Database/Entities/TenantEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/TenantRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

TenantEntity::TenantEntity() 
    : BaseEntity<TenantEntity>()
    , name_("")
    , description_("")
    , domain_("")
    , status_(Status::TRIAL)
    , max_users_(10)
    , max_devices_(50)
    , max_data_points_(500)
    , contact_email_("")
    , contact_phone_("")
    , address_("")
    , city_("")
    , country_("")
    , timezone_("UTC")
    , subscription_start_(std::chrono::system_clock::now())
    , subscription_end_(std::chrono::system_clock::now() + std::chrono::hours(24 * 30))
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

TenantEntity::TenantEntity(int tenant_id) 
    : TenantEntity() {  // 위임 생성자 사용
    setId(tenant_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool TenantEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("TenantEntity::loadFromDatabase - Invalid tenant ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getTenantRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("TenantEntity - Loaded tenant: " + name_);
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("TenantEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool TenantEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("TenantEntity::saveToDatabase - Invalid tenant data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getTenantRepository();
        if (repo) {
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("TenantEntity - Saved tenant: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("TenantEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool TenantEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("TenantEntity::deleteFromDatabase - Invalid tenant ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getTenantRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("TenantEntity - Deleted tenant: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("TenantEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool TenantEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("TenantEntity::updateToDatabase - Invalid tenant data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getTenantRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("TenantEntity - Updated tenant: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("TenantEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 정적 유틸리티 메서드들
// =============================================================================

std::string TenantEntity::statusToString(Status status) {
    switch (status) {
        case Status::ACTIVE: return "ACTIVE";
        case Status::SUSPENDED: return "SUSPENDED";
        case Status::TRIAL: return "TRIAL";
        case Status::EXPIRED: return "EXPIRED";
        case Status::DISABLED: return "DISABLED";
        default: return "TRIAL";
    }
}

TenantEntity::Status TenantEntity::stringToStatus(const std::string& status_str) {
    if (status_str == "ACTIVE") return Status::ACTIVE;
    if (status_str == "SUSPENDED") return Status::SUSPENDED;
    if (status_str == "TRIAL") return Status::TRIAL;
    if (status_str == "EXPIRED") return Status::EXPIRED;
    if (status_str == "DISABLED") return Status::DISABLED;
    return Status::TRIAL;  // 기본값
}

// =============================================================================
// 고급 기능 메서드들
// =============================================================================

json TenantEntity::extractConfiguration() const {
    json config = {
        {"basic", {
            {"name", name_},
            {"description", description_},
            {"domain", domain_},
            {"status", statusToString(status_)}
        }},
        {"limits", {
            {"max_users", max_users_},
            {"max_devices", max_devices_},
            {"max_data_points", max_data_points_}
        }},
        {"contact", {
            {"email", contact_email_},
            {"phone", contact_phone_},
            {"address", address_},
            {"city", city_},
            {"country", country_},
            {"timezone", timezone_}
        }}
    };
    
    return config;
}

json TenantEntity::getSubscriptionInfo() const {
    json subscription;
    subscription["tenant_id"] = getId();
    subscription["start"] = timestampToString(subscription_start_);
    subscription["end"] = timestampToString(subscription_end_);
    subscription["expired"] = isSubscriptionExpired();
    subscription["status"] = statusToString(status_);
    
    // 남은 일수 계산
    auto now = std::chrono::system_clock::now();
    if (subscription_end_ > now) {
        auto remaining = std::chrono::duration_cast<std::chrono::hours>(subscription_end_ - now);
        subscription["remaining_days"] = remaining.count() / 24;
    } else {
        subscription["remaining_days"] = 0;
    }
    
    return subscription;
}

json TenantEntity::getLimitInfo() const {
    json limits;
    limits["tenant_id"] = getId();
    limits["max_users"] = max_users_;
    limits["max_devices"] = max_devices_;
    limits["max_data_points"] = max_data_points_;
    
    // 현재 사용량은 Repository에서 조회해야 함
    limits["note"] = "Use TenantRepository to get current usage statistics";
    
    return limits;
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

std::string TenantEntity::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::shared_ptr<Repositories::TenantRepository> TenantEntity::getRepository() const {
    auto& factory = RepositoryFactory::getInstance();
    return factory.getTenantRepository();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne