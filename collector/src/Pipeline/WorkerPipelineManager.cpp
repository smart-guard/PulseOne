// collector/src/Pipeline/WorkerPipelineManager.cpp
#include "Pipeline/WorkerPipelineManager.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

WorkerPipelineManager::WorkerPipelineManager(
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client,
    std::shared_ptr<RabbitMQClient> rabbitmq_client)
    : redis_client_(redis_client)
    , influx_client_(influx_client)
    , rabbitmq_client_(rabbitmq_client) {
    
    // 기본 스레드 수를 CPU 코어 수로 설정 (최소 2개, 최대 8개)
    thread_count_ = std::max(2u, std::min(8u, std::thread::hardware_concurrency()));
    
    LogManager::getInstance().Info("WorkerPipelineManager created with {} threads", thread_count_);
}

WorkerPipelineManager::~WorkerPipelineManager() {
    Stop();
    LogManager::getInstance().Info("WorkerPipelineManager destroyed");
}

// =============================================================================
// 🔥 공개 인터페이스 구현
// =============================================================================

bool WorkerPipelineManager::ProcessDeviceData(
    const std::string& device_id,
    const std::vector<Structs::TimestampedValue>& values,
    uint32_t priority) {
    
    if (values.empty() || !is_running_.load()) {
        return false;
    }
    
    // 🔥 DeviceDataMessage 생성
    Structs::DeviceDataMessage message;
    message.device_id = device_id;
    message.protocol = "AUTO_DETECTED";  // Worker에서 설정하도록 개선 가능
    message.points = values;
    message.priority = priority;
    
    // 🔥 라운드 로빈으로 스레드 선택
    size_t thread_index = round_robin_counter_.fetch_add(1) % thread_count_;
    
    {
        std::lock_guard<std::mutex> lock(thread_mutexes_[thread_index]);
        
        // 큐 오버플로우 체크
        if (thread_queues_[thread_index].size() >= max_queue_size_ / thread_count_) {
            stats_.total_dropped.fetch_add(1);
            LogManager::getInstance().Warn("Pipeline queue {} overflow, dropping data for device: {}", 
                                         thread_index, device_id);
            return false;
        }
        
        // 🔥 DeviceDataMessage를 큐에 추가
        thread_queues_[thread_index].push(message);
    }
    
    // 해당 스레드 깨우기
    thread_cvs_[thread_index].notify_one();
    
    // 통계 업데이트
    UpdateQueueSizeStats();
    
    return true;
}

bool WorkerPipelineManager::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("WorkerPipelineManager already running");
        return false;
    }
    
    LogManager::getInstance().Info("Starting WorkerPipelineManager with {} threads...", thread_count_);
    
    // 🔥 멀티스레드 큐 시스템 초기화
    thread_queues_.resize(thread_count_);
    thread_mutexes_.resize(thread_count_);
    thread_cvs_.resize(thread_count_);
    processing_threads_.reserve(thread_count_);
    
    is_running_ = true;
    
    // 🔥 처리 스레드들 시작
    for (size_t i = 0; i < thread_count_; ++i) {
        processing_threads_.emplace_back(&WorkerPipelineManager::ProcessingThreadLoop, this, i);
        LogManager::getInstance().Debug("Started processing thread {}", i);
    }
    
    // 통계 초기화
    stats_.start_time = std::chrono::system_clock::now();
    
    LogManager::getInstance().Info("WorkerPipelineManager started successfully");
    return true;
}

void WorkerPipelineManager::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("Stopping WorkerPipelineManager...");
    
    // 🔥 모든 스레드에 종료 신호
    is_running_ = false;
    
    // 모든 condition variable 깨우기
    for (auto& cv : thread_cvs_) {
        cv.notify_all();
    }
    
    // 🔥 모든 스레드 종료 대기
    for (size_t i = 0; i < processing_threads_.size(); ++i) {
        if (processing_threads_[i].joinable()) {
            processing_threads_[i].join();
            LogManager::getInstance().Debug("Stopped processing thread {}", i);
        }
    }
    
    // 리소스 정리
    processing_threads_.clear();
    thread_queues_.clear();
    thread_mutexes_.clear();
    thread_cvs_.clear();
    
    LogManager::getInstance().Info("WorkerPipelineManager stopped");
}

void WorkerPipelineManager::SetThreadCount(size_t thread_count) {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("Cannot change thread count while running");
        return;
    }
    
    thread_count_ = std::max(1u, std::min(16u, static_cast<unsigned>(thread_count)));
    LogManager::getInstance().Info("Thread count set to {}", thread_count_);
}

