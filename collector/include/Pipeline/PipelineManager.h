// =============================================================================
// collector/include/Pipeline/PipelineManager.h - 멀티스레드 개선
// =============================================================================
#ifndef PULSEONE_PIPELINE_MANAGER_H
#define PULSEONE_PIPELINE_MANAGER_H

#include "Common/Structs.h"
#include "Pipeline/DataProcessingService.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <vector>

namespace PulseOne {
namespace Pipeline {

/**
 * @brief 멀티스레드 전역 파이프라인 매니저 (싱글톤)
 * @details 🔥 병목 해결: 1개 큐 + N개 처리 스레드로 병렬 처리
 */
class PipelineManager {
public:
    // ==========================================================================
    // 🔥 싱글톤 패턴
    // ==========================================================================
    
    static PipelineManager& GetInstance() {
        static PipelineManager instance;
        return instance;
    }
    
    // 복사/이동/대입 방지
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) = delete;
    PipelineManager& operator=(PipelineManager&&) = delete;

    // ==========================================================================
    // 🔥 전역 초기화
    // ==========================================================================
    
    bool Initialize(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client = nullptr,
        size_t processing_threads = 0  // 0 = CPU 코어 수 자동
    );
    
    bool Start();
    void Shutdown();
    
    bool IsInitialized() const { return is_initialized_.load(); }
    bool IsRunning() const { return is_running_.load(); }

    // ==========================================================================
    // 🔥 Worker 인터페이스 (변경 없음)
    // ==========================================================================
    
    bool SendDeviceData(
        const std::string& device_id,
        const std::vector<Structs::TimestampedValue>& values,
        const std::string& worker_id = "unknown",
        uint32_t priority = 0
    );
    
    // ==========================================================================
    // 🔥 모니터링
    // ==========================================================================
    
    struct PipelineStats {
        uint64_t total_received = 0;
        uint64_t total_processed = 0;
        uint64_t total_dropped = 0;
        size_t current_queue_size = 0;
        uint64_t redis_writes = 0;
        double avg_processing_time_ms = 0.0;
        size_t active_processing_threads = 0;  // 활성 처리 스레드 수
    };
    
    PipelineStats GetStatistics() const;
    void ResetStatistics();

private:
    PipelineManager() = default;
    ~PipelineManager() { Shutdown(); }

    // ==========================================================================
    // 🔥 멀티스레드 큐 처리 시스템
    // ==========================================================================
    
    // 초기화 상태
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // 🔥 공유 큐 (1개) + 멀티스레드 처리 (N개)
    std::queue<Structs::DeviceDataMessage> global_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 🔥 멀티 처리 스레드들
    std::vector<std::thread> processing_threads_;
    size_t num_processing_threads_ = 4;  // 기본 4개 스레드
    
    // 🔥 DataProcessingService (스레드 안전)
    std::shared_ptr<DataProcessingService> data_processing_service_;
    
    // 설정
    size_t max_queue_size_ = 100000;
    size_t batch_size_ = 500;
    
    // 통계
    std::atomic<uint64_t> total_received_{0};
    std::atomic<uint64_t> total_processed_{0};
    std::atomic<uint64_t> total_dropped_{0};
    std::atomic<size_t> current_queue_size_{0};
    
    // ==========================================================================
    // 🔥 멀티스레드 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 각 처리 스레드의 루프 (N개 스레드가 병렬 실행)
     */
    void ProcessingThreadLoop(size_t thread_id);
    
    /**
     * @brief 큐에서 배치 수집 (스레드 안전)
     */
    std::vector<Structs::DeviceDataMessage> CollectBatch(size_t thread_id);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_PIPELINE_MANAGER_H