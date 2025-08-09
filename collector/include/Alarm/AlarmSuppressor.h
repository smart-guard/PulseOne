// =============================================================================
// collector/include/Alarm/AlarmSuppressor.h
// 알람 억제 관리 (선택적 컴포넌트)
// =============================================================================

#ifndef ALARM_SUPPRESSOR_H
#define ALARM_SUPPRESSOR_H

#include "Alarm/AlarmTypes.h"
#include <unordered_map>
#include <mutex>

namespace PulseOne {
namespace Alarm {

class AlarmSuppressor {
public:
    AlarmSuppressor();
    ~AlarmSuppressor();
    
    // 억제 규칙 관리
    void addSuppressionRule(int rule_id, const nlohmann::json& suppression);
    void removeSuppressionRule(int rule_id);
    
    // 억제 체크
    bool isAlarmSuppressed(const AlarmRule& rule) const;
    bool checkTimeBasedSuppression(const nlohmann::json& rules) const;
    bool checkConditionBasedSuppression(const nlohmann::json& rules) const;
    
    // 임시 억제
    void setSuppression(int rule_id, bool suppress, std::chrono::seconds duration);
    
private:
    struct SuppressionInfo {
        bool is_suppressed = false;
        std::chrono::system_clock::time_point until;
        nlohmann::json rules;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<int, SuppressionInfo> suppressions_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_SUPPRESSOR_H