void WorkerPipelineManager::ResetStatistics() {
    stats_.total_processed = 0;
    stats_.total_dropped = 0;
    stats_.redis_writes = 0;
    stats_.influx_writes = 0;
    stats_.rabbitmq_publishes = 0;
    stats_.alarm_events = 0;
    stats_.high_priority_events = 0;
    stats_.current_queue_size = 0;
    stats_.avg_processing_time_ms = 0.0;
    stats_.start_time = std::chrono::system_clock::now();
    
    LogManager::getInstance().Info("Pipeline statistics reset");
}

// =============================================================================
// 🔥 멀티스레드 처리 로직
// =============================================================================

void WorkerPipelineManager::ProcessingThreadLoop(size_t thread_index) {
    LogManager::getInstance().Info("Pipeline processing thread {} started", thread_index);
    
    while (is_running_.load()) {
        std::vector<Structs::DeviceDataMessage> batch;
        batch.reserve(batch_size_);
        
        // 🔥 큐에서 배치 데이터 수집
        {
            std::unique_lock<std::mutex> lock(thread_mutexes_[thread_index]);
            
            // 데이터가 있거나 종료 신호까지 대기
            thread_cvs_[thread_index].wait(lock, [this, thread_index] {
                return !thread_queues_[thread_index].empty() || !is_running_.load();
            });
            
            // 배치 크기만큼 또는 큐가 빌 때까지 수집
            while (!thread_queues_[thread_index].empty() && batch.size() < batch_size_) {
                batch.push_back(std::move(thread_queues_[thread_index].front()));
                thread_queues_[thread_index].pop();
            }
        }
        
        // 🔥 배치 처리 (락 해제 후)
        if (!batch.empty()) {
            ProcessBatch(batch, thread_index);
        }
    }
    
    LogManager::getInstance().Info("Pipeline processing thread {} stopped", thread_index);
}

void WorkerPipelineManager::ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index) {
    auto start_time = std::chrono::steady_clock::now();
    
    for (const auto& message : batch) {
        try {
            // 🔥 1. Redis에 최신 값 저장 (DeviceDataMessage 활용)
            WriteToRedis(message);
            stats_.redis_writes.fetch_add(1);
            
            // 🔥 2. InfluxDB에 시계열 저장
            WriteToInflux(message);
            stats_.influx_writes.fetch_add(1);
            
            // 🔥 3. 각 값에 대해 알람 체크
            for (const auto& point : message.points) {
                // 알람 조건 체크
                if (CheckAlarmCondition(message.device_id, point)) {
                    PublishAlarmToRabbitMQ(message.device_id, point, "HIGH");
                    stats_.alarm_events.fetch_add(1);
                }
                
                // 높은 우선순위 데이터 체크
                if (message.priority > 0 || IsHighPriorityData(point)) {
                    // 중요 데이터는 RabbitMQ로 실시간 알림
                    PublishAlarmToRabbitMQ(message.device_id, point, "PRIORITY");
                    stats_.high_priority_events.fetch_add(1);
                }
            }
            
            stats_.total_processed.fetch_add(message.points.size());
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Thread {} processing error for device {}: {}", 
                                          thread_index, message.device_id, e.what());
        }
    }
    
    // 처리 시간 통계 업데이트
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double processing_time_ms = static_cast<double>(duration.count());
    
    UpdateProcessingTimeStats(processing_time_ms);
    
    LogManager::getInstance().Debug("Thread {} processed batch of {} messages in {}ms", 
                                   thread_index, batch.size(), processing_time_ms);
}

// =============================================================================
// 🔥 데이터 저장 구현
// =============================================================================

void WorkerPipelineManager::WriteToRedis(const Structs::DeviceDataMessage& message) {
    if (!redis_client_) return;
    
    try {
        // 🔥 DeviceDataMessage의 ToJSON() 메서드 활용
        std::string json_data = message.ToJSON();
        
        // 디바이스별 최신 데이터 저장
        std::string key = "device:" + message.device_id + ":latest";
        redis_client_->Set(key, json_data);
        
        // 개별 포인트별로도 저장 (빠른 조회용)
        for (const auto& point : message.points) {
            std::string point_key = "device:" + message.device_id + ":point:" + point.point_id;
            std::string point_json = point.ToJSON();  // TimestampedValue의 ToJSON 사용
            redis_client_->Set(point_key, point_json);
        }
        
        // 디바이스 마지막 업데이트 시간
        std::string last_update_key = "device:" + message.device_id + ":last_update";
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        redis_client_->Set(last_update_key, std::to_string(timestamp));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis write error for device {}: {}", message.device_id, e.what());
    }
}

