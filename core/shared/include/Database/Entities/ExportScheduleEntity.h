#ifndef EXPORT_SCHEDULE_ENTITY_H
#define EXPORT_SCHEDULE_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

namespace PulseOne {
namespace Database {
namespace Repositories {
class ExportScheduleRepository;
}
namespace Entities {

using json = nlohmann::json;

class ExportScheduleEntity : public BaseEntity<ExportScheduleEntity> {
public:
  ExportScheduleEntity();
  explicit ExportScheduleEntity(int id);
  virtual ~ExportScheduleEntity();

  std::shared_ptr<Repositories::ExportScheduleRepository> getRepository() const;

  // ⭐ BaseEntity 순수 가상 함수 모두 구현!
  bool updateToDatabase() override { return saveToDatabase(); }

  bool fromJson(const json &data) override;

  // ⭐ toJson() 추가 - 이게 누락되어 있었음!
  json toJson() const override {
    json j;
    j["id"] = getId();
    j["profile_id"] = profile_id_;
    j["target_id"] = target_id_;
    j["schedule_name"] = schedule_name_;
    j["description"] = description_;
    j["cron_expression"] = cron_expression_;
    j["timezone"] = timezone_;
    j["data_range"] = data_range_;
    j["lookback_periods"] = lookback_periods_;
    j["is_enabled"] = is_enabled_;
    j["last_status"] = last_status_;
    j["total_runs"] = total_runs_;
    j["successful_runs"] = successful_runs_;
    j["failed_runs"] = failed_runs_;
    j["success_rate"] = getSuccessRate();
    return j;
  }

  std::string toString() const override {
    std::ostringstream oss;
    oss << "ExportSchedule[id=" << getId() << ", name=" << schedule_name_
        << ", target=" << target_id_
        << ", enabled=" << (is_enabled_ ? "yes" : "no")
        << ", runs=" << total_runs_ << ", rate=" << std::fixed
        << std::setprecision(1) << getSuccessRate() << "%]";
    return oss.str();
  }

  std::string getTableName() const override { return "export_schedules"; }

  std::string getEntityTypeName() const override {
    return "ExportScheduleEntity";
  }

  // Getters
  int getProfileId() const { return profile_id_; }
  int getTargetId() const { return target_id_; }
  std::string getScheduleName() const { return schedule_name_; }
  std::string getDescription() const { return description_; }
  std::string getCronExpression() const { return cron_expression_; }
  std::string getTimezone() const { return timezone_; }
  std::string getDataRange() const { return data_range_; }
  int getLookbackPeriods() const { return lookback_periods_; }
  bool isEnabled() const { return is_enabled_; }
  std::string getLastStatus() const { return last_status_; }
  int getTotalRuns() const { return total_runs_; }
  int getSuccessfulRuns() const { return successful_runs_; }
  int getFailedRuns() const { return failed_runs_; }

  // Time getters (optional 처리)
  std::optional<std::chrono::system_clock::time_point> getLastRunAt() const {
    return last_run_at_;
  }
  std::optional<std::chrono::system_clock::time_point> getNextRunAt() const {
    return next_run_at_;
  }
  std::optional<std::chrono::system_clock::time_point> getCreatedAt() const {
    return created_at_;
  }
  std::optional<std::chrono::system_clock::time_point> getUpdatedAt() const {
    return updated_at_;
  }

  // Setters (간결하게)
  void setProfileId(int id) {
    profile_id_ = id;
    markModified();
  }
  void setTargetId(int id) {
    target_id_ = id;
    markModified();
  }
  void setScheduleName(const std::string &name) {
    schedule_name_ = name;
    markModified();
  }
  void setDescription(const std::string &desc) {
    description_ = desc;
    markModified();
  }
  void setCronExpression(const std::string &cron) {
    cron_expression_ = cron;
    markModified();
  }
  void setTimezone(const std::string &tz) {
    timezone_ = tz;
    markModified();
  }
  void setDataRange(const std::string &range) {
    data_range_ = range;
    markModified();
  }
  void setLookbackPeriods(int periods) {
    lookback_periods_ = periods;
    markModified();
  }
  void setEnabled(bool enabled) {
    is_enabled_ = enabled;
    markModified();
  }
  void setLastStatus(const std::string &status) {
    last_status_ = status;
    markModified();
  }
  void setTotalRuns(int count) {
    total_runs_ = count;
    markModified();
  }
  void setSuccessfulRuns(int count) {
    successful_runs_ = count;
    markModified();
  }
  void setFailedRuns(int count) {
    failed_runs_ = count;
    markModified();
  }

  void setLastRunAt(const std::chrono::system_clock::time_point &time) {
    last_run_at_ = time;
    markModified();
  }
  void setNextRunAt(const std::chrono::system_clock::time_point &time) {
    next_run_at_ = time;
    markModified();
  }

  // 비즈니스 로직
  bool validate() const {
    return !schedule_name_.empty() && target_id_ > 0 &&
           !cron_expression_.empty() && lookback_periods_ >= 1;
  }

  void recordRun(bool success,
                 const std::chrono::system_clock::time_point &next_run);
  double getSuccessRate() const;

  // DB 연산
  bool loadFromDatabase();
  bool saveToDatabase();
  bool deleteFromDatabase();

private:
  int profile_id_;
  int target_id_;
  std::string schedule_name_;
  std::string description_;
  std::string cron_expression_;
  std::string timezone_;
  std::string data_range_;
  int lookback_periods_;
  bool is_enabled_;
  std::optional<std::chrono::system_clock::time_point> last_run_at_;
  std::string last_status_;
  std::optional<std::chrono::system_clock::time_point> next_run_at_;
  int total_runs_;
  int successful_runs_;
  int failed_runs_;
  std::optional<std::chrono::system_clock::time_point> created_at_;
  std::optional<std::chrono::system_clock::time_point> updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif