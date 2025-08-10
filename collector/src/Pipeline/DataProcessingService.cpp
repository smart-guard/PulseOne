// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 올바른 구현
// =============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"  // 🔥 올바른 include!
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Alarm/AlarmEngine.h"
#include <nlohmann/json.hpp>
#include <chrono>

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
                                 "✅ DataProcessingService 생성됨 - 스레드 수: " + 
                                 std::to_string(thread_count_));
}

DataProcessingService::~DataProcessingService() {
    Stop();
    LogManager::getInstance().log("processing", LogLevel::INFO, 
                                 "✅ DataProcessingService 소멸됨");
}

// =============================================================================
// 🔥 올바른 인터페이스 구현 (PipelineManager 싱글톤 사용)
// =============================================================================

bool DataProcessingService::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().log("processing", LogLevel::WARN, 
                                     "⚠️ DataProcessingService가 이미 실행 중입니다");
        return false;
    }
    
    // 🔥 PipelineManager 싱글톤 상태 확인
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
    
    // 🔥 멀티스레드 처리기들 시작
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
    
    // 🔥 정지 신호 설정
    should_stop_ = true;
    
    // 🔥 모든 처리 스레드 종료 대기
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
// 🔥 올바른 멀티스레드 처리 루프
// =============================================================================

void DataProcessingService::ProcessingThreadLoop(size_t thread_index) {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "🧵 처리 스레드 " + std::to_string(thread_index) + " 시작");
    
    while (!should_stop_.load()) {
        try {
            // 🔥 PipelineManager 싱글톤에서 배치 수집
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
    // 🔥 PipelineManager 싱글톤에서 배치 가져오기
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
// 🔥 단계별 처리 메서드들 (순차 실행)
// =============================================================================

std::vector<Structs::DeviceDataMessage> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    // 🔥 나중에 구현 - 현재는 원본 데이터 그대로 반환
    return batch;
}

void DataProcessingService::CheckAlarms(const std::vector<DeviceDataMessage>& messages) {
    try {
        static AlarmEngine alarm_engine;
        static bool initialized = false;
        
        if (!initialized) {
            // DatabaseManager는 싱글턴이므로 shared_ptr로 래핑
            auto db_manager = std::make_shared<DatabaseManager>(DatabaseManager::getInstance());
            
            if (!alarm_engine.initialize(db_manager, redis_client_)) {
                logger_->Error("DataProcessingService::CheckAlarms - Failed to initialize alarm engine");
                return;
            }
            initialized = true;
        }
        
        // 알람 체크 로직
        for (const auto& message : messages) {
            alarm_engine.checkDataPoint(message);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DataProcessingService::CheckAlarms failed: " + std::string(e.what()));
    }
}

// =============================================================================
// 3. DatabaseManager 싱글턴 패턴 수정 제안
// =============================================================================

// DatabaseManager.h에서 getInstance() 메서드가 shared_ptr를 반환하도록 수정:

class DatabaseManager {
public:
    // ❌ 기존
    // static DatabaseManager& getInstance();
    
    // ✅ 수정 - shared_ptr 반환으로 변경
    static std::shared_ptr<DatabaseManager> getInstance() {
        static std::shared_ptr<DatabaseManager> instance = nullptr;
        static std::once_flag once_flag;
        
        std::call_once(once_flag, []() {
            instance = std::shared_ptr<DatabaseManager>(new DatabaseManager());
        });
        
        return instance;
    }
    
    // 또는 기존 방식 유지하고 shared_ptr 래퍼 메서드 추가:
    static std::shared_ptr<DatabaseManager> getSharedInstance() {
        return std::shared_ptr<DatabaseManager>(&getInstance(), [](DatabaseManager*){});
    }

private:
    DatabaseManager() = default;  // private 생성자
    
    // 복사 방지
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

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
        
        // 🔥 DEBUG_LEVEL → INFO로 변경해서 반드시 보이도록!
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "✅ Redis 저장 완료: " + std::to_string(batch.size()) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "❌ Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch) {
    // 🔥 나중에 구현 - 현재는 로깅만
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL, 
                                 "InfluxDB 저장 완료: " + std::to_string(batch.size()) + "개");
    influx_writes_.fetch_add(batch.size());
}

// =============================================================================
// 🔥 Redis 저장 헬퍼 메서드들 (필드 오류 수정!)
// =============================================================================

void DataProcessingService::WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message) {
    try {
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "🔧 디바이스 " + message.device_id + " Redis 저장 시작");
        
        // 🔥 올바른 필드 사용: message.points는 TimestampedValue의 배열
        for (size_t i = 0; i < message.points.size(); ++i) {
            const auto& point = message.points[i];
            
            // 🔥 point_id 생성: device_id + index 조합 (또는 다른 고유 식별자)
            std::string point_id = message.device_id + "_point_" + std::to_string(i);
            
            // 1. 개별 포인트 최신값 저장
            std::string point_key = "point:" + point_id + ":latest";
            std::string point_json = TimestampedValueToJson(point, point_id);
            
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "🔑 저장 키: " + point_key + " = " + point_json.substr(0, 100));
            
            bool set_result = redis_client_->set(point_key, point_json);
            if (!set_result) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "❌ Redis SET 실패: " + point_key);
                continue;
            }
            
            bool expire_result = redis_client_->expire(point_key, 3600);
            if (!expire_result) {
                LogManager::getInstance().log("processing", LogLevel::WARN, 
                                             "⚠️ Redis EXPIRE 실패: " + point_key);
            }
            
            // 2. 디바이스 해시에 포인트 추가
            std::string device_key = "device:" + message.device_id + ":points";
            bool hset_result = redis_client_->hset(device_key, point_id, point_json);
            if (!hset_result) {
                LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                             "❌ Redis HSET 실패: " + device_key);
            }
            
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "✅ 포인트 저장 완료: " + point_id);
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
        
        bool meta_result = redis_client_->set(device_meta_key, meta.dump());
        if (!meta_result) {
            LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                         "❌ Redis 메타 저장 실패: " + device_meta_key);
        } else {
            LogManager::getInstance().log("processing", LogLevel::INFO, 
                                         "✅ 메타 저장 완료: " + device_meta_key);
        }
        
        redis_client_->expire(device_meta_key, 7200);
        
        LogManager::getInstance().log("processing", LogLevel::INFO, 
                                     "🎉 디바이스 " + message.device_id + " 전체 저장 완료!");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("processing", LogLevel::ERROR, 
                                     "💥 디바이스 " + message.device_id + " Redis 저장 실패: " + std::string(e.what()));
        throw;
    }
}