void WorkerPipelineManager::WriteToInflux(const Structs::DeviceDataMessage& message) {
    if (!influx_client_) return;
    
    try {
        for (const auto& point : message.points) {
            // InfluxDB 라인 프로토콜 생성
            std::ostringstream line;
            line << "device_data,";
            line << "device_id=" << message.device_id << ",";
            line << "protocol=" << message.protocol << ",";
            line << "point_id=" << point.point_id << " ";
            
            // variant 값 처리
            std::visit([&line](const auto& v) {
                line << "value=" << v;
            }, point.value);
            
            line << " ";
            
            // 나노초 타임스탬프
            auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                point.timestamp.time_since_epoch()).count();
            line << timestamp_ns;
            
            influx_client_->Write(line.str());
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("InfluxDB write error for device {}: {}", message.device_id, e.what());
    }
}

// =============================================================================
// 🔥 알람 및 이벤트 처리
// =============================================================================

bool WorkerPipelineManager::CheckAlarmCondition(
    const std::string& device_id, 
    const Structs::TimestampedValue& value) {
    
    // 임계값 체크
    auto threshold_key = device_id + ":" + value.point_id;
    auto it = alarm_thresholds_.find(threshold_key);
    if (it != alarm_thresholds_.end()) {
        return value.value.numeric_value > it->second;
    }
    
    // 기본 임계값들
    if (value.point_id.find("temperature") != std::string::npos) {
        return value.value.numeric_value > 80.0;  // 온도 80도 초과
    }
    if (value.point_id.find("pressure") != std::string::npos) {
        return value.value.numeric_value > 10.0;  // 압력 10 초과
    }
    
    return false;
}

void WorkerPipelineManager::PublishAlarmToRabbitMQ(
    const std::string& device_id,
    const Structs::TimestampedValue& point,
    const std::string& severity) {
    
    if (!rabbitmq_client_) return;
    
    try {
        // 🔥 Structs::AlarmEvent 사용
        Structs::AlarmEvent alarm;
        alarm.device_id = device_id;
        alarm.point_id = point.point_id;
        alarm.current_value = point.value;
        alarm.severity = severity;
        alarm.timestamp = point.timestamp;
        alarm.message = "Value exceeded threshold";
        
        // 🔥 AlarmEvent의 ToJSON() 메서드 활용
        std::string json_alarm = alarm.ToJSON();
        
        // RabbitMQ에 발행
        std::string exchange = "alarms";
        std::string routing_key = severity == "CRITICAL" ? "alarm.critical" : "alarm.normal";
        
        rabbitmq_client_->Publish(exchange, routing_key, json_alarm);
        stats_.rabbitmq_publishes.fetch_add(1);
        
        LogManager::getInstance().Info("Published {} alarm for device {} point {}", 
                                     severity, device_id, point.point_id);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("RabbitMQ publish error: {}", e.what());
    }
}

bool WorkerPipelineManager::IsHighPriorityData(const Structs::TimestampedValue& point) {
    // 특정 포인트들은 항상 높은 우선순위
    return point.point_id.find("critical") != std::string::npos ||
           point.point_id.find("emergency") != std::string::npos ||
           point.point_id.find("alarm") != std::string::npos;
}

// =============================================================================
// 🔥 유틸리티 메서드들 (SerializeAlarmEvent, SerializeTimestampedValue 제거)
// =============================================================================

void WorkerPipelineManager::UpdateProcessingTimeStats(double processing_time_ms) {
    // 이동 평균으로 처리 시간 업데이트
    double current_avg = stats_.avg_processing_time_ms.load();
    double new_avg = (current_avg * 0.95) + (processing_time_ms * 0.05);
    stats_.avg_processing_time_ms.store(new_avg);
}

void WorkerPipelineManager::UpdateQueueSizeStats() {
    size_t total_queue_size = 0;
    for (size_t i = 0; i < thread_count_; ++i) {
        std::lock_guard<std::mutex> lock(thread_mutexes_[i]);
        total_queue_size += thread_queues_[i].size();
    }
    stats_.current_queue_size.store(total_queue_size);
}

} // namespace Pipeline
} // namespace PulseOne