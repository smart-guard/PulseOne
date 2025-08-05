// collector/include/Pipeline/WorkerPipelineManager.h
#ifndef PULSEONE_WORKER_PIPELINE_MANAGER_H
#define PULSEONE_WORKER_PIPELINE_MANAGER_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include "Client/RabbitMQClient.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 파이프라인 데이터 구조체들
// =============================================================================

// 🔥 기존 DeviceDataMessage 구조체 사용 (Structs.h에서 가져옴)
using PipelineData = Structs::DeviceDataMessage;

// =============================================================================
// 🔥 멀티스레드 WorkerPipelineManager
// =============================================================================

class WorkerPipelineManager {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit WorkerPipelineManager(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client,
        std::shared_ptr<RabbitMQClient> rabbitmq_client = nullptr
    );
    
    ~WorkerPipelineManager();
    
    // 복사/이동 방지
    WorkerPipelineManager(const WorkerPipelineManager&) = delete;
    WorkerPipelineManager& operator=(const WorkerPipelineManager&) = delete;
    
    // ==========================================================================
    // 🔥 공개 인터페이스 (Worker들이 호출)
    // ==========================================================================
    
    /**
     * @brief Worker가 호출하는 데이터 처리 메서드
     * @param device_id 디바이스 ID
     * @param values 데이터 값들
     * @param priority 우선순위 (0: 일반, 1: 높음, 2: 긴급)
     * @return 성공 시 true
     */
    bool ProcessDeviceData(const std::string& device_id,
                          const std::vector<Structs::TimestampedValue>& values,
                          uint32_t priority = 0);
    
    /**
     * @brief 파이프라인 시작
     */
    bool Start();
    
    /**
     * @brief 파이프라인 정지
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }
    
    // ==========================================================================
    // 🔥 설정 메서드들
    // ==========================================================================
    
    /**
     * @brief 처리 스레드 수 설정 (시작 전에 호출)
     */
    void SetThreadCount(size_t thread_count);
    
    /**
     * @brief 큐 최대 크기 설정
     */
    void SetMaxQueueSize(size_t max_size) { max_queue_size_ = max_size; }
    
    /**
     * @brief 배치 크기 설정
     */
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    
    /**
     * @brief 알람 임계값 설정
     */
    void SetAlarmThresholds(const std::map<std::string, double>& thresholds) {
        alarm_thresholds_ = thresholds;
    }
    
    // ==========================================================================
    // 🔥 통계 구조체 (Structs.h로 이동 예정)
    // ==========================================================================
    
    // 🔥 임시: 통계는 Structs.h에 PipelineStatistics로 정의될 예정
    using PipelineStatistics = Structs::PipelineStatistics;
    
    const PipelineStatistics& GetStatistics() const { return stats_; }
    void ResetStatistics();

private:
    // ==========================================================================
    // 🔥 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    std::shared_ptr<RabbitMQClient> rabbitmq_client_;
    
    // 🔥 멀티스레드 큐 시스템 (DeviceDataMessage 사용)
    std::vector<std::queue<Structs::DeviceDataMessage>> thread_queues_;
    std::vector<std::mutex> thread_mutexes_;
    std::vector<std::condition_variable> thread_cvs_;
    std::vector<std::thread> processing_threads_;
    
    // 스레드 관리
    std::atomic<bool> is_running_{false};
    size_t thread_count_ = 4;  // 기본 4개 스레드
    std::atomic<size_t> round_robin_counter_{0};
    
    // 설정
    size_t max_queue_size_ = 10000;
    size_t batch_size_ = 100;
    std::map<std::string, double> alarm_thresholds_;
    
    // 통계
    mutable PipelineStatistics stats_;
    
    // ==========================================================================
    // 🔥 내부 메서드들
    // ==========================================================================
    
    /**
     * @brief 스레드별 처리 루프
     */
    void ProcessingThreadLoop(size_t thread_index);
    
    /**
     * @brief 배치 데이터 처리
     */
    void ProcessBatch(const std::vector<PipelineData>& batch, size_t thread_index);
    
    /**
     * @brief Redis에 데이터 저장
     */
    void WriteToRedis(const PipelineData& data);
    
    /**
     * @brief InfluxDB에 데이터 저장
     */
    void WriteToInflux(const PipelineData& data);
    
    /**
     * @brief 알람 조건 체크
     */
    bool CheckAlarmCondition(const std::string& device_id, 
                           const Structs::TimestampedValue& value);
    
    /**
     * @brief RabbitMQ에 알람 발행
     */
    void PublishAlarmToRabbitMQ(const std::string& device_id,
                               const Structs::TimestampedValue& value,
                               const std::string& severity);
    
    /**
     * @brief 높은 우선순위 데이터 체크
     */
    bool IsHighPriorityData(const Structs::TimestampedValue& value);
    
    /**
     * @brief 알람 이벤트 직렬화
     */
    std::string SerializeAlarmEvent(const AlarmEvent& alarm);
    
    /**
     * @brief TimestampedValue 직렬화
     */
    std::string SerializeTimestampedValue(const Structs::TimestampedValue& value);
    
    /**
     * @brief 처리 시간 통계 업데이트
     */
    void UpdateProcessingTimeStats(double processing_time_ms);
    
    /**
     * @brief 큐 크기 통계 업데이트
     */
    void UpdateQueueSizeStats();
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_WORKER_PIPELINE_MANAGER_H