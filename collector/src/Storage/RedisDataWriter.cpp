//=============================================================================
// collector/src/Storage/RedisDataWriter.cpp - WriteStats 복사 에러 수정
// 
// 목적: Backend 완전 호환 Redis 데이터 저장 구현 (컴파일 에러 수정)
// 특징:
//   - realtime.js가 읽는 정확한 JSON 구조 생성
//   - device:{id}:{point_name} 키 패턴 완벽 구현
//   - Worker 초기화 데이터 저장 지원
//   - atomic 복사 문제 해결
//=============================================================================

#include "Storage/RedisDataWriter.h"
#include "Client/RedisClientImpl.h"
#include "Common/Utils.h"
#include "Common/Enums.h"
#include <chrono>
#include <iomanip>
#include <sstream>

using LogLevel = PulseOne::Enums::LogLevel;
using json = nlohmann::json;

namespace PulseOne {
namespace Storage {

// =============================================================================
// 생성자 및 초기화
// =============================================================================

RedisDataWriter::RedisDataWriter(std::shared_ptr<RedisClient> redis_client)
    : redis_client_(redis_client) {
    
    // Redis 클라이언트 자동 생성
    if (!redis_client_) {
        try {
            redis_client_ = std::make_shared<RedisClientImpl>();
            LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                "Redis 클라이언트 자동 생성 및 연결 성공");
        } catch (const std::exception& e) {
            LogManager::getInstance().log("redis_writer", LogLevel::WARN,
                "Redis 클라이언트 자동 생성 실패: " + std::string(e.what()) + 
                " - Redis 없이 계속 진행");
        }
    }
    
    LogManager::getInstance().log("redis_writer", LogLevel::INFO, 
                "RedisDataWriter 생성 완료");
}

void RedisDataWriter::SetRedisClient(std::shared_ptr<RedisClient> redis_client) {
    std::lock_guard<std::mutex> lock(redis_mutex_);
    redis_client_ = redis_client;
    
    if (redis_client_ && redis_client_->isConnected()) {
        LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                   "Redis 클라이언트 연결 확인됨");
    } else {
        LogManager::getInstance().log("redis_writer", LogLevel::WARN,
                   "Redis 클라이언트 미연결");
    }
}

bool RedisDataWriter::IsConnected() const {
    std::lock_guard<std::mutex> lock(redis_mutex_);
    return redis_client_ && redis_client_->isConnected();
}

void RedisDataWriter::SetStorageMode(StorageMode mode) {
    storage_mode_ = mode;
    LogManager::getInstance().log("redis_writer", LogLevel::INFO,
        "저장 모드 변경: " + std::to_string(static_cast<int>(mode)));
}

void RedisDataWriter::EnableDevicePatternStorage(bool enable) {
    store_device_pattern_ = enable;
}

void RedisDataWriter::EnablePointLatestStorage(bool enable) {
    store_point_latest_ = enable;
}

void RedisDataWriter::EnableFullDataStorage(bool enable) {
    store_full_data_ = enable;
}

// =============================================================================
// Backend 완전 호환 저장 메서드들 (메인 API)
// =============================================================================

size_t RedisDataWriter::SaveDeviceMessage(const Structs::DeviceDataMessage& message) {
    if (!IsConnected()) return 0;
    
    size_t total_saved = 0;
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        // 1. 경량 모드 저장
        if (storage_mode_ == StorageMode::LIGHTWEIGHT || storage_mode_ == StorageMode::HYBRID) {
            total_saved += SaveLightweightFormat(message);
        }
        
        // 2. 완전 데이터 저장
        if (storage_mode_ == StorageMode::FULL_DATA || storage_mode_ == StorageMode::HYBRID) {
            total_saved += SaveFullDataFormat(message);
        }
        
        // 3. Backend 호환 device:{id}:{name} 패턴
        if (store_device_pattern_) {
            total_saved += SaveDevicePatternFormat(message);
        }
        
        // 4. point:{id}:latest 패턴
        if (store_point_latest_) {
            total_saved += SavePointLatestFormat(message);
        }
        
        return total_saved;
        
    } catch (const std::exception& e) {
        HandleError("SaveDeviceMessage", e.what());
        return 0;
    }
}

