// collector/include/Engine/MaintenanceManager.h
// 점검 모드 및 알람 억제 관리 시스템
#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>

#include "Drivers/CommonTypes.h"
#include "Database/DatabaseManager.h"
#include "RedisClient.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Engine {

/**
 * @brief 점검 타입 열거형
 */
enum class MaintenanceType {
    DEVICE_MAINTENANCE = 0,     ///< 디바이스 전체 점검
    POINT_MAINTENANCE,          ///< 개별 포인트 점검
    COMMUNICATION_CHECK,        ///< 통신 라인 점검
    CALIBRATION,                ///< 센서 교정
    SAFETY_TEST,                ///< 안전 시스템 테스트
    ELECTRICAL_WORK,            ///< 전기 작업
    MECHANICAL_WORK,            ///< 기계 작업
    SOFTWARE_UPDATE,            ///< 소프트웨어 업데이트
    EMERGENCY_REPAIR,           ///< 비상 수리
    CUSTOM                      ///< 사용자 정의
};

/**
 * @brief 점검 상태 열거형
 */
enum class MaintenanceStatus {
    PLANNED = 0,                ///< 계획됨
    APPROVED,                   ///< 승인됨
    ACTIVE,                     ///< 진행 중
    PAUSED,                     ///< 일시정지
    COMPLETED,                  ///< 완료
    CANCELLED,                  ///< 취소됨
    EXPIRED                     ///< 시간 초과
};

/**
 * @brief 점검 우선순위
 */
enum class MaintenancePriority {
    LOW = 0,                    ///< 낮음
    MEDIUM,                     ///< 보통
    HIGH,                       ///< 높음
    EMERGENCY                   ///< 비상
};

/**
 * @brief 알람 억제 액션
 */
enum class AlarmSuppressionAction {
    KEEP = 0,                   ///< 유지 (억제하지 않음)
    SUPPRESS,                   ///< 억제
    DOWNGRADE,                  ///< 심각도 낮춤
    MARK_MAINTENANCE            ///< 점검 표시만 추가
};

/**
 * @brief 체크리스트 항목
 */
struct ChecklistItem {
    std::string item_id;                ///< 항목 ID
    std::string description;            ///< 설명
    bool is_required;                   ///< 필수 여부
    bool is_safety_critical;           ///< 안전 관련 여부
    
    // 완료 정보
    bool completed;                     ///< 완료 여부
    std::string completed_by;           ///< 완료자
    Timestamp completed_at;             ///< 완료 시간
    std::string notes;                  ///< 메모
    
    // 검증 정보
    bool requires_verification;         ///< 검증 필요 여부
    std::string verified_by;            ///< 검증자
    Timestamp verified_at;              ///< 검증 시간
    
    ChecklistItem() 
        : is_required(false)
        , is_safety_critical(false)
        , completed(false)
        , completed_at(std::chrono::system_clock::now())
        , requires_verification(false)
        , verified_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 알람 억제 규칙
 */
struct AlarmSuppressionRule {
    std::string rule_id;                ///< 규칙 ID
    std::string alarm_type;             ///< 알람 타입 (패턴)
    std::string point_pattern;          ///< 포인트 패턴 (정규식)
    AlarmSeverity min_severity;         ///< 최소 심각도
    AlarmSeverity max_severity;         ///< 최대 심각도
    
    AlarmSuppressionAction action;      ///< 억제 액션
    AlarmSeverity downgrade_to;         ///< 낮출 심각도 (DOWNGRADE 시)
    
    bool is_enabled;                    ///< 활성 여부
    std::string description;            ///< 설명
    
    AlarmSuppressionRule() 
        : min_severity(AlarmSeverity::INFO)
        , max_severity(AlarmSeverity::EMERGENCY)
        , action(AlarmSuppressionAction::SUPPRESS)
        , downgrade_to(AlarmSeverity::INFO)
        , is_enabled(true) {}
};

/**
 * @brief 알람 억제 설정
 */
struct AlarmSuppressionConfig {
    bool suppress_all;                  ///< 모든 알람 억제
    bool suppress_communication;        ///< 통신 관련 알람 억제
    bool suppress_operational;          ///< 운전 관련 알람 억제
    bool keep_safety_alarms;            ///< 안전 알람 유지
    bool keep_emergency_alarms;         ///< 비상 알람 유지
    
