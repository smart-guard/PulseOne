// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 멀티스레드 처리 전용
// 🔥 핵심! WorkerPipelineManager의 큐에 접근해서 배치로 처리
// =============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace PulseOne {
namespace Pipeline {

// 전방 선언
class WorkerPipelineManager;

/**
 * @brief DataProcessingService - 멀티스레드 데이터 처리 엔진
 * @details WorkerPipelineManager의 큐에 접근해서 배치로 데이터 처리
 * 8개 스레드가 큐에서 데이터 꺼내서 순차 처리: 가상포인트 → 알람 → Redis → InfluxDB
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
    // 🔥 핵심 인터페이스
    // ==========================================================================
    
    /**
     * @brief 서비스 시작 - 멀티스레드 처리기들 시작
     * @param pipeline_manager WorkerPipelineManager 참조 (큐 접근용)
     */
    bool Start(std::shared_ptr<WorkerPipelineManager> pipeline_manager);
    
    /**
     * @brief 서비스 중지 - 모든 스레드 종료
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }
    
    /**
     * @brief 처리 스레드 수 설정 (시작 전에만 가능)
     */
    void SetThreadCount(size_t thread_count);
    
    /**
     * @brief 배치 크기 설정
     */
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // WorkerPipelineManager 참조 (큐 접근용)
    std::weak_ptr<WorkerPipelineManager> pipeline_manager_;
    
    // 멀티스레드 관리
    std::vector<std::thread> processing_threads_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    size_t thread_count_{8};
    size_t batch_size_{500};
    
    // 통계
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    
    // ==========================================================================
    // 🔥 멀티스레드 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 처리 스레드 메인 루프
     * @param thread_index 스레드 인덱스 (0~7)
     */
    void ProcessingThreadLoop(size_t thread_index);
    
    /**
     * @brief WorkerPipelineManager 큐에서 배치 수집
     * @return 수집된 배치 (최대 batch_size_개)
     */
    std::vector<Structs::DeviceDataMessage> CollectBatchFromQueue();
    
    /**
     * @brief 수집된 배치를 순차 처리
     * @param batch 처리할 배치
     * @param thread_index 처리하는 스레드 인덱스
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, 
                     size_t thread_index);
    
    // ==========================================================================
    // 🔥 단계별 처리 메서드들 (순차 실행)
    // ==========================================================================
    
    /**
     * @brief 1단계: 가상포인트 계산 (나중에 구현)
     */
    std::vector<Structs::DeviceDataMessage> CalculateVirtualPoints(
        const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 2단계: 알람 체크 (나중에 구현)
     */
    void CheckAlarms(const std::vector<Structs::DeviceDataMessage>& all_data);
    
    /**
     * @brief 3단계: Redis 저장 (지금 구현!)
     */
    void SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 4단계: InfluxDB 저장 (나중에 구현)
     */
    void SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch);
    
    // ==========================================================================
    // 🔥 Redis 저장 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 디바이스 데이터를 Redis에 저장
     */
    void WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief TimestampedValue를 JSON으로 변환
     */
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H