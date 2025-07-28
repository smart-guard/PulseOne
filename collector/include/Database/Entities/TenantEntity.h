#ifndef TENANT_ENTITY_H
#define TENANT_ENTITY_H

/**
 * @file TenantEntity.h
 * @brief PulseOne Tenant Entity - 테넌트 정보 엔티티 (DeviceEntity 패턴 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 DeviceEntity/DataPointEntity 패턴 100% 준수:
 * - BaseEntity<TenantEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화
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
 * @brief 테넌트 엔티티 클래스 (BaseEntity 템플릿 상속)
 */
class TenantEntity : public BaseEntity<TenantEntity> {
public:
    // =======================================================================
    // 테넌트 상태 열거형
    // =======================================================================
    
    enum class Status {
        ACTIVE,         // 활성
        SUSPENDED,      // 정지
        TRIAL,          // 체험
        EXPIRED,        // 만료
        DISABLED        // 비활성화
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 테넌트)
     */
    TenantEntity();
    
    /**
     * @brief ID로 생성자 (기존 테넌트 로드)
     * @param id 테넌트 ID
     */
    explicit TenantEntity(int id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~TenantEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
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
    std::string getTableName() const override { return "tenants"; }
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool isValid() const override;

    // =======================================================================
    // JSON 직렬화 (BaseEntity 순수 가상 함수)
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
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;

    // =======================================================================
    // Getter 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getDomain() const { return domain_; }
    Status getStatus() const { return status_; }
    int getMaxUsers() const { return max_users_; }
    int getMaxDevices() const { return max_devices_; }
    int getMaxDataPoints() const { return max_data_points_; }
    const std::string& getContactEmail() const { return contact_email_; }
    const std::string& getContactPhone() const { return contact_phone_; }
    const std::string& getAddress() const { return address_; }
    const std::string& getCity() const { return city_; }
    const std::string& getCountry() const { return country_; }
    const std::string& getTimezone() const { return timezone_; }
    const std::chrono::system_clock::time_point& getSubscriptionStart() const { return subscription_start_; }
    const std::chrono::system_clock::time_point& getSubscriptionEnd() const { return subscription_end_; }

    // =======================================================================
    // Setter 메서드들 (markModified 패턴 통일)
    // =======================================================================
    
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    void setDescription(const std::string& desc) { 
        description_ = desc; 
        markModified(); 
    }
    
    void setDomain(const std::string& domain) { 
        domain_ = domain; 
        markModified(); 
    }
    
    void setStatus(Status status) { 
        status_ = status; 
        markModified(); 
    }
    
    void setMaxUsers(int max_users) { 
        max_users_ = max_users; 
        markModified(); 
    }
    
    void setMaxDevices(int max_devices) { 
        max_devices_ = max_devices; 
        markModified(); 
    }
    
    void setMaxDataPoints(int max_data_points) { 
        max_data_points_ = max_data_points; 
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
    
    void setTimezone(const std::string& timezone) { 
        timezone_ = timezone; 
        markModified(); 
    }
    
    void setSubscriptionStart(const std::chrono::system_clock::time_point& start) { 
        subscription_start_ = start; 
        markModified(); 
    }
    
    void setSubscriptionEnd(const std::chrono::system_clock::time_point& end) { 
        subscription_end_ = end; 
        markModified(); 
    }

    // =======================================================================
    // 비즈니스 로직 메서드들 (DataPointEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 구독 만료 여부 확인
     * @return 만료되었으면 true
     */
    bool isSubscriptionExpired() const;
    
    /**
     * @brief 구독 연장
     * @param days 연장할 일수
     */
    void extendSubscription(int days);
    
    /**
     * @brief 사용자 제한 확인
     * @param current_users 현재 사용자 수
     * @return 제한 내이면 true
     */
    bool isWithinUserLimit(int current_users) const;
    
    /**
     * @brief 디바이스 제한 확인
     * @param current_devices 현재 디바이스 수
     * @return 제한 내이면 true
     */
    bool isWithinDeviceLimit(int current_devices) const;
    
    /**
     * @brief 데이터포인트 제한 확인
     * @param current_points 현재 데이터포인트 수
     * @return 제한 내이면 true
     */
    bool isWithinDataPointLimit(int current_points) const;
    
    /**
     * @brief 상태 문자열 변환
     * @param status 상태 열거형
     * @return 상태 문자열
     */
    static std::string statusToString(Status status);
    
    /**
     * @brief 문자열을 상태로 변환
     * @param status_str 상태 문자열
     * @return 상태 열거형
     */
    static Status stringToStatus(const std::string& status_str);

    // =======================================================================
    // 고급 기능 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 테넌트 설정을 JSON으로 추출
     * @return 설정 JSON
     */
    json extractConfiguration() const;
    
    /**
     * @brief 구독 정보 조회
     * @return 구독 정보
     */
    json getSubscriptionInfo() const;
    
    /**
     * @brief 제한 정보 조회
     * @return 제한 정보
     */
    json getLimitInfo() const;

private:
    // =======================================================================
    // 멤버 변수들 (DeviceEntity 패턴)
    // =======================================================================
    
    std::string name_;
    std::string description_;
    std::string domain_;            // 도메인명 (고유)
    Status status_;
    
    // 제한 설정
    int max_users_;
    int max_devices_;
    int max_data_points_;
    
    // 연락처 정보
    std::string contact_email_;
    std::string contact_phone_;
    std::string address_;
    std::string city_;
    std::string country_;
    std::string timezone_;
    
    // 구독 정보
    std::chrono::system_clock::time_point subscription_start_;
    std::chrono::system_clock::time_point subscription_end_;

    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief DB 행을 엔티티로 매핑
     * @param row DB 행 데이터
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
     * @brief 타임스탬프를 문자열로 변환
     * @param tp 타임스탬프
     * @return 문자열 표현
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    
    /**
     * @brief 문자열을 타임스탬프로 변환
     * @param str 문자열
     * @return 타임스탬프
     */
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // TENANT_ENTITY_H