    std::vector<AlarmSuppressionRule> custom_rules; ///< 사용자 정의 규칙들
    
    AlarmSuppressionConfig() 
        : suppress_all(false)
        , suppress_communication(true)
        , suppress_operational(true)
        , keep_safety_alarms(true)
        , keep_emergency_alarms(true) {}
};

/**
 * @brief 점검 범위 정의
 */
struct MaintenanceScope {
    std::vector<UUID> device_ids;       ///< 대상 디바이스들
    std::vector<std::string> point_ids; ///< 대상 포인트들
    std::vector<std::string> alarm_rule_ids; ///< 대상 알람 규칙들
    
    // 패턴 기반 범위 (정규식)
    std::vector<std::string> device_patterns;   ///< 디바이스 패턴들
    std::vector<std::string> point_patterns;    ///< 포인트 패턴들
    
    bool include_dependencies;          ///< 종속 항목 포함 여부
    
    MaintenanceScope() : include_dependencies(false) {}
    
    /**
     * @brief 디바이스가 범위에 포함되는지 확인
     */
    bool ContainsDevice(const UUID& device_id) const;
    
    /**
     * @brief 포인트가 범위에 포함되는지 확인
     */
    bool ContainsPoint(const std::string& point_id) const;
};

/**
 * @brief 점검 세션 정보
 */
struct MaintenanceSession {
    std::string maintenance_id;         ///< 점검 세션 ID
    MaintenanceType type;               ///< 점검 타입
    std::string title;                  ///< 점검 제목
    std::string description;            ///< 상세 설명
    
    // 범위
    MaintenanceScope scope;             ///< 점검 범위
    
    // 시간 정보
    Timestamp planned_start;            ///< 계획 시작 시간
    Timestamp planned_end;              ///< 계획 종료 시간
    Timestamp actual_start;             ///< 실제 시작 시간
    std::optional<Timestamp> actual_end; ///< 실제 종료 시간
    
    bool auto_end_enabled;              ///< 자동 종료 활성화
    int max_duration_hours;             ///< 최대 지속 시간
    
    // 인원 정보
    std::string requested_by;           ///< 요청자
    std::string approved_by;            ///< 승인자
    std::string technician;             ///< 담당 기술자
    std::string safety_officer;         ///< 안전 관리자
    std::string supervisor;             ///< 감독자
    
    // 상태 및 우선순위
    MaintenanceStatus status;           ///< 현재 상태
    MaintenancePriority priority;       ///< 우선순위
    
    // 알람 억제 설정
    AlarmSuppressionConfig suppression_config; ///< 알람 억제 설정
    
    // 체크리스트
    std::vector<ChecklistItem> checklist; ///< 체크리스트 항목들
    
    // 통계
    int suppressed_alarm_count;         ///< 억제된 알람 수
    int kept_alarm_count;               ///< 유지된 알람 수
    
    // 메타데이터
    std::string work_order_number;      ///< 작업 지시서 번호
    std::string permit_number;          ///< 작업 허가서 번호
    std::map<std::string, std::string> custom_fields; ///< 사용자 정의 필드들
    
    MaintenanceSession() 
        : type(MaintenanceType::DEVICE_MAINTENANCE)
        , planned_start(std::chrono::system_clock::now())
        , planned_end(std::chrono::system_clock::now())
        , actual_start(std::chrono::system_clock::now())
        , auto_end_enabled(true)
        , max_duration_hours(8)
        , status(MaintenanceStatus::PLANNED)
        , priority(MaintenancePriority::MEDIUM)
        , suppressed_alarm_count(0)
        , kept_alarm_count(0) {}
};

/**
 * @brief 점검 컨텍스트 (알람 처리용)
 */
struct MaintenanceContext {
    std::string maintenance_id;         ///< 점검 세션 ID
    MaintenanceType type;               ///< 점검 타입
    std::string title;                  ///< 점검 제목
    std::string technician;             ///< 담당 기술자
    Timestamp actual_start;             ///< 시작 시간
    AlarmSuppressionConfig suppression_config; ///< 억제 설정
    
