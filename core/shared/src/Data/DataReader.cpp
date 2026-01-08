// =============================================================================
// core/shared/src/Data/DataReader.cpp
// Redis 데이터 읽기 구현 (검증 및 수정 완료)
// =============================================================================

// ✅ 핵심: 헤더 include 순서 중요!
#include "Data/DataReader.h"          // 자신의 헤더
#include "Client/RedisClient.h"        // RedisClient 전체 정의 (필수!)
#include "Logging/LogManager.h"          // LogManager

#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DataReader::DataReader(std::shared_ptr<RedisClient> redis,
                       const DataReaderOptions& options)
    : redis_(redis)
    , options_(options)
    , stats_{}
{
    // ✅ 수정: throw 대신 에러 로그만 남기기 (DataWriter 패턴)
    if (!redis_) {
        LogManager::getInstance().Error("DataReader: Redis client is null");
        return;
    }
    
    LogManager::getInstance().Info("DataReader: 초기화 완료");
}

DataReader::~DataReader() {
    UnsubscribeAll();
    LogManager::getInstance().Info("DataReader: 종료 완료");
}

// =============================================================================
// 데이터 포인트 읽기 (device:{num}:{name})
// =============================================================================

std::optional<CurrentValue> DataReader::ReadDevicePoint(int device_num, 
                                                        const std::string& point_name) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::DevicePointKey(device_num, point_name);
        
        // 캐시 확인
        if (options_.enable_cache) {
            auto cached = getFromCache(key);
            if (cached) {
                cache_hit_count_++;
                auto result = parseCurrentValue(*cached);
                updateStats(true, 0.0);
                return result;
            }
            cache_miss_count_++;
        }
        
        // Redis에서 읽기
        std::string json_str = readWithRetry(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (json_str.empty()) {
            updateStats(false, elapsed);
            return std::nullopt;
        }
        
        // 캐시 저장
        if (options_.enable_cache) {
            putToCache(key, json_str);
        }
        
        auto result = parseCurrentValue(json_str);
        updateStats(true, elapsed);
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadDevicePoint failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return std::nullopt;
    }
}

std::vector<CurrentValue> DataReader::ReadAllDevicePoints(int device_num) {
    auto start = std::chrono::steady_clock::now();
    std::vector<CurrentValue> results;
    
    try {
        std::string pattern = RedisKeyBuilder::DeviceAllPointsPattern(device_num);
        auto keys = FindKeys(pattern);
        
        LogManager::getInstance().Info(
            "DataReader::ReadAllDevicePoints: Found " + std::to_string(keys.size()) + 
            " keys for device " + std::to_string(device_num));
        
        for (const auto& key : keys) {
            std::string json_str = readWithRetry(key);
            if (!json_str.empty()) {
                auto value = parseCurrentValue(json_str);
                if (value) {
                    results.push_back(*value);
                }
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(!results.empty(), elapsed);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadAllDevicePoints failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
    }
    
    return results;
}

// =============================================================================
// Point Latest 읽기 (point:{id}:latest)
// =============================================================================

std::optional<CurrentValue> DataReader::ReadPointLatest(int point_id) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::PointLatestKey(point_id);
        
        // 캐시 확인
        if (options_.enable_cache) {
            auto cached = getFromCache(key);
            if (cached) {
                cache_hit_count_++;
                auto result = parseCurrentValue(*cached);
                updateStats(true, 0.0);
                return result;
            }
            cache_miss_count_++;
        }
        
        // Redis에서 읽기
        std::string json_str = readWithRetry(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (json_str.empty()) {
            updateStats(false, elapsed);
            return std::nullopt;
        }
        
        // 캐시 저장
        if (options_.enable_cache) {
            putToCache(key, json_str);
        }
        
        auto result = parseCurrentValue(json_str);
        updateStats(true, elapsed);
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadPointLatest failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return std::nullopt;
    }
}

BatchReadResult DataReader::ReadPointsBatch(const std::vector<int>& point_ids) {
    auto start = std::chrono::steady_clock::now();
    BatchReadResult result;
    result.total_requested = point_ids.size();
    
    try {
        // Redis 키 생성
        std::vector<std::string> keys;
        for (int point_id : point_ids) {
            keys.push_back(RedisKeyBuilder::PointLatestKey(point_id));
        }
        
        // MGET으로 배치 읽기
        auto values = redis_->mget(keys);
        
        // 결과 파싱
        for (size_t i = 0; i < values.size(); i++) {
            if (!values[i].empty()) {
                auto value = parseCurrentValue(values[i]);
                if (value) {
                    result.values.push_back(*value);
                    result.successful++;
                } else {
                    result.failed_point_ids.push_back(point_ids[i]);
                    result.failed++;
                }
            } else {
                result.failed_point_ids.push_back(point_ids[i]);
                result.failed++;
            }
        }
        
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        LogManager::getInstance().Info(
            "DataReader::ReadPointsBatch: " + std::to_string(result.successful) + 
            "/" + std::to_string(result.total_requested) + 
            " in " + std::to_string(result.elapsed_ms) + "ms");
        
        updateStats(true, result.elapsed_ms, true);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadPointsBatch failed: " + std::string(e.what()));
        
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, result.elapsed_ms, true);
    }
    
    return result;
}

