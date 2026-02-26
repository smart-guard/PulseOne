//=============================================================================
// collector/include/Pipeline/DataProcessingService.h - 완성본 (컴파일 에러
// 수정)
//=============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Alarm/AlarmTypes.h"
#include "Client/InfluxClient.h"
#include "Common/Structs.h"
#include "Common/Utils.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Logging/LogManager.h"
#include "Pipeline/IPersistenceQueue.h"
#include "Pipeline/IPipelineStage.h"
#include "Utils/ThreadSafeQueue.h"
#include "VirtualPoint/VirtualPointBatchWriter.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// 전방 선언
namespace PulseOne {
namespace Alarm {
struct AlarmEvent;
struct AlarmProcessingStats;
} // namespace Alarm
namespace VirtualPoint {
class VirtualPointEngine;
}
namespace Storage {
class RedisDataWriter;
namespace BackendFormat {
struct AlarmEventData;
}
} // namespace Storage
} // namespace PulseOne

namespace PulseOne {
namespace Pipeline {

using DataValue = PulseOne::Structs::DataValue;

struct PersistenceTask {
  enum class Type { RDB_SAVE, INFLUX_SAVE, COMM_STATS_SAVE };
  Type type;
  Structs::DeviceDataMessage message;
  std::vector<Structs::TimestampedValue> points;
};

class DataProcessingService : public IPersistenceQueue {
public:
  // Pipeline Helpers
  void InitializePipeline();
  // 기본 구조체들
  struct ProcessingStats {
    std::atomic<size_t> total_batches_processed{0};
    std::atomic<size_t> total_messages_processed{0};
    std::atomic<size_t> redis_writes{0};
    std::atomic<size_t> influx_writes{0};
    std::atomic<size_t> processing_errors{0};
    double avg_processing_time_ms = 0.0;

    // 복사 생성자 명시적 구현
    ProcessingStats() = default;
    ProcessingStats(const ProcessingStats &other) {
      total_batches_processed.store(other.total_batches_processed.load());
      total_messages_processed.store(other.total_messages_processed.load());
      redis_writes.store(other.redis_writes.load());
      influx_writes.store(other.influx_writes.load());
      processing_errors.store(other.processing_errors.load());
      avg_processing_time_ms = other.avg_processing_time_ms;
    }

    // 복사 대입 연산자 수정
    ProcessingStats &operator=(const ProcessingStats &other) {
      if (this != &other) {
        total_batches_processed.store(other.total_batches_processed.load());
        total_messages_processed.store(other.total_messages_processed.load());
        redis_writes.store(other.redis_writes.load());
        influx_writes.store(other.influx_writes.load());
        processing_errors.store(other.processing_errors.load());
        avg_processing_time_ms = other.avg_processing_time_ms;
      }
      return *this;
    }

    nlohmann::json toJson() const {
      nlohmann::json j;
      j["total_batches_processed"] = total_batches_processed.load();
      j["total_messages_processed"] = total_messages_processed.load();
      j["redis_writes"] = redis_writes.load();
      j["influx_writes"] = influx_writes.load();
      j["processing_errors"] = processing_errors.load();
      j["avg_time_ms"] = avg_processing_time_ms;
      return j;
    }
  };

  struct ExtendedProcessingStats {
    ProcessingStats processing;
    PulseOne::Alarm::AlarmProcessingStats alarms;

    nlohmann::json toJson() const;
  };

  struct ServiceConfig {
    size_t thread_count;
    size_t batch_size;
    bool alarm_evaluation_enabled;
    bool virtual_point_calculation_enabled;
    bool external_notification_enabled;
  };

  // 생성자 - 매개변수 제거
  DataProcessingService();
  virtual ~DataProcessingService();

  // 서비스 제어
  bool Start();
  void Stop();
  bool IsRunning() const { return is_running_.load(); }
  void SetThreadCount(size_t thread_count);
  ServiceConfig GetConfig() const;

