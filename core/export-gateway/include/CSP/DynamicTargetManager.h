/**
 * @file DynamicTargetManager.h
 * @brief 동적 타겟 관리자 - JSON 설정 기반 다중 타겟 관리 및 실시간 제어
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/DynamicTargetManager.h
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "CSPDynamicTargets.h"
#include "FailureProtector.h"
#include <shared_mutex>
#include <memory>
#include <future>
#include <unordered_set>
#include <thread>
#include <condition_variable>

namespace PulseOne {
namespace CSP {

/**
 * @brief 동적 타겟 관리자
 * 
 * 주요 기능:
 * - JSON 설정 파일 기반 타겟 관리
 * - 실시간 설정 재로드 (파일 변경 감지)
 * - 병렬 알람 전송 (성능 최적화)
 * - 실패 방지기 통합 (CircuitBreaker 패턴)
 * - 타겟별 상세 통계 수집
 * - 우선순위 기반 처리
 * - 부하 분산 및 Rate Limiting
 * - 헬스 체크 및 자동 복구
 * - 플러그인 아키텍처 (핸들러 동적 등록)
 * - 감사 로그 및 모니터링
 */
class DynamicTargetManager {
public:
    /**
     * @brief 시스템 메트릭
     */
    struct SystemMetrics {
        size_t total_targets = 0;
        size_t active_targets = 0;
        size_t healthy_targets = 0;
        size_t total_requests = 0;
        size_t successful_requests = 0;
        size_t failed_requests = 0;
        double overall_success_rate = 0.0;
        double avg_response_time_ms = 0.0;
        std::chrono::system_clock::time_point last_update;
        
        // 성능 메트릭
        size_t requests_per_second = 0;
        size_t peak_concurrent_requests = 0;
        size_t current_concurrent_requests = 0;
        
        // 리소스 사용량
        double cpu_usage_percent = 0.0;
        size_t memory_usage_mb = 0;
        size_t thread_count = 0;
        
        json toJson() const;
    };
    
    /**
     * @brief 배치 처리 결과
     */
    struct BatchProcessingResult {
        size_t total_alarms = 0;
        size_t processed_alarms = 0;
        size_t successful_deliveries = 0;
        size_t failed_deliveries = 0;
        std::chrono::milliseconds total_processing_time{0};
        std::chrono::milliseconds avg_processing_time{0};
        std::unordered_map<int, BatchTargetResult> building_results;
        
        double getSuccessRate() const {
            return processed_alarms > 0 ? 
                (static_cast<double>(successful_deliveries) / processed_alarms * 100.0) : 0.0;
        }
    };

private:
    // 핵심 데이터
    std::vector<DynamicTarget> targets_;
    std::unordered_map<std::string, std::unique_ptr<ITargetHandler>> handlers_;
    std::unordered_map<std::string, std::shared_ptr<FailureProtector>> failure_protectors_;
    
    // 설정 관리
    json global_settings_;
    std::string config_file_path_;
    std::chrono::system_clock::time_point last_config_check_;
    std::atomic<bool> auto_reload_enabled_{true};
    std::atomic<bool> config_changed_{false};
    
    // 동시성 제어
    mutable std::shared_mutex targets_mutex_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex config_mutex_;
    
    // 성능 최적화
    std::atomic<size_t> concurrent_requests_{0};
    std::atomic<size_t> peak_concurrent_requests_{0};
    
    // 백그라운드 작업
    std::unique_ptr<std::thread> config_watcher_thread_;
    std::unique_ptr<std::thread> health_check_thread_;
    std::unique_ptr<std::thread> metrics_collector_thread_;
    std::atomic<bool> should_stop_{false};
    
    // Rate Limiting
    mutable std::mutex rate_limit_mutex_;
    std::chrono::system_clock::time_point last_rate_reset_;
    std::atomic<size_t> current_requests_count_{0};
    
    // 헬스 체크
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_health_check_;
    mutable std::mutex health_check_mutex_;

public:
    /**
     * @brief 생성자
     * @param config_file_path JSON 설정 파일 경로
     */
    explicit DynamicTargetManager(const std::string& config_file_path);
    
    /**
     * @brief 소멸자
     */
    ~DynamicTargetManager();
    
