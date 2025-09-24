/**
 * @file DynamicTargetManager.h
 * @brief 동적 타겟 관리자 - 통합 타입 사용 (완성본)
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 2.3.0 (TargetTypes.h 통합 타입 사용)
 * 저장 위치: core/export-gateway/include/CSP/DynamicTargetManager.h
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "CSPDynamicTargets.h"
#include "TargetTypes.h"  // ✅ 통합 타입 사용
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
        
        json toJson() const {
            return json{
                {"total_targets", total_targets},
                {"active_targets", active_targets},
                {"healthy_targets", healthy_targets},
                {"total_requests", total_requests},
                {"successful_requests", successful_requests},
                {"failed_requests", failed_requests},
                {"overall_success_rate", overall_success_rate},
                {"avg_response_time_ms", avg_response_time_ms}
            };
        }
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
    
    bool start();
    void stop();
    bool isRunning() const { return !should_stop_.load(); }

    // =======================================================================
    // 설정 관리
    // =======================================================================
    
    bool loadConfiguration();
    bool reloadIfChanged();
    bool forceReload();
    void setAutoReloadEnabled(bool enabled) { auto_reload_enabled_.store(enabled); }
    bool saveConfiguration(const json& config);

    // =======================================================================
    // 알람 전송 (핵심 기능) - 통합 타입 사용
    // =======================================================================
    
    /**
     * @brief 모든 활성 타겟에 알람 전송 (순차)
     * @param alarm 전송할 알람 메시지
     * @return 타겟별 전송 결과 (TargetTypes.h의 TargetSendResult 사용)
     */
    std::vector<TargetSendResult> sendAlarmToAllTargets(const AlarmMessage& alarm);
    
    /**
     * @brief 모든 활성 타겟에 알람 전송 (병렬, 성능 최적화)
     * @param alarm 전송할 알람 메시지
     * @return 타겟별 전송 결과 (TargetTypes.h의 TargetSendResult 사용)
     */
    std::vector<TargetSendResult> sendAlarmToAllTargetsParallel(const AlarmMessage& alarm);
    
    /**
     * @brief 특정 타겟에만 알람 전송
     * @param alarm 전송할 알람 메시지
     * @param target_name 타겟 이름
     * @return 전송 결과 (TargetTypes.h의 TargetSendResult 사용)
     */
    TargetSendResult sendAlarmToTarget(const AlarmMessage& alarm, const std::string& target_name);
    
    /**
     * @brief 비동기 알람 전송 (논블로킹)
     * @param alarm 전송할 알람 메시지
     * @return Future 객체 (TargetTypes.h의 TargetSendResult 사용)
     */
    std::future<std::vector<TargetSendResult>> sendAlarmAsync(const AlarmMessage& alarm);

    // =======================================================================
    // 타겟 관리
    // =======================================================================
    
    std::unordered_map<std::string, bool> testAllConnections();
    bool testTargetConnection(const std::string& target_name);
    bool enableTarget(const std::string& target_name, bool enabled);
    bool changeTargetPriority(const std::string& target_name, int new_priority);
    bool addTarget(const DynamicTarget& target);
    bool removeTarget(const std::string& target_name);
    bool updateTargetConfig(const std::string& target_name, const json& new_config);
    std::vector<std::string> getTargetNames(bool include_disabled = true) const;

    // =======================================================================
    // 실패 방지기 관리
    // =======================================================================
    
    void resetFailureProtector(const std::string& target_name);
    void resetAllFailureProtectors();
    void forceOpenFailureProtector(const std::string& target_name);

    // =======================================================================
    // 통계 및 모니터링
    // =======================================================================
    
    std::vector<DynamicTarget> getTargetStatistics() const;
    json getSystemStatus() const;
    json getDetailedStatistics() const;
    SystemMetrics getSystemMetrics() const;

    // =======================================================================
    // 설정 유효성 검증
    // =======================================================================
    
    bool validateConfiguration(const json& config, std::vector<std::string>& errors);
    bool validateTargetConfig(const json& target_config, std::vector<std::string>& errors);

    // =======================================================================
    // 핸들러 관리 (플러그인 아키텍처)
    // =======================================================================
    
    bool registerHandler(const std::string& type_name, std::unique_ptr<ITargetHandler> handler);
    bool unregisterHandler(const std::string& type_name);
    std::vector<std::string> getSupportedHandlerTypes() const;

private:
    // =======================================================================
    // 내부 구현 메서드들
    // =======================================================================
    
    void registerDefaultHandlers();
    void initializeFailureProtectors();
    void initializeFailureProtectorForTarget(const std::string& target_name);
    bool createDefaultConfigFile();
    void applyGlobalSettings();
    void startBackgroundThreads();
    void stopBackgroundThreads();
    
    // 타겟 처리 (핵심 로직) - 통합 타입 사용
    bool processTarget(const DynamicTarget& target, const AlarmMessage& alarm, TargetSendResult& result);
    bool processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result);
    bool checkRateLimit();
    bool checkConcurrencyLimit();
    
    // 변수 확장 및 설정 처리
    void expandConfigVariables(json& config, const AlarmMessage& alarm);
    void expandJsonVariables(json& obj, const AlarmMessage& alarm);
    std::string expandVariables(const std::string& template_str, const AlarmMessage& alarm);
    
    // 통계 관리
    void updateTargetStatistics(const std::string& target_name, bool success, 
                               std::chrono::milliseconds response_time = std::chrono::milliseconds(0),
                               size_t content_size = 0);
    void updateSystemMetrics();
    void collectPerformanceMetrics();
    
    // 백그라운드 스레드들
    void configWatcherThread();
    void healthCheckThread();
    void metricsCollectorThread();
    void cleanupThread();
    
    // 유틸리티 메서드들
    std::chrono::system_clock::time_point getFileModificationTime(const std::string& file_path) const;
    std::vector<DynamicTarget>::iterator findTarget(const std::string& target_name);
    std::vector<DynamicTarget>::const_iterator findTarget(const std::string& target_name) const;
    bool backupConfigFile() const;
    void logMessage(const std::string& level, const std::string& message, 
                   const std::string& target_name = "") const;
};

} // namespace CSP
} // namespace PulseOne

#endif // DYNAMIC_TARGET_MANAGER_H