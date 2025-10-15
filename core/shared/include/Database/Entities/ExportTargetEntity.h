/**
 * @file ExportTargetEntity.h
 * @version 2.0.0 - inline 구현 제거
 */

#ifndef EXPORT_TARGET_ENTITY_H
#define EXPORT_TARGET_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;

class ExportTargetEntity : public BaseEntity<ExportTargetEntity> {
public:
    // ✅ 선언만 (cpp로 이동)
    ExportTargetEntity();
    explicit ExportTargetEntity(int id);
    virtual ~ExportTargetEntity();
    
    // ✅ Repository 패턴
    std::shared_ptr<Repositories::ExportTargetRepository> getRepository() const;
    
    // Getters (inline 허용)
    int getProfileId() const { return profile_id_; }
    std::string getName() const { return name_; }
    std::string getTargetType() const { return target_type_; }
    std::string getDescription() const { return description_; }
    bool isEnabled() const { return is_enabled_; }
    std::string getConfig() const { return config_; }
    std::string getExportMode() const { return export_mode_; }
    int getExportInterval() const { return export_interval_; }
    int getBatchSize() const { return batch_size_; }
    uint64_t getTotalExports() const { return total_exports_; }
    uint64_t getSuccessfulExports() const { return successful_exports_; }
    uint64_t getFailedExports() const { return failed_exports_; }
    std::optional<std::chrono::system_clock::time_point> getLastExportAt() const { return last_export_at_; }
    std::optional<std::chrono::system_clock::time_point> getLastSuccessAt() const { return last_success_at_; }
    std::string getLastError() const { return last_error_; }
    std::optional<std::chrono::system_clock::time_point> getLastErrorAt() const { return last_error_at_; }
    int getAvgExportTimeMs() const { return avg_export_time_ms_; }
    
    // Setters (inline 허용)
    void setProfileId(int profile_id) { profile_id_ = profile_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setTargetType(const std::string& target_type) { target_type_ = target_type; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setConfig(const std::string& config) { config_ = config; markModified(); }
    void setExportMode(const std::string& mode) { export_mode_ = mode; markModified(); }
    void setExportInterval(int interval) { export_interval_ = interval; markModified(); }
    void setBatchSize(int size) { batch_size_ = size; markModified(); }
    void incrementTotalExports() { total_exports_++; markModified(); }
    void incrementSuccessfulExports() { successful_exports_++; markModified(); }
    void incrementFailedExports() { failed_exports_++; markModified(); }
    void setLastExportAt(const std::chrono::system_clock::time_point& time) { last_export_at_ = time; markModified(); }
    void setLastSuccessAt(const std::chrono::system_clock::time_point& time) { last_success_at_ = time; markModified(); }
    void setLastError(const std::string& error) { last_error_ = error; markModified(); }
    void setLastErrorAt(const std::chrono::system_clock::time_point& time) { last_error_at_ = time; markModified(); }
    void setAvgExportTimeMs(int time_ms) { avg_export_time_ms_ = time_ms; markModified(); }
    
    // ✅ 비즈니스 로직 선언만 (cpp로 이동)
    double getSuccessRate() const;
    bool isHealthy() const;
    bool validate() const;
    std::string getEntityTypeName() const;
    
    // ✅ JSON 직렬화 선언만 (cpp로 이동)
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    // ✅ DB 연산 선언만
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    
    std::string getTableName() const override { return "export_targets"; }

private:
    int profile_id_ = 0;
    std::string name_;
    std::string target_type_;
    std::string description_;
    bool is_enabled_ = true;
    std::string config_;
    std::string export_mode_ = "on_change";
    int export_interval_ = 0;
    int batch_size_ = 100;
    uint64_t total_exports_ = 0;
    uint64_t successful_exports_ = 0;
    uint64_t failed_exports_ = 0;
    std::optional<std::chrono::system_clock::time_point> last_export_at_;
    std::optional<std::chrono::system_clock::time_point> last_success_at_;
    std::string last_error_;
    std::optional<std::chrono::system_clock::time_point> last_error_at_;
    int avg_export_time_ms_ = 0;
};

}
}
}

#endif