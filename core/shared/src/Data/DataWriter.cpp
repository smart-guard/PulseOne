// =============================================================================
// core/shared/src/Data/DataWriter.cpp
// Redis 데이터 쓰기 전용 클래스 구현 (Export Gateway용)
// =============================================================================

// ✅ 핵심: 헤더 include 순서 중요!
#include "Data/DataWriter.h"           // 자신의 헤더
#include "Client/RedisClient.h"        // RedisClient 전체 정의 (필수!)
#include "Logging/LogManager.h"          // LogManager

#include <chrono>
#include <sstream>
#include <iomanip>

using namespace std::chrono;
using json = nlohmann::json;

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DataWriter::DataWriter(std::shared_ptr<RedisClient> redis,
                       const DataWriterOptions& options)
    : redis_(redis), options_(options) {
    
    if (!redis_) {
        LogManager::getInstance().Error("DataWriter: Redis client is null");
        return;
    }
    
    // 통계 초기화
    stats_ = {};
    
    // 비동기 쓰기 스레드 시작
    if (options_.enable_async_write) {
        async_thread_ = std::make_unique<std::thread>(
            &DataWriter::asyncWriteThreadFunc, this);
        LogManager::getInstance().Info("DataWriter: 비동기 쓰기 스레드 시작됨");
    }
    
    LogManager::getInstance().Info("DataWriter: 초기화 완료");
}

DataWriter::~DataWriter() {
    // 비동기 스레드 정리
    running_ = false;
    async_cv_.notify_all();
    
    if (async_thread_ && async_thread_->joinable()) {
        async_thread_->join();
    }
    
    // 남은 작업 플러시
    FlushBatch();
    
    LogManager::getInstance().Info("DataWriter: 종료 완료");
}

// =============================================================================
// Export 로그 쓰기
// =============================================================================

bool DataWriter::WriteExportLog(const ExportLogEntry& log, bool async) {
    auto start = high_resolution_clock::now();
    
    try {
        std::string key = generateLogKey(log);
        std::string value = log.toJson().dump();
        
        bool success;
        if (async && options_.enable_async_write) {
            success = enqueueWrite(key, value, options_.log_ttl_seconds);
        } else {
            success = writeWithRetry(key, value, options_.log_ttl_seconds);
        }
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "log_write");
        
        if (success) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.log_writes++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::WriteExportLog failed: " + 
                                       std::string(e.what()));
        updateStats(false, 0, "log_write");
        return false;
    }
}

// =============================================================================
// 서비스 상태 관리
// =============================================================================

bool DataWriter::UpdateServiceStatus(const ServiceStatus& status) {
    auto start = high_resolution_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::ServiceStatusKey(status.service_id);
        std::string value = status.toJson().dump();
        
        bool success = writeWithRetry(key, value, options_.default_ttl_seconds);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "status_update");
        
        if (success) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.status_updates++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::UpdateServiceStatus failed: " + 
                                       std::string(e.what()));
        updateStats(false, 0, "status_update");
        return false;
    }
}

bool DataWriter::RecordServiceStart(int service_id, 
                                    const std::string& service_type,
                                    const std::string& service_name) {
    ServiceStatus status;
    status.service_id = service_id;
    status.service_type = service_type;
    status.service_name = service_name;
    status.is_running = true;
    status.active_connections = 0;
    status.total_requests = 0;
    status.successful_requests = 0;
    status.failed_requests = 0;
    status.last_request_at = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    bool success = UpdateServiceStatus(status);
    
    if (success) {
        LogManager::getInstance().Info("DataWriter: 서비스 시작 기록 - " + service_name);
    }
    
    return success;
}

bool DataWriter::RecordServiceStop(int service_id) {
    auto start = high_resolution_clock::now();
    
    try {
        // 기존 상태 읽기
        std::string key = RedisKeyBuilder::ServiceStatusKey(service_id);
        std::string existing = redis_->get(key);
        
        if (existing.empty()) {
            return false;
        }
        
        auto j = json::parse(existing);
        j["is_running"] = false;
        j["last_request_at"] = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        
        bool success = writeWithRetry(key, j.dump(), options_.default_ttl_seconds);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "status_update");
        
        if (success) {
            LogManager::getInstance().Info("DataWriter: 서비스 중지 기록 - service_id=" + 
                                         std::to_string(service_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::RecordServiceStop failed: " + 
                                       std::string(e.what()));
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        updateStats(false, elapsed, "status_update");
        return false;
    }
}

bool DataWriter::IncrementServiceRequest(int service_id, bool success, 
                                        int response_time_ms) {
    auto start = high_resolution_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::ServiceStatusKey(service_id);
        std::string existing = redis_->get(key);
        
        if (existing.empty()) {
            return false;
        }
        
        auto j = json::parse(existing);
        j["total_requests"] = j.value("total_requests", 0) + 1;
        
        if (success) {
            j["successful_requests"] = j.value("successful_requests", 0) + 1;
        } else {
            j["failed_requests"] = j.value("failed_requests", 0) + 1;
        }
        
        j["last_request_at"] = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        
        bool write_success = writeWithRetry(key, j.dump(), options_.default_ttl_seconds);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(write_success, elapsed, "status_update");
        
        return write_success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::IncrementServiceRequest failed: " + 
                                       std::string(e.what()));
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        updateStats(false, elapsed, "status_update");
        return false;
    }
}

