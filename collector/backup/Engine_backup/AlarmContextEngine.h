/**
 * @file AlarmContextEngine.h
 * @brief 알람 컨텍스트 추적 및 분석 엔진
 * @details 사용자 제어 명령과 알람 발생 간의 상관관계를 분석하여 컨텍스트 정보 제공
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#ifndef ENGINE_ALARM_CONTEXT_ENGINE_H
#define ENGINE_ALARM_CONTEXT_ENGINE_H

#include <memory>
#include <unordered_map>
#include <deque>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <map>
#include <string>

#include "Drivers/CommonTypes.h"
#include "Database/DatabaseManager.h"
#include "Database/DeviceDataAccess.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Engine {

// =============================================================================
// 전방 선언 및 타입 정의
// =============================================================================

using UUID = Drivers::UUID;
using Timestamp = std::chrono::system_clock::time_point;

/**
 * @brief 알람 트리거 타입
 */
enum class AlarmTriggerType {
    AUTOMATIC = 0,          ///< 자동 발생 (폴링 중 감지)
    USER_ACTION,            ///< 사용자 제어 행동으로 인한 발생
    SYSTEM_EVENT,           ///< 시스템 이벤트로 인한 발생
    EXTERNAL_TRIGGER,       ///< 외부 트리거
    UNKNOWN                 ///< 원인 불명
};

/**
 * @brief 알람 심각도
 */
enum class AlarmSeverity {
    INFO = 0,               ///< 정보
    WARNING,                ///< 경고
    CRITICAL,               ///< 치명적
    EMERGENCY               ///< 비상
};

/**
 * @brief 기본 알람 이벤트 구조체
 */
struct AlarmEvent {
    std::string alarm_id;               ///< 알람 ID
    UUID device_id;                     ///< 디바이스 ID
    std::string device_name;            ///< 디바이스 이름
    std::string point_id;               ///< 포인트 ID
    std::string point_name;             ///< 포인트 이름
    std::string message;                ///< 알람 메시지
    int severity;                       ///< 심각도 (정수형)
    double trigger_value;               ///< 트리거 값
    double threshold_value;             ///< 임계값
    std::string unit;                   ///< 단위
    Timestamp triggered_at;             ///< 발생 시간
    bool is_active;                     ///< 활성 상태
    std::string alarm_rule_id;          ///< 알람 규칙 ID
    std::string alarm_type;             ///< 알람 타입
    
    AlarmEvent() 
        : severity(1)
        , trigger_value(0.0)
        , threshold_value(0.0)
        , triggered_at(std::chrono::system_clock::now())
        , is_active(true) {}
};

/**
 * @brief 사용자 명령 컨텍스트
 */
struct UserCommandContext {
    std::string command_id;             ///< 명령 ID
    std::string user_id;                ///< 사용자 ID
    std::string user_name;              ///< 사용자 이름
    UUID device_id;                     ///< 대상 디바이스 ID
    std::string control_point_id;       ///< 제어 포인트 ID
    std::string command_type;           ///< 명령 타입 (start, stop, set_value 등)
    double target_value;                ///< 목표 값
    double previous_value;              ///< 이전 값
    std::string reason;                 ///< 제어 사유
    
    Timestamp executed_at;              ///< 실행 시간
    Timestamp completed_at;             ///< 완료 시간
    bool was_successful;                ///< 성공 여부
    
    // 영향받는 포인트들 (예측)
    std::vector<std::string> affected_point_ids; ///< 영향받을 가능성이 있는 포인트들
    std::vector<std::string> dependent_devices;  ///< 종속 디바이스들
    
    UserCommandContext() 
        : target_value(0.0)
        , previous_value(0.0)
        , executed_at(std::chrono::system_clock::now())
        , completed_at(std::chrono::system_clock::now())
        , was_successful(false) {}
};

/**
 * @brief 알람 컨텍스트 정보
 */