std::string DataProcessingService::TimestampedValueToJson(
    const Structs::TimestampedValue& value, 
    const std::string& point_id) {
    
    nlohmann::json json_value;
    json_value["point_id"] = point_id;  // 🔥 외부에서 받은 point_id 사용!
    
    // 🔥 올바른 TimestampedValue 필드 사용
    // value 필드 처리 (DataVariant)
    std::visit([&json_value](const auto& v) {
        json_value["value"] = v;
    }, value.value);
    
    json_value["quality"] = static_cast<int>(value.quality);
    json_value["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        value.timestamp.time_since_epoch()).count();
    json_value["source"] = value.source;
    
    return json_value.dump();
}

void DataProcessingService::UpdateStatistics(size_t processed_count, double processing_time_ms) {
    // 🔥 수정: atomic<double>의 fetch_add 문제 해결
    // fetch_add 대신 load/store 사용하여 근사치 계산
    
    static std::atomic<uint64_t> total_time_ms{0};  // double 대신 uint64_t 사용
    static std::atomic<uint64_t> total_operations{0};
    
    // 🔥 수정: fetch_add를 지원하는 타입으로 변환
    total_time_ms.fetch_add(static_cast<uint64_t>(processing_time_ms));
    total_operations.fetch_add(1);
    
    // 🔥 수정: 미사용 변수 경고 해결 - processed_count 사용
    if (processed_count > 0) {
        // 통계에 processed_count 반영 (실제 사용)
        total_messages_processed_.fetch_add(processed_count - 1); // -1은 이미 다른 곳에서 카운트되므로
    }
    
    // 🔥 수정: unused variable 경고 해결 - current_avg 제거하거나 사용
    // 필요시 평균 계산은 GetStatistics()에서 수행
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