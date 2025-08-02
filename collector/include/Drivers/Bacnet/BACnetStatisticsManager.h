// =============================================================================
// collector/include/Drivers/Bacnet/BACnetStatisticsManager.h
// 🔥 BACnet 통계 관리자 - 성능 및 상태 모니터링
// =============================================================================

#ifndef BACNET_STATISTICS_MANAGER_H
#define BACNET_STATISTICS_MANAGER_H

#include "Common/UnifiedCommonTypes.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include <atomic>
#include <mutex>
#include <chrono>
#include <deque>
#include <memory>

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 통계 관리자
 * 
 * 기능:
 * - 실시간 성능 통계 수집
 * - 표준 DriverStatistics 생성
 * - BACnet 특화 통계 관리
 * - 히스토리 데이터 관리
 * - 성능 임계값 모니터링
 */
class BACnetStatisticsManager {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    BACnetStatisticsManager();
    ~BACnetStatisticsManager() = default;
    
    // 복사/이동 방지
    BACnetStatisticsManager(const BACnetStatisticsManager&) = delete;
    BACnetStatisticsManager& operator=(const BACnetStatisticsManager&) = delete;
    
    // ==========================================================================
    // 🔥 통계 업데이트 메서드들
    // ==========================================================================
    
    /**
     * @brief 읽기 작업 통계 업데이트
     * @param total_points 시도한 총 포인트 수
     * @param successful_points 성공한 포인트 수
     * @param duration 소요 시간
     */
    void UpdateReadStatistics(size_t total_points, size_t successful_points, 
                             std::chrono::milliseconds duration);
    
    /**
     * @brief 쓰기 작업 통계 업데이트
     * @param total_points 시도한 총 포인트 수
     * @param successful_points 성공한 포인트 수
     * @param duration 소요 시간
     */
    void UpdateWriteStatistics(size_t total_points, size_t successful_points,
                              std::chrono::milliseconds duration);
    
    /**
     * @brief 연결 시도 통계 업데이트
     * @param success 연결 성공 여부
     */
    void IncrementConnectionAttempts(bool success = true);
    
    /**
     * @brief 에러 카운트 증가
     * @param error_type 에러 타입 (선택적)
     */
    void IncrementErrorCount(const std::string& error_type = "");
    
    /**
     * @brief 네트워크 메시지 통계 업데이트
     */
    void IncrementMessagesReceived();
    void IncrementMessagesSent();
    
    /**
     * @brief 연결 상태 설정
     * @param connected 연결 상태
     */
    void SetConnectionStatus(bool connected);
    
    /**
     * @brief 네트워크 통계 업데이트 (주기적 호출)
     */
    void UpdateNetworkStatistics();
    
    // ==========================================================================
    // 🔥 통계 조회 메서드들
    // ==========================================================================
    
    /**
     * @brief 표준 드라이버 통계 반환
     * @return IProtocolDriver 호환 통계
     */
    const DriverStatistics& GetStandardStatistics() const;
    
    /**
     * @brief BACnet 특화 통계 반환
     * @return BACnet 프로토콜 전용 통계
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    /**
     * @brief 성능 히스토리 반환
     * @param duration 조회할 기간 (기본: 1시간)
     * @return 성능 히스토리 데이터
     */
    std::vector<PerformanceSnapshot> GetPerformanceHistory(
        std::chrono::minutes duration = std::chrono::minutes(60)) const;
    
    /**
     * @brief 현재 성능 지표 반환
     * @return 실시간 성능 정보
     */
    PerformanceMetrics GetCurrentPerformance() const;
    
    /**
     * @brief 에러 분석 결과 반환
     * @return 에러 타입별 통계
     */
    std::map<std::string, uint64_t> GetErrorAnalysis() const;
    
    // ==========================================================================
    // 관리 메서드들
    // ==========================================================================
    
    /**
     * @brief 모든 통계 초기화
     */
    void Reset();
    
    /**
     * @brief 히스토리 데이터 정리
     * @param max_age 보관할 최대 기간
     */
    void CleanupHistory(std::chrono::hours max_age = std::chrono::hours(24));
    
    /**
     * @brief 통계를 JSON 형태로 내보내기
     * @return JSON 문자열
     */
    std::string ExportToJson() const;

private:
    // ==========================================================================
    // 내부 구조체들
    // ==========================================================================
    
    /**
     * @brief 성능 스냅샷 (시점별 성능 기록)
     */
    struct PerformanceSnapshot {
        std::chrono::system_clock::time_point timestamp;
        double read_success_rate;
        double write_success_rate;
        double avg_response_time_ms;
        uint64_t messages_per_second;
        uint64_t active_connections;
        uint64_t error_rate_per_minute;
    };
    
    /**
     * @brief 누적 통계 데이터
     */
    struct CumulativeStats {
        // 읽기 통계
        std::atomic<uint64_t> total_read_requests{0};
        std::atomic<uint64_t> successful_reads{0};
        std::atomic<uint64_t> failed_reads{0};
        std::atomic<uint64_t> total_read_time_ms{0};
        
