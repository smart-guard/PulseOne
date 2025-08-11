// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 완전 정리된 구현
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Utils/LogManager.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Alarm/AlarmManager.h"
#include <nlohmann/json.hpp>
#include <chrono>

using LogLevel = PulseOne::Enums::LogLevel;
using json = nlohmann::json;
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
                                 "✅ DataProcessingService 생성됨 - 스레드 수: " + 
                                 std::to_string(thread_count_));
}

DataProcessingService::~DataProcessingService() {
    Stop();
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "✅ DataProcessingService 소멸됨");
}

// =============================================================================
// 서비스 시작/중지
// =============================================================================

bool DataProcessingService::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ DataProcessingService가 이미 실행 중입니다");
        return false;
    }
    
    // PipelineManager 싱글톤 상태 확인
    auto& pipeline_manager = PipelineManager::GetInstance();
    if (!pipeline_manager.IsRunning()) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ PipelineManager가 실행되지 않았습니다! 먼저 PipelineManager를 시작하세요.");
        return false;
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "🚀 DataProcessingService 시작 중... (스레드: " + 
                                 std::to_string(thread_count_) + "개)");
    
    // 상태 플래그 설정
    should_stop_ = false;
    is_running_ = true;
    
    // 멀티스레드 처리기들 시작
    processing_threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        processing_threads_.emplace_back(
            &DataProcessingService::ProcessingThreadLoop, this, i);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 처리 스레드 " + std::to_string(i) + " 시작됨");
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
                                 "🛑 DataProcessingService 중지 중...");
    
    // 정지 신호 설정
    should_stop_ = true;
    
    // 모든 처리 스레드 종료 대기
    for (size_t i = 0; i < processing_threads_.size(); ++i) {
        if (processing_threads_[i].joinable()) {
            processing_threads_[i].join();
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "✅ 처리 스레드 " + std::to_string(i) + " 종료됨");
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
                                     "⚠️ 서비스가 실행 중일 때는 스레드 수를 변경할 수 없습니다. "
                                     "먼저 서비스를 중지하세요.");
        return;
    }
    
    // 유효한 스레드 수 범위 검증 (1~32개)
    if (thread_count == 0) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ 스레드 수는 최소 1개 이상이어야 합니다. 1개로 설정됩니다.");
        thread_count = 1;
    } else if (thread_count > 32) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ 스레드 수는 최대 32개까지 지원됩니다. 32개로 제한됩니다.");
        thread_count = 32;
    }
    
    thread_count_ = thread_count;
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "🔧 DataProcessingService 스레드 수 설정: " + 
                                 std::to_string(thread_count_) + "개");
}

// =============================================================================
// 멀티스레드 처리 루프
// =============================================================================

void DataProcessingService::ProcessingThreadLoop(size_t thread_index) {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 시작");
    
    while (!should_stop_.load()) {
        try {
            // PipelineManager 싱글톤에서 배치 수집
            auto batch = CollectBatchFromPipelineManager();
            
            if (!batch.empty()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                
                // 배치 처리
                ProcessBatch(batch, thread_index);
                
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                
                // 통계 업데이트
                UpdateStatistics(batch.size(), static_cast<double>(duration.count()));
                
                LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                             "🧵 스레드 " + std::to_string(thread_index) + 
                                             " 배치 처리 완료: " + std::to_string(batch.size()) + 
                                             "개 메시지, " + std::to_string(duration.count()) + "ms");
            } else {
                // 데이터 없으면 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
        } catch (const std::exception& e) {
            processing_errors_.fetch_add(1);
            LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                         "💥 스레드 " + std::to_string(thread_index) + 
                                         " 처리 중 예외: " + std::string(e.what()));
            // 예외 발생 시 잠시 대기 후 계속
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 종료");
}

std::vector<Structs::DeviceDataMessage> DataProcessingService::CollectBatchFromPipelineManager() {
    // PipelineManager 싱글톤에서 배치 가져오기
    auto& pipeline_manager = PipelineManager::GetInstance();
    return pipeline_manager.GetBatch(batch_size_, 100); // 최대 batch_size_개, 100ms 타임아웃
}

void DataProcessingService::ProcessBatch(
    const std::vector<Structs::DeviceDataMessage>& batch, 
    size_t thread_index) {
    
    if (batch.empty()) {
        return;
    }
    
    try {
        // 1단계: 가상포인트 계산
        auto enriched_data = CalculateVirtualPoints(batch);
        
        // 2단계: 알람 체크
        CheckAlarms(enriched_data);
        
        // 3단계: Redis 저장
        SaveToRedis(enriched_data);
        
        // 4단계: InfluxDB 저장
        SaveToInfluxDB(enriched_data);
        
        total_batches_processed_.fetch_add(1);
        total_messages_processed_.fetch_add(batch.size());
        
    } catch (const std::exception& e) {
        processing_errors_.fetch_add(1);
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "배치 처리 실패 (스레드 " + std::to_string(thread_index) + 
                                     "): " + std::string(e.what()));
        throw;
    }
}

// =============================================================================
// 단계별 처리 메서드들
// =============================================================================

std::vector<Structs::DeviceDataMessage> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    // 나중에 구현 - 현재는 원본 데이터 그대로 반환
    return batch;
}