size_t RedisDataWriter::SaveLightweightFormat(const Structs::DeviceDataMessage& message) {
    size_t saved = 0;
    for (const auto& point : message.points) {
        json light_data;
        light_data["id"] = point.point_id;
        light_data["val"] = ConvertValueToString(point.value);
        light_data["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            point.timestamp.time_since_epoch()).count();
        light_data["q"] = static_cast<int>(point.quality);
        
        std::string key = "point:" + std::to_string(point.point_id) + ":light";
        redis_client_->setex(key, light_data.dump(), 1800);
        saved++;
    }
    return saved;
}

size_t RedisDataWriter::SaveFullDataFormat(const Structs::DeviceDataMessage& message) {
    json full_data;
    full_data["device_id"] = message.device_id;
    full_data["protocol"] = message.protocol;
    full_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count();
    full_data["points"] = json::array();
    
    for (const auto& point : message.points) {
        json point_data;
        point_data["point_id"] = point.point_id;
        point_data["value"] = ConvertValueToString(point.value);
        point_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            point.timestamp.time_since_epoch()).count();
        point_data["quality"] = static_cast<int>(point.quality);
        point_data["changed"] = point.value_changed;
        full_data["points"].push_back(point_data);
    }
    
    std::string key = "device:full:" + ExtractDeviceNumber(message.device_id);
    redis_client_->setex(key, full_data.dump(), 3600);
    return 1;
}

size_t RedisDataWriter::SaveDevicePatternFormat(const Structs::DeviceDataMessage& message) {
    size_t saved = 0;
    std::string device_num = ExtractDeviceNumber(message.device_id);
    
    for (const auto& point : message.points) {
        auto device_point = ConvertToDevicePointData(point, device_num);
        std::string device_key = "device:" + device_num + ":" + device_point.point_name;
        redis_client_->setex(device_key, device_point.toJson().dump(), 3600);
        saved++;
    }
    return saved;
}

size_t RedisDataWriter::SavePointLatestFormat(const Structs::DeviceDataMessage& message) {
    size_t saved = 0;
    std::string device_num = ExtractDeviceNumber(message.device_id);
    
    for (const auto& point : message.points) {
        auto point_latest = ConvertToPointLatestData(point, device_num);
        std::string point_key = "point:" + std::to_string(point.point_id) + ":latest";
        redis_client_->setex(point_key, point_latest.toJson().dump(), 3600);
        saved++;
    }
    return saved;
}

bool RedisDataWriter::SaveSinglePoint(const Structs::TimestampedValue& point, const std::string& device_id) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        std::string device_num = ExtractDeviceNumber(device_id);
        auto device_point = ConvertToDevicePointData(point, device_num);
        
        // 1. device:{id}:{name} 키 저장
        std::string device_key = "device:" + device_num + ":" + device_point.point_name;
        redis_client_->setex(device_key, device_point.toJson().dump(), 3600);
        
        // 2. point:{id}:latest 키 저장
        auto point_latest = ConvertToPointLatestData(point, device_num);
        std::string point_key = "point:" + std::to_string(point.point_id) + ":latest";
        redis_client_->setex(point_key, point_latest.toJson().dump(), 3600);
        
        stats_.total_writes.fetch_add(1);
        stats_.successful_writes.fetch_add(1);
        stats_.device_point_writes.fetch_add(1);
        stats_.point_latest_writes.fetch_add(1);
        
        LogManager::getInstance().log("redis_writer", LogLevel::DEBUG_LEVEL,
                   "단일 포인트 저장 성공: " + device_key);
        
        return true;
        
    } catch (const std::exception& e) {
        HandleError("SaveSinglePoint", e.what());
        return false;
    }
}

bool RedisDataWriter::PublishAlarmEvent(const BackendFormat::AlarmEventData& alarm_data) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        std::string json_str = alarm_data.toJson().dump();
        
        // 1. 여러 채널에 발행
        redis_client_->publish("alarms:all", json_str);
        redis_client_->publish("tenant:" + std::to_string(alarm_data.tenant_id) + ":alarms", json_str);
        redis_client_->publish("device:" + alarm_data.device_id + ":alarms", json_str);
        
        // 심각도별 채널 발행 (severity는 이제 string 타입)
        if (alarm_data.severity == "CRITICAL") {
            redis_client_->publish("alarms:critical", json_str);
        } else if (alarm_data.severity == "HIGH") {
            redis_client_->publish("alarms:high", json_str);
        }
        
        // 2. 활성 알람으로 저장 (state는 이제 string 타입)
        if (alarm_data.state == "active") {
            std::string active_key = "alarm:active:" + std::to_string(alarm_data.rule_id);
            redis_client_->setex(active_key, json_str, 7200); // 2시간 TTL
        }
        
        // 3. ✅ incr 메서드 대신 다른 방법 사용 (RedisClient에 incr이 없음)
        std::string counter_key = "alarms:count:today";
        std::string current_count = redis_client_->get(counter_key);
        int count = current_count.empty() ? 1 : std::stoi(current_count) + 1;
        redis_client_->setex(counter_key, std::to_string(count), 86400); // 24시간 TTL
        
        stats_.total_writes.fetch_add(1);
        stats_.successful_writes.fetch_add(1);
        stats_.alarm_publishes.fetch_add(1);
        
        // ✅ severity 로그 수정: string이므로 to_string 제거
        LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                   "알람 이벤트 발행: rule_id=" + std::to_string(alarm_data.rule_id) + 
                   ", severity=" + alarm_data.severity);  // to_string 제거!
        
        return true;
        
    } catch (const std::exception& e) {
        HandleError("PublishAlarmEvent", e.what());
        return false;
    }
}
// =============================================================================
// Worker 초기화 전용 메서드들
// =============================================================================