// =============================================================================
// 카운터 관리
// =============================================================================

int DataWriter::IncrementCounter(const std::string& counter_name, int increment) {
    auto start = high_resolution_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::ExportCounterKey(counter_name);
        
        // Redis INCRBY 명령 사용 (한 번에 처리)
        int new_value = redis_->incr(key, increment);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(true, elapsed, "counter_increment");
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.counter_increments++;
        
        return new_value;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::IncrementCounter failed: " + 
                                       std::string(e.what()));
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        updateStats(false, elapsed, "counter_increment");
        return -1;
    }
}

bool DataWriter::SetCounter(const std::string& counter_name, int value) {
    auto start = high_resolution_clock::now();
    
    try {
        std::string key = RedisKeyBuilder::ExportCounterKey(counter_name);
        bool success = redis_->set(key, std::to_string(value));
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "counter_set");
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::SetCounter failed: " + 
                                       std::string(e.what()));
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        updateStats(false, elapsed, "counter_set");
        return false;
    }
}

bool DataWriter::ResetCounter(const std::string& counter_name) {
    return SetCounter(counter_name, 0);
}

// =============================================================================
// 에러 로깅
// =============================================================================

bool DataWriter::WriteError(const std::string& service_name,
                           const std::string& error_message,
                           const std::string& error_code,
                           const std::string& details) {
    auto start = high_resolution_clock::now();
    
    try {
        json error_log;
        error_log["service_name"] = service_name;
        error_log["error_message"] = error_message;
        error_log["error_code"] = error_code;
        error_log["details"] = details;
        error_log["timestamp"] = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        
        // 에러 로그 키: error:{service_name}:{timestamp}
        auto now_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        std::string key = "error:" + service_name + ":" + std::to_string(now_ms);
        
        // 24시간 TTL
        bool success = writeWithRetry(key, error_log.dump(), 86400);
        
        // 에러 카운터 증가
        IncrementCounter("errors:" + service_name);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "error_write");
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::WriteError failed: " + 
                                       std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 일반 데이터 쓰기
// =============================================================================

bool DataWriter::Write(const std::string& key, const std::string& value, 
                       int ttl_seconds) {
    auto start = high_resolution_clock::now();
    
    int ttl = (ttl_seconds > 0) ? ttl_seconds : options_.default_ttl_seconds;
    bool success = writeWithRetry(key, value, ttl);
    
    auto elapsed = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count() / 1000.0;
    
    updateStats(success, elapsed, "write");
    
    return success;
}

bool DataWriter::WriteJson(const std::string& key, const json& j, 
                          int ttl_seconds) {
    return Write(key, j.dump(), ttl_seconds);
}

bool DataWriter::Delete(const std::string& key) {
    auto start = high_resolution_clock::now();
    
    try {
        bool success = redis_->del(key) > 0;
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(success, elapsed, "delete");
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::Delete failed: " + 
                                       std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Pub/Sub 발행
// =============================================================================

bool DataWriter::Publish(const std::string& channel, const std::string& message) {
    auto start = high_resolution_clock::now();
    
    try {
        int subscribers = redis_->publish(channel, message);
        
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        
        updateStats(true, elapsed, "publish");
        
        LogManager::getInstance().Debug("DataWriter: 메시지 발행 - " + channel + 
                                       " (구독자: " + std::to_string(subscribers) + ")");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DataWriter::Publish failed: " + 
                                       std::string(e.what()));
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count() / 1000.0;
        updateStats(false, elapsed, "publish");
        return false;
    }
}

bool DataWriter::PublishJson(const std::string& channel, const json& j) {
    return Publish(channel, j.dump());
}

// =============================================================================
// 배치 처리
// =============================================================================

int DataWriter::FlushBatch() {
    std::lock_guard<std::mutex> lock(async_mutex_);
    
    int flushed = 0;
    
    while (!async_queue_.empty()) {
        auto& task = async_queue_.front();
        
        if (writeWithRetry(task.key, task.value, task.ttl_seconds)) {
            flushed++;
        }
        
        async_queue_.pop();
    }
    
    if (flushed > 0) {
        LogManager::getInstance().Info("DataWriter: 배치 플러시 완료 - " + 
                                      std::to_string(flushed) + " items");
    }
    
    return flushed;
}

bool DataWriter::WaitForPendingWrites(int timeout_ms) {
    auto start = high_resolution_clock::now();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(async_mutex_);
            if (async_queue_.empty()) {
                return true;
            }
        }
        
        auto elapsed = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        if (elapsed >= timeout_ms) {
            LogManager::getInstance().Warn("DataWriter::WaitForPendingWrites timeout: " + 
                                          std::to_string(GetPendingQueueSize()) + 
                                          " items remaining");
            return false;
        }
        
        std::this_thread::sleep_for(milliseconds(10));
    }
}

