// =============================================================================
// collector/include/Drivers/Bacnet/BACnetStatisticsManager.h
// 🔥 기존 DriverStatistics를 활용하는 BACnet 통계 관리자
// =============================================================================

#ifndef BACNET_STATISTICS_MANAGER_H
#define BACNET_STATISTICS_MANAGER_H

#include "Common/DriverStatistics.h"
#include "Common/DriverError.h"
#include <memory>
#include <chrono>
#include <string>

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 통계 관리자 (DriverStatistics 활용)
 * 
 * 기존 DriverStatistics의 protocol_counters/metrics 기능을 활용하여
 * BACnet 특화 통계를 관리합니다.
 */
class BACnetStatisticsManager {
public:
    // ==========================================================================
    // 생성자 및 기본 관리
    // ==========================================================================
    BACnetStatisticsManager();
    ~BACnetStatisticsManager() = default;
    
    // 복사/이동 방지
    BACnetStatisticsManager(const BACnetStatisticsManager&) = delete;
    BACnetStatisticsManager& operator=(const BACnetStatisticsManager&) = delete;
    
    // ==========================================================================
    // 🔥 통계 수집 메서드들 (DriverStatistics에 위임)
    // ==========================================================================
    
    void StartOperation(const std::string& operation_type);
    void CompleteOperation(const std::string& operation_type, bool success);
    
    // BACnet 특화 카운터 업데이트
    void RecordWhoIsSent() { 
        driver_stats_->IncrementProtocolCounter("who_is_sent"); 
    }
    
    void RecordIAmReceived(uint32_t device_id) { 
        driver_stats_->IncrementProtocolCounter("i_am_received");
        driver_stats_->SetProtocolStatus("last_discovered_device", std::to_string(device_id));
    }
    
    void RecordReadPropertyRequest() { 
        driver_stats_->IncrementProtocolCounter("read_property_requests");
        driver_stats_->RecordReadOperation(true, 0.0); // 일단 성공으로 기록, 나중에 실제 결과로 업데이트
    }
    
    void RecordReadPropertyResponse(double response_time_ms) { 
        driver_stats_->IncrementProtocolCounter("read_property_responses");
        driver_stats_->SetProtocolMetric("avg_read_response_time", response_time_ms);
    }
    
    void RecordWritePropertyRequest() { 
        driver_stats_->IncrementProtocolCounter("write_property_requests");
        driver_stats_->RecordWriteOperation(true, 0.0);
    }
    
    void RecordWritePropertyResponse(double response_time_ms) { 
        driver_stats_->IncrementProtocolCounter("write_property_responses");
        driver_stats_->SetProtocolMetric("avg_write_response_time", response_time_ms);
    }
    
    void RecordRPMRequest(size_t object_count) { 
        driver_stats_->IncrementProtocolCounter("rpm_requests");
        driver_stats_->SetProtocolMetric("avg_rpm_objects", static_cast<double>(object_count));
    }
    
    void RecordWPMRequest(size_t object_count) { 
        driver_stats_->IncrementProtocolCounter("wpm_requests");
        driver_stats_->SetProtocolMetric("avg_wpm_objects", static_cast<double>(object_count));
    }
    
    // COV 통계
    void RecordCOVSubscription(uint32_t device_id, uint32_t object_instance) { 
        driver_stats_->IncrementProtocolCounter("cov_subscriptions");
        driver_stats_->SetProtocolStatus("last_cov_device", std::to_string(device_id) + "_" + std::to_string(object_instance));
    }
    
    void RecordCOVNotification(uint32_t device_id, uint32_t object_instance) { 
        driver_stats_->IncrementProtocolCounter("cov_notifications");
        (void)device_id; (void)object_instance; // 사용하지 않는 매개변수 경고 제거
    }
    
    void RecordCOVTimeout() { 
        driver_stats_->IncrementProtocolCounter("cov_timeouts"); 
    }
    
    // 에러 통계
    void RecordProtocolError() { 
        driver_stats_->IncrementProtocolCounter("protocol_errors"); 
    }
    
    void RecordTimeoutError() { 
        driver_stats_->IncrementProtocolCounter("timeout_errors"); 
    }
    
    void RecordDeviceNotFoundError() { 
        driver_stats_->IncrementProtocolCounter("device_not_found_errors"); 
    }
    
    void RecordNetworkError() { 
        driver_stats_->IncrementProtocolCounter("network_errors"); 
    }
    
    // 디바이스 및 객체 통계
    void RecordDeviceDiscovered(uint32_t device_id) { 
        driver_stats_->IncrementProtocolCounter("devices_discovered");
        driver_stats_->SetProtocolStatus("last_discovered_device", std::to_string(device_id));
    }
    
    void RecordObjectDiscovered(uint32_t device_id, uint32_t object_instance) { 
        driver_stats_->IncrementProtocolCounter("objects_discovered");
        (void)device_id; (void)object_instance;
    }
    
