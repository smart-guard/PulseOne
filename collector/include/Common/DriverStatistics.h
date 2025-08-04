// collector/include/Common/DriverStatistics.h 수정된 전체 구조

#ifndef PULSEONE_UNIFIED_DRIVER_STATISTICS_H
#define PULSEONE_UNIFIED_DRIVER_STATISTICS_H

#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include <atomic>
#include <map>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef HAS_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Structs {

// =============================================================================
// 전방 선언
// =============================================================================
struct DriverStatisticsSnapshot;

/**
 * @brief 통합 드라이버 통계 구조체
 * @details 모든 프로토콜 드라이버에서 공통으로 사용하는 표준 통계 구조
 */
struct DriverStatistics {
    // ===== 기존 필드들 (100% 호환성 유지) =====
    std::atomic<uint64_t> total_reads{0};
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> successful_reads{0};
    std::atomic<uint64_t> successful_writes{0};
    std::atomic<uint64_t> failed_reads{0};
    std::atomic<uint64_t> failed_writes{0};
    
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<uint64_t> failed_operations{0};
    
    std::atomic<double> avg_response_time_ms{0.0};
    std::atomic<double> min_response_time_ms{999999.0};
    std::atomic<double> max_response_time_ms{0.0};
    std::chrono::milliseconds average_response_time{0};
    
    std::atomic<uint64_t> successful_connections{0};
    std::atomic<uint64_t> failed_connections{0};
    std::atomic<uint32_t> consecutive_failures{0};
    std::atomic<uint64_t> uptime_seconds{0};
    std::atomic<double> success_rate{0.0};
    
    BasicTypes::Timestamp last_read_time;
    BasicTypes::Timestamp last_write_time;
    BasicTypes::Timestamp last_error_time;
    BasicTypes::Timestamp start_time;
    BasicTypes::Timestamp last_success_time;
    BasicTypes::Timestamp last_connection_time;
    
    // ===== 새로운 확장 필드들 =====
    std::map<std::string, std::atomic<uint64_t>> protocol_counters;
    std::map<std::string, std::atomic<double>> protocol_metrics;
    std::map<std::string, std::string> protocol_status;
    
    mutable std::mutex extension_mutex_;
    mutable std::mutex calculation_mutex_;
    
    // ===== 생성자들 =====
    DriverStatistics() 
        : last_read_time(Utils::GetCurrentTimestamp())
        , last_write_time(Utils::GetCurrentTimestamp())
        , last_error_time(Utils::GetCurrentTimestamp())
        , start_time(Utils::GetCurrentTimestamp())
        , last_success_time(Utils::GetCurrentTimestamp())
        , last_connection_time(Utils::GetCurrentTimestamp())
    {
        ResetStatistics();
    }
    
    explicit DriverStatistics(const std::string& protocol_type) : DriverStatistics() {
        InitializeProtocolSpecificCounters(protocol_type);
    }
    
    // ===== 복사/이동 방지 (std::atomic 때문에) =====
    DriverStatistics(const DriverStatistics&) = delete;
    DriverStatistics& operator=(const DriverStatistics&) = delete;
    DriverStatistics(DriverStatistics&&) = delete;
    DriverStatistics& operator=(DriverStatistics&&) = delete;
    
    // ===== 소멸자 =====
    ~DriverStatistics() = default;
    
    // ===== 복사를 위한 별도 메서드 제공 =====
    
    /**
     * @brief 통계 스냅샷 생성 (복사용)
     * @return 현재 통계의 스냅샷
     */
    DriverStatisticsSnapshot CreateSnapshot() const;
    
    // ===== 기존 호환성 메서드들 =====
    double GetSuccessRate() const {
        uint64_t total = total_reads.load() + total_writes.load();
        if (total == 0) return 100.0;
        
        uint64_t successful = successful_reads.load() + successful_writes.load();
        double rate = (static_cast<double>(successful) / total) * 100.0;
        
        const_cast<DriverStatistics*>(this)->success_rate.store(rate);
        return rate;
    }
    
