#ifndef TENANT_ENTITY_H
#define TENANT_ENTITY_H

/**
 * @file TenantEntity.h
 * @brief PulseOne Tenant Entity - 테넌트 정보 엔티티 (DeviceEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - BaseEntity<TenantEntity> 상속 (CRTP)
 * - tenants 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <vector>
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
    class TenantRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 테넌트 엔티티 클래스 (BaseEntity 상속, 정규화된 스키마)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE tenants (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     
 *     -- 기본 정보
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     domain VARCHAR(100) NOT NULL UNIQUE,
 *     status VARCHAR(20) DEFAULT 'TRIAL',
 *     
 *     -- 제한 설정
 *     max_users INTEGER DEFAULT 10,
 *     max_devices INTEGER DEFAULT 50,
 *     max_data_points INTEGER DEFAULT 500,
 *     
 *     -- 연락처 정보
 *     contact_email VARCHAR(255),
 *     contact_phone VARCHAR(50),
 *     address TEXT,
 *     city VARCHAR(100),
 *     country VARCHAR(100),
 *     timezone VARCHAR(50) DEFAULT 'UTC',
 *     
 *     -- 구독 정보
 *     subscription_start DATETIME NOT NULL,
 *     subscription_end DATETIME NOT NULL,
 *     
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class TenantEntity : public BaseEntity<TenantEntity> {
public:
    // =======================================================================
    // 테넌트 상태 열거형 (DB 스키마와 일치)
    // =======================================================================
    
    enum class Status {
        ACTIVE,         // 활성
        SUSPENDED,      // 정지
        TRIAL,          // 체험
        EXPIRED,        // 만료
        DISABLED        // 비활성화
    };

    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    TenantEntity();
    explicit TenantEntity(int tenant_id);
    virtual ~TenantEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현)
    // =======================================================================
    
    json toJson() const override {
        json j;
        
        try {
            // 기본 정보
            j["id"] = getId();
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
            
            // 메타데이터
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
        } catch (const std::exception&) {
            // JSON 생성 실패 시 기본 객체 반환
        }
        
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            // 기본 정보
            if (data.contains("id")) {
                setId(data["id"].get<int>());
            }
            if (data.contains("name")) {
                setName(data["name"].get<std::string>());
            }
            if (data.contains("description")) {
                setDescription(data["description"].get<std::string>());
            }
            if (data.contains("domain")) {
                setDomain(data["domain"].get<std::string>());
            }
            if (data.contains("status")) {
                setStatus(stringToStatus(data["status"].get<std::string>()));
            }
            
            // 제한 설정
            if (data.contains("limits")) {
                auto limits = data["limits"];
                if (limits.contains("max_users")) {
                    setMaxUsers(limits["max_users"].get<int>());
                }
                if (limits.contains("max_devices")) {
                    setMaxDevices(limits["max_devices"].get<int>());
                }
                if (limits.contains("max_data_points")) {
                    setMaxDataPoints(limits["max_data_points"].get<int>());
                }
            }
            
            // 연락처 정보
            if (data.contains("contact")) {
                auto contact = data["contact"];
                if (contact.contains("email")) {
                    setContactEmail(contact["email"].get<std::string>());
                }
                if (contact.contains("phone")) {
                    setContactPhone(contact["phone"].get<std::string>());
                }
                if (contact.contains("address")) {
                    setAddress(contact["address"].get<std::string>());
                }
                if (contact.contains("city")) {
                    setCity(contact["city"].get<std::string>());
                }
                if (contact.contains("country")) {
                    setCountry(contact["country"].get<std::string>());
                }
                if (contact.contains("timezone")) {
                    setTimezone(contact["timezone"].get<std::string>());
                }
            }
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "TenantEntity[";
        oss << "id=" << getId();
        oss << ", name=" << name_;
        oss << ", domain=" << domain_;
        oss << ", status=" << statusToString(status_);
        oss << ", max_users=" << max_users_;
        oss << ", max_devices=" << max_devices_;
        oss << ", max_data_points=" << max_data_points_;
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "tenants"; 
    }

    // =======================================================================
    // 기본 속성 접근자 (인라인)
    // =======================================================================
    
    // 기본 정보
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { 
        name_ = name; 
        markModified();
    }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified();
    }
    
    const std::string& getDomain() const { return domain_; }
    void setDomain(const std::string& domain) { 
        domain_ = domain; 
        markModified();
    }
    
    Status getStatus() const { return status_; }
    void setStatus(Status status) { 
        status_ = status; 
        markModified();
    }
    
    // 제한 설정
    int getMaxUsers() const { return max_users_; }
    void setMaxUsers(int max_users) { 
        max_users_ = max_users; 
        markModified();
    }
    
    int getMaxDevices() const { return max_devices_; }
    void setMaxDevices(int max_devices) { 
        max_devices_ = max_devices; 
        markModified();
    }
    
    int getMaxDataPoints() const { return max_data_points_; }
    void setMaxDataPoints(int max_data_points) { 
        max_data_points_ = max_data_points; 
        markModified();
    }
    
    // 연락처 정보
    const std::string& getContactEmail() const { return contact_email_; }
    void setContactEmail(const std::string& contact_email) { 
        contact_email_ = contact_email; 
        markModified();
    }
    
    const std::string& getContactPhone() const { return contact_phone_; }
    void setContactPhone(const std::string& contact_phone) { 
        contact_phone_ = contact_phone; 
        markModified();
    }
    
    const std::string& getAddress() const { return address_; }
    void setAddress(const std::string& address) { 
        address_ = address; 
        markModified();
    }
    
    const std::string& getCity() const { return city_; }
    void setCity(const std::string& city) { 
        city_ = city; 
        markModified();
    }
    
    const std::string& getCountry() const { return country_; }
    void setCountry(const std::string& country) { 
        country_ = country; 
        markModified();
    }
    
    const std::string& getTimezone() const { return timezone_; }
    void setTimezone(const std::string& timezone) { 
        timezone_ = timezone; 
        markModified();
    }
    
    // 구독 정보
    std::chrono::system_clock::time_point getSubscriptionStart() const { return subscription_start_; }
    void setSubscriptionStart(const std::chrono::system_clock::time_point& subscription_start) { 
        subscription_start_ = subscription_start; 
        markModified();
    }
    
    std::chrono::system_clock::time_point getSubscriptionEnd() const { return subscription_end_; }
    void setSubscriptionEnd(const std::chrono::system_clock::time_point& subscription_end) { 
        subscription_end_ = subscription_end; 
        markModified();
    }
    
    // 타임스탬프
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::chrono::system_clock::time_point& created_at) { 
        created_at_ = created_at; 
        markModified();
    }
    
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }
    void setUpdatedAt(const std::chrono::system_clock::time_point& updated_at) { 
        updated_at_ = updated_at; 
        markModified();
    }

    // =======================================================================
    // 유효성 검사
    // =======================================================================
    
    bool isValid() const override {
        if (!BaseEntity<TenantEntity>::isValid()) return false;
        
        // 필수 필드 검사 (NOT NULL 컬럼들)
        if (name_.empty()) return false;
        if (domain_.empty()) return false;
        
        // 제한값 검사
        if (max_users_ <= 0 || max_devices_ <= 0 || max_data_points_ <= 0) return false;
        
        // 구독 기간 검사
        if (subscription_start_ >= subscription_end_) return false;
        
        // 도메인 형식 간단 검사
        if (domain_.find(".") == std::string::npos && domain_.length() < 3) return false;
        
        return true;
    }

    // =======================================================================
    // 비즈니스 로직 메서드들 (인라인)
    // =======================================================================
    
    bool isSubscriptionExpired() const {
        return std::chrono::system_clock::now() > subscription_end_;
    }
    
    void extendSubscription(int days) {
        auto extension = std::chrono::hours(24 * days);
        subscription_end_ += extension;
        markModified();
    }
    
    bool isWithinUserLimit(int current_users) const {
        return current_users <= max_users_;
    }
    
    bool isWithinDeviceLimit(int current_devices) const {
        return current_devices <= max_devices_;
    }
    
    bool isWithinDataPointLimit(int current_points) const {
        return current_points <= max_data_points_;
    }

    // =======================================================================
    // 정적 유틸리티 메서드들 (CPP에서 구현)
    // =======================================================================
    
    static std::string statusToString(Status status);
    static Status stringToStatus(const std::string& status_str);

    // =======================================================================
    // 고급 기능 메서드들 (CPP에서 구현)
    // =======================================================================
    
    json extractConfiguration() const;
    json getSubscriptionInfo() const;
    json getLimitInfo() const;

    // =======================================================================
    // 헬퍼 메서드들 (CPP에서 구현)
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Repository 접근자 (CPP에서 구현)
    std::shared_ptr<Repositories::TenantRepository> getRepository() const;

private:
    // =======================================================================
    // 멤버 변수들 (정규화된 스키마와 1:1 매핑)
    // =======================================================================
    
    // 기본 정보
    std::string name_;                              // VARCHAR(100) NOT NULL
    std::string description_;                       // TEXT
    std::string domain_;                            // VARCHAR(100) NOT NULL UNIQUE
    Status status_;                                 // VARCHAR(20) DEFAULT 'TRIAL'
    
    // 제한 설정
    int max_users_;                                 // INTEGER DEFAULT 10
    int max_devices_;                               // INTEGER DEFAULT 50
    int max_data_points_;                           // INTEGER DEFAULT 500
    
    // 연락처 정보
    std::string contact_email_;                     // VARCHAR(255)
    std::string contact_phone_;                     // VARCHAR(50)
    std::string address_;                           // TEXT
    std::string city_;                              // VARCHAR(100)
    std::string country_;                           // VARCHAR(100)
    std::string timezone_;                          // VARCHAR(50) DEFAULT 'UTC'
    
    // 구독 정보
    std::chrono::system_clock::time_point subscription_start_;    // DATETIME NOT NULL
    std::chrono::system_clock::time_point subscription_end_;      // DATETIME NOT NULL
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
    std::chrono::system_clock::time_point updated_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // TENANT_ENTITY_H