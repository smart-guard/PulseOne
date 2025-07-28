#ifndef VIRTUAL_POINT_ENTITY_H
#define VIRTUAL_POINT_ENTITY_H

/**
 * @file VirtualPointEntity.h
 * @brief PulseOne VirtualPoint Entity - 가상포인트 엔티티 (SiteEntity 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 데이터베이스 스키마 (virtual_points 테이블):
 * - id: PRIMARY KEY AUTOINCREMENT
 * - tenant_id: INTEGER NOT NULL (FK to tenants)
 * - site_id: INTEGER (FK to sites, nullable)
 * - name: VARCHAR(100) NOT NULL
 * - description: TEXT
 * - formula: TEXT NOT NULL (JavaScript 계산식)
 * - data_type: VARCHAR(20) DEFAULT 'float'
 * - unit: VARCHAR(20)
 * - calculation_interval: INTEGER DEFAULT 1000 (ms)
 * - is_enabled: BOOLEAN DEFAULT true
 * - created_at: DATETIME DEFAULT CURRENT_TIMESTAMP
 * - updated_at: DATETIME DEFAULT CURRENT_TIMESTAMP
 */

#include "Database/Entities/BaseEntity.h"
#include "Database/DatabaseTypes.h"
#include "Utils/LogManager.h"
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <map>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief VirtualPoint Entity 클래스 (BaseEntity 템플릿 상속)
 */
class VirtualPointEntity : public BaseEntity<VirtualPointEntity> {
public:
    // =======================================================================
    // 데이터 타입 열거형 (기존 패턴 준수)
    // =======================================================================
    
    /**
     * @brief 가상포인트 데이터 타입 열거형
     */
    enum class DataType {
        INT16,
        INT32,
        UINT16,
        UINT32,
        FLOAT,
        DOUBLE,
        BOOLEAN,
        STRING,
        UNKNOWN
    };
    
    /**
     * @brief 계산 트리거 타입 열거형
     */
    enum class CalculationTrigger {
        TIMER,          // 주기적 계산 (calculation_interval 사용)
        ON_CHANGE,      // 입력값 변경 시 계산
        MANUAL,         // 수동 계산
        EVENT           // 이벤트 기반 계산
    };

private:
    // =======================================================================
    // 데이터베이스 필드들 (SiteEntity 패턴)
    // =======================================================================
    
    int tenant_id_;                 ///< 테넌트 ID (필수)
    int site_id_;                   ///< 사이트 ID (선택적, 0이면 전체 테넌트 범위)
    std::string name_;              ///< 가상포인트 이름 (필수)
    std::string description_;       ///< 설명
    std::string formula_;           ///< JavaScript 계산식 (필수)
    DataType data_type_;            ///< 데이터 타입
    std::string unit_;              ///< 단위
    int calculation_interval_;      ///< 계산 주기 (ms)
    CalculationTrigger calculation_trigger_;  ///< 계산 트리거 타입
    bool is_enabled_;              ///< 활성화 여부
    std::string category_;         ///< 카테고리
    std::vector<std::string> tags_; ///< 태그 목록
    
    // =======================================================================
    // 메타데이터 (SiteEntity 패턴)
    // =======================================================================
    
    int created_by_;               ///< 생성자 사용자 ID
    std::chrono::system_clock::time_point created_at_;  ///< 생성일시
    std::chrono::system_clock::time_point updated_at_;  ///< 수정일시
    
    // =======================================================================
    // 계산 관련 필드들 (확장)
    // =======================================================================
    
    mutable std::optional<double> last_calculated_value_;     ///< 마지막 계산값 (캐시)
    mutable std::chrono::system_clock::time_point last_calculation_time_;  ///< 마지막 계산 시간
    mutable std::string last_calculation_error_;             ///< 마지막 계산 에러

public:
    // =======================================================================
    // 생성자들 (SiteEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    VirtualPointEntity();
    
