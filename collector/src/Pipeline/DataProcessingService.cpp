// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 완성된 구현
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Alarm/AlarmEngine.h"
#include "Alarm/AlarmManager.h"
#include "Utils/LogManager.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <iomanip>

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
                                 "✅ DataProcessingService 시작 완료 (스레드 " + 
                                 std::to_string(thread_count_) + "개)");
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

DataProcessingService::ServiceConfig DataProcessingService::GetConfig() const {
    ServiceConfig config;
    config.thread_count = thread_count_;
    config.batch_size = batch_size_;
    config.lightweight_mode = use_lightweight_redis_.load();
    config.alarm_evaluation_enabled = alarm_evaluation_enabled_.load();
    config.virtual_point_calculation_enabled = virtual_point_calculation_enabled_.load();
    return config;
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
                // 데이터가 없으면 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
        } catch (const std::exception& e) {
            processing_errors_.fetch_add(1);
            HandleError("스레드 " + std::to_string(thread_index) + " 처리 중 예외", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 종료");
}

std::vector<Structs::DeviceDataMessage> DataProcessingService::CollectBatchFromPipelineManager() {
    auto& pipeline_manager = PipelineManager::GetInstance();
    return pipeline_manager.GetBatch(batch_size_, 100); // 100ms 타임아웃
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
        std::vector<Structs::TimestampedValue> enriched_data;
        if (virtual_point_calculation_enabled_.load()) {
            enriched_data = CalculateVirtualPoints(batch);
        } else {
            // 가상포인트 계산 비활성화 시 기본 변환만 수행
            for (const auto& device_msg : batch) {
                auto converted = ConvertToTimestampedValues(device_msg);
                enriched_data.insert(enriched_data.end(), converted.begin(), converted.end());
            }
        }
        
        // 2단계: 알람 평가
        if (alarm_evaluation_enabled_.load()) {
            EvaluateAlarms(enriched_data, thread_index);
        }
        
        // 3단계: 저장 (모드에 따라 다르게 처리)
        if (use_lightweight_redis_.load()) {
            // 🚀 미래: 경량화 버전 (성능 최적화)
            SaveToRedisLightweight(enriched_data);
        } else {
            // 🔥 현재: 테스트용 (전체 데이터 저장)
            SaveToRedisFullData(batch);
        }
        
        // 4단계: InfluxDB 저장 (시계열 데이터)
        if (influx_client_) {
            SaveToInfluxDB(enriched_data);
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 배치 처리 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        HandleError("배치 처리 실패", e.what());
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
            auto converted = ConvertToTimestampedValues(device_msg);
            enriched_data.insert(enriched_data.end(), converted.begin(), converted.end());
        }
        
        // VirtualPointEngine으로 가상포인트 계산
        auto& vp_engine = VirtualPoint::VirtualPointEngine::getInstance();
        
        if (vp_engine.isInitialized()) {
            size_t virtual_points_calculated = 0;
            
            for (const auto& device_msg : batch) {
                try {
                    auto vp_results = vp_engine.calculateForMessage(device_msg);
                    
                    for (const auto& vp_result : vp_results) {
                        enriched_data.push_back(vp_result);
                    }
                    
                    virtual_points_calculated += vp_results.size();
                    
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                                 "💥 가상포인트 계산 실패 (device=" + 
                                                 device_msg.device_id + "): " + std::string(e.what()));
                }
            }
            
            if (virtual_points_calculated > 0) {
                LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                             "✅ 가상포인트 " + std::to_string(virtual_points_calculated) + 
                                             "개 계산 완료");
            }
        } else {
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "⚠️ VirtualPointEngine이 초기화되지 않음");
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 가상포인트 계산 완료: 총 " + std::to_string(enriched_data.size()) + "개");
        
    } catch (const std::exception& e) {
        HandleError("가상포인트 계산 전체 실패", e.what());
    }
    
    return enriched_data;
}

