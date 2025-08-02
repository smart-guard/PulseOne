// =============================================================================
// collector/src/Drivers/Bacnet/BACnetStatisticsManager.cpp
// 🔥 BACnet 통계 관리자 구현
// =============================================================================

#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 초기화
// =============================================================================

BACnetStatisticsManager::BACnetStatisticsManager() {
    cumulative_stats_.start_time = std::chrono::system_clock::now();
    cumulative_stats_.last_update_time = cumulative_stats_.start_time;
    last_standard_update_ = cumulative_stats_.start_time;
    last_bacnet_update_ = cumulative_stats_.start_time;
    last_snapshot_time_ = cumulative_stats_.start_time;
    
    // 초기 통계 캐시 생성
    standard_statistics_cache_ = std::make_unique<DriverStatistics>();
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔢 BACnet Statistics Manager initialized");
}

// =============================================================================
// 통계 업데이트 메서드들
// =============================================================================

void BACnetStatisticsManager::UpdateReadStatistics(size_t total_points, 
                                                   size_t successful_points, 
                                                   std::chrono::milliseconds duration) {
    // 기본 통계 업데이트
    cumulative_stats_.total_read_requests.fetch_add(total_points);
    cumulative_stats_.successful_reads.fetch_add(successful_points);
    cumulative_stats_.failed_reads.fetch_add(total_points - successful_points);
    cumulative_stats_.total_read_time_ms.fetch_add(duration.count());
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
    
    // 응답 시간 추가
    if (total_points > 0) {
        auto avg_time = std::chrono::milliseconds(duration.count() / total_points);
        AddResponseTime(avg_time);
    }
    
    // BACnet 특화 통계
    {
        std::lock_guard<std::mutex> lock(bacnet_stats_mutex_);
        bacnet_statistics_cache_.total_read_requests.fetch_add(total_points);
        bacnet_statistics_cache_.successful_reads.fetch_add(successful_points);
        bacnet_statistics_cache_.failed_reads.fetch_add(total_points - successful_points);
        bacnet_statistics_cache_.read_property_requests.fetch_add(total_points);
        bacnet_statistics_cache_.total_response_time_ms.fetch_add(duration.count());
        
        // 최대/최소 응답 시간 업데이트
        uint64_t current_time = duration.count();
        uint64_t current_max = bacnet_statistics_cache_.max_response_time_ms.load();
        while (current_time > current_max && 
               !bacnet_statistics_cache_.max_response_time_ms.compare_exchange_weak(current_max, current_time)) {
        }
        
        uint64_t current_min = bacnet_statistics_cache_.min_response_time_ms.load();
        while (current_time < current_min && current_time > 0 &&
               !bacnet_statistics_cache_.min_response_time_ms.compare_exchange_weak(current_min, current_time)) {
        }
        
        bacnet_statistics_cache_.last_update = std::chrono::system_clock::now();
    }
    
    // 최근 작업 시간 기록
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        auto now = std::chrono::system_clock::now();
        for (size_t i = 0; i < total_points; ++i) {
            recent_operations_.push_back(now);
        }
        CleanupRecentData();
    }
}

void BACnetStatisticsManager::UpdateWriteStatistics(size_t total_points, 
                                                    size_t successful_points,
                                                    std::chrono::milliseconds duration) {
    // 기본 통계 업데이트
    cumulative_stats_.total_write_requests.fetch_add(total_points);
    cumulative_stats_.successful_writes.fetch_add(successful_points);
    cumulative_stats_.failed_writes.fetch_add(total_points - successful_points);
    cumulative_stats_.total_write_time_ms.fetch_add(duration.count());
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
    
    // 응답 시간 추가
    if (total_points > 0) {
        auto avg_time = std::chrono::milliseconds(duration.count() / total_points);
        AddResponseTime(avg_time);
    }
    
    // BACnet 특화 통계
    {
        std::lock_guard<std::mutex> lock(bacnet_stats_mutex_);
        bacnet_statistics_cache_.total_write_requests.fetch_add(total_points);
        bacnet_statistics_cache_.successful_writes.fetch_add(successful_points);
        bacnet_statistics_cache_.failed_writes.fetch_add(total_points - successful_points);
        bacnet_statistics_cache_.write_property_requests.fetch_add(total_points);
        bacnet_statistics_cache_.total_response_time_ms.fetch_add(duration.count());
        bacnet_statistics_cache_.last_update = std::chrono::system_clock::now();
    }
    
    // 최근 작업 시간 기록
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        auto now = std::chrono::system_clock::now();
        for (size_t i = 0; i < total_points; ++i) {
            recent_operations_.push_back(now);
        }
        CleanupRecentData();
    }
}