    /**
     * @brief 특정 알람을 억제해야 하는지 확인
     */
    bool ShouldSuppressAlarm(const DataPoint& point, const DataValue& value) const;
    
    /**
     * @brief 알람 억제 액션 결정
     */
    AlarmSuppressionAction GetSuppressionAction(const AlarmEvent& alarm) const;
};

/**
 * @brief 억제된 알람 기록
 */
struct SuppressedAlarmRecord {
    std::string record_id;              ///< 기록 ID
    std::string maintenance_id;         ///< 점검 세션 ID
    std::string original_alarm_id;      ///< 원래 알람 ID
    
    // 원래 알람 정보
    UUID device_id;                     ///< 디바이스 ID
    std::string point_id;               ///< 포인트 ID
    std::string original_message;       ///< 원래 메시지
    AlarmSeverity original_severity;    ///< 원래 심각도
    double trigger_value;               ///< 트리거 값
    
    // 억제 정보
    AlarmSuppressionAction action_taken; ///< 취한 액션
    std::string suppression_reason;     ///< 억제 사유
    Timestamp suppressed_at;            ///< 억제 시간
    
    // 메타데이터
    std::string notes;                  ///< 메모
    
    SuppressedAlarmRecord() 
        : original_severity(AlarmSeverity::WARNING)
        , trigger_value(0.0)
        , action_taken(AlarmSuppressionAction::SUPPRESS)
        , suppressed_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 점검 모드 및 알람 억제 관리 시스템
 * @details 현장 점검 작업 중 발생하는 불필요한 알람을 억제하고
 *          점검 상태를 추적하는 시스템
 * 
 * 주요 기능:
 * - 점검 세션 생성/관리/종료
 * - 알람 억제 규칙 적용
 * - 점검 체크리스트 관리
 * - 점검 이력 추적
 * - 안전 관련 알람 보호
 * - 자동 종료 및 알림
 */
class MaintenanceManager {
private:
    // 활성 점검 세션들
    std::unordered_map<std::string, MaintenanceSession> active_sessions_;
    mutable std::shared_mutex sessions_mutex_;
    
    // 억제된 알람 기록들
    std::deque<SuppressedAlarmRecord> suppressed_alarms_;
    mutable std::mutex suppressed_alarms_mutex_;
    
    // 스레드 관리
    std::atomic<bool> running_{false};
    std::thread monitoring_thread_;
    std::thread cleanup_thread_;
    
    // 설정
    int max_concurrent_sessions_;       ///< 최대 동시 점검 세션 수
    int max_session_duration_hours_;    ///< 최대 세션 지속 시간
    int suppressed_alarm_history_days_; ///< 억제 알람 기록 보관 일수
    int monitoring_interval_seconds_;   ///< 모니터링 간격
    
    // 외부 의존성
    std::shared_ptr<DatabaseManager> db_manager_;
    std::shared_ptr<RedisClient> redis_client_;
    LogManager& logger_;
    
    // 통계
    std::atomic<uint64_t> total_sessions_started_{0};
    std::atomic<uint64_t> total_sessions_completed_{0};
    std::atomic<uint64_t> total_alarms_suppressed_{0};

public:
    /**
     * @brief 점검 관리자 생성자
     * @param db_manager 데이터베이스 매니저
     * @param redis_client Redis 클라이언트
     */
    MaintenanceManager(std::shared_ptr<DatabaseManager> db_manager,
                      std::shared_ptr<RedisClient> redis_client);
    
    ~MaintenanceManager();
    
    // 복사/이동 생성자 비활성화
    MaintenanceManager(const MaintenanceManager&) = delete;
    MaintenanceManager& operator=(const MaintenanceManager&) = delete;
    
    // =================================================================
    // 생명주기 관리
    // =================================================================
    
    /**
     * @brief 매니저 초기화
     */
    bool Initialize();
    
    /**
     * @brief 매니저 시작
     */
    bool Start();
    
    /**
     * @brief 매니저 중지
     */
    void Stop();
    
    /**
     * @brief 실행 중 여부 확인
     */
    bool IsRunning() const { return running_.load(); }
    
    // =================================================================
    // 점검 세션 관리 (메인 기능)
    // =================================================================
    