    void SyncResponseTime() {
        avg_response_time_ms.store(static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(average_response_time).count()
        ));
    }
    
    void UpdateTotalOperations() {
        std::lock_guard<std::mutex> lock(calculation_mutex_);
        
        total_operations.store(total_reads.load() + total_writes.load());
        successful_operations.store(successful_reads.load() + successful_writes.load());
        failed_operations.store(failed_reads.load() + failed_writes.load());
        
        GetSuccessRate();
    }
    
    void ResetStatistics() {
        // 기본 통계 초기화
        total_reads = 0;
        total_writes = 0;
        successful_reads = 0;
        successful_writes = 0;
        failed_reads = 0;
        failed_writes = 0;
        
        total_operations = 0;
        successful_operations = 0;
        failed_operations = 0;
        
        avg_response_time_ms = 0.0;
        min_response_time_ms = 999999.0;
        max_response_time_ms = 0.0;
        average_response_time = std::chrono::milliseconds(0);
        
        successful_connections = 0;
        failed_connections = 0;
        consecutive_failures = 0;
        uptime_seconds = 0;
        success_rate = 0.0;
        
        // 확장 통계 초기화
        std::lock_guard<std::mutex> lock(extension_mutex_);
        for (auto& [name, counter] : protocol_counters) {
            counter = 0;
        }
        for (auto& [name, metric] : protocol_metrics) {
            metric = 0.0;
        }
        protocol_status.clear();
        
        // 시간 정보 업데이트
        auto now = Utils::GetCurrentTimestamp();
        last_read_time = now;
        last_write_time = now;
        last_error_time = now;
        start_time = now;
        last_success_time = now;
        last_connection_time = now;
    }
    
    // ===== 새로운 확장 메서드들 =====
    void IncrementProtocolCounter(const std::string& counter_name, uint64_t increment = 1) {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        protocol_counters[counter_name] += increment;
    }
    
    void SetProtocolMetric(const std::string& metric_name, double value) {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        protocol_metrics[metric_name] = value;
    }
    
    void SetProtocolStatus(const std::string& status_name, const std::string& value) {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        protocol_status[status_name] = value;
    }
    
    uint64_t GetProtocolCounter(const std::string& counter_name) const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        auto it = protocol_counters.find(counter_name);
        return (it != protocol_counters.end()) ? it->second.load() : 0;
    }
    
    double GetProtocolMetric(const std::string& metric_name) const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        auto it = protocol_metrics.find(metric_name);
        return (it != protocol_metrics.end()) ? it->second.load() : 0.0;
    }
    
    std::string GetProtocolStatus(const std::string& status_name) const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        auto it = protocol_status.find(status_name);
        return (it != protocol_status.end()) ? it->second : "";
    }
    
    // ===== 프로토콜별 초기화 메서드 =====
    void InitializeProtocolSpecificCounters(const std::string& protocol_type) {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        
        if (protocol_type == "MODBUS") {
            protocol_counters["register_reads"] = 0;
            protocol_counters["coil_reads"] = 0;
            protocol_counters["register_writes"] = 0;
            protocol_counters["coil_writes"] = 0;
            protocol_counters["slave_errors"] = 0;
            protocol_counters["crc_checks"] = 0;
            protocol_metrics["avg_response_time_ms"] = 0.0;
            protocol_status["connection_status"] = "disconnected";
            
        } else if (protocol_type == "MQTT") {
            protocol_counters["mqtt_messages"] = 0;
            protocol_counters["published_messages"] = 0;
            protocol_counters["received_messages"] = 0;
            protocol_counters["qos0_messages"] = 0;
            protocol_counters["qos1_messages"] = 0;
            protocol_counters["qos2_messages"] = 0;
            protocol_counters["retained_messages"] = 0;
            protocol_counters["connection_failures"] = 0;
            protocol_metrics["avg_message_size_bytes"] = 0.0;
            protocol_metrics["connection_uptime_seconds"] = 0.0;
            protocol_status["broker_status"] = "disconnected";
            
        } else if (protocol_type == "BACNET") {
            protocol_counters["read_property_requests"] = 0;
            protocol_counters["write_property_requests"] = 0;
            protocol_counters["device_discoveries"] = 0;
            protocol_counters["object_discoveries"] = 0;
            protocol_counters["network_errors"] = 0;
            protocol_metrics["network_response_time_ms"] = 0.0;
            protocol_metrics["discovery_success_rate"] = 100.0;
            protocol_status["network_status"] = "offline";
        }
    }
    /**
     * @brief 모든 프로토콜 카운터 맵 반환 (읽기 전용)
     * @return 프로토콜 카운터들의 복사본
     */
    std::map<std::string, uint64_t> GetProtocolCounters() const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        
        std::map<std::string, uint64_t> counters_copy;
        for (const auto& [key, value] : protocol_counters) {
            counters_copy[key] = value.load();
        }
        return counters_copy;
    }
    
    /**
     * @brief 모든 프로토콜 메트릭 맵 반환 (읽기 전용)
     * @return 프로토콜 메트릭들의 복사본
     */
    std::map<std::string, double> GetProtocolMetrics() const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        
        std::map<std::string, double> metrics_copy;
        for (const auto& [key, value] : protocol_metrics) {
            metrics_copy[key] = value.load();
        }
        return metrics_copy;
    }
    
    /**
     * @brief 모든 프로토콜 상태 맵 반환 (읽기 전용)
     * @return 프로토콜 상태들의 복사본
     */
    std::map<std::string, std::string> GetProtocolStatuses() const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        return protocol_status;  // string 맵이므로 복사 안전
    }
    
    /**
     * @brief 평균 응답 시간 반환 (호환성 메서드)
     * @return 평균 응답 시간 (밀리초)
     */
    double GetAverageResponseTime() const {
        return avg_response_time_ms.load();
    }
    
    /**
     * @brief 최소 응답 시간 반환
     * @return 최소 응답 시간 (밀리초)
     */
    double GetMinResponseTime() const {
        return min_response_time_ms.load();
    }
    
    /**
     * @brief 최대 응답 시간 반환
     * @return 최대 응답 시간 (밀리초)
     */
    double GetMaxResponseTime() const {
        return max_response_time_ms.load();
    }
    
    /**
     * @brief 연결 성공률 계산
     * @return 연결 성공률 (0.0 ~ 100.0)
     */
    double GetConnectionSuccessRate() const {
        uint64_t total_conn = successful_connections.load() + failed_connections.load();
        if (total_conn == 0) return 100.0;
        
        return (static_cast<double>(successful_connections.load()) / total_conn) * 100.0;
    }
    
    /**
     * @brief 전체 작업 수 반환
     * @return 전체 읽기 + 쓰기 작업 수
     */
    uint64_t GetTotalOperations() const {
        return total_reads.load() + total_writes.load();
    }
    
    /**
     * @brief 전체 성공 작업 수 반환
     * @return 전체 성공한 읽기 + 쓰기 작업 수
     */
    uint64_t GetTotalSuccessfulOperations() const {
        return successful_reads.load() + successful_writes.load();
    }
    
    /**
     * @brief 전체 실패 작업 수 반환
     * @return 전체 실패한 읽기 + 쓰기 작업 수
     */
    uint64_t GetTotalFailedOperations() const {
        return failed_reads.load() + failed_writes.load();
    }
    
    /**
     * @brief 프로토콜별 특정 카운터가 존재하는지 확인
     * @param counter_name 카운터 이름
     * @return 존재 여부
     */
    bool HasProtocolCounter(const std::string& counter_name) const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        return protocol_counters.find(counter_name) != protocol_counters.end();
    }
    
    /**
     * @brief 프로토콜별 특정 메트릭이 존재하는지 확인
     * @param metric_name 메트릭 이름
     * @return 존재 여부
     */
    bool HasProtocolMetric(const std::string& metric_name) const {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        return protocol_metrics.find(metric_name) != protocol_metrics.end();
    }
    
    /**
     * @brief 시작 시간부터 현재까지의 업타임 계산 (초)
     * @return 업타임 (초)
     */
    uint64_t GetUptimeSeconds() const {
        auto now = Utils::GetCurrentTimestamp();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        return static_cast<uint64_t>(duration.count());
    }
    
    /**
     * @brief 마지막 성공한 작업 이후 경과 시간 (초)
     * @return 경과 시간 (초)
     */
    uint64_t GetSecondsSinceLastSuccess() const {
        auto now = Utils::GetCurrentTimestamp();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_success_time);
        return static_cast<uint64_t>(duration.count());
    }
    
    /**
     * @brief 통계를 JSON 형태의 문자열로 변환
     * @return JSON 문자열
     */
    std::string ToJsonString() const {
#ifdef HAS_NLOHMANN_JSON
        try {
            json stats_json;
            
            // 기본 통계
            stats_json["total_reads"] = total_reads.load();
            stats_json["successful_reads"] = successful_reads.load();
            stats_json["failed_reads"] = failed_reads.load();
            stats_json["total_writes"] = total_writes.load();
            stats_json["successful_writes"] = successful_writes.load();
            stats_json["failed_writes"] = failed_writes.load();
            stats_json["success_rate"] = GetSuccessRate();
            stats_json["avg_response_time_ms"] = avg_response_time_ms.load();
            
            // 연결 통계
            stats_json["successful_connections"] = successful_connections.load();
            stats_json["failed_connections"] = failed_connections.load();
            stats_json["connection_success_rate"] = GetConnectionSuccessRate();
            
            // 시간 정보
            stats_json["uptime_seconds"] = GetUptimeSeconds();
            stats_json["last_read_time"] = Utils::TimestampToString(last_read_time);
            stats_json["last_write_time"] = Utils::TimestampToString(last_write_time);
            stats_json["start_time"] = Utils::TimestampToString(start_time);
            
            // 프로토콜별 통계
            if (!protocol_counters.empty()) {
                stats_json["protocol_counters"] = GetProtocolCounters();
            }
            if (!protocol_metrics.empty()) {
                stats_json["protocol_metrics"] = GetProtocolMetrics();
            }
            if (!protocol_status.empty()) {
                stats_json["protocol_status"] = GetProtocolStatuses();
            }
            
            return stats_json.dump(2);
            
        } catch (const std::exception& e) {
            return "{\"error\":\"Failed to serialize statistics: " + std::string(e.what()) + "\"}";
        }
#else
        // JSON 라이브러리가 없는 경우 간단한 형태
        std::ostringstream oss;
        oss << "{"
            << "\"total_reads\":" << total_reads.load() << ","
            << "\"successful_reads\":" << successful_reads.load() << ","
            << "\"failed_reads\":" << failed_reads.load() << ","
            << "\"success_rate\":" << std::fixed << std::setprecision(2) << GetSuccessRate() << ","
            << "\"uptime_seconds\":" << GetUptimeSeconds()
            << "}";
        return oss.str();
#endif
    }

};