    // ==========================================================================
    // 🔥 통계 조회 메서드들
    // ==========================================================================
    
    /**
     * @brief 표준 DriverStatistics 반환
     */
    const Structs::DriverStatistics* GetStandardStatistics() const {
        return driver_stats_.get();
    }
    
    /**
     * @brief BACnet 특화 카운터 조회
     */
    uint64_t GetBACnetCounter(const std::string& counter_name) const {
        return driver_stats_->GetProtocolCounter(counter_name);
    }
    
    /**
     * @brief BACnet 특화 메트릭 조회
     */
    double GetBACnetMetric(const std::string& metric_name) const {
        return driver_stats_->GetProtocolMetric(metric_name);
    }
    
    /**
     * @brief BACnet 요약 보고서
     */
    std::string GetBACnetSummary() const {
        std::ostringstream oss;
        oss << "BACnet Stats: ";
        oss << "Devices: " << GetBACnetCounter("devices_discovered") << ", ";
        oss << "Objects: " << GetBACnetCounter("objects_discovered") << ", ";
        oss << "COV: " << GetBACnetCounter("cov_subscriptions") << ", ";
        oss << "Errors: " << GetBACnetCounter("protocol_errors");
        return oss.str();
    }
    
    /**
     * @brief JSON 형식 통계 반환 (DriverStatistics 위임)
     */
    std::string GetStatisticsJson() const {
        return driver_stats_->ToJsonString();
    }
    
    // ==========================================================================
    // 통계 관리
    // ==========================================================================
    void ResetStatistics() {
        driver_stats_->ResetStatistics();
        start_time_ = std::chrono::system_clock::now();
    }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    std::unique_ptr<Structs::DriverStatistics> driver_stats_;
    std::chrono::system_clock::time_point start_time_;
    
    // 작업 추적용
    struct OperationTracker {
        std::chrono::steady_clock::time_point start_time;
        std::string operation_type;
    };
    std::map<std::string, OperationTracker> active_operations_;
    std::mutex operations_mutex_;
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    void InitializeBACnetCounters();
};

// =============================================================================
// 🔥 간단한 BACnet 통계 구조체 (기존 호환성용)
// =============================================================================

/**
 * @brief 기존 코드 호환성을 위한 BACnet 통계 구조체
 * @details 실제로는 DriverStatistics 데이터를 참조
 */
struct BACnetStatistics {
    std::atomic<uint64_t> who_is_sent{0};
    std::atomic<uint64_t> i_am_received{0};
    std::atomic<uint64_t> read_property_requests{0};
    std::atomic<uint64_t> read_property_responses{0};
    std::atomic<uint64_t> write_property_requests{0};
    std::atomic<uint64_t> write_property_responses{0};
    std::atomic<uint64_t> rpm_requests{0};
    std::atomic<uint64_t> wpm_requests{0};
    
    std::atomic<uint64_t> cov_subscriptions{0};
    std::atomic<uint64_t> cov_notifications_received{0};
    std::atomic<uint64_t> cov_timeouts{0};
    std::atomic<uint64_t> active_cov_subscriptions{0};
    
    std::atomic<uint64_t> protocol_errors{0};
    std::atomic<uint64_t> timeout_errors{0};
    std::atomic<uint64_t> device_not_found_errors{0};
    std::atomic<uint64_t> network_errors{0};
    
    std::atomic<double> avg_response_time_ms{0.0};
    std::atomic<uint32_t> discovered_devices{0};
    std::atomic<uint32_t> mapped_objects{0};
    
    std::chrono::system_clock::time_point start_time;
    
    BACnetStatistics() : start_time(std::chrono::system_clock::now()) {}
    
    void Reset() {
        who_is_sent = 0;
        i_am_received = 0;
        read_property_requests = 0;
        read_property_responses = 0;
        write_property_requests = 0;
        write_property_responses = 0;
        rpm_requests = 0;
        wpm_requests = 0;
        
        cov_subscriptions = 0;
        cov_notifications_received = 0;
        cov_timeouts = 0;
        active_cov_subscriptions = 0;
        
        protocol_errors = 0;
        timeout_errors = 0;
        device_not_found_errors = 0;
        network_errors = 0;
        
        avg_response_time_ms = 0.0;
        discovered_devices = 0;
        mapped_objects = 0;
        
        start_time = std::chrono::system_clock::now();
    }
    
    double GetSuccessRate() const {
        uint64_t total_requests = read_property_requests.load() + write_property_requests.load();
        if (total_requests == 0) return 100.0;
        
        uint64_t total_responses = read_property_responses.load() + write_property_responses.load();
        return (static_cast<double>(total_responses) / total_requests) * 100.0;
    }
    
    std::chrono::seconds GetUptime() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time);
    }
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_STATISTICS_MANAGER_H