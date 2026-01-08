/**
 * @file AlarmStartupRecovery.h
 * @brief 시스템 시작 시 DB에서 활성 알람을 Redis로 복구하는 관리자 - 최적화 버전
 * @date 2025-09-01
 * 
 * 수정사항:
 * 1. 불필요한 변환 함수 선언 제거 (AlarmTypes.h 함수 사용)
 * 2. enum 직접 사용으로 단순화
 * 3. 중복 선언 완전 제거
 */

#ifndef ALARM_STARTUP_RECOVERY_H
#define ALARM_STARTUP_RECOVERY_H

#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <string>
#include <mutex>
#include <map>
#include <set>
#include <optional>

// PulseOne 기본 시스템
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

// Common 구조체 및 타입들
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Alarm/AlarmTypes.h"

// Database 시스템
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"

// Storage 시스템
#include "Storage/BackendFormat.h" 
#include "Storage/RedisDataWriter.h"

namespace PulseOne {
namespace Alarm {

/**
 * @class AlarmStartupRecovery
 * @brief 시스템 재시작 시 활성 알람을 DB에서 Redis로 복구하는 관리자
 * 
 * 동작 원리:
 * 1. 시스템 시작 시 DB 조회 (state='active' AND acknowledged_time IS NULL)
 * 2. 각 활성 알람을 BackendFormat으로 변환 (enum 직접 사용)
 * 3. RedisDataWriter::PublishAlarmEvent()로 Redis 재발행
 * 4. Backend AlarmEventSubscriber가 구독하여 WebSocket 전달
 * 5. Frontend에서 즉시 활성 알람 목록 확인 가능
 */
class AlarmStartupRecovery {
private:
    // =============================================================================
    // 싱글톤 패턴 - 생성자/소멸자 private
    // =============================================================================
    
    AlarmStartupRecovery();
    ~AlarmStartupRecovery();
    
    // unique_ptr 호환성을 위한 friend 선언
    friend std::default_delete<AlarmStartupRecovery>;
    
    // 싱글톤 static 멤버
    static std::unique_ptr<AlarmStartupRecovery> instance_;
    static std::mutex instance_mutex_;

public:
    // =============================================================================
    // 싱글톤 패턴 - 복사/이동 금지
    // =============================================================================
    
    static AlarmStartupRecovery& getInstance();
    
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
     * @brief RDB에서 최신 포인트 값들을 읽어 Redis로 복구 (Warm Startup)
     * @return 복구된 포인트 수
     */
    size_t RecoverLatestPointValues();
    
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
     * @brief 복구 통계 구조체
     */
    struct RecoveryStats {
        size_t total_active_alarms = 0;
        size_t successfully_published = 0;
        size_t failed_to_publish = 0;
        size_t invalid_alarms = 0;
        std::chrono::milliseconds recovery_duration{0};
        std::string last_recovery_time;
        std::string last_error;
    };
    
    RecoveryStats GetRecoveryStats() const { return recovery_stats_; }
    void ResetRecoveryStats();
    
    // =============================================================================
    // 고급 설정 메서드들
    // =============================================================================
    
    /**
     * @brief 복구 정책 설정
     */
    enum class RecoveryPolicy {
        ALL_ACTIVE_ALARMS,
        CRITICAL_ONLY,
        HIGH_AND_CRITICAL,
        TENANT_SPECIFIC,
        TIME_BASED
    };
    
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
    // 복구 제어 메서드들
    // =============================================================================
    
    void PauseRecovery();
    void ResumeRecovery();
    void CancelRecovery();
    double GetRecoveryProgress() const;
    bool IsRecoveryPaused() const { return recovery_paused_.load(); }
    bool IsRecoveryCancelled() const { return recovery_cancelled_.load(); }
    
    // =============================================================================
    // 진단 및 모니터링
    // =============================================================================
    
    std::string GetDiagnosticInfo() const;
    std::map<std::string, double> GetPerformanceMetrics() const;
    std::vector<int> GetProcessedAlarmIds() const;
    void ClearProcessedAlarmCache();

private:
    // =============================================================================
    // 초기화 메서드들
    // =============================================================================
    
    bool InitializeComponents();
    
