#ifndef SITE_ENTITY_H
#define SITE_ENTITY_H

/**
 * @file SiteEntity.h
 * @brief PulseOne Site Entity - 사이트 정보 엔티티 (TenantEntity/UserEntity 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 기존 패턴 100% 준수:
 * - BaseEntity<SiteEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화
 * - TenantEntity와 동일한 구조/네이밍
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <chrono>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 사이트 엔티티 클래스 (BaseEntity 템플릿 상속)
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
    // 생성자 및 소멸자 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 사이트)
     */
    SiteEntity();
    
    /**
     * @brief ID로 생성자 (기존 사이트 로드)
     * @param id 사이트 ID
     */
    explicit SiteEntity(int id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~SiteEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief DB에서 엔티티 로드
     * @return 성공 시 true
     */
    bool loadFromDatabase() override;
    
    /**
     * @brief DB에 엔티티 저장
     * @return 성공 시 true
     */
    bool saveToDatabase() override;
    
    /**
     * @brief DB에 엔티티 업데이트
     * @return 성공 시 true
     */
    bool updateToDatabase() override;
    
    /**
     * @brief DB에서 엔티티 삭제
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override;
    
    /**
     * @brief 테이블명 반환
     * @return 테이블명
     */
    std::string getTableName() const override { return "sites"; }
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool isValid() const override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief JSON으로 변환
     * @return JSON 객체
     */
    json toJson() const override;
    
    /**
     * @brief JSON에서 로드
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override;
    
    /**
     * @brief 문자열 표현
     * @return 사이트 정보 문자열
     */
    std::string toString() const override;

    // =======================================================================
    // Getter 메서드들 (TenantEntity 패턴)
    // =======================================================================
    
    int getTenantId() const { return tenant_id_; }
    int getParentSiteId() const { return parent_site_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getCode() const { return code_; }
    SiteType getSiteType() const { return site_type_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getLocation() const { return location_; }
    const std::string& getTimezone() const { return timezone_; }
    const std::string& getAddress() const { return address_; }
    const std::string& getCity() const { return city_; }
    const std::string& getCountry() const { return country_; }
    double getLatitude() const { return latitude_; }
    double getLongitude() const { return longitude_; }
    int getHierarchyLevel() const { return hierarchy_level_; }
    const std::string& getHierarchyPath() const { return hierarchy_path_; }
    bool isActive() const { return is_active_; }
    const std::string& getContactName() const { return contact_name_; }
    const std::string& getContactEmail() const { return contact_email_; }
    const std::string& getContactPhone() const { return contact_phone_; }

    // =======================================================================
    // Setter 메서드들 (markModified 패턴 적용)
    // =======================================================================
    
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified(); 
    }
    
    void setParentSiteId(int parent_site_id) { 
        parent_site_id_ = parent_site_id; 
        markModified(); 
    }
    
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    void setCode(const std::string& code) { 
        code_ = code; 
        markModified(); 
    }
    
    void setSiteType(SiteType site_type) { 
        site_type_ = site_type; 
        markModified(); 
    }
    
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified(); 
    }
    
    void setLocation(const std::string& location) { 
        location_ = location; 
        markModified(); 
    }
    
    void setTimezone(const std::string& timezone) { 
        timezone_ = timezone; 
        markModified(); 
    }
    
    void setAddress(const std::string& address) { 
        address_ = address; 
        markModified(); 
    }
    
    void setCity(const std::string& city) { 
        city_ = city; 
        markModified(); 
    }
    
    void setCountry(const std::string& country) { 
        country_ = country; 
        markModified(); 
    }
    
    void setLatitude(double latitude) { 
        latitude_ = latitude; 
        markModified(); 
    }
    
    void setLongitude(double longitude) { 
        longitude_ = longitude; 
        markModified(); 
    }
    
    void setHierarchyLevel(int level) { 
        hierarchy_level_ = level; 
        markModified(); 
    }
    
    void setHierarchyPath(const std::string& path) { 
        hierarchy_path_ = path; 
        markModified(); 
    }
    
    void setActive(bool active) { 
        is_active_ = active; 
        markModified(); 
    }
    
    void setContactName(const std::string& name) { 
        contact_name_ = name; 
        markModified(); 
    }
    
    void setContactEmail(const std::string& email) { 
        contact_email_ = email; 
        markModified(); 
    }
    
    void setContactPhone(const std::string& phone) { 
        contact_phone_ = phone; 
        markModified(); 
    }

    // =======================================================================
    // 비즈니스 로직 메서드들 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 사이트 타입을 문자열로 변환
     * @param type 사이트 타입
     * @return 문자열
     */
    static std::string siteTypeToString(SiteType type);
    
    /**
     * @brief 문자열을 사이트 타입으로 변환
     * @param type_str 사이트 타입 문자열
     * @return 사이트 타입
     */
    static SiteType stringToSiteType(const std::string& type_str);
    
    /**
     * @brief 계층 경로 업데이트
     */
    void updateHierarchyPath();
    
    /**
     * @brief 루트 사이트인지 확인
     * @return 루트 사이트면 true
     */
    bool isRootSite() const { return parent_site_id_ <= 0; }
    
    /**
     * @brief 하위 사이트가 있는지 확인 (DB 조회)
     * @return 하위 사이트가 있으면 true
     */
    bool hasChildSites();
    
    /**
     * @brief GPS 좌표가 설정되어 있는지 확인
     * @return GPS 좌표가 있으면 true
     */
    bool hasGpsCoordinates() const { 
        return latitude_ != 0.0 || longitude_ != 0.0; 
    }

    // =======================================================================
    // 고급 기능 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 사이트 설정을 JSON으로 추출
     * @return 설정 JSON
     */
    json extractConfiguration() const;
    
    /**
     * @brief 사이트 위치 정보를 JSON으로 추출
     * @return 위치 정보 JSON
     */
    json getLocationInfo() const;
    
    /**
     * @brief 사이트 계층 정보를 JSON으로 추출
     * @return 계층 정보 JSON
     */
    json getHierarchyInfo() const;

private:
    // =======================================================================
    // private 멤버 변수들 (DB 스키마와 일치)
    // =======================================================================
    
    int tenant_id_;                 // 테넌트 ID (FK)
    int parent_site_id_;            // 상위 사이트 ID (자기 참조 FK)
    std::string name_;              // 사이트명
    std::string code_;              // 사이트 코드
    SiteType site_type_;            // 사이트 타입
    std::string description_;       // 설명
    std::string location_;          // 위치
    std::string timezone_;          // 시간대
    std::string address_;           // 주소
    std::string city_;              // 도시
    std::string country_;           // 국가
    double latitude_;               // 위도
    double longitude_;              // 경도
    int hierarchy_level_;           // 계층 레벨
    std::string hierarchy_path_;    // 계층 경로
    bool is_active_;                // 활성 상태
    std::string contact_name_;      // 담당자명
    std::string contact_email_;     // 담당자 이메일
    std::string contact_phone_;     // 담당자 전화번호

    // =======================================================================
    // private 헬퍼 메서드들 (TenantEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 매핑
     * @param row 데이터베이스 행
     * @return 성공 시 true
     */
    bool mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief INSERT SQL 생성
     * @return INSERT SQL 문
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 생성
     * @return UPDATE SQL 문
     */
    std::string buildUpdateSQL() const;
    
    /**
     * @brief 시간 문자열 변환
     * @param tp 시간 포인트
     * @return 문자열
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 시간으로 변환
     * @param str 시간 문자열
     * @return 시간 포인트
     */
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SITE_ENTITY_H