    /**
     * @brief 매개변수 생성자 (필수 필드들)
     * @param tenant_id 테넌트 ID
     * @param name 가상포인트 이름
     * @param formula 계산식
     */
    VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula);
    
    /**
     * @brief 전체 매개변수 생성자
     */
    VirtualPointEntity(int tenant_id, int site_id, const std::string& name, 
                      const std::string& description, const std::string& formula,
                      DataType data_type, const std::string& unit, 
                      int calculation_interval, bool is_enabled);
    
    /**
     * @brief 복사 생성자
     */
    VirtualPointEntity(const VirtualPointEntity& other) = default;
    
    /**
     * @brief 이동 생성자
     */
    VirtualPointEntity(VirtualPointEntity&& other) noexcept = default;
    
    /**
     * @brief 소멸자
     */
    virtual ~VirtualPointEntity() = default;

    // =======================================================================
    // 할당 연산자들 (SiteEntity 패턴)
    // =======================================================================
    
    VirtualPointEntity& operator=(const VirtualPointEntity& other) = default;
    VirtualPointEntity& operator=(VirtualPointEntity&& other) noexcept = default;

    // =======================================================================
    // Getter 메서드들 (SiteEntity 패턴)
    // =======================================================================
    
    int getTenantId() const { return tenant_id_; }
    int getSiteId() const { return site_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::string& getFormula() const { return formula_; }
    DataType getDataType() const { return data_type_; }
    const std::string& getUnit() const { return unit_; }
    int getCalculationInterval() const { return calculation_interval_; }
    CalculationTrigger getCalculationTrigger() const { return calculation_trigger_; }
    bool isEnabled() const { return is_enabled_; }
    const std::string& getCategory() const { return category_; }
    const std::vector<std::string>& getTags() const { return tags_; }
    
    int getCreatedBy() const { return created_by_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // Setter 메서드들 (SiteEntity 패턴)
    // =======================================================================
    
    void setTenantId(int tenant_id) { tenant_id_ = tenant_id; }
    void setSiteId(int site_id) { site_id_ = site_id; }
    void setName(const std::string& name) { name_ = name; }
    void setDescription(const std::string& description) { description_ = description; }
    void setFormula(const std::string& formula) { formula_ = formula; }
    void setDataType(DataType data_type) { data_type_ = data_type; }
    void setUnit(const std::string& unit) { unit_ = unit; }
    void setCalculationInterval(int interval) { calculation_interval_ = interval; }
    void setCalculationTrigger(CalculationTrigger trigger) { calculation_trigger_ = trigger; }
    void setEnabled(bool enabled) { is_enabled_ = enabled; }
    void setCategory(const std::string& category) { category_ = category; }
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; }
    
    void setCreatedBy(int created_by) { created_by_ = created_by; }
    void setCreatedAt(const std::chrono::system_clock::time_point& created_at) { created_at_ = created_at; }
    void setUpdatedAt(const std::chrono::system_clock::time_point& updated_at) { updated_at_ = updated_at; }

    // =======================================================================
    // 계산 관련 메서드들 (VirtualPoint 전용)
    // =======================================================================
    
    /**
     * @brief 수식 유효성 검사
     * @return 유효하면 true, 그렇지 않으면 false
     */
    bool validateFormula() const;
    
    /**
     * @brief 가상포인트 계산 실행
     * @param input_values 입력값들 (변수명 -> 값)
     * @return 계산 결과값, 실패 시 nullopt
     */
    std::optional<double> calculateValue(const std::map<std::string, double>& input_values) const;
    
    /**
     * @brief 수식에서 사용되는 변수명들 추출
     * @return 변수명 목록
     */
    std::vector<std::string> extractVariableNames() const;
    
    /**
     * @brief 순환 참조 검사 (다른 가상포인트 참조 시)
     * @param referenced_points 참조되는 가상포인트 ID 목록
     * @return 순환 참조가 있으면 true
     */
    bool hasCircularReference(const std::vector<int>& referenced_points) const;
    
    /**
     * @brief 마지막 계산값 조회
     * @return 계산값 (없으면 nullopt)
     */
    std::optional<double> getLastCalculatedValue() const { return last_calculated_value_; }
    
    /**
     * @brief 마지막 계산 시간 조회
     * @return 계산 시간
     */
    const std::chrono::system_clock::time_point& getLastCalculationTime() const { return last_calculation_time_; }
    
    /**
     * @brief 마지막 계산 에러 조회
     * @return 에러 메시지 (없으면 빈 문자열)
     */
    const std::string& getLastCalculationError() const { return last_calculation_error_; }

    // =======================================================================
    // 유틸리티 메서드들 (SiteEntity 패턴)
    // =======================================================================
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool isValid() const;
    
    /**
     * @brief 같은 테넌트인지 확인
     * @param other 다른 VirtualPointEntity
     * @return 같은 테넌트이면 true
     */
    bool isSameTenant(const VirtualPointEntity& other) const;
    
    /**
     * @brief 같은 사이트인지 확인
     * @param other 다른 VirtualPointEntity
     * @return 같은 사이트이면 true
     */
    bool isSameSite(const VirtualPointEntity& other) const;
    
    /**
     * @brief 계산이 필요한지 확인
     * @return 계산이 필요하면 true
     */
    bool needsCalculation() const;

    // =======================================================================
    // JSON 직렬화/역직렬화 (SiteEntity 패턴)
    // =======================================================================
    
    /**
     * @brief JSON으로 직렬화
     * @return JSON 객체
     */
    json toJson() const;
    
    /**
     * @brief JSON에서 역직렬화
     * @param j JSON 객체
     * @return 성공 시 true
     */
    bool fromJson(const json& j) override;
    
    /**
     * @brief 요약 JSON (목록용)
     * @return 요약된 JSON 객체
     */
    json toSummaryJson() const;

    // =======================================================================
    // 정적 유틸리티 메서드들 (SiteEntity 패턴)
    // =======================================================================
    
    /**
     * @brief DataType을 문자열로 변환
     * @param data_type 데이터 타입
     * @return 문자열 표현
     */
    static std::string dataTypeToString(DataType data_type);
    
    /**
     * @brief 문자열을 DataType으로 변환
     * @param type_str 데이터 타입 문자열
     * @return DataType (파싱 실패 시 UNKNOWN)
     */
    static DataType stringToDataType(const std::string& type_str);
    
    /**
     * @brief CalculationTrigger를 문자열로 변환
     * @param trigger 계산 트리거
     * @return 문자열 표현
     */
    static std::string calculationTriggerToString(CalculationTrigger trigger);
    
    /**
     * @brief 문자열을 CalculationTrigger로 변환
     * @param trigger_str 계산 트리거 문자열
     * @return CalculationTrigger (파싱 실패 시 TIMER)
     */
    static CalculationTrigger stringToCalculationTrigger(const std::string& trigger_str);

    // =======================================================================
    // 비교 연산자들 (SiteEntity 패턴)
    // =======================================================================
    
    bool operator==(const VirtualPointEntity& other) const;
    bool operator!=(const VirtualPointEntity& other) const;
    bool operator<(const VirtualPointEntity& other) const;  // 정렬용

    // =======================================================================
    // 출력 연산자 (SiteEntity 패턴)
    // =======================================================================
    
    friend std::ostream& operator<<(std::ostream& os, const VirtualPointEntity& entity);

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (필수)
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
    std::string getTableName() const override { return "virtual_points"; }
    
    /**
     * @brief 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;

private:
    // =======================================================================
    // private 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 수식 안전성 검사 (SQL Injection, Script Injection 방지)
     * @param formula 검사할 수식
     * @return 안전하면 true
     */
    bool isFormulaSafe(const std::string& formula) const;
    
    /**
     * @brief 태그 목록을 JSON 문자열로 변환
     * @return JSON 배열 문자열
     */
    std::string tagsToJsonString() const;
    
    /**
     * @brief JSON 문자열을 태그 목록으로 변환
     * @param json_str JSON 배열 문자열
     */
    void tagsFromJsonString(const std::string& json_str);
    
    /**
     * @brief 간단한 수식 계산기 (private 헬퍼)
     * @param expression 계산할 수식
     * @return 계산 결과
     */
    double evaluateSimpleExpression(const std::string& expression) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H