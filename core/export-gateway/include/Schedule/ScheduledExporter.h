/**
 * @file ScheduledExporter.h
 * @brief DB 기반 스케줄 관리 및 실행 (Cron 표현식 지원)
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 * 
 * 기능:
 * - ExportScheduleRepository에서 활성화된 스케줄 로드
 * - Cron 표현식 파싱 및 다음 실행 시각 계산
 * - 각 스케줄의 next_run_at 체크 후 자동 실행
 * - Redis에서 데이터 수집 (data_range, lookback_periods 적용)
 * - ExportTarget별로 데이터 전송
 * - 실행 결과를 ExportLog에 기록
 * - 스케줄 상태 업데이트 (total_runs, successful_runs 등)
 * 
 * 저장 위치: core/export-gateway/include/Schedule/ScheduledExporter.h
 */

#ifndef SCHEDULED_EXPORTER_H
#define SCHEDULED_EXPORTER_H

#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Client/RedisClient.h"
#include "CSP/DynamicTargetManager.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Schedule {

// =============================================================================
// 설정 구조체
// =============================================================================

struct ScheduledExporterConfig {
    // Redis 연결
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password = "";
    
    // 스케줄 체크 간격
    int check_interval_seconds = 10;  // 10초마다 스케줄 체크
    
    // DB 스케줄 리로드 간격
    int reload_interval_seconds = 60; // 1분마다 DB에서 스케줄 리로드
    
    // 배치 크기
    int batch_size = 100;
    
    // 타임아웃
    int export_timeout_seconds = 300; // 5분
    
    // 로깅
    bool enable_debug_log = false;
};

// =============================================================================
// 데이터 포인트 구조체
// =============================================================================

struct ExportDataPoint {
    int point_id = 0;
    std::string point_name;
    std::string value;
    int64_t timestamp = 0;
    int quality = 0;
    std::string unit;
    
    json to_json() const {
        return json{
            {"point_id", point_id},
            {"point_name", point_name},
            {"value", value},
            {"timestamp", timestamp},
            {"quality", quality},
            {"unit", unit}
        };
    }
};

// =============================================================================
// 스케줄 실행 결과 구조체
// =============================================================================

struct ScheduleExecutionResult {
    int schedule_id = 0;
    bool success = false;
    size_t total_points = 0;
    size_t exported_points = 0;
    size_t failed_points = 0;
    std::chrono::milliseconds execution_time{0};
    std::string error_message;
    std::chrono::system_clock::time_point next_run_time;
};

// =============================================================================
// ScheduledExporter 클래스
// =============================================================================

class ScheduledExporter {
public:
    // =========================================================================
    // 생성자 및 소멸자
    // =========================================================================
    
    explicit ScheduledExporter(const ScheduledExporterConfig& config);
    ~ScheduledExporter();
    
    // 복사/이동 방지
    ScheduledExporter(const ScheduledExporter&) = delete;
    ScheduledExporter& operator=(const ScheduledExporter&) = delete;
    ScheduledExporter(ScheduledExporter&&) = delete;
    ScheduledExporter& operator=(ScheduledExporter&&) = delete;
    
    // =========================================================================
    // 라이프사이클 관리
    // =========================================================================
    
    /**
     * @brief 스케줄러 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief 스케줄러 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =========================================================================
    // 수동 실행
    // =========================================================================
    
    /**
     * @brief 특정 스케줄 즉시 실행
     * @param schedule_id 스케줄 ID
     * @return 실행 결과
     */
    ScheduleExecutionResult executeSchedule(int schedule_id);
    
    /**
     * @brief 모든 활성 스케줄 즉시 실행
     * @return 실행된 스케줄 수
     */
    int executeAllSchedules();
    
    // =========================================================================
    // 스케줄 관리
    // =========================================================================
    
    /**
     * @brief DB에서 스케줄 리로드
     * @return 로드된 스케줄 수
     */
    int reloadSchedules();
    
    /**
     * @brief 활성 스케줄 목록 조회
     * @return 스케줄 ID 목록
     */
    std::vector<int> getActiveScheduleIds() const;
    
    /**
     * @brief 다음 실행 대기 중인 스케줄 조회
     * @return 스케줄 정보 (ID, 다음 실행 시각)
     */
    std::map<int, std::chrono::system_clock::time_point> getUpcomingSchedules() const;
    
    // =========================================================================
    // 통계
    // =========================================================================
    
    /**
     * @brief 통계 조회
     * @return 통계 정보 JSON
     */
    json getStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStatistics();
    
private:
    // =========================================================================
    // 내부 메서드 - 메인 루프
    // =========================================================================
    
    /**
     * @brief 스케줄 체크 루프 (워커 스레드)
     */
    void scheduleCheckLoop();
    
    /**
     * @brief 실행할 스케줄 확인
     * @return 실행할 스케줄 목록
     */
    std::vector<PulseOne::Database::Entities::ExportScheduleEntity> 
        findDueSchedules();
    
    /**
     * @brief 스케줄 실행
     * @param schedule 스케줄 엔티티
     * @return 실행 결과
     */
    ScheduleExecutionResult executeScheduleInternal(
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule);
    
