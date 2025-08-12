// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 컴파일 에러 수정
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Alarm/AlarmEngine.h"
#include "Alarm/AlarmManager.h"  // 🔥 추가
#include "Utils/LogManager.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
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
    
    auto& pipeline_manager = PipelineManager::GetInstance();
    if (!pipeline_manager.IsRunning()) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ PipelineManager가 실행되지 않았습니다!");
        return false;
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "🚀 DataProcessingService 시작 중...");
    
    should_stop_ = false;
    is_running_ = true;
    
    processing_threads_.reserve(thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        processing_threads_.emplace_back(
            &DataProcessingService::ProcessingThreadLoop, this, i);
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "✅ DataProcessingService 시작 완료");
    return true;
}

void DataProcessingService::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "🛑 DataProcessingService 중지 중...");
    
    should_stop_ = true;
    
    for (auto& thread : processing_threads_) {
        if (thread.joinable()) {
            thread.join();
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
                                     "⚠️ 서비스 실행 중에는 스레드 수를 변경할 수 없습니다");
        return;
    }
    
    if (thread_count == 0) {
        thread_count = 1;
    } else if (thread_count > 32) {
        thread_count = 32;
    }
    
    thread_count_ = thread_count;
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "🔧 스레드 수 설정: " + std::to_string(thread_count_));
}

// =============================================================================
// 멀티스레드 처리 루프
// =============================================================================

void DataProcessingService::ProcessingThreadLoop(size_t thread_index) {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 시작");
    
    while (!should_stop_.load()) {
        try {
            auto batch = CollectBatchFromPipelineManager();
            
            if (!batch.empty()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                
                ProcessBatch(batch, thread_index);
                
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                
                UpdateStatistics(batch.size(), static_cast<double>(duration.count()));
                
                LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                             "🧵 스레드 " + std::to_string(thread_index) + 
                                             " 배치 처리 완료: " + std::to_string(batch.size()) + "개");
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
        } catch (const std::exception& e) {
            processing_errors_.fetch_add(1);
            LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                         "💥 스레드 " + std::to_string(thread_index) + 
                                         " 처리 중 예외: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 종료");
}

std::vector<Structs::DeviceDataMessage> DataProcessingService::CollectBatchFromPipelineManager() {
    auto& pipeline_manager = PipelineManager::GetInstance();
    return pipeline_manager.GetBatch(batch_size_, 100);
}

void DataProcessingService::ProcessBatch(
    const std::vector<Structs::DeviceDataMessage>& batch, 
    size_t thread_index) {
    
    if (batch.empty()) {
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 배치 처리 시작: " + std::to_string(batch.size()) + "개");
        
        // 1단계: 가상포인트 계산 및 TimestampedValue 변환
        auto enriched_data = CalculateVirtualPoints(batch);
        
        // 2단계: 알람 평가
        EvaluateAlarms(enriched_data, thread_index);
        
        // 3단계: Redis 저장
        SaveToRedis(enriched_data);
        
        // 4단계: InfluxDB 저장
        SaveToInfluxDB(enriched_data);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 배치 처리 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 배치 처리 실패: " + std::string(e.what()));
        throw;
    }
}

// =============================================================================
// 🔥 가상포인트 계산 (단일 메서드로 정리)
// =============================================================================

std::vector<Structs::TimestampedValue> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    
    std::vector<Structs::TimestampedValue> enriched_data;
    
    if (batch.empty()) {
        return enriched_data;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🧮 가상포인트 계산 시작: " + std::to_string(batch.size()) + "개");
        
        // 원본 데이터를 TimestampedValue로 변환
        for (const auto& device_msg : batch) {
            for (const auto& point : device_msg.points) {
                Structs::TimestampedValue tv;
                tv.point_id = point.point_id;  // 🔥 int는 그대로 사용
                tv.value = point.value;
                tv.timestamp = point.timestamp;
                tv.quality = point.quality;
                
                enriched_data.push_back(tv);
            }
        }
        
        // VirtualPointEngine으로 가상포인트 계산
        auto& vp_engine = VirtualPoint::VirtualPointEngine::getInstance();
        
        if (vp_engine.isInitialized()) {
            for (const auto& device_msg : batch) {
                try {
                    auto vp_results = vp_engine.calculateForMessage(device_msg);
                    
                    for (const auto& vp_result : vp_results) {
                        enriched_data.push_back(vp_result);
                    }
                    
                    if (!vp_results.empty()) {
                        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                                     "✅ 가상포인트 " + std::to_string(vp_results.size()) + 
                                                     "개 계산 완료");
                    }
                    
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                                 "💥 가상포인트 계산 실패: " + std::string(e.what()));
                }
            }
        } else {
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "⚠️ VirtualPointEngine이 초기화되지 않음");
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 가상포인트 계산 완료: " + std::to_string(enriched_data.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 가상포인트 계산 전체 실패: " + std::string(e.what()));
    }
    
    return enriched_data;
}

