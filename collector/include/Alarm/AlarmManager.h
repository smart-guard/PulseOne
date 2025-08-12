// =============================================================================
// collector/include/Alarm/AlarmManager.h
// PulseOne 알람 매니저 헤더 - 명확한 싱글톤 패턴 + 초기화 순서 수정
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
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

namespace PulseOne {

// Forward declarations
class RedisClientImpl;

namespace Alarm {

    using json = nlohmann::json;
    using AlarmEvent = Structs::AlarmEvent;
    using DeviceDataMessage = Structs::DeviceDataMessage;
    using DataValue = Structs::DataValue;
    using AlarmType = PulseOne::Alarm::AlarmType;
    using AlarmSeverity = PulseOne::Alarm::AlarmSeverity;
    using TargetType = PulseOne::Alarm::TargetType;
    using DigitalTrigger = PulseOne::Alarm::DigitalTrigger;

// =============================================================================
// AlarmManager 클래스 - 🔥 명확한 싱글톤 패턴
// =============================================================================
class AlarmManager {
public:
    // =======================================================================
    // 🔥 싱글톤 패턴 (AlarmEngine과 동일한 패턴)
    // =======================================================================
    static AlarmManager& getInstance();
    
    // =======================================================================
    // 🔥 생성자에서 자동 초기화 (AlarmEngine 패턴 따르기)
    // =======================================================================
    
    /**
     * @brief 알람 매니저 종료
     */
    void shutdown();
    
    /**
     * @brief 초기화 상태 확인
     */
    bool isInitialized() const { return initialized_.load(); }
    
    // =======================================================================
    // 🔥 메인 비즈니스 인터페이스 (Pipeline에서 호출)
    // =======================================================================
    
    /**
     * @brief 디바이스 메시지에 대해 고수준 알람 처리 (비즈니스 로직 포함)
     * @param msg 디바이스 데이터 메시지
     * @return 강화된 알람 이벤트들 (알림, 메시지 강화 포함)
     */
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& msg);
    
    // =======================================================================
    // 알람 규칙 관리 (비즈니스 레이어)
    // =======================================================================
    bool loadAlarmRules(int tenant_id);
    bool reloadAlarmRule(int rule_id);
    std::optional<AlarmRule> getAlarmRule(int rule_id) const;
    std::vector<AlarmRule> getAlarmRulesForPoint(int point_id, const std::string& point_type) const;
    
    // =======================================================================
    // 고수준 평가 메서드들 (AlarmEngine 위임 + 비즈니스 로직)
    // =======================================================================
    AlarmEvaluation evaluateRule(const AlarmRule& rule, const DataValue& value);
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRule& rule, double value);
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRule& rule, bool state);
    AlarmEvaluation evaluateScriptAlarm(const AlarmRule& rule, const json& context);
    
    // =======================================================================
    // 알람 발생/해제 (비즈니스 로직 포함)
    // =======================================================================
    std::optional<int64_t> raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval);
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value = 0.0);
    bool acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment = "");
    
    // =======================================================================
    // 메시지 생성 (고급 비즈니스 로직)
    // =======================================================================
    std::string generateMessage(const AlarmRule& rule, const DataValue& value, 
                               const std::string& condition = "");
    std::string generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event);
    std::string generateCustomMessage(const AlarmRule& rule, const DataValue& value);
    
    // =======================================================================
    // 통계 및 상태
    // =======================================================================
    json getStatistics() const;

private:
    // =======================================================================
    // 🔥 명확한 싱글톤 생성자/소멸자 (AlarmEngine과 동일)
    // =======================================================================
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;
    
    // =======================================================================
    // 🔥 생성자에서 호출되는 초기화 메서드들
    // =======================================================================
    void initializeClients();       // Redis 등 클라이언트 생성
    void initializeData();          // 초기 데이터 로드
    bool initScriptEngine();        // JavaScript 엔진 초기화
    void cleanupScriptEngine();     // JavaScript 엔진 정리
    
    // =======================================================================
    // 🔥 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 이벤트를 비즈니스 규칙으로 강화
     */
    void enhanceAlarmEvent(AlarmEvent& event, const DeviceDataMessage& msg);
    
    /**
     * @brief 심각도 및 우선순위 조정 (시간대, 연속발생 등)
     */
    void adjustSeverityAndPriority(AlarmEvent& event, const AlarmRule& rule);
    
    /**
     * @brief AlarmRule을 AlarmRuleEntity로 변환 (AlarmEngine 호출용)
     */
    Database::Entities::AlarmRuleEntity convertToEntity(const AlarmRule& rule);
    
    // =======================================================================
    // 외부 시스템 연동 (고수준)
    // =======================================================================
    void publishToRedis(const AlarmEvent& event);    // 다중 채널 Redis 발송
    void sendNotifications(const AlarmEvent& event); // 이메일, SMS, Slack 등
    
    // =======================================================================
    // 데이터베이스 작업
    // =======================================================================
    bool loadAlarmRulesFromDB(int tenant_id);
    
    // =======================================================================
    // 메시지 템플릿 처리
    // =======================================================================
    std::string interpolateTemplate(const std::string& tmpl, 
                                   const std::map<std::string, std::string>& variables);

private:
    // =======================================================================
    // 🔥 멤버 변수들 - 초기화 순서에 맞게 정렬 (경고 해결)
    // =======================================================================
    
    // 1. 원시 타입들 (atomic 포함)
    std::atomic<bool> initialized_{false};
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    std::atomic<int64_t> next_occurrence_id_{1};
    
    // 2. 포인터들 (JavaScript 엔진)
    void* js_runtime_{nullptr};  // JSRuntime*
    void* js_context_{nullptr};  // JSContext*
    
    // 3. 스마트 포인터들
    std::shared_ptr<RedisClientImpl> redis_client_;
    
    // 4. 뮤텍스들 (mutable 키워드 순서)
    mutable std::shared_mutex rules_mutex_;
    mutable std::shared_mutex index_mutex_;
    mutable std::mutex js_mutex_;
    
    // 5. 컨테이너들 (복잡한 객체들)
    std::map<int, AlarmRule> alarm_rules_;
    std::map<int, std::vector<int>> point_alarm_map_;  // point_id -> [rule_ids]
    std::map<std::string, std::vector<int>> group_alarm_map_;  // group -> [rule_ids]
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_MANAGER_H