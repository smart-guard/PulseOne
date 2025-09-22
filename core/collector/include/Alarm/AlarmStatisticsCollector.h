// =============================================================================
// collector/include/Alarm/AlarmStatisticsCollector.h
// 알람 통계 수집 (선택적 컴포넌트)
// =============================================================================

#ifndef ALARM_STATISTICS_COLLECTOR_H
#define ALARM_STATISTICS_COLLECTOR_H

#include "Alarm/AlarmTypes.h"
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace PulseOne {
namespace Alarm {

class AlarmStatisticsCollector {
public:
    AlarmStatisticsCollector();
    ~AlarmStatisticsCollector();
    
    // 이벤트 기록
    void recordAlarmRaised(int tenant_id, AlarmSeverity severity);
    void recordAlarmCleared(int tenant_id);
    void recordAlarmAcknowledged(int tenant_id);
    void recordEvaluationTime(int tenant_id, std::chrono::microseconds time);
    
    // 통계 조회
    AlarmStatistics getStatistics(int tenant_id) const;
    nlohmann::json getDetailedStatistics(int tenant_id) const;
    
    // 리셋
    void resetStatistics(int tenant_id);
    void resetAllStatistics();
    
private:
    struct TenantStatistics {
        AlarmStatistics current;
        std::vector<std::chrono::microseconds> evaluation_times;
        std::chrono::system_clock::time_point last_reset;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<int, TenantStatistics> statistics_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_STATISTICS_COLLECTOR_H