// =============================================================================
// 🔥 알람 평가 (올바른 필드 사용)
// =============================================================================

void DataProcessingService::EvaluateAlarms(
    const std::vector<Structs::TimestampedValue>& data, 
    size_t thread_index) {
    
    if (data.empty()) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t alarms_evaluated = 0;
    size_t alarms_triggered = 0;
    
    try {
        auto& alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();
        
        if (!alarm_manager.isInitialized()) {
            LogManager::getInstance().log("processing", LogLevel::WARN, 
                                         "⚠️ AlarmManager가 초기화되지 않음");
            return;
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🚨 알람 평가 시작: " + std::to_string(data.size()) + "개");
        
        for (const auto& timestamped_value : data) {
            try {
                int tenant_id = 1;
                
                // 🔥 point_id는 이미 int이므로 직접 사용
                int point_id = timestamped_value.point_id;
                
                PulseOne::BasicTypes::DataVariant alarm_value;
                std::visit([&alarm_value](const auto& val) {
                    alarm_value = val;
                }, timestamped_value.value);
                
                // 🔥 AlarmManager의 올바른 API 사용
                // DeviceDataMessage 생성하여 evaluateForMessage 호출
                Structs::DeviceDataMessage alarm_msg;
                alarm_msg.tenant_id = tenant_id;
                alarm_msg.device_id = "device_" + std::to_string(point_id); // 임시 device_id
                
                Structs::TimestampedValue alarm_point;
                alarm_point.point_id = point_id;
                alarm_point.value = timestamped_value.value;
                alarm_point.timestamp = timestamped_value.timestamp;
                alarm_point.quality = timestamped_value.quality;
                
                alarm_msg.points.push_back(alarm_point);
                
                auto alarm_events = alarm_manager.evaluateForMessage(alarm_msg);
                
                alarms_evaluated++;
                
                if (!alarm_events.empty()) {
                    alarms_triggered += alarm_events.size();
                    
                    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                                 "🚨 알람 발생! point_id=" + std::to_string(timestamped_value.point_id) + 
                                                 ", 이벤트 수=" + std::to_string(alarm_events.size()));
                    
                    ProcessAlarmEvents(alarm_events, timestamped_value, thread_index);
                }
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "💥 알람 평가 실패 (point_id=" + std::to_string(timestamped_value.point_id) + 
                                             "): " + std::string(e.what()));
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        total_alarms_evaluated_.fetch_add(alarms_evaluated);
        total_alarms_triggered_.fetch_add(alarms_triggered);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 알람 평가 완료: 평가=" + std::to_string(alarms_evaluated) + 
                                     "개, 발생=" + std::to_string(alarms_triggered) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 알람 평가 전체 실패: " + std::string(e.what()));
    }
}

void DataProcessingService::ProcessAlarmEvents(
    const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events,
    const Structs::TimestampedValue& source_data,
    size_t thread_index) {
    
    if (alarm_events.empty()) {
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "🔔 알람 이벤트 후처리: " + std::to_string(alarm_events.size()) + "개");
        
        for (const auto& alarm_event : alarm_events) {
            try {
                if (redis_client_) {
                    std::string redis_key = "alarm:active:" + std::to_string(alarm_event.rule_id);
                    
                    nlohmann::json alarm_json;
                    alarm_json["rule_id"] = alarm_event.rule_id;
                    alarm_json["point_id"] = alarm_event.point_id;
                    alarm_json["message"] = alarm_event.message;
                    alarm_json["severity"] = alarm_event.getSeverityString();
                    alarm_json["state"] = alarm_event.getStateString();
                    alarm_json["trigger_value"] = alarm_event.getTriggerValueString();
                    alarm_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        alarm_event.timestamp.time_since_epoch()).count();
                    
                    // 🔥 올바른 RedisClient API 사용
                    redis_client_->set(redis_key, alarm_json.dump());
                    redis_client_->expire(redis_key, 86400);  // TTL 24시간
                    
                    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                                 "📝 Redis 알람 저장: " + redis_key);
                }
                
                if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::CRITICAL) {
                    critical_alarms_count_.fetch_add(1);
                } else if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::HIGH) {
                    high_alarms_count_.fetch_add(1);
                }
                
                LogManager::getInstance().log("processing", LogLevel::INFO, 
                                             "✅ 알람 이벤트 처리 완료: rule_id=" + 
                                             std::to_string(alarm_event.rule_id));
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "💥 개별 알람 이벤트 처리 실패: " + std::string(e.what()));
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 알람 이벤트 후처리 전체 실패: " + std::string(e.what()));
    }
}