size_t RedisDataWriter::SaveWorkerInitialData(
    const std::string& device_id, 
    const std::vector<Structs::TimestampedValue>& current_values) {
    
    if (!IsConnected() || current_values.empty()) {
        return 0;
    }
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        std::string device_num = ExtractDeviceNumber(device_id);
        size_t success_count = 0;
        
        LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                   "Worker 초기화 데이터 저장: " + device_id + " (" + 
                   std::to_string(current_values.size()) + "개 포인트)");
        
        for (const auto& value : current_values) {
            try {
                auto device_point = ConvertToDevicePointData(value, device_num);
                
                // 초기화 소스 표시
                auto json_data = device_point.toJson();
                json_data["source"] = "worker_init";
                json_data["changed"] = false; // 초기값은 변경 아님
                
                std::string device_key = "device:" + device_num + ":" + device_point.point_name;
                redis_client_->setex(device_key, json_data.dump(), 7200); // 2시간 TTL
                
                // Legacy 키도 저장
                auto point_latest = ConvertToPointLatestData(value, device_num);
                std::string point_key = "point:" + std::to_string(value.point_id) + ":latest";
                redis_client_->setex(point_key, point_latest.toJson().dump(), 7200);
                
                success_count++;
                
                LogManager::getInstance().log("redis_writer", LogLevel::DEBUG_LEVEL,
                           "초기값 저장: " + device_key + " = " + device_point.value);
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("redis_writer", LogLevel::WARN,
                           "개별 초기값 저장 실패: " + std::string(e.what()));
            }
        }
        
        // Worker 초기화 알림 발송
        try {
            json init_notification;
            init_notification["type"] = "worker_initialized";
            init_notification["device_id"] = device_id;
            init_notification["device_num"] = device_num;
            init_notification["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            init_notification["points_loaded"] = success_count;
            init_notification["total_points"] = current_values.size();
            
            redis_client_->publish("worker:notifications", init_notification.dump());
        } catch (const std::exception& e) {
            LogManager::getInstance().log("redis_writer", LogLevel::WARN,
                       "초기화 알림 발송 실패: " + std::string(e.what()));
        }
        
        stats_.total_writes.fetch_add(1);
        stats_.worker_init_writes.fetch_add(success_count);
        
        if (success_count == current_values.size()) {
            stats_.successful_writes.fetch_add(1);
            LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                       "Worker 초기화 완료: " + device_id + " - 모든 " + 
                       std::to_string(success_count) + "개 포인트 성공");
        } else {
            stats_.failed_writes.fetch_add(1);
            LogManager::getInstance().log("redis_writer", LogLevel::WARN,
                       "Worker 초기화 부분 실패: " + device_id + " - " + 
                       std::to_string(success_count) + "/" + 
                       std::to_string(current_values.size()) + "개 성공");
        }
        
        return success_count;
        
    } catch (const std::exception& e) {
        HandleError("SaveWorkerInitialData", e.what());
        return 0;
    }
}

