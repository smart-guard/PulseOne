// =============================================================================
// collector/include/Alarm/AlarmEvaluator.h  
// 알람 평가 엔진 (분리)
// =============================================================================

#ifndef ALARM_EVALUATOR_H
#define ALARM_EVALUATOR_H

#include "Alarm/AlarmTypes.h"
#include <memory>
#include <mutex>

namespace PulseOne {
namespace Alarm {

class AlarmEvaluator {
public:
    AlarmEvaluator();
    ~AlarmEvaluator();
    
    // 초기화
    bool initialize(const nlohmann::json& config);
    void shutdown();
    
    // 알람 평가 메서드
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRule& rule, double value);
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRule& rule, bool state);
    AlarmEvaluation evaluateScriptAlarm(const AlarmRule& rule, const nlohmann::json& context);
    
    // 히스테리시스 처리
    bool checkDeadband(const AlarmRule& rule, double current_value, double last_value);
    
    // 스크립트 엔진
    bool evaluateJavaScript(const std::string& script, 
                           const nlohmann::json& context,
                           nlohmann::json& result);
    
private:
    // 아날로그 레벨 판정
    AnalogAlarmLevel determineAnalogLevel(const AlarmRule::AnalogLimits& limits, double value);
    
    // 디지털 엣지 감지
    bool detectDigitalEdge(DigitalTrigger trigger, bool current, bool previous);
    
    // JavaScript 엔진 (QuickJS)
    std::unique_ptr<void, std::function<void(void*)>> js_runtime_;
    std::unique_ptr<void, std::function<void(void*)>> js_context_;
    std::mutex js_mutex_;
    
    bool initialized_ = false;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_EVALUATOR_H