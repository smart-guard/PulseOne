// collector/src/Engine/DeviceIntegration.cpp
// PulseOne DeviceWorker와 Database 연동 레이어 구현

#include "Engine/DeviceIntegration.h"
#include "Database/DataAccessManager.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Engine {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DeviceIntegration::DeviceIntegration(std::shared_ptr<LogManager> logger,
                                   std::shared_ptr<ConfigManager> config)
    : logger_(logger)
    , config_(config)
    , running_(false)
    , event_queue_(EventComparator) {
    
    logger_->Info("DeviceIntegration created");
}

DeviceIntegration::~DeviceIntegration() {
    if (running_) {
        Shutdown();
    }
    logger_->Info("DeviceIntegration destroyed");
}

// =============================================================================
// 초기화 및 종료
// =============================================================================

bool DeviceIntegration::Initialize() {
    if (running_) {
        logger_->Warning("DeviceIntegration already initialized");
        return true;
    }
    
    try {
        // DataAccessManager에서 DeviceDataAccess 획득
        auto& dam = Database::DataAccessManager::GetInstance();
        device_data_access_ = dam.GetDomain<Database::DeviceDataAccess>();
        
        if (!device_data_access_) {
            logger_->Error("Failed to get DeviceDataAccess from DataAccessManager");
            return false;
        }
        
        // 설정값 로드
        LoadConfiguration();
        
        // 워커 스레드 시작
        running_ = true;
        
        sync_worker_thread_ = std::thread(&DeviceIntegration::SyncWorkerThread, this);
        batch_processor_thread_ = std::thread(&DeviceIntegration::BatchProcessorThread, this);
        
        logger_->Info("DeviceIntegration initialized successfully");
        logger_->Info("  - Batch size: " + std::to_string(max_batch_size_));
        logger_->Info("  - Batch timeout: " + std::to_string(batch_timeout_.count()) + "ms");
        logger_->Info("  - Cache TTL: " + std::to_string(cache_ttl_.count()) + "s");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceIntegration initialization failed: " + std::string(e.what()));
        return false;
    }
}

void DeviceIntegration::Shutdown() {
    if (!running_) {
        return;
    }
    
    logger_->Info("DeviceIntegration shutting down...");
    
    // 워커 스레드 종료 신호
    running_ = false;
    
    // 대기 중인 스레드들 깨우기
    event_queue_cv_.notify_all();
    batch_cv_.notify_all();
    
    // 스레드 종료 대기
    if (sync_worker_thread_.joinable()) {
        sync_worker_thread_.join();
    }
    if (batch_processor_thread_.joinable()) {
        batch_processor_thread_.join();
    }
    
    // 남은 배치 처리
    FlushBatch();
    
    // 캐시 정리
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        device_info_cache_.clear();
        datapoints_cache_.clear();
        cache_timestamps_.clear();
    }
    
    logger_->Info("DeviceIntegration shutdown completed");
    logger_->Info("Final stats: " + GetProcessingStats().dump(2));
}

// =============================================================================
// 설정 로드
// =============================================================================

void DeviceIntegration::LoadConfiguration() {
    if (!config_) {
        return;
    }
    
    // 배치 처리 설정
    max_batch_size_ = std::stoul(config_->GetValue("device_integration.max_batch_size", "100"));
    batch_timeout_ = std::chrono::milliseconds(
        std::stoul(config_->GetValue("device_integration.batch_timeout_ms", "1000"))
    );
    
    // 캐시 설정
    cache_ttl_ = std::chrono::seconds(
        std::stoul(config_->GetValue("device_integration.cache_ttl_seconds", "300"))
    );
    max_cache_size_ = std::stoul(config_->GetValue("device_integration.max_cache_size", "1000"));
    
    logger_->Debug("Configuration loaded");
}

// =============================================================================
// DeviceWorker에서 호출하는 메소드들
// =============================================================================

