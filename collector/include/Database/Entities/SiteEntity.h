#ifndef SITE_ENTITY_H
#define SITE_ENTITY_H

/**
 * @file SiteEntity.h
 * @brief PulseOne Site Entity - 사이트 정보 엔티티 (DeviceEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - BaseEntity<SiteEntity> 상속 (CRTP)
 * - sites 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <sstream>
#include <iomanip>


namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class SiteRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 사이트 엔티티 클래스 (BaseEntity 상속, 정규화된 스키마)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE sites (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     tenant_id INTEGER NOT NULL,
 *     parent_site_id INTEGER,
 *     
 *     -- 사이트 기본 정보
 *     name VARCHAR(100) NOT NULL,
 *     code VARCHAR(20) NOT NULL,
 *     site_type VARCHAR(20) NOT NULL,
 *     description TEXT,
 *     location VARCHAR(255),
 *     timezone VARCHAR(50) DEFAULT 'UTC',
 *     
 *     -- 주소 정보
 *     address TEXT,
 *     city VARCHAR(100),
 *     country VARCHAR(100),
 *     latitude REAL DEFAULT 0.0,
 *     longitude REAL DEFAULT 0.0,
 *     
 *     -- 계층 정보
 *     hierarchy_level INTEGER DEFAULT 1,
 *     hierarchy_path VARCHAR(255),
 *     
 *     -- 상태 정보
 *     is_active INTEGER DEFAULT 1,
 *     
 *     -- 담당자 정보
 *     contact_name VARCHAR(100),
 *     contact_email VARCHAR(255),
 *     contact_phone VARCHAR(50),
 *     
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class SiteEntity : public BaseEntity<SiteEntity> {
public:
    // =======================================================================
    // 사이트 타입 열거형 (DB 스키마와 일치)
    // =======================================================================
    
    enum class SiteType {
        COMPANY,        // 회사
        FACTORY,        // 공장
        OFFICE,         // 사무실
        BUILDING,       // 빌딩
        FLOOR,          // 층
        LINE,           // 라인
        AREA,           // 구역
        ZONE            // 존
    };

    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    SiteEntity();
    explicit SiteEntity(int site_id);
    virtual ~SiteEntity() = default;

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
            // 기본 식별자
            j["id"] = getId();
            j["tenant_id"] = tenant_id_;
            j["parent_site_id"] = parent_site_id_;
            
            // 사이트 기본 정보
            j["name"] = name_;
            j["code"] = code_;
            j["site_type"] = siteTypeToString(site_type_);
            j["description"] = description_;
            j["location"] = location_;
            j["timezone"] = timezone_;
            
            // 주소 정보
            j["address"] = address_;
            j["city"] = city_;
            j["country"] = country_;
            j["latitude"] = latitude_;
            j["longitude"] = longitude_;
            
            // 계층 정보
            j["hierarchy_level"] = hierarchy_level_;
            j["hierarchy_path"] = hierarchy_path_;
            
            // 상태 정보
            j["is_active"] = is_active_;
            
            // 담당자 정보
            j["contact_name"] = contact_name_;
            j["contact_email"] = contact_email_;
            j["contact_phone"] = contact_phone_;
            
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
            // 기본 식별자
            if (data.contains("id")) {
                setId(data["id"].get<int>());
            }
            if (data.contains("tenant_id")) {
                setTenantId(data["tenant_id"].get<int>());
            }
            if (data.contains("parent_site_id")) {
                setParentSiteId(data["parent_site_id"].get<int>());
            }
            
            // 사이트 기본 정보
            if (data.contains("name")) {
                setName(data["name"].get<std::string>());
            }
            if (data.contains("code")) {
                setCode(data["code"].get<std::string>());
            }
            if (data.contains("site_type")) {
                setSiteType(stringToSiteType(data["site_type"].get<std::string>()));
            }
            if (data.contains("description")) {
                setDescription(data["description"].get<std::string>());
            }
            if (data.contains("location")) {
                setLocation(data["location"].get<std::string>());
            }
            if (data.contains("timezone")) {
                setTimezone(data["timezone"].get<std::string>());
            }
            
            // 주소 정보
            if (data.contains("address")) {
                setAddress(data["address"].get<std::string>());
            }
            if (data.contains("city")) {
                setCity(data["city"].get<std::string>());
            }
            if (data.contains("country")) {
                setCountry(data["country"].get<std::string>());
            }
            if (data.contains("latitude")) {
                setLatitude(data["latitude"].get<double>());
            }
            if (data.contains("longitude")) {
                setLongitude(data["longitude"].get<double>());
            }
            
            // 계층 정보
            if (data.contains("hierarchy_level")) {
                setHierarchyLevel(data["hierarchy_level"].get<int>());
            }
            if (data.contains("hierarchy_path")) {
                setHierarchyPath(data["hierarchy_path"].get<std::string>());
            }
            
            // 상태 정보
            if (data.contains("is_active")) {
                setActive(data["is_active"].get<bool>());
            }
            
            // 담당자 정보
            if (data.contains("contact_name")) {
                setContactName(data["contact_name"].get<std::string>());
            }
            if (data.contains("contact_email")) {
                setContactEmail(data["contact_email"].get<std::string>());
            }
            if (data.contains("contact_phone")) {
                setContactPhone(data["contact_phone"].get<std::string>());
            }
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "SiteEntity[";
        oss << "id=" << getId();
        oss << ", tenant_id=" << tenant_id_;
        oss << ", name=" << name_;
        oss << ", code=" << code_;
        oss << ", type=" << siteTypeToString(site_type_);
        oss << ", location=" << location_;
        oss << ", active=" << (is_active_ ? "true" : "false");
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "sites"; 
    }

    // =======================================================================
    // 기본 속성 접근자 (인라인)
    // =======================================================================
    
    // ID 및 관계 정보
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified();
    }
    
    int getParentSiteId() const { return parent_site_id_; }
    void setParentSiteId(int parent_site_id) { 
        parent_site_id_ = parent_site_id; 
        markModified();
    }
    
    // 사이트 기본 정보
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { 
        name_ = name; 
        markModified();
    }
    
    const std::string& getCode() const { return code_; }
    void setCode(const std::string& code) { 
        code_ = code; 
        markModified();
    }
    
    SiteType getSiteType() const { return site_type_; }
    void setSiteType(SiteType site_type) { 
        site_type_ = site_type; 
        markModified();
    }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified();
    }
    
    const std::string& getLocation() const { return location_; }
    void setLocation(const std::string& location) { 
        location_ = location; 
        markModified();
    }
    
    const std::string& getTimezone() const { return timezone_; }
    void setTimezone(const std::string& timezone) { 
        timezone_ = timezone; 
        markModified();
    }
    
    // 주소 정보
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
    
    double getLatitude() const { return latitude_; }
    void setLatitude(double latitude) { 
        latitude_ = latitude; 
        markModified();
    }
    
    double getLongitude() const { return longitude_; }
    void setLongitude(double longitude) { 
        longitude_ = longitude; 
        markModified();
    }
    
    // GPS 좌표 유틸리티
    bool hasGpsCoordinates() const { 
        return latitude_ != 0.0 || longitude_ != 0.0; 
    }
    
    // 계층 정보
    int getHierarchyLevel() const { return hierarchy_level_; }
    void setHierarchyLevel(int hierarchy_level) { 
        hierarchy_level_ = hierarchy_level; 
        markModified();
    }
    
    const std::string& getHierarchyPath() const { return hierarchy_path_; }
    void setHierarchyPath(const std::string& hierarchy_path) { 
        hierarchy_path_ = hierarchy_path; 
        markModified();
    }
    
    bool isRootSite() const { return parent_site_id_ <= 0; }
    
    // 상태 정보
    bool isActive() const { return is_active_; }
    void setActive(bool is_active) { 
        is_active_ = is_active; 
        markModified();
    }
    
    // 담당자 정보
    const std::string& getContactName() const { return contact_name_; }
    void setContactName(const std::string& contact_name) { 
        contact_name_ = contact_name; 
        markModified();
    }
    
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
        if (!BaseEntity<SiteEntity>::isValid()) return false;
        
        // 필수 필드 검사 (NOT NULL 컬럼들)
        if (tenant_id_ <= 0) return false;
        if (name_.empty()) return false;
        if (code_.empty()) return false;
        
        // GPS 좌표 유효성 검사 (설정된 경우만)
        if (hasGpsCoordinates()) {
            if (latitude_ < -90.0 || latitude_ > 90.0) return false;
            if (longitude_ < -180.0 || longitude_ > 180.0) return false;
        }
        
        return true;
    }

    // =======================================================================
    // 정적 유틸리티 메서드들 (CPP에서 구현)
    // =======================================================================
    
    static std::string siteTypeToString(SiteType type);
    static SiteType stringToSiteType(const std::string& type_str);

    // =======================================================================
    // 비즈니스 로직 메서드들 (CPP에서 구현 - Repository 위임)
    // =======================================================================
    
    void updateHierarchyPath();
    bool hasChildSites();
    json extractConfiguration() const;
    json getLocationInfo() const;
    json getHierarchyInfo() const;

    // =======================================================================
    // 헬퍼 메서드들 (CPP에서 구현)
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Repository 접근자 (CPP에서 구현)
    std::shared_ptr<Repositories::SiteRepository> getRepository() const;

private:
    // =======================================================================
    // 멤버 변수들 (정규화된 스키마와 1:1 매핑)
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                                 // NOT NULL, FOREIGN KEY to tenants.id
    int parent_site_id_;                            // FOREIGN KEY to sites.id (자기 참조)
    
    // 사이트 기본 정보
    std::string name_;                              // VARCHAR(100) NOT NULL
    std::string code_;                              // VARCHAR(20) NOT NULL
    SiteType site_type_;                            // VARCHAR(20) NOT NULL
    std::string description_;                       // TEXT
    std::string location_;                          // VARCHAR(255)
    std::string timezone_;                          // VARCHAR(50) DEFAULT 'UTC'
    
    // 주소 정보
    std::string address_;                           // TEXT
    std::string city_;                              // VARCHAR(100)
    std::string country_;                           // VARCHAR(100)
    double latitude_;                               // REAL DEFAULT 0.0
    double longitude_;                              // REAL DEFAULT 0.0
    
    // 계층 정보
    int hierarchy_level_;                           // INTEGER DEFAULT 1
    std::string hierarchy_path_;                    // VARCHAR(255)
    
    // 상태 정보
    bool is_active_;                                // INTEGER DEFAULT 1
    
    // 담당자 정보
    std::string contact_name_;                      // VARCHAR(100)
    std::string contact_email_;                     // VARCHAR(255)
    std::string contact_phone_;                     // VARCHAR(50)
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
    std::chrono::system_clock::time_point updated_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SITE_ENTITY_H