    // 복사/이동 생성자 비활성화
    DynamicTargetManager(const DynamicTargetManager&) = delete;
    DynamicTargetManager& operator=(const DynamicTargetManager&) = delete;
    DynamicTargetManager(DynamicTargetManager&&) = delete;
    DynamicTargetManager& operator=(DynamicTargetManager&&) = delete;

    // =======================================================================
    // 라이프사이클 관리
    // =======================================================================
    
    /**
     * @brief 매니저 시작 (백그라운드 스레드 시작)
     * @return 시작 성공 여부
     */
    bool start();
    
    /**
     * @brief 매니저 중지 (모든 스레드 정지)
     */
    void stop();
    
    /**
     * @brief 실행 중 여부 확인
     */
    bool isRunning() const { return !should_stop_.load(); }

    // =======================================================================
    // 설정 관리
    // =======================================================================
    
    /**
     * @brief 설정 파일 로드
     * @return 성공 시 true
     */
    bool loadConfiguration();
    
    /**
     * @brief 설정 파일 변경 시 자동 재로드
     * @return 변경된 경우 재로드 후 true
     */
    bool reloadIfChanged();
    
    /**
     * @brief 설정 파일 강제 재로드
     * @return 재로드 성공 여부
     */
    bool forceReload();
    
    /**
     * @brief 자동 재로드 활성화/비활성화
     * @param enabled 활성화 여부
     */
    void setAutoReloadEnabled(bool enabled) { auto_reload_enabled_.store(enabled); }
    
    /**
     * @brief 설정을 파일에 저장
     * @param config 저장할 설정
     * @return 저장 성공 여부
     */
    bool saveConfiguration(const json& config);

    // =======================================================================
    // 알람 전송 (핵심 기능)
    // =======================================================================
    
    /**
     * @brief 모든 활성 타겟에 알람 전송 (순차)
     * @param alarm 전송할 알람 메시지
     * @return 타겟별 전송 결과
     */
    std::vector<TargetSendResult> sendAlarmToAllTargets(const AlarmMessage& alarm);
    
    /**
     * @brief 모든 활성 타겟에 알람 전송 (병렬, 성능 최적화)
     * @param alarm 전송할 알람 메시지
     * @return 타겟별 전송 결과
     */
    std::vector<TargetSendResult> sendAlarmToAllTargetsParallel(const AlarmMessage& alarm);
    
    /**
     * @brief 특정 타겟에만 알람 전송
     * @param alarm 전송할 알람 메시지
     * @param target_name 타겟 이름
     * @return 전송 결과
     */
    TargetSendResult sendAlarmToTarget(const AlarmMessage& alarm, const std::string& target_name);
    
    /**
     * @brief 우선순위별 알람 전송 (CRITICAL > HIGH > MEDIUM > LOW)
     * @param alarm 전송할 알람 메시지
     * @param max_priority 최대 우선순위 (이 값 이하의 우선순위만 처리)
     * @return 타겟별 전송 결과
     */
    std::vector<TargetSendResult> sendAlarmByPriority(const AlarmMessage& alarm, int max_priority = 999);
    
    /**
     * @brief 배치 알람 처리 (멀티빌딩 지원)
     * @param building_alarms 빌딩별 알람 맵
     * @return 빌딩별, 타겟별 전송 결과
     */
    BatchProcessingResult processBuildingAlarms(
        const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms);
    
    /**
     * @brief 비동기 알람 전송 (논블로킹)
     * @param alarm 전송할 알람 메시지
     * @return Future 객체 (결과는 나중에 조회)
     */
    std::future<std::vector<TargetSendResult>> sendAlarmAsync(const AlarmMessage& alarm);

    // =======================================================================
    // 타겟 관리
    // =======================================================================
    
    /**
     * @brief 모든 타겟 연결 테스트
     * @return 타겟별 연결 상태
     */
    std::unordered_map<std::string, bool> testAllConnections();
    
    /**
     * @brief 특정 타겟 연결 테스트
     * @param target_name 타겟 이름
     * @return 연결 성공 여부
     */
    bool testTargetConnection(const std::string& target_name);
    
    /**
     * @brief 타겟 활성화/비활성화
     * @param target_name 타겟 이름
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool enableTarget(const std::string& target_name, bool enabled);
    
    /**
     * @brief 타겟 우선순위 변경
     * @param target_name 타겟 이름
     * @param new_priority 새로운 우선순위
     * @return 성공 시 true
     */
    bool changeTargetPriority(const std::string& target_name, int new_priority);
    