    /**
     * @brief 점검 세션 시작
     * @param session 점검 세션 정보
     * @return 점검 세션 ID
     */
    std::string StartMaintenanceSession(const MaintenanceSession& session);
    
    /**
     * @brief 점검 세션 종료
     * @param maintenance_id 점검 세션 ID
     * @param user_id 종료 요청 사용자 ID
     * @param completion_notes 완료 메모
     * @return 성공 시 true
     */
    bool EndMaintenanceSession(const std::string& maintenance_id, 
                              const std::string& user_id,
                              const std::string& completion_notes = "");
    
    /**
     * @brief 점검 세션 일시정지
     * @param maintenance_id 점검 세션 ID
     * @param user_id 요청 사용자 ID
     * @param reason 일시정지 사유
     * @return 성공 시 true
     */
    bool PauseMaintenanceSession(const std::string& maintenance_id,
                                const std::string& user_id,
                                const std::string& reason = "");
    
    /**
     * @brief 점검 세션 재개
     * @param maintenance_id 점검 세션 ID
     * @param user_id 요청 사용자 ID
     * @return 성공 시 true
     */
    bool ResumeMaintenanceSession(const std::string& maintenance_id,
                                 const std::string& user_id);
    
    /**
     * @brief 점검 세션 취소
     * @param maintenance_id 점검 세션 ID
     * @param user_id 요청 사용자 ID
     * @param reason 취소 사유
     * @return 성공 시 true
     */
    bool CancelMaintenanceSession(const std::string& maintenance_id,
                                 const std::string& user_id,
                                 const std::string& reason);
    
    // =================================================================
    // 점검 상태 확인 (알람 처리에서 호출)
    // =================================================================
    
    /**
     * @brief 특정 디바이스가 점검 중인지 확인
     * @param device_id 디바이스 ID
     * @return 점검 중이면 true
     */
    bool IsDeviceInMaintenance(const UUID& device_id) const;
    
    /**
     * @brief 특정 포인트가 점검 중인지 확인
     * @param point_id 포인트 ID
     * @return 점검 중이면 true
     */
    bool IsPointInMaintenance(const std::string& point_id) const;
    
    /**
     * @brief 알람을 억제해야 하는지 확인
     * @param alarm_event 알람 이벤트
     * @return 억제해야 하면 true
     */
    bool ShouldSuppressAlarm(const AlarmEvent& alarm_event) const;
    
    /**
     * @brief 점검 컨텍스트 정보 조회
     * @param device_id 디바이스 ID
     * @return 점검 컨텍스트 (점검 중이 아니면 빈 옵션)
     */
    std::optional<MaintenanceContext> GetMaintenanceContext(const UUID& device_id) const;
    
    /**
     * @brief 알람 억제 처리
     * @param alarm_event 알람 이벤트
     * @return 억제 처리 결과 (억제됨, 다운그레이드됨, 유지됨)
     */
    AlarmSuppressionAction ProcessAlarmSuppression(const AlarmEvent& alarm_event);
    
    // =================================================================
    // 체크리스트 관리
    // =================================================================
    
    /**
     * @brief 체크리스트 항목 완료 처리
     * @param maintenance_id 점검 세션 ID
     * @param item_id 체크리스트 항목 ID
     * @param completed_by 완료자 ID
     * @param notes 메모
     * @return 성공 시 true
     */
    bool CompleteChecklistItem(const std::string& maintenance_id,
                              const std::string& item_id,
                              const std::string& completed_by,
                              const std::string& notes = "");
    
    /**
     * @brief 체크리스트 항목 검증
     * @param maintenance_id 점검 세션 ID
     * @param item_id 체크리스트 항목 ID
     * @param verified_by 검증자 ID
     * @return 성공 시 true
     */
    bool VerifyChecklistItem(const std::string& maintenance_id,
                            const std::string& item_id,
                            const std::string& verified_by);
    
    /**
     * @brief 체크리스트 완료 상태 확인
     * @param maintenance_id 점검 세션 ID
     * @return 모든 필수 항목이 완료되면 true
     */
    bool IsChecklistCompleted(const std::string& maintenance_id) const;
    
    // =================================================================
    // 조회 및 모니터링
    // =================================================================
    