// =============================================================================
// Redis/InfluxDB 저장 (올바른 타입 사용)
// =============================================================================

void DataProcessingService::SaveToRedis(const std::vector<Structs::TimestampedValue>& batch) {
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
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 시작: " + std::to_string(batch.size()) + "개");
        
        for (const auto& value : batch) {
            WriteTimestampedValueToRedis(value);
            redis_writes_.fetch_add(1);
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ Redis 저장 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch) {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "InfluxDB 저장 완료: " + std::to_string(batch.size()) + "개");
    influx_writes_.fetch_add(batch.size());
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

void DataProcessingService::WriteTimestampedValueToRedis(const Structs::TimestampedValue& value) {
    if (!redis_client_) {
        return;
    }
    
    try {
        std::string json_str = TimestampedValueToJson(value);
        std::string point_key = "point:" + std::to_string(value.point_id) + ":latest";
        
        redis_client_->set(point_key, json_str);
        redis_client_->expire(point_key, 3600);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "Redis 저장 실패: " + std::string(e.what()));
    }
}

std::string DataProcessingService::TimestampedValueToJson(const Structs::TimestampedValue& value) {
    try {
        json json_value;
        json_value["point_id"] = value.point_id;
        
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
        return R"({"point_id":)" + std::to_string(value.point_id) + R"(,"value":null,"error":"conversion_failed"})";
    }
}

// =============================================================================
// 통계 관리
// =============================================================================

void DataProcessingService::UpdateStatistics(size_t processed_count, double processing_time_ms) {
    static std::atomic<uint64_t> total_time_ms{0};
    static std::atomic<uint64_t> total_operations{0};
    
    total_time_ms.fetch_add(static_cast<uint64_t>(processing_time_ms));
    total_operations.fetch_add(1);
    
    if (processed_count > 0) {
        total_messages_processed_.fetch_add(processed_count);
        total_batches_processed_.fetch_add(1);
    }
}

DataProcessingService::ProcessingStats DataProcessingService::GetStatistics() const {
    ProcessingStats stats;
    stats.total_batches_processed = total_batches_processed_.load();
    stats.total_messages_processed = total_messages_processed_.load();
    stats.redis_writes = redis_writes_.load();
    stats.influx_writes = influx_writes_.load();
    stats.processing_errors = processing_errors_.load();
    stats.avg_processing_time_ms = 0.0;
    
    return stats;
}

PulseOne::Alarm::AlarmProcessingStats DataProcessingService::GetAlarmStatistics() const {
    PulseOne::Alarm::AlarmProcessingStats stats;
    stats.total_evaluated = total_alarms_evaluated_.load();
    stats.total_triggered = total_alarms_triggered_.load();
    stats.critical_count = critical_alarms_count_.load();
    stats.high_count = high_alarms_count_.load();
    return stats;
}

DataProcessingService::ExtendedProcessingStats DataProcessingService::GetExtendedStatistics() const {
    ExtendedProcessingStats stats;
    stats.processing = GetStatistics();
    stats.alarms = GetAlarmStatistics();
    return stats;
}

void DataProcessingService::evaluateAlarmsForMessage(const Structs::DeviceDataMessage& message) {
    try {
        auto& alarm_engine = Alarm::AlarmEngine::getInstance();
        
        if (!alarm_engine.isInitialized()) {
            // 알람 엔진 초기화 안됨 - 조용히 스킵
            return;
        }
        
        // 🔥 메시지 전체를 AlarmEngine에 전달
        alarm_engine.evaluateForMessage(message);
        
    } catch (const std::exception& e) {
        // 🔥 수정: LogMessage -> LogManager::getInstance().log
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "🚨 알람 평가 중 에러: " + std::string(e.what()));
        // 알람 평가 실패해도 데이터 처리는 계속 진행 (Graceful degradation)
    }
}

} // namespace Pipeline
} // namespace PulseOne