// =============================================================================
// 가상포인트 읽기 (virtualpoint:{id})
// =============================================================================

std::optional<VirtualPointValue> DataReader::ReadVirtualPoint(int point_id) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::VirtualPointKey(point_id);
        std::string json_str = readWithRetry(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (json_str.empty()) {
            updateStats(false, elapsed);
            return std::nullopt;
        }
        
        auto result = parseVirtualPoint(json_str);
        updateStats(true, elapsed);
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadVirtualPoint failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return std::nullopt;
    }
}

std::vector<VirtualPointValue> DataReader::ReadVirtualPointsBatch(
    const std::vector<int>& point_ids) {
    auto start = std::chrono::steady_clock::now();
    std::vector<VirtualPointValue> results;
    
    try {
        // Redis 키 생성
        std::vector<std::string> keys;
        for (int point_id : point_ids) {
            keys.push_back(RedisKeyBuilder::VirtualPointKey(point_id));
        }
        
        // MGET으로 배치 읽기
        auto values = redis_->mget(keys);
        
        // 결과 파싱
        for (const auto& json_str : values) {
            if (!json_str.empty()) {
                auto value = parseVirtualPoint(json_str);
                if (value) {
                    results.push_back(*value);
                }
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(!results.empty(), elapsed, true);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadVirtualPointsBatch failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed, true);
    }
    
    return results;
}

// =============================================================================
// 알람 읽기 (alarm:active:{rule_id})
// =============================================================================

std::optional<ActiveAlarm> DataReader::ReadActiveAlarm(int rule_id) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::ActiveAlarmKey(rule_id);
        std::string json_str = readWithRetry(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (json_str.empty()) {
            updateStats(false, elapsed);
            return std::nullopt;
        }
        
        auto result = parseActiveAlarm(json_str);
        updateStats(true, elapsed);
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadActiveAlarm failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return std::nullopt;
    }
}