  // 메인 처리
  void ProcessBatch(const std::vector<Structs::DeviceDataMessage> &batch,
                    size_t thread_index);

  // 가상포인트 처리
  Structs::DeviceDataMessage CalculateVirtualPointsAndEnrich(
      const Structs::DeviceDataMessage &original_message);
  std::vector<Structs::TimestampedValue>
  CalculateVirtualPoints(const std::vector<Structs::DeviceDataMessage> &batch);

  // 알람 처리
  void EvaluateAlarms(const std::vector<Structs::DeviceDataMessage> &batch,
                      size_t thread_index);
  void ProcessAlarmEvents(
      const std::vector<PulseOne::Alarm::AlarmEvent> &alarm_events,
      size_t thread_index);

  // 알람 변환 (구현부에 있는 함수)
  Storage::BackendFormat::AlarmEventData ConvertAlarmEventToBackendFormat(
      const PulseOne::Alarm::AlarmEvent &alarm_event) const;

  // RDB 저장
  void SaveChangedPointsToRDB(const Structs::DeviceDataMessage &message);
  void SaveChangedPointsToRDB(
      const Structs::DeviceDataMessage &message,
      const std::vector<Structs::TimestampedValue> &changed_points);
  void SaveChangedPointsToRDBBatch(
      const Structs::DeviceDataMessage &message,
      const std::vector<Structs::TimestampedValue> &changed_points);

  // InfluxDB 저장
  void SaveToInfluxDB(const std::vector<Structs::TimestampedValue> &batch);
  void BufferForInfluxDB(const Structs::DeviceDataMessage &message);
  void BufferCommStatsForInfluxDB(const Structs::DeviceDataMessage &message);

  // 헬퍼 메서드들 (구현부에 있는 함수들)
  std::vector<Structs::TimestampedValue>
  ConvertToTimestampedValues(const Structs::DeviceDataMessage &device_msg);
  std::vector<Structs::TimestampedValue>
  GetChangedPoints(const Structs::DeviceDataMessage &message);

  // JSON 변환 함수들 (구현부에 있는 것들)
  std::string TimestampedValueToJson(const Structs::TimestampedValue &value);
  std::string
  DeviceDataMessageToJson(const Structs::DeviceDataMessage &message);
  std::string
  ConvertToLightDeviceStatus(const Structs::DeviceDataMessage &message);
  std::string ConvertToLightPointValue(const Structs::TimestampedValue &value,
                                       const std::string &device_id);
  std::string
  ConvertToBatchPointData(const Structs::DeviceDataMessage &message);
  std::string getDeviceIdForPoint(int point_id);

  // 가상포인트 저장 (구현부에 있는 함수)
  void StoreVirtualPointToRedis(const Structs::TimestampedValue &vp_result);

  // 외부 알림
  void SendExternalNotifications(const PulseOne::Alarm::AlarmEvent &event);
  void NotifyWebClients(const PulseOne::Alarm::AlarmEvent &event);

  // DB 관련 - ✅ 컴파일 에러 수정: 중복 Structs:: 제거
  PulseOne::Database::Entities::CurrentValueEntity
  ConvertToCurrentValueEntity(const Structs::TimestampedValue &point,
                              const Structs::DeviceDataMessage &message);
  void SaveAlarmToDatabase(const PulseOne::Alarm::AlarmEvent &event);

  // 통계 (구현부에 있는 모든 함수들)
  ProcessingStats GetStatistics() const;
  PulseOne::Alarm::AlarmProcessingStats GetAlarmStatistics() const;
  ExtendedProcessingStats GetExtendedStatistics() const;
  nlohmann::json GetVirtualPointBatchStats() const;

  void UpdateStatistics(size_t processed_count,
                        double processing_time_ms = 0.0);
  void UpdateStatistics(size_t point_count);
  void ResetStatistics();

  // VirtualPoint 관련 (구현부에 있는 함수들)
  void FlushVirtualPointBatch();
  void LogPerformanceComparison();

  // 상태 확인
  bool IsHealthy() const;
  nlohmann::json GetDetailedStatus() const;