        // 쓰기 통계
        std::atomic<uint64_t> total_write_requests{0};
        std::atomic<uint64_t> successful_writes{0};
        std::atomic<uint64_t> failed_writes{0};
        std::atomic<uint64_t> total_write_time_ms{0};
        
        // 연결 통계
        std::atomic<uint64_t> connection_attempts{0};
        std::atomic<uint64_t> successful_connections{0};
        std::atomic<uint64_t> connection_failures{0};
        
        // 네트워크 통계
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        
        // 에러 통계
        std::atomic<uint64_t> total_errors{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> protocol_errors{0};
        std::atomic<uint64_t> connection_errors{0};
        
        // 상태
        std::atomic<bool> is_connected{false};
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_update_time;
    };
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 통계 데이터
    CumulativeStats cumulative_stats_;
    
    // 표준 통계 (캐시)
    mutable std::mutex standard_stats_mutex_;
    mutable std::unique_ptr<DriverStatistics> standard_statistics_cache_;
    mutable std::chrono::system_clock::time_point last_standard_update_;
    
    // BACnet 특화 통계 (캐시)
    mutable std::mutex bacnet_stats_mutex_;
    mutable BACnetStatistics bacnet_statistics_cache_;
    mutable std::chrono::system_clock::time_point last_bacnet_update_;
    
    // 성능 히스토리
    mutable std::mutex history_mutex_;
    std::deque<PerformanceSnapshot> performance_history_;
    std::chrono::system_clock::time_point last_snapshot_time_;
    
    // 에러 분석
    std::mutex error_analysis_mutex_;
    std::map<std::string, std::atomic<uint64_t>> error_counts_by_type_;
    
    // 실시간 성능 추적
    std::mutex performance_mutex_;
    std::deque<std::chrono::milliseconds> recent_response_times_;
    std::deque<std::chrono::system_clock::time_point> recent_operations_;
    
    static constexpr size_t MAX_RECENT_OPERATIONS = 1000;
    static constexpr size_t MAX_HISTORY_ENTRIES = 1440; // 24시간 (분 단위)
    static constexpr auto CACHE_REFRESH_INTERVAL = std::chrono::seconds(5);
    static constexpr auto SNAPSHOT_INTERVAL = std::chrono::minutes(1);
    
    // ==========================================================================
    // 비공개 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief 표준 통계 캐시 업데이트
     */
    void UpdateStandardStatisticsCache() const;
    
    /**
     * @brief BACnet 특화 통계 캐시 업데이트
     */
    void UpdateBACnetStatisticsCache() const;
    
    /**
     * @brief 성능 스냅샷 생성 및 히스토리에 추가
     */
    void CreatePerformanceSnapshot();
    
    /**
     * @brief 최근 응답 시간 관리
     */
    void AddResponseTime(std::chrono::milliseconds duration);
    void CleanupRecentData();
    
    /**
     * @brief 평균 응답 시간 계산
     */
    double CalculateAverageResponseTime() const;
    
    /**
     * @brief 초당 메시지 수 계산
     */
    double CalculateMessagesPerSecond() const;
    
    /**
     * @brief 성공률 계산
     */
    double CalculateSuccessRate(uint64_t successful, uint64_t total) const;
    
    /**
     * @brief 런타임 계산
     */
    std::chrono::seconds GetRuntime() const;
    
    /**
     * @brief 캐시 만료 확인
     */
    bool IsCacheExpired(std::chrono::system_clock::time_point last_update) const;
};

// =============================================================================
// 인라인 구현들
// =============================================================================

inline void BACnetStatisticsManager::IncrementConnectionAttempts(bool success) {
    cumulative_stats_.connection_attempts.fetch_add(1);
    if (success) {
        cumulative_stats_.successful_connections.fetch_add(1);
    } else {
        cumulative_stats_.connection_failures.fetch_add(1);
    }
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
}

inline void BACnetStatisticsManager::IncrementErrorCount(const std::string& error_type) {
    cumulative_stats_.total_errors.fetch_add(1);
    
    if (!error_type.empty()) {
        std::lock_guard<std::mutex> lock(error_analysis_mutex_);
        error_counts_by_type_[error_type].fetch_add(1);
    }
    
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
}

inline void BACnetStatisticsManager::IncrementMessagesReceived() {
    cumulative_stats_.messages_received.fetch_add(1);
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
}

inline void BACnetStatisticsManager::IncrementMessagesSent() {
    cumulative_stats_.messages_sent.fetch_add(1);
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
}

inline void BACnetStatisticsManager::SetConnectionStatus(bool connected) {
    cumulative_stats_.is_connected.store(connected);
    cumulative_stats_.last_update_time = std::chrono::system_clock::now();
}

inline double BACnetStatisticsManager::CalculateSuccessRate(uint64_t successful, uint64_t total) const {
    return total > 0 ? (static_cast<double>(successful) / total) * 100.0 : 0.0;
}

inline std::chrono::seconds BACnetStatisticsManager::GetRuntime() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - cumulative_stats_.start_time);
}

inline bool BACnetStatisticsManager::IsCacheExpired(std::chrono::system_clock::time_point last_update) const {
    auto now = std::chrono::system_clock::now();
    return (now - last_update) > CACHE_REFRESH_INTERVAL;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_STATISTICS_MANAGER_H