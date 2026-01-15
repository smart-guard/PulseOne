// =============================================================================
// collector/include/Database/Entities/VirtualPointEntity.h
// PulseOne VirtualPointEntity - DB 스키마와 완전 동기화
// =============================================================================

#ifndef VIRTUAL_POINT_ENTITY_H
#define VIRTUAL_POINT_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <optional>
#include <vector>
#include <chrono>


namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief VirtualPointEntity - DB 스키마와 완전 동기화된 가상포인트 엔티티
 */
class VirtualPointEntity : public BaseEntity<VirtualPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    VirtualPointEntity() = default;
    explicit VirtualPointEntity(int id) : BaseEntity(id) {}
    virtual ~VirtualPointEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    std::string getTableName() const override { return "virtual_points"; }

    // Repository access
    std::shared_ptr<Repositories::VirtualPointRepository> getRepository() const;

    // =======================================================================
    // Getter/Setter - DB 스키마와 완전 일치
    // =======================================================================
    
    // 기본 필드
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int id) { tenant_id_ = id; markModified(); }
    
    const std::string& getScopeType() const { return scope_type_; }
    void setScopeType(const std::string& type) { scope_type_ = type; markModified(); }
    
    const std::optional<int>& getSiteId() const { return site_id_; }
    void setSiteId(const std::optional<int>& id) { site_id_ = id; markModified(); }
    
    const std::optional<int>& getDeviceId() const { return device_id_; }
    void setDeviceId(const std::optional<int>& id) { device_id_ = id; markModified(); }
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; markModified(); }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; markModified(); }
    
    const std::string& getFormula() const { return formula_; }
    void setFormula(const std::string& formula) { formula_ = formula; markModified(); }
    
    const std::string& getDataType() const { return data_type_; }
    void setDataType(const std::string& type) { data_type_ = type; markModified(); }
    
    const std::string& getUnit() const { return unit_; }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    
    // 계산 설정
    int getCalculationInterval() const { return calculation_interval_; }
    void setCalculationInterval(int interval) { calculation_interval_ = interval; markModified(); }
    
    const std::string& getCalculationTrigger() const { return calculation_trigger_; }
    void setCalculationTrigger(const std::string& trigger) { calculation_trigger_ = trigger; markModified(); }
    
    bool getIsEnabled() const { return is_enabled_; }
    void setIsEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    
    const std::string& getCategory() const { return category_; }
    void setCategory(const std::string& cat) { category_ = cat; markModified(); }
    
    const std::string& getTags() const { return tags_; }
    void setTags(const std::string& tags) { tags_ = tags; markModified(); }
    
    // v3.0.0 확장 필드들 - DB 스키마와 일치
    const std::string& getExecutionType() const { return execution_type_; }
    void setExecutionType(const std::string& type) { execution_type_ = type; markModified(); }
    
    const std::string& getDependencies() const { return dependencies_; }
    void setDependencies(const std::string& deps) { dependencies_ = deps; markModified(); }
    
    int getCacheDurationMs() const { return cache_duration_ms_; }
    void setCacheDurationMs(int duration) { cache_duration_ms_ = duration; markModified(); }
    
    const std::string& getErrorHandling() const { return error_handling_; }
    void setErrorHandling(const std::string& handling) { error_handling_ = handling; markModified(); }
    
    const std::string& getLastError() const { return last_error_; }
    void setLastError(const std::string& error) { last_error_ = error; markModified(); }
    
    int getExecutionCount() const { return execution_count_; }
    void setExecutionCount(int count) { execution_count_ = count; markModified(); }
    
    double getAvgExecutionTimeMs() const { return avg_execution_time_ms_; }
    void setAvgExecutionTimeMs(double time) { avg_execution_time_ms_ = time; markModified(); }
    
    // 실행 시간 관련 - DB 스키마에 실제 존재
    const std::optional<std::chrono::system_clock::time_point>& getLastExecutionTime() const { 
        return last_execution_time_; 
    }
    void setLastExecutionTime(const std::optional<std::chrono::system_clock::time_point>& time) { 
        last_execution_time_ = time; markModified(); 
    }
    
    // 스크립트 라이브러리 연동 - DB 스키마에 실제 존재
    const std::optional<int>& getScriptLibraryId() const { return script_library_id_; }
    void setScriptLibraryId(const std::optional<int>& id) { script_library_id_ = id; markModified(); }
    
    // 성능 추적 설정 - DB 스키마에 실제 존재
    bool getPerformanceTrackingEnabled() const { return performance_tracking_enabled_; }
    void setPerformanceTrackingEnabled(bool enabled) { performance_tracking_enabled_ = enabled; markModified(); }
    
    bool getLogCalculations() const { return log_calculations_; }
    void setLogCalculations(bool log) { log_calculations_ = log; markModified(); }
    
    bool getLogErrors() const { return log_errors_; }
    void setLogErrors(bool log) { log_errors_ = log; markModified(); }
    
    // 알람 연동 - DB 스키마에 실제 존재
    bool getAlarmEnabled() const { return alarm_enabled_; }
    void setAlarmEnabled(bool enabled) { alarm_enabled_ = enabled; markModified(); }
    
    const std::optional<double>& getHighLimit() const { return high_limit_; }
    void setHighLimit(const std::optional<double>& limit) { high_limit_ = limit; markModified(); }
    
    const std::optional<double>& getLowLimit() const { return low_limit_; }
    void setLowLimit(const std::optional<double>& limit) { low_limit_ = limit; markModified(); }
    
    double getDeadband() const { return deadband_; }
    void setDeadband(double deadband) { deadband_ = deadband; markModified(); }
    
    // 감사 필드 - DB 스키마와 일치
    const std::optional<int>& getCreatedBy() const { return created_by_; }
    void setCreatedBy(const std::optional<int>& user_id) { created_by_ = user_id; markModified(); }
    
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // JSON 변환 및 유틸리티 메서드
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& j) override;
    std::string toString() const override;
    
    std::vector<std::string> getTagList() const;
    bool hasTag(const std::string& tag) const;
    bool validate() const;

