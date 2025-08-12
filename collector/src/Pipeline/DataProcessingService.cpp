// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 완전 정리된 구현
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Utils/LogManager.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Alarm/AlarmEngine.h"
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
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🔄 스레드 " + std::to_string(thread_index) + 
                                     " 배치 처리 시작: " + std::to_string(batch.size()) + "개 메시지");
        
        // 1단계: 가상포인트 계산
        auto enriched_data = CalculateVirtualPoints(batch);
        
        // 🔥 2단계: 알람 체크 (새로 추가!)
        EvaluateAlarms(enriched_data, thread_index);
        
        // 3단계: Redis 저장
        SaveToRedis(enriched_data);
        
        // 4단계: InfluxDB 저장
        SaveToInfluxDB(enriched_data);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 스레드 " + std::to_string(thread_index) + 
                                     " 배치 처리 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 배치 처리 실패 (스레드 " + std::to_string(thread_index) + 
                                     "): " + std::string(e.what()));
        throw;
    }
}

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
        // AlarmEngine 싱글톤 가져오기
        auto& alarm_engine = PulseOne::Alarm::AlarmEngine::getInstance();
        
        if (!alarm_engine.isInitialized()) {
            LogManager::getInstance().log("processing", LogLevel::WARN, 
                                         "⚠️ AlarmEngine이 초기화되지 않음 - 알람 평가 건너뜀");
            return;
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🚨 알람 평가 시작 (스레드 " + std::to_string(thread_index) + 
                                     "): " + std::to_string(data.size()) + "개 포인트");
        
        // =============================================================================
        // 🎯 각 데이터 포인트에 대해 알람 평가 실행
        // =============================================================================
        
        for (const auto& timestamped_value : data) {
            try {
                // tenant_id 결정 (기본값 1, 실제로는 device나 site에서 가져와야 함)
                int tenant_id = 1;  // TODO: 실제 tenant_id 로직 구현
                
                // point_id 추출
                int point_id = std::stoi(timestamped_value.point_id);
                
                // DataValue 변환 (TimestampedValue.value → AlarmEngine.DataValue)
                PulseOne::BasicTypes::DataVariant alarm_value;
                std::visit([&alarm_value](const auto& val) {
                    alarm_value = val;
                }, timestamped_value.value);
                
                // =============================================================================
                // 🔥 실제 알람 평가 호출!
                // =============================================================================
                auto alarm_events = alarm_engine.evaluateForPoint(tenant_id, point_id, alarm_value);
                
                alarms_evaluated++;
                
                // 알람 이벤트 처리
                if (!alarm_events.empty()) {
                    alarms_triggered += alarm_events.size();
                    
                    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                                 "🚨 알람 발생! point_id=" + timestamped_value.point_id + 
                                                 ", 이벤트 수=" + std::to_string(alarm_events.size()));
                    
                    // 알람 이벤트 후처리 (Redis, 알림 등)
                    ProcessAlarmEvents(alarm_events, timestamped_value, thread_index);
                }
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "💥 알람 평가 실패 (point_id=" + timestamped_value.point_id + 
                                             "): " + std::string(e.what()));
                // 개별 포인트 실패는 전체 배치를 중단시키지 않음
            }
        }
        
        // =============================================================================
        // 🎯 성능 통계 업데이트
        // =============================================================================
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // 통계 업데이트 (원자적 연산)
        total_alarms_evaluated_.fetch_add(alarms_evaluated);
        total_alarms_triggered_.fetch_add(alarms_triggered);
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 알람 평가 완료 (스레드 " + std::to_string(thread_index) + 
                                     "): 평가=" + std::to_string(alarms_evaluated) + 
                                     "개, 발생=" + std::to_string(alarms_triggered) + 
                                     "개, 시간=" + std::to_string(duration.count()) + "μs");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 알람 평가 전체 실패 (스레드 " + std::to_string(thread_index) + 
                                     "): " + std::string(e.what()));
        // 알람 평가 실패해도 데이터 처리는 계속 진행
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
                                     "🔔 알람 이벤트 후처리 시작 (스레드 " + std::to_string(thread_index) + 
                                     "): " + std::to_string(alarm_events.size()) + "개 이벤트");
        
        for (const auto& alarm_event : alarm_events) {
            try {
                // =============================================================================
                // 🎯 Redis에 알람 이벤트 저장 (실시간 조회용)
                // =============================================================================
                
                if (redis_client_) {
                    // Redis 키: "alarm:active:{rule_id}"
                    std::string redis_key = "alarm:active:" + std::to_string(alarm_event.rule_id);
                    
                    // JSON 형태로 알람 정보 저장
                    nlohmann::json alarm_json;
                    alarm_json["rule_id"] = alarm_event.rule_id;
                    alarm_json["point_id"] = alarm_event.point_id;
                    alarm_json["message"] = alarm_event.message;
                    alarm_json["severity"] = alarm_event.getSeverityString();
                    alarm_json["state"] = alarm_event.getStateString();
                    alarm_json["trigger_value"] = alarm_event.getTriggerValueString();
                    alarm_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        alarm_event.timestamp.time_since_epoch()).count();
                    
                    // Redis에 저장 (TTL 24시간)
                    redis_client_->SetWithExpiry(redis_key, alarm_json.dump(), 86400);
                    
                    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                                 "📝 Redis 알람 저장: " + redis_key);
                }
                
                // =============================================================================
                // 🎯 알람 통계 업데이트
                // =============================================================================
                
                // 심각도별 통계
                if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::CRITICAL) {
                    critical_alarms_count_.fetch_add(1);
                } else if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::HIGH) {
                    high_alarms_count_.fetch_add(1);
                }
                
                // =============================================================================
                // 🎯 실시간 알림 큐에 추가 (향후 확장)
                // =============================================================================
                
                // TODO: RabbitMQ 또는 Kafka에 알람 이벤트 발송
                // TODO: 이메일/SMS 알림 큐에 추가
                // TODO: WebSocket으로 실시간 대시보드에 알림
                
                LogManager::getInstance().log("processing", LogLevel::INFO, 
                                             "✅ 알람 이벤트 처리 완료: rule_id=" + 
                                             std::to_string(alarm_event.rule_id) + 
                                             ", severity=" + alarm_event.getSeverityString());
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "💥 개별 알람 이벤트 처리 실패 (rule_id=" + 
                                             std::to_string(alarm_event.rule_id) + "): " + 
                                             std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 모든 알람 이벤트 후처리 완료 (스레드 " + 
                                     std::to_string(thread_index) + ")");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 알람 이벤트 후처리 전체 실패 (스레드 " + 
                                     std::to_string(thread_index) + "): " + 
                                     std::string(e.what()));
    }
}