void BACnetStatisticsManager::UpdateNetworkStatistics() {
    // 주기적으로 호출되는 네트워크 통계 업데이트
    auto now = std::chrono::system_clock::now();
    
    // 성능 스냅샷 생성 (1분마다)
    if ((now - last_snapshot_time_) >= SNAPSHOT_INTERVAL) {
        CreatePerformanceSnapshot();
        last_snapshot_time_ = now;
    }
    
    // 최근 데이터 정리
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        CleanupRecentData();
    }
}

// =============================================================================
// 통계 조회 메서드들
// =============================================================================

const DriverStatistics& BACnetStatisticsManager::GetStandardStatistics() const {
    if (IsCacheExpired(last_standard_update_)) {
        UpdateStandardStatisticsCache();
    }
    
    std::lock_guard<std::mutex> lock(standard_stats_mutex_);
    return *standard_statistics_cache_;
}

const BACnetStatistics& BACnetStatisticsManager::GetBACnetStatistics() const {
    if (IsCacheExpired(last_bacnet_update_)) {
        UpdateBACnetStatisticsCache();
    }
    
    std::lock_guard<std::mutex> lock(bacnet_stats_mutex_);
    return bacnet_statistics_cache_;
}

std::vector<BACnetStatisticsManager::PerformanceSnapshot> 
BACnetStatisticsManager::GetPerformanceHistory(std::chrono::minutes duration) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    std::vector<PerformanceSnapshot> result;
    auto cutoff_time = std::chrono::system_clock::now() - duration;
    
    for (const auto& snapshot : performance_history_) {
        if (snapshot.timestamp >= cutoff_time) {
            result.push_back(snapshot);
        }
    }
    
    return result;
}

PerformanceMetrics BACnetStatisticsManager::GetCurrentPerformance() const {
    PerformanceMetrics metrics;
    metrics.measurement_time = std::chrono::system_clock::now();
    
    // 기본 지표 계산
    uint64_t total_reads = cumulative_stats_.total_read_requests.load();
    uint64_t successful_reads = cumulative_stats_.successful_reads.load();
    uint64_t total_writes = cumulative_stats_.total_write_requests.load();
    uint64_t successful_writes = cumulative_stats_.successful_writes.load();
    uint64_t total_ops = total_reads + total_writes;
    uint64_t successful_ops = successful_reads + successful_writes;
    
    // 성공률 계산
    metrics.success_rate_percent = CalculateSuccessRate(successful_ops, total_ops);
    
    // 에러율 계산
    uint64_t total_errors = cumulative_stats_.total_errors.load();
    metrics.error_rate_percent = total_ops > 0 ? 
        (static_cast<double>(total_errors) / total_ops) * 100.0 : 0.0;
    
    // 평균 응답 시간
    metrics.avg_response_time_ms = CalculateAverageResponseTime();
    
    // 초당 작업 수
    auto runtime = GetRuntime();
    metrics.current_ops_per_second = runtime.count() > 0 ? 
        static_cast<double>(total_ops) / runtime.count() : 0.0;
    
    // 연결 상태
    metrics.active_connections = cumulative_stats_.is_connected.load() ? 1 : 0;
    metrics.pending_requests = 0; // TODO: 실제 펜딩 요청 수 추가
    
    // 최근 성능 (최근 1분간)
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto one_minute_ago = now - std::chrono::minutes(1);
        
        // 최근 1분간 작업 수 계산
        size_t recent_ops_count = 0;
        for (auto it = recent_operations_.rbegin(); it != recent_operations_.rend(); ++it) {
            if (*it >= one_minute_ago) {
                recent_ops_count++;
            } else {
                break;
            }
        }
        
        metrics.recent_ops_per_second = static_cast<double>(recent_ops_count) / 60.0;
        
        // 최근 평균 응답 시간
        if (!recent_response_times_.empty()) {
            uint64_t total_time = 0;
            for (const auto& time : recent_response_times_) {
                total_time += time.count();
            }
            metrics.recent_avg_response_time_ms = 
                static_cast<double>(total_time) / recent_response_times_.size();
        }
        
        // 최근 성공률 (간단히 전체 성공률로 근사)
        metrics.recent_success_rate_percent = metrics.success_rate_percent;
    }
    
    return metrics;
}

std::map<std::string, uint64_t> BACnetStatisticsManager::GetErrorAnalysis() const {
    std::lock_guard<std::mutex> lock(error_analysis_mutex_);
    
    std::map<std::string, uint64_t> result;
    for (const auto& pair : error_counts_by_type_) {
        result[pair.first] = pair.second.load();
    }
    
    return result;
}