struct AlarmContext {
    AlarmTriggerType trigger_type;      ///< 트리거 타입
    bool is_user_triggered;             ///< 사용자 제어로 인한 알람 여부
    double correlation_confidence;      ///< 상관관계 신뢰도 (0.0 ~ 1.0)
    
    // 관련 사용자 명령 정보
    std::optional<UserCommandContext> related_command; ///< 관련 명령
    std::vector<UserCommandContext> potential_causes;  ///< 잠재적 원인들
    
    // 시간 정보
    Timestamp analysis_time;            ///< 분석 시간
    int correlation_window_ms;          ///< 상관관계 분석 윈도우 (밀리초)
    
    // 추가 메타데이터
    std::vector<std::string> analysis_notes; ///< 분석 노트
    std::map<std::string, std::string> metadata; ///< 추가 메타데이터
    
    AlarmContext() 
        : trigger_type(AlarmTriggerType::UNKNOWN)
        , is_user_triggered(false)
        , correlation_confidence(0.0)
        , analysis_time(std::chrono::system_clock::now())
        , correlation_window_ms(30000) {}
};

/**
 * @brief 확장된 알람 이벤트 (컨텍스트 포함)
 */
struct EnhancedAlarmEvent {
    // 기본 알람 정보
    std::string alarm_id;               ///< 알람 ID
    UUID device_id;                     ///< 디바이스 ID
    std::string device_name;            ///< 디바이스 이름
    std::string point_id;               ///< 포인트 ID
    std::string point_name;             ///< 포인트 이름
    std::string message;                ///< 알람 메시지
    AlarmSeverity severity;             ///< 심각도
    
    // 값 정보
    double trigger_value;               ///< 트리거 값
    double threshold_value;             ///< 임계값
    std::string unit;                   ///< 단위
    
    // 시간 정보
    Timestamp triggered_at;             ///< 발생 시간
    Timestamp acknowledged_at;          ///< 확인 시간
    Timestamp cleared_at;               ///< 해제 시간
    
    // 상태
    bool is_acknowledged;               ///< 확인 여부
    bool is_cleared;                    ///< 해제 여부
    bool is_active;                     ///< 활성 상태
    std::string acknowledged_by;        ///< 확인한 사용자
    
    // 🔥 확장: 컨텍스트 정보
    AlarmContext context;               ///< 알람 컨텍스트
    
    // 알람 규칙 정보
    std::string alarm_rule_id;          ///< 알람 규칙 ID
    std::string alarm_type;             ///< 알람 타입 (high, low, deviation 등)
    
    EnhancedAlarmEvent() 
        : severity(AlarmSeverity::WARNING)
        , trigger_value(0.0)
        , threshold_value(0.0)
        , triggered_at(std::chrono::system_clock::now())
        , acknowledged_at(std::chrono::system_clock::now())
        , cleared_at(std::chrono::system_clock::now())
        , is_acknowledged(false)
        , is_cleared(false)
        , is_active(true) {}
};

/**
 * @brief 의존성 규칙 구조체
 */
struct DependencyRule {
    std::string rule_id;                ///< 규칙 ID
    std::string source_point_id;        ///< 원인 포인트 ID
    std::string target_point_id;        ///< 영향받는 포인트 ID
    std::string dependency_type;        ///< 의존성 타입 (direct, indirect, delayed)
    
    int delay_ms;                       ///< 지연 시간 (밀리초)
    double influence_factor;            ///< 영향도 (0.0 ~ 1.0)
    std::string description;            ///< 설명
    
    bool is_enabled;                    ///< 활성 상태
    
    DependencyRule() 
        : delay_ms(0)
        , influence_factor(1.0)
        , is_enabled(true) {}
};

// =============================================================================
// AlarmContextEngine 클래스 정의
// =============================================================================

