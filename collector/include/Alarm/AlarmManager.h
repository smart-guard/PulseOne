// =============================================================================
// collector/include/Alarm/AlarmManager.h
// PulseOne 알람 매니저 헤더 - 완성본
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
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"

namespace PulseOne {

// Forward declarations
class RedisClientImpl;
class RabbitMQClient;

namespace Alarm {

using json = nlohmann::json;
using AlarmEvent = Structs::AlarmEventEnhanced;
using DeviceDataMessage = Structs::DeviceDataMessage;
using TimestampedValue = Structs::TimestampedValue;
using DataValue = Structs::DataValue;

// =============================================================================
// 알람 규칙 정의
// =============================================================================
struct AlarmRule {
    // 기본 정보
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    
    // 대상 정보
    std::string target_type;     // "data_point", "virtual_point", "group"
    int target_id = 0;
    std::string target_group;
    
    // 알람 타입
    std::string alarm_type;      // "analog", "digital", "script"
    
    // 아날로그 알람 설정
    std::optional<double> high_high_limit;
    std::optional<double> high_limit;
    std::optional<double> low_limit;
    std::optional<double> low_low_limit;
    double deadband = 0.0;
    double rate_of_change = 0.0;
    
    // 디지털 알람 설정
    std::string trigger_condition;  // "on_true", "on_false", "on_change", "on_rising", "on_falling"
    
    // 스크립트 기반 알람
    std::string condition_script;
    std::string message_script;
    
    // 메시지 커스터마이징
    json message_config;         // 포인트별 커스텀 메시지
    std::string message_template;
    
    // 우선순위
    std::string severity = "medium";
    int priority = 100;
    
    // 자동 처리
    bool auto_acknowledge = false;
    int acknowledge_timeout_min = 0;
    bool auto_clear = true;
    
    // 억제 규칙
    json suppression_rules;
    
    // 알림 설정
    bool notification_enabled = true;
    int notification_delay_sec = 0;
    int notification_repeat_interval_min = 0;
    json notification_channels;
    json notification_recipients;
    
    // 상태
    bool is_enabled = true;
    bool is_latched = false;
    
    // 런타임 상태 (메모리에만 유지)
    mutable double last_value = 0.0;
    mutable bool last_digital_state = false;
    mutable std::chrono::system_clock::time_point last_check_time;
    mutable bool in_alarm_state = false;
};

// =============================================================================
// 알람 발생 정보
// =============================================================================
struct AlarmOccurrence {
    int64_t id = 0;
    int rule_id = 0;
    int tenant_id = 0;
    
    // 발생 정보
    std::chrono::system_clock::time_point occurrence_time;
    DataValue trigger_value;
    std::string trigger_condition;
    std::string alarm_message;
    std::string severity;
    
    // 상태
    std::string state = "active";  // "active", "acknowledged", "cleared", "suppressed"
    
    // Acknowledge 정보
    std::optional<std::chrono::system_clock::time_point> acknowledged_time;
    std::optional<int> acknowledged_by;
    std::string acknowledge_comment;
    
    // Clear 정보
    std::optional<std::chrono::system_clock::time_point> cleared_time;
    std::optional<DataValue> cleared_value;
    std::string clear_comment;
    
    // 알림 정보
    bool notification_sent = false;
    std::chrono::system_clock::time_point notification_time;
    int notification_count = 0;
    
    // 컨텍스트
    json context_data;
    std::string source_name;
    std::string location;
};

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
    json context_data;
    
    std::chrono::microseconds evaluation_time{0};
};

// =============================================================================
// AlarmManager 클래스
// =============================================================================
class AlarmManager {
public:
    // 싱글톤 패턴
    static AlarmManager& getInstance();
    
    // 초기화/종료
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                   std::shared_ptr<RedisClientImpl> redis_client = nullptr,
                   std::shared_ptr<RabbitMQClient> mq_client = nullptr);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // 알람 규칙 관리
    bool loadAlarmRules(int tenant_id);
    bool reloadAlarmRule(int rule_id);
    std::optional<AlarmRule> getAlarmRule(int rule_id) const;
    std::vector<AlarmRule> getAlarmRulesForPoint(int point_id, const std::string& point_type) const;
    
    // Pipeline에서 호출 - 메인 인터페이스
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& msg);
    
    // 개별 평가
    AlarmEvaluation evaluateRule(const AlarmRule& rule, const DataValue& value);
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRule& rule, double value);
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRule& rule, bool state);
    AlarmEvaluation evaluateScriptAlarm(const AlarmRule& rule, const json& context);
    
    // 알람 발생/해제
    std::optional<int64_t> raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval);
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value = 0.0);
    bool acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment = "");
    
    // 활성 알람 조회
    std::vector<AlarmOccurrence> getActiveAlarms(int tenant_id) const;
    std::optional<AlarmOccurrence> getAlarmOccurrence(int64_t occurrence_id) const;
    
    // 메시지 생성
    std::string generateMessage(const AlarmRule& rule, const DataValue& value, 
                               const std::string& condition = "");
    std::string generateCustomMessage(const AlarmRule& rule, const DataValue& value);
    
    // 통계
    json getStatistics() const;
    
private:
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;
    
    // 내부 평가 메서드
    bool checkAnalogThreshold(const AlarmRule& rule, double value, double& level_value);
    std::string getAnalogLevel(const AlarmRule& rule, double value);
    bool checkDigitalTrigger(const AlarmRule& rule, bool current, bool previous);
    
    // 히스테리시스 처리
    bool checkDeadband(const AlarmRule& rule, double current, double previous, double threshold);
    
    // 억제 확인
    bool isAlarmSuppressed(const AlarmRule& rule, const json& context) const;
    
    // 데이터베이스 작업
    bool loadAlarmRulesFromDB(int tenant_id);
    bool saveOccurrenceToDB(const AlarmOccurrence& occurrence);
    bool updateOccurrenceInDB(const AlarmOccurrence& occurrence);
    
    // 외부 시스템 연동
    void sendToMessageQueue(const AlarmEvent& event);
    void updateRedisCache(const AlarmOccurrence& occurrence);
    
    // 메시지 템플릿 처리
    std::string interpolateTemplate(const std::string& tmpl, 
                                   const std::map<std::string, std::string>& variables);
    
private:
    // 알람 규칙 저장소
    std::map<int, AlarmRule> alarm_rules_;
    mutable std::shared_mutex rules_mutex_;
    
    // 포인트별 알람 규칙 인덱스
    std::map<int, std::vector<int>> point_alarm_map_;  // point_id -> [rule_ids]
    std::map<std::string, std::vector<int>> group_alarm_map_;  // group -> [rule_ids]
    mutable std::shared_mutex index_mutex_;
    
    // 활성 알람
    std::map<int64_t, AlarmOccurrence> active_alarms_;
    std::map<int, int64_t> rule_occurrence_map_;  // rule_id -> occurrence_id
    mutable std::shared_mutex active_mutex_;
    
    // 통계
    std::atomic<uint64_t> total_evaluations_{0};
    std::atomic<uint64_t> alarms_raised_{0};
    std::atomic<uint64_t> alarms_cleared_{0};
    
    // 외부 연결
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<RabbitMQClient> mq_client_;
    
    // 로거
    Utils::LogManager& logger_;
    
    // ID 생성기
    std::atomic<int64_t> next_occurrence_id_{1};
    
    // 상태
    std::atomic<bool> initialized_{false};
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_MANAGER_H