// =============================================================================
// 관리 메서드들
// =============================================================================

void BACnetStatisticsManager::Reset() {
    auto& logger = LogManager::getInstance();
    logger.Info("🔄 Resetting BACnet statistics");
    
    // 기본 통계 초기화
    cumulative_stats_ = CumulativeStats();
    cumulative_stats_.start_time = std::chrono::system_clock::now();
    cumulative_stats_.last_update_time = cumulative_stats_.start_time;
    
    // 캐시 초기화
    {
        std::lock_guard<std::mutex> lock(standard_stats_mutex_);
        standard_statistics_cache_ = std::make_unique<DriverStatistics>();
        last_standard_update_ = cumulative_stats_.start_time;
    }
    
    {
        std::lock_guard<std::mutex> lock(bacnet_stats_mutex_);
        bacnet_statistics_cache_ = BACnetStatistics();
        last_bacnet_update_ = cumulative_stats_.start_time;
    }
    
    // 히스토리 초기화
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        performance_history_.clear();
        last_snapshot_time_ = cumulative_stats_.start_time;
    }
    
    // 에러 분석 초기화
    {
        std::lock_guard<std::mutex> lock(error_analysis_mutex_);
        error_counts_by_type_.clear();
    }
    
    // 성능 데이터 초기화
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        recent_response_times_.clear();
        recent_operations_.clear();
    }
}

void BACnetStatisticsManager::CleanupHistory(std::chrono::hours max_age) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    auto cutoff_time = std::chrono::system_clock::now() - max_age;
    
    performance_history_.erase(
        std::remove_if(performance_history_.begin(), performance_history_.end(),
            [cutoff_time](const PerformanceSnapshot& snapshot) {
                return snapshot.timestamp < cutoff_time;
            }),
        performance_history_.end()
    );
    
    auto& logger = LogManager::getInstance();
    logger.Debug("🧹 Cleaned up statistics history, " + 
                std::to_string(performance_history_.size()) + " entries remaining");
}

std::string BACnetStatisticsManager::ExportToJson() const {
#ifdef HAS_NLOHMANN_JSON
    json j;
    
    // 기본 통계
    j["basic_stats"] = {
        {"total_read_requests", cumulative_stats_.total_read_requests.load()},
        {"successful_reads", cumulative_stats_.successful_reads.load()},
        {"failed_reads", cumulative_stats_.failed_reads.load()},
        {"total_write_requests", cumulative_stats_.total_write_requests.load()},
        {"successful_writes", cumulative_stats_.successful_writes.load()},
        {"failed_writes", cumulative_stats_.failed_writes.load()},
        {"connection_attempts", cumulative_stats_.connection_attempts.load()},
        {"successful_connections", cumulative_stats_.successful_connections.load()},
        {"total_errors", cumulative_stats_.total_errors.load()}
    };
    
    // BACnet 특화 통계
    const auto& bacnet_stats = GetBACnetStatistics();
    j["bacnet_stats"] = {
        {"who_is_sent", bacnet_stats.who_is_sent.load()},
        {"i_am_received", bacnet_stats.i_am_received.load()},
        {"read_property_requests", bacnet_stats.read_property_requests.load()},
        {"write_property_requests", bacnet_stats.write_property_requests.load()},
        {"cov_subscriptions", bacnet_stats.cov_subscriptions.load()},
        {"devices_discovered", bacnet_stats.devices_discovered.load()}
    };
    
    // 성능 지표
    auto metrics = GetCurrentPerformance();
    j["performance"] = {
        {"current_ops_per_second", metrics.current_ops_per_second},
        {"avg_response_time_ms", metrics.avg_response_time_ms},
        {"success_rate_percent", metrics.success_rate_percent},
        {"error_rate_percent", metrics.error_rate_percent},
        {"active_connections", metrics.active_connections}
    };
    
    // 에러 분석
    j["error_analysis"] = GetErrorAnalysis();
    
    // 시간 정보
    auto now = std::chrono::system_clock::now();
    auto start_time_t = std::chrono::system_clock::to_time_t(cumulative_stats_.start_time);
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    j["time_info"] = {
        {"start_time", start_time_t},
        {"current_time", now_time_t},
        {"runtime_seconds", GetRuntime().count()}
    };
    
    return j.dump(2);
#else
    // JSON 라이브러리가 없는 경우 간단한 문자열 형태로 반환
    std::ostringstream oss;
    oss << "BACnet Statistics Export:\n";
    oss << "========================\n";
    oss << "Total Read Requests: " << cumulative_stats_.total_read_requests.load() << "\n";
    oss << "Successful Reads: " << cumulative_stats_.successful_reads.load() << "\n";
    oss << "Total Write Requests: " << cumulative_stats_.total_write_requests.load() << "\n";
    oss << "Successful Writes: " << cumulative_stats_.successful_writes.load() << "\n";
    oss << "Connection Attempts: " << cumulative_stats_.connection_attempts.load() << "\n";
    oss << "Total Errors: " << cumulative_stats_.total_errors.load() << "\n";
    oss << "Runtime: " << GetRuntime().count() << " seconds\n";
    
    auto metrics = GetCurrentPerformance();
    oss << "Current OPS: " << std::fixed << std::setprecision(2) 
        << metrics.current_ops_per_second << "\n";
    oss << "Avg Response Time: " << std::fixed << std::setprecision(2) 
        << metrics.avg_response_time_ms << " ms\n";
    oss << "Success Rate: " << std::fixed << std::setprecision(2) 
        << metrics.success_rate_percent << "%\n";
    
    return oss.str();
#endif
}

