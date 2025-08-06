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

// =============================================================================
// collector/src/Pipeline/PipelineManager.cpp - 멀티스레드 구현
// =============================================================================
#include "Pipeline/PipelineManager.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 🔥 초기화 - 멀티스레드 설정
// =============================================================================

bool PipelineManager::Initialize(
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client,
    size_t processing_threads) {
    
    if (is_initialized_.load()) {
        LogManager::getInstance().Warn("⚠️ PipelineManager already initialized");
        return true;
    }
    
    LogManager::getInstance().Info("🚀 PipelineManager 멀티스레드 초기화 시작...");
    
    try {
        // 🔥 처리 스레드 수 결정
        if (processing_threads == 0) {
            // CPU 코어 수 기반 (최소 2개, 최대 8개)
            num_processing_threads_ = std::max(2u, std::min(8u, std::thread::hardware_concurrency()));
        } else {
            num_processing_threads_ = std::max(1u, std::min(16u, processing_threads));
        }
        
        LogManager::getInstance().Info("🔧 처리 스레드 수: {}개", num_processing_threads_);
        
        // DataProcessingService 생성 (스레드 안전)
        data_processing_service_ = std::make_shared<DataProcessingService>(
            redis_client, influx_client);
        
        is_initialized_ = true;
        
        LogManager::getInstance().Info("✅ PipelineManager 멀티스레드 초기화 완료");
        LogManager::getInstance().Info("📊 설정 - 큐: {}, 배치: {}, 스레드: {}개", 
                                     max_queue_size_, batch_size_, num_processing_threads_);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ PipelineManager 초기화 실패: {}", e.what());
        return false;
    }
}

bool PipelineManager::Start() {
    if (!is_initialized_.load()) {
        LogManager::getInstance().Error("❌ PipelineManager not initialized!");
        return false;
    }
    
    if (is_running_.load()) {
        LogManager::getInstance().Warn("⚠️ PipelineManager already running");
        return true;
    }
    
    LogManager::getInstance().Info("🚀 PipelineManager 멀티스레드 시작...");
    
    try {
        // DataProcessingService 시작
        if (!data_processing_service_->Start()) {
            LogManager::getInstance().Error("❌ DataProcessingService 시작 실패");
            return false;
        }
        
        is_running_ = true;
        should_stop_ = false;
        
        // 🔥 N개 처리 스레드 시작
        processing_threads_.clear();
        processing_threads_.reserve(num_processing_threads_);
        
        for (size_t i = 0; i < num_processing_threads_; ++i) {
            processing_threads_.emplace_back(&PipelineManager::ProcessingThreadLoop, this, i);
            LogManager::getInstance().Debug("🧵 처리 스레드 {} 시작됨", i);
        }
        
        LogManager::getInstance().Info("✅ PipelineManager 멀티스레드 시작 완료 ({}개 스레드)", 
                                     num_processing_threads_);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ PipelineManager 시작 실패: {}", e.what());
        return false;
    }
}

void PipelineManager::Shutdown() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("🛑 PipelineManager 멀티스레드 종료 시작...");
    
    should_stop_ = true;
    queue_cv_.notify_all();  // 모든 스레드 깨우기
    
    // 🔥 모든 처리 스레드 종료 대기
    for (size_t i = 0; i < processing_threads_.size(); ++i) {
        if (processing_threads_[i].joinable()) {
            processing_threads_[i].join();
            LogManager::getInstance().Debug("🧵 처리 스레드 {} 종료됨", i);
        }
    }
    processing_threads_.clear();
    
    // DataProcessingService 종료
    if (data_processing_service_) {
        data_processing_service_->Stop();
    }
    
    is_running_ = false;
    
    LogManager::getInstance().Info("✅ PipelineManager 멀티스레드 종료 완료");
    LogManager::getInstance().Info("📊 최종 통계 - 받음: {}, 처리: {}, 누락: {}", 
                                 total_received_.load(), total_processed_.load(), total_dropped_.load());
}

// =============================================================================
// 🔥 Worker 인터페이스 (변경 없음)
// =============================================================================

