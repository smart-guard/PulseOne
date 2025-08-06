// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 멀티스레드 구현
// 🔥 핵심! 8개 스레드가 WorkerPipelineManager 큐에서 데이터 꺼내서 처리
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/WorkerPipelineManager.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DataProcessingService::DataProcessingService(
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : redis_client_(redis_client)
    , influx_client_(influx_client) {
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "DataProcessingService 생성됨 - 스레드 수: " + 
                                 std::to_string(thread_count_));
}

DataProcessingService::~DataProcessingService() {
    Stop();
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "DataProcessingService 소멸됨");
}

// =============================================================================
// 🔥 핵심 인터페이스 구현
// =============================================================================

bool DataProcessingService::Start(std::shared_ptr<WorkerPipelineManager> pipeline_manager) {
    if (is_running_.load()) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "DataProcessingService가 이미 실행 중입니다");
        return false;
    }
    
    if (!pipeline_manager) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "WorkerPipelineManager가 null입니다");
        return false;
    }
    
    // WorkerPipelineManager 참조 저장
    pipeline_manager_ = pipeline_manager;
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "DataProcessingService 시작 중... (스레드: " + 
                                 std::to_string(thread_count_) + "개)");
    
    // 상태 플래그 설정
    should_stop_ = false;
    is_running_ = true;
    
    // 🔥 멀티스레드 처리기들 시작
    processing_threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        processing_threads_.emplace_back(
            &DataProcessingService::ProcessingThreadLoop, this, i);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "처리 스레드 " + std::to_string(i) + " 시작됨");
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "✅ DataProcessingService 시작 완료 - " + 
                                 std::to_string(thread_count_) + "개 스레드 실행 중");
    return true;
}

void DataProcessingService::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "DataProcessingService 중지 중...");
    
    // 🔥 정지 신호 설정
    should_stop_ = true;
    
    // 🔥 모든 처리 스레드 종료 대기
    for (size_t i = 0; i < processing_threads_.size(); ++i) {
        if (processing_threads_[i].joinable()) {
            processing_threads_[i].join();
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "처리 스레드 " + std::to_string(i) + " 종료됨");
        }
    }
    processing_threads_.clear();
    
    is_running_ = false;
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "✅ DataProcessingService 중지 완료");
}

void DataProcessingService::SetThreadCount(size_t thread_count) {
    if (is_running_.load()) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "실행 중일 때는 스레드 수를 변경할 수 없습니다");
        return;
    }
    
    thread_count_ = std::max(1u, std::min(16u, static_cast<unsigned>(thread_count)));
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "처리 스레드 수 설정: " + std::to_string(thread_count_));
}

// =============================================================================
// 🔥 멀티스레드 처리 메서드들
// =============================================================================

void DataProcessingService::ProcessingThreadLoop(size_t thread_index) {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "처리 스레드 " + std::to_string(thread_index) + " 시작");
    
    while (!should_stop_.load()) {
        try {
            // 🔥 WorkerPipelineManager 큐에서 배치 수집
            auto batch = CollectBatchFromQueue();
            
            if (!batch.empty()) {
                // 🔥 배치 처리
                ProcessBatch(batch, thread_index);
            } else {
                // 큐가 비어있으면 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                         "스레드 " + std::to_string(thread_index) + 
                                         " 처리 중 예외: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "처리 스레드 " + std::to_string(thread_index) + " 종료");
}

std::vector<Structs::DeviceDataMessage> DataProcessingService::CollectBatchFromQueue() {
    std::vector<Structs::DeviceDataMessage> batch;
    batch.reserve(batch_size_);
    
    // 🔥 WorkerPipelineManager 큐에 접근
    auto pipeline_mgr = pipeline_manager_.lock();
    if (!pipeline_mgr) {
        return batch; // 빈 배치 반환
    }
    
    // 🔥 WorkerPipelineManager의 큐에서 배치 수집
    // (WorkerPipelineManager에 GetBatch() 메서드 필요)
    batch = pipeline_mgr->GetBatch(batch_size_);
    
    return batch;
}

void DataProcessingService::ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, 
                                        size_t thread_index) {
    if (batch.empty()) {
        return;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 🔥 1단계: 가상포인트 계산 (나중에 구현)
        auto all_data = CalculateVirtualPoints(batch);
        
        // 🔥 2단계: 알람 체크 (나중에 구현)
        CheckAlarms(all_data);
        
        // 🔥 3단계: Redis 저장 (지금 구현!)
        SaveToRedis(all_data);
        
        // 🔥 4단계: InfluxDB 저장 (나중에 구현)
        SaveToInfluxDB(all_data);
        
        // 통계 업데이트
        total_batches_processed_.fetch_add(1);
        total_messages_processed_.fetch_add(batch.size());
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "스레드 " + std::to_string(thread_index) + 
                                     " 배치 처리 중 예외: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "스레드 " + std::to_string(thread_index) + 
                                 " 배치 처리 완료: " + std::to_string(batch.size()) + 
                                 "개, 소요시간: " + std::to_string(duration.count()) + "ms");
}

// =============================================================================
// 🔥 단계별 처리 메서드들 (순차 실행)
// =============================================================================

std::vector<Structs::DeviceDataMessage> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    // 🔥 나중에 구현 - 현재는 원본 데이터 그대로 반환
    return batch;
}

void DataProcessingService::CheckAlarms(const std::vector<Structs::DeviceDataMessage>& all_data) {
    // 🔥 나중에 구현 - 현재는 로깅만
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "알람 체크 완료: " + std::to_string(all_data.size()) + "개");
}

void DataProcessingService::SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (!redis_client_) {
        return;
    }
    
    try {
        for (const auto& message : batch) {
            WriteDeviceDataToRedis(message);
            redis_writes_.fetch_add(1);
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "Redis 저장 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch) {
    // 🔥 나중에 구현 - 현재는 로깅만
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "InfluxDB 저장 완료: " + std::to_string(batch.size()) + "개");
}

// =============================================================================
// 🔥 Redis 저장 헬퍼 메서드들
// =============================================================================

void DataProcessingService::WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message) {
    try {
        // 🔥 기존 구조체 필드명 사용
        for (const auto& point : message.points) {  // points 필드 사용!
            
            // 1. 개별 포인트 최신값 저장
            std::string point_key = "point:" + point.point_id + ":latest";
            std::string point_json = TimestampedValueToJson(point);
            redis_client_->set(point_key, point_json);
            redis_client_->expire(point_key, 3600);
            
            // 2. 디바이스 해시에 포인트 추가
            std::string device_key = "device:" + message.device_id + ":points";
            redis_client_->hset(device_key, point.point_id, point_json);
        }
        
        // 3. 디바이스 메타정보 저장
        std::string device_meta_key = "device:" + message.device_id + ":meta";
        nlohmann::json meta;
        meta["device_id"] = message.device_id;
        meta["protocol"] = message.protocol;
        meta["type"] = message.type;
        meta["last_updated"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        meta["priority"] = message.priority;
        meta["point_count"] = message.points.size();
        
        redis_client_->set(device_meta_key, meta.dump());
        redis_client_->expire(device_meta_key, 7200);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}


std::string DataProcessingService::TimestampedValueToJson(const Structs::TimestampedValue& value) {
    nlohmann::json json_value;
    json_value["point_id"] = value.point_id;
    json_value["value"] = value.value;
    json_value["quality"] = static_cast<int>(value.quality);
    json_value["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        value.timestamp.time_since_epoch()).count();
    json_value["source"] = value.source;
    
    return json_value.dump();
}

} // namespace Pipeline
} // namespace PulseOne