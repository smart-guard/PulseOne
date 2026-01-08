// =============================================================================
// collector/include/Alarm/AlarmManager.h - 완성본 (Redis 의존성 제거)
// =============================================================================

#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

#include "Common/Structs.h"
#include "Alarm/AlarmTypes.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

// ❌ 제거: Redis 의존성 완전 제거
// #include "Client/RedisClientImpl.h"

namespace PulseOne {
namespace Alarm {

    using json = nlohmann::json;
    using AlarmEvent = Structs::AlarmEvent;
    using DeviceDataMessage = Structs::DeviceDataMessage;
    using DataValue = Structs::DataValue;
    using AlarmType = PulseOne::Alarm::AlarmType;
    using AlarmSeverity = PulseOne::Alarm::AlarmSeverity;
    using TargetType = PulseOne::Alarm::TargetType;
    using DigitalTrigger = PulseOne::Alarm::DigitalTrigger;

/**
 * @brief 순수 알람 관리자 - 핵심 알람 로직만 담당
 * @details Redis 이벤트 발송은 DataProcessingService가 담당
 */
class AlarmManager {
public:
    // =======================================================================
    // 🎯 싱글톤 패턴
    // =======================================================================
    static AlarmManager& getInstance();
    
    // =======================================================================
    // 🎯 라이프사이클 관리
    // =======================================================================
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    // =======================================================================
    // 🎯 핵심 알람 평가 인터페이스
    // =======================================================================
    
    /**
     * @brief 디바이스 메시지에 대한 알람 평가 및 비즈니스 로직 적용
     * @param msg 디바이스 데이터 메시지
     * @return 강화된 알람 이벤트들 (외부 발송은 호출자가 담당)
     */
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& msg);
    
    // =======================================================================
    // 🎯 알람 규칙 관리
    // =======================================================================
    bool loadAlarmRules(int tenant_id);
    bool reloadAlarmRule(int rule_id);
    std::optional<AlarmRule> getAlarmRule(int rule_id) const;
    std::vector<AlarmRule> getAlarmRulesForPoint(int point_id, const std::string& point_type) const;
    
    // =======================================================================
    // 🎯 알람 평가 메서드들 (AlarmEngine 위임)
    // =======================================================================
    AlarmEvaluation evaluateRule(const AlarmRule& rule, const DataValue& value);
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRule& rule, double value);
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRule& rule, bool state);
    AlarmEvaluation evaluateScriptAlarm(const AlarmRule& rule, const json& context);
    
    // =======================================================================
    // 🎯 알람 상태 관리
    // =======================================================================
    std::optional<int64_t> raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval);
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value = 0.0);
    bool acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment = "");
    
    // =======================================================================
    // 🎯 메시지 생성
    // =======================================================================
    std::string generateMessage(const AlarmRule& rule, const DataValue& value, 
                               const std::string& condition = "");
    std::string generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event);
    std::string generateCustomMessage(const AlarmRule& rule, const DataValue& value);
    
    // =======================================================================
    // 🎯 통계 정보
    // =======================================================================
    json getStatistics() const;

private:
    // =======================================================================
    // 🎯 싱글톤 생성자/소멸자
    // =======================================================================
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;
    
    // =======================================================================
    // 🎯 초기화 메서드들
    // =======================================================================
    void initializeData();          // 초기 데이터 로드
    bool initScriptEngine();        // JavaScript 엔진 초기화
    void cleanupScriptEngine();     // JavaScript 엔진 정리
    
    // =======================================================================
    // 🎯 비즈니스 로직 메서드들
    // =======================================================================
    void enhanceAlarmEventWithBusinessLogic(AlarmEvent& event, const DeviceDataMessage& msg);
    void adjustSeverityByBusinessRules(AlarmEvent& event);
    void addLocationAndContext(AlarmEvent& event, const DeviceDataMessage& msg);
    void generateLocalizedMessage(AlarmEvent& event);
    void analyzeContinuousAlarmPattern(AlarmEvent& event);
    void applyCategorySpecificRules(AlarmEvent& event);
    
    // =======================================================================
    // 🎯 유틸리티 메서드들
    // =======================================================================
    Database::Entities::AlarmRuleEntity convertToEntity(const AlarmRule& rule);
    std::string interpolateTemplate(const std::string& tmpl, 
                                   const std::map<std::string, std::string>& variables);

private:
    // =======================================================================
    // 🎯 멤버 변수들 (간소화됨)
    // =======================================================================
    
    // 상태 관리
    std::atomic<bool> initialized_{false};
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    std::atomic<int64_t> next_occurrence_id_{1};
    
    // JavaScript 엔진
    void* js_runtime_{nullptr};  // JSRuntime*
    void* js_context_{nullptr};  // JSContext*
    
    // ❌ 제거: Redis 클라이언트
    // std::shared_ptr<RedisClientImpl> redis_client_;
    
    // 동기화
    mutable std::shared_mutex rules_mutex_;
    mutable std::shared_mutex index_mutex_;
    mutable std::mutex js_mutex_;
    
    // 데이터 저장
    std::map<int, AlarmRule> alarm_rules_;
    std::map<int, std::vector<int>> point_alarm_map_;  // point_id -> [rule_ids]
    std::map<std::string, std::vector<int>> group_alarm_map_;  // group -> [rule_ids]
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_MANAGER_H