bool PipelineManager::SendDeviceData(
    const std::string& device_id,
    const std::vector<Structs::TimestampedValue>& values,
    const std::string& worker_id,
    uint32_t priority) {
    
    if (!is_running_.load() || values.empty()) {
        return false;
    }
    
    // DeviceDataMessage 생성
    Structs::DeviceDataMessage message;
    message.device_id = device_id;
    message.protocol = "AUTO_DETECTED";
    message.points = values;
    message.priority = priority;
    message.timestamp = std::chrono::system_clock::now();
    
    // 🔥 공유 큐에 추가 (스레드 안전)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // 큐 오버플로우 체크
        if (global_queue_.size() >= max_queue_size_) {
            total_dropped_.fetch_add(1);
            LogManager::getInstance().Warn("❌ 전역 큐 오버플로우, 데이터 누락: {} (Worker: {})", 
                                         device_id, worker_id);
            return false;
        }
        
        // 큐에 추가
        global_queue_.push(std::move(message));
        current_queue_size_.store(global_queue_.size());
    }
    
    // 🔥 대기 중인 스레드 하나 깨우기
    queue_cv_.notify_one();
    
    // 통계 업데이트
    total_received_.fetch_add(1);
    
    LogManager::getInstance().Debug("✅ [{}] 디바이스 {} 데이터 전역큐 추가 ({}개 포인트)", 
                                   worker_id, device_id, values.size());
    return true;
}

// =============================================================================
// 🔥 멀티스레드 처리 루프
// =============================================================================

void PipelineManager::ProcessingThreadLoop(size_t thread_id) {
    LogManager::getInstance().Info("🧵 처리 스레드 {} 시작", thread_id);
    
    while (!should_stop_.load()) {
        try {
            // 🔥 큐에서 배치 수집 (스레드 경쟁)
            auto batch = CollectBatch(thread_id);
            
            if (!batch.empty()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                
                // 🔥 DataProcessingService에 배치 전달
                if (data_processing_service_) {
                    data_processing_service_->ProcessBatch(batch);
                    total_processed_.fetch_add(batch.size());
                    
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                    
                    LogManager::getInstance().Debug("🧵 스레드 {} 배치 처리: {}개 메시지, {}μs", 
                                                   thread_id, batch.size(), duration.count());
                } else {
                    LogManager::getInstance().Error("❌ DataProcessingService 없음! (스레드 {})", thread_id);
                }
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("💥 스레드 {} 처리 중 예외: {}", thread_id, e.what());
            // 예외 발생 시 잠시 대기 후 계속
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogManager::getInstance().Info("🧵 처리 스레드 {} 종료", thread_id);
}

std::vector<Structs::DeviceDataMessage> PipelineManager::CollectBatch(size_t thread_id) {
    std::vector<Structs::DeviceDataMessage> batch;
    batch.reserve(batch_size_);
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // 🔥 데이터가 있거나 종료 신호까지 대기
    queue_cv_.wait(lock, [this] {
        return !global_queue_.empty() || should_stop_.load();
    });
    
    // 🔥 배치 크기만큼 수집 (스레드들이 경쟁적으로 가져감)
    while (!global_queue_.empty() && batch.size() < batch_size_) {
        batch.push_back(std::move(global_queue_.front()));
        global_queue_.pop();
    }
    
    current_queue_size_.store(global_queue_.size());
    
    if (!batch.empty()) {
        LogManager::getInstance().Debug("🧵 스레드 {} 배치 수집: {}개 메시지 (큐 남은 크기: {})", 
                                       thread_id, batch.size(), global_queue_.size());
    }
    
    return batch;
}

// =============================================================================
// 🔥 통계
// =============================================================================

PipelineManager::PipelineStats PipelineManager::GetStatistics() const {
    PipelineStats stats;
    stats.total_received = total_received_.load();
    stats.total_processed = total_processed_.load();
    stats.total_dropped = total_dropped_.load();
    stats.current_queue_size = current_queue_size_.load();
    stats.active_processing_threads = processing_threads_.size();
    
    return stats;
}

void PipelineManager::ResetStatistics() {
    total_received_ = 0;
    total_processed_ = 0;
    total_dropped_ = 0;
    current_queue_size_ = 0;
    
    LogManager::getInstance().Info("📊 PipelineManager 통계 리셋됨");
}

} // namespace Pipeline
} // namespace PulseOne