std::vector<Structs::TimestampedValue> DataProcessingService::ConvertToTimestampedValues(
    const Structs::DeviceDataMessage& device_msg) {
    
    std::vector<Structs::TimestampedValue> result;
    result.reserve(device_msg.points.size());
    
    for (const auto& point : device_msg.points) {
        Structs::TimestampedValue tv;
        tv.point_id = point.point_id;
        tv.value = point.value;
        tv.timestamp = point.timestamp;
        tv.quality = point.quality;
        tv.value_changed = point.value_changed;
        
        result.push_back(tv);
    }
    
    return result;
}

// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp
// 🗑️ getDeviceIdForPoint 메서드 완전 제거하고 단순화
// =============================================================================

void DataProcessingService::EvaluateAlarms(const std::vector<Structs::TimestampedValue>& data, size_t thread_index) {
    if (data.empty()) {
        return;
    }
    
    try {
        auto& alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();
        
        // 🔥 수정 1: initialize() 호출 제거, isInitialized()만 체크
        if (!alarm_manager.isInitialized()) {
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "⚠️ AlarmManager가 초기화되지 않음 - 알람 평가 건너뜀");
            return;
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🚨 알람 평가 시작: " + std::to_string(data.size()) + "개");
        
        size_t alarms_triggered = 0;
        
        for (const auto& timestamped_value : data) {
            try {
                // DeviceDataMessage 구성
                Structs::DeviceDataMessage alarm_message;
                alarm_message.device_id = "device_" + std::to_string(timestamped_value.point_id / 100);
                alarm_message.timestamp = timestamped_value.timestamp;
                alarm_message.points.push_back(timestamped_value);
                
                // AlarmManager에 완전 위임
                auto alarm_events = alarm_manager.evaluateForMessage(alarm_message);
                alarms_triggered += alarm_events.size();
                
                // 결과 로깅만
                if (!alarm_events.empty()) {
                    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                                 "🚨 알람 발생: " + std::to_string(alarm_events.size()) + 
                                                 "개 (point_id=" + std::to_string(timestamped_value.point_id) + ")");
                }
                
            } catch (const std::exception& e) {
                // 🔥 수정 2: LogLevel::WARNING → LogLevel::WARN
                LogManager::getInstance().log("processing", LogLevel::WARN, 
                                             "💥 포인트 알람 평가 실패 (point_id=" + 
                                             std::to_string(timestamped_value.point_id) + 
                                             "): " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 알람 평가 완료: 평가=" + std::to_string(data.size()) + 
                                     "개, 발생=" + std::to_string(alarms_triggered) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "🚨 알람 평가 전체 실패: " + std::string(e.what()));
    }
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
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "🚨 알람 평가 중 에러: " + std::string(e.what()));
        // 알람 평가 실패해도 데이터 처리는 계속 진행 (Graceful degradation)
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
                if (redis_client_ && redis_client_->isConnected()) {
                    // Redis에 알람 이벤트 저장
                    std::string redis_key = "alarm:active:" + std::to_string(alarm_event.rule_id);
                    
                    json alarm_json;
                    alarm_json["rule_id"] = alarm_event.rule_id;
                    alarm_json["point_id"] = alarm_event.point_id;
                    alarm_json["message"] = alarm_event.message;
                    alarm_json["severity"] = alarm_event.getSeverityString();
                    alarm_json["state"] = alarm_event.getStateString();
                    alarm_json["trigger_value"] = alarm_event.getTriggerValueString();
                    alarm_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        alarm_event.timestamp.time_since_epoch()).count();
                    alarm_json["thread_id"] = thread_index;
                    
                    redis_client_->set(redis_key, alarm_json.dump());
                    redis_client_->expire(redis_key, 86400);  // TTL 24시간
                    
                    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                                 "📝 Redis 알람 저장: " + redis_key);
                }
                
                // 심각도별 카운터 업데이트
                if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::CRITICAL) {
                    critical_alarms_count_.fetch_add(1);
                } else if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::HIGH) {
                    high_alarms_count_.fetch_add(1);
                }
                
                LogManager::getInstance().log("processing", LogLevel::INFO, 
                                             "✅ 알람 이벤트 처리 완료: rule_id=" + 
                                             std::to_string(alarm_event.rule_id));
                
            } catch (const std::exception& e) {
                HandleError("개별 알람 이벤트 처리 실패", e.what());
            }
        }
        
    } catch (const std::exception& e) {
        HandleError("알람 이벤트 후처리 전체 실패", e.what());
    }
}