/**
 * @brief 알람 컨텍스트 추적 및 분석 엔진
 * @details 사용자 제어 명령과 알람 발생 간의 상관관계를 분석하여
 *          알람의 원인을 추적하고 컨텍스트 정보를 제공
 * 
 * 주요 기능:
 * - 사용자 명령 추적 (sliding window)
 * - 알람-명령 상관관계 분석
 * - 의존성 규칙 기반 영향도 분석
 * - 시간 기반 상관관계 계산
 * - 컨텍스트 정보 생성 및 저장
 */
class AlarmContextEngine {
public:
    /**
     * @brief 알람 컨텍스트 엔진 생성자
     * @param db_manager 데이터베이스 매니저
     * @param redis_client Redis 클라이언트
     */
    AlarmContextEngine(std::shared_ptr<DatabaseManager> db_manager,
                      std::shared_ptr<RedisClient> redis_client);
    
    /**
     * @brief 소멸자
     */
    ~AlarmContextEngine();
    
    // 복사/이동 생성자 비활성화
    AlarmContextEngine(const AlarmContextEngine&) = delete;
    AlarmContextEngine& operator=(const AlarmContextEngine&) = delete;
    
    // =================================================================
    // 생명주기 관리
    // =================================================================
    
    /**
     * @brief 엔진 초기화
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief 엔진 시작
     * @return 성공 시 true
     */
    bool Start();
    
    /**
     * @brief 엔진 중지
     */
    void Stop();
    
    /**
     * @brief 실행 중 여부 확인
     * @return 실행 중이면 true
     */
    bool IsRunning() const { return running_.load(); }
    
    // =================================================================
    // 사용자 명령 추적
    // =================================================================
    
    /**
     * @brief 사용자 명령 등록
     * @param command_context 명령 컨텍스트
     */
    void RegisterUserCommand(const UserCommandContext& command_context);
    
    /**
     * @brief 명령 완료 업데이트
     * @param command_id 명령 ID
     * @param was_successful 성공 여부
     * @param completion_time 완료 시간
     */
    void UpdateCommandCompletion(const std::string& command_id, 
                                bool was_successful,
                                const Timestamp& completion_time = std::chrono::system_clock::now());
    
    /**
     * @brief 최근 명령 조회
     * @param device_id 디바이스 ID (빈 문자열 시 전체)
     * @param limit 최대 개수
     * @return 최근 명령 목록
     */
    std::vector<UserCommandContext> GetRecentCommands(const UUID& device_id = UUID{}, int limit = 50) const;
    
    // =================================================================
    // 알람 컨텍스트 분석 (메인 기능)
    // =================================================================
    
    /**
     * @brief 알람 컨텍스트 분석
     * @param alarm_event 기본 알람 이벤트
     * @return 컨텍스트가 포함된 확장 알람 이벤트
     */
    EnhancedAlarmEvent AnalyzeAlarmContext(const AlarmEvent& alarm_event);
    
    /**
     * @brief 사용자 트리거 알람 여부 확인
     * @param alarm_event 알람 이벤트
     * @return 사용자 제어로 인한 알람이면 true
     */
    bool IsUserTriggeredAlarm(const AlarmEvent& alarm_event);
    
    /**
     * @brief 알람 상관관계 분석
     * @param alarm_event 알람 이벤트
     * @param correlation_window_ms 분석 윈도우 (밀리초)
     * @return 알람 컨텍스트
     */
    AlarmContext AnalyzeCorrelation(const AlarmEvent& alarm_event, 
                                   int correlation_window_ms = 0);
    
    /**
     * @brief 배치 알람 분석 (여러 알람 동시 분석)
     * @param alarm_events 알람 이벤트들
     * @return 확장된 알람 이벤트들
     */
    std::vector<EnhancedAlarmEvent> AnalyzeBatchAlarms(const std::vector<AlarmEvent>& alarm_events);
    
    // =================================================================
    // 의존성 규칙 관리
    // =================================================================
    
