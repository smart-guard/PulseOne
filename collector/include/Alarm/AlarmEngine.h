// =============================================================================
// collector/include/Alarm/AlarmEngine.h
// PulseOne 알람 엔진 헤더 - 실제 구현부와 완전 일치
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

// 🔥 기존 구조와 완전 호환 + 누락된 include 추가
#include "Common/Structs.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"  // 🔥 누락된 include 추가

// 🔥 JSON 지원 (스크립트 알람용)
namespace nlohmann {
    class json;
}

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
using AlarmEvent = Structs::AlarmEvent;
using AlarmRuleEntity = Database::Entities::AlarmRuleEntity;
using AlarmOccurrenceEntity = Database::Entities::AlarmOccurrenceEntity;
using AlarmRuleRepository = Database::Repositories::AlarmRuleRepository;
using AlarmOccurrenceRepository = Database::Repositories::AlarmOccurrenceRepository;

// =============================================================================
// 알람 평가 결과 - 🔥 실제 구현부와 일치
// =============================================================================
struct AlarmEvaluation {
    bool should_trigger = false;
    bool should_clear = false;
    bool state_changed = false;
    
    std::string triggered_value;
    std::string message;
    std::string severity;
    std::string condition_met;
    std::string alarm_level;
    
    std::chrono::microseconds evaluation_time{0};
    std::chrono::system_clock::time_point timestamp;
    
    int rule_id = 0;
    int tenant_id = 0;
};

// =============================================================================
// 알람 상태 추적
// =============================================================================
struct AlarmState {
    int rule_id = 0;
    bool is_active = false;
    int occurrence_id = 0;  // 0이면 비활성
    std::chrono::system_clock::time_point last_evaluation = std::chrono::system_clock::now();
    std::string last_value;
    int consecutive_triggers = 0;  // 연속 트리거 횟수 (히스테리시스용)
};

// =============================================================================
// 메인 알람 엔진 클래스
// =============================================================================
class AlarmEngine {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    
    static AlarmEngine& getInstance();
    
    // =======================================================================
    // 🔥 실제 구현부와 일치하는 초기화 (단순화된 버전)
    // =======================================================================
    
    /**
     * @brief 알람 엔진 초기화
     * @return 성공 여부
     */
    bool initialize();
    
    /**
     * @brief 알람 엔진 종료
     */
    void shutdown();
    
    /**
     * @brief 초기화 상태 확인
     */
    bool isInitialized() const { return initialized_.load(); }

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 메인 인터페이스
    // =======================================================================
    
    /**
     * @brief 디바이스 메시지에 대해 알람 평가 실행 (실제 구현부 메서드)
     * @param message 디바이스 데이터 메시지
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& message);
    
    /**
     * @brief 개별 포인트에 대해 알람 평가 (실제 구현부 메서드)
     * @param tenant_id 테넌트 ID
     * @param point_id 포인트 ID (예: "dp_123", "vp_456")
     * @param value 현재 값
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForPoint(int tenant_id, 
                                           const std::string& point_id, 
                                           const DataValue& value);

    /**
     * @brief 단일 규칙에 대해 알람 평가 (실제 구현부 메서드)
     * @param rule 알람 규칙
     * @param value 현재 값
     * @return 평가 결과
     */
    AlarmEvaluation evaluateRule(const AlarmRuleEntity& rule, const DataValue& value);

    // =======================================================================
    // 🔥 기존 헤더의 유용한 메서드들 유지
    // =======================================================================
    
    /**
     * @brief 모든 데이터에 대해 알람 평가 실행
     * @param messages 디바이스 데이터 메시지들
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateAlarms(const std::vector<DeviceDataMessage>& messages);
    
    /**
     * @brief 단일 데이터 포인트에 대해 알람 평가
     * @param target_type 대상 타입 ('data_point', 'virtual_point')
     * @param target_id 대상 ID
     * @param current_value 현재 값
     * @return 평가 결과
     */
    std::vector<AlarmEvaluation> evaluateDataPoint(const std::string& target_type, 
                                                  int target_id, 
                                                  const DataValue& current_value);

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 알람 발생 관리
    // =======================================================================
    
    /**
     * @brief 알람 발생 생성 (실제 구현부 메서드)
     * @param rule 알람 규칙
     * @param eval 평가 결과
     * @param trigger_value 트리거 값
     * @return 생성된 AlarmOccurrence ID (실패 시 nullopt)
     */
    std::optional<int64_t> raiseAlarm(const AlarmRuleEntity& rule, 
                                     const AlarmEvaluation& eval,
                                     const DataValue& trigger_value);
    
