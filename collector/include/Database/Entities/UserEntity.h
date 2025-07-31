#ifndef USER_ENTITY_H
#define USER_ENTITY_H

/**
 * @file UserEntity.h
 * @brief PulseOne User Entity - 사용자 정보 엔티티 (DeviceEntity 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔥 DeviceEntity 패턴 100% 준수:
 * - BaseEntity<UserEntity> 상속 (CRTP)
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - JSON 직렬화/역직렬화 인라인 구현
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <sstream>
#include <iomanip>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
struct json {
    template<typename T> T get() const { return T{}; }
    bool contains(const std::string&) const { return false; }
    std::string dump() const { return "{}"; }
    static json parse(const std::string&) { return json{}; }
    static json object() { return json{}; }
};
#endif

namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class UserRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 사용자 엔티티 클래스 (BaseEntity 템플릿 상속)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE users (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     tenant_id INTEGER NOT NULL,
 *     username VARCHAR(50) UNIQUE NOT NULL,
 *     email VARCHAR(100) UNIQUE NOT NULL,
 *     password_hash VARCHAR(255) NOT NULL,
 *     full_name VARCHAR(100),
 *     role VARCHAR(20) NOT NULL DEFAULT 'viewer',
 *     is_enabled BOOLEAN DEFAULT true,
 *     phone_number VARCHAR(20),
 *     department VARCHAR(50),
 *     permissions TEXT,  -- JSON array
 *     login_count INTEGER DEFAULT 0,
 *     last_login_at TIMESTAMP,
 *     notes TEXT,
 *     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
 *     updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
 * );
 */
class UserEntity : public BaseEntity<UserEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 사용자)
     */
    UserEntity();
    
    /**
     * @brief ID로 생성자 (기존 사용자 로드)
     * @param user_id 사용자 ID
     */
    explicit UserEntity(int user_id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~UserEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (선언만)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    bool isValid() const override;
    std::string getTableName() const override { return "users"; }

    // =======================================================================
    // JSON 직렬화 (인라인 구현 - 컴파일 최적화)
    // =======================================================================
    
    /**
     * @brief JSON으로 변환 (인라인 구현)
     */
    json toJson() const override {
        json j;
        try {
            // 기본 정보
            j["id"] = getId();
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
            
            // 시간 정보
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
            // 보안상 비밀번호 해시는 제외
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("UserEntity::toJson failed: " + std::string(e.what()));
            }
        }
        
        return j;
    }
    
    /**
     * @brief JSON에서 로드 (인라인 구현)
     */
    bool fromJson(const json& data) override {
        try {
            if (data.contains("id")) setId(data["id"]);
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
            if (logger_) {
                logger_->Error("UserEntity::fromJson failed: " + std::string(e.what()));
            }
            markError();
            return false;
        }
    }
    
    /**
     * @brief 문자열 표현 (인라인 구현)
     */
    std::string toString() const override {
        std::stringstream ss;
        ss << "UserEntity{";
        ss << "id=" << getId();
        ss << ", username='" << username_ << "'";
        ss << ", email='" << email_ << "'";
        ss << ", role='" << role_ << "'";
        ss << ", tenant_id=" << tenant_id_;
        ss << ", enabled=" << (is_enabled_ ? "true" : "false");
        ss << "}";
        return ss.str();
    }

    // =======================================================================
    // Getter 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    const std::string& getUsername() const { return username_; }
    const std::string& getEmail() const { return email_; }
    const std::string& getFullName() const { return full_name_; }
    const std::string& getRole() const { return role_; }
    int getTenantId() const { return tenant_id_; }
    bool isEnabled() const { return is_enabled_; }
    const std::string& getPhoneNumber() const { return phone_number_; }
    const std::string& getDepartment() const { return department_; }
    const std::vector<std::string>& getPermissions() const { return permissions_; }
    const std::chrono::system_clock::time_point& getLastLoginAt() const { return last_login_at_; }
    int getLoginCount() const { return login_count_; }
    const std::string& getNotes() const { return notes_; }

    // =======================================================================
    // Setter 메서드들 (markModified 패턴 통일)
    // =======================================================================
    
    void setUsername(const std::string& username) { 
        username_ = username; 
        markModified(); 
    }
    
    void setEmail(const std::string& email) { 
        email_ = email; 
        markModified(); 
    }
    
    void setFullName(const std::string& full_name) { 
        full_name_ = full_name; 
        markModified(); 
    }
    
    void setRole(const std::string& role) { 
        role_ = role; 
        markModified(); 
    }
    
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified(); 
    }
    
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified(); 
    }
    
    void setPhoneNumber(const std::string& phone) { 
        phone_number_ = phone; 
        markModified(); 
    }
    
    void setDepartment(const std::string& dept) { 
        department_ = dept; 
        markModified(); 
    }
    
    void setPermissions(const std::vector<std::string>& perms) { 
        permissions_ = perms; 
        markModified(); 
    }
    
    void setNotes(const std::string& notes) { 
        notes_ = notes; 
        markModified(); 
    }
    
    // 🔥 UserRepository에서 필요한 추가 메서드들
    void setLoginCount(int count) {
        login_count_ = count;
        markModified();
    }

    // =======================================================================
    // 비즈니스 로직 메서드들 (선언만)
    // =======================================================================
    
    /**
     * @brief 비밀번호 설정 (해싱 포함)
     */
    void setPassword(const std::string& password);
    
    /**
     * @brief 비밀번호 검증
     */
    bool verifyPassword(const std::string& password) const;
    
    /**
     * @brief 로그인 시간 업데이트
     */
    void updateLastLogin();
    
    /**
     * @brief 권한 확인
     */
    bool hasPermission(const std::string& permission) const;
    
    /**
     * @brief 권한 추가
     */
    void addPermission(const std::string& permission);
    
    /**
     * @brief 권한 제거
     */
    void removePermission(const std::string& permission);

    // =======================================================================
    // 고급 기능 (선언만)
    // =======================================================================
    
    json extractConfiguration() const;
    json getAuthContext() const;
    json getProfileInfo() const;

private:
    // =======================================================================
    // 멤버 변수들 (DeviceEntity 패턴)
    // =======================================================================
    
    int tenant_id_;
    std::string username_;
    std::string email_;
    std::string password_hash_;
    std::string full_name_;
    std::string role_;              // admin, engineer, operator, viewer
    bool is_enabled_;
    std::string phone_number_;
    std::string department_;
    std::vector<std::string> permissions_;
    std::chrono::system_clock::time_point last_login_at_;
    int login_count_;
    std::string notes_;
    
    // 추가 메타데이터
    std::string password_salt_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;

    // =======================================================================
    // 내부 헬퍼 메서드들 (인라인 구현)
    // =======================================================================
    
    /**
     * @brief 타임스탬프를 문자열로 변환 (인라인)
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    /**
     * @brief 비밀번호 해싱 (선언만 - CPP에서 구현)
     */
    std::string hashPassword(const std::string& password) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // USER_ENTITY_H