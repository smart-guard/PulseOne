/**
 * @file AlarmStartupRecovery.h
 * @brief 시스템 시작 시 DB에서 활성 알람을 Redis로 복구하는 관리자
 * @date 2025-08-31
 * 
 * 핵심 기능:
 * 1. DB에서 state='active' 알람 조회
 * 2. Redis Pub/Sub으로 활성 알람 재발행  
 * 3. Backend AlarmEventSubscriber가 구독하여 WebSocket 전달
 * 4. Frontend activealarm 페이지에 즉시 표시
 */

#ifndef ALARM_STARTUP_RECOVERY_H
#define ALARM_STARTUP_RECOVERY_H

#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <string>

// PulseOne 기본 시스템
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// Database 시스템
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"

// Storage 시스템 
#include "Storage/BackendFormat.h" 
#include "Storage/RedisDataWriter.h"

// Common 구조체
#include "Common/Structs.h"
#include "Common/Enums.h"

namespace PulseOne {
namespace Alarm {

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
    // 생성자/소멸자
    // =============================================================================
    
    AlarmStartupRecovery();
    ~AlarmStartupRecovery();
    
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
    Storage::BackendFormat::AlarmEventData ConvertToBackendFormat(
        const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    /**
     * @brief 단일 알람을 Redis로 발행
     * @param alarm_data Backend 포맷 알람 데이터
     * @return 발행 성공 여부
     */
    bool PublishAlarmToRedis(const Storage::BackendFormat::AlarmEventData& alarm_data);
    
    /**
     * @brief 복구 과정에서 유효성 검증
     * @param occurrence_entity 검증할 알람 엔티티
     * @return 유효한 알람인지 여부
     */
    bool ValidateAlarmForRecovery(const Database::Entities::AlarmOccurrenceEntity& occurrence_entity) const;
    
    // =============================================================================
    // 유틸리티 메서드들
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
     * @brief 오류 처리 및 로깅
     */
    void HandleRecoveryError(const std::string& operation, const std::string& error_message);
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // Repository 참조
    std::shared_ptr<Database::Repositories::AlarmOccurrenceRepository> alarm_occurrence_repo_;
    
    // Redis 발행자
    std::unique_ptr<Storage::RedisDataWriter> redis_data_writer_;
    
    // 상태 관리
    std::atomic<bool> recovery_enabled_{true};        // 복구 기능 활성화 여부
    std::atomic<bool> recovery_completed_{false};     // 복구 완료 여부
    std::atomic<bool> recovery_in_progress_{false};   // 복구 진행 중 여부
    
    // 통계 및 상태
    mutable std::mutex stats_mutex_;
    RecoveryStats recovery_stats_;
    
    // 설정
    static constexpr size_t MAX_RECOVERY_BATCH_SIZE = 100;  // 한 번에 처리할 최대 알람 수
    static constexpr int REDIS_PUBLISH_RETRY_COUNT = 3;     // Redis 발행 재시도 횟수
    static constexpr std::chrono::milliseconds RETRY_DELAY{500}; // 재시도 간 대기시간
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_STARTUP_RECOVERY_H