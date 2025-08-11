// =============================================================================
// collector/include/Alarm/AlarmEngine.h
// PulseOne 알람 엔진 헤더 - 구현부(.cpp)와 완전 일치
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

// 🔥 JSON include (전방 선언 대신 직접 포함)
#include <nlohmann/json.hpp>

namespace PulseOne {

namespace Alarm {

// =============================================================================
// 타입 별칭들 (기존 Structs.h 사용)
// =============================================================================
using DeviceDataMessage = Structs::DeviceDataMessage;
using DataValue = Structs::DataValue;
using AlarmEvent = Structs::AlarmEvent;
using AlarmRuleEntity = Database::Entities::AlarmRuleEntity;
using AlarmOccurrenceEntity = Database::Entities::AlarmOccurrenceEntity;
using AlarmRuleRepository = Database::Repositories::AlarmRuleRepository;
using AlarmOccurrenceRepository = Database::Repositories::AlarmOccurrenceRepository;



// =============================================================================
// 메인 알람 엔진 클래스 - 🔥 구현부와 정확히 일치
// =============================================================================
class AlarmEngine {
public:
    // =======================================================================
    // 싱글톤 패턴 - 🔥 구현부와 일치
    // =======================================================================
    static AlarmEngine& getInstance();
    
    // =======================================================================
    // 🔥 구현부에서 제거된 initialize() 메서드 - 헤더에서도 제거
    // 생성자에서 모든 초기화를 수행하므로 별도 initialize() 불필요
    // =======================================================================
    
    /**
     * @brief 알람 엔진 종료
     */
    void shutdown();
    
    /**
     * @brief 초기화 상태 확인
     */
    bool isInitialized() const { return initialized_.load(); }

    // =======================================================================
    // 🔥 구현부와 정확히 일치하는 메인 인터페이스
    // =======================================================================
    
    /**
     * @brief 디바이스 메시지에 대해 알람 평가 실행
     * @param message 디바이스 데이터 메시지
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForMessage(const DeviceDataMessage& message);
    
    /**
     * @brief 개별 포인트에 대해 알람 평가
     * @param tenant_id 테넌트 ID
     * @param point_id 포인트 ID (예: "dp_123", "vp_456")
     * @param value 현재 값
     * @return 발생한 알람 이벤트들
     */
    std::vector<AlarmEvent> evaluateForPoint(int tenant_id, 
                                           int point_id, 
                                           const DataValue& value);

    /**
     * @brief 단일 규칙에 대해 알람 평가
     * @param rule 알람 규칙
     * @param value 현재 값
     * @return 평가 결과
     */
    AlarmEvaluation evaluateRule(const AlarmRuleEntity& rule, const DataValue& value);

    // =======================================================================
    // 🔥 구현부와 일치하는 알람 발생 관리
    // =======================================================================
    
    /**
     * @brief 알람 발생 생성
     * @param rule 알람 규칙
     * @param eval 평가 결과
     * @param trigger_value 트리거 값
     * @return 생성된 AlarmOccurrence ID (실패 시 nullopt)
     */
    std::optional<int64_t> raiseAlarm(const AlarmRuleEntity& rule, 
                                     const AlarmEvaluation& eval,
                                     const DataValue& trigger_value);
    
    /**
     * @brief 알람 해제
     * @param occurrence_id 발생 ID
     * @param clear_value 해제 시 값
     * @return 성공 여부
     */
    bool clearAlarm(int64_t occurrence_id, const DataValue& clear_value);

    // =======================================================================
    // 🔥 구현부와 일치하는 상태 조회
    // =======================================================================
    
    /**
     * @brief 엔진 통계 조회
     */
    nlohmann::json getStatistics() const;
    
    /**
     * @brief 활성 알람 조회
     */
    std::vector<AlarmOccurrenceEntity> getActiveAlarms(int tenant_id) const;
    
    /**
     * @brief 알람 발생 상세 조회
     */
    std::optional<AlarmOccurrenceEntity> getAlarmOccurrence(int64_t occurrence_id) const;

    // =======================================================================
    // 🔥 구현부와 일치하는 규칙 관리
    // =======================================================================
    
    /**
     * @brief 포인트에 해당하는 알람 규칙들 조회
     */
    std::vector<AlarmRuleEntity> getAlarmRulesForPoint(int tenant_id, 
                                                      const std::string& point_type, 
                                                      int point_id) const;
    
    /**
     * @brief 특정 규칙의 활성 상태 확인
     */
    bool isAlarmActive(int rule_id) const;
    // =======================================================================
    // 🔥 구현부와 정확히 일치하는 핵심 알람 평가 메서드들
    // =======================================================================
    
    /**
     * @brief 아날로그 알람 평가
     */
    AlarmEvaluation evaluateAnalogAlarm(const AlarmRuleEntity& rule, double value);
    
    /**
     * @brief 디지털 알람 평가
     */
    AlarmEvaluation evaluateDigitalAlarm(const AlarmRuleEntity& rule, bool state);
    
    /**
     * @brief 스크립트 알람 평가
     */
    AlarmEvaluation evaluateScriptAlarm(const AlarmRuleEntity& rule, const nlohmann::json& context);

private:
    // =======================================================================
    // 🔥 구현부와 정확히 일치하는 생성자/소멸자
    // =======================================================================
    
    AlarmEngine();
    ~AlarmEngine();
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;

    // =======================================================================
    // 🔥 구현부와 일치하는 내부 초기화 메서드들
    // =======================================================================
    void initializeClients();       // Redis 등 클라이언트 생성
    void initializeRepositories();  // Repository 초기화
    void loadInitialData();         // 초기 데이터 로드


    // =======================================================================
    // 🔥 구현부와 일치하는 헬퍼 메서드들
    // =======================================================================
    bool checkDeadband(const AlarmRuleEntity& rule, double current, double previous, double threshold);
    std::string getAnalogLevel(const AlarmRuleEntity& rule, double value);
    std::string generateMessage(const AlarmRuleEntity& rule, const AlarmEvaluation& eval, const DataValue& value);
    
    // 상태 관리
    void updateAlarmState(int rule_id, bool active);
    double getLastValue(int rule_id) const;
    void updateLastValue(int rule_id, double value);
    bool getLastDigitalState(int rule_id) const;
    void updateLastDigitalState(int rule_id, bool state);
    
    // 외부 시스템 연동
    void publishToRedis(const AlarmEvent& event);

    // =======================================================================
    // JavaScript 엔진 관련
    // =======================================================================
    bool initScriptEngine();
    void cleanupScriptEngine();

    // =======================================================================
    // 🔥 구현부와 정확히 일치하는 멤버 변수들
    // =======================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_{false};
    
    // 🔥 구현부와 일치하는 포인터 멤버들
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
    std::unordered_map<int, bool> alarm_states_;           // rule_id -> is_active
    std::unordered_map<int, double> last_values_;          // rule_id -> last_value
    std::unordered_map<int, bool> last_digital_states_;   // rule_id -> last_state
    std::unordered_map<int, std::chrono::system_clock::time_point> last_check_times_;  // rule_id -> last_check
    
    // 발생 매핑
    mutable std::shared_mutex occurrence_map_mutex_;
    std::unordered_map<int, int64_t> rule_occurrence_map_; // rule_id -> occurrence_id
    
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