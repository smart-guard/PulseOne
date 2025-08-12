// =============================================================================
// collector/include/Alarm/AlarmEngine.h - 컴파일 오류 해결
// =============================================================================

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <optional>
#include <variant>

// 🔥 프로젝트 헤더들 (순서 중요!)
#include "Common/Structs.h"
#include "Alarm/AlarmTypes.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Client/RedisClientImpl.h"

// 🔥 JSON include
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 타입 별칭들
// =============================================================================
using DeviceDataMessage = Structs::DeviceDataMessage;
using DataValue = Structs::DataValue;
using AlarmEvent = Structs::AlarmEvent;
using TimestampedValue = Structs::TimestampedValue;
using AlarmRuleEntity = Database::Entities::AlarmRuleEntity;
using AlarmOccurrenceEntity = Database::Entities::AlarmOccurrenceEntity;
using AlarmRuleRepository = Database::Repositories::AlarmRuleRepository;
using AlarmOccurrenceRepository = Database::Repositories::AlarmOccurrenceRepository;

// =============================================================================
// AlarmEngine 클래스 선언
// =============================================================================
class AlarmEngine {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    static AlarmEngine& getInstance();
    
    // =======================================================================
    // 주요 인터페이스
    // =======================================================================
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    // 메인 평가 메서드들
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& message);
    std::vector<AlarmEvent> evaluateForPoint(int tenant_id, int point_id, const DataValue& value);
    
    // 개별 규칙 평가
    AlarmEvaluation evaluateRule(const AlarmRuleEntity& rule, const DataValue& value);
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value);
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool value);
    AlarmEvaluation evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context);
    
    // 알람 관리
    std::optional<int64_t> raiseAlarm(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& trigger_value);
    bool clearAlarm(int64_t occurrence_id, const DataValue& current_value);
    bool clearActiveAlarm(int rule_id, const DataValue& current_value);
    
    // 조회 및 통계
    nlohmann::json getStatistics() const;
    std::vector<AlarmOccurrenceEntity> getActiveAlarms(int tenant_id = 0) const;
    std::optional<AlarmOccurrenceEntity> getAlarmOccurrence(int64_t occurrence_id) const;
    
    // 상태 관리
    bool isAlarmActive(int rule_id) const;

private:
    // =======================================================================
    // 생성자/소멸자 (싱글톤)
    // =======================================================================
    AlarmEngine();
    ~AlarmEngine();
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;

    // =======================================================================
    // 초기화 메서드들
    // =======================================================================
    void initializeClients();
    void initializeRepositories();
    void loadInitialData();

    // =======================================================================
    // JavaScript 엔진 관련
    // =======================================================================
    bool initScriptEngine();
    void cleanupScriptEngine();
    bool registerSystemFunctions();

    // =======================================================================
    // 🔥 수정: 올바른 시그니처로 선언 (extra qualification 제거)
    // =======================================================================
    std::map<std::string, std::variant<double, bool, std::string>> 
    prepareScriptContext(const AlarmRuleEntity& rule, 
                        const std::vector<TimestampedValue>& point_values);
    nlohmann::json prepareScriptContextFromValue(const AlarmRuleEntity& rule, 
                                                          int point_id,
                                                          const DataValue& value);
    std::string generateScriptAlarmMessage(const AlarmRuleEntity& rule,
                                         const std::map<std::string, std::variant<double, bool, std::string>>& context,
                                         bool triggered);

    // =======================================================================
    // 헬퍼 메서드들
    // =======================================================================
    std::string generateMessage(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& value);
    std::vector<AlarmRuleEntity> getAlarmRulesForPoint(int tenant_id, const std::string& point_type, int target_id) const;
    
    // 상태 관리
    void updateAlarmState(int rule_id, bool active);
    double getLastValue(int rule_id) const;
    void updateLastValue(int rule_id, double value);
    bool getLastDigitalState(int rule_id) const;
    void updateLastDigitalState(int rule_id, bool state);
    
    // 외부 시스템 연동
    void publishToRedis(const AlarmEvent& event);
    void publishAlarmClearedEvent(const AlarmOccurrenceEntity& alarm);
    
    // 유틸리티
    UUID getDeviceIdForPoint(int point_id);
    std::string getPointLocation(int point_id);
    AlarmType convertToAlarmType(const AlarmRuleEntity::AlarmType& entity_type);
    TriggerCondition determineTriggerCondition(const AlarmRuleEntity& rule, const AlarmEvaluation& eval);
    double getThresholdValue(const AlarmRuleEntity& rule, const AlarmEvaluation& eval);
    size_t getActiveAlarmsCount() const;

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 상태 관리
    std::atomic<bool> initialized_{false};
    
    // Repository들
    std::shared_ptr<AlarmRuleRepository> alarm_rule_repo_;
    std::shared_ptr<AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // 클라이언트들
    std::shared_ptr<RedisClientImpl> redis_client_;
    bool redis_available_{false};
    
    // JavaScript 엔진
    void* js_runtime_{nullptr};  // JSRuntime*
    void* js_context_{nullptr};  // JSContext*
    
    // 캐시 및 상태 (스레드 안전)
    mutable std::shared_mutex rules_cache_mutex_;
    mutable std::shared_mutex state_cache_mutex_;
    mutable std::shared_mutex occurrence_map_mutex_;
    mutable std::mutex state_mutex_;  // 추가: 간단한 뮤텍스
    
    std::unordered_map<int, std::vector<AlarmRuleEntity>> tenant_rules_;
    std::unordered_map<std::string, std::vector<int>> point_rule_index_;
    std::unordered_map<int, bool> alarm_states_;
    std::unordered_map<int, double> last_values_;
    std::unordered_map<int, bool> last_digital_states_;
    std::unordered_map<int, std::chrono::system_clock::time_point> last_check_times_;
    std::unordered_map<int, int64_t> rule_occurrence_map_;
    
    // 통계
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    std::atomic<uint64_t> evaluations_errors_{0};
    
    // ID 관리
    std::atomic<int64_t> next_occurrence_id_{1};
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_ENGIN