    /**
     * @brief 활성 점검 세션 목록 조회
     * @return 활성 세션 목록
     */
    std::vector<MaintenanceSession> GetActiveMaintenanceSessions() const;
    
    /**
     * @brief 특정 점검 세션 정보 조회
     * @param maintenance_id 점검 세션 ID
     * @return 점검 세션 정보 (존재하지 않으면 빈 옵션)
     */
    std::optional<MaintenanceSession> GetMaintenanceSession(const std::string& maintenance_id) const;
    
    /**
     * @brief 점검 이력 조회
     * @param device_id 디바이스 ID (빈 문자열 시 전체)
     * @param limit 최대 개수
     * @return 점검 이력 목록
     */
    std::vector<MaintenanceSession> GetMaintenanceHistory(const UUID& device_id = "", int limit = 100) const;
    
    /**
     * @brief 억제된 알람 목록 조회
     * @param maintenance_id 점검 세션 ID
     * @return 억제된 알람 기록들
     */
    std::vector<SuppressedAlarmRecord> GetSuppressedAlarms(const std::string& maintenance_id) const;
    
    /**
     * @brief 점검 통계 조회
     * @return JSON 형태의 통계 정보
     */
    nlohmann::json GetMaintenanceStatistics() const;
    
    /**
     * @brief 특정 디바이스의 점검 상태 조회
     * @param device_id 디바이스 ID
     * @return JSON 형태의 상태 정보
     */
    nlohmann::json GetDeviceMaintenanceStatus(const UUID& device_id) const;

private:
    // =================================================================
    // 스레드 함수들
    // =================================================================
    
    /**
     * @brief 모니터링 스레드 함수
     */
    void MonitoringThreadFunction();
    
    /**
     * @brief 정리 스레드 함수
     */
    void CleanupThreadFunction();
    
    // =================================================================
    // 내부 로직
    // =================================================================
    
    /**
     * @brief 점검 세션 ID 생성
     * @return 고유 세션 ID
     */
    std::string GenerateMaintenanceId();
    
    /**
     * @brief 점검 세션 유효성 검사
     * @param session 점검 세션
     * @return 유효하면 true
     */
    bool ValidateMaintenanceSession(const MaintenanceSession& session) const;
    
    /**
     * @brief 세션 시간 초과 체크
     */
    void CheckSessionTimeouts();
    
    /**
     * @brief 자동 종료 처리
     */
    void ProcessAutoEndSessions();
    
    /**
     * @brief 억제된 알람 기록
     * @param alarm_event 알람 이벤트
     * @param maintenance_id 점검 세션 ID
     * @param action 취한 액션
     */
    void RecordSuppressedAlarm(const AlarmEvent& alarm_event,
                              const std::string& maintenance_id,
                              AlarmSuppressionAction action);
    
    /**
     * @brief 점검 세션 상태 변경
     * @param maintenance_id 점검 세션 ID
     * @param new_status 새 상태
     * @param user_id 상태 변경 사용자
     */
    void ChangeSessionStatus(const std::string& maintenance_id,
                            MaintenanceStatus new_status,
                            const std::string& user_id);
    
    /**
     * @brief Redis에 점검 상태 발행
     * @param session 점검 세션
     */
    void PublishMaintenanceStatus(const MaintenanceSession& session);
    
    /**
     * @brief 데이터베이스에 점검 이력 저장
     * @param session 점검 세션
     */
    void SaveMaintenanceHistory(const MaintenanceSession& session);
    
    /**
     * @brief 오래된 기록 정리
     */
    void CleanupOldRecords();
    
    /**
     * @brief 점검 세션 알림 전송
     * @param session 점검 세션
     * @param event_type 이벤트 타입
     */
    void SendMaintenanceNotification(const MaintenanceSession& session, 
                                   const std::string& event_type);
    
    /**
     * @brief 억제 규칙 매칭
     * @param alarm_event 알람 이벤트
     * @param suppression_config 억제 설정
     * @return 매칭된 억제 액션
     */
    AlarmSuppressionAction MatchSuppressionRules(const AlarmEvent& alarm_event,
                                                const AlarmSuppressionConfig& suppression_config) const;
};

} // namespace Engine
} // namespace PulseOne