// =============================================================================
// 복사 가능한 스냅샷 구조체 정의
// =============================================================================

/**
 * @brief 복사 가능한 드라이버 통계 스냅샷 구조체
 * @details std::atomic을 사용하지 않아 복사 생성자가 가능함
 */
struct DriverStatisticsSnapshot {
    // 기본 통계들 (일반 타입)
    uint64_t total_reads{0};
    uint64_t successful_reads{0};
    uint64_t failed_reads{0};
    uint64_t total_writes{0};
    uint64_t successful_writes{0};
    uint64_t failed_writes{0};
    uint64_t total_connections{0};
    uint64_t successful_connections{0};
    uint64_t failed_connections{0};
    uint64_t total_operations{0};
    uint64_t successful_operations{0};
    uint64_t failed_operations{0};
    uint64_t total_bytes_sent{0};
    uint64_t total_bytes_received{0};
    
    // 계산된 메트릭들
    double success_rate{100.0};
    double avg_response_time_ms{0.0};
    uint32_t min_response_time_ms{UINT32_MAX};
    uint32_t max_response_time_ms{0};
    
    // 시간 정보들 - string 타입
    std::string last_read_time;
    std::string last_write_time;
    std::string last_error_time;
    std::string start_time;
    std::string last_success_time;
    std::string last_connection_time;
    
