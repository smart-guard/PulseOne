// =============================================================================
// collector/src/Database/Entities/UserEntity.cpp
// PulseOne 사용자 엔티티 구현 - DeviceEntity 패턴 100% 준수
// =============================================================================

#include "Database/Entities/UserEntity.h"
#include "Common/Constants.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <crypt.h>
#include <random>

using namespace PulseOne::Constants;

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

UserEntity::UserEntity() 
    : BaseEntity<UserEntity>()
    , username_("")
    , email_("")
    , password_hash_("")
    , full_name_("")
    , role_("viewer")
    , tenant_id_(0)
    , is_enabled_(true)
    , phone_number_("")
    , department_("")
    , permissions_()
    , last_login_at_(std::chrono::system_clock::now())
    , login_count_(0)
    , notes_("")
    , password_salt_("") {
}

UserEntity::UserEntity(int user_id) 
    : BaseEntity<UserEntity>(user_id)
    , username_("")
    , email_("")
    , password_hash_("")
    , full_name_("")
    , role_("viewer")
    , tenant_id_(0)
    , is_enabled_(true)
    , phone_number_("")
    , department_("")
    , permissions_()
    , last_login_at_(std::chrono::system_clock::now())
    , login_count_(0)
    , notes_("")
    , password_salt_("") {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
// =============================================================================

bool UserEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("UserEntity::loadFromDatabase - Invalid user ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        // 🔥 DeviceEntity와 동일한 방식으로 executeUnifiedQuery 사용
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_->Warn("UserEntity::loadFromDatabase - User not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 엔티티로 변환
        bool success = mapRowToEntity(results[0]);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("UserEntity::loadFromDatabase - Loaded user: " + username_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool UserEntity::saveToDatabase() {
    if (!isValid()) {
        logger_->Error("UserEntity::saveToDatabase - Invalid user data");
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
            logger_->Info("UserEntity::saveToDatabase - Saved user: " + username_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool UserEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_->Error("UserEntity::updateToDatabase - Invalid user data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("UserEntity::updateToDatabase - Updated user: " + username_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool UserEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("UserEntity::deleteFromDatabase - Invalid user ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();  // DeviceEntity 패턴
            logger_->Info("UserEntity::deleteFromDatabase - Deleted user: " + username_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::deleteFromDatabase failed: " + std::string(e.what()));
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
// JSON 직렬화 (DataPointEntity 패턴)
// =============================================================================

json UserEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = id_;
        j["username"] = username_;
        j["email"] = email_;
        j["full_name"] = full_name_;
        j["role"] = role_;
        j["tenant_id"] = tenant_id_;
        j["is_enabled"] = is_enabled_;
        
        // 연락처 정보
        j["phone_number"] = phone_number_;
        j["department"] = department_;
        j["permissions"] = permissions_;
        
        // 통계 정보
        j["login_count"] = login_count_;
        j["last_login_at"] = timestampToString(last_login_at_);
        
        // 메타데이터
        j["notes"] = notes_;
        
        // 시간 정보 (DeviceEntity 패턴)
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
        // 보안상 비밀번호 해시는 제외
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::toJson failed: " + std::string(e.what()));
    }
    
    return j;
}

bool UserEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) id_ = data["id"];
        if (data.contains("username")) username_ = data["username"];
        if (data.contains("email")) email_ = data["email"];
        if (data.contains("full_name")) full_name_ = data["full_name"];
        if (data.contains("role")) role_ = data["role"];
        if (data.contains("tenant_id")) tenant_id_ = data["tenant_id"];
        if (data.contains("is_enabled")) is_enabled_ = data["is_enabled"];
        if (data.contains("phone_number")) phone_number_ = data["phone_number"];
        if (data.contains("department")) department_ = data["department"];
        if (data.contains("permissions")) permissions_ = data["permissions"];
        if (data.contains("login_count")) login_count_ = data["login_count"];
        if (data.contains("notes")) notes_ = data["notes"];
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::fromJson failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

std::string UserEntity::toString() const {
    std::stringstream ss;
    ss << "UserEntity{";
    ss << "id=" << id_;
    ss << ", username='" << username_ << "'";
    ss << ", email='" << email_ << "'";
    ss << ", role='" << role_ << "'";
    ss << ", tenant_id=" << tenant_id_;
    ss << ", enabled=" << (is_enabled_ ? "true" : "false");
    ss << "}";
    return ss.str();
}

// =============================================================================
// 비즈니스 로직 메서드들
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
    context["user_id"] = id_;
    context["username"] = username_;
    context["role"] = role_;
    context["tenant_id"] = tenant_id_;
    context["is_enabled"] = is_enabled_;
    context["permissions"] = permissions_;
    
    return context;
}

json UserEntity::getProfileInfo() const {
    json profile;
    profile["user_id"] = id_;
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

bool UserEntity::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        if (row.count("id")) id_ = std::stoi(row.at("id"));
        if (row.count("username")) username_ = row.at("username");
        if (row.count("email")) email_ = row.at("email");
        if (row.count("password_hash")) password_hash_ = row.at("password_hash");
        if (row.count("full_name")) full_name_ = row.at("full_name");
        if (row.count("role")) role_ = row.at("role");
        if (row.count("tenant_id")) tenant_id_ = std::stoi(row.at("tenant_id"));
        if (row.count("is_enabled")) is_enabled_ = (row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
        if (row.count("phone_number")) phone_number_ = row.at("phone_number");
        if (row.count("department")) department_ = row.at("department");
        if (row.count("login_count")) login_count_ = std::stoi(row.at("login_count"));
        if (row.count("notes")) notes_ = row.at("notes");
        
        // 권한은 JSON 배열로 저장되어 있다고 가정
        if (row.count("permissions")) {
            try {
                json perms_json = json::parse(row.at("permissions"));
                permissions_ = perms_json.get<std::vector<std::string>>();
            } catch (...) {
                permissions_.clear();
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("UserEntity::mapRowToEntity failed: " + std::string(e.what()));
        return false;
    }
}

std::string UserEntity::buildInsertSQL() const {
    std::stringstream ss;
    ss << "INSERT INTO " << getTableName() << " ";
    ss << "(username, email, password_hash, full_name, role, tenant_id, is_enabled, ";
    ss << "phone_number, department, permissions, login_count, notes, created_at, updated_at) ";
    ss << "VALUES ('";
    ss << username_ << "', '";
    ss << email_ << "', '";
    ss << password_hash_ << "', '";
    ss << full_name_ << "', '";
    ss << role_ << "', ";
    ss << tenant_id_ << ", ";
    ss << (is_enabled_ ? 1 : 0) << ", '";
    ss << phone_number_ << "', '";
    ss << department_ << "', '";
    
    // 권한을 JSON 배열로 직렬화
    json perms_json = permissions_;
    ss << perms_json.dump() << "', ";
    ss << login_count_ << ", '";
    ss << notes_ << "', '";
    ss << timestampToString(created_at_) << "', '";
    ss << timestampToString(updated_at_) << "')";
    
    return ss.str();
}

std::string UserEntity::buildUpdateSQL() const {
    std::stringstream ss;
    ss << "UPDATE " << getTableName() << " SET ";
    ss << "username = '" << username_ << "', ";
    ss << "email = '" << email_ << "', ";
    ss << "password_hash = '" << password_hash_ << "', ";
    ss << "full_name = '" << full_name_ << "', ";
    ss << "role = '" << role_ << "', ";
    ss << "tenant_id = " << tenant_id_ << ", ";
    ss << "is_enabled = " << (is_enabled_ ? 1 : 0) << ", ";
    ss << "phone_number = '" << phone_number_ << "', ";
    ss << "department = '" << department_ << "', ";
    
    json perms_json = permissions_;
    ss << "permissions = '" << perms_json.dump() << "', ";
    ss << "login_count = " << login_count_ << ", ";
    ss << "notes = '" << notes_ << "', ";
    ss << "updated_at = '" << timestampToString(updated_at_) << "' ";
    ss << "WHERE id = " << id_;
    
    return ss.str();
}

std::string UserEntity::hashPassword(const std::string& password) const {
    // 간단한 해싱 (실제 환경에서는 bcrypt 사용 권장)
    std::hash<std::string> hasher;
    size_t hashed = hasher(password + "pulseone_salt");
    return std::to_string(hashed);
}

std::string UserEntity::timestampToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne