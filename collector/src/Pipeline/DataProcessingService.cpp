// =============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 구현부
// =============================================================================
#include "Pipeline/DataProcessingService.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

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
    
    LogManager::getInstance().Info("🔧 DataProcessingService 생성됨");
}

DataProcessingService::~DataProcessingService() {
    Stop();
    LogManager::getInstance().Info("💀 DataProcessingService 소멸됨");
}

// =============================================================================
// 🔥 핵심 인터페이스 구현
// =============================================================================

void DataProcessingService::ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (batch.empty() || !is_running_.load()) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        LogManager::getInstance().Debug("🔄 배치 처리 시작: {}개 메시지", batch.size());
        
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
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        LogManager::getInstance().Debug("✅ 배치 처리 완료: {}개 메시지, {}μs 소요", 
                                      batch.size(), duration.count());
                                      
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("💥 배치 처리 중 예외: {}", e.what());
    }
}

bool DataProcessingService::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("⚠️ DataProcessingService already running");
        return false;
    }
    
    LogManager::getInstance().Info("🚀 DataProcessingService 시작됨");
    
    is_running_ = true;
    return true;
}

void DataProcessingService::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("🛑 DataProcessingService 중지됨");
    LogManager::getInstance().Info("📊 최종 통계 - 배치: {}, 메시지: {}, Redis: {}, InfluxDB: {}", 
                                 total_batches_processed_.load(), 
                                 total_messages_processed_.load(),
                                 redis_writes_.load(), 
                                 influx_writes_.load());
    
    is_running_ = false;
}

// =============================================================================
// 🔥 단계별 처리 메서드들
// =============================================================================

std::vector<Structs::DeviceDataMessage> DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage>& batch) {
    
    // 🔥 나중에 구현 - 지금은 원본 데이터 그대로 반환
    LogManager::getInstance().Debug("🔢 가상포인트 계산 건너뜀 (나중에 구현)");
    return batch;
}

void DataProcessingService::CheckAlarms(const std::vector<Structs::DeviceDataMessage>& all_data) {
    // 🔥 나중에 구현
    LogManager::getInstance().Debug("🚨 알람 체크 건너뜀 (나중에 구현)");
}

void DataProcessingService::SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch) {
    if (!redis_client_) {
        LogManager::getInstance().Debug("⚠️ Redis 클라이언트 없음");
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        for (const auto& message : batch) {
            WriteDeviceDataToRedis(message);
        }
        
        redis_writes_.fetch_add(batch.size());
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        LogManager::getInstance().Debug("💾 Redis 저장 완료: {}개 메시지, {}μs 소요", 
                                      batch.size(), duration.count());
                                      
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("💥 Redis 저장 중 예외: {}", e.what());
    }
}

void DataProcessingService::SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch) {
    // 🔥 나중에 구현
    LogManager::getInstance().Debug("📈 InfluxDB 저장 건너뜀 (나중에 구현)");
}

// =============================================================================
// 🔥 Redis 저장 헬퍼 메서드들
// =============================================================================

void DataProcessingService::WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message) {
    try {
        // 🔥 1. 디바이스 최신값 해시 저장
        // Key: device:{device_id}:latest
        // Fields: point_id -> JSON value
        std::string device_hash_key = fmt::format("device:{}:latest", message.device_id);
        
        for (const auto& point : message.points) {
            try {
                // JSON 형태로 변환
                std::string json_value = TimestampedValueToJson(point);
                
                // Redis Hash Field 저장
                redis_client_->HSet(device_hash_key, point.point_id, json_value);
                
                LogManager::getInstance().Debug("💾 Redis 저장: {} -> {} = {}", 
                                              device_hash_key, point.point_id, json_value);
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("❌ Redis 포인트 저장 실패: {} - {}", 
                                              point.point_id, e.what());
            }
        }
        
        // 🔥 2. 디바이스 마지막 업데이트 시간 저장
        std::string timestamp_key = fmt::format("device:{}:last_update", message.device_id);
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        redis_client_->Set(timestamp_key, std::to_string(timestamp));
        
        // 🔥 3. 디바이스 상태 저장 (선택적)
        std::string status_key = fmt::format("device:{}:status", message.device_id);
        redis_client_->Set(status_key, "online");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("💥 Redis 디바이스 저장 실패: {} - {}", 
                                      message.device_id, e.what());
    }
}

std::string DataProcessingService::TimestampedValueToJson(const Structs::TimestampedValue& value) {
    try {
        json j;
        j["value"] = value.value.numeric_value;  // 기본적으로 숫자값 사용
        j["quality"] = value.quality;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            value.timestamp.time_since_epoch()).count();
        
        // 추가 정보들
        if (!value.unit.empty()) {
            j["unit"] = value.unit;
        }
        
        if (!value.description.empty()) {
            j["description"] = value.description;
        }
        
        return j.dump();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ JSON 변환 실패: {}", e.what());
        return "{}";
    }
}

} // namespace Pipeline
} // namespace PulseOne