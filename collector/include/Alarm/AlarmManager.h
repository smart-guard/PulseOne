// =============================================================================
// collector/include/Alarm/AlarmManager.h
// PulseOne 알람 매니저 헤더 - 컴파일 에러 완전 해결 버전
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

// 🔥 수정: RedisClientImpl 완전 include (forward declaration 대신)
#include "Client/RedisClientImpl.h"

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
    // 🔥 초기화 상태 확인 (생성자에서 자동 초기화됨)
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
    // 🔥 새로운 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 알람 이벤트를 비즈니스 규칙으로 강화 (새로운 버전)
     */
    void enhanceAlarmEventWithBusinessLogic(AlarmEvent& event, const DeviceDataMessage& msg);
    
    /**
     * @brief 비즈니스 규칙에 따른 심각도 조정
     */
    void adjustSeverityByBusinessRules(AlarmEvent& event);
    
    /**
     * @brief 위치 및 컨텍스트 정보 추가
     */
    void addLocationAndContext(AlarmEvent& event, const DeviceDataMessage& msg);
    
    /**
     * @brief 다국어 메시지 생성
     */
    void generateLocalizedMessage(AlarmEvent& event);
    
    /**
     * @brief 연속 알람 패턴 분석
     */
    void analyzeContinuousAlarmPattern(AlarmEvent& event);
    
    /**
     * @brief 카테고리별 특수 규칙 적용
     */
    void applyCategorySpecificRules(AlarmEvent& event);
    
    // =======================================================================
    // 🔥 외부 시스템 연동 메서드들
    // =======================================================================
    
    /**
     * @brief 외부 알림 시스템 총괄 호출
     */
    void sendNotifications(const AlarmEvent& event);
    
    /**
     * @brief 이메일 알림 발송
     */
    void sendEmailNotification(const AlarmEvent& event);
    
    /**
     * @brief SMS 알림 발송
     */
    void sendSMSNotification(const AlarmEvent& event);
    
    /**
     * @brief Slack 알림 발송
     */
    void sendSlackNotification(const AlarmEvent& event);
    
    /**
     * @brief Discord 알림 발송
     */
    void sendDiscordNotification(const AlarmEvent& event);
    
    /**
     * @brief Redis 비즈니스 채널 발송
     */
    void publishToRedisChannels(const AlarmEvent& event);
    
    // =======================================================================
    // 🔥 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief AlarmRule을 AlarmRuleEntity로 변환 (AlarmEngine 호출용)
     */
    Database::Entities::AlarmRuleEntity convertToEntity(const AlarmRule& rule);
    
    /**
     * @brief 메시지 템플릿 처리
     */
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