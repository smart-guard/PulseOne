#ifndef VIRTUAL_POINT_ENTITY_H
#define VIRTUAL_POINT_ENTITY_H

/**
 * @file VirtualPointEntity.h
 * @brief PulseOne VirtualPoint Entity - 가상포인트 엔티티 (DeviceEntity 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔥 DeviceEntity 패턴 완전 적용:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - BaseEntity<VirtualPointEntity> 상속 (CRTP)
 * - virtual_points 테이블과 1:1 매핑
 * - timestampToString() 메서드 포함
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>

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
    class VirtualPointRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 가상포인트 엔티티 클래스 (BaseEntity 상속, DeviceEntity 패턴)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE virtual_points (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     tenant_id INTEGER NOT NULL,
 *     site_id INTEGER,
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     formula TEXT NOT NULL,
 *     data_type VARCHAR(20) DEFAULT 'float',
 *     unit VARCHAR(20),
 *     calculation_interval INTEGER DEFAULT 1000,
 *     calculation_trigger VARCHAR(20) DEFAULT 'timer',
 *     is_enabled BOOLEAN DEFAULT true,
 *     category VARCHAR(50),
 *     tags TEXT,
 *     created_by INTEGER,
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class VirtualPointEntity : public BaseEntity<VirtualPointEntity> {
public:
    // =======================================================================
    // 데이터 타입 열거형
    // =======================================================================
    
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
    
    enum class CalculationTrigger {
        TIMER,          // 주기적 계산
        ON_CHANGE,      // 입력값 변경 시
        MANUAL,         // 수동 계산
        EVENT           // 이벤트 기반
    };

    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    VirtualPointEntity();
    VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula);
    VirtualPointEntity(int tenant_id, int site_id, const std::string& name, 
                      const std::string& description, const std::string& formula,
                      DataType data_type, const std::string& unit, 
                      int calculation_interval, bool is_enabled);
    virtual ~VirtualPointEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (CPP에서 구현)
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    std::string getTableName() const override { 
        return "virtual_points"; 
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
    
    int getSiteId() const { return site_id_; }
    void setSiteId(int site_id) { 
        site_id_ = site_id; 
        markModified();
    }
    
    // 가상포인트 기본 정보
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
    
    const std::string& getFormula() const { return formula_; }
    void setFormula(const std::string& formula) { 
        formula_ = formula; 
        markModified();
    }
    
    DataType getDataType() const { return data_type_; }
    void setDataType(DataType data_type) { 
        data_type_ = data_type; 
        markModified();
    }
    
    const std::string& getUnit() const { return unit_; }
    void setUnit(const std::string& unit) { 
        unit_ = unit; 
        markModified();
    }
    
    // 계산 설정
    int getCalculationInterval() const { return calculation_interval_; }
    void setCalculationInterval(int calculation_interval) { 
        calculation_interval_ = calculation_interval; 
        markModified();
    }
    
    CalculationTrigger getCalculationTrigger() const { return calculation_trigger_; }
    void setCalculationTrigger(CalculationTrigger calculation_trigger) { 
        calculation_trigger_ = calculation_trigger; 
        markModified();
    }
    
    bool isEnabled() const { return is_enabled_; }
    void setEnabled(bool is_enabled) { 
        is_enabled_ = is_enabled; 
        markModified();
    }
    
    const std::string& getCategory() const { return category_; }
    void setCategory(const std::string& category) { 
        category_ = category; 
        markModified();
    }
    
    const std::vector<std::string>& getTags() const { return tags_; }
    void setTags(const std::vector<std::string>& tags) { 
        tags_ = tags; 
        markModified();
    }
    
    int getCreatedBy() const { return created_by_; }
    void setCreatedBy(int created_by) { 
        created_by_ = created_by; 
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
    // 계산 관련 메서드들 (VirtualPoint 전용 - CPP에서 구현)
    // =======================================================================
    
    bool validateFormula() const;
    std::optional<double> calculateValue(const std::map<std::string, double>& input_values) const;
    std::vector<std::string> extractVariableNames() const;
    bool hasCircularReference(const std::vector<int>& referenced_points) const;
    
    // 계산 결과 조회
    std::optional<double> getLastCalculatedValue() const { return last_calculated_value_; }
    const std::chrono::system_clock::time_point& getLastCalculationTime() const { return last_calculation_time_; }
    const std::string& getLastCalculationError() const { return last_calculation_error_; }

    // =======================================================================
    // 유효성 검사 (CPP에서 구현)
    // =======================================================================
    
    bool isValid() const override;

    // =======================================================================
    // 유틸리티 메서드들 (CPP에서 구현)
    // =======================================================================
    
    bool isSameTenant(const VirtualPointEntity& other) const;
    bool isSameSite(const VirtualPointEntity& other) const;
    bool needsCalculation() const;
    json toSummaryJson() const;

    // =======================================================================
    // 정적 유틸리티 메서드들 (CPP에서 구현)
    // =======================================================================
    
    static std::string dataTypeToString(DataType data_type);
    static DataType stringToDataType(const std::string& type_str);
    static std::string calculationTriggerToString(CalculationTrigger trigger);
    static CalculationTrigger stringToCalculationTrigger(const std::string& trigger_str);

    // =======================================================================
    // 비교 연산자들 (CPP에서 구현)
    // =======================================================================
    
    bool operator==(const VirtualPointEntity& other) const;
    bool operator!=(const VirtualPointEntity& other) const;
    bool operator<(const VirtualPointEntity& other) const;

    // =======================================================================
    // 출력 연산자 (CPP에서 구현)
    // =======================================================================
    
    friend std::ostream& operator<<(std::ostream& os, const VirtualPointEntity& entity);

    // =======================================================================
    // 🔥 DeviceEntity 패턴: 헬퍼 메서드들 (CPP에서 구현)
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Repository 접근자 (CPP에서 구현)
    std::shared_ptr<Repositories::VirtualPointRepository> getRepository() const;
    
private:
    // =======================================================================
    // 멤버 변수들 (정규화된 스키마와 1:1 매핑)
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                                 // NOT NULL, FOREIGN KEY to tenants.id
    int site_id_;                                   // FOREIGN KEY to sites.id
    
    // 가상포인트 기본 정보
    std::string name_;                              // VARCHAR(100) NOT NULL
    std::string description_;                       // TEXT
    std::string formula_;                           // TEXT NOT NULL (JavaScript 계산식)
    DataType data_type_;                            // VARCHAR(20) DEFAULT 'float'
    std::string unit_;                              // VARCHAR(20)
    int calculation_interval_;                      // INTEGER DEFAULT 1000 (ms)
    CalculationTrigger calculation_trigger_;        // VARCHAR(20) DEFAULT 'timer'
    bool is_enabled_;                               // BOOLEAN DEFAULT true
    std::string category_;                          // VARCHAR(50)
    std::vector<std::string> tags_;                 // TEXT (JSON 배열)
    
    // 메타데이터
    int created_by_;                                // INTEGER, FOREIGN KEY to users.id
    std::chrono::system_clock::time_point created_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
    std::chrono::system_clock::time_point updated_at_;    // DATETIME DEFAULT CURRENT_TIMESTAMP
    
    // 계산 관련 필드들 (확장)
    mutable std::optional<double> last_calculated_value_;     // 마지막 계산값 (캐시)
    mutable std::chrono::system_clock::time_point last_calculation_time_;  // 마지막 계산 시간
    mutable std::string last_calculation_error_;             // 마지막 계산 에러

    // =======================================================================
    // private 헬퍼 메서드들 (CPP에서 구현)
    // =======================================================================
    
    bool isFormulaSafe(const std::string& formula) const;
    std::string tagsToJsonString() const;
    void tagsFromJsonString(const std::string& json_str);
    double evaluateSimpleExpression(const std::string& expression) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H