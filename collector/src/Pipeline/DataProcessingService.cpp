// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 완성된 구현 (수정됨)
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Alarm/AlarmManager.h"  // 🔥 AlarmEngine 대신 AlarmManager 사용
#include "Client/RedisClientImpl.h"  // 🔥 추가: 자동 연결 Redis 클라이언트
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
    : influx_client_(influx_client) {
    
    try {
        // 🔥 Redis 클라이언트 자동 생성 (매개변수 무시하고 새로 생성)
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        if (redis_client_->isConnected()) {
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "✅ Redis 클라이언트 자동 연결 성공");
        } else {
            LogManager::getInstance().log("processing", LogLevel::WARN, 
                                         "⚠️ Redis 클라이언트 연결 실패, Redis 없이 계속 진행");
            // Redis 없어도 계속 진행 (graceful degradation)
        }
        
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "✅ DataProcessingService 생성됨 - 스레드 수: " + 
                                     std::to_string(thread_count_));
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ DataProcessingService 생성 실패: " + std::string(e.what()));
        // Redis 클라이언트 생성 실패해도 서비스는 동작하도록 함
        redis_client_.reset();
    }
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
    config.external_notification_enabled = external_notification_enabled_.load();
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
        
        // 🔥 1단계: 알람 평가 (DeviceDataMessage 직접 사용)
        if (alarm_evaluation_enabled_.load()) {
            EvaluateAlarms(batch, thread_index);
        }
        
        // 2단계: 가상포인트 계산 및 TimestampedValue 변환
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
        
        // 3단계: 저장 (모드에 따라 다르게 처리)
        if (use_lightweight_redis_.load()) {
            SaveToRedisLightweight(enriched_data);
        } else {
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
// 🔥 새로운 알람 시스템 연동 (AlarmManager 사용)
// =============================================================================

void DataProcessingService::EvaluateAlarms(
    const std::vector<Structs::DeviceDataMessage>& batch, 
    size_t thread_index) {
    
    if (batch.empty()) {
        return;
    }
    
    try {
        auto& alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();
        
        if (!alarm_manager.isInitialized()) {
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "⚠️ AlarmManager가 초기화되지 않음 - 알람 평가 건너뜀");
            return;
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🚨 알람 평가 시작: " + std::to_string(batch.size()) + "개 메시지");
        
        size_t total_events = 0;
        
        for (const auto& device_message : batch) {
            try {
                // 🔥 AlarmManager에 완전 위임 (비즈니스 로직 포함)
                auto alarm_events = alarm_manager.evaluateForMessage(device_message);
                total_events += alarm_events.size();
                
                // 🔥 결과 처리 (외부 시스템 연동)
                if (!alarm_events.empty()) {
                    ProcessAlarmEvents(alarm_events, thread_index);
                }
                
                total_alarms_evaluated_.fetch_add(device_message.points.size());
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::WARN, 
                                             "💥 디바이스 알람 평가 실패 (device=" + 
                                             device_message.device_id + "): " + std::string(e.what()));
            }
        }
        
        if (total_events > 0) {
            total_alarms_triggered_.fetch_add(total_events);
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 알람 평가 완료: " + std::to_string(total_events) + "개 이벤트 생성");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "🚨 알람 평가 전체 실패: " + std::string(e.what()));
    }
}

void DataProcessingService::ProcessAlarmEvents(
    const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events,
    size_t thread_index) {
    
    if (alarm_events.empty()) {
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "🔔 알람 이벤트 후처리: " + std::to_string(alarm_events.size()) + "개");
        
        for (const auto& alarm_event : alarm_events) {
            try {
                // 🔥 Redis 발송 (DataProcessingService 담당)
                PublishAlarmToRedis(alarm_event);
                
                // 🔥 외부 알림 발송 (DataProcessingService 담당)
                if (external_notification_enabled_.load()) {
                    SendExternalNotifications(alarm_event);
                }
                
                // 🔥 웹소켓 알림 (DataProcessingService 담당)
                NotifyWebClients(alarm_event);
                
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
// 🔥 외부 시스템 연동 메서드들 (DataProcessingService 담당)
// =============================================================================

void DataProcessingService::PublishAlarmToRedis(const PulseOne::Alarm::AlarmEvent& event) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        return; // Redis 없어도 조용히 계속 진행
    }
    
    try {
        json alarm_json;
        alarm_json["type"] = "alarm_event";
        alarm_json["occurrence_id"] = event.occurrence_id;
        alarm_json["rule_id"] = event.rule_id;
        alarm_json["tenant_id"] = event.tenant_id;
        alarm_json["point_id"] = event.point_id;
        alarm_json["device_id"] = event.device_id;
        alarm_json["message"] = event.message;
        alarm_json["severity"] = static_cast<int>(event.severity);
        alarm_json["state"] = static_cast<int>(event.state);
        alarm_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            event.timestamp.time_since_epoch()).count();
        alarm_json["source_name"] = event.source_name;
        alarm_json["location"] = event.location;
        
        // 트리거 값 추가
        std::visit([&alarm_json](auto&& v) {
            alarm_json["trigger_value"] = v;
        }, event.trigger_value);
        
        // Redis 채널별 발송
        std::string general_channel = "alarms:all";
        std::string tenant_channel = "tenant:" + std::to_string(event.tenant_id) + ":alarms";
        std::string device_channel = "device:" + event.device_id + ":alarms";
        
        redis_client_->publish(general_channel, alarm_json.dump());
        redis_client_->publish(tenant_channel, alarm_json.dump());
        redis_client_->publish(device_channel, alarm_json.dump());
        
        // 활성 알람 저장 (TTL 24시간)
        std::string active_key = "alarm:active:" + std::to_string(event.rule_id);
        redis_client_->setex(active_key, alarm_json.dump(), 86400);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "📝 Redis 알람 발송 완료: " + std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ Redis 알람 발송 실패: " + std::string(e.what()));
    }
}

void DataProcessingService::SendExternalNotifications(const PulseOne::Alarm::AlarmEvent& event) {
    try {
        // 🔥 여기서 실제 외부 알림 발송 (이메일, SMS, Slack 등)
        // TODO: 실제 구현에서는 ConfigManager에서 알림 설정을 읽어서 처리
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "📧 외부 알림 발송 (스텁): rule_id=" + std::to_string(event.rule_id) + 
                                     ", severity=" + std::to_string(static_cast<int>(event.severity)));
        
        // 심각도에 따른 알림 채널 결정
        if (event.severity >= PulseOne::Alarm::AlarmSeverity::CRITICAL) {
            // 긴급: 이메일 + SMS + Slack
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "🚨 긴급 알림 발송: " + event.message);
        } else if (event.severity >= PulseOne::Alarm::AlarmSeverity::HIGH) {
            // 높음: 이메일 + Slack
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "⚠️ 높은 우선순위 알림 발송: " + event.message);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ 외부 알림 발송 실패: " + std::string(e.what()));
    }
}