    // 프로토콜별 확장
    std::map<std::string, uint64_t> protocol_counters;
    std::map<std::string, double> protocol_metrics;
    std::map<std::string, std::string> protocol_status;
    
    // 복사 생성자 허용 (모든 멤버가 복사 가능)
    DriverStatisticsSnapshot() = default;
    DriverStatisticsSnapshot(const DriverStatisticsSnapshot&) = default;
    DriverStatisticsSnapshot& operator=(const DriverStatisticsSnapshot&) = default;
    DriverStatisticsSnapshot(DriverStatisticsSnapshot&&) = default;
    DriverStatisticsSnapshot& operator=(DriverStatisticsSnapshot&&) = default;
    ~DriverStatisticsSnapshot() = default;
    
    /**
     * @brief 성공률 계산
     */
    double GetSuccessRate() const {
        uint64_t total = total_reads + total_writes;
        if (total == 0) return 100.0;
        uint64_t successful = successful_reads + successful_writes;
        return (static_cast<double>(successful) / total) * 100.0;
    }
    
    /**
     * @brief 평균 응답 시간 반환
     */
    double GetAverageResponseTime() const {
        return avg_response_time_ms;
    }
};

// =============================================================================
// CreateSnapshot 메서드 구현 (인라인)
// =============================================================================

