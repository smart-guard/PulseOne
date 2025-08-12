// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 컴파일 에러 수정
// =============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmTypes.h"  // 🔥 AlarmProcessingStats를 위해 추가
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>  // 🔥 JSON 사용을 위해 추가

namespace PulseOne {
namespace Pipeline {

// 전방 선언
class PipelineManager;

/**
 * @brief DataProcessingService - PipelineManager 싱글톤과 연동
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit DataProcessingService(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    ~DataProcessingService();
    
    // ==========================================================================
    // 공개 인터페이스
    // ==========================================================================
    
    bool Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }
    void SetThreadCount(size_t thread_count);
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    
    /**
     * @brief 기본 처리 통계
     */
    struct ProcessingStats {
        uint64_t total_batches_processed = 0;
        uint64_t total_messages_processed = 0;
        uint64_t redis_writes = 0;
        uint64_t influx_writes = 0;
        uint64_t processing_errors = 0;
        double avg_processing_time_ms = 0.0;
    };
    
    /**
     * @brief 확장된 통계 정보 (처리 + 알람)
     */
    struct ExtendedProcessingStats {
        ProcessingStats processing;
        PulseOne::Alarm::AlarmProcessingStats alarms;
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["processing"] = {
                {"total_batches", processing.total_batches_processed},
                {"total_messages", processing.total_messages_processed},
                {"redis_writes", processing.redis_writes},
                {"influx_writes", processing.influx_writes},
                {"errors", processing.processing_errors},
                {"avg_time_ms", processing.avg_processing_time_ms}
            };
            j["alarms"] = alarms.toJson();
            return j;
        }
    };
    
    ProcessingStats GetStatistics() const;
    PulseOne::Alarm::AlarmProcessingStats GetAlarmStatistics() const;
    ExtendedProcessingStats GetExtendedStatistics() const;
    
    std::shared_ptr<RedisClient> GetRedisClient() const { 
        return redis_client_; 
    }

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
    size_t thread_count_{8};
    size_t batch_size_{500};
    
    // 기본 통계 (스레드 안전)
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    std::atomic<uint64_t> processing_errors_{0};
    
    // 알람 관련 통계
    std::atomic<uint64_t> total_alarms_evaluated_{0};
    std::atomic<uint64_t> total_alarms_triggered_{0};
    std::atomic<uint64_t> critical_alarms_count_{0};
    std::atomic<uint64_t> high_alarms_count_{0};
    
    // ==========================================================================
    // 내부 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 처리 스레드 메인 루프
     */
    void ProcessingThreadLoop(size_t thread_index);
    
    /**
     * @brief PipelineManager에서 배치 수집
     */
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    
    /**
     * @brief 배치 처리 실행
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, 
                     size_t thread_index);
    
    // ==========================================================================
    // 🔥 단일 메서드로 정리 (중복 선언 제거)
    // ==========================================================================
    
    /**
     * @brief 가상포인트 계산 및 TimestampedValue 변환
     * @param batch DeviceDataMessage 배치
     * @return 원본 데이터 + 가상포인트를 TimestampedValue로 변환한 결과
     */
    std::vector<Structs::TimestampedValue> CalculateVirtualPoints(
        const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 알람 평가 실행
     */
    void EvaluateAlarms(const std::vector<Structs::TimestampedValue>& data, 
                       size_t thread_index);
    
    /**
     * @brief 알람 이벤트 후처리
     */
    void ProcessAlarmEvents(const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events,
                           const Structs::TimestampedValue& source_data,
                           size_t thread_index);
    
    /**
     * @brief Redis 저장
     */
    void SaveToRedis(const std::vector<Structs::TimestampedValue>& batch);
    
    /**
     * @brief InfluxDB 저장
     */
    void SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch);
    
    // ==========================================================================
    // 헬퍼 메서드들
    // ==========================================================================
    
    void WriteTimestampedValueToRedis(const Structs::TimestampedValue& value);
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
    void UpdateStatistics(size_t processed_count, double processing_time_ms);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H