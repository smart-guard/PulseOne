/**
 * @file ExportCoordinator.h
 * @brief Export 시스템 중앙 조율자 헤더
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0
 *
 * v2.0 변경사항:
 * - AlarmSubscriber → EventSubscriber
 * - 스케줄 이벤트 자동 처리
 * - 범용 이벤트 지원
 */

#ifndef EXPORT_COORDINATOR_H
#define EXPORT_COORDINATOR_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AlarmMessage.h"
#include "Export/ExportTypes.h"
#include <nlohmann/json.hpp>

#include "Client/RedisClient.h"
#include "Client/RedisClientImpl.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "DynamicTargetManager.h"
#include "Event/EventSubscriber.h"
#include "Logging/LogManager.h"
#include "Schedule/ScheduledExporter.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ConfigManager.h"

// namespace Coordinator

namespace PulseOne {
namespace Coordinator {

struct ExportCoordinatorConfig {
  std::string database_path = "/app/data/pulseone.db";
  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_password = "";
  std::vector<std::string> alarm_channels = {"alarms:all"};
  std::vector<std::string> alarm_patterns;
  int alarm_worker_threads = 4;
  size_t alarm_max_queue_size = 10000;
  int schedule_check_interval_seconds = 60;
  int schedule_reload_interval_seconds = 300;
  int schedule_batch_size = 100;
  bool enable_debug_log = false;
  int log_retention_days = 30;
  int max_concurrent_exports = 50;
  int export_timeout_seconds = 30;

  // Subscription Mode
  std::string subscription_mode = "all"; // "all" or "selective"
};

struct ExportResult {
  bool success = false;
  int target_id = 0;
  std::string target_name;
  std::string error_message;
  int http_status_code = 0;
  std::chrono::milliseconds processing_time{0};
  size_t data_size = 0;
};

struct ExportCoordinatorStats {
  size_t total_exports = 0;
  size_t successful_exports = 0;
  size_t failed_exports = 0;
  double avg_processing_time_ms = 0.0;
  size_t alarm_events = 0;
  size_t alarm_exports = 0;
  size_t schedule_events = 0;
  size_t schedule_executions = 0;
  size_t schedule_exports = 0;
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point last_export_time;

  json to_json() const {
    auto now = std::chrono::system_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
            .count();

    return json{{"total_exports", total_exports},
                {"successful_exports", successful_exports},
                {"failed_exports", failed_exports},
                {"avg_processing_time_ms", avg_processing_time_ms},
                {"alarm_events", alarm_events},
                {"alarm_exports", alarm_exports},
                {"schedule_events", schedule_events},
                {"schedule_executions", schedule_executions},
                {"schedule_exports", schedule_exports},
                {"uptime_seconds", uptime}};
  }
};

class ExportCoordinator {
public:
  explicit ExportCoordinator(const ExportCoordinatorConfig &config);
  ~ExportCoordinator();

  ExportCoordinator(const ExportCoordinator &) = delete;
  ExportCoordinator &operator=(const ExportCoordinator &) = delete;
  ExportCoordinator(ExportCoordinator &&) = delete;
  ExportCoordinator &operator=(ExportCoordinator &&) = delete;

  bool start();
  void stop();
  bool isRunning() const { return is_running_.load(); }

  void setGatewayId(int id) { gateway_id_ = id; }
  void updateHeartbeat();

  std::vector<ExportResult> handleAlarmEvent(PulseOne::CSP::AlarmMessage alarm);
  std::vector<ExportResult>
  handleAlarmBatch(std::vector<PulseOne::CSP::AlarmMessage> alarms);
  std::vector<ExportResult> handleScheduledExport(int schedule_id);
  ExportResult handleManualExport(const std::string &target_name,
                                  const nlohmann::json &data);

  void
  logExportResult(const ExportResult &result,
                  const PulseOne::CSP::AlarmMessage *alarm_message = nullptr);
  void logExportResults(const std::vector<ExportResult> &results);

  ExportCoordinatorStats getStats() const;
  void resetStats();
  nlohmann::json getTargetStats(const std::string &target_name) const;

  int reloadTargets();
  int reloadTemplates();
  void updateConfig(const ExportCoordinatorConfig &new_config);

  bool healthCheck() const;
  nlohmann::json getComponentStatus() const;

  static std::shared_ptr<PulseOne::CSP::DynamicTargetManager>
  getTargetManager();
  static std::shared_ptr<PulseOne::Transform::PayloadTransformer>
  getPayloadTransformer();

  PulseOne::Event::EventSubscriber *getEventSubscriber() const {
    return event_subscriber_.get();
  }

  PulseOne::Schedule::ScheduledExporter *getScheduledExporter() const {
    return scheduled_exporter_.get();
  }

  void handleScheduleEvent(const std::string &channel,
                           const std::string & /*message*/);
  void handleConfigEvent(const std::string &channel,
                         const std::string &message);

  // Batch Processing Methods
  void flushAlarmBatch();
  void checkAlarmBatchTimeout();
  void sendValueSnapshot();

private:
  void startBatchTimers();
  void stopBatchTimers();

  bool initializeDatabase();
  bool initializeRepositories();
  bool initializeEventSubscriber();
  bool initializeScheduledExporter();

  static bool initializeSharedResources();
  static void cleanupSharedResources();

  ExportResult convertTargetSendResult(
      const PulseOne::CSP::TargetSendResult &target_result) const;
  void updateStats(const ExportResult &result);
  std::string getDatabasePath() const;

  ExportCoordinatorConfig config_;
  std::atomic<bool> is_running_{false};

  std::unique_ptr<PulseOne::Event::EventSubscriber> event_subscriber_;
  std::unique_ptr<PulseOne::Schedule::ScheduledExporter> scheduled_exporter_;

  std::unique_ptr<PulseOne::Database::Repositories::ExportLogRepository>
      log_repo_;
  std::unique_ptr<PulseOne::Database::Repositories::ExportTargetRepository>
      target_repo_;

  int gateway_id_ = -1;
  std::thread heartbeat_thread_;
  std::atomic<bool> heartbeat_running_{false};
  std::unique_ptr<RedisClient> redis_client_;

  mutable std::mutex stats_mutex_;

  ExportCoordinatorStats stats_;
  mutable std::mutex export_mutex_;

  static std::shared_ptr<PulseOne::CSP::DynamicTargetManager>
      shared_target_manager_;
  static std::shared_ptr<PulseOne::Transform::PayloadTransformer>
      shared_payload_transformer_;
  static std::mutex init_mutex_;
  static std::atomic<bool> shared_resources_initialized_;

  // Batching Members
  mutable std::mutex batch_mutex_;
  std::vector<PulseOne::CSP::AlarmMessage> pending_alarms_;
  std::chrono::system_clock::time_point last_batch_flush_time_;

  std::thread batch_timer_thread_;
  std::atomic<bool> batch_timer_running_{false};
};

} // namespace Coordinator
} // namespace PulseOne

#endif // EXPORT_COORDINATOR_H