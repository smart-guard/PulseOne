/**
 * @file DriverStatistics.h
 * @brief PulseOne 표준화된 드라이버 통계 구조
 * @author PulseOne Development Team  
 * @date 2025-08-02
 * @version 2.0.0 - 모든 프로토콜 지원 통합 버전
 */

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
    
    // ===== 작업 기록 메서드들 =====
    void RecordReadOperation(bool success, double response_time_ms) {
        total_reads++;
        if (success) {
            successful_reads++;
            last_success_time = Utils::GetCurrentTimestamp();
            consecutive_failures = 0;
        } else {
            failed_reads++;
            consecutive_failures++;
            last_error_time = Utils::GetCurrentTimestamp();
        }
        
        last_read_time = Utils::GetCurrentTimestamp();
        UpdateResponseTimeStats(response_time_ms);
        UpdateTotalOperations();
    }
    
    void RecordWriteOperation(bool success, double response_time_ms) {
        total_writes++;
        if (success) {
            successful_writes++;
            last_success_time = Utils::GetCurrentTimestamp();
            consecutive_failures = 0;
        } else {
            failed_writes++;
            consecutive_failures++;
            last_error_time = Utils::GetCurrentTimestamp();
        }
        
        last_write_time = Utils::GetCurrentTimestamp();
        UpdateResponseTimeStats(response_time_ms);
        UpdateTotalOperations();
    }
    
    void RecordConnectionAttempt(bool success) {
        if (success) {
            successful_connections++;
            last_connection_time = Utils::GetCurrentTimestamp();
        } else {
            failed_connections++;
        }
    }
    
    // ===== JSON 출력 =====
    std::string ToJsonString() const {
#ifdef HAS_NLOHMANN_JSON
        json stats_json;
        
        // 기본 작업 통계
        stats_json["basic_operations"] = {
            {"total_reads", total_reads.load()},
            {"total_writes", total_writes.load()},
            {"successful_reads", successful_reads.load()},
            {"successful_writes", successful_writes.load()},
            {"failed_reads", failed_reads.load()},
            {"failed_writes", failed_writes.load()}
        };
        
        // 통합 통계
        stats_json["aggregated_operations"] = {
            {"total_operations", total_operations.load()},
            {"successful_operations", successful_operations.load()},
            {"failed_operations", failed_operations.load()},
            {"success_rate", success_rate.load()}
        };
        
        // 성능 통계
        stats_json["performance"] = {
            {"avg_response_time_ms", avg_response_time_ms.load()},
            {"min_response_time_ms", min_response_time_ms.load()},
            {"max_response_time_ms", max_response_time_ms.load()}
        };
        
        // 프로토콜별 확장 통계
        {
            std::lock_guard<std::mutex> lock(extension_mutex_);
            
            if (!protocol_counters.empty()) {
                json protocol_counters_json;
                for (const auto& [name, counter] : protocol_counters) {
                    protocol_counters_json[name] = counter.load();
                }
                stats_json["protocol_counters"] = protocol_counters_json;
            }
            
            if (!protocol_metrics.empty()) {
                json protocol_metrics_json;
                for (const auto& [name, metric] : protocol_metrics) {
                    protocol_metrics_json[name] = metric.load();
                }
                stats_json["protocol_metrics"] = protocol_metrics_json;
            }
            
            if (!protocol_status.empty()) {
                stats_json["protocol_status"] = protocol_status;
            }
        }
        
        return stats_json.dump(2);
#else
        return "{\"message\":\"JSON library not available\"}";
#endif
    }
    
    std::string GetSummary() const {
        std::ostringstream oss;
        oss << "Operations: " << total_operations.load() 
            << " (Success: " << successful_operations.load() 
            << ", Failed: " << failed_operations.load() 
            << "), Success Rate: " << std::fixed << std::setprecision(1) 
            << success_rate.load() << "%, Avg Response: " 
            << std::setprecision(2) << avg_response_time_ms.load() << "ms";
        return oss.str();
    }