    /**
     * @brief 새 타겟 동적 추가
     * @param target 추가할 타겟
     * @return 성공 시 true
     */
    bool addTarget(const DynamicTarget& target);
    
    /**
     * @brief 타겟 제거
     * @param target_name 제거할 타겟 이름
     * @return 성공 시 true
     */
    bool removeTarget(const std::string& target_name);
    
    /**
     * @brief 타겟 설정 업데이트
     * @param target_name 타겟 이름
     * @param new_config 새로운 설정
     * @return 성공 시 true
     */
    bool updateTargetConfig(const std::string& target_name, const json& new_config);
    
    /**
     * @brief 타겟 목록 조회
     * @param include_disabled 비활성화된 타겟 포함 여부
     * @return 타겟 이름 목록
     */
    std::vector<std::string> getTargetNames(bool include_disabled = true) const;

    // =======================================================================
    // 실패 방지기 관리
    // =======================================================================
    
    /**
     * @brief 실패 방지기 리셋
     * @param target_name 타겟 이름
     */
    void resetFailureProtector(const std::string& target_name);
    
    /**
     * @brief 모든 실패 방지기 리셋
     */
    void resetAllFailureProtectors();
    
    /**
     * @brief 실패 방지기 강제 열기 (테스트용)
     * @param target_name 타겟 이름
     */
    void forceOpenFailureProtector(const std::string& target_name);
    
    /**
     * @brief 실패 방지기 상태 조회
     * @return 타겟별 실패 방지기 상태
     */
    std::unordered_map<std::string, FailureProtector::Statistics> getFailureProtectorStats() const;

    // =======================================================================
    // 통계 및 모니터링
    // =======================================================================
    
    /**
     * @brief 타겟 통계 조회
     * @return 타겟 목록 (통계 포함)
     */
    std::vector<DynamicTarget> getTargetStatistics() const;
    
    /**
     * @brief 전체 시스템 상태 (JSON 형태)
     * @return 시스템 상태 정보
     */
    json getSystemStatus() const;
    
    /**
     * @brief 상세 통계 (JSON 형태)
     * @return 상세 통계 정보
     */
    json getDetailedStatistics() const;
    
    /**
     * @brief 시스템 메트릭 반환
     * @return 시스템 성능 메트릭
     */
    SystemMetrics getSystemMetrics() const;
    