// =============================================================================
// 비공개 헬퍼 메서드들
// =============================================================================

void BACnetStatisticsManager::UpdateStandardStatisticsCache() const {
    std::lock_guard<std::mutex> lock(standard_stats_mutex_);
    
    if (!standard_statistics_cache_) {
        standard_statistics_cache_ = std::make_unique<DriverStatistics>();
    }
    
    // 기본 통계 업데이트 (DriverStatistics 구조체 호환)
    standard_statistics_cache_->total_reads = cumulative_stats_.total_read_requests.load();
    standard_statistics_cache_->total_writes = cumulative_stats_.total_write_requests.load();
    standard_statistics_cache_->successful_reads = cumulative_stats_.successful_reads.load();
    standard_statistics_cache_->successful_writes = cumulative_stats_.successful_writes.load();
    standard_statistics_cache_->failed_reads = cumulative_stats_.failed_reads.load();
    standard_statistics_cache_->failed_writes = cumulative_stats_.failed_writes.load();
    
    standard_statistics_cache_->total_operations = 
        cumulative_stats_.total_read_requests.load() + cumulative_stats_.total_write_requests.load();
    standard_statistics_cache_->successful_operations = 
        cumulative_stats_.successful_reads.load() + cumulative_stats_.successful_writes.load();
    standard_statistics_cache_->failed_operations = 
        cumulative_stats_.failed_reads.load() + cumulative_stats_.failed_writes.load();
    
    standard_statistics_cache_->successful_connections = cumulative_stats_.successful_connections.load();
    standard_statistics_cache_->failed_connections = cumulative_stats_.connection_failures.load();
    
    // 성능 지표 계산
    uint64_t total_time = cumulative_stats_.total_read_time_ms.load() + 
                         cumulative_stats_.total_write_time_ms.load();
    double avg_time = standard_statistics_cache_->total_operations > 0 ? 
        static_cast<double>(total_time) / standard_statistics_cache_->total_operations : 0.0;
    
    standard_statistics_cache_->avg_response_time_ms.store(avg_time);
    standard_statistics_cache_->average_response_time = std::chrono::milliseconds(static_cast<long>(avg_time));
    
    auto runtime = GetRuntime();
    if (runtime.count() > 0) {
        standard_statistics_cache_->uptime_seconds.store(static_cast<uint64_t>(runtime.count()));
    }
    
    standard_statistics_cache_->success_rate.store(
        CalculateSuccessRate(standard_statistics_cache_->successful_operations, 
                           standard_statistics_cache_->total_operations));
    
    // 시간 정보 업데이트
    standard_statistics_cache_->last_read_time = cumulative_stats_.last_update_time;
    standard_statistics_cache_->last_write_time = cumulative_stats_.last_update_time;
    
    // 마지막 업데이트 시간 갱신
    last_standard_update_ = std::chrono::system_clock::now();
}

