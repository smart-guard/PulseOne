// =============================================================================
// collector/include/Alarm/AlarmManager.h
// 알람 관리자 - 최상위 인터페이스 (간소화)
// =============================================================================

#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include "Alarm/AlarmTypes.h"
#include <memory>

namespace PulseOne {
namespace Alarm {

// Forward declarations
class AlarmEvaluator;
class AlarmRepository;
class AlarmNotifier;
class AlarmSuppressor;
class AlarmStatisticsCollector;

class AlarmManager {
public:
    // 싱글톤
    static AlarmManager& getInstance();
    
    // 초기화
    AlarmErrorCode initialize(const nlohmann::json& config);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // === 규칙 관리 (Repository에 위임) ===
    AlarmErrorCode loadAlarmRules(int tenant_id);
    AlarmErrorCode addAlarmRule(const AlarmRule& rule);
    AlarmErrorCode updateAlarmRule(int rule_id, const AlarmRule& rule);
    AlarmErrorCode deleteAlarmRule(int rule_id);
    std::optional<AlarmRule> getAlarmRule(int rule_id) const;
    
    // === 알람 체크 (주요 인터페이스) ===
    void checkAlarmsForPoint(int point_id, const nlohmann::json& value, 
                            const std::string& point_type = "data_point");
    void checkAlarmsForVirtualPoint(int vp_id, const nlohmann::json& value);
    
    // === 알람 상태 관리 ===
    std::optional<int64_t> raiseAlarm(int rule_id, const AlarmEvaluation& eval);
    AlarmErrorCode clearAlarm(int64_t occurrence_id, const nlohmann::json& clear_value = {});
    AlarmErrorCode acknowledgeAlarm(int64_t occurrence_id, int user_id, 
                                   const std::string& comment = "");
    
    // === 조회 (Repository에 위임) ===
    std::vector<AlarmOccurrence> getActiveAlarms(int tenant_id) const;
    std::vector<AlarmOccurrence> getAlarms(const AlarmFilter& filter) const;
    
    // === 통계 (StatisticsCollector에 위임) ===
    AlarmStatistics getStatistics(int tenant_id) const;
    
private:
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;
    
    // 내부 처리
    void processAlarmEvaluation(const AlarmRule& rule, const AlarmEvaluation& eval);
    
private:
    // 컴포넌트들
    std::unique_ptr<AlarmEvaluator> evaluator_;
    std::unique_ptr<AlarmRepository> repository_;
    std::unique_ptr<AlarmNotifier> notifier_;
    std::unique_ptr<AlarmSuppressor> suppressor_;
    std::unique_ptr<AlarmStatisticsCollector> statistics_;
    
    // 상태
    bool initialized_ = false;
    nlohmann::json config_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_MANAGER_H