  // 기능 토글
  void EnableAlarmEvaluation(bool enable) {
    alarm_evaluation_enabled_.store(enable);
  }
  void EnableVirtualPointCalculation(bool enable) {
    virtual_point_calculation_enabled_.store(enable);
  }
  void EnableExternalNotifications(bool enable) {
    external_notification_enabled_.store(enable);
  }
  void SetInfluxDbStorageInterval(int interval_ms) {
    influxdb_storage_interval_ms_.store(interval_ms);
  }

  // IPersistenceQueue Implementation
  void
  QueueRDBTask(const Structs::DeviceDataMessage &message,
               const std::vector<Structs::TimestampedValue> &points) override;
  void QueueInfluxTask(
      const Structs::DeviceDataMessage &message,
      const std::vector<Structs::TimestampedValue> &points) override;
  void QueueCommStatsTask(const Structs::DeviceDataMessage &message) override;

  // Helper functions
  std::string getPointName(int point_id) const;
  std::string getUnit(int point_id) const;

  // Pipeline Helpers
  // InitializePipeline is declared above (line 60)

private:
  // 스레드 처리
  void ProcessingThreadLoop(size_t thread_index);
  void PersistenceThreadLoop();
  std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
  void HandleError(const std::string &error_message,
                   const std::string &context = "");

  // 클라이언트들
  std::unique_ptr<Storage::RedisDataWriter> redis_data_writer_;
  std::shared_ptr<PulseOne::Client::InfluxClient> influx_client_;
  std::unique_ptr<VirtualPoint::VirtualPointBatchWriter> vp_batch_writer_;

  // Persistence Task Processing Helpers
  void ProcessRDBTasks(const std::vector<PersistenceTask> &rdb_tasks);
  void ProcessInfluxTasks(const std::vector<PersistenceTask> &influx_tasks);
  void
  ProcessCommStatsTasks(const std::vector<PersistenceTask> &comm_stats_tasks);

  // Pipeline
  std::vector<std::unique_ptr<IPipelineStage>> pipeline_stages_;

  // 서비스 상태
  std::atomic<bool> should_stop_{false};
  std::atomic<bool> is_running_{false};

  // 스레드 관리
  size_t thread_count_;
  size_t batch_size_;
  std::vector<std::thread> processing_threads_;
  std::thread persistence_thread_;
  Utils::ThreadSafeQueue<PersistenceTask> persistence_queue_;

  // 기능 플래그들
  std::atomic<bool> alarm_evaluation_enabled_{true};
  std::atomic<bool> virtual_point_calculation_enabled_{true};
  std::atomic<bool> external_notification_enabled_{false};

  // 통계
  std::atomic<size_t> total_batches_processed_{0};
  std::atomic<size_t> total_messages_processed_{0};
  std::atomic<size_t> points_processed_{0};
  std::atomic<size_t> redis_writes_{0};
  std::atomic<size_t> influx_writes_{0};
  std::atomic<size_t> processing_errors_{0};
  std::atomic<size_t> alarms_evaluated_{0};
  std::atomic<size_t> virtual_points_calculated_{0};
  std::atomic<size_t> total_operations_{0};
  std::atomic<uint64_t> total_processing_time_ms_{0};
  std::atomic<size_t> total_alarms_evaluated_{0};
  std::atomic<size_t> total_alarms_triggered_{0};
  std::atomic<size_t> critical_alarms_count_{0};
  std::atomic<size_t> high_alarms_count_{0};

  mutable std::mutex processing_mutex_;
  mutable std::mutex rdb_save_mutex_;
  std::unordered_map<int, std::chrono::steady_clock::time_point>
      last_rdb_save_times_;
  mutable std::mutex influxd_save_mutex_;
  std::unordered_map<int, std::chrono::steady_clock::time_point>
      last_influxd_save_times_;
  std::atomic<int> influxdb_storage_interval_ms_{0};

  std::chrono::steady_clock::time_point start_time_;
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H