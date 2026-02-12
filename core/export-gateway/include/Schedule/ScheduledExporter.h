/**
 * @file ScheduledExporter.h
 * @brief DB 기반 스케줄 관리 및 실행
 * @version 2.0.0
 */

#ifndef SCHEDULED_EXPORTER_H
#define SCHEDULED_EXPORTER_H
#include "Client/RedisClient.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Export/GatewayExportTypes.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

namespace PulseOne {
namespace Schedule {

using json = nlohmann::json;

struct ScheduledExporterConfig {
  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_password = "";
  int check_interval_seconds = 10;
  int reload_interval_seconds = 60;
  int batch_size = 100;
  int export_timeout_seconds = 300;
  int retry_interval_seconds = 300;
  bool enable_debug_log = false;
};

struct ExportDataPoint {
  int point_id = 0;
  int building_id = 0;
  std::string point_name;
  std::string mapped_name;
  std::string value;
  int64_t timestamp = 0;
  int quality = 0;
  std::string unit;
  json extra_info;

  json to_json() const {
    return json::object({{"point_id", point_id},
                         {"building_id", building_id},
                         {"point_name", point_name},
                         {"mapped_name", mapped_name},
                         {"value", value},
                         {"timestamp", timestamp},
                         {"quality", quality},
                         {"unit", unit},
                         {"extra_info", extra_info}});
  }
};

struct ScheduleExecutionResult {
  int schedule_id = 0;
  bool success = false;
  std::string error_message;
  int data_point_count = 0;
  int64_t execution_time_ms = 0;
  std::chrono::system_clock::time_point execution_timestamp;
  std::chrono::system_clock::time_point next_run_time;
  std::vector<std::string> target_names;
  int successful_targets = 0;
  int failed_targets = 0;
  int total_points = 0;
  int exported_points = 0;
  int failed_points = 0;
  std::chrono::milliseconds execution_time{0};

  json to_json() const {
    return json{{"schedule_id", schedule_id},
                {"success", success},
                {"error_message", error_message},
                {"data_point_count", data_point_count},
                {"execution_time_ms", execution_time_ms},
                {"target_names", target_names},
                {"successful_targets", successful_targets},
                {"failed_targets", failed_targets}};
  }
};

class ScheduledExporter {
public:
  static ScheduledExporter &
  getInstance(const ScheduledExporterConfig &config = {});

  // Singleton: Delete copy/move constructors
  ScheduledExporter(const ScheduledExporter &) = delete;
  ScheduledExporter &operator=(const ScheduledExporter &) = delete;
  ScheduledExporter(ScheduledExporter &&) = delete;
  ScheduledExporter &operator=(ScheduledExporter &&) = delete;

  bool start();
  void stop();
  bool isRunning() const { return is_running_.load(); }

  ScheduleExecutionResult executeSchedule(int schedule_id);
  int executeAllSchedules();

  int reloadSchedules();
  std::vector<PulseOne::Database::Entities::ExportScheduleEntity>
  getActiveSchedules() const;
  std::optional<PulseOne::Database::Entities::ExportScheduleEntity>
  getSchedule(int schedule_id) const;
  std::vector<int> getActiveScheduleIds() const;
  std::map<int, std::chrono::system_clock::time_point>
  getUpcomingSchedules() const;

  json getStatistics() const;
  void resetStatistics();

  // 실패 데이터 처리 (Persistent Retry) - Public for ExportCoordinator use
  void
  saveFailedBatchToFile(const std::string &target_name,
                        const std::vector<PulseOne::CSP::ValueMessage> &values,
                        const json &target_config);

  void saveFailedAlarmBatchToFile(
      const std::string &target_name,
      const std::vector<PulseOne::CSP::AlarmMessage> &alarms,
      const json &target_config);

  void startRetryThread();
  void stopRetryThread();
  void backgroundRetryLoop();

private:
  explicit ScheduledExporter(const ScheduledExporterConfig &config);
  ~ScheduledExporter();
  void scheduleCheckLoop();