// =============================================================================
// 유틸리티
// =============================================================================

DataWriter::Statistics DataWriter::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void DataWriter::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
    LogManager::getInstance().Info("DataWriter: 통계 초기화 완료");
}

bool DataWriter::IsConnected() const {
    return redis_ && redis_->isConnected();
}

size_t DataWriter::GetPendingQueueSize() const {
    std::lock_guard<std::mutex> lock(async_mutex_);
    return async_queue_.size();
}

// =============================================================================
// 내부 메서드들
// =============================================================================

bool DataWriter::writeWithRetry(const std::string& key, const std::string& value, 
                                int ttl_seconds) {
    for (int attempt = 0; attempt < options_.retry_count; attempt++) {
        try {
            bool success;
            
            if (ttl_seconds > 0) {
                success = redis_->setex(key, value, ttl_seconds);
            } else {
                success = redis_->set(key, value);
            }
            
            if (success) {
                return true;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("DataWriter::writeWithRetry attempt " + 
                                          std::to_string(attempt + 1) + 
                                          " failed: " + std::string(e.what()));
        }
        
        if (attempt < options_.retry_count - 1) {
            std::this_thread::sleep_for(milliseconds(options_.retry_delay_ms));
        }
    }
    
    LogManager::getInstance().Error("DataWriter::writeWithRetry final failure: " + key);
    return false;
}

bool DataWriter::enqueueWrite(const std::string& key, const std::string& value, 
                              int ttl_seconds) {
    std::lock_guard<std::mutex> lock(async_mutex_);
    
    if (async_queue_.size() >= static_cast<size_t>(options_.async_queue_size)) {
        LogManager::getInstance().Warn("DataWriter: 비동기 큐 가득참, 동기 쓰기로 전환");
        return writeWithRetry(key, value, ttl_seconds);
    }
    
    WriteTask task;
    task.key = key;
    task.value = value;
    task.ttl_seconds = ttl_seconds;
    
    async_queue_.push(task);
    async_cv_.notify_one();
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.async_writes++;
    
    return true;
}

void DataWriter::asyncWriteThreadFunc() {
    LogManager::getInstance().Info("DataWriter: 비동기 쓰기 스레드 시작");
    
    while (running_) {
        std::unique_lock<std::mutex> lock(async_mutex_);
        
        async_cv_.wait(lock, [this] {
            return !async_queue_.empty() || !running_;
        });
        
        if (!running_ && async_queue_.empty()) {
            break;
        }
        
        // 배치 처리
        std::vector<WriteTask> batch;
        int batch_size = std::min(static_cast<int>(async_queue_.size()), 
                                 options_.batch_size);
        
        for (int i = 0; i < batch_size && !async_queue_.empty(); i++) {
            batch.push_back(async_queue_.front());
            async_queue_.pop();
        }
        
        lock.unlock();
        
        // 배치 쓰기 실행
        for (const auto& task : batch) {
            writeWithRetry(task.key, task.value, task.ttl_seconds);
        }
    }
    
    LogManager::getInstance().Info("DataWriter: 비동기 쓰기 스레드 종료");
}

void DataWriter::updateStats(bool success, double elapsed_ms, 
                             const std::string& operation_type) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_writes++;
    
    if (success) {
        stats_.successful_writes++;
    } else {
        stats_.failed_writes++;
    }
    
    // 평균 쓰기 시간 업데이트 (이동 평균)
    stats_.avg_write_time_ms = 
        (stats_.avg_write_time_ms * (stats_.total_writes - 1) + elapsed_ms) / 
        stats_.total_writes;
}

std::string DataWriter::generateLogKey(const ExportLogEntry& log) {
    return RedisKeyBuilder::ExportLogKey(log.target_id, log.timestamp);
}

} // namespace Data
} // namespace Shared
} // namespace PulseOne