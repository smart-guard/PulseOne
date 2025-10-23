/**
 * @file ExportCoordinator.h
 * @brief 중앙 집중식 Export 조율자 - CSPGateway 리팩토링 버전
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 1.0.0
 * 
 * 주요 역할:
 * 1. 싱글턴 DynamicTargetManager 관리 (중복 생성 방지)
 * 2. 싱글턴 PayloadTransformer 관리 (중앙 집중식 변환)
 * 3. AlarmSubscriber와 ScheduledExporter 통합 관리
 * 4. 전송 결과 로깅 (ExportLogRepository)
 * 5. 전체 Export 시스템 라이프사이클 관리
 * 
 * 기존 CSPGateway와의 차이점:
 * - CSPGateway: 각 클래스가 독립적으로 DynamicTargetManager 생성
 * - ExportCoordinator: 싱글턴으로 공유 리소스 관리
 * 
 * 저장 위치: core/export-gateway/include/Coordinator/ExportCoordinator.h
 */

#ifndef EXPORT_COORDINATOR_H
#define EXPORT_COORDINATOR_H

#include "Alarm/AlarmSubscriber.h"
#include "Schedule/ScheduledExporter.h"
#include "CSP/DynamicTargetManager.h"
#include "Transform/PayloadTransformer.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Entities/ExportLogEntity.h"
#include "CSP/AlarmMessage.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <chrono>

namespace PulseOne {
namespace Coordinator {

// =============================================================================
// 설정 구조체
// =============================================================================

struct ExportCoordinatorConfig {
    // 데이터베이스 경로
    std::string database_path = "/app/data/db/pulseone.db";
    
    // Redis 설정
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password = "";
    
    // AlarmSubscriber 설정
    std::vector<std::string> alarm_channels = {"alarms:*"};
    std::vector<std::string> alarm_patterns = {"alarm:building:*"};
    int alarm_worker_threads = 4;
    int alarm_max_queue_size = 10000;
    
    // ScheduledExporter 설정
    int schedule_check_interval_seconds = 10;
    int schedule_reload_interval_seconds = 60;
    int schedule_batch_size = 100;
    
    // 공통 설정
    bool enable_debug_log = false;
    int log_retention_days = 30;
    
    // 성능 설정
    int max_concurrent_exports = 50;
    int export_timeout_seconds = 30;
};

// =============================================================================
// 전송 결과 구조체
// =============================================================================

struct ExportResult {
    bool success = false;
    int target_id = 0;
    std::string target_name;
    std::string error_message;
    int http_status_code = 0;
    std::chrono::milliseconds processing_time{0};
    size_t data_size = 0;
};

// =============================================================================
// 통계 구조체
// =============================================================================

struct ExportCoordinatorStats {
    // 전체 통계
    uint64_t total_exports = 0;
    uint64_t successful_exports = 0;
    uint64_t failed_exports = 0;
    
    // 알람 통계
    uint64_t alarm_events = 0;
    uint64_t alarm_exports = 0;
    
    // 스케줄 통계
    uint64_t schedule_executions = 0;
    uint64_t schedule_exports = 0;
    
    // 시간 통계
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_export_time;
    
    // 평균 처리 시간
    double avg_processing_time_ms = 0.0;
};

// =============================================================================
// ExportCoordinator 클래스
// =============================================================================

class ExportCoordinator {
public:
    // =========================================================================
    // 생성자 및 소멸자
    // =========================================================================
    
    explicit ExportCoordinator(const ExportCoordinatorConfig& config);
    ~ExportCoordinator();
    
    // 복사/이동 방지
    ExportCoordinator(const ExportCoordinator&) = delete;
    ExportCoordinator& operator=(const ExportCoordinator&) = delete;
    ExportCoordinator(ExportCoordinator&&) = delete;
    ExportCoordinator& operator=(ExportCoordinator&&) = delete;
    
    // =========================================================================
    // 라이프사이클 관리
    // =========================================================================
    
    /**
     * @brief Export 시스템 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief Export 시스템 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =========================================================================
    // 공유 리소스 접근자 (싱글턴 패턴)
    // =========================================================================
    
    /**
     * @brief 공유 DynamicTargetManager 인스턴스 가져오기
     * @return DynamicTargetManager 포인터
     */
    static std::shared_ptr<PulseOne::CSP::DynamicTargetManager> getTargetManager();
    
    /**
     * @brief 공유 PayloadTransformer 인스턴스 가져오기
     * @return PayloadTransformer 포인터
     */
    static std::shared_ptr<PulseOne::Transform::PayloadTransformer> getPayloadTransformer();
    
    /**
     * @brief 공유 리소스 초기화 (start() 내부에서 호출)
     * @return 성공 시 true
     */
    bool initializeSharedResources();
    
    /**
     * @brief 공유 리소스 정리 (stop() 내부에서 호출)
     */
    void cleanupSharedResources();
    
    // =========================================================================
    // 알람 전송 조율
    // =========================================================================
    
