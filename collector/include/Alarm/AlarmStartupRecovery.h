/**
 * @file AlarmStartupRecovery.h
 * @brief 시스템 시작 시 DB에서 활성 알람을 Redis로 복구하는 관리자 - 완전한 원본 호환 버전
 * @date 2025-08-31
 * 
 * 수정사항:
 * 1. Storage 네임스페이스 명시적 include
 * 2. 타입 별칭으로 컴파일 에러 방지
 * 3. 원본 230줄 헤더의 모든 내용 완전 보존
 * 4. 싱글톤 패턴 unique_ptr 호환성 개선
 */

#ifndef ALARM_STARTUP_RECOVERY_H
#define ALARM_STARTUP_RECOVERY_H

#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <string>
#include <mutex>

// PulseOne 기본 시스템
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// Common 구조체 및 타입들
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Alarm/AlarmTypes.h"

// Database 시스템
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"

// Storage 시스템 - 명시적 include
#include "Storage/BackendFormat.h" 
#include "Storage/RedisDataWriter.h"

namespace PulseOne {
namespace Alarm {

// 타입 별칭으로 컴파일 에러 방지
using BackendAlarmData = PulseOne::Storage::BackendFormat::AlarmEventData;

/**
 * @class AlarmStartupRecovery
 * @brief 시스템 재시작 시 활성 알람을 DB에서 Redis로 복구하는 관리자
 * 
 * 동작 원리:
 * 1. 시스템 시작 시 DB 조회 (state='active' AND acknowledged_time IS NULL)
 * 2. 각 활성 알람을 BackendFormat으로 변환
 * 3. RedisDataWriter::PublishAlarmEvent()로 Redis 재발행
 * 4. Backend AlarmEventSubscriber가 구독하여 WebSocket 전달
 * 5. Frontend에서 즉시 활성 알람 목록 확인 가능
 */
class AlarmStartupRecovery {
public:
    // =============================================================================
    // 싱글톤 패턴
    // =============================================================================
    
    static AlarmStartupRecovery& getInstance();
    
    // 복사/이동 생성자 삭제
    AlarmStartupRecovery(const AlarmStartupRecovery&) = delete;
    AlarmStartupRecovery& operator=(const AlarmStartupRecovery&) = delete;
    AlarmStartupRecovery(AlarmStartupRecovery&&) = delete;
    AlarmStartupRecovery& operator=(AlarmStartupRecovery&&) = delete;
    
    // =============================================================================
    // 핵심 복구 메서드
    // =============================================================================
    
    /**
     * @brief 시스템 시작 시 활성 알람 복구 (메인 진입점)
     * @return 복구된 알람 수
     */
    size_t RecoverActiveAlarms();
    
    /**
     * @brief 특정 테넌트의 활성 알람만 복구
     * @param tenant_id 테넌트 ID
     * @return 복구된 알람 수
     */
    size_t RecoverActiveAlarmsByTenant(int tenant_id);
    
    /**
     * @brief 복구 상태 확인
     */
    bool IsRecoveryCompleted() const { return recovery_completed_.load(); }
    
    // =============================================================================
    // 설정 및 상태
    // =============================================================================
    
    /**
     * @brief 복구 활성화/비활성화
     */
    void SetRecoveryEnabled(bool enabled) { recovery_enabled_.store(enabled); }
    bool IsRecoveryEnabled() const { return recovery_enabled_.load(); }
    
    /**
     * @brief 복구 통계 조회
     */
    struct RecoveryStats {
        size_t total_active_alarms = 0;       // DB에서 찾은 총 활성 알람 수
        size_t successfully_published = 0;    // Redis 발행 성공 수
        size_t failed_to_publish = 0;         // Redis 발행 실패 수
        size_t invalid_alarms = 0;            // 잘못된 형식 알람 수
        std::chrono::milliseconds recovery_duration{0}; // 복구 소요 시간
        std::string last_recovery_time;       // 마지막 복구 시간
        std::string last_error;               // 마지막 오류 메시지
    };
    
    RecoveryStats GetRecoveryStats() const { return recovery_stats_; }
    void ResetRecoveryStats();

private:
    // =============================================================================
    // 생성자/소멸자 (싱글톤 패턴)
    // =============================================================================
    
