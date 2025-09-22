// =============================================================================
// collector/src/Pipeline/PipelineManager.cpp - 순수 큐 관리 구현
// =============================================================================

#include "Pipeline/PipelineManager.h"
#include "Utils/LogManager.h"
#include <chrono>

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 🔥 순수 큐 관리 구현
// =============================================================================

void PipelineManager::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("⚠️ PipelineManager already running");
        return;
    }
    
    is_running_ = true;
    LogManager::getInstance().Info("✅ PipelineManager 큐 시스템 시작됨");
}

void PipelineManager::Shutdown() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("🛑 PipelineManager 큐 시스템 종료 시작...");
    
    is_running_ = false;
    queue_cv_.notify_all(); // 대기 중인 모든 스레드 깨우기
    
    // 남은 데이터 정리
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        size_t remaining = data_queue_.size();
        if (remaining > 0) {
            LogManager::getInstance().Warn("⚠️ 큐에 {}개 미처리 데이터 남음", remaining);
        }
        
        // 큐 비우기
        while (!data_queue_.empty()) {
            data_queue_.pop();
        }
    }
    
    LogManager::getInstance().Info("✅ PipelineManager 큐 시스템 종료 완료");
}

bool PipelineManager::SendDeviceData(const Structs::DeviceDataMessage& message) {
    if (!is_running_.load() || message.points.empty()) {
        return false;
    }
    
    try {
        // 🔥 큐에 직접 추가 (오버플로우 체크)
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            
            // 오버플로우 체크
            if (data_queue_.size() >= MAX_QUEUE_SIZE) {
                total_dropped_.fetch_add(1);
                LogManager::getInstance().Warn("❌ 큐 오버플로우! 데이터 드롭: {} (포인트: {}개)", 
                                             message.device_id, message.points.size());
                return false;
            }
            
            // 큐에 추가 (복사 or 이동)
            data_queue_.push(message);
        }
        
        // 대기 중인 처리기 깨우기
        queue_cv_.notify_one();
        
        // 통계 업데이트
        total_received_.fetch_add(1);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("SendDeviceData 예외: {}", e.what());
        return false;
    }
}

bool PipelineManager::SendDeviceData(
    const std::string& device_id,
    const std::vector<Structs::TimestampedValue>& values,
    const std::string& worker_id,
    uint32_t priority) {
    
    if (!is_running_.load() || values.empty()) {
        return false;
    }
    
    // DeviceDataMessage 생성 (기존 공식 구조체 사용!)
    Structs::DeviceDataMessage message;
    message.device_id = device_id;
    message.protocol = "AUTO_DETECTED";  // Worker에서 설정 가능
    message.points = values;              // 🔥 기존 points 필드 사용!
    message.priority = priority;
    message.timestamp = std::chrono::system_clock::now();
    
    // 🔥 큐에 추가 (오버플로우 체크)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // 오버플로우 체크
        if (data_queue_.size() >= MAX_QUEUE_SIZE) {
            total_dropped_.fetch_add(1);
            LogManager::getInstance().Warn("❌ 큐 오버플로우! 데이터 드롭: {} (Worker: {})", 
                                         device_id, worker_id);
            return false;
        }
        
        // 큐에 추가
        data_queue_.push(std::move(message));
    }
    
    // 대기 중인 처리기 깨우기
    queue_cv_.notify_one();
    
    // 통계 업데이트
    total_received_.fetch_add(1);
    
    return true;
}

std::vector<Structs::DeviceDataMessage> PipelineManager::GetBatch(
    size_t max_batch_size,
    uint32_t timeout_ms) {
    
    std::vector<Structs::DeviceDataMessage> batch;
    batch.reserve(max_batch_size);
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // 타임아웃 또는 데이터 있을 때까지 대기
    auto timeout = std::chrono::milliseconds(timeout_ms);
    bool has_data = queue_cv_.wait_for(lock, timeout, [this] {
        return !data_queue_.empty() || !is_running_.load();
    });
    
    if (!has_data || !is_running_.load()) {
        return batch; // 빈 배치 반환
    }
    
    // 배치 크기만큼 수집
    while (!data_queue_.empty() && batch.size() < max_batch_size) {
        batch.push_back(std::move(data_queue_.front()));
        data_queue_.pop();
    }
    
    // 통계 업데이트
    total_delivered_.fetch_add(batch.size());
    
    return batch;
}

bool PipelineManager::IsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return data_queue_.empty();
}

size_t PipelineManager::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return data_queue_.size();
}

bool PipelineManager::IsOverflowing() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return data_queue_.size() >= OVERFLOW_THRESHOLD;
}

PipelineManager::QueueStats PipelineManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    QueueStats stats;
    stats.total_received = total_received_.load();
    stats.total_delivered = total_delivered_.load();
    stats.total_dropped = total_dropped_.load();
    stats.current_queue_size = data_queue_.size();
    stats.max_queue_size = MAX_QUEUE_SIZE;
    stats.fill_percentage = (static_cast<double>(data_queue_.size()) / MAX_QUEUE_SIZE) * 100.0;
    
    return stats;
}

void PipelineManager::ResetStatistics() {
    total_received_ = 0;
    total_delivered_ = 0;
    total_dropped_ = 0;
    
    LogManager::getInstance().Info("📊 PipelineManager 통계 리셋됨");
}

} // namespace Pipeline
} // namespace PulseOne