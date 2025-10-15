/**
 * @file ExportTargetEntity.h
 * @brief Export Target Entity - BaseEntity 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.1
 * 저장 위치: core/shared/include/Database/Entities/ExportTargetEntity.h
 * 
 * 🔧 수정: toJson() 반환타입, override 제거
 */

#ifndef EXPORT_TARGET_ENTITY_H
#define EXPORT_TARGET_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>  // 🔧 추가

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;  // 🔧 추가

/**
 * @brief Export Target Entity
 * 
 * export_targets 테이블과 매핑
 */
class ExportTargetEntity : public BaseEntity<ExportTargetEntity> {
public:
    // =======================================================================
    // 생성자
    // =======================================================================
    
    ExportTargetEntity() = default;
    
    explicit ExportTargetEntity(int id) : BaseEntity(id) {}
    
    virtual ~ExportTargetEntity() = default;
    
    // =======================================================================
    // Getters
    // =======================================================================
    
    int getProfileId() const { return profile_id_; }
    std::string getName() const { return name_; }
    std::string getTargetType() const { return target_type_; }
    std::string getDescription() const { return description_; }
    bool isEnabled() const { return is_enabled_; }
    std::string getConfig() const { return config_; }
    std::string getExportMode() const { return export_mode_; }
    int getExportInterval() const { return export_interval_; }
    int getBatchSize() const { return batch_size_; }
    
    // 통계
    uint64_t getTotalExports() const { return total_exports_; }
    uint64_t getSuccessfulExports() const { return successful_exports_; }
    uint64_t getFailedExports() const { return failed_exports_; }
    
    std::optional<std::chrono::system_clock::time_point> getLastExportAt() const { 
        return last_export_at_; 
    }
    std::optional<std::chrono::system_clock::time_point> getLastSuccessAt() const { 
        return last_success_at_; 
    }
    std::string getLastError() const { return last_error_; }
    std::optional<std::chrono::system_clock::time_point> getLastErrorAt() const { 
        return last_error_at_; 
    }
    int getAvgExportTimeMs() const { return avg_export_time_ms_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    
    void setProfileId(int profile_id) { 
        profile_id_ = profile_id; 
        markModified(); 
    }
    
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    void setTargetType(const std::string& target_type) { 
        target_type_ = target_type; 
        markModified(); 
    }
    
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified(); 
    }
    
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified(); 
    }
    
    void setConfig(const std::string& config) { 
        config_ = config; 
        markModified(); 
    }
    
    void setExportMode(const std::string& mode) { 
        export_mode_ = mode; 
        markModified(); 
    }
    
    void setExportInterval(int interval) { 
        export_interval_ = interval; 
        markModified(); 
    }
    
    void setBatchSize(int size) { 
        batch_size_ = size; 
        markModified(); 
    }
    
    // 통계 업데이트
    void incrementTotalExports() { 
        total_exports_++; 
        markModified(); 
    }
    
    void incrementSuccessfulExports() { 
        successful_exports_++; 
        markModified(); 
    }
    
    void incrementFailedExports() { 
        failed_exports_++; 
        markModified(); 
    }
    
    void setLastExportAt(const std::chrono::system_clock::time_point& time) { 
        last_export_at_ = time; 
        markModified(); 
    }
    
    void setLastSuccessAt(const std::chrono::system_clock::time_point& time) { 
        last_success_at_ = time; 
        markModified(); 
    }
    
    void setLastError(const std::string& error) { 
        last_error_ = error; 
        markModified(); 
    }
    
    void setLastErrorAt(const std::chrono::system_clock::time_point& time) { 
        last_error_at_ = time; 
        markModified(); 
    }
    
    void setAvgExportTimeMs(int time_ms) { 
        avg_export_time_ms_ = time_ms; 
        markModified(); 
    }
    
    // =======================================================================
    // 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 성공률 계산
     */
    double getSuccessRate() const {
        if (total_exports_ == 0) return 0.0;
        return (static_cast<double>(successful_exports_) / total_exports_) * 100.0;
    }
    
    /**
     * @brief 타겟 상태 확인
     */
    bool isHealthy() const {
        return is_enabled_ && (getSuccessRate() > 50.0);
    }
    
    /**
     * @brief 유효성 검증
     */
    bool validate() const {  // 🔧 override 제거!
        if (name_.empty()) return false;
        if (target_type_.empty()) return false;
        if (config_.empty()) return false;
        return true;
    }
    
    /**
     * @brief JSON 변환
     */
    json toJson() const override {  // 🔧 반환타입 json으로 변경!
        json j;
        j["id"] = id_;
        j["profile_id"] = profile_id_;
        j["name"] = name_;
        j["target_type"] = target_type_;
        j["description"] = description_;
        j["is_enabled"] = is_enabled_;
        j["config"] = config_;
        j["export_mode"] = export_mode_;
        j["export_interval"] = export_interval_;
        j["batch_size"] = batch_size_;
        j["total_exports"] = total_exports_;
        j["successful_exports"] = successful_exports_;
        j["failed_exports"] = failed_exports_;
        j["success_rate"] = getSuccessRate();
        j["last_error"] = last_error_;
        j["avg_export_time_ms"] = avg_export_time_ms_;
        
        if (last_export_at_.has_value()) {
            j["last_export_at"] = std::chrono::system_clock::to_time_t(last_export_at_.value());
        }
        if (last_success_at_.has_value()) {
            j["last_success_at"] = std::chrono::system_clock::to_time_t(last_success_at_.value());
        }
        if (last_error_at_.has_value()) {
            j["last_error_at"] = std::chrono::system_clock::to_time_t(last_error_at_.value());
        }
        
        return j;  // 🔧 json 객체 직접 반환!
    }
    
    // 🔧 BaseEntity 나머지 순수가상함수 구현 (CPP에서)
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    bool fromJson(const json& data) override;
    std::string toString() const override { return toJson().dump(2); }
    
    /**
     * @brief 엔티티 타입 이름
     */
    std::string getEntityTypeName() const {  // 🔧 override 제거!
        return "ExportTarget";
    }
    
    std::string getTableName() const override { return "export_targets"; }
private:
    // 기본 필드
    int profile_id_ = 0;
    std::string name_;
    std::string target_type_;  // "HTTP", "S3", "FILE", "MQTT"
    std::string description_;
    bool is_enabled_ = true;
    std::string config_;  // JSON 문자열
    
    // Export 설정
    std::string export_mode_ = "on_change";  // "on_change", "periodic", "both"
    int export_interval_ = 0;  // 초 단위
    int batch_size_ = 100;
    
    // 통계
    uint64_t total_exports_ = 0;
    uint64_t successful_exports_ = 0;
    uint64_t failed_exports_ = 0;
    std::optional<std::chrono::system_clock::time_point> last_export_at_;
    std::optional<std::chrono::system_clock::time_point> last_success_at_;
    std::string last_error_;
    std::optional<std::chrono::system_clock::time_point> last_error_at_;
    int avg_export_time_ms_ = 0;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_TARGET_ENTITY_H