private:
    // =======================================================================
    // 멤버 변수 - DB 스키마와 완전 일치
    // =======================================================================
    
    // 기본 필드 (DB 스키마 순서대로)
    int tenant_id_ = 0;
    std::string scope_type_ = "tenant";              // scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant'
    std::optional<int> site_id_;                     // site_id INTEGER
    std::optional<int> device_id_;                   // device_id INTEGER
    
    std::string name_;                               // name VARCHAR(100) NOT NULL
    std::string description_;                        // description TEXT
    std::string formula_;                            // formula TEXT NOT NULL
    std::string data_type_ = "float";                // data_type VARCHAR(20) NOT NULL DEFAULT 'float'
    std::string unit_;                               // unit VARCHAR(20)
    
    // 계산 설정
    int calculation_interval_ = 1000;                // calculation_interval INTEGER DEFAULT 1000
    std::string calculation_trigger_ = "timer";     // calculation_trigger VARCHAR(20) DEFAULT 'timer'
    bool is_enabled_ = true;                         // is_enabled INTEGER DEFAULT 1
    
    std::string category_;                           // category VARCHAR(50)
    std::string tags_ = "[]";                        // tags TEXT (JSON)
    
    // v3.0.0 확장 필드들 - DB 스키마와 일치
    std::string execution_type_ = "javascript";     // execution_type VARCHAR(20) DEFAULT 'javascript'
    std::string dependencies_ = "[]";                // dependencies TEXT (JSON)
    int cache_duration_ms_ = 0;                      // cache_duration_ms INTEGER DEFAULT 0
    std::string error_handling_ = "return_null";    // error_handling VARCHAR(20) DEFAULT 'return_null'
    std::string last_error_;                         // last_error TEXT
    int execution_count_ = 0;                        // execution_count INTEGER DEFAULT 0
    double avg_execution_time_ms_ = 0.0;             // avg_execution_time_ms REAL DEFAULT 0.0
    std::optional<std::chrono::system_clock::time_point> last_execution_time_; // last_execution_time DATETIME
    
    // 스크립트 라이브러리 연동
    std::optional<int> script_library_id_;           // script_library_id INTEGER
    
    // 성능 추적 설정
    bool performance_tracking_enabled_ = true;      // performance_tracking_enabled INTEGER DEFAULT 1
    bool log_calculations_ = false;                  // log_calculations INTEGER DEFAULT 0
    bool log_errors_ = true;                         // log_errors INTEGER DEFAULT 1
    
    // 알람 연동
    bool alarm_enabled_ = false;                     // alarm_enabled INTEGER DEFAULT 0
    std::optional<double> high_limit_;               // high_limit REAL
    std::optional<double> low_limit_;                // low_limit REAL
    double deadband_ = 0.0;                          // deadband REAL DEFAULT 0.0
    
    // 감사 필드
    std::optional<int> created_by_;                  // created_by INTEGER
    std::chrono::system_clock::time_point created_at_; // created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    std::chrono::system_clock::time_point updated_at_; // updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    
    // =======================================================================
    // 헬퍼 메서드
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimestamp(const std::string& str) const;
};

} // namespace Entities
} // namespace Database  
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENTITY_H