#ifndef USER_ENTITY_H
#define USER_ENTITY_H

/**
 * @file UserEntity.h
 * @brief PulseOne User Entity - 사용자 정보 엔티티
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <vector>
#include <chrono>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 사용자 엔티티 클래스
 */
class UserEntity : public BaseEntity {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 사용자)
     */
    UserEntity();
    
    /**
     * @brief ID로 생성자 (기존 사용자 로드)
     * @param id 사용자 ID
     */
    explicit UserEntity(int id);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~UserEntity() = default;

    // =======================================================================
    // BaseEntity 인터페이스 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    std::string getTableName() const override { return "users"; }
    bool isValid() const override;

    // =======================================================================
    // Getter 메서드들
    // =======================================================================
    
    const std::string& getUsername() const { return username_; }
    const std::string& getEmail() const { return email_; }
    const std::string& getFullName() const { return full_name_; }
    const std::string& getRole() const { return role_; }
    int getTenantId() const { return tenant_id_; }
    bool isEnabled() const { return is_enabled_; }
    const std::chrono::system_clock::time_point& getLastLogin() const { return last_login_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::string& getPhoneNumber() const { return phone_number_; }
    const std::string& getDepartment() const { return department_; }
    const std::vector<std::string>& getPermissions() const { return permissions_; }

    // =======================================================================
    // Setter 메서드들
    // =======================================================================
    
    void setUsername(const std::string& username) { username_ = username; markModified(); }
    void setEmail(const std::string& email) { email_ = email; markModified(); }
    void setFullName(const std::string& full_name) { full_name_ = full_name; markModified(); }
    void setRole(const std::string& role) { role_ = role; markModified(); }
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setPhoneNumber(const std::string& phone) { phone_number_ = phone; markModified(); }
    void setDepartment(const std::string& dept) { department_ = dept; markModified(); }
    void setPermissions(const std::vector<std::string>& perms) { permissions_ = perms; markModified(); }

    // =======================================================================
    // 사용자 관련 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 비밀번호 설정 (해시 처리)
     * @param password 원본 비밀번호
     * @return 성공 시 true
     */
    bool setPassword(const std::string& password);
    
    /**
     * @brief 비밀번호 검증
     * @param password 입력된 비밀번호
     * @return 일치하면 true
     */
    bool verifyPassword(const std::string& password) const;
    
    /**
     * @brief 권한 확인
     * @param permission 확인할 권한명
     * @return 권한이 있으면 true
     */
    bool hasPermission(const std::string& permission) const;
    
    /**
     * @brief 권한 추가
     * @param permission 추가할 권한명
     */
    void addPermission(const std::string& permission);
    
    /**
     * @brief 권한 제거
     * @param permission 제거할 권한명
     */
    void removePermission(const std::string& permission);
    
    /**
     * @brief 마지막 로그인 시간 업데이트
     */
    void updateLastLogin();

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 사용자 기본 정보
    std::string username_;
    std::string email_;
    std::string full_name_;
    std::string password_hash_;
    std::string role_;                       // admin, user, viewer, operator
    int tenant_id_;                          // 외래키 (tenants.id)
    bool is_enabled_;
    
    // 추가 정보
    std::string phone_number_;
    std::string department_;
    
    // 시간 정보
    std::chrono::system_clock::time_point last_login_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    
    // 권한 정보
    std::vector<std::string> permissions_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 비밀번호 해시 생성
     * @param password 원본 비밀번호
     * @return 해시된 비밀번호
     */
    std::string hashPassword(const std::string& password) const;
    
    /**
     * @brief 데이터베이스 행을 멤버 변수로 매핑
     * @param row 데이터베이스 행
     */
    void mapRowToMembers(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 권한 문자열을 벡터로 파싱
     * @param permissions_str 콤마로 구분된 권한 문자열
     * @return 권한 벡터
     */
    std::vector<std::string> parsePermissions(const std::string& permissions_str) const;
    
    /**
     * @brief 권한 벡터를 문자열로 변환
     * @return 콤마로 구분된 권한 문자열
     */
    std::string permissionsToString() const;
    
    /**
     * @brief INSERT SQL 쿼리 생성
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 쿼리 생성
     */
    std::string buildUpdateSQL() const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // USER_ENTITY_H