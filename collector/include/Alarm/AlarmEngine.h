// =============================================================================
// collector/include/Alarm/AlarmEngine.h
// PulseOne 알람 엔진 헤더 - LogManager 타입 오류 완전 해결
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
#include "Utils/LogManager.h"  // ✅ LogManager 헤더 추가

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
// 알람 평가 결과
// =============================================================================
struct AlarmEvaluation {
    bool should_trigger = false;
    bool should_clear = false;
    bool state_changed = false;
    
    std::string triggered_value;
    std::string message;
    AlarmOccurrenceEntity::Severity severity = AlarmOccurrenceEntity::Severity::INFO;
    
    // 상태 정보
    int rule_id = 0;
    int target_id = 0;
    std::string target_type;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
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
    // 초기화/종료
    // =======================================================================
    
    /**
     * @brief 알람 엔진 초기화
     * @param db_manager 데이터베이스 매니저
     * @param redis_client Redis 클라이언트 (알람 이벤트 발송용)
     * @param mq_client RabbitMQ 클라이언트 (알림 발송용)
     * @return 성공 여부
     */
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                   std::shared_ptr<RedisClientImpl> redis_client = nullptr,
                   std::shared_ptr<RabbitMQClient> mq_client = nullptr);
    
    /**
     * @brief 알람 엔진 종료
     */
    void shutdown();
    
    /**
     * @brief 초기화 상태 확인
     */
    bool isInitialized() const { return initialized_.load(); }

    // =======================================================================
    // 알람 평가 메인 메서드
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
    // 알람 발생 관리
    // =======================================================================
    
    /**
     * @brief 알람 발생 생성
     * @param evaluation 평가 결과
     * @return 생성된 AlarmOccurrence ID (실패 시 -1)
     */
    int createAlarmOccurrence(const AlarmEvaluation& evaluation);
    
    /**
     * @brief 알람 해제
     * @param rule_id 규칙 ID
     * @param cleared_value 해제 시 값
     * @param comment 해제 코멘트
     * @return 성공 여부
     */
    bool clearAlarm(int rule_id, const std::string& cleared_value = "", const std::string& comment = "Auto-cleared");
    
    /**
     * @brief 알람 인지 처리
     * @param occurrence_id 발생 ID
     * @param user_id 인지한 사용자 ID
     * @param comment 인지 코멘트
     * @return 성공 여부
     */
    bool acknowledgeAlarm(int occurrence_id, int user_id, const std::string& comment = "");

    // =======================================================================
    // 상태 조회 메서드
    // =======================================================================
    
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
    // 설정 관리
    // =======================================================================
    
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
    // 생성자/소멸자 (싱글톤)
    // =======================================================================
    
    AlarmEngine();
    ~AlarmEngine();
    
    // 복사 생성자와 대입 연산자 삭제
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;

    // =======================================================================
    // 핵심 알람 평가 메서드들
    // =======================================================================
    
    /**
     * @brief 아날로그 알람 평가
     */
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRuleEntity& rule, const DataValue& value);
    
    /**
     * @brief 디지털 알람 평가
     */
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRuleEntity& rule, const DataValue& value);
    
    /**
     * @brief 스크립트 알람 평가
     */
    AlarmEvaluation evaluateScriptAlarm(const AlarmRuleEntity& rule, const DataValue& value);

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 상태 업데이트
     */
    void updateAlarmState(int rule_id, const AlarmEvaluation& evaluation);
    
    /**
     * @brief 알람 이벤트 발송 (Redis + RabbitMQ)
     */
    void sendAlarmEvent(const AlarmEvent& event);
    
    /**
     * @brief 메시지 템플릿 렌더링
     */
    std::string renderMessage(const std::string& template_str, const std::map<std::string, std::string>& variables);
    
    /**
     * @brief 히스테리시스 처리
     */
    bool checkHysteresis(const AlarmRuleEntity& rule, const AlarmState& state, bool should_trigger);

    // =======================================================================
    // JavaScript 엔진 관련
    // =======================================================================
    
    /**
     * @brief JavaScript 컨텍스트 초기화
     */
    bool initializeJSContext();
    
    /**
     * @brief JavaScript 코드 실행
     */
    bool executeScript(const std::string& script, double value, bool& result);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_{false};
    
    // 의존성 객체들
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<RabbitMQClient> mq_client_;
    
    // Repository들
    std::shared_ptr<AlarmRuleRepository> alarm_rule_repo_;
    std::shared_ptr<AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // 알람 규칙 캐시 (성능 최적화)
    mutable std::shared_mutex rules_cache_mutex_;
    std::map<std::string, std::vector<AlarmRuleEntity>> rules_by_target_;  // "data_point:123" -> rules
    std::chrono::system_clock::time_point rules_cache_time_;
    
    // 알람 상태 추적
    mutable std::shared_mutex states_mutex_;
    std::map<int, AlarmState> alarm_states_;  // rule_id -> state
    
    // ID 생성기
    std::atomic<int64_t> next_occurrence_id_{1};
    
    // JavaScript 엔진 (스레드별)
    thread_local static void* js_runtime_;
    thread_local static void* js_context_;
    
    // ✅ LogManager 참조 (타입 수정)
    Utils::LogManager& logger_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_ENGINE_H