std::vector<ActiveAlarm> DataReader::ReadAllActiveAlarms() {
    auto start = std::chrono::steady_clock::now();
    std::vector<ActiveAlarm> results;
    
    try {
        auto keys = FindKeys("alarm:active:*");
        
        for (const auto& key : keys) {
            std::string json_str = readWithRetry(key);
            if (!json_str.empty()) {
                auto alarm = parseActiveAlarm(json_str);
                if (alarm) {
                    results.push_back(*alarm);
                }
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        LogManager::getInstance().Info(
            "DataReader::ReadAllActiveAlarms: Found " + 
            std::to_string(results.size()) + " alarms");
        
        updateStats(!results.empty(), elapsed);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadAllActiveAlarms failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
    }
    
    return results;
}

int DataReader::ReadAlarmCount() {
    auto start = std::chrono::steady_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::AlarmCountKey();
        std::string count_str = redis_->get(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (count_str.empty()) {
            updateStats(false, elapsed);
            return 0;
        }
        
        int count = std::stoi(count_str);
        updateStats(true, elapsed);
        
        return count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::ReadAlarmCount failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return 0;
    }
}

// =============================================================================
// Pub/Sub 구독
// =============================================================================

bool DataReader::SubscribeToAlarms(
    std::function<void(const ActiveAlarm&)> callback,
    const std::string& channel) {
    
    try {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        
        // 이미 구독 중이면 false 반환
        if (is_subscribed_.load()) {
            LogManager::getInstance().Warn("DataReader: Already subscribed");
            return false;
        }
        
        // 구독 스레드 생성
        auto thread = std::make_unique<std::thread>(
            &DataReader::subscriptionThreadFunc,
            this,
            channel,
            [callback](const std::string& ch, const std::string& message) {
                (void)ch;  // unused parameter 경고 억제
                auto alarm = ActiveAlarm::fromJson(nlohmann::json::parse(message));
                if (alarm) {
                    callback(*alarm);
                }
            }
        );
        
        subscription_threads_.push_back(std::move(thread));
        is_subscribed_.store(true);
        
        LogManager::getInstance().Info("DataReader: Subscribed to " + channel);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::SubscribeToAlarms failed: " + std::string(e.what()));
        return false;
    }
}

void DataReader::UnsubscribeAll() {
    std::lock_guard<std::mutex> lock(subscription_mutex_);
    
    // ✅ 수정: 스레드 종료 먼저, 플래그는 나중에
    for (auto& thread : subscription_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    
    subscription_threads_.clear();
    is_subscribed_.store(false);
    
    LogManager::getInstance().Info("DataReader: Unsubscribed all");
}

bool DataReader::IsSubscribed() const {
    return is_subscribed_.load();
}

// =============================================================================
// 캐시 관리
// =============================================================================

void DataReader::EnableCache(bool enable) {
    options_.enable_cache = enable;
    if (!enable) {
        ClearCache();
    }
    LogManager::getInstance().Info("DataReader: Cache " + 
                                  std::string(enable ? "enabled" : "disabled"));
}

void DataReader::ClearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    cache_hit_count_.store(0);
    cache_miss_count_.store(0);
    LogManager::getInstance().Info("DataReader: Cache cleared");
}

DataReader::CacheStats DataReader::GetCacheStats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    CacheStats stats;
    stats.size = cache_.size();
    stats.hit_count = cache_hit_count_.load();
    stats.miss_count = cache_miss_count_.load();
    
    int total = stats.hit_count + stats.miss_count;
    stats.hit_rate = total > 0 ? static_cast<double>(stats.hit_count) / total : 0.0;
    
    return stats;
}

// =============================================================================
// 유틸리티
// =============================================================================

bool DataReader::KeyExists(const std::string& key) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        bool exists = redis_->exists(key);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(true, elapsed);
        
        return exists;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::KeyExists failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return false;
    }
}

std::vector<std::string> DataReader::FindKeys(const std::string& pattern) {
    auto start = std::chrono::steady_clock::now();
    
    try {
        auto keys = redis_->keys(pattern);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(!keys.empty(), elapsed);
        
        return keys;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::FindKeys failed: " + std::string(e.what()));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        updateStats(false, elapsed);
        return {};
    }
}

DataReader::Statistics DataReader::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void DataReader::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics{};
    LogManager::getInstance().Info("DataReader: 통계 초기화 완료");
}

bool DataReader::IsConnected() const {
    return redis_ && redis_->isConnected();
}

// =============================================================================
// 내부 메서드들
// =============================================================================