bool RedisDataWriter::SaveWorkerStatus(
    const std::string& device_id,
    const std::string& status,
    const json& metadata) {
    
    if (!IsConnected()) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        std::string device_num = ExtractDeviceNumber(device_id);
        
        json worker_status;
        worker_status["device_id"] = device_id;
        worker_status["device_num"] = device_num;
        worker_status["status"] = status;
        worker_status["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        if (!metadata.empty()) {
            worker_status["metadata"] = metadata;
        }
        
        // Worker 상태 저장
        std::string status_key = "worker:" + device_id + ":status";
        redis_client_->setex(status_key, worker_status.dump(), 3600);
        
        // Device 상태도 업데이트
        std::string device_status_key = "device:" + device_num + ":status";
        json device_status;
        device_status["device_id"] = device_num;
        device_status["device_name"] = "Device " + device_num;
        device_status["connection_status"] = (status == "running") ? "connected" : "disconnected";
        device_status["last_communication"] = worker_status["timestamp"];
        device_status["worker_status"] = status;
        
        redis_client_->setex(device_status_key, device_status.dump(), 3600);
        
        // 상태 변화 알림 발송
        json status_notification;
        status_notification["type"] = "worker_status_changed";
        status_notification["device_id"] = device_id;
        status_notification["status"] = status;
        status_notification["timestamp"] = worker_status["timestamp"];
        
        redis_client_->publish("worker:status", status_notification.dump());
        
        stats_.total_writes.fetch_add(1);
        stats_.successful_writes.fetch_add(1);
        
        LogManager::getInstance().log("redis_writer", LogLevel::INFO,
                   "Worker 상태 저장: " + device_id + " -> " + status);
        
        return true;
        
    } catch (const std::exception& e) {
        HandleError("SaveWorkerStatus", e.what());
        return false;
    }
}

// =============================================================================
// 내부 데이터 변환 메서드들
// =============================================================================

BackendFormat::DevicePointData RedisDataWriter::ConvertToDevicePointData(
    const Structs::TimestampedValue& point, 
    const std::string& device_num) const {
    
    BackendFormat::DevicePointData data;
    
    data.point_id = point.point_id;
    data.device_id = device_num;
    data.device_name = "Device " + device_num;
    data.point_name = GetPointName(point.point_id);
    data.value = ConvertValueToString(point.value);
    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        point.timestamp.time_since_epoch()).count();
    data.quality = ConvertQualityToString(point.quality);
    data.data_type = GetDataTypeString(point.value);
    data.unit = GetUnit(point.point_id);
    //data.changed = point.value_changed;
    
    return data;
}

BackendFormat::PointLatestData RedisDataWriter::ConvertToPointLatestData(
    const Structs::TimestampedValue& point, 
    const std::string& device_num) const {
    
    BackendFormat::PointLatestData data;
    
    data.device_id = device_num;
    data.point_id = point.point_id;
    data.value = ConvertValueToString(point.value);
    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        point.timestamp.time_since_epoch()).count();
    data.quality = static_cast<int>(point.quality);
    data.changed = point.value_changed;
    
    return data;
}

std::string RedisDataWriter::ConvertValueToString(const Structs::DataValue& value) const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        }
        else if constexpr (std::is_integral_v<T>) {
            return std::to_string(v);
        }
        else if constexpr (std::is_floating_point_v<T>) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << v;
            return oss.str();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        }
        else {
            return "unknown";
        }
    }, value);
}

std::string RedisDataWriter::ConvertQualityToString(const Structs::DataQuality& quality) const {
    switch (quality) {
        case Structs::DataQuality::GOOD: return "good";
        case Structs::DataQuality::BAD: return "bad";
        case Structs::DataQuality::UNCERTAIN: return "uncertain";
        case Structs::DataQuality::NOT_CONNECTED: return "comm_failure";
        case Structs::DataQuality::TIMEOUT: return "timeout";
        default: return "unknown";
    }
}

std::string RedisDataWriter::GetDataTypeString(const Structs::DataValue& value) const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) return "boolean";
        else if constexpr (std::is_integral_v<T>) return "integer";
        else if constexpr (std::is_floating_point_v<T>) return "number";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else return "unknown";
    }, value);
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::string RedisDataWriter::ExtractDeviceNumber(const std::string& device_id) const {
    // "device_001" -> "1"
    size_t pos = device_id.find_last_of('_');
    if (pos != std::string::npos) {
        std::string number = device_id.substr(pos + 1);
        try {
            int num = std::stoi(number);
            return std::to_string(num); // "001" -> "1"
        } catch (...) {
            return "1";
        }
    }
    
    // 숫자만 있는 경우도 처리
    try {
        int num = std::stoi(device_id);
        return std::to_string(num);
    } catch (...) {
        return "1";
    }
}