    /**
     * @brief 데이터베이스에서 의존성 규칙 로드
     * @return 성공 시 true
     */
    bool LoadDependencyRulesFromDB();
    
    /**
     * @brief 의존성 규칙 추가/업데이트
     * @param rule 의존성 규칙
     * @return 성공 시 true
     */
    bool AddDependencyRule(const DependencyRule& rule);
    
    /**
     * @brief 의존성 규칙 제거
     * @param rule_id 규칙 ID
     * @return 성공 시 true
     */
    bool RemoveDependencyRule(const std::string& rule_id);
    
    /**
     * @brief 모든 의존성 규칙 조회
     * @return 의존성 규칙 목록
     */
    std::vector<DependencyRule> GetAllDependencyRules() const;
    
    // =================================================================
    // 통계 및 상태 조회
    // =================================================================
    
    /**
     * @brief 엔진 통계 정보 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetStatisticsJson() const;
    
    /**
     * @brief 설정 업데이트
     * @param correlation_window_ms 상관관계 윈도우
     * @param min_confidence 최소 신뢰도
     * @param max_history_size 최대 히스토리 크기
     */
    void UpdateSettings(int correlation_window_ms = 0, 
                       double min_confidence = 0.0, 
                       int max_history_size = 0);

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    // 최근 명령 추적 (sliding window)
    std::deque<UserCommandContext> recent_commands_;
    mutable std::shared_mutex commands_mutex_;
    
    // 의존성 규칙 관리
    std::vector<DependencyRule> dependency_rules_;
    std::unordered_map<std::string, std::vector<std::string>> point_dependencies_; // point_id -> dependent_points
    mutable std::shared_mutex dependencies_mutex_;
    
    // 알람 이력 (분석용)
    std::deque<EnhancedAlarmEvent> recent_alarms_;
    mutable std::shared_mutex alarms_mutex_;
    
    // 스레드 관리
    std::atomic<bool> running_{false};
    std::thread cleanup_thread_;
    std::thread analysis_thread_;
    
    // 설정
    int max_command_history_size_;      ///< 최대 명령 이력 크기
    int max_alarm_history_size_;        ///< 최대 알람 이력 크기
    int default_correlation_window_ms_; ///< 기본 상관관계 윈도우
    int cleanup_interval_seconds_;      ///< 정리 작업 간격
    double min_correlation_confidence_; ///< 최소 상관관계 신뢰도
    
    // 외부 의존성
    std::shared_ptr<DatabaseManager> db_manager_;
    std::shared_ptr<RedisClient> redis_client_;
    LogManager& logger_;
    
    // 통계
    std::atomic<uint64_t> total_alarms_analyzed_{0};
    std::atomic<uint64_t> user_triggered_alarms_{0};
    std::atomic<uint64_t> automatic_alarms_{0};
    
    // =============================================================================
    // 내부 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief 기본 알람 정보를 확장 알람으로 복사
     * @param source 원본 알람 이벤트
     * @param target 대상 확장 알람 이벤트
     */
    void CopyBaseAlarmInfo(const AlarmEvent& source, EnhancedAlarmEvent& target);
    
    /**
     * @brief 관련 명령들 찾기
     * @param alarm_event 알람 이벤트
     * @param cutoff_time 컷오프 시간
     * @return 관련 명령들
     */
    std::vector<UserCommandContext> FindRelatedCommands(const AlarmEvent& alarm_event, 
                                                       const Timestamp& cutoff_time);
    
    /**
     * @brief 가장 관련성이 높은 명령 선택
     * @param related_commands 관련 명령들
     * @param alarm_event 알람 이벤트
     * @return 최적의 관련 명령
     */
    std::optional<UserCommandContext> SelectBestRelatedCommand(
        const std::vector<UserCommandContext>& related_commands,
        const AlarmEvent& alarm_event);
    