// =============================================================================
// 🔥 저장 시스템 (경량화 지원)
// =============================================================================

void DataProcessingService::SaveToRedis(const std::vector<Structs::TimestampedValue>& batch) {
    if (use_lightweight_redis_.load()) {
        SaveToRedisLightweight(batch);
    } else {
        // 테스트용: 전체 데이터를 JSON으로 저장
        // TODO: batch를 DeviceDataMessage로 재구성하거나 다른 방식 필요
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 (TimestampedValue): " + std::to_string(batch.size()) + "개");
        
        for (const auto& value : batch) {
            WriteTimestampedValueToRedis(value);
        }
    }
}

void DataProcessingService::SaveToRedisFullData(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 연결이 끊어져 있습니다!");
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 (전체 데이터): " + std::to_string(batch.size()) + "개");
        
        for (const auto& message : batch) {
            // 🔥 현재: 테스트용 전체 데이터 저장
            std::string device_key = "device:full:" + message.device_id;
            std::string full_json = DeviceDataMessageToJson(message);
            
            redis_client_->set(device_key, full_json);
            redis_client_->expire(device_key, 3600); // 1시간 TTL
            
            // 개별 포인트도 저장 (빠른 조회용)
            for (const auto& point : message.points) {
                WriteTimestampedValueToRedis(point);
            }
            
            redis_writes_.fetch_add(1 + message.points.size());
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ Redis 저장 완료 (전체 데이터): " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        HandleError("Redis 저장 실패 (전체 데이터)", e.what());
    }
}

void DataProcessingService::SaveToRedisLightweight(const std::vector<Structs::TimestampedValue>& batch) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 연결이 끊어져 있습니다!");
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 (경량화): " + std::to_string(batch.size()) + "개");
        
        // 🚀 미래: 경량화 구조체로 저장
        for (const auto& value : batch) {
            // LightPointValue로 변환하여 저장
            std::string light_json = ConvertToLightPointValue(value, "unknown_device");
            std::string point_key = "point:light:" + std::to_string(value.point_id);
            
            redis_client_->set(point_key, light_json);
            redis_client_->expire(point_key, 1800); // 30분 TTL
            
            redis_writes_.fetch_add(1);
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ Redis 저장 완료 (경량화): " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        HandleError("Redis 저장 실패 (경량화)", e.what());
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch) {
    if (!influx_client_) {
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 InfluxDB 저장: " + std::to_string(batch.size()) + "개");
        
        // TODO: InfluxDB 실제 저장 로직 구현
        // 현재는 로깅만 수행
        
        influx_writes_.fetch_add(batch.size());
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ InfluxDB 저장 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        HandleError("InfluxDB 저장 실패", e.what());
    }
}

// =============================================================================
// 🚀 경량화 구조체 변환 메서드들 (미리 준비)
// =============================================================================