    /**
     * @brief 성능 보고서 생성 (시간 범위별)
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 성능 보고서 JSON
     */
    json generatePerformanceReport(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;

    // =======================================================================
    // 설정 유효성 검증
    // =======================================================================
    
    /**
     * @brief 설정 유효성 검증
     * @param config 검증할 설정
     * @param errors 에러 메시지 목록 (출력)
     * @return 유효하면 true
     */
    bool validateConfiguration(const json& config, std::vector<std::string>& errors);
    
    /**
     * @brief 타겟 설정 검증
     * @param target_config 타겟 설정
     * @param errors 에러 메시지 목록 (출력)
     * @return 유효하면 true
     */
    bool validateTargetConfig(const json& target_config, std::vector<std::string>& errors);

    // =======================================================================
    // 핸들러 관리 (플러그인 아키텍처)
    // =======================================================================
    
    /**
     * @brief 커스텀 핸들러 등록
     * @param type_name 타입 이름
     * @param handler 핸들러 인스턴스
     * @return 등록 성공 여부
     */
    bool registerHandler(const std::string& type_name, std::unique_ptr<ITargetHandler> handler);
    
    /**
     * @brief 핸들러 제거
     * @param type_name 타입 이름
     * @return 제거 성공 여부
     */
    bool unregisterHandler(const std::string& type_name);
    
    /**
     * @brief 지원하는 핸들러 타입 목록
     * @return 핸들러 타입 목록
     */
    std::vector<std::string> getSupportedHandlerTypes() const;

private:
    // =======================================================================
    // 초기화 및 설정
    // =======================================================================
    
    /**
     * @brief 기본 핸들러 등록 (HTTP, S3, MQTT, File)
     */
    void registerDefaultHandlers();
    
    /**
     * @brief 실패 방지기 초기화
     */
    void initializeFailureProtectors();
    
    /**
     * @brief 글로벌 설정 적용
     */
    void applyGlobalSettings();
    
    /**
     * @brief 백그라운드 스레드들 시작
     */
    void startBackgroundThreads();
    
    /**
     * @brief 백그라운드 스레드들 중지
     */
    void stopBackgroundThreads();

    // =======================================================================
    // 타겟 처리 (핵심 로직)
    // =======================================================================
    
    /**
     * @brief 개별 타겟 처리
     * @param target 처리할 타겟
     * @param alarm 알람 메시지
     * @param result 처리 결과 (출력)
     * @return 성공 시 true
     */
    bool processTarget(const DynamicTarget& target, const AlarmMessage& alarm, TargetSendResult& result);
    
    /**
     * @brief 병렬 처리 작업자
     * @param targets 처리할 타겟 목록
     * @param alarm 알람 메시지
     * @param results 결과 벡터 (출력)
     * @param start_index 시작 인덱스
     * @param end_index 종료 인덱스
     */
    void parallelWorker(const std::vector<DynamicTarget>& targets,
                       const AlarmMessage& alarm,
                       std::vector<TargetSendResult>& results,
                       size_t start_index, size_t end_index);
    
    /**
     * @brief Rate Limiting 확인
     * @return 요청 허용 여부
     */
    bool checkRateLimit();
    
    /**
     * @brief 동시 요청 수 제한 확인
     * @return 요청 허용 여부
     */
    bool checkConcurrencyLimit();

    // =======================================================================
    // 변수 확장 및 설정 처리
    // =======================================================================
    
    /**
     * @brief 설정 변수 확장
     * @param config 설정 객체 (입출력)
     * @param alarm 알람 메시지 (변수 소스)
     */
    void expandConfigVariables(json& config, const AlarmMessage& alarm);
    
    /**
     * @brief JSON 객체 재귀 변수 확장
     * @param obj JSON 객체 (입출력)
     * @param alarm 알람 메시지
     */
    void expandJsonVariables(json& obj, const AlarmMessage& alarm);
    
    /**
     * @brief 문자열 변수 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandVariables(const std::string& template_str, const AlarmMessage& alarm);

    // =======================================================================
    // 통계 관리
    // =======================================================================
    
    /**
     * @brief 타겟 통계 업데이트
     * @param target_name 타겟 이름
     * @param success 성공 여부
     * @param response_time 응답 시간
     * @param content_size 전송 데이터 크기
     */
    void updateTargetStatistics(const std::string& target_name, bool success, 
                               std::chrono::milliseconds response_time = std::chrono::milliseconds(0),
                               size_t content_size = 0);
    
    /**
     * @brief 시스템 메트릭 업데이트
     */
    void updateSystemMetrics();
    
    /**
     * @brief 성능 메트릭 수집
     */
    void collectPerformanceMetrics();

    // =======================================================================
    // 백그라운드 스레드들
    // =======================================================================
    
    /**
     * @brief 설정 파일 감시 스레드
     */
    void configWatcherThread();
    
    /**
     * @brief 헬스 체크 스레드
     */
    void healthCheckThread();
    
    /**
     * @brief 메트릭 수집 스레드
     */
    void metricsCollectorThread();
    
    /**
     * @brief 정리 작업 스레드 (오래된 로그, 임시 파일 등)
     */
    void cleanupThread();

    // =======================================================================
    // 유틸리티 메서드
    // =======================================================================
    
    /**
     * @brief 파일 변경 시간 확인
     * @param file_path 파일 경로
     * @return 마지막 변경 시간
     */
    std::chrono::system_clock::time_point getFileModificationTime(const std::string& file_path) const;
    
    /**
     * @brief 타겟 찾기
     * @param target_name 타겟 이름
     * @return 타겟 반복자 (찾지 못하면 end())
     */
    std::vector<DynamicTarget>::iterator findTarget(const std::string& target_name);
    std::vector<DynamicTarget>::const_iterator findTarget(const std::string& target_name) const;
    
    /**
     * @brief 설정 파일 백업
     * @return 백업 성공 여부
     */
    bool backupConfigFile() const;
    
    /**
     * @brief 로그 메시지 생성 (디버그용)
     * @param level 로그 레벨
     * @param message 메시지
     * @param target_name 타겟 이름 (선택)
     */
    void logMessage(const std::string& level, const std::string& message, 
                   const std::string& target_name = "") const;
};

} // namespace CSP
} // namespace PulseOne

#endif // DYNAMIC_TARGET_MANAGER_H