std::optional<CurrentValue> DataReader::parseCurrentValue(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);
        return CurrentValue::fromJson(j);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::parseCurrentValue failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<VirtualPointValue> DataReader::parseVirtualPoint(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);
        return VirtualPointValue::fromJson(j);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::parseVirtualPoint failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<ActiveAlarm> DataReader::parseActiveAlarm(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);
        return ActiveAlarm::fromJson(j);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::parseActiveAlarm failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

int DataReader::extractDeviceNumber(const std::string& device_id) const {
    try {
        // "device_001" → 1
        size_t pos = device_id.find_last_of('_');
        if (pos != std::string::npos) {
            return std::stoi(device_id.substr(pos + 1));
        }
        // "1" → 1
        return std::stoi(device_id);
    } catch (...) {
        return 0;
    }
}

std::string DataReader::readWithRetry(const std::string& key) {
    int retry_count = 0;
    
    while (retry_count < options_.retry_count) {
        try {
            std::string value = redis_->get(key);
            return value;
            
        } catch (const std::exception& e) {
            retry_count++;
            
            if (retry_count >= options_.retry_count) {
                LogManager::getInstance().Error(
                    "DataReader::readWithRetry final failure after " + 
                    std::to_string(retry_count) + " retries: " + 
                    std::string(e.what()));
                throw;
            }
            
            // 재시도 대기
            std::this_thread::sleep_for(
                std::chrono::milliseconds(options_.retry_delay_ms));
        }
    }
    
    return "";
}

std::optional<std::string> DataReader::getFromCache(const std::string& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    
    // TTL 확인
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        now - it->second.timestamp).count();
    
    if (age > options_.cache_ttl_seconds) {
        cache_.erase(it);
        return std::nullopt;
    }
    
    return it->second.value;
}

void DataReader::putToCache(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // 캐시 크기 제한
    if (cache_.size() >= options_.max_cache_size) {
        cleanExpiredCache();
        
        // 여전히 초과하면 가장 오래된 항목 제거
        if (cache_.size() >= options_.max_cache_size) {
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.timestamp < oldest->second.timestamp) {
                    oldest = it;
                }
            }
            cache_.erase(oldest);
        }
    }
    
    CacheEntry entry;
    entry.value = value;
    entry.timestamp = std::chrono::steady_clock::now();
    
    cache_[key] = entry;
}

void DataReader::cleanExpiredCache() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = cache_.begin(); it != cache_.end();) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.timestamp).count();
        
        if (age > options_.cache_ttl_seconds) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void DataReader::subscriptionThreadFunc(
    const std::string& channel,
    std::function<void(const std::string&, const std::string&)> callback) {
    
    try {
        LogManager::getInstance().Info("DataReader: Subscription thread started for " + channel);
        
        redis_->subscribe(channel);
        redis_->setMessageCallback(
            [callback](const std::string& ch, const std::string& msg) {
                callback(ch, msg);
            }
        );
        
        LogManager::getInstance().Info("DataReader: Subscription thread ended for " + channel);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DataReader::subscriptionThreadFunc failed: " + std::string(e.what()));
    }
}

void DataReader::updateStats(bool success, double elapsed_ms, bool is_batch) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_reads++;
    
    if (success) {
        stats_.successful_reads++;
    } else {
        stats_.failed_reads++;
    }
    
    if (is_batch) {
        stats_.batch_reads++;
    }
    
    if (options_.enable_cache) {
        stats_.cache_hits = cache_hit_count_.load();
        stats_.cache_misses = cache_miss_count_.load();
    }
    
    // 평균 시간 계산
    if (elapsed_ms > 0) {
        stats_.avg_read_time_ms = 
            (stats_.avg_read_time_ms * (stats_.total_reads - 1) + elapsed_ms) 
            / stats_.total_reads;
    }
}

} // namespace Data
} // namespace Shared
} // namespace PulseOne