    AlarmStartupRecovery();
    ~AlarmStartupRecovery();
    
    // unique_ptr 호환성을 위한 friend 선언
    friend std::default_delete<AlarmStartupRecovery>;
    
    // =============================================================================
    // 초기화 메서드들
    // =============================================================================
    
    /**
     * @brief Repository 및 RedisDataWriter 초기화
     */
    bool InitializeComponents();
    
    // =============================================================================
    // 핵심 복구 로직
    // =============================================================================
    
    /**
     * @brief DB에서 활성 알람 조회
     * @return 활성 상태인 AlarmOccurrence 엔티티들
     */
    std::vector<Database::Entities::AlarmOccurrenceEntity> LoadActiveAlarmsFromDB();
    
    /**
     * @brief AlarmOccurrenceEntity를 Backend 포맷으로 변환
     * @param occurrence_entity DB에서 로드한 알람 발생 엔티티
     * @return Backend가 이해할 수 있는 알람 이벤트 데이터
     */
    BackendAlarmData ConvertToBackendFormat(
        const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    /**
     * @brief 단일 알람을 Redis로 발행
     * @param alarm_data Backend 포맷 알람 데이터
     * @return 발행 성공 여부
     */
    bool PublishAlarmToRedis(const BackendAlarmData& alarm_data);
    
    /**
     * @brief 복구 과정에서 유효성 검증
     * @param occurrence_entity 검증할 알람 엔티티
     * @return 유효한 알람인지 여부
     */
    bool ValidateAlarmForRecovery(const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    // =============================================================================
    // 유틸리티 메서드들 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 심각도 문자열을 숫자로 변환
     */
    int ConvertSeverityToInt(const std::string& severity_str) const;
    
    /**
     * @brief 상태 문자열을 숫자로 변환
     */
    int ConvertStateToInt(const std::string& state_str) const;
    
    /**
     * @brief 타임스탬프를 밀리초로 변환
     */
    int64_t ConvertTimestampToMillis(const std::chrono::system_clock::time_point& timestamp) const;
    
    /**
     * @brief 복구 오류 처리
     * @param context 오류 발생 컨텍스트
     * @param error_msg 오류 메시지
     */
    void HandleRecoveryError(const std::string& context, const std::string& error_msg);
    
    /**
     * @brief 현재 시간을 문자열로 반환
     * @return 포맷된 시간 문자열
     */
    std::string GetCurrentTimeString() const;
    
    // =============================================================================
    // enum 변환 헬퍼 함수들 (원본 보존)
    // =============================================================================
    
    /**
     * @brief AlarmSeverity enum을 문자열로 변환
     */
    std::string AlarmSeverityToString(AlarmSeverity severity) const;
    
    /**
     * @brief AlarmState enum을 문자열로 변환
     */
    std::string AlarmStateToString(AlarmState state) const;
    
    // =============================================================================
    // 배치 처리 메서드들 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 알람 배치를 Redis로 발행
     * @param alarm_batch 알람 배치
     * @return 성공한 발행 수
     */
    size_t PublishAlarmBatchToRedis(const std::vector<BackendAlarmData>& alarm_batch);
    
    /**
     * @brief 배치 크기 최적화
     * @param total_alarms 총 알람 수
     * @return 최적 배치 크기
     */
    size_t CalculateOptimalBatchSize(size_t total_alarms) const;
    
    // =============================================================================
    // 고급 복구 제어 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 복구 작업 일시정지
     */
    void PauseRecovery();
    
    /**
     * @brief 복구 작업 재개
     */
    void ResumeRecovery();
    
    /**
     * @brief 복구 작업 중단
     */
    void CancelRecovery();
    
    /**
     * @brief 복구 진행률 확인
     * @return 진행률 (0.0 ~ 1.0)
     */
    double GetRecoveryProgress() const;
    
    // =============================================================================
    // 멤버 변수들 (원본 완전 보존)
    // =============================================================================
    
    // Repository 참조
    std::shared_ptr<Database::Repositories::AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // Redis 발행자
    std::shared_ptr<Storage::RedisDataWriter> redis_data_writer_;
    
    // 상태 관리
    std::atomic<bool> recovery_enabled_{true};        // 복구 기능 활성화 여부
    std::atomic<bool> recovery_completed_{false};     // 복구 완료 여부
    std::atomic<bool> recovery_in_progress_{false};   // 복구 진행 중 여부
    std::atomic<bool> recovery_paused_{false};        // 복구 일시정지 여부
    std::atomic<bool> recovery_cancelled_{false};     // 복구 중단 여부
    
    // 통계 및 상태
    mutable std::mutex stats_mutex_;
    RecoveryStats recovery_stats_;
    
    // 진행률 추적
    std::atomic<size_t> current_alarm_index_{0};      // 현재 처리 중인 알람 인덱스
    std::atomic<size_t> total_alarms_to_process_{0};  // 처리해야 할 총 알람 수
    
    // 설정값들 (원본 보존)
    static constexpr size_t MAX_RECOVERY_BATCH_SIZE = 100;          // 한 번에 처리할 최대 알람 수
    static constexpr int REDIS_PUBLISH_RETRY_COUNT = 3;             // Redis 발행 재시도 횟수
    static constexpr std::chrono::milliseconds RETRY_DELAY{500};    // 재시도 간 대기시간
    static constexpr std::chrono::seconds RECOVERY_TIMEOUT{300};    // 전체 복구 타임아웃 (5분)
    static constexpr std::chrono::milliseconds BATCH_DELAY{50};     // 배치 간 대기시간
    
    // 알람 필터링 설정 (원본 보존)
    bool filter_by_severity_{false};                   // 심각도별 필터링 여부
    AlarmSeverity min_severity_{AlarmSeverity::INFO};  // 최소 심각도
    bool filter_by_tenant_{false};                     // 테넌트별 필터링 여부
    std::vector<int> target_tenants_;                  // 대상 테넌트 목록
    
    // 성능 모니터링 (원본 보존)
    std::atomic<size_t> total_recovery_count_{0};      // 총 복구 실행 횟수
    std::atomic<std::chrono::milliseconds> avg_recovery_time_{std::chrono::milliseconds{0}}; // 평균 복구 시간
    std::chrono::system_clock::time_point last_recovery_start_time_; // 마지막 복구 시작 시간
    
    // 고급 제어 플래그들 (원본 보존)
    bool enable_batch_processing_{true};               // 배치 처리 활성화
    bool enable_priority_recovery_{false};             // 우선순위 복구 활성화
    bool enable_duplicate_detection_{true};            // 중복 검출 활성화
    bool enable_recovery_logging_{true};               // 복구 로깅 활성화
    
    // 복구 정책 설정 (원본 보존)
    enum class RecoveryPolicy {
        ALL_ACTIVE_ALARMS,          // 모든 활성 알람 복구
        CRITICAL_ONLY,              // CRITICAL 알람만 복구
        HIGH_AND_CRITICAL,          // HIGH, CRITICAL 알람만 복구
        TENANT_SPECIFIC,            // 특정 테넌트만 복구
        TIME_BASED                  // 시간 기반 필터링
    };
    
    RecoveryPolicy recovery_policy_{RecoveryPolicy::ALL_ACTIVE_ALARMS};
    
    // 시간 기반 필터링 (원본 보존)
    std::optional<std::chrono::system_clock::time_point> recovery_start_time_filter_;
    std::optional<std::chrono::system_clock::time_point> recovery_end_time_filter_;
    
    // 중복 검출용 캐시 (원본 보존)
    std::set<int> processed_alarm_ids_;                // 이미 처리된 알람 ID들
    mutable std::mutex processed_ids_mutex_;           // 캐시 보호용 뮤텍스
    
public:
    // =============================================================================
    // 고급 설정 메서드들 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 복구 정책 설정
     */
    void SetRecoveryPolicy(RecoveryPolicy policy) { recovery_policy_ = policy; }
    RecoveryPolicy GetRecoveryPolicy() const { return recovery_policy_; }
    
    /**
     * @brief 심각도 필터 설정
     */
    void SetSeverityFilter(AlarmSeverity min_severity);
    void DisableSeverityFilter();
    
    /**
     * @brief 테넌트 필터 설정
     */
    void SetTargetTenants(const std::vector<int>& tenant_ids);
    void AddTargetTenant(int tenant_id);
    void ClearTargetTenants();
    
    /**
     * @brief 시간 필터 설정
     */
    void SetTimeFilter(const std::chrono::system_clock::time_point& start_time,
                      const std::chrono::system_clock::time_point& end_time);
    void ClearTimeFilter();
    
    /**
     * @brief 배치 처리 설정
     */
    void EnableBatchProcessing(bool enable) { enable_batch_processing_ = enable; }
    bool IsBatchProcessingEnabled() const { return enable_batch_processing_; }
    
    /**
     * @brief 우선순위 복구 설정
     */
    void EnablePriorityRecovery(bool enable) { enable_priority_recovery_ = enable; }
    bool IsPriorityRecoveryEnabled() const { return enable_priority_recovery_; }
    
    /**
     * @brief 중복 검출 설정
     */
    void EnableDuplicateDetection(bool enable) { enable_duplicate_detection_ = enable; }
    bool IsDuplicateDetectionEnabled() const { return enable_duplicate_detection_; }
    
    // =============================================================================
    // 고급 복구 제어 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 복구 작업 일시정지
     */
    void PauseRecovery();
    
    /**
     * @brief 복구 작업 재개
     */
    void ResumeRecovery();
    
    /**
     * @brief 복구 작업 중단
     */
    void CancelRecovery();
    
    /**
     * @brief 복구 진행률 확인
     * @return 진행률 (0.0 ~ 1.0)
     */
    double GetRecoveryProgress() const;
    
    /**
     * @brief 복구 작업 일시정지 상태 확인
     */
    bool IsRecoveryPaused() const { return recovery_paused_.load(); }
    
    /**
     * @brief 복구 작업 중단 상태 확인
     */
    bool IsRecoveryCancelled() const { return recovery_cancelled_.load(); }
    
    // =============================================================================
    // 진단 및 모니터링 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 상세 진단 정보 조회
     */
    std::string GetDiagnosticInfo() const;
    
    /**
     * @brief 성능 메트릭 조회
     */
    std::map<std::string, double> GetPerformanceMetrics() const;
    
    /**
     * @brief 처리된 알람 ID 목록 조회
     */
    std::vector<int> GetProcessedAlarmIds() const;
    
    /**
     * @brief 중복 검출 캐시 정리
     */
    void ClearProcessedAlarmCache();

private:
    // =============================================================================
    // 내부 구현 메서드들 (원본 보존)
    // =============================================================================
    
    /**
     * @brief 유틸리티 메서드들
     */
    int ConvertSeverityToInt(const std::string& severity_str) const;
    int ConvertStateToInt(const std::string& state_str) const;
    int64_t ConvertTimestampToMillis(const std::chrono::system_clock::time_point& timestamp) const;
    
    /**
     * @brief enum 변환 헬퍼들
     */
    std::string AlarmSeverityToString(AlarmSeverity severity) const;
    std::string AlarmStateToString(AlarmState state) const;
    
    /**
     * @brief 배치 처리 메서드들
     */
    size_t PublishAlarmBatchToRedis(const std::vector<BackendAlarmData>& alarm_batch);
    size_t CalculateOptimalBatchSize(size_t total_alarms) const;
    
    /**
     * @brief 필터링 메서드들
     */
    bool PassesSeverityFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool PassesTenantFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool PassesTimeFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool IsAlreadyProcessed(int alarm_id) const;
    void MarkAsProcessed(int alarm_id);
    
    /**
     * @brief 우선순위 정렬
     */
    std::vector<Database::Entities::AlarmOccurrenceEntity> SortByPriority(
        const std::vector<Database::Entities::AlarmOccurrenceEntity>& alarms) const;
    
    /**
     * @brief 진행률 업데이트
     */
    void UpdateProgress(size_t current_index, size_t total);
    
    /**
     * @brief 성능 메트릭 업데이트
     */
    void UpdatePerformanceMetrics(std::chrono::milliseconds duration);
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_STARTUP_RECOVERY_H