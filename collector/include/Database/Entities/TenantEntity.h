#ifndef TENANT_ENTITY_H
#define TENANT_ENTITY_H

/**
 * @file TenantEntity.h
 * @brief PulseOne Tenant Entity - 테넌트(조직) 엔티티
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <map>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 테넌트(조직) 엔티티 클래스
 */
class TenantEntity : public BaseEntity {
public:
    // 테넌트 상태 열거형
    enum class Status {
        ACTIVE = 1,
        INACTIVE = 2,
        SUSPENDED = 3,
        TRIAL = 4
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
    // BaseEntity 인터페이스 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    std::string getTableName() const override { return "tenants"; }
    bool isValid() const override;

    // =======================================================================
    // Getter 메서드들
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
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }

    // =======================================================================
    // Setter 메서드들
    // =======================================================================
    
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    void setDomain(const std::string& domain) { domain_ = domain; markModified(); }
    void setStatus(Status status) { status_ = status; markModified(); }
    void setMaxUsers(int max_users) { max_users_ = max_users; markModified(); }
    void setMaxDevices(int max_devices) { max_devices_ = max_devices; markModified(); }
    void setMaxDataPoints(int max_data_points) { max_data_points_ = max_data_points; markModified(); }
    void setContactEmail(const std::string& email) { contact_email_ = email; markModified(); }
    void setContactPhone(const std::string& phone) { contact_phone_ = phone; markModified(); }
    void setAddress(const std::string& address) { address_ = address; markModified(); }
    void setCity(const std::string& city) { city_ = city; markModified(); }
    void setCountry(const std::string& country) { country_ = country; markModified(); }
    void setTimezone(const std::string& timezone) { timezone_ = timezone; markModified(); }
    void setSubscriptionStart(const std::chrono::system_clock::time_point& start) { subscription_start_ = start; markModified(); }
    void setSubscriptionEnd(const std::chrono::system_clock::time_point& end) { subscription_end_ = end; markModified(); }

    // =======================================================================
    // 테넌트 관련 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 테넌트가 활성 상태인지 확인
     * @return 활성 상태이면 true
     */
    bool isActive() const;
    
    /**
     * @brief 구독이 만료되었는지 확인
     * @return 만료되었으면 true
     */
    bool isSubscriptionExpired() const;
    
    /**
     * @brief 사용자 수 제한 확인
     * @param current_user_count 현재 사용자 수
     * @return 제한 내이면 true
     */
    bool canAddUsers(int current_user_count, int users_to_add = 1) const;
    
    /**
     * @brief 디바이스 수 제한 확인
     * @param current_device_count 현재 디바이스 수
     * @return 제한 내이면 true
     */
    bool canAddDevices(int current_device_count, int devices_to_add = 1) const;
    
    /**
     * @brief 데이터포인트 수 제한 확인
     * @param current_point_count 현재 데이터포인트 수
     * @return 제한 내이면 true
     */
    bool canAddDataPoints(int current_point_count, int points_to_add = 1) const;
    
    /**
     * @brief 구독 기간 연장
     * @param additional_days 연장할 일수
     */
    void extendSubscription(int additional_days);
    
    /**
     * @brief 테넌트 상태를 문자열로 변환
     * @return 상태 문자열
     */
    std::string statusToString() const;
    
    /**
     * @brief 테넌트 사용량 통계 조회
     * @return 사용량 정보 맵
     */
    std::map<std::string, int> getUsageStats() const;
    
    /**
     * @brief 남은 구독 일수 계산
     * @return 남은 일수 (음수면 만료)
     */
    int getRemainingDays() const;

    // =======================================================================
    // 정적 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 문자열을 상태로 변환
     * @param status_str 상태 문자열
     * @return 상태 열거형
     */
    static Status stringToStatus(const std::string& status_str);
    
    /**
     * @brief 도메인 유효성 검사
     * @param domain 도메인 문자열
     * @return 유효하면 true
     */
    static bool isValidDomain(const std::string& domain);

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 테넌트 기본 정보
    std::string name_;                       // 테넌트 이름
    std::string description_;                // 설명
    std::string domain_;                     // 도메인 (예: acme.pulseone.com)
    Status status_;                          // 상태
    
    // 제한 설정
    int max_users_;                          // 최대 사용자 수
    int max_devices_;                        // 최대 디바이스 수
    int max_data_points_;                    // 최대 데이터포인트 수
    
    // 연락처 정보
    std::string contact_email_;              // 담당자 이메일
    std::string contact_phone_;              // 담당자 전화번호
    std::string address_;                    // 주소
    std::string city_;                       // 도시
    std::string country_;                    // 국가
    std::string timezone_;                   // 타임존 (예: Asia/Seoul)
    
    // 구독 정보
    std::chrono::system_clock::time_point subscription_start_;
    std::chrono::system_clock::time_point subscription_end_;
    
    // 시간 정보 (BaseEntity에서 상속)
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 멤버 변수로 매핑
     * @param row 데이터베이스 행
     */
    void mapRowToMembers(const std::map<std::string, std::string>& row);
    
    /**
     * @brief INSERT SQL 쿼리 생성
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 쿼리 생성
     */
    std::string buildUpdateSQL() const;
    
    /**
     * @brief 시간을 ISO 문자열로 변환
     * @param time_point 변환할 시간
     * @return ISO 형식 문자열
     */
    std::string timeToString(const std::chrono::system_clock::time_point& time_point) const;
    
    /**
     * @brief ISO 문자열을 시간으로 변환
     * @param time_str ISO 형식 문자열
     * @return 시간 포인트
     */
    std::chrono::system_clock::time_point stringToTime(const std::string& time_str) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // TENANT_ENTITY_H