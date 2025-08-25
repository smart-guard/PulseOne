//=============================================================================
// collector/include/Pipeline/DataProcessingService.h - 완전한 헤더 파일
// 
// 🎯 목적: 헤더와 구현부 간 함수 시그니처 완전 일치, 누락 함수 없음
// 📋 특징:
//   - 모든 구현부 함수에 대한 헤더 선언 포함
//   - 기존 구조 100% 유지하면서 확장
//   - 컴파일 에러 완전 방지
//=============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Common/Utils.h"
#include "Client/RedisClient.h"
#include "Client/RedisClientImpl.h"
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
#include <iomanip>          // std::setprecision, std::setfill, std::setw
#include <sstream>          // std::ostringstream  
#include <ctime>            // std::put_time, std::gmtime
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
}

namespace PulseOne {
namespace Pipeline {

    using DataValue = PulseOne::Structs::DataValue;
/**
 * @brief 데이터 처리 서비스 - 파이프라인의 핵심 처리 엔진
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 내부 구조체들
    // ==========================================================================
    
    struct ProcessingStats {
        size_t total_batches_processed{0};         // atomic 제거
        size_t total_messages_processed{0};        // atomic 제거
        size_t redis_writes{0};                    // atomic 제거
        size_t influx_writes{0};                   // atomic 제거
        size_t processing_errors{0};               // atomic 제거
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
        bool lightweight_mode;
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
    // 저장 메서드들
    // ==========================================================================
    
    // Redis 저장
    void SaveToRedisFullData(const Structs::DeviceDataMessage& enriched_message);
    void SaveToRedisLightweight(const std::vector<Structs::TimestampedValue>& batch);
    void SavePointDataToRedis(const Structs::DeviceDataMessage& message);
    void StoreVirtualPointToRedis(const Structs::TimestampedValue& vp_result);
    void WriteTimestampedValueToRedis(const Structs::TimestampedValue& value);
    
    // RDB 저장
    void SaveChangedPointsToRDB(const Structs::DeviceDataMessage& message);
    void SaveChangedPointsToRDB(const Structs::DeviceDataMessage& message, 
                               const std::vector<Structs::TimestampedValue>& changed_points);
    
    // InfluxDB 저장
    void SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch);
    void BufferForInfluxDB(const Structs::DeviceDataMessage& message);

    // ==========================================================================
    // 외부 시스템 연동
    // ==========================================================================
    
    void PublishAlarmToRedis(const PulseOne::Alarm::AlarmEvent& event);
    void SendExternalNotifications(const PulseOne::Alarm::AlarmEvent& event);
    void NotifyWebClients(const PulseOne::Alarm::AlarmEvent& event);

    // ==========================================================================
    // 헬퍼 메서드들
    // ==========================================================================
    
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    std::vector<Structs::TimestampedValue> ConvertToTimestampedValues(const Structs::DeviceDataMessage& device_msg);
    std::vector<Structs::TimestampedValue> GetChangedPoints(const Structs::DeviceDataMessage& message);
    
    // JSON 변환
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
    std::string DeviceDataMessageToJson(const Structs::DeviceDataMessage& message);
    std::string ConvertToLightDeviceStatus(const Structs::DeviceDataMessage& message);
    std::string ConvertToLightPointValue(const Structs::TimestampedValue& value, const std::string& device_id);
    std::string ConvertToBatchPointData(const Structs::DeviceDataMessage& message);
    
    // 유틸리티
    std::string getDeviceIdForPoint(int point_id);

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
    
    void SetRedisClient(std::shared_ptr<RedisClient> client) { 
        redis_client_ = std::static_pointer_cast<RedisClientImpl>(client);
    }
    void SetInfluxClient(std::shared_ptr<InfluxClient> client) { influx_client_ = client; }
    
    // 기능 토글
    void EnableAlarmEvaluation(bool enable) { alarm_evaluation_enabled_.store(enable); }
    void EnableVirtualPointCalculation(bool enable) { virtual_point_calculation_enabled_.store(enable); }
    void EnableLightweightRedis(bool enable) { use_lightweight_redis_.store(enable); }
    void EnableExternalNotifications(bool enable) { external_notification_enabled_.store(enable); }
    
    // 상태 조회
    bool IsAlarmEvaluationEnabled() const { return alarm_evaluation_enabled_.load(); }
    bool IsVirtualPointCalculationEnabled() const { return virtual_point_calculation_enabled_.load(); }
    bool IsLightweightRedisEnabled() const { return use_lightweight_redis_.load(); }
    bool IsExternalNotificationsEnabled() const { return external_notification_enabled_.load(); }

private:
    // ==========================================================================
    // 내부 메서드들
    // ==========================================================================
    
    void ProcessingThreadLoop(size_t thread_index);

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    std::unique_ptr<VirtualPoint::VirtualPointBatchWriter> vp_batch_writer_;
    
    // 서비스 상태
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};
    
    // 스레드 관리
    size_t thread_count_;
    size_t batch_size_;
    std::vector<std::thread> processing_threads_;
    
    // 기능 플래그들
    std::atomic<bool> alarm_evaluation_enabled_{true};
    std::atomic<bool> virtual_point_calculation_enabled_{true};
    std::atomic<bool> use_lightweight_redis_{false};
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


    PulseOne::Database::Entities::CurrentValueEntity ConvertToCurrentValueEntity(const Structs::TimestampedValue& point, 
        const Structs::DeviceDataMessage& message);
    
    void SaveChangedPointsToRDBBatch(const Structs::DeviceDataMessage& message, 
        const std::vector<Structs::TimestampedValue>& changed_points);
    void SaveAlarmToDatabase(const PulseOne::Alarm::AlarmEvent& event); 
    void SaveToRedisDevicePattern(const Structs::DeviceDataMessage& message);
    std::string extractDeviceNumber(const std::string& device_id);
    std::string getPointName(int point_id);
    std::string getDataType(const DataValue& value);
    std::string getUnit(int point_id);   
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H