void BACnetStatisticsManager::UpdateBACnetStatisticsCache() const {
    std::lock_guard<std::mutex> lock(bacnet_stats_mutex_);
    
    // 기본 통계를 BACnet 통계에 동기화
    bacnet_statistics_cache_.total_read_requests.store(cumulative_stats_.total_read_requests.load());
    bacnet_statistics_cache_.successful_reads.store(cumulative_stats_.successful_reads.load());
    bacnet_statistics_cache_.failed_reads.store(cumulative_stats_.failed_reads.load());
    bacnet_statistics_cache_.total_write_requests.store(cumulative_stats_.total_write_requests.load());
    bacnet_statistics_cache_.successful_writes.store(cumulative_stats_.successful_writes.load());
    bacnet_statistics_cache_.failed_writes.store(cumulative_stats_.failed_writes.load());
    
    bacnet_statistics_cache_.connection_attempts.store(cumulative_stats_.connection_attempts.load());
    bacnet_statistics_cache_.successful_connections.store(cumulative_stats_.successful_connections.load());
    bacnet_statistics_cache_.connection_failures.store(cumulative_stats_.connection_failures.load());
    
    bacnet_statistics_cache_.error_count.store(cumulative_stats_.total_errors.load());
    bacnet_statistics_cache_.timeout_errors.store(cumulative_stats_.timeout_errors.load());
    bacnet_statistics_cache_.protocol_errors.store(cumulative_stats_.protocol_errors.load());
    bacnet_statistics_cache_.communication_errors.store(cumulative_stats_.connection_errors.load());
    
    bacnet_statistics_cache_.messages_sent.store(cumulative_stats_.messages_sent.load());
    bacnet_statistics_cache_.messages_received.store(cumulative_stats_.messages_received.load());
    bacnet_statistics_cache_.bytes_sent.store(cumulative_stats_.bytes_sent.load());
    bacnet_statistics_cache_.bytes_received.store(cumulative_stats_.bytes_received.load());
    
    // 마지막 업데이트 시간 갱신
    last_bacnet_update_ = std::chrono::system_clock::now();
    bacnet_statistics_cache_.last_update = last_bacnet_update_;
}

void BACnetStatisticsManager::CreatePerformanceSnapshot() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    PerformanceSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    
    // 현재 성능 지표 계산
    uint64_t total_reads = cumulative_stats_.total_read_requests.load();
    uint64_t successful_reads = cumulative_stats_.successful_reads.load();
    uint64_t total_writes = cumulative_stats_.total_write_requests.load();
    uint64_t successful_writes = cumulative_stats_.successful_writes.load();
    uint64_t total_ops = total_reads + total_writes;
    uint64_t successful_ops = successful_reads + successful_writes;
    
    snapshot.read_success_rate = CalculateSuccessRate(successful_reads, total_reads);
    snapshot.write_success_rate = CalculateSuccessRate(successful_writes, total_writes);
    snapshot.avg_response_time_ms = CalculateAverageResponseTime();
    snapshot.messages_per_second = CalculateMessagesPerSecond();
    snapshot.active_connections = cumulative_stats_.is_connected.load() ? 1 : 0;
    
    // 에러율 계산 (분당)
    uint64_t total_errors = cumulative_stats_.total_errors.load();
    auto runtime_minutes = std::chrono::duration_cast<std::chrono::minutes>(
        snapshot.timestamp - cumulative_stats_.start_time).count();
    snapshot.error_rate_per_minute = runtime_minutes > 0 ? 
        static_cast<double>(total_errors) / runtime_minutes : 0.0;
    
    // 히스토리에 추가
    performance_history_.push_back(snapshot);
    
    // 최대 엔트리 수 제한
    while (performance_history_.size() > MAX_HISTORY_ENTRIES) {
        performance_history_.pop_front();
    }
}

void BACnetStatisticsManager::AddResponseTime(std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    
    recent_response_times_.push_back(duration);
    
    // 최대 크기 제한
    while (recent_response_times_.size() > MAX_RECENT_OPERATIONS) {
        recent_response_times_.pop_front();
    }
}

void BACnetStatisticsManager::CleanupRecentData() {
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::minutes(5); // 최근 5분간만 유지
    
    // 최근 작업 시간 정리
    while (!recent_operations_.empty() && recent_operations_.front() < cutoff) {
        recent_operations_.pop_front();
    }
    
    // 최대 크기 제한
    while (recent_operations_.size() > MAX_RECENT_OPERATIONS) {
        recent_operations_.pop_front();
    }
}

double BACnetStatisticsManager::CalculateAverageResponseTime() const {
    uint64_t total_ops = cumulative_stats_.total_read_requests.load() + 
                        cumulative_stats_.total_write_requests.load();
    uint64_t total_time = cumulative_stats_.total_read_time_ms.load() + 
                         cumulative_stats_.total_write_time_ms.load();
    
    return total_ops > 0 ? static_cast<double>(total_time) / total_ops : 0.0;
}

double BACnetStatisticsManager::CalculateMessagesPerSecond() const {
    auto runtime = GetRuntime();
    uint64_t total_messages = cumulative_stats_.messages_sent.load() + 
                             cumulative_stats_.messages_received.load();
    
    return runtime.count() > 0 ? static_cast<double>(total_messages) / runtime.count() : 0.0;
}

} // namespace Drivers
} // namespace PulseOne