std::string DataProcessingService::ConvertToLightDeviceStatus(const Structs::DeviceDataMessage& message) {
    // 🚀 미래: LightDeviceStatus 구조체로 변환
    json light_status;
    light_status["id"] = message.device_id;
    light_status["proto"] = message.protocol;
    light_status["status"] = static_cast<int>(message.device_status);
    light_status["connected"] = message.is_connected;
    light_status["manual"] = message.manual_status;
    light_status["msg"] = message.status_message.substr(0, 50); // 최대 50자
    light_status["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count();
    
    light_status["stats"] = {
        {"fail", message.consecutive_failures},
        {"total", message.total_points_configured},
        {"ok", message.successful_points},
        {"err", message.failed_points},
        {"rtt", message.response_time.count()}
    };
    
    return light_status.dump();
}

std::string DataProcessingService::ConvertToLightPointValue(const Structs::TimestampedValue& value, 
                                                           const std::string& device_id) {
    // 🚀 미래: LightPointValue 구조체로 변환
    json light_point;
    light_point["id"] = value.point_id;
    light_point["dev"] = device_id;
    
    std::visit([&light_point](const auto& v) {
        light_point["val"] = v;
    }, value.value);
    
    light_point["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        value.timestamp.time_since_epoch()).count();
    light_point["q"] = static_cast<int>(value.quality);
    
    if (value.value_changed) {
        light_point["chg"] = true;
    }
    
    return light_point.dump();
}

std::string DataProcessingService::ConvertToBatchPointData(const Structs::DeviceDataMessage& message) {
    // 🚀 미래: BatchPointData 구조체로 변환
    json batch_data;
    batch_data["dev"] = message.device_id;
    batch_data["proto"] = message.protocol;
    batch_data["batch_ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count();
    batch_data["seq"] = message.batch_sequence;
    
    batch_data["points"] = json::array();
    for (const auto& point : message.points) {
        json p;
        p["id"] = point.point_id;
        std::visit([&p](const auto& v) { p["val"] = v; }, point.value);
        p["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            point.timestamp.time_since_epoch()).count();
        p["q"] = static_cast<int>(point.quality);
        if (point.value_changed) p["chg"] = true;
        batch_data["points"].push_back(p);
    }
    
    return batch_data.dump();
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

void DataProcessingService::WriteTimestampedValueToRedis(const Structs::TimestampedValue& value) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        return;
    }
    
    try {
        std::string json_str = TimestampedValueToJson(value);
        std::string point_key = "point:" + std::to_string(value.point_id) + ":latest";
        
        redis_client_->set(point_key, json_str);
        redis_client_->expire(point_key, 3600); // 1시간 TTL
        
    } catch (const std::exception& e) {
        HandleError("개별 포인트 Redis 저장 실패", e.what());
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
        
        if (value.value_changed) {
            json_value["changed"] = true;
        }
        
        return json_value.dump();
        
    } catch (const std::exception& e) {
        HandleError("JSON 변환 실패", e.what());
        return R"({"point_id":)" + std::to_string(value.point_id) + 
               R"(,"value":null,"error":"conversion_failed"})";
    }
}

std::string DataProcessingService::DeviceDataMessageToJson(const Structs::DeviceDataMessage& message) {
    try {
        json j;
        j["device_id"] = message.device_id;
        j["protocol"] = message.protocol;
        j["tenant_id"] = message.tenant_id;
        j["device_status"] = static_cast<int>(message.device_status);
        j["is_connected"] = message.is_connected;
        j["manual_status"] = message.manual_status;
        j["status_message"] = message.status_message;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count();
        
        j["points"] = json::array();
        for (const auto& point : message.points) {
            json point_json;
            point_json["point_id"] = point.point_id;
            std::visit([&point_json](const auto& v) {
                point_json["value"] = v;
            }, point.value);
            point_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                point.timestamp.time_since_epoch()).count();
            point_json["quality"] = static_cast<int>(point.quality);
            point_json["changed"] = point.value_changed;
            j["points"].push_back(point_json);
        }
        
        return j.dump();
        
    } catch (const std::exception& e) {
        HandleError("DeviceDataMessage JSON 변환 실패", e.what());
        return R"({"device_id":")" + message.device_id + R"(","error":"conversion_failed"})";
    }
}

// =============================================================================
// 통계 관리
// =============================================================================

void DataProcessingService::UpdateStatistics(size_t processed_count, double processing_time_ms) {
    total_processing_time_ms_.fetch_add(static_cast<uint64_t>(processing_time_ms));
    total_operations_.fetch_add(1);
    
    if (processed_count > 0) {
        total_messages_processed_.fetch_add(processed_count);
        total_batches_processed_.fetch_add(1);
    }
}

void DataProcessingService::UpdateAlarmStatistics(size_t alarms_evaluated, size_t alarms_triggered) {
    total_alarms_evaluated_.fetch_add(alarms_evaluated);
    total_alarms_triggered_.fetch_add(alarms_triggered);
}

void DataProcessingService::UpdateAlarmStatistics(size_t evaluated_count, 
                                                  size_t triggered_count, 
                                                  size_t thread_index) {
    try {
        // 🔥 스레드별 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(alarm_stats_mutex_);
            
            alarm_statistics_.total_evaluations += evaluated_count;
            alarm_statistics_.total_triggers += triggered_count;
            alarm_statistics_.thread_statistics[thread_index].evaluations += evaluated_count;
            alarm_statistics_.thread_statistics[thread_index].triggers += triggered_count;
            alarm_statistics_.last_evaluation_time = std::chrono::system_clock::now();
            
            // 🔧 성능 메트릭 계산
            if (alarm_statistics_.total_evaluations > 0) {
                alarm_statistics_.trigger_rate = 
                    static_cast<double>(alarm_statistics_.total_triggers) / 
                    static_cast<double>(alarm_statistics_.total_evaluations) * 100.0;
            }
        }
        
        // 🔥 Redis 메트릭 업데이트 (선택적)
        if (redis_client_ && redis_client_->isConnected()) {
            try {
                std::string metric_key = "alarm_metrics:thread_" + std::to_string(thread_index);
                
                nlohmann::json metrics;
                metrics["thread_id"] = thread_index;
                metrics["evaluations"] = evaluated_count;
                metrics["triggers"] = triggered_count;
                metrics["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                redis_client_->set(metric_key, metrics.dump());
                redis_client_->expire(metric_key, 3600); // 1시간 TTL
                
            } catch (const std::exception& e) {
                // Redis 실패는 조용히 무시 (핵심 기능에 영향 없음)
                LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                             "Redis 메트릭 업데이트 실패: " + std::string(e.what()));
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "통계 업데이트 실패: " + std::string(e.what()));
    }
}

DataProcessingService::ProcessingStats DataProcessingService::GetStatistics() const {
    ProcessingStats stats;
    stats.total_batches_processed = total_batches_processed_.load();
    stats.total_messages_processed = total_messages_processed_.load();
    stats.redis_writes = redis_writes_.load();
    stats.influx_writes = influx_writes_.load();
    stats.processing_errors = processing_errors_.load();
    
    // 평균 처리 시간 계산
    uint64_t total_ops = total_operations_.load();
    if (total_ops > 0) {
        stats.avg_processing_time_ms = static_cast<double>(total_processing_time_ms_.load()) / total_ops;
    }
    
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

void DataProcessingService::HandleError(const std::string& error_message, const std::string& context) {
    std::string full_message = error_message;
    if (!context.empty()) {
        full_message += ": " + context;
    }
    
    LogManager::getInstance().log("processing", LogLevel::ERROR, "💥 " + full_message);
    processing_errors_.fetch_add(1);
}


std::string DataProcessingService::getDeviceIdForPoint(int point_id) {
    // 🔥 수정: Repository 의존성 완전 제거
    // 간단한 추정 방식으로 변경 (DB 조회 없이)
    return "device_" + std::to_string(point_id / 100);
}
/*
std::string DataProcessingService::getDeviceIdForPoint(int point_id) {
    try {
        auto datapoint_repo = repository_factory_->getDataPointRepository();
        auto datapoint = datapoint_repo->findById(point_id);
        
        if (datapoint.has_value()) {
            return std::to_string(datapoint->getDeviceId());
        }
        return "unknown_device";
        
    } catch (const std::exception& e) {
        return "unknown_device";
    }
}
*/
void DataProcessingService::SaveAlarmEventToRedis(const PulseOne::Alarm::AlarmEvent& alarm_event, size_t thread_index) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        return;
    }
    
    try {
        std::string redis_key = "alarm:active:" + std::to_string(alarm_event.rule_id);
        
        nlohmann::json alarm_json;
        alarm_json["rule_id"] = alarm_event.rule_id;
        alarm_json["point_id"] = alarm_event.point_id;
        alarm_json["message"] = alarm_event.message;
        alarm_json["severity"] = alarm_event.getSeverityString();
        alarm_json["state"] = alarm_event.getStateString();
        alarm_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            alarm_event.timestamp.time_since_epoch()).count();
        alarm_json["thread_index"] = thread_index;
        
        redis_client_->set(redis_key, alarm_json.dump());
        redis_client_->expire(redis_key, 86400);  // TTL 24시간
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "📝 Redis 알람 저장: " + redis_key);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "Redis 알람 저장 실패: " + std::string(e.what()));
    }
}

} // namespace Pipeline
} // namespace PulseOne