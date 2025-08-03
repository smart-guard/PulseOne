// =============================================================================
// collector/include/Drivers/Bacnet/BACnetStatisticsManager.h (부분 수정)
// PerformanceSnapshot 타입 불일치 해결
// =============================================================================

#ifndef BACNET_STATISTICS_MANAGER_H
#define BACNET_STATISTICS_MANAGER_H

#include "Drivers/Bacnet/BACnetCommonTypes.h"  // BACnetStatistics, PerformanceMetrics 정의 포함
#include "Common/UnifiedCommonTypes.h"
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 드라이버 통계 관리자
 * @details 성능 모니터링, 통계 수집, 캐싱 기능 제공
 */
class BACnetStatisticsManager {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit BACnetStatisticsManager(std::chrono::seconds cache_ttl = std::chrono::seconds(30));
    ~BACnetStatisticsManager();
    
    // 복사/이동 방지
    BACnetStatisticsManager(const BACnetStatisticsManager&) = delete;
    BACnetStatisticsManager& operator=(const BACnetStatisticsManager&) = delete;
    
    // ==========================================================================
    // 통계 조회 인터페이스
    // ==========================================================================
    
    /**
     * @brief 표준 드라이버 통계 반환 (캐시됨)
     */
    const PulseOne::Structs::DriverStatistics& GetStandardStatistics() const;
    
    /**
     * @brief BACnet 특화 통계 반환 (캐시됨)
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    /**
     * @brief 성능 히스토리 조회
     * @param duration 조회할 기간
     * @return 성능 스냅샷 목록
     */
    // 🔥 수정: BACnetCommonTypes.h의 PerformanceSnapshot 사용
    std::vector<PerformanceSnapshot> GetPerformanceHistory(
        std::chrono::minutes duration = std::chrono::minutes(60)
    ) const;
    
    /**
     * @brief 현재 성능 메트릭스 반환
     */
    PerformanceMetrics GetCurrentPerformance() const;
    
    // ==========================================================================
    // 통계 업데이트 인터페이스
    // ==========================================================================
    
    /**
     * @brief 읽기 작업 완료 기록
     */
    void RecordReadOperation(bool success, std::chrono::milliseconds duration);
    
    /**
     * @brief 쓰기 작업 완료 기록
     */
    void RecordWriteOperation(bool success, std::chrono::milliseconds duration);
    
    /**
     * @brief BACnet 서비스 요청 기록
     */
    void RecordBACnetService(const std::string& service_name, bool success);
    
    /**
     * @brief BACnet 에러 기록
     */
    void RecordBACnetError(BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
    
    /**
     * @brief 네트워크 통계 업데이트
     */
    void RecordNetworkActivity(size_t bytes_sent, size_t bytes_received, bool is_broadcast);
    
    /**
     * @brief Discovery 결과 기록
     */
    void RecordDiscoveryResult(size_t devices_found, size_t objects_found);
    
    /**
     * @brief 캐시 사용 기록
     */
    void RecordCacheAccess(bool hit);
    
    // ==========================================================================
    // 제어 인터페이스
    // ==========================================================================
    
    /**
     * @brief 모든 통계 초기화
     */
    void ResetAllStatistics();
    
    /**
     * @brief 캐시 강제 업데이트
     */
    void ForceUpdateCache();
    
    /**
     * @brief 성능 히스토리 저장 시작/중지
     */
    void EnablePerformanceHistory(bool enable);
    
private:
    // ==========================================================================
    // 내부 타입 정의 (BACnetCommonTypes.h와 중복 제거)
    // ==========================================================================
    
    // 🔥 제거: PerformanceSnapshot은 BACnetCommonTypes.h에서 정의됨
    // struct PerformanceSnapshot { ... }  // 삭제됨
    
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
    
    // 설정
    std::chrono::seconds cache_ttl_;
    bool performance_history_enabled_;
    
    // 누적 통계
    CumulativeStats cumulative_stats_;
    
    // 캐시된 통계 (스레드 안전성을 위한 뮤텍스와 함께)
    mutable std::mutex standard_stats_mutex_;
    mutable std::mutex bacnet_stats_mutex_;
    mutable std::mutex history_mutex_;
    
    mutable std::unique_ptr<PulseOne::Structs::DriverStatistics> standard_statistics_cache_;
    mutable BACnetStatistics bacnet_statistics_cache_;
    
    // 성능 히스토리
    mutable std::deque<PerformanceSnapshot> performance_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 1440; // 24시간 (1분 간격)
    
    // 캐시 유효성 추적
    mutable std::chrono::system_clock::time_point last_standard_update_;
    mutable std::chrono::system_clock::time_point last_bacnet_update_;
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    bool IsCacheExpired(const std::chrono::system_clock::time_point& last_update) const;
    void UpdateStandardStatisticsCache() const;
    void UpdateBACnetStatisticsCache() const;
    void AddPerformanceSnapshot();
    
    // 계산 헬퍼들
    double CalculateSuccessRate(uint64_t successful, uint64_t total) const;
    double CalculateAverageResponseTime() const;
    std::chrono::seconds GetRuntime() const;
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_STATISTICS_MANAGER_H