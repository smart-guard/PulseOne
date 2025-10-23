/**
 * @file DynamicTargetManager.h (싱글턴 리팩토링 버전)
 * @brief 동적 타겟 관리자 - 싱글턴 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 6.0.0 (싱글턴 변환)
 * 
 * 주요 변경사항:
 * - 싱글턴 패턴 적용
 * - JSON 파일 로드 제거 (DB 전용)
 * - 생성자 private으로 변경
 * - getInstance() 정적 메서드 추가
 * 
 * 사용법:
 *   auto& manager = DynamicTargetManager::getInstance();
 *   manager.start();
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "CSP/AlarmMessage.h"
#include "CSP/ITargetHandler.h"
#include "CSP/FailureProtector.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// 구조체 정의 (네임스페이스 레벨)
// =============================================================================

struct DynamicTarget {
    std::string name;
    std::string type;
    bool enabled = true;
    int priority = 100;
    std::string description;
    json config;
    
    std::atomic<bool> healthy{true};
    std::atomic<bool> handler_initialized{false};
    std::atomic<uint64_t> request_count{0};
    std::atomic<uint64_t> success_count{0};
    std::atomic<uint64_t> failure_count{0};
};

struct TargetSendResult {
    std::string target_name;
    bool success = false;
    std::string error_message;
    int http_status_code = 0;
    std::string response_body;
    std::chrono::milliseconds response_time{0};
};

struct BatchProcessingResult {
    std::vector<TargetSendResult> results;
    int total_processed = 0;
    int successful_count = 0;
    int failed_count = 0;
    std::chrono::milliseconds total_processing_time{0};
};

// =============================================================================
// DynamicTargetManager 싱글턴 클래스
// =============================================================================

class DynamicTargetManager {
public:
    // =======================================================================
    // 싱글턴 패턴
    // =======================================================================
    
    /**
     * @brief 싱글턴 인스턴스 가져오기
     * @return DynamicTargetManager 참조
     */
    static DynamicTargetManager& getInstance();
    
    // 복사/이동/삭제 방지
    DynamicTargetManager(const DynamicTargetManager&) = delete;
    DynamicTargetManager& operator=(const DynamicTargetManager&) = delete;
    DynamicTargetManager(DynamicTargetManager&&) = delete;
    DynamicTargetManager& operator=(DynamicTargetManager&&) = delete;
    
    // =======================================================================
    // 라이프사이클 관리
    // =======================================================================
    
    /**
     * @brief DynamicTargetManager 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief DynamicTargetManager 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =======================================================================
    // DB 기반 설정 관리 (JSON 파일 제거)
    // =======================================================================
    
    /**
     * @brief 데이터베이스에서 타겟 로드
     * @return 성공 시 true
     */
    bool loadFromDatabase();
    
    /**
     * @brief 타겟 강제 리로드 (DB에서)
     * @return 성공 시 true
     */
    bool forceReload();
    
    /**
     * @brief 템플릿 포함 타겟 조회
     * @param name 타겟 이름
     * @return 타겟 정보 (템플릿 포함)
     */
    std::optional<DynamicTarget> getTargetWithTemplate(const std::string& name);
    
    // =======================================================================
    // 알람 전송 (핵심 기능)
    // =======================================================================
    
    std::vector<TargetSendResult> sendAlarmToAllTargets(const AlarmMessage& alarm);
    std::vector<TargetSendResult> sendAlarmToAllTargetsParallel(const AlarmMessage& alarm);
    TargetSendResult sendAlarmToTarget(const AlarmMessage& alarm, const std::string& target_name);
    std::future<std::vector<TargetSendResult>> sendAlarmAsync(const AlarmMessage& alarm);
    
    std::vector<TargetSendResult> sendAlarmByPriority(const AlarmMessage& alarm, int max_priority);
    BatchProcessingResult processBuildingAlarms(
        const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms);
    
    // =======================================================================
    // 타겟 관리
    // =======================================================================
    
    bool addTarget(const DynamicTarget& target);
    bool removeTarget(const std::string& target_name);
    std::unordered_map<std::string, bool> testAllConnections();
    bool testTargetConnection(const std::string& target_name);
    
    bool enableTarget(const std::string& target_name, bool enabled);
    bool changeTargetPriority(const std::string& target_name, int new_priority);
    bool updateTargetConfig(const std::string& target_name, const json& new_config);
    
    std::vector<std::string> getTargetNames(bool include_disabled = false) const;
    std::vector<DynamicTarget> getTargetStatistics() const;
    
    // =======================================================================
    // 실패 방지기 관리
    // =======================================================================
    
    void resetFailureProtector(const std::string& target_name);
    void resetAllFailureProtectors();
    void forceOpenFailureProtector(const std::string& target_name);
    std::unordered_map<std::string, FailureProtectorStats> getFailureProtectorStats() const;
    
    // =======================================================================
    // 핸들러 관리
    // =======================================================================
    
    bool registerHandler(const std::string& type_name, std::unique_ptr<ITargetHandler> handler);
    bool unregisterHandler(const std::string& type_name);
    std::vector<std::string> getSupportedHandlerTypes() const;

private:
    // =======================================================================
    // 싱글턴 생성자/소멸자 (private)
    // =======================================================================
    
    DynamicTargetManager();
    ~DynamicTargetManager();
    
    // =======================================================================
    // 내부 초기화
    // =======================================================================
    
    void registerDefaultHandlers();
    void initializeFailureProtectors();
    void initializeFailureProtectorForTarget(const std::string& target_name);
    
    // =======================================================================
    // 백그라운드 스레드
    // =======================================================================
    
    void startBackgroundThreads();
    void stopBackgroundThreads();
    void healthCheckThread();
    void metricsCollectorThread();
    void cleanupThread();
    
    // =======================================================================
    // 유틸리티 메서드
    // =======================================================================
    
    std::vector<DynamicTarget>::iterator findTarget(const std::string& target_name);
    std::vector<DynamicTarget>::const_iterator findTarget(const std::string& target_name) const;
    bool processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result);
    bool checkRateLimit();
    void updateTargetHealth(const std::string& target_name, bool healthy);
    void updateTargetStatistics(const std::string& target_name, const TargetSendResult& result);
    void expandConfigVariables(json& config, const AlarmMessage& alarm);
    
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    // 핸들러
    std::unordered_map<std::string, std::unique_ptr<ITargetHandler>> handlers_;
    
    // 타겟 리스트
    std::vector<DynamicTarget> targets_;
    mutable std::shared_mutex targets_mutex_;
    
    // 실패 방지기
    std::unordered_map<std::string, std::shared_ptr<FailureProtector>> failure_protectors_;
    
    // 글로벌 설정
    json global_settings_;
    std::mutex config_mutex_;
    
    // 상태
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // Rate Limiting
    std::atomic<int> concurrent_requests_{0};
    std::atomic<int> peak_concurrent_requests_{0};
    std::atomic<uint64_t> total_requests_{0};
    std::chrono::system_clock::time_point last_rate_reset_;
    std::mutex rate_limit_mutex_;
    
    // 백그라운드 스레드
    std::unique_ptr<std::thread> health_check_thread_;
    std::unique_ptr<std::thread> metrics_collector_thread_;
    std::unique_ptr<std::thread> cleanup_thread_;
    
    // Condition Variables
    std::condition_variable health_check_cv_;
    std::condition_variable metrics_collector_cv_;
    std::mutex cv_mutex_;
};

} // namespace CSP
} // namespace PulseOne

#endif // DYNAMIC_TARGET_MANAGER_H