    /**
     * @brief 알람 해제 (실제 구현부 메서드)
     * @param occurrence_id 발생 ID
     * @param clear_value 해제 시 값
     * @return 성공 여부
     */
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value);
    
    /**
     * @brief 알람 해제 (기존 헤더 호환)
     * @param rule_id 규칙 ID
     * @param cleared_value 해제 시 값
     * @param comment 해제 코멘트
     * @return 성공 여부
     */
    bool clearAlarm(int rule_id, const std::string& cleared_value = "", const std::string& comment = "Auto-cleared");
    
    /**
     * @brief 알람 발생 생성 (기존 헤더 호환)
     * @param evaluation 평가 결과
     * @return 생성된 AlarmOccurrence ID (실패 시 -1)
     */
    int createAlarmOccurrence(const AlarmEvaluation& evaluation);
    
    /**
     * @brief 알람 인지 처리
     * @param occurrence_id 발생 ID
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 여부
     */
    bool acknowledgeAlarm(int occurrence_id, int user_id, const std::string& comment = "");

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 상태 조회
    // =======================================================================
    
    /**
     * @brief 엔진 통계 조회 (실제 구현부 메서드)
     */
    nlohmann::json getStatistics() const;
    
    /**
     * @brief 활성 알람 조회 (실제 구현부 메서드)
     */
    std::vector<AlarmOccurrenceEntity> getActiveAlarms(int tenant_id) const;
    
    /**
     * @brief 알람 발생 상세 조회 (실제 구현부 메서드)
     */
    std::optional<AlarmOccurrenceEntity> getAlarmOccurrence(int64_t occurrence_id) const;
    
    /**
     * @brief 활성 알람 개수 조회
     */
    int getActiveAlarmCount() const;
    
    /**
     * @brief 심각도별 활성 알람 개수 조회
     */
    std::map<AlarmOccurrenceEntity::Severity, int> getActiveAlarmsBySeverity() const;
    
    /**
     * @brief 특정 규칙의 활성 상태 확인
     */
    bool isAlarmActive(int rule_id) const;

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 규칙 관리
    // =======================================================================
    
    /**
     * @brief 특정 테넌트의 알람 규칙 로드 (실제 구현부 메서드)
     */
    bool loadAlarmRules(int tenant_id);
    
    /**
     * @brief 포인트에 해당하는 알람 규칙들 조회 (실제 구현부 메서드)
     */
    std::vector<AlarmRuleEntity> getAlarmRulesForPoint(int tenant_id, 
                                                      const std::string& point_type, 
                                                      int point_id) const;
    
    /**
     * @brief 알람 규칙 다시 로드
     */
    void reloadAlarmRules();
    
    /**
     * @brief 특정 규칙 활성화/비활성화
     */
    bool setRuleEnabled(int rule_id, bool enabled);

private:
    // =======================================================================
    // 🔥 실제 구현부와 일치하는 생성자/소멸자
    // =======================================================================
    
    AlarmEngine();
    ~AlarmEngine();
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 내부 초기화 메서드들
    // =======================================================================
    void initializeClients();      // Redis 등 클라이언트 생성
    void initializeRepositories(); // Repository 초기화

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 핵심 알람 평가 메서드들
    // =======================================================================
    
    /**
     * @brief 아날로그 알람 평가 (실제 구현부 메서드)
     */
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value);
    
    /**
     * @brief 디지털 알람 평가 (실제 구현부 메서드)
     */
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool state);
    
    /**
     * @brief 스크립트 알람 평가 (실제 구현부 메서드)
     */
    AlarmEvaluation evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context);

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 헬퍼 메서드들
    // =======================================================================
    bool checkDeadband(const AlarmRuleEntity& rule, double current, double previous, double threshold);
    std::string getAnalogLevel(const AlarmRuleEntity& rule, double value);
    std::string generateMessage(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& value);
    std::string interpolateTemplate(const std::string& tmpl, const std::map<std::string, std::string>& variables);
    
    // 상태 관리
    void updateAlarmState(int rule_id, bool active);
    double getLastValue(int rule_id) const;
    void updateLastValue(int rule_id, double value);
    bool getLastDigitalState(int rule_id) const;
    void updateLastDigitalState(int rule_id, bool state);
    
    // 외부 시스템 연동
    void publishToRedis(const AlarmEvent& event);

    // =======================================================================
    // JavaScript 엔진 관련 (실제 구현부와 일치)
    // =======================================================================
    bool initScriptEngine();
    void cleanupScriptEngine();

    // =======================================================================
    // 🔥 실제 구현부와 일치하는 멤버 변수들
    // =======================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_{false};
    
    // 🔥 실제 구현부와 일치하는 싱글톤 참조
    Database::DatabaseManager& db_manager_;
    Utils::LogManager& logger_;
    
    // 🔥 내부에서 생성하는 객체들
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<AlarmRuleRepository> alarm_rule_repo_;
    std::shared_ptr<AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // JavaScript 엔진 (스크립트 알람용)
    void* js_runtime_{nullptr};  // JSRuntime*
    void* js_context_{nullptr};  // JSContext*
    std::mutex js_mutex_;
    
    // 알람 규칙 캐시
    mutable std::shared_mutex rules_cache_mutex_;
    std::map<int, std::vector<AlarmRuleEntity>> tenant_rules_;  // tenant_id -> rules
    std::map<std::string, std::vector<int>> point_rule_index_;  // "t{tenant}_type_{id}" -> rule_ids
    
    // 알람 상태 캐시
    mutable std::shared_mutex state_cache_mutex_;
    std::map<int, bool> alarm_states_;           // rule_id -> is_active
    std::map<int, double> last_values_;          // rule_id -> last_value
    std::map<int, bool> last_digital_states_;   // rule_id -> last_state
    std::map<int, std::chrono::system_clock::time_point> last_check_times_;  // rule_id -> last_check
    
    // 발생 매핑
    mutable std::shared_mutex occurrence_map_mutex_;
    std::map<int, int64_t> rule_occurrence_map_; // rule_id -> occurrence_id
    
    // 통계
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    std::atomic<uint64_t> evaluations_errors_{0};
    std::atomic<int64_t> next_occurrence_id_{1};
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_ENGINE_H