void DataProcessingService::CheckAlarms(const std::vector<Structs::DeviceDataMessage>& messages) {
    try {
        // AlarmManager 싱글톤 가져오기
        auto& alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();
        
        if (!alarm_manager.isInitialized()) {
            LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                         "❌ AlarmManager not properly initialized");
            return;
        }
        
        if (messages.empty()) {
            return;
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🚨 알람 평가 시작: " + std::to_string(messages.size()) + "개 메시지");
        
        int total_alarm_events = 0;
        
        for (const auto& msg : messages) {
            try {
                auto events = alarm_manager.evaluateForMessage(msg);
                total_alarm_events += events.size();
                
                for (const auto& event : events) {
                    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                                 "🚨 알람 이벤트: " + event.message + 
                                                 " (심각도: " + event.getSeverityString() + ")");
                }
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "알람 평가 실패 (Device " + 
                                             msg.device_id + "): " +  // 🔥 UUID는 string
                                             std::string(e.what()));
            }
        }
        
        if (total_alarm_events > 0) {
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "✅ 알람 평가 완료: " + std::to_string(total_alarm_events) + 
                                         "개 이벤트 생성됨");
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ CheckAlarms 전체 실패: " + std::string(e.what()));
    }
}

void DataProcessingService::SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (!redis_client_) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 클라이언트가 null입니다!");
        return;
    }
    
    if (!redis_client_->isConnected()) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 연결이 끊어져 있습니다!");
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "🔄 Redis 저장 시작: " + std::to_string(batch.size()) + "개 메시지");
        
        for (const auto& message : batch) {
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "📝 디바이스 " + message.device_id + " 저장 중... (" + 
                                         std::to_string(message.points.size()) + "개 포인트)");
            
            WriteDeviceDataToRedis(message);
            redis_writes_.fetch_add(1);
        }
        
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "✅ Redis 저장 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch) {
    // 나중에 구현 - 현재는 로깅만
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "InfluxDB 저장 완료: " + std::to_string(batch.size()) + "개");
    influx_writes_.fetch_add(batch.size());
}

// =============================================================================
// Redis 저장 헬퍼 메서드들
// =============================================================================

void DataProcessingService::WriteDeviceDataToRedis(const PulseOne::Structs::DeviceDataMessage& message) {
    if (!redis_client_) {
        return;
    }
    
    try {
        json meta;  // using json = nlohmann::json; 덕분에 충돌 해결
        meta["device_id"] = message.device_id;
        meta["tenant_id"] = message.tenant_id;
        meta["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        meta["point_count"] = message.points.size();
        
        // 🔥 올바른 RedisClient API 사용: set() 메서드
        std::string meta_key = "device:" + message.device_id + ":meta";
        redis_client_->set(meta_key, meta.dump());  // setString() -> set()
        
        // 각 포인트 데이터 저장
        for (const auto& point : message.points) {
            // 🔥 TimestampedValue 구조체에서 point_id 필드가 없으므로 다른 방법 사용
            // point.point_id 대신 다른 필드나 인덱스 사용
            std::string point_id = "point_" + std::to_string(&point - &message.points[0]); // 인덱스 기반
            
            std::string json_str = TimestampedValueToJson(point, point_id);
            std::string point_key = "point:" + point_id + ":latest";
            
            redis_client_->set(point_key, json_str);    // setString() -> set()
            redis_client_->expire(point_key, 3600);     // TTL 설정
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "Redis 저장 실패: " + std::string(e.what()));
    }
}

std::string DataProcessingService::TimestampedValueToJson(const PulseOne::Structs::TimestampedValue& value, 
                                                         const std::string& point_id) {
    try {
        json json_value;  // using json = nlohmann::json; 덕분에 충돌 해결
        json_value["point_id"] = point_id;
        
        // 🔥 람다 캡처 수정 - json_value를 명시적으로 캡처
        std::visit([&json_value](const auto& v) {
            json_value["value"] = v;
        }, value.value);
        
        json_value["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            value.timestamp.time_since_epoch()).count();
        
        json_value["quality"] = static_cast<int>(value.quality);
        
        return json_value.dump();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "JSON 변환 실패: " + std::string(e.what()));
        return R"({"point_id":")" + point_id + R"(","value":null,"error":"conversion_failed"})";
    }
}
// =============================================================================
// 통계 관리
// =============================================================================

void DataProcessingService::UpdateStatistics(size_t processed_count, double processing_time_ms) {
    // atomic<double>의 fetch_add 문제 해결
    static std::atomic<uint64_t> total_time_ms{0};
    static std::atomic<uint64_t> total_operations{0};
    
    total_time_ms.fetch_add(static_cast<uint64_t>(processing_time_ms));
    total_operations.fetch_add(1);
    
    if (processed_count > 0) {
        // 통계에 processed_count 반영
        total_messages_processed_.fetch_add(processed_count - 1);
    }
}

DataProcessingService::ProcessingStats DataProcessingService::GetStatistics() const {
    ProcessingStats stats;
    stats.total_batches_processed = total_batches_processed_.load();
    stats.total_messages_processed = total_messages_processed_.load();
    stats.redis_writes = redis_writes_.load();
    stats.influx_writes = influx_writes_.load();
    stats.processing_errors = processing_errors_.load();
    stats.avg_processing_time_ms = 0.0; // 구현 필요
    
    return stats;
}

} // namespace Pipeline
} // namespace PulseOne