    /**
     * @brief 알람 이벤트 처리 (AlarmSubscriber 콜백)
     * @param alarm 알람 메시지
     * @return 전송 결과 리스트
     */
    std::vector<ExportResult> handleAlarmEvent(const PulseOne::CSP::AlarmMessage& alarm);
    
    /**
     * @brief 다중 알람 배치 처리
     * @param alarms 알람 리스트
     * @return 전송 결과 리스트
     */
    std::vector<ExportResult> handleAlarmBatch(
        const std::vector<PulseOne::CSP::AlarmMessage>& alarms);
    
    // =========================================================================
    // 스케줄 전송 조율
    // =========================================================================
    
    /**
     * @brief 스케줄 실행 (ScheduledExporter 콜백)
     * @param schedule_id 스케줄 ID
     * @return 전송 결과 리스트
     */
    std::vector<ExportResult> handleScheduledExport(int schedule_id);
    
    /**
     * @brief 수동 전송 요청
     * @param target_name 타겟 이름
     * @param data JSON 데이터
     * @return 전송 결과
     */
    ExportResult handleManualExport(const std::string& target_name, const nlohmann::json& data);
    
    // =========================================================================
    // 로깅 및 통계
    // =========================================================================
    
    /**
     * @brief 전송 결과를 DB에 로깅
     * @param result 전송 결과
     * @param alarm_message 알람 메시지 (옵션)
     */
    void logExportResult(const ExportResult& result, 
                        const PulseOne::CSP::AlarmMessage* alarm_message = nullptr);
    
    /**
     * @brief 배치 전송 결과 로깅
     * @param results 전송 결과 리스트
     */
    void logExportResults(const std::vector<ExportResult>& results);
    
    /**
     * @brief 통계 조회
     * @return 현재 통계
     */
    ExportCoordinatorStats getStats() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStats();
    
    /**
     * @brief 특정 타겟의 통계 조회
     * @param target_name 타겟 이름
     * @return 타겟 통계 (total, success, failure, avg_time)
     */
    nlohmann::json getTargetStats(const std::string& target_name) const;
    
    // =========================================================================
    // 설정 관리
    // =========================================================================
    
    /**
     * @brief DB에서 타겟 리로드
     * @return 로드된 타겟 수
     */
    int reloadTargets();
    
    /**
     * @brief DB에서 템플릿 리로드
     * @return 로드된 템플릿 수
     */
    int reloadTemplates();
    
    /**
     * @brief 설정 업데이트
     * @param new_config 새 설정
     */
    void updateConfig(const ExportCoordinatorConfig& new_config);
    
    // =========================================================================
    // 헬스 체크
    // =========================================================================
    
    /**
     * @brief 전체 시스템 헬스 체크
     * @return 정상 시 true
     */
    bool healthCheck() const;
    
    /**
     * @brief 개별 컴포넌트 상태 조회
     * @return {"alarm_subscriber": true, "scheduled_exporter": true, ...}
     */
    nlohmann::json getComponentStatus() const;

private:
    // =========================================================================
    // 내부 초기화 메서드
    // =========================================================================
    
    bool initializeDatabase();
    bool initializeAlarmSubscriber();
    bool initializeScheduledExporter();
    bool initializeRepositories();
    
    // =========================================================================
    // 내부 헬퍼 메서드
    // =========================================================================
    
    ExportResult convertTargetSendResult(
        const PulseOne::CSP::TargetSendResult& target_result) const;
    
    void updateStats(const ExportResult& result);
    
    std::string getDatabasePath() const;
    
    // =========================================================================
    // 공유 리소스 (정적 멤버)
    // =========================================================================
    
    static std::shared_ptr<PulseOne::CSP::DynamicTargetManager> shared_target_manager_;
    static std::shared_ptr<PulseOne::Transform::PayloadTransformer> shared_payload_transformer_;
    static std::mutex init_mutex_;
    static std::atomic<bool> shared_resources_initialized_;
    
    // =========================================================================
    // 멤버 변수
    // =========================================================================
    
    // 설정
    ExportCoordinatorConfig config_;
    
    // 컴포넌트
    std::unique_ptr<PulseOne::Alarm::AlarmSubscriber> alarm_subscriber_;
    std::unique_ptr<PulseOne::Schedule::ScheduledExporter> scheduled_exporter_;
    
    // Repositories
    std::unique_ptr<PulseOne::Database::Repositories::ExportLogRepository> log_repo_;
    std::unique_ptr<PulseOne::Database::Repositories::ExportTargetRepository> target_repo_;
    
    // 상태
    std::atomic<bool> is_running_{false};
    
    // 통계
    mutable std::mutex stats_mutex_;
    ExportCoordinatorStats stats_;
    
    // 동시성 제어
    std::mutex export_mutex_;
    std::atomic<int> current_concurrent_exports_{0};
};

} // namespace Coordinator
} // namespace PulseOne

#endif // EXPORT_COORDINATOR_H