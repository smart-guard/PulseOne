//=============================================================================
// collector/include/Pipeline/DataProcessingService.h - RedisDataWriter 통합 버전
// 
// 주요 변경사항:
//   - Redis 관련 멤버 변수 및 메서드들을 RedisDataWriter로 대체
//   - SaveToRedisFullData, SaveToRedisLightweight 등 Redis 저장 메서드 제거
//   - RedisDataWriter 멤버 변수 추가
//=============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Common/Utils.h"
#include "Client/InfluxClient.h"
#include "Utils/LogManager.h"
#include "Alarm/AlarmTypes.h"
#include "VirtualPoint/VirtualPointBatchWriter.h"
#include "Database/Entities/CurrentValueEntity.h"
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

// 전방 선언
namespace PulseOne {
namespace Alarm {
    struct AlarmEvent;
    struct AlarmProcessingStats;
}
namespace VirtualPoint {
    class VirtualPointEngine;
}
namespace Storage {
    class RedisDataWriter;  // RedisDataWriter 전방 선언 추가
}
}

namespace PulseOne {
namespace Pipeline {

    using DataValue = PulseOne::Structs::DataValue;

/**
 * @brief 데이터 처리 서비스 - RedisDataWriter 통합 버전
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 내부 구조체들
    // ==========================================================================
    
    struct ProcessingStats {
        size_t total_batches_processed{0};
        size_t total_messages_processed{0};
        size_t redis_writes{0};
        size_t influx_writes{0};
        size_t processing_errors{0};
        double avg_processing_time_ms{0.0};
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["total_batches"] = total_batches_processed;
            j["total_messages"] = total_messages_processed;
            j["redis_writes"] = redis_writes;
            j["influx_writes"] = influx_writes;
            j["errors"] = processing_errors;
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

    // ==========================================================================
    // 생성자/소멸자
    // ==========================================================================
    
    DataProcessingService(std::shared_ptr<RedisClient> redis_client = nullptr,
                         std::shared_ptr<InfluxClient> influx_client = nullptr);
    ~DataProcessingService();

    // ==========================================================================
    // 서비스 제어
    // ==========================================================================
    
    bool Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }
    
    void SetThreadCount(size_t thread_count);
    ServiceConfig GetConfig() const;

    // ==========================================================================
    // 메인 처리 메서드들
    // ==========================================================================
    
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    
    // 가상포인트 처리
    Structs::DeviceDataMessage CalculateVirtualPointsAndEnrich(const Structs::DeviceDataMessage& original_message);
    std::vector<Structs::TimestampedValue> CalculateVirtualPoints(const std::vector<Structs::DeviceDataMessage>& batch);
    
    // 알람 처리
    void EvaluateAlarms(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    void ProcessAlarmEvents(const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events, size_t thread_index);

    // ==========================================================================
    // 저장 메서드들 - Redis 관련 메서드들 제거됨
    // ==========================================================================
    
    // RDB 저장
    void SaveChangedPointsToRDB(const Structs::DeviceDataMessage& message);
    void SaveChangedPointsToRDB(const Structs::DeviceDataMessage& message, 
                               const std::vector<Structs::TimestampedValue>& changed_points);
    
    // InfluxDB 저장
    void SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch);
    void BufferForInfluxDB(const Structs::DeviceDataMessage& message);

    // ==========================================================================
    // 외부 시스템 연동 - 일부 Redis 관련 제거됨
    // ==========================================================================
    
    void SendExternalNotifications(const PulseOne::Alarm::AlarmEvent& event);
    void NotifyWebClients(const PulseOne::Alarm::AlarmEvent& event);

    // ==========================================================================
    // 헬퍼 메서드들
    // ==========================================================================
    
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    std::vector<Structs::TimestampedValue> ConvertToTimestampedValues(const Structs::DeviceDataMessage& device_msg);
    std::vector<Structs::TimestampedValue> GetChangedPoints(const Structs::DeviceDataMessage& message);

    // ==========================================================================
    // 통계 및 상태
    // ==========================================================================
    
    ProcessingStats GetStatistics() const;
    PulseOne::Alarm::AlarmProcessingStats GetAlarmStatistics() const;
    ExtendedProcessingStats GetExtendedStatistics() const;
    nlohmann::json GetVirtualPointBatchStats() const;
    
    void UpdateStatistics(size_t processed_count, double processing_time_ms = 0.0);
    void UpdateStatistics(size_t point_count);
    void ResetStatistics();
    
    // VirtualPoint 관련
    void FlushVirtualPointBatch();
    void LogPerformanceComparison();
    
    // 상태 확인
    bool IsHealthy() const;
    nlohmann::json GetDetailedStatus() const;

    // ==========================================================================
    // 에러 처리
    // ==========================================================================
    
    void HandleError(const std::string& error_message, const std::string& context = "");

    // ==========================================================================
    // 설정 관리
    // ==========================================================================
    
    void SetInfluxClient(std::shared_ptr<InfluxClient> client) { influx_client_ = client; }
    
    // Redis 설정은 RedisDataWriter를 통해 처리
    std::shared_ptr<Storage::RedisDataWriter> GetRedisDataWriter() const { return redis_data_writer_; }
    
    // 기능 토글
    void EnableAlarmEvaluation(bool enable) { alarm_evaluation_enabled_.store(enable); }
    void EnableVirtualPointCalculation(bool enable) { virtual_point_calculation_enabled_.store(enable); }
    void EnableExternalNotifications(bool enable) { external_notification_enabled_.store(enable); }
    
    // 상태 조회
    bool IsAlarmEvaluationEnabled() const { return alarm_evaluation_enabled_.load(); }
    bool IsVirtualPointCalculationEnabled() const { return virtual_point_calculation_enabled_.load(); }
    bool IsExternalNotificationsEnabled() const { return external_notification_enabled_.load(); }

private:
    // ==========================================================================
    // 내부 메서드들
    // ==========================================================================
    
    void ProcessingThreadLoop(size_t thread_index);
    
    // Entity 변환
    PulseOne::Database::Entities::CurrentValueEntity ConvertToCurrentValueEntity(
        const Structs::TimestampedValue& point, 
        const Structs::DeviceDataMessage& message);
     
    Storage::BackendFormat::AlarmEventData ConvertAlarmEventToBackendFormat(
        const PulseOne::Alarm::AlarmEvent& alarm_event) const;    
    
    // 알람 DB 저장
    void SaveAlarmToDatabase(const PulseOne::Alarm::AlarmEvent& event);

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들 - Redis 관련 제거, RedisDataWriter 추가
    std::shared_ptr<Storage::RedisDataWriter> redis_data_writer_;  // RedisDataWriter 사용
    std::shared_ptr<InfluxClient> influx_client_;
    std::unique_ptr<VirtualPoint::VirtualPointBatchWriter> vp_batch_writer_;
    
    // 서비스 상태
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};
    
    // 스레드 관리
    size_t thread_count_;
    size_t batch_size_;
    std::vector<std::thread> processing_threads_;
    
    // 기능 플래그들 - lightweight_redis 관련 제거
    std::atomic<bool> alarm_evaluation_enabled_{true};
    std::atomic<bool> virtual_point_calculation_enabled_{true};
    std::atomic<bool> external_notification_enabled_{false};
    
    // 통계 카운터들
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
    
    // 알람 통계
    std::atomic<size_t> total_alarms_evaluated_{0};
    std::atomic<size_t> total_alarms_triggered_{0};
    std::atomic<size_t> critical_alarms_count_{0};
    std::atomic<size_t> high_alarms_count_{0};
    
    // 동기화
    mutable std::mutex processing_mutex_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H