std::string RedisDataWriter::GetPointName(int point_id) const {
    // 캐시 확인
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = point_name_cache_.find(point_id);
    if (it != point_name_cache_.end()) {
        return it->second;
    }
    
    // Backend realtime.js와 일치하는 포인트 이름들
    std::unordered_map<int, std::string> point_names = {
        {1, "temperature_sensor_01"},
        {2, "pressure_sensor_01"},
        {3, "flow_rate"},
        {4, "pump_status"},
        {5, "room_temperature"},
        {6, "humidity_level"},
        {7, "fan_speed"},
        {8, "cooling_mode"},
        {9, "voltage_l1"},
        {10, "current_l1"},
        {11, "power_total"},
        {12, "energy_consumed"}
    };
    
    auto name_it = point_names.find(point_id);
    std::string point_name = (name_it != point_names.end()) ? 
        name_it->second : ("point_" + std::to_string(point_id));
    
    // 캐시에 저장
    point_name_cache_[point_id] = point_name;
    return point_name;
}

std::string RedisDataWriter::GetUnit(int point_id) const {
    // 캐시 확인
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = unit_cache_.find(point_id);
    if (it != unit_cache_.end()) {
        return it->second;
    }
    
    std::unordered_map<int, std::string> units = {
        {1, "°C"}, {2, "bar"}, {3, "L/min"}, {4, ""}, {5, "°C"},
        {6, "%"}, {7, "rpm"}, {8, ""}, {9, "V"}, {10, "A"},
        {11, "W"}, {12, "kWh"}
    };
    
    auto unit_it = units.find(point_id);
    std::string unit = (unit_it != units.end()) ? unit_it->second : "";
    
    // 캐시에 저장
    unit_cache_[point_id] = unit;
    return unit;
}

std::string RedisDataWriter::GetDeviceIdForPoint(int point_id) const {
    // 포인트 ID를 기반으로 디바이스 ID 추론 (임시 로직)
    return "device_" + std::to_string(point_id / 100 + 1);
}

void RedisDataWriter::HandleError(const std::string& context, const std::string& error_message) {
    stats_.total_writes.fetch_add(1);
    stats_.failed_writes.fetch_add(1);
    
    std::string full_message = context + ": " + error_message;
    LogManager::getInstance().log("redis_writer", LogLevel::ERROR, full_message);
}

// =============================================================================
// 통계 및 상태 - ✅ 컴파일 에러 수정: nlohmann::json으로 반환
// =============================================================================

nlohmann::json RedisDataWriter::GetStatistics() const {
    json stats_json;
    stats_json["total_writes"] = stats_.total_writes.load();
    stats_json["successful_writes"] = stats_.successful_writes.load();
    stats_json["failed_writes"] = stats_.failed_writes.load();
    stats_json["device_point_writes"] = stats_.device_point_writes.load();
    stats_json["point_latest_writes"] = stats_.point_latest_writes.load();
    stats_json["alarm_publishes"] = stats_.alarm_publishes.load();
    stats_json["worker_init_writes"] = stats_.worker_init_writes.load();
    
    return stats_json;
}

void RedisDataWriter::ResetStatistics() {
    stats_.total_writes.store(0);
    stats_.successful_writes.store(0);
    stats_.failed_writes.store(0);
    stats_.device_point_writes.store(0);
    stats_.point_latest_writes.store(0);
    stats_.alarm_publishes.store(0);
    stats_.worker_init_writes.store(0);
    
    LogManager::getInstance().log("redis_writer", LogLevel::INFO, "Redis 쓰기 통계 리셋 완료");
}

json RedisDataWriter::GetStatus() const {
    json status;
    status["connected"] = IsConnected();
    status["statistics"] = GetStatistics();  // 이제 json 반환
    status["redis_client_available"] = (redis_client_ != nullptr);
    status["cache_size"] = {
        {"point_names", point_name_cache_.size()},
        {"units", unit_cache_.size()}
    };
    
    return status;
}

bool RedisDataWriter::StoreVirtualPointToRedis(const Structs::TimestampedValue& virtual_point_data) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        
        // 가상포인트 전용 키 패턴
        std::string vp_key = "virtualpoint:" + std::to_string(virtual_point_data.point_id);
        
        json vp_json;
        vp_json["point_id"] = virtual_point_data.point_id;
        vp_json["value"] = ConvertValueToString(virtual_point_data.value);
        vp_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            virtual_point_data.timestamp.time_since_epoch()).count();
        vp_json["quality"] = ConvertQualityToString(virtual_point_data.quality);
        vp_json["is_virtual"] = true;
        vp_json["source"] = virtual_point_data.source;
        
        redis_client_->setex(vp_key, vp_json.dump(), 3600);
        
        stats_.total_writes.fetch_add(1);
        stats_.successful_writes.fetch_add(1);
        
        return true;
        
    } catch (const std::exception& e) {
        HandleError("StoreVirtualPointToRedis", e.what());
        return false;
    }
}

} // namespace Storage
} // namespace PulseOne