    /**
     * @brief 상관관계 신뢰도 계산
     * @param context 알람 컨텍스트
     * @param alarm_event 알람 이벤트
     * @return 신뢰도 (0.0 ~ 1.0)
     */
    double CalculateCorrelationConfidence(const AlarmContext& context, 
                                         const AlarmEvent& alarm_event);
    
    /**
     * @brief 분석 노트 생성
     * @param context 알람 컨텍스트 (참조로 수정)
     * @param alarm_event 알람 이벤트
     */
    void GenerateAnalysisNotes(AlarmContext& context, const AlarmEvent& alarm_event);
    
    /**
     * @brief 알람 히스토리에 추가
     * @param alarm 확장 알람 이벤트
     */
    void AddToAlarmHistory(const EnhancedAlarmEvent& alarm);
    
    /**
     * @brief 명령 이벤트를 Redis에 발행
     * @param command 사용자 명령 컨텍스트
     */
    void PublishCommandEvent(const UserCommandContext& command);
    
    /**
     * @brief 확장된 알람을 Redis에 발행
     * @param alarm 확장 알람 이벤트
     */
    void PublishEnhancedAlarm(const EnhancedAlarmEvent& alarm);
    
    // =============================================================================
    // 의존성 관련 함수들
    // =============================================================================
    
    /**
     * @brief 기본 의존성 규칙 로드
     */
    void LoadDefaultDependencyRules();
    
    /**
     * @brief 의존성 맵 재구성
     */
    void RebuildDependencyMap();
    
    /**
     * @brief 포인트 관련성 확인
     * @param control_point 제어 포인트
     * @param alarm_point 알람 포인트
     * @return 관련이 있으면 true
     */
    bool IsPointRelated(const std::string& control_point, const std::string& alarm_point);
    
    /**
     * @brief 직접 의존성 확인
     * @param source_point 원본 포인트
     * @param target_point 대상 포인트
     * @return 직접 의존성이 있으면 true
     */
    bool IsDirectlyDependent(const std::string& source_point, const std::string& target_point);
    
    /**
     * @brief 간접 의존성 확인
     * @param source_point 원본 포인트
     * @param target_point 대상 포인트
     * @return 간접 의존성이 있으면 true
     */
    bool IsIndirectlyDependent(const std::string& source_point, const std::string& target_point);
    
    /**
     * @brief 종속 포인트들 조회
     * @param source_point 원본 포인트
     * @return 종속 포인트 목록
     */
    std::vector<std::string> GetDependentPoints(const std::string& source_point);
    
    /**
     * @brief 의존성 영향 팩터 조회
     * @param source_point 원본 포인트
     * @param target_point 대상 포인트
     * @return 영향 팩터 (0.0 ~ 1.0)
     */
    double GetDependencyInfluenceFactor(const std::string& source_point, const std::string& target_point);
    
    /**
     * @brief 디바이스 간 의존성 확인
     * @param source_device 원본 디바이스
     * @param target_device 대상 디바이스
     * @return 의존성이 있으면 true
     */
    bool IsDeviceDependent(const UUID& source_device, const UUID& target_device);
    
    /**
     * @brief 배치 알람들 간의 상관관계 분석
     * @param enhanced_alarms 확장된 알람들 (참조로 수정)
     */
    void AnalyzeBatchCorrelations(std::vector<EnhancedAlarmEvent>& enhanced_alarms);
    
    // =============================================================================
    // 스레드 함수들
    // =============================================================================
    
    /**
     * @brief 정리 작업 스레드 메인 함수
     */
    void CleanupThreadMain();
    
    /**
     * @brief 분석 스레드 메인 함수
     */
    void AnalysisThreadMain();
    
    /**
     * @brief 정리 작업 수행
     */
    void PerformCleanup();
    
    /**
     * @brief 주기적 분석 수행
     */
    void PerformPeriodicAnalysis();
};

} // namespace Engine
} // namespace PulseOne

#endif // ENGINE_ALARM_CONTEXT_ENGINE_H