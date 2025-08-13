// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 완성된 버전 (수정됨)
// =============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmTypes.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Pipeline {

// 전방 선언
class PipelineManager;

/**
 * @brief DataProcessingService - 데이터 파이프라인 처리 서비스
 * @details 멀티스레드로 DeviceDataMessage를 처리하여 Redis/InfluxDB에 저장
 *          가상포인트 계산, 알람 평가, 외부 시스템 연동 포함
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit DataProcessingService(
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    ~DataProcessingService();
    
    // 복사/이동 생성자 금지 (싱글톤 스타일)
    DataProcessingService(const DataProcessingService&) = delete;
    DataProcessingService& operator=(const DataProcessingService&) = delete;
    DataProcessingService(DataProcessingService&&) = delete;
    DataProcessingService& operator=(DataProcessingService&&) = delete;
    
    // ==========================================================================
    // 공개 인터페이스
    // ==========================================================================
    
    bool Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }
    
    void SetThreadCount(size_t thread_count);
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    void EnableLightweightMode(bool enable) { use_lightweight_redis_ = enable; }
    
    struct ServiceConfig {
        size_t thread_count;
        size_t batch_size;
        bool lightweight_mode;
        bool alarm_evaluation_enabled;
        bool virtual_point_calculation_enabled;
        bool external_notification_enabled;  // 🔥 추가: 외부 알림 기능
    };
    
    ServiceConfig GetConfig() const;
    
    // ==========================================================================
    // 통계 구조체들
    // ==========================================================================
    
    struct ProcessingStats {
        uint64_t total_batches_processed = 0;
        uint64_t total_messages_processed = 0;
        uint64_t redis_writes = 0;
        uint64_t influx_writes = 0;
        uint64_t processing_errors = 0;
        double avg_processing_time_ms = 0.0;
        
        nlohmann::json toJson() const;
    };
    
    struct ExtendedProcessingStats {
        ProcessingStats processing;
        PulseOne::Alarm::AlarmProcessingStats alarms;
        
        nlohmann::json toJson() const;
    };
    
    ProcessingStats GetStatistics() const;
    PulseOne::Alarm::AlarmProcessingStats GetAlarmStatistics() const;
    ExtendedProcessingStats GetExtendedStatistics() const;
    
    std::shared_ptr<RedisClient> GetRedisClient() const { return redis_client_; }
    std::shared_ptr<InfluxClient> GetInfluxClient() const { return influx_client_; }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // 멀티스레드 관리
    std::vector<std::thread> processing_threads_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    size_t thread_count_{4};
    size_t batch_size_{100};
    
    // 기능 플래그
    std::atomic<bool> use_lightweight_redis_{false};
    std::atomic<bool> alarm_evaluation_enabled_{true};
    std::atomic<bool> virtual_point_calculation_enabled_{true};
    std::atomic<bool> external_notification_enabled_{true};  // 🔥 추가
    
    // 통계 (스레드 안전)
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    std::atomic<uint64_t> processing_errors_{0};
    std::atomic<uint64_t> total_alarms_evaluated_{0};
    std::atomic<uint64_t> total_alarms_triggered_{0};
    std::atomic<uint64_t> critical_alarms_count_{0};
    std::atomic<uint64_t> high_alarms_count_{0};
    std::atomic<uint64_t> total_processing_time_ms_{0};
    std::atomic<uint64_t> total_operations_{0};
    
    // ==========================================================================
    // 내부 처리 메서드들
    // ==========================================================================
    
    void ProcessingThreadLoop(size_t thread_index);
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    
    // 가상포인트 및 데이터 변환
    std::vector<Structs::TimestampedValue> CalculateVirtualPoints(
        const std::vector<Structs::DeviceDataMessage>& batch);
    std::vector<Structs::TimestampedValue> ConvertToTimestampedValues(
        const Structs::DeviceDataMessage& device_msg);
    
    // 🔥 새로운 알람 시스템 연동
    void EvaluateAlarms(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    void ProcessAlarmEvents(const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events, size_t thread_index);
    
    // 🔥 외부 시스템 연동 (AlarmManager 결과 처리)
    void PublishAlarmToRedis(const PulseOne::Alarm::AlarmEvent& event);
    void SendExternalNotifications(const PulseOne::Alarm::AlarmEvent& event);
    void NotifyWebClients(const PulseOne::Alarm::AlarmEvent& event);
    
    // 저장 시스템
    void SaveToRedisFullData(const std::vector<Structs::DeviceDataMessage>& batch);
    void SaveToRedisLightweight(const std::vector<Structs::TimestampedValue>& batch);
    void SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch);
    
    // 헬퍼 메서드들
    void WriteTimestampedValueToRedis(const Structs::TimestampedValue& value);
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
    std::string DeviceDataMessageToJson(const Structs::DeviceDataMessage& message);
    void UpdateStatistics(size_t processed_count, double processing_time_ms);
    void HandleError(const std::string& error_message, const std::string& context);
    std::string getDeviceIdForPoint(int point_id);
    
    // 🔥 경량화 구조체 변환 (미래용)
    std::string ConvertToLightDeviceStatus(const Structs::DeviceDataMessage& message);
    std::string ConvertToLightPointValue(const Structs::TimestampedValue& value, const std::string& device_id);
    std::string ConvertToBatchPointData(const Structs::DeviceDataMessage& message);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H