void DeviceIntegration::UpdateDataValue(int device_id, int point_id, 
                                       const DataValue& value, 
                                       const std::string& quality,
                                       int priority) {
    try {
        SyncEvent event(SyncEventType::DATA_VALUE_UPDATED, device_id, point_id, priority);
        
        // 데이터를 JSON으로 저장
        event.data["value"] = ConvertDataValueToJson(value);
        event.data["quality"] = quality;
        
        // 이벤트 큐에 추가
        {
            std::lock_guard<std::mutex> lock(event_queue_mutex_);
            event_queue_.push(event);
        }
        event_queue_cv_.notify_one();
        
        stats_.events_processed.fetch_add(1);
        
    } catch (const std::exception& e) {
        logger_->Error("UpdateDataValue failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

void DeviceIntegration::UpdateDeviceStatus(int device_id, 
                                          const std::string& connection_status,
                                          int response_time,
                                          const std::string& error_message) {
    try {
        SyncEvent event(SyncEventType::DEVICE_STATUS_CHANGED, device_id, -1, 3); // 높은 우선순위
        
        event.data["connection_status"] = connection_status;
        event.data["response_time"] = response_time;
        event.data["error_message"] = error_message;
        
        // 즉시 처리하거나 배치에 추가
        if (connection_status == "error" || connection_status == "disconnected") {
            // 에러 상태는 즉시 처리
            {
                std::lock_guard<std::mutex> lock(event_queue_mutex_);
                event_queue_.push(event);
            }
            event_queue_cv_.notify_one();
        } else {
            // 정상 상태는 배치 처리
            AddToStatusBatch(device_id, connection_status, response_time, error_message);
        }
        
        stats_.events_processed.fetch_add(1);
        
    } catch (const std::exception& e) {
        logger_->Error("UpdateDeviceStatus failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

void DeviceIntegration::UpdateDataValuesBatch(int device_id, 
                                             const std::map<int, DataValue>& data_updates) {
    try {
        if (data_updates.empty()) {
            return;
        }
        
        SyncEvent event(SyncEventType::BULK_DATA_UPDATE, device_id, -1, 5);
        
        // 모든 데이터를 JSON으로 변환
        nlohmann::json bulk_data = nlohmann::json::object();
        for (const auto& [point_id, value] : data_updates) {
            bulk_data[std::to_string(point_id)] = ConvertDataValueToJson(value);
        }
        event.data["bulk_updates"] = bulk_data;
        
        // 배치 처리 큐에 직접 추가
        AddToBatch(device_id, data_updates);
        
        stats_.events_processed.fetch_add(data_updates.size());
        
        logger_->Debug("Added " + std::to_string(data_updates.size()) + 
                      " data values to batch for device " + std::to_string(device_id));
        
    } catch (const std::exception& e) {
        logger_->Error("UpdateDataValuesBatch failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

void DeviceIntegration::NotifyConfigurationChange(int device_id, const nlohmann::json& config_changes) {
    try {
        SyncEvent event(SyncEventType::DEVICE_CONFIGURATION_CHANGED, device_id, -1, 1); // 최고 우선순위
        
        event.data["config_changes"] = config_changes;
        
        // 즉시 처리
        {
            std::lock_guard<std::mutex> lock(event_queue_mutex_);
            event_queue_.push(event);
        }
        event_queue_cv_.notify_one();
        
        // 캐시 무효화
        InvalidateDeviceCache(device_id);
        
        stats_.events_processed.fetch_add(1);
        
        logger_->Info("Configuration change notified for device " + std::to_string(device_id));
        
    } catch (const std::exception& e) {
        logger_->Error("NotifyConfigurationChange failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

// =============================================================================
// 디바이스 설정 조회 (캐시 포함)
// =============================================================================

bool DeviceIntegration::GetDeviceInfo(int device_id, Database::DeviceInfo& device_info) {
    // 캐시 확인
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        auto cache_it = device_info_cache_.find(device_id);
        auto timestamp_it = cache_timestamps_.find(device_id);
        
        if (cache_it != device_info_cache_.end() && timestamp_it != cache_timestamps_.end()) {
            auto now = std::chrono::steady_clock::now();
            if (now - timestamp_it->second < cache_ttl_) {
                device_info = cache_it->second;
                stats_.cache_hits.fetch_add(1);
                return true;
            } else {
                // 만료된 캐시 제거
                device_info_cache_.erase(cache_it);
                cache_timestamps_.erase(timestamp_it);
            }
        }
    }
    
    // 데이터베이스에서 조회
    stats_.cache_misses.fetch_add(1);
    bool success = device_data_access_->GetDevice(device_id, device_info);
    
    if (success) {
        // 캐시에 저장
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        device_info_cache_[device_id] = device_info;
        cache_timestamps_[device_id] = std::chrono::steady_clock::now();
        
        // 캐시 크기 제한
        if (device_info_cache_.size() > max_cache_size_) {
            CleanupExpiredCache();
        }
    }
    
    return success;
}

std::vector<Database::DataPointInfo> DeviceIntegration::GetEnabledDataPoints(int device_id) {
    // 캐시 확인
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        auto cache_it = datapoints_cache_.find(device_id);
        auto timestamp_it = cache_timestamps_.find(device_id);
        
        if (cache_it != datapoints_cache_.end() && timestamp_it != cache_timestamps_.end()) {
            auto now = std::chrono::steady_clock::now();
            if (now - timestamp_it->second < cache_ttl_) {
                stats_.cache_hits.fetch_add(1);
                return cache_it->second;
            }
        }
    }
    
    // 데이터베이스에서 조회
    stats_.cache_misses.fetch_add(1);
    auto datapoints = device_data_access_->GetEnabledDataPoints(device_id);
    
    // 캐시에 저장
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        datapoints_cache_[device_id] = datapoints;
        cache_timestamps_[device_id] = std::chrono::steady_clock::now();
    }
    
    return datapoints;
}

bool DeviceIntegration::GetDeviceConfig(int device_id, Drivers::DeviceConfig& config) {
    Database::DeviceInfo device_info;
    if (!GetDeviceInfo(device_id, device_info)) {
        return false;
    }
    
    try {
        // DeviceInfo를 DeviceConfig로 변환
        config.device_id = device_id;
        config.name = device_info.name;
        config.protocol_type = StringToProtocolType(device_info.protocol_type);
        config.endpoint = device_info.endpoint;
        config.polling_interval_ms = device_info.polling_interval;
        config.timeout_ms = device_info.timeout;
        config.retry_count = device_info.retry_count;
        config.enabled = device_info.is_enabled;
        
        // JSON 설정 파싱
        if (!device_info.config.empty()) {
            auto json_config = nlohmann::json::parse(device_info.config);
            for (auto& [key, value] : json_config.items()) {
                config.properties[key] = value.get<std::string>();
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("GetDeviceConfig conversion failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 워커 스레드 구현
// =============================================================================

void DeviceIntegration::SyncWorkerThread() {
    logger_->Info("Sync worker thread started");
    
    while (running_) {
        try {
            SyncEvent event(SyncEventType::DATA_VALUE_UPDATED); // 기본값
            
            // 이벤트 대기
            {
                std::unique_lock<std::mutex> lock(event_queue_mutex_);
                event_queue_cv_.wait(lock, [this] { return !running_ || !event_queue_.empty(); });
                
                if (!running_) {
                    break;
                }
                
                if (!event_queue_.empty()) {
                    event = event_queue_.top();
                    event_queue_.pop();
                }
            }
            
            // 이벤트 처리
            ProcessSyncEvent(event);
            
        } catch (const std::exception& e) {
            logger_->Error("Sync worker thread error: " + std::string(e.what()));
            stats_.database_errors.fetch_add(1);
            
            // 에러 발생 시 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    logger_->Info("Sync worker thread ended");
}

void DeviceIntegration::BatchProcessorThread() {
    logger_->Info("Batch processor thread started");
    
    while (running_) {
        try {
            // 배치 타임아웃 또는 배치 크기에 도달할 때까지 대기
            {
                std::unique_lock<std::mutex> lock(batch_mutex_);
                batch_cv_.wait_for(lock, batch_timeout_, [this] {
                    return !running_ || current_batch_.size() >= max_batch_size_;
                });
                
                if (!running_) {
                    break;
                }
            }
            
            // 배치 처리
            if (!current_batch_.empty()) {
                FlushBatch();
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Batch processor thread error: " + std::string(e.what()));
            stats_.database_errors.fetch_add(1);
            
            // 에러 발생 시 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
    logger_->Info("Batch processor thread ended");
}

void DeviceIntegration::ProcessSyncEvent(const SyncEvent& event) {
    switch (event.type) {
        case SyncEventType::DEVICE_STATUS_CHANGED:
            ProcessDeviceStatusEvent(event);
            break;
            
        case SyncEventType::DATA_VALUE_UPDATED:
            ProcessDataValueEvent(event);
            break;
            
        case SyncEventType::DEVICE_CONFIGURATION_CHANGED:
            ProcessConfigurationChangeEvent(event);
            break;
            
        case SyncEventType::BULK_DATA_UPDATE:
            ProcessBulkDataEvent(event);
            break;
            
        case SyncEventType::CONNECTION_STATE_CHANGED:
            ProcessConnectionStateEvent(event);
            break;
            
        default:
            logger_->Warning("Unknown sync event type: " + std::to_string(static_cast<int>(event.type)));
            break;
    }
}

// =============================================================================
// 이벤트 처리 메소드들
// =============================================================================

void DeviceIntegration::ProcessDeviceStatusEvent(const SyncEvent& event) {
    try {
        Database::DeviceStatus status;
        status.device_id = event.device_id;
        status.connection_status = event.data["connection_status"];
        status.response_time = event.data["response_time"];
        status.last_error = event.data["error_message"];
        status.error_count = 0; // TODO: 에러 카운터 누적 로직
        
        bool success = device_data_access_->UpdateDeviceStatus(status);
        if (!success) {
            logger_->Error("Failed to update device status for device " + std::to_string(event.device_id));
            stats_.database_errors.fetch_add(1);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ProcessDeviceStatusEvent failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

void DeviceIntegration::ProcessDataValueEvent(const SyncEvent& event) {
    try {
        Database::CurrentValue current_value = ConvertToCurrentValue(
            event.point_id, 
            event.data["value"], 
            event.data["quality"]
        );
        
        bool success = device_data_access_->UpdateCurrentValue(event.point_id, current_value);
        if (!success) {
            logger_->Error("Failed to update data value for point " + std::to_string(event.point_id));
            stats_.database_errors.fetch_add(1);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ProcessDataValueEvent failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

void DeviceIntegration::ProcessConfigurationChangeEvent(const SyncEvent& event) {
    try {
        // 설정 변경 로그
        logger_->Info("Configuration changed for device " + std::to_string(event.device_id) + 
                     ": " + event.data["config_changes"].dump());
        
        // 캐시 무효화
        InvalidateDeviceCache(event.device_id);
        
        // TODO: 설정 변경 이력을 별도 테이블에 저장
        
    } catch (const std::exception& e) {
        logger_->Error("ProcessConfigurationChangeEvent failed: " + std::string(e.what()));
    }
}

void DeviceIntegration::ProcessBulkDataEvent(const SyncEvent& event) {
    // 벌크 데이터는 이미 배치에서 처리되므로 여기서는 로깅만
    logger_->Debug("Bulk data update processed for device " + std::to_string(event.device_id));
}

void DeviceIntegration::ProcessConnectionStateEvent(const SyncEvent& event) {
    // 연결 상태 변경은 device status와 동일하게 처리
    ProcessDeviceStatusEvent(event);
}

// =============================================================================
// 배치 처리 구현
// =============================================================================

void DeviceIntegration::AddToBatch(int device_id, const std::map<int, DataValue>& data_updates) {
    std::lock_guard<std::mutex> lock(batch_mutex_);
    
    for (const auto& [point_id, value] : data_updates) {
        Database::CurrentValue current_value = ConvertToCurrentValue(point_id, value, "good");
        current_batch_.current_values[point_id] = current_value;
    }
    
    // 배치가 가득 찬 경우 신호 보내기
    if (current_batch_.size() >= max_batch_size_) {
        batch_cv_.notify_one();
    }
}

void DeviceIntegration::AddToStatusBatch(int device_id, const std::string& connection_status, 
                                        int response_time, const std::string& error_message) {
    std::lock_guard<std::mutex> lock(batch_mutex_);
    
    Database::DeviceStatus status;
    status.device_id = device_id;
    status.connection_status = connection_status;
    status.response_time = response_time;
    status.last_error = error_message;
    
    current_batch_.device_statuses[device_id] = status;
    
    // 배치가 가득 찬 경우 신호 보내기
    if (current_batch_.size() >= max_batch_size_) {
        batch_cv_.notify_one();
    }
}

void DeviceIntegration::FlushBatch() {
    std::lock_guard<std::mutex> lock(batch_mutex_);
    
    if (current_batch_.empty()) {
        return;
    }
    
    try {
        // 현재값 배치 업데이트
        if (!current_batch_.current_values.empty()) {
            bool success = device_data_access_->UpdateCurrentValuesBatch(current_batch_.current_values);
            if (success) {
                logger_->Debug("Flushed " + std::to_string(current_batch_.current_values.size()) + " current values");
                stats_.batch_operations.fetch_add(1);
            } else {
                logger_->Error("Failed to flush current values batch");
                stats_.database_errors.fetch_add(1);
            }
        }
        
        // 디바이스 상태 배치 업데이트
        for (const auto& [device_id, status] : current_batch_.device_statuses) {
            bool success = device_data_access_->UpdateDeviceStatus(status);
            if (!success) {
                logger_->Error("Failed to update device status in batch for device " + std::to_string(device_id));
                stats_.database_errors.fetch_add(1);
            }
        }
        
        if (!current_batch_.device_statuses.empty()) {
            logger_->Debug("Flushed " + std::to_string(current_batch_.device_statuses.size()) + " device statuses");
            stats_.batch_operations.fetch_add(1);
        }
        
        // 배치 클리어
        current_batch_.clear();
        
    } catch (const std::exception& e) {
        logger_->Error("FlushBatch failed: " + std::string(e.what()));
        stats_.database_errors.fetch_add(1);
    }
}

// =============================================================================
// 캐시 관리
// =============================================================================

void DeviceIntegration::InvalidateDeviceCache(int device_id) {
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    device_info_cache_.erase(device_id);
    datapoints_cache_.erase(device_id);
    cache_timestamps_.erase(device_id);
}

void DeviceIntegration::CleanupExpiredCache() {
    auto now = std::chrono::steady_clock::now();
    
    auto it = cache_timestamps_.begin();
    while (it != cache_timestamps_.end()) {
        if (now - it->second > cache_ttl_) {
            int device_id = it->first;
            device_info_cache_.erase(device_id);
            datapoints_cache_.erase(device_id);
            it = cache_timestamps_.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

nlohmann::json DeviceIntegration::GetProcessingStats() const {
    nlohmann::json stats;
    
    stats["events_processed"] = stats_.events_processed.load();
    stats["batch_operations"] = stats_.batch_operations.load();
    stats["cache_hits"] = stats_.cache_hits.load();
    stats["cache_misses"] = stats_.cache_misses.load();
    stats["database_errors"] = stats_.database_errors.load();
    
    // 캐시 히트율 계산
    uint64_t total_cache_requests = stats_.cache_hits.load() + stats_.cache_misses.load();
    if (total_cache_requests > 0) {
        stats["cache_hit_rate"] = static_cast<double>(stats_.cache_hits.load()) / total_cache_requests;
    } else {
        stats["cache_hit_rate"] = 0.0;
    }
    
    return stats;
}

nlohmann::json DeviceIntegration::GetQueueStatus() const {
    nlohmann::json status;
    
    {
        std::lock_guard<std::mutex> lock(event_queue_mutex_);
        status["event_queue_size"] = event_queue_.size();
    }
    
    {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        status["current_batch_size"] = current_batch_.size();
        status["current_values_count"] = current_batch_.current_values.size();
        status["device_statuses_count"] = current_batch_.device_statuses.size();
    }
    
    status["is_running"] = running_.load();
    
    return status;
}

nlohmann::json DeviceIntegration::GetCacheStats() const {
    nlohmann::json stats;
    
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        stats["device_info_cache_size"] = device_info_cache_.size();
        stats["datapoints_cache_size"] = datapoints_cache_.size();
        stats["total_cached_devices"] = cache_timestamps_.size();
    }
    
    stats["cache_ttl_seconds"] = cache_ttl_.count();
    stats["max_cache_size"] = max_cache_size_;
    
    return stats;
}

void DeviceIntegration::RefreshConfiguration() {
    LoadConfiguration();
    
    // 캐시 전체 클리어
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        device_info_cache_.clear();
        datapoints_cache_.clear();
        cache_timestamps_.clear();
    }
    
    logger_->Info("Configuration refreshed and caches cleared");
}

// =============================================================================
// 설정 변경
// =============================================================================

void DeviceIntegration::SetBatchSettings(size_t max_batch_size, int batch_timeout_ms) {
    max_batch_size_ = max_batch_size;
    batch_timeout_ = std::chrono::milliseconds(batch_timeout_ms);
    
    logger_->Info("Batch settings updated: size=" + std::to_string(max_batch_size) + 
                 ", timeout=" + std::to_string(batch_timeout_ms) + "ms");
}

void DeviceIntegration::SetCacheSettings(int cache_ttl_seconds, size_t max_cache_size) {
    cache_ttl_ = std::chrono::seconds(cache_ttl_seconds);
    max_cache_size_ = max_cache_size;
    
    // 필요한 경우 캐시 정리
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        if (device_info_cache_.size() > max_cache_size_) {
            CleanupExpiredCache();
        }
    }
    
    logger_->Info("Cache settings updated: TTL=" + std::to_string(cache_ttl_seconds) + 
                 "s, max_size=" + std::to_string(max_cache_size));
}

// =============================================================================
// 유틸리티 메소드들
// =============================================================================

Database::CurrentValue DeviceIntegration::ConvertToCurrentValue(int point_id, 
                                                               const DataValue& value,
                                                               const std::string& quality) {
    Database::CurrentValue current_value;
    current_value.point_id = point_id;
    current_value.quality = quality;
    current_value.timestamp = GetCurrentTimestamp();
    
    // DataValue에서 값 추출
    std::visit([&current_value](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_arithmetic_v<T>) {
            current_value.value = static_cast<double>(val);
            current_value.raw_value = current_value.value;
            current_value.string_value = std::to_string(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            current_value.string_value = val;
            current_value.value = 0.0;
            current_value.raw_value = 0.0;
        } else if constexpr (std::is_same_v<T, bool>) {
            current_value.value = val ? 1.0 : 0.0;
            current_value.raw_value = current_value.value;
            current_value.string_value = val ? "true" : "false";
        }
    }, value);
    
    return current_value;
}

Database::CurrentValue DeviceIntegration::ConvertToCurrentValue(int point_id, 
                                                               const nlohmann::json& value_json,
                                                               const std::string& quality) {
    Database::CurrentValue current_value;
    current_value.point_id = point_id;
    current_value.quality = quality;
    current_value.timestamp = GetCurrentTimestamp();
    
    // JSON에서 값 추출
    if (value_json.is_number()) {
        current_value.value = value_json.get<double>();
        current_value.raw_value = current_value.value;
        current_value.string_value = std::to_string(current_value.value);
    } else if (value_json.is_string()) {
        current_value.string_value = value_json.get<std::string>();
        current_value.value = 0.0;
        current_value.raw_value = 0.0;
    } else if (value_json.is_boolean()) {
        bool bool_val = value_json.get<bool>();
        current_value.value = bool_val ? 1.0 : 0.0;
        current_value.raw_value = current_value.value;
        current_value.string_value = bool_val ? "true" : "false";
    }
    
    return current_value;
}

nlohmann::json DeviceIntegration::ConvertDataValueToJson(const DataValue& value) {
    nlohmann::json json_value;
    
    std::visit([&json_value](const auto& val) {
        json_value = val;
    }, value);
    
    return json_value;
}

std::string DeviceIntegration::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

Drivers::ProtocolType DeviceIntegration::StringToProtocolType(const std::string& protocol_str) {
    if (protocol_str == "modbus_tcp") return Drivers::ProtocolType::MODBUS_TCP;
    if (protocol_str == "modbus_rtu") return Drivers::ProtocolType::MODBUS_RTU;
    if (protocol_str == "mqtt") return Drivers::ProtocolType::MQTT;
    if (protocol_str == "bacnet") return Drivers::ProtocolType::BACNET;
    return Drivers::ProtocolType::UNKNOWN;
}

} // namespace Engine
} // namespace PulseOne