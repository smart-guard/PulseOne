/**
 * @file DynamicTargetManager.h - 구조체 정의 위치 수정
 * @brief 동적 타겟 관리자 헤더 - 구조체를 네임스페이스 레벨로 이동
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 5.0.0 (구조체 정의 위치 수정)
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "CSPDynamicTargets.h"
#include "FailureProtector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// 🚨 구조체들을 클래스 외부 네임스페이스 레벨에 정의
// =============================================================================

/**
 * @brief 시스템 메트릭 구조체 (네임스페이스 레벨)
 */
struct SystemMetrics {
    size_t total_targets = 0;
    size_t active_targets = 0;
    size_t healthy_targets = 0;
    uint64_t total_requests = 0;
    uint64_t successful_requests = 0;
    uint64_t failed_requests = 0;
    double overall_success_rate = 0.0;
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
            {"last_update", std::chrono::duration_cast<std::chrono::milliseconds>(
                last_update.time_since_epoch()).count()}
        };
    }
};

/**
 * @brief 배치 처리 결과 구조체 (네임스페이스 레벨)
 */
struct BatchProcessingResult {
    std::vector<TargetSendResult> results;
    size_t total_processed = 0;
    size_t successful_count = 0;
    size_t failed_count = 0;
    std::chrono::milliseconds total_processing_time{0};
    
    json toJson() const {
        json result_array = json::array();
        for (const auto& result : results) {
            result_array.push_back(result.toJson());
        }
        
        return json{
            {"results", result_array},
            {"total_processed", total_processed},
            {"successful_count", successful_count},
            {"failed_count", failed_count},
            {"total_processing_time_ms", total_processing_time.count()}
        };
    }
};

/**
 * @brief 동적 타겟 관리자 클래스
 */
class DynamicTargetManager {
private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 설정 관련
    std::string config_file_path_;
    mutable std::mutex config_mutex_;
    json global_settings_;
    std::atomic<bool> auto_reload_enabled_{true};
    std::chrono::system_clock::time_point last_config_check_;
    
    // 타겟 관리
    std::vector<DynamicTarget> targets_;
    mutable std::shared_mutex targets_mutex_;
    std::unordered_map<std::string, std::unique_ptr<ITargetHandler>> handlers_;
    std::unordered_map<std::string, std::shared_ptr<FailureProtector>> failure_protectors_;
    
    // 동시성 및 성능 제어
    std::atomic<size_t> concurrent_requests_{0};
    std::atomic<size_t> peak_concurrent_requests_{0};
    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> successful_requests_{0};
    std::atomic<size_t> failed_requests_{0};
    
    // Rate Limiting
    std::chrono::system_clock::time_point last_rate_reset_;
    std::atomic<size_t> current_rate_count_{0};
    
    // 백그라운드 스레드들
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};
    std::unique_ptr<std::thread> config_watcher_thread_;
    std::unique_ptr<std::thread> health_check_thread_;
    std::unique_ptr<std::thread> metrics_collector_thread_;
    std::unique_ptr<std::thread> cleanup_thread_;

    // Condition Variable들 (MqttFailover 패턴)
    std::condition_variable config_watcher_cv_;
    std::condition_variable health_check_cv_;
    std::condition_variable metrics_collector_cv_;
    std::mutex cv_mutex_;  // condition variable용 mutex

public:
    // =======================================================================
    // 생성자 및 라이프사이클
    // =======================================================================
    
    explicit DynamicTargetManager(const std::string& config_file_path = "targets.json");
    ~DynamicTargetManager();
    
    // 복사/이동 방지
    DynamicTargetManager(const DynamicTargetManager&) = delete;
    DynamicTargetManager& operator=(const DynamicTargetManager&) = delete;
    DynamicTargetManager(DynamicTargetManager&&) = delete;
    DynamicTargetManager& operator=(DynamicTargetManager&&) = delete;
    
    bool start();
    void stop();
    bool isRunning() const { return is_running_.load(); }
    
    // =======================================================================
    // 설정 관리
    // =======================================================================
    
    bool loadConfiguration();
    bool reloadIfChanged();
    bool forceReload();
    void setAutoReloadEnabled(bool enabled) { auto_reload_enabled_.store(enabled); }
    bool saveConfiguration(const json& config);

    // =======================================================================
    // 알람 전송 (핵심 기능)
    // =======================================================================
    
    std::vector<TargetSendResult> sendAlarmToAllTargets(const AlarmMessage& alarm);
    std::vector<TargetSendResult> sendAlarmToAllTargetsParallel(const AlarmMessage& alarm);
    TargetSendResult sendAlarmToTarget(const AlarmMessage& alarm, const std::string& target_name);
    std::future<std::vector<TargetSendResult>> sendAlarmAsync(const AlarmMessage& alarm);
    
    // 🚨 이제 네임스페이스 레벨 구조체 사용
    std::vector<TargetSendResult> sendAlarmByPriority(const AlarmMessage& alarm, int max_priority);
    BatchProcessingResult processBuildingAlarms(
        const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms);

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
    std::unordered_map<std::string, FailureProtectorStats> getFailureProtectorStats() const;

    // =======================================================================
    // 통계 및 모니터링
    // =======================================================================
    
    std::vector<DynamicTarget> getTargetStatistics() const;
    json getSystemStatus() const;
    json getDetailedStatistics() const;
    
    // 🚨 네임스페이스 레벨 구조체 사용
    SystemMetrics getSystemMetrics() const;
    json generatePerformanceReport(
        std::chrono::system_clock::time_point start_time,
        std::chrono::system_clock::time_point end_time) const;

    // =======================================================================
    // 설정 유효성 검증
    // =======================================================================
    
    bool validateConfiguration(const json& config, std::vector<std::string>& errors);
    bool validateTargetConfig(const json& target_config, std::vector<std::string>& errors);

    // =======================================================================
    // 핸들러 관리
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
    
    // 백그라운드 스레드들
    void startBackgroundThreads();
    void stopBackgroundThreads();
    void configWatcherThread();
    void healthCheckThread();
    void metricsCollectorThread();
    void cleanupThread();
    
    // 유틸리티 메서드들
    std::vector<DynamicTarget>::iterator findTarget(const std::string& target_name);
    std::vector<DynamicTarget>::const_iterator findTarget(const std::string& target_name) const;
    bool processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result);
    bool checkRateLimit();
    void updateTargetHealth(const std::string& target_name, bool healthy);
    void updateTargetStatistics(const std::string& target_name, const TargetSendResult& result);
    
    // 설정 관리
    bool createDefaultConfigFile();
    bool backupConfigFile();
    void expandConfigVariables(json& config, const AlarmMessage& alarm);
};

} // namespace CSP
} // namespace PulseOne

#endif // DYNAMIC_TARGET_MANAGER_H