    // =============================================================================
    // 핵심 복구 로직
    // =============================================================================
    
    std::vector<Database::Entities::AlarmOccurrenceEntity> LoadActiveAlarmsFromDB();
    
    /**
     * @brief AlarmOccurrenceEntity를 Backend 포맷으로 변환 (enum 직접 사용)
     */
    Storage::BackendFormat::AlarmEventData ConvertToBackendFormat(
        const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    bool PublishAlarmToRedis(const Storage::BackendFormat::AlarmEventData& alarm_data);
    bool ValidateAlarmForRecovery(const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    // =============================================================================
    // 유틸리티 메서드들
    // =============================================================================
    
    void HandleRecoveryError(const std::string& context, const std::string& error_msg);
    std::string GetCurrentTimeString() const;
    
    // =============================================================================
    // 배치 처리 메서드들
    // =============================================================================
    
    size_t PublishAlarmBatchToRedis(const std::vector<Storage::BackendFormat::AlarmEventData>& alarm_batch);
    size_t CalculateOptimalBatchSize(size_t total_alarms) const;
    
    // =============================================================================
    // 필터링 메서드들
    // =============================================================================
    
    bool PassesSeverityFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool PassesTenantFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool PassesTimeFilter(const Database::Entities::AlarmOccurrenceEntity& entity) const;
    bool IsAlreadyProcessed(int alarm_id) const;
    void MarkAsProcessed(int alarm_id);
    
    std::vector<Database::Entities::AlarmOccurrenceEntity> SortByPriority(
        const std::vector<Database::Entities::AlarmOccurrenceEntity>& alarms) const;
    
    void UpdateProgress(size_t current_index, size_t total);
    void UpdatePerformanceMetrics(std::chrono::milliseconds duration);
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // Repository 참조
    std::shared_ptr<Database::Repositories::AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // Redis 발행자
    std::shared_ptr<Storage::RedisDataWriter> redis_data_writer_;
    
    // 상태 관리
    std::atomic<bool> recovery_enabled_{true};
    std::atomic<bool> recovery_completed_{false};
    std::atomic<bool> recovery_in_progress_{false};
    std::atomic<bool> recovery_paused_{false};
    std::atomic<bool> recovery_cancelled_{false};
    
    // 통계 및 상태
    mutable std::mutex stats_mutex_;
    RecoveryStats recovery_stats_;
    
    // 진행률 추적
    std::atomic<size_t> current_alarm_index_{0};
    std::atomic<size_t> total_alarms_to_process_{0};
    
    // 설정값들
    static constexpr size_t MAX_RECOVERY_BATCH_SIZE = 100;
    static constexpr int REDIS_PUBLISH_RETRY_COUNT = 3;
    static constexpr std::chrono::milliseconds RETRY_DELAY{500};
    static constexpr std::chrono::seconds RECOVERY_TIMEOUT{300};
    static constexpr std::chrono::milliseconds BATCH_DELAY{50};
    
    // 알람 필터링 설정
    bool filter_by_severity_{false};
    AlarmSeverity min_severity_{AlarmSeverity::INFO};
    bool filter_by_tenant_{false};
    std::vector<int> target_tenants_;
    
    // 성능 모니터링
    std::atomic<size_t> total_recovery_count_{0};
    std::atomic<std::chrono::milliseconds> avg_recovery_time_{std::chrono::milliseconds{0}};
    std::chrono::system_clock::time_point last_recovery_start_time_;
    
    // 고급 제어 플래그들
    bool enable_batch_processing_{true};
    bool enable_priority_recovery_{false};
    bool enable_duplicate_detection_{true};
    bool enable_recovery_logging_{true};
    
    // 복구 정책 설정
    RecoveryPolicy recovery_policy_{RecoveryPolicy::ALL_ACTIVE_ALARMS};
    
    // 시간 기반 필터링
    std::optional<std::chrono::system_clock::time_point> recovery_start_time_filter_;
    std::optional<std::chrono::system_clock::time_point> recovery_end_time_filter_;
    
    // 중복 검출용 캐시
    std::set<int> processed_alarm_ids_;
    mutable std::mutex processed_ids_mutex_;
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_STARTUP_RECOVERY_H