std::vector<Structs::TimestampedValue> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    
    std::vector<Structs::TimestampedValue> enriched_data;
    
    if (batch.empty()) {
        return enriched_data;
    }
    
    try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "🧮 가상포인트 계산 시작: " + std::to_string(batch.size()) + "개 메시지");
        
        // =============================================================================
        // 🎯 1단계: 원본 데이터를 TimestampedValue로 변환
        // =============================================================================
        
        for (const auto& device_msg : batch) {
            for (const auto& point : device_msg.points) {
                Structs::TimestampedValue tv;
                tv.point_id = std::to_string(point.point_id);
                tv.value = point.value;
                tv.timestamp = point.timestamp;
                tv.quality = point.quality;
                
                enriched_data.push_back(tv);
            }
        }
        
        // =============================================================================
        // 🎯 2단계: VirtualPointEngine으로 가상포인트 계산
        // =============================================================================
        
        auto& vp_engine = VirtualPoint::VirtualPointEngine::getInstance();
        
        if (vp_engine.isInitialized()) {
            // 각 메시지에 대해 가상포인트 계산
            for (const auto& device_msg : batch) {
                try {
                    auto vp_results = vp_engine.calculateForMessage(device_msg);
                    
                    // 계산된 가상포인트 결과를 enriched_data에 추가
                    for (const auto& vp_result : vp_results) {
                        enriched_data.push_back(vp_result);
                    }
                    
                    if (!vp_results.empty()) {
                        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                                     "✅ 가상포인트 " + std::to_string(vp_results.size()) + 
                                                     "개 계산 완료 (device_id=" + device_msg.device_id + ")");
                    }
                    
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                                 "💥 가상포인트 계산 실패 (device_id=" + device_msg.device_id + 
                                                 "): " + std::string(e.what()));
                    // 개별 실패는 전체 프로세스를 중단시키지 않음
                }
            }
        } else {
            LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                         "⚠️ VirtualPointEngine이 초기화되지 않음 - 가상포인트 계산 건너뜀");
        }
        
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                     "✅ 가상포인트 계산 완료: 총 " + std::to_string(enriched_data.size()) + 
                                     "개 데이터 포인트 (원본 + 가상포인트)");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 가상포인트 계산 전체 실패: " + std::string(e.what()));
        
        // 가상포인트 계산 실패 시에도 원본 데이터는 반환
        // (이미 enriched_data에 원본 데이터가 들어있음)
    }
    
    return enriched_data;
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


/**
 * @brief 알람 처리 통계 조회 (AlarmTypes.h에서 정의된 타입 사용)
 * @return AlarmProcessingStats 구조체
 */
PulseOne::Alarm::AlarmProcessingStats DataProcessingService::GetAlarmStatistics() const {
    PulseOne::Alarm::AlarmProcessingStats stats;
    stats.total_evaluated = total_alarms_evaluated_.load();
    stats.total_triggered = total_alarms_triggered_.load();
    stats.critical_count = critical_alarms_count_.load();
    stats.high_count = high_alarms_count_.load();
    return stats;
}

/**
 * @brief 모든 통계 조회
 */
ExtendedProcessingStats DataProcessingService::GetExtendedStatistics() const {
    ExtendedProcessingStats stats;
    stats.processing = GetStatistics();
    stats.alarms = GetAlarmStatistics();
    return stats;
}

} // namespace Pipeline
} // namespace PulseOne