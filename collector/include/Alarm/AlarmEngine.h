// =============================================================================
// collector/include/Alarm/AlarmEngine.h
// PulseOne 알람 엔진 헤더 - 기존 구조 100% 호환
// =============================================================================

#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <optional>

// 🔥 기존 구조와 완전 호환
#include "Common/Structs.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Utils/LogManager.h"

namespace PulseOne {

// Forward declarations (순환 참조 방지)
class RedisClientImpl;
class RabbitMQClient;

namespace Alarm {

// =============================================================================
// 타입 별칭들 (기존 Structs.h 사용)
// =============================================================================
using DeviceDataMessage = Structs::DeviceDataMessage;
using TimestampedValue = Structs::TimestampedValue;
using DataValue = Structs::DataValue;
using AlarmEvent = Structs::AlarmEventEnhanced;
using AlarmRuleEntity = Database::Entities::AlarmRuleEntity;
using AlarmOccurrenceEntity = Database::Entities::AlarmOccurrenceEntity;
using AlarmRuleRepository = Database::Repositories::AlarmRuleRepository;
using AlarmOccurrenceRepository = Database::Repositories::AlarmOccurrenceRepository;

// =============================================================================
// 알람 평가 결과
// =============================================================================
struct AlarmEvaluation {
    bool should_trigger = false;
    bool should_clear = false;
    bool state_changed = false;
    
    std::string alarm_level;     // "high_high", "high", "normal", "low", "low_low"
    std::string condition_met;
    std::string message;
    std::string severity;
    
    std::chrono::microseconds evaluation_time{0};
};

// =============================================================================
// AlarmEngine 클래스 - 싱글톤 패턴
// =============================================================================
class AlarmEngine {
public:
    // 싱글톤 인스턴스
    static AlarmEngine& getInstance();
    
    // ==========================================================================
    // 초기화/종료
    // ==========================================================================
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                   std::shared_ptr<RedisClientImpl> redis_client = nullptr,
                   std::shared_ptr<RabbitMQClient> mq_client = nullptr);
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }
    
    // ==========================================================================
    // 🔥 Pipeline 인터페이스 - DataProcessingService에서 호출
    // ==========================================================================
    
    /**
     * @brief 메시지에 대한 알람 평가 수행 (메인 인터페이스)
     * @param message 디바이스 데이터 메시지
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& message);
    
    /**
     * @brief 개별 포인트에 대한 알람 평가
     * @param tenant_id 테넌트 ID
     * @param point_id 포인트 ID (형식: "dp_123" 또는 "vp_456")
     * @param value 현재 값
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForPoint(int tenant_id, 
                                           const std::string& point_id, 
                                           const DataValue& value);
    
    // ==========================================================================
    // 알람 규칙 관리
    // ==========================================================================
    bool loadAlarmRules(int tenant_id);
    bool reloadAlarmRule(int rule_id);
    std::vector<AlarmRuleEntity> getAlarmRulesForPoint(int tenant_id, 
                                                      const std::string& point_type, 
                                                      int point_id) const;
    
    // ==========================================================================
    // 개별 평가 메서드들
    // ==========================================================================
    
    /**
     * @brief 아날로그 알람 평가 (임계값 + 히스테리시스)
     * @param rule 알람 규칙
     * @param value 현재 값
     * @return 평가 결과
     */
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value);
    
    /**
     * @brief 디지털 알람 평가 (상태 변화 감지)
     * @param rule 알람 규칙
     * @param state 현재 상태
     * @return 평가 결과
     */
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool state);
    
    /**
     * @brief 스크립트 기반 알람 평가 (JavaScript)
     * @param rule 알람 규칙
     * @param context 평가 컨텍스트 (JSON)
     * @return 평가 결과
     */
    AlarmEvaluation evaluateScriptAlarm(const AlarmRuleEntity& rule, 
                                       const nlohmann::json& context);
    
    // ==========================================================================
    // 알람 발생/해제
    // ==========================================================================
    std::optional<int64_t> raiseAlarm(const AlarmRuleEntity& rule, 
                                     const AlarmEvaluation& eval,
                                     const DataValue& trigger_value);
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value);
    bool acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment = "");
    
    // ==========================================================================
    // 조회 및 통계
    // ==========================================================================
    std::vector<AlarmOccurrenceEntity> getActiveAlarms(int tenant_id) const;
    std::optional<AlarmOccurrenceEntity> getAlarmOccurrence(int64_t occurrence_id) const;
    nlohmann::json getStatistics() const;
    
private:
    // 생성자/소멸자 (싱글톤)
    AlarmEngine();
    ~AlarmEngine();
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;
    
    // ==========================================================================
    // 내부 평가 로직
    // ==========================================================================
    AlarmEvaluation evaluateRule(const AlarmRuleEntity& rule, const DataValue& value);
    
    // 히스테리시스 처리
    bool checkDeadband(const AlarmRuleEntity& rule, double current, double previous, double threshold);
    std::string getAnalogLevel(const AlarmRuleEntity& rule, double value);
    
    // 메시지 생성
    std::string generateMessage(const AlarmRuleEntity& rule, 
                              const AlarmEvaluation& eval, 
                              const DataValue& value);
    std::string interpolateTemplate(const std::string& tmpl, 
                                  const std::map<std::string, std::string>& variables);
    
    // 알람 상태 관리
    bool isAlarmActive(int rule_id) const;
    void updateAlarmState(int rule_id, bool active);
    double getLastValue(int rule_id) const;
    void updateLastValue(int rule_id, double value);
    bool getLastDigitalState(int rule_id) const;
    void updateLastDigitalState(int rule_id, bool state);
    
    // 외부 시스템 연동
    void publishToRedis(const AlarmEvent& event);
    void sendToMessageQueue(const AlarmEvent& event);
    
    // JavaScript 엔진 (스크립트 알람용)
    bool initScriptEngine();
    void cleanupScriptEngine();
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 상태
    std::atomic<bool> initialized_{false};
    
    // Database 연결
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    std::shared_ptr<AlarmRuleRepository> alarm_rule_repo_;
    std::shared_ptr<AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // 외부 연결
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<RabbitMQClient> mq_client_;
    
    // 알람 규칙 캐시 (성능 최적화)
    std::map<int, std::vector<AlarmRuleEntity>> tenant_rules_;  // tenant_id -> rules
    std::map<std::string, std::vector<int>> point_rule_index_;  // "dp_123" -> rule_ids
    mutable std::shared_mutex rules_cache_mutex_;
    
    // 알람 상태 캐시 (런타임 상태)
    std::map<int, bool> alarm_states_;           // rule_id -> in_alarm
    std::map<int, double> last_values_;          // rule_id -> last_value
    std::map<int, bool> last_digital_states_;    // rule_id -> last_digital_state
    std::map<int, std::chrono::system_clock::time_point> last_check_times_;
    mutable std::shared_mutex state_cache_mutex_;
    
    // 활성 알람 매핑
    std::map<int, int64_t> rule_occurrence_map_;  // rule_id -> occurrence_id
    mutable std::shared_mutex occurrence_map_mutex_;
    
    // 통계
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    std::atomic<uint64_t> evaluations_errors_{0};
    
    // ID 생성기
    std::atomic<int64_t> next_occurrence_id_{1};
    
    // JavaScript 엔진 (QuickJS)
    void* js_runtime_ = nullptr;
    void* js_context_ = nullptr;
    mutable std::mutex js_mutex_;
    
    // 로거
    Utils::LogManager& logger_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_ENGINE_H