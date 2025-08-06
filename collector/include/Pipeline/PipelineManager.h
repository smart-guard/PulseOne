// =============================================================================
// collector/include/Pipeline/PipelineManager.h - 순수 큐 관리자 (수정됨)
// 🔥 DataProcessingService 제거 - 순수하게 큐 관리만!
// =============================================================================

#ifndef PULSEONE_PIPELINE_MANAGER_H
#define PULSEONE_PIPELINE_MANAGER_H

#include "Common/Structs.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>

namespace PulseOne {
namespace Pipeline {

/**
 * @brief 순수 큐 관리자 (싱글톤) - 데이터 수집 및 큐잉만 담당
 * @details Worker들로부터 데이터를 받아서 큐에 저장하는 역할만!
 * DataProcessingService와 완전 분리됨
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
    // 🔥 순수 큐 관리 인터페이스
    // ==========================================================================
    
    /**
     * @brief Worker에서 데이터 전송 (큐에 추가만!)
     */
    bool SendDeviceData(
        const std::string& device_id,
        const std::vector<Structs::TimestampedValue>& values,
        const std::string& worker_id = "unknown",
        uint32_t priority = 0
    );
    
    /**
     * @brief 큐에서 배치 데이터 가져오기 (DataProcessingService가 호출)
     * @param max_batch_size 최대 배치 크기
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 배치 데이터 (빈 벡터면 타임아웃)
     */
    std::vector<Structs::DeviceDataMessage> GetBatch(
        size_t max_batch_size = 500,
        uint32_t timeout_ms = 100
    );
    
    /**
     * @brief 큐가 비어있는지 확인
     */
    bool IsEmpty() const;
    
    /**
     * @brief 현재 큐 크기
     */
    size_t GetQueueSize() const;
    
    /**
     * @brief 큐 오버플로우 여부 확인
     */
    bool IsOverflowing() const;

    // ==========================================================================
    // 🔥 상태 관리
    // ==========================================================================
    
    bool IsRunning() const { return is_running_.load(); }
    
    void Start();
    void Shutdown();
    
    // ==========================================================================
    // 🔥 통계 및 모니터링
    // ==========================================================================
    
    struct QueueStats {
        uint64_t total_received = 0;
        uint64_t total_delivered = 0;
        uint64_t total_dropped = 0;
        size_t current_queue_size = 0;
        size_t max_queue_size = 0;
        double fill_percentage = 0.0;
    };
    
    QueueStats GetStatistics() const;
    void ResetStatistics();

private:
    PipelineManager() = default;
    ~PipelineManager() { Shutdown(); }

    // ==========================================================================
    // 🔥 순수 큐 시스템 (처리 로직 없음!)
    // ==========================================================================
    
    // 상태 관리
    std::atomic<bool> is_running_{false};
    
    // 🔥 순수 큐 시스템
    std::queue<Structs::DeviceDataMessage> data_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 설정
    static constexpr size_t MAX_QUEUE_SIZE = 100000;
    static constexpr size_t OVERFLOW_THRESHOLD = 90000; // 90% 임계점
    
    // 통계 (스레드 안전)
    std::atomic<uint64_t> total_received_{0};
    std::atomic<uint64_t> total_delivered_{0};
    std::atomic<uint64_t> total_dropped_{0};
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_PIPELINE_MANAGER_H