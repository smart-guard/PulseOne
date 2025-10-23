/**
 * @file ScheduledExporter.h
 * @brief DB 기반 스케줄 관리 및 실행 (리팩토링 버전)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 2.0.0
 * 
 * 주요 변경사항 (v1.0 → v2.0):
 * - ❌ DynamicTargetManager 멤버 소유 제거
 * - ✅ DynamicTargetManager::getInstance() 싱글턴 사용
 * - ✅ getTargetWithTemplate() 메서드 활용 (템플릿 로드)
 * - ✅ 초기화 로직 단순화
 * 
 * 기능:
 * - ExportScheduleRepository에서 활성화된 스케줄 로드
 * - Cron 표현식 파싱 및 다음 실행 시각 계산
 * - 각 스케줄의 next_run_at 체크 후 자동 실행
 * - Redis에서 데이터 수집 (data_range, lookback_periods 적용)
 * - ExportTarget별로 데이터 전송 (템플릿 지원)
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
#include "Database/Repositories/PayloadTemplateRepository.h"  // ✅ 추가
#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Client/RedisClient.h"
// ❌ #include "CSP/DynamicTargetManager.h" 제거 (싱글턴 사용)
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
    std::string error_message;
    int data_point_count = 0;
    int64_t execution_time_ms = 0;
    std::chrono::system_clock::time_point execution_timestamp;
    std::vector<std::string> target_names;
    int successful_targets = 0;
    int failed_targets = 0;
    
    json to_json() const {
        return json{
            {"schedule_id", schedule_id},
            {"success", success},
            {"error_message", error_message},
            {"data_point_count", data_point_count},
            {"execution_time_ms", execution_time_ms},
            {"target_names", target_names},
            {"successful_targets", successful_targets},
            {"failed_targets", failed_targets}
        };
    }
};

// =============================================================================
// ScheduledExporter 클래스 (v2.0)
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
     * @brief 스케줄 관리 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief 스케줄 관리 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     * @return 실행 중이면 true
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =========================================================================
    // 수동 실행
    // =========================================================================
    
    /**
     * @brief 특정 스케줄 수동 실행
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
     * @brief 활성 스케줄 목록
     * @return 스케줄 엔티티 목록
     */
    std::vector<PulseOne::Database::Entities::ExportScheduleEntity> getActiveSchedules() const;
    
    /**
     * @brief 특정 스케줄 정보
     * @param schedule_id 스케줄 ID
     * @return 스케줄 엔티티 (optional)
     */
    std::optional<PulseOne::Database::Entities::ExportScheduleEntity> 
        getSchedule(int schedule_id) const;
    
    // =========================================================================
    // 통계
    // =========================================================================
    
    /**
     * @brief 실행 통계
     * @return 통계 JSON
     */
    json getStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStatistics();

private:
    // =========================================================================
    // 내부 메서드
    // =========================================================================
    
    /**
     * @brief 스케줄 체크 루프 (워커 스레드)
     */
    void scheduleCheckLoop();
    
    /**
     * @brief 스케줄 실행 (내부)
     * @param schedule 스케줄 엔티티
     * @return 실행 결과
     */
    ScheduleExecutionResult executeScheduleInternal(
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule);
    
    /**
     * @brief Redis에서 데이터 수집
     * @param schedule 스케줄 엔티티
     * @return 데이터 포인트 목록
     */
    std::vector<ExportDataPoint> collectDataFromRedis(
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule);
    
    /**
     * @brief 데이터를 타겟으로 전송 (v2.0 - 템플릿 지원)
     * @param target_name 타겟 이름
     * @param data_points 데이터 포인트 목록
     * @param schedule 스케줄 엔티티
     * @return 성공 시 true
     */
    bool sendDataToTarget(
        const std::string& target_name,
        const std::vector<ExportDataPoint>& data_points,
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule);
    
    /**
     * @brief 데이터를 템플릿 형식으로 변환 (v2.0 추가)
     * @param template_json 템플릿 JSON
     * @param field_mappings 필드 매핑
     * @param data_points 데이터 포인트 목록
     * @return 변환된 JSON
     */
    json transformDataWithTemplate(
        const json& template_json,
        const json& field_mappings,
        const std::vector<ExportDataPoint>& data_points);
    
    /**
     * @brief 실행 로그 저장
     * @param schedule 스케줄 엔티티
     * @param result 실행 결과
     */
    void saveExecutionLog(
        const PulseOne::Database::Entities::ExportScheduleEntity& schedule,
        const ScheduleExecutionResult& result);
    
    /**
     * @brief 스케줄 상태 업데이트
     * @param schedule 스케줄 엔티티
     * @param success 성공 여부
     */
    void updateScheduleStatus(
        PulseOne::Database::Entities::ExportScheduleEntity& schedule,
        bool success);
    
    /**
     * @brief 다음 실행 시각 계산
     * @param cron_expression Cron 표현식
     * @param from_time 기준 시각
     * @return 다음 실행 시각
     */
    std::chrono::system_clock::time_point calculateNextRun(
        const std::string& cron_expression,
        std::chrono::system_clock::time_point from_time);
    
    /**
     * @brief Redis 연결 초기화
     * @return 성공 시 true
     */
    bool initializeRedisConnection();
    
    /**
     * @brief Repositories 초기화
     * @return 성공 시 true
     */
    bool initializeRepositories();
    
    /**
     * @brief ❌ initializeDynamicTargetManager() 제거
     * (DynamicTargetManager 싱글턴 사용)
     */
    
    // =========================================================================
    // 멤버 변수
    // =========================================================================
    
    // 설정
    ScheduledExporterConfig config_;
    
    // Redis 클라이언트
    std::shared_ptr<RedisClient> redis_client_;
    
    // Repositories
    std::unique_ptr<PulseOne::Database::Repositories::ExportScheduleRepository> schedule_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportTargetRepository> target_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportLogRepository> log_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportTargetMappingRepository> mapping_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::PayloadTemplateRepository> template_repo_;  // ✅ 추가
    
    // ❌ DynamicTargetManager 멤버 제거 (싱글턴 사용)
    // std::unique_ptr<PulseOne::CSP::DynamicTargetManager> dynamic_target_manager_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // 스케줄 캐시
    std::vector<PulseOne::Database::Entities::ExportScheduleEntity> cached_schedules_;
    mutable std::mutex schedule_mutex_;
    std::chrono::system_clock::time_point last_reload_time_;
    
    // 통계
    std::atomic<uint64_t> total_executions_{0};
    std::atomic<uint64_t> successful_executions_{0};
    std::atomic<uint64_t> failed_executions_{0};
    std::atomic<int64_t> total_data_points_exported_{0};
};

} // namespace Schedule
} // namespace PulseOne

#endif // SCHEDULED_EXPORTER_H