    // =========================================================================
    // 내부 메서드 - 데이터 수집
    // =========================================================================
    
    /**
     * @brief 스케줄에 따른 데이터 수집
     * @param schedule 스케줄 엔티티
     * @param target 타겟 엔티티
     * @return 데이터 포인트 목록
     */
    std::vector<ExportDataPoint> collectDataForSchedule(
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule,
        const PulseOne::Database::Entities::ExportTargetEntity& target);
    
    /**
     * @brief 시간 범위 계산 (data_range, lookback_periods 기반)
     * @param data_range "day", "hour", "minute" 등
     * @param lookback_periods 몇 개 단위 (예: 7일, 24시간)
     * @return (시작 시각, 종료 시각)
     */
    std::pair<std::chrono::system_clock::time_point, 
              std::chrono::system_clock::time_point>
        calculateTimeRange(const std::string& data_range, int lookback_periods);
    
    /**
     * @brief Redis에서 포인트 데이터 조회
     * @param point_id 포인트 ID
     * @param start_time 시작 시각
     * @param end_time 종료 시각
     * @return 데이터 포인트 (최신값 또는 평균)
     */
    std::optional<ExportDataPoint> fetchPointData(
        int point_id,
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time);
    
    // =========================================================================
    // 내부 메서드 - 데이터 전송
    // =========================================================================
    
    /**
     * @brief 타겟으로 데이터 전송
     * @param target 타겟 엔티티
     * @param data_points 데이터 포인트 목록
     * @return 성공 여부
     */
    bool sendDataToTarget(
        const PulseOne::Database::Entities::ExportTargetEntity& target,
        const std::vector<ExportDataPoint>& data_points);
    
    /**
     * @brief 배치 생성
     * @param data_points 데이터 포인트 목록
     * @return 배치 목록
     */
    std::vector<std::vector<ExportDataPoint>> createBatches(
        const std::vector<ExportDataPoint>& data_points);
    
    // =========================================================================
    // 내부 메서드 - Cron 처리
    // =========================================================================
    
    /**
     * @brief Cron 표현식 파싱하여 다음 실행 시각 계산
     * @param cron_expression Cron 표현식 (예: "0 * * * *")
     * @param timezone 타임존 (예: "Asia/Seoul")
     * @param from_time 기준 시각 (기본: 현재)
     * @return 다음 실행 시각
     */
    std::chrono::system_clock::time_point calculateNextRunTime(
        const std::string& cron_expression,
        const std::string& timezone,
        const std::chrono::system_clock::time_point& from_time = 
            std::chrono::system_clock::now());
    
    // =========================================================================
    // 내부 메서드 - DB 연동
    // =========================================================================
    
    /**
     * @brief 실행 결과를 ExportLog에 기록
     * @param result 실행 결과
     */
    void logExecutionResult(const ScheduleExecutionResult& result);
    
    /**
     * @brief 스케줄 상태 업데이트 (total_runs, next_run_at 등)
     * @param schedule_id 스케줄 ID
     * @param success 성공 여부
     * @param next_run 다음 실행 시각
     */
    bool updateScheduleStatus(
        int schedule_id,
        bool success,
        const std::chrono::system_clock::time_point& next_run);
    
    // =========================================================================
    // 내부 메서드 - 초기화
    // =========================================================================
    
    /**
     * @brief Redis 연결 초기화
     */
    bool initializeRedisConnection();
    
    /**
     * @brief Repository 초기화
     */
    bool initializeRepositories();
    
    /**
     * @brief DynamicTargetManager 초기화
     */
    bool initializeDynamicTargetManager();

    bool loadTargetsFromDatabase();
    
    // =========================================================================
    // 멤버 변수
    // =========================================================================
    
    // 설정
    ScheduledExporterConfig config_;
    
    // Repositories
    std::unique_ptr<PulseOne::Database::Repositories::ExportScheduleRepository> 
        schedule_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportTargetRepository> 
        target_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportLogRepository> 
        log_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportTargetMappingRepository> 
        mapping_repo_;
    
    // Redis 클라이언트
    std::shared_ptr<RedisClient> redis_client_;
    
    // DynamicTargetManager
    std::unique_ptr<PulseOne::CSP::DynamicTargetManager> dynamic_target_manager_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // 스케줄 캐시 (메모리)
    std::map<int, PulseOne::Database::Entities::ExportScheduleEntity> active_schedules_;
    mutable std::mutex schedule_mutex_;
    
    // 마지막 리로드 시각
    std::chrono::system_clock::time_point last_reload_time_;
    
    // 통계
    std::atomic<size_t> total_executions_{0};
    std::atomic<size_t> successful_executions_{0};
    std::atomic<size_t> failed_executions_{0};
    std::atomic<size_t> total_points_exported_{0};
    std::atomic<int64_t> total_execution_time_ms_{0};
    mutable std::mutex stats_mutex_;
};

} // namespace Schedule
} // namespace PulseOne

#endif // SCHEDULED_EXPORTER_H