inline DriverStatisticsSnapshot DriverStatistics::CreateSnapshot() const {
    std::lock_guard<std::mutex> lock(calculation_mutex_);
    
    DriverStatisticsSnapshot snapshot;
    
    // 기본 통계들 복사
    snapshot.total_reads = total_reads.load();
    snapshot.successful_reads = successful_reads.load();
    snapshot.failed_reads = failed_reads.load();
    snapshot.total_writes = total_writes.load();
    snapshot.successful_writes = successful_writes.load();
    snapshot.failed_writes = failed_writes.load();
    snapshot.total_operations = total_operations.load();
    snapshot.successful_operations = successful_operations.load();
    snapshot.failed_operations = failed_operations.load();
    
    // 연결 통계 복사
    snapshot.successful_connections = successful_connections.load();
    snapshot.failed_connections = failed_connections.load();
    snapshot.total_connections = successful_connections.load() + failed_connections.load();
    
    // 바이트 통계는 기본값으로 설정 (기존 DriverStatistics에 없는 필드)
    snapshot.total_bytes_sent = 0;
    snapshot.total_bytes_received = 0;
    
    // 응답 시간 메트릭들
    snapshot.success_rate = success_rate.load();
    snapshot.avg_response_time_ms = avg_response_time_ms.load();
    snapshot.min_response_time_ms = static_cast<uint32_t>(min_response_time_ms.load());
    snapshot.max_response_time_ms = static_cast<uint32_t>(max_response_time_ms.load());
    
    // 시간 정보들 - Timestamp를 string으로 변환
    snapshot.last_read_time = Utils::TimestampToString(last_read_time);
    snapshot.last_write_time = Utils::TimestampToString(last_write_time);
    snapshot.last_error_time = Utils::TimestampToString(last_error_time);
    snapshot.start_time = Utils::TimestampToString(start_time);
    snapshot.last_success_time = Utils::TimestampToString(last_success_time);
    snapshot.last_connection_time = Utils::TimestampToString(last_connection_time);
    
    // 프로토콜별 카운터들 복사
    {
        std::lock_guard<std::mutex> ext_lock(extension_mutex_);
        for (const auto& [key, value] : protocol_counters) {
            snapshot.protocol_counters[key] = value.load();
        }
        for (const auto& [key, value] : protocol_metrics) {
            snapshot.protocol_metrics[key] = value.load();
        }
        snapshot.protocol_status = protocol_status;
    }
    
    return snapshot;
}

} // namespace Structs
} // namespace PulseOne

#endif // PULSEONE_UNIFIED_DRIVER_STATISTICS_H