private:
    void UpdateResponseTimeStats(double response_time_ms) {
        double current_avg = avg_response_time_ms.load();
        double new_avg = (current_avg == 0.0) ? response_time_ms : 
                        (current_avg * 0.9 + response_time_ms * 0.1);
        avg_response_time_ms.store(new_avg);
        
        double current_min = min_response_time_ms.load();
        while (response_time_ms < current_min) {
            if (min_response_time_ms.compare_exchange_weak(current_min, response_time_ms)) {
                break;
            }
        }
        
        double current_max = max_response_time_ms.load();
        while (response_time_ms > current_max) {
            if (max_response_time_ms.compare_exchange_weak(current_max, response_time_ms)) {
                break;
            }
        }
        
        average_response_time = std::chrono::milliseconds(static_cast<long long>(new_avg));
    }
    
    void InitializeProtocolSpecificCounters(const std::string& protocol_type) {
        std::lock_guard<std::mutex> lock(extension_mutex_);
        
        if (protocol_type == "MODBUS") {
            // Modbus 특화 카운터
            protocol_counters["register_reads"] = 0;
            protocol_counters["coil_reads"] = 0;
            protocol_counters["discrete_input_reads"] = 0;
            protocol_counters["holding_register_writes"] = 0;
            protocol_counters["coil_writes"] = 0;
            protocol_counters["slave_errors"] = 0;
            protocol_counters["timeout_errors"] = 0;
            protocol_counters["crc_errors"] = 0;
            protocol_counters["exception_responses"] = 0;
            
            protocol_metrics["avg_slave_response_time"] = 0.0;
            protocol_metrics["packet_loss_rate"] = 0.0;
            protocol_metrics["error_rate"] = 0.0;
            
            protocol_status["current_slave_id"] = "1";
            protocol_status["connection_type"] = protocol_type;
            
        } else if (protocol_type == "MQTT") {
            // MQTT 특화 카운터
            protocol_counters["messages_published"] = 0;
            protocol_counters["messages_received"] = 0;
            protocol_counters["qos0_messages"] = 0;
            protocol_counters["qos1_messages"] = 0;
            protocol_counters["qos2_messages"] = 0;
            protocol_counters["retained_messages"] = 0;
            protocol_counters["broker_disconnections"] = 0;
            protocol_counters["subscription_count"] = 0;
            protocol_counters["publish_failures"] = 0;
            
            protocol_metrics["avg_publish_latency"] = 0.0;
            protocol_metrics["message_loss_rate"] = 0.0;
            protocol_metrics["connection_uptime"] = 0.0;
            
            protocol_status["broker_status"] = "disconnected";
            protocol_status["client_id"] = "";
            protocol_status["last_will_topic"] = "";
            
        } else if (protocol_type == "BACNET" || protocol_type == "BACNET_IP") {
            // BACnet 특화 카운터
            protocol_counters["who_is_sent"] = 0;
            protocol_counters["i_am_received"] = 0;
            protocol_counters["read_property_requests"] = 0;
            protocol_counters["write_property_requests"] = 0;
            protocol_counters["cov_subscriptions"] = 0;
            protocol_counters["cov_notifications"] = 0;
            protocol_counters["devices_discovered"] = 0;
            protocol_counters["objects_discovered"] = 0;
            protocol_counters["segmented_messages"] = 0;
            
            protocol_metrics["discovery_success_rate"] = 0.0;
            protocol_metrics["avg_apdu_response_time"] = 0.0;
            protocol_metrics["network_load"] = 0.0;
            
            protocol_status["network_number"] = "0";
            protocol_status["device_instance"] = "0";
            protocol_status["max_apdu_length"] = "1476";
            
        } else if (protocol_type == "OPCUA") {
            // OPC-UA 특화 카운터
            protocol_counters["subscriptions_active"] = 0;
            protocol_counters["monitored_items"] = 0;
            protocol_counters["data_change_notifications"] = 0;
            protocol_counters["method_calls"] = 0;
            protocol_counters["browse_requests"] = 0;
            protocol_counters["certificate_errors"] = 0;
            protocol_counters["security_violations"] = 0;
            
            protocol_metrics["session_timeout"] = 0.0;
            protocol_metrics["subscription_interval"] = 0.0;
            protocol_metrics["publishing_interval"] = 0.0;
            
            protocol_status["security_mode"] = "none";
            protocol_status["security_policy"] = "none";
            protocol_status["session_state"] = "closed";
        }
    }
};

} // namespace Structs
} // namespace PulseOne

#endif // PULSEONE_UNIFIED_DRIVER_STATISTICS_H