  std::vector<PulseOne::Database::Entities::ExportScheduleEntity>
  findDueSchedules();

  ScheduleExecutionResult executeScheduleInternal(
      const PulseOne::Database::Entities::ExportScheduleEntity &schedule);

  std::vector<ExportDataPoint> collectDataFromRedis(
      const PulseOne::Database::Entities::ExportScheduleEntity &schedule);

  std::vector<ExportDataPoint> collectDataForSchedule(
      const PulseOne::Database::Entities::ExportScheduleEntity &schedule,
      const PulseOne::Database::Entities::ExportTargetEntity &target);

  std::pair<std::chrono::system_clock::time_point,
            std::chrono::system_clock::time_point>
  calculateTimeRange(const std::string &data_range, int lookback_periods);

  std::optional<ExportDataPoint>
  fetchPointData(int point_id,
                 const std::chrono::system_clock::time_point &start_time,
                 const std::chrono::system_clock::time_point &end_time);

  bool sendDataToTarget(
      const std::string &target_name,
      const std::vector<ExportDataPoint> &data_points,
      const PulseOne::Database::Entities::ExportScheduleEntity &schedule);

  void processFailedExports();

  json
  transformDataWithTemplate(const json &template_json,
                            const json &field_mappings,
                            const std::vector<ExportDataPoint> &data_points);

  std::vector<std::vector<ExportDataPoint>>
  createBatches(const std::vector<ExportDataPoint> &data_points);

  void saveExecutionLog(
      const PulseOne::Database::Entities::ExportScheduleEntity &schedule,
      const ScheduleExecutionResult &result);

  void logExecutionResult(const ScheduleExecutionResult &result);

  bool
  updateScheduleStatus(int schedule_id, bool success,
                       const std::chrono::system_clock::time_point &next_run);

  void updateScheduleStatus(
      PulseOne::Database::Entities::ExportScheduleEntity &schedule,
      bool success);

  std::chrono::system_clock::time_point
  calculateNextRun(const std::string &cron_expression,
                   std::chrono::system_clock::time_point from_time);

  std::chrono::system_clock::time_point
  calculateNextRunTime(const std::string &cron_expression,
                       const std::string &timezone,
                       const std::chrono::system_clock::time_point &from_time);

  bool initializeRedisConnection();
  bool initializeRepositories();

  ScheduledExporterConfig config_;
  std::shared_ptr<RedisClient> redis_client_;
  std::unique_ptr<PulseOne::Database::Repositories::ExportScheduleRepository>
      schedule_repo_;
  std::unique_ptr<PulseOne::Database::Repositories::ExportTargetRepository>
      target_repo_;
  std::unique_ptr<PulseOne::Database::Repositories::ExportLogRepository>
      log_repo_;
  std::unique_ptr<
      PulseOne::Database::Repositories::ExportTargetMappingRepository>
      mapping_repo_;
  std::unique_ptr<PulseOne::Database::Repositories::PayloadTemplateRepository>
      template_repo_;

  std::unique_ptr<std::thread> worker_thread_;
  std::atomic<bool> is_running_{false};
  std::atomic<bool> should_stop_{false};

  // 재시도 스레드
  std::unique_ptr<std::thread> retry_thread_;
  std::atomic<bool> retry_thread_running_{false};

  std::map<int, PulseOne::Database::Entities::ExportScheduleEntity>
      cached_schedules_;
  mutable std::mutex schedule_mutex_;
  std::chrono::system_clock::time_point last_reload_time_;

  std::atomic<uint64_t> total_executions_{0};
  std::atomic<uint64_t> successful_executions_{0};
  std::atomic<uint64_t> failed_executions_{0};
  std::atomic<int64_t> total_data_points_exported_{0};
  std::atomic<int64_t> total_execution_time_ms_{0};
};

} // namespace Schedule
} // namespace PulseOne

#endif // SCHEDULED_EXPORTER_H