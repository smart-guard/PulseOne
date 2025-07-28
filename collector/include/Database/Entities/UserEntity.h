#ifndef USER_ENTITY_H
#define USER_ENTITY_H

/**
 * @file UserEntity.h
 * @brief PulseOne User Entity - 사용자 정보 엔티티 (DeviceEntity 패턴 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 DeviceEntity/DataPointEntity 패턴 100% 준수:
 * - BaseEntity<UserEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화
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
 * @brief 사용자 엔티티 클래스 (BaseEntity 템플릿 상속)
 */
class UserEntity : public BaseEntity<UserEntity> {
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
    std::string getTableName() const override { return "users"; }
    
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

    // =======================================================================
    // 비즈니스 로직 메서드들 (DataPointEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 비밀번호 설정 (해싱 포함)
     * @param password 평문 비밀번호
     */
    void setPassword(const std::string& password);
    
    /**
     * @brief 비밀번호 검증
     * @param password 검증할 비밀번호
     * @return 일치하면 true
     */
    bool verifyPassword(const std::string& password) const;
    
    /**
     * @brief 로그인 시간 업데이트
     */
    void updateLastLogin();
    
    /**
     * @brief 권한 확인
     * @param permission 확인할 권한
     * @return 권한이 있으면 true
     */
    bool hasPermission(const std::string& permission) const;
    
    /**
     * @brief 권한 추가
     * @param permission 추가할 권한
     */
    void addPermission(const std::string& permission);
    
    /**
     * @brief 권한 제거
     * @param permission 제거할 권한
     */
    void removePermission(const std::string& permission);

    // =======================================================================
    // 고급 기능 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 사용자 설정을 JSON으로 추출
     * @return 설정 JSON
     */
    json extractConfiguration() const;
    
    /**
     * @brief 인증용 컨텍스트 조회
     * @return 인증 컨텍스트
     */
    json getAuthContext() const;
    
    /**
     * @brief 프로필 정보 조회
     * @return 프로필 정보
     */
    json getProfileInfo() const;

private:
    // =======================================================================
    // 멤버 변수들 (DeviceEntity 패턴)
    // =======================================================================
    
    std::string username_;
    std::string email_;
    std::string password_hash_;     // bcrypt 해시
    std::string full_name_;
    std::string role_;              // admin, engineer, operator, viewer
    int tenant_id_;
    bool is_enabled_;
    std::string phone_number_;
    std::string department_;
    std::vector<std::string> permissions_;
    std::chrono::system_clock::time_point last_login_at_;
    int login_count_;
    
    // 추가 메타데이터
    std::string notes_;
    std::string password_salt_;     // 추가 보안을 위한 솔트

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
     * @brief 비밀번호 해싱
     * @param password 평문 비밀번호
     * @return 해싱된 비밀번호
     */
    std::string hashPassword(const std::string& password) const;
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     * @param tp 타임스탬프
     * @return 문자열 표현
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // USER_ENTITY_H