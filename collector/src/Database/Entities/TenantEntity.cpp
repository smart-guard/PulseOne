// =============================================================================
// collector/src/Database/Entities/TenantEntity.cpp
// PulseOne 테넌트 엔티티 구현 - DeviceEntity 패턴 100% 준수
// =============================================================================

#include "Database/Entities/TenantEntity.h"
#include "Common/Constants.h"
#include "Common/Utils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>


namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
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
    , subscription_end_(std::chrono::system_clock::now() + std::chrono::hours(24 * 30)) {  // 30일 기본
}

TenantEntity::TenantEntity(int tenant_id) 
    : BaseEntity<TenantEntity>(tenant_id)
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
    , subscription_end_(std::chrono::system_clock::now() + std::chrono::hours(24 * 30)) {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
// =============================================================================

bool TenantEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("TenantEntity::loadFromDatabase - Invalid tenant ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        // 🔥 DeviceEntity와 동일한 방식으로 executeUnifiedQuery 사용
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_->Warn("TenantEntity::loadFromDatabase - Tenant not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 엔티티로 변환
        bool success = mapRowToEntity(results[0]);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("TenantEntity::loadFromDatabase - Loaded tenant: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool TenantEntity::saveToDatabase() {
    if (!isValid()) {
        logger_->Error("TenantEntity::saveToDatabase - Invalid tenant data");
        return false;
    }
    
    try {
        std::string sql = buildInsertSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            // SQLite인 경우 마지막 INSERT ID 조회
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            if (db_type == "SQLITE") {
                auto results = executeUnifiedQuery("SELECT last_insert_rowid() as id");
                if (!results.empty()) {
                    id_ = std::stoi(results[0]["id"]);
                }
            }
            
            markSaved();  // DeviceEntity 패턴
            logger_->Info("TenantEntity::saveToDatabase - Saved tenant: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool TenantEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_->Error("TenantEntity::updateToDatabase - Invalid tenant data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("TenantEntity::updateToDatabase - Updated tenant: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool TenantEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("TenantEntity::deleteFromDatabase - Invalid tenant ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();  // DeviceEntity 패턴
            logger_->Info("TenantEntity::deleteFromDatabase - Deleted tenant: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool TenantEntity::isValid() const {
    // 기본 유효성 검사
    if (name_.empty()) {
        return false;
    }
    
    if (domain_.empty()) {
        return false;
    }
    
    // 제한값 검사
    if (max_users_ <= 0 || max_devices_ <= 0 || max_data_points_ <= 0) {
        return false;
    }
    
    // 구독 기간 검사
    if (subscription_start_ >= subscription_end_) {
        return false;
    }
    
    // 도메인 형식 간단 검사
    if (domain_.find(".") == std::string::npos && domain_.length() < 3) {
        return false;
    }
    
    return true;
}

// =============================================================================
// JSON 직렬화 (DataPointEntity 패턴)
// =============================================================================

json TenantEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = id_;
        j["name"] = name_;
        j["description"] = description_;
        j["domain"] = domain_;
        j["status"] = statusToString(status_);
        
        // 제한 설정
        j["limits"] = {
            {"max_users", max_users_},
            {"max_devices", max_devices_},
            {"max_data_points", max_data_points_}
        };
        
        // 연락처 정보
        j["contact"] = {
            {"email", contact_email_},
            {"phone", contact_phone_},
            {"address", address_},
            {"city", city_},
            {"country", country_},
            {"timezone", timezone_}
        };
        
        // 구독 정보
        j["subscription"] = {
            {"start", timestampToString(subscription_start_)},
            {"end", timestampToString(subscription_end_)},
            {"expired", isSubscriptionExpired()}
        };
        
        // 시간 정보 (DeviceEntity 패턴)
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::toJson failed: " + std::string(e.what()));
    }
    
    return j;
}

bool TenantEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) id_ = data["id"];
        if (data.contains("name")) name_ = data["name"];
        if (data.contains("description")) description_ = data["description"];
        if (data.contains("domain")) domain_ = data["domain"];
        if (data.contains("status")) status_ = stringToStatus(data["status"]);
        
        // 제한 설정
        if (data.contains("limits")) {
            auto limits = data["limits"];
            if (limits.contains("max_users")) max_users_ = limits["max_users"];
            if (limits.contains("max_devices")) max_devices_ = limits["max_devices"];
            if (limits.contains("max_data_points")) max_data_points_ = limits["max_data_points"];
        }
        
        // 연락처 정보
        if (data.contains("contact")) {
            auto contact = data["contact"];
            if (contact.contains("email")) contact_email_ = contact["email"];
            if (contact.contains("phone")) contact_phone_ = contact["phone"];
            if (contact.contains("address")) address_ = contact["address"];
            if (contact.contains("city")) city_ = contact["city"];
            if (contact.contains("country")) country_ = contact["country"];
            if (contact.contains("timezone")) timezone_ = contact["timezone"];
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::fromJson failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

std::string TenantEntity::toString() const {
    std::stringstream ss;
    ss << "TenantEntity{";
    ss << "id=" << id_;
    ss << ", name='" << name_ << "'";
    ss << ", domain='" << domain_ << "'";
    ss << ", status='" << statusToString(status_) << "'";
    ss << ", max_users=" << max_users_;
    ss << ", max_devices=" << max_devices_;
    ss << ", max_data_points=" << max_data_points_;
    ss << "}";
    return ss.str();
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

bool TenantEntity::isSubscriptionExpired() const {
    return std::chrono::system_clock::now() > subscription_end_;
}

void TenantEntity::extendSubscription(int days) {
    auto extension = std::chrono::hours(24 * days);
    subscription_end_ += extension;
    markModified();
}

bool TenantEntity::isWithinUserLimit(int current_users) const {
    return current_users <= max_users_;
}

bool TenantEntity::isWithinDeviceLimit(int current_devices) const {
    return current_devices <= max_devices_;
}

bool TenantEntity::isWithinDataPointLimit(int current_points) const {
    return current_points <= max_data_points_;
}

std::string TenantEntity::statusToString(Status status) {
    switch (status) {
        case Status::ACTIVE: return "ACTIVE";
        case Status::SUSPENDED: return "SUSPENDED";
        case Status::TRIAL: return "TRIAL";
        case Status::EXPIRED: return "EXPIRED";
        case Status::DISABLED: return "DISABLED";
        default: return "UNKNOWN";
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
// 고급 기능 (DeviceEntity 패턴)
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
    subscription["tenant_id"] = id_;
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
    limits["tenant_id"] = id_;
    limits["max_users"] = max_users_;
    limits["max_devices"] = max_devices_;
    limits["max_data_points"] = max_data_points_;
    
    return limits;
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceEntity 패턴)
// =============================================================================

bool TenantEntity::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        if (row.count("id")) id_ = std::stoi(row.at("id"));
        if (row.count("name")) name_ = row.at("name");
        if (row.count("description")) description_ = row.at("description");
        if (row.count("domain")) domain_ = row.at("domain");
        if (row.count("status")) status_ = stringToStatus(row.at("status"));
        if (row.count("max_users")) max_users_ = std::stoi(row.at("max_users"));
        if (row.count("max_devices")) max_devices_ = std::stoi(row.at("max_devices"));
        if (row.count("max_data_points")) max_data_points_ = std::stoi(row.at("max_data_points"));
        if (row.count("contact_email")) contact_email_ = row.at("contact_email");
        if (row.count("contact_phone")) contact_phone_ = row.at("contact_phone");
        if (row.count("address")) address_ = row.at("address");
        if (row.count("city")) city_ = row.at("city");
        if (row.count("country")) country_ = row.at("country");
        if (row.count("timezone")) timezone_ = row.at("timezone");
        if (row.count("subscription_start")) 
            subscription_start_ = PulseOne::Utils::ParseTimestampFromString(row.at("subscription_start"));  // ✅ Utils 함수 사용
        if (row.count("subscription_end")) 
            subscription_end_ = PulseOne::Utils::ParseTimestampFromString(row.at("subscription_end"));         
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("TenantEntity::mapRowToEntity failed: " + std::string(e.what()));
        return false;
    }
}

std::string TenantEntity::buildInsertSQL() const {
    std::stringstream ss;
    ss << "INSERT INTO " << getTableName() << " ";
    ss << "(name, description, domain, status, max_users, max_devices, max_data_points, ";
    ss << "contact_email, contact_phone, address, city, country, timezone, ";
    ss << "subscription_start, subscription_end, created_at, updated_at) ";
    ss << "VALUES ('";
    ss << name_ << "', '";
    ss << description_ << "', '";
    ss << domain_ << "', '";
    ss << statusToString(status_) << "', ";
    ss << max_users_ << ", ";
    ss << max_devices_ << ", ";
    ss << max_data_points_ << ", '";
    ss << contact_email_ << "', '";
    ss << contact_phone_ << "', '";
    ss << address_ << "', '";
    ss << city_ << "', '";
    ss << country_ << "', '";
    ss << timezone_ << "', '";
    ss << PulseOne::Utils::TimestampToDBString(subscription_start_) << "', '";  // ✅ Utils 함수 사용
    ss << PulseOne::Utils::TimestampToDBString(subscription_end_) << "', '";    // ✅ Utils 함수 사용
    ss << PulseOne::Utils::TimestampToDBString(created_at_) << "', '";          // ✅ Utils 함수 사용
    ss << PulseOne::Utils::TimestampToDBString(updated_at_) << "')";            // ✅ Utils 함수 사용

    
    return ss.str();
}

std::string TenantEntity::buildUpdateSQL() const {
    std::stringstream ss;
    ss << "UPDATE " << getTableName() << " SET ";
    ss << "name = '" << name_ << "', ";
    ss << "description = '" << description_ << "', ";
    ss << "domain = '" << domain_ << "', ";
    ss << "status = '" << statusToString(status_) << "', ";
    ss << "max_users = " << max_users_ << ", ";
    ss << "max_devices = " << max_devices_ << ", ";
    ss << "max_data_points = " << max_data_points_ << ", ";
    ss << "contact_email = '" << contact_email_ << "', ";
    ss << "contact_phone = '" << contact_phone_ << "', ";
    ss << "address = '" << address_ << "', ";
    ss << "city = '" << city_ << "', ";
    ss << "country = '" << country_ << "', ";
    ss << "timezone = '" << timezone_ << "', ";
    ss << "subscription_start = '" << timestampToString(subscription_start_) << "', ";
    ss << "subscription_end = '" << timestampToString(subscription_end_) << "', ";
    ss << "updated_at = '" << timestampToString(updated_at_) << "' ";
    ss << "WHERE id = " << id_;
    
    return ss.str();
}


} // namespace Entities
} // namespace Database
} // namespace PulseOne