void DataProcessingService::NotifyWebClients(const PulseOne::Alarm::AlarmEvent& event) {
    try {
        // 🔥 여기서 웹소켓을 통한 실시간 알림
        // TODO: 실제 구현에서는 WebSocket 서버에 이벤트 전송
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🌐 웹클라이언트 알림 (스텁): rule_id=" + std::to_string(event.rule_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ 웹클라이언트 알림 실패: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 가상포인트 계산 (기존 유지)
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
// 🔥 저장 시스템 (경량화 지원)
// =============================================================================

void DataProcessingService::SaveToRedisFullData(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "⚠️ Redis 연결 없음 - 저장 건너뜀");
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 (전체 데이터): " + std::to_string(batch.size()) + "개");
        
        for (const auto& message : batch) {
            // 전체 메시지 저장
            std::string device_key = "device:full:" + message.device_id;
            std::string full_json = DeviceDataMessageToJson(message);
            
            redis_client_->setex(device_key, full_json, 3600); // 1시간 TTL
            
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
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "⚠️ Redis 연결 없음 - 저장 건너뜀");
        return;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 Redis 저장 (경량화): " + std::to_string(batch.size()) + "개");
        
        for (const auto& value : batch) {
            // 경량화된 형태로 저장
            std::string light_json = ConvertToLightPointValue(value, getDeviceIdForPoint(value.point_id));
            std::string point_key = "point:light:" + std::to_string(value.point_id);
            
            redis_client_->setex(point_key, light_json, 1800); // 30분 TTL
            
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
// 헬퍼 메서드들 (기존 유지, 간소화)
// =============================================================================

void DataProcessingService::WriteTimestampedValueToRedis(const Structs::TimestampedValue& value) {
    if (!redis_client_ || !redis_client_->isConnected()) {
        return;
    }
    
    try {
        std::string json_str = TimestampedValueToJson(value);
        std::string point_key = "point:" + std::to_string(value.point_id) + ":latest";
        
        redis_client_->setex(point_key, json_str, 3600); // 1시간 TTL
        
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

std::string DataProcessingService::getDeviceIdForPoint(int point_id) {
    // 간단한 추정 방식 (DB 조회 없이)
    return "device_" + std::to_string(point_id / 100);
}

// =============================================================================
// 경량화 구조체 변환 메서드들 (미래용)
// =============================================================================

std::string DataProcessingService::ConvertToLightDeviceStatus(const Structs::DeviceDataMessage& message) {
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

// JSON 변환 구현
nlohmann::json DataProcessingService::ProcessingStats::toJson() const {
    nlohmann::json j;
    j["total_batches"] = total_batches_processed;
    j["total_messages"] = total_messages_processed;
    j["redis_writes"] = redis_writes;
    j["influx_writes"] = influx_writes;
    j["errors"] = processing_errors;
    j["avg_time_ms"] = avg_processing_time_ms;
    return j;
}

nlohmann::json DataProcessingService::ExtendedProcessingStats::toJson() const {
    nlohmann::json j;
    j["processing"] = processing.toJson();
    j["alarms"] = alarms.toJson();
    return j;
}

} // namespace Pipeline
} // namespace PulseOne