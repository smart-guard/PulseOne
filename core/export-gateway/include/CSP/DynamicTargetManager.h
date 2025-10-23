/**
 * @file DynamicTargetManager.h (싱글턴 리팩토링 버전)
 * @brief 동적 타겟 관리자 - 싱글턴 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 6.1.0 (ExportTypes.h 사용)
 * 
 * 주요 변경사항:
 * - 싱글턴 패턴 적용
 * - JSON 파일 로드 제거 (DB 전용)
 * - ExportTypes.h 사용 (CSPDynamicTargets.h 대체)
 * - Export 네임스페이스 타입을 CSP에서 사용
 * 
 * 사용법:
 *   auto& manager = DynamicTargetManager::getInstance();
 *   manager.start();
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "Export/ExportTypes.h"  // ← CSP/ITargetHandler.h 대체
#include "CSP/AlarmMessage.h"
#include "CSP/FailureProtector.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <future>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// Export 네임스페이스 타입들을 CSP에서 사용
// =============================================================================

using PulseOne::Export::TargetSendResult;
using PulseOne::Export::ITargetHandler;
using PulseOne::Export::DynamicTarget;
using PulseOne::Export::FailureProtectorConfig;
using PulseOne::Export::FailureProtectorStats;
using PulseOne::Export::BatchTargetResult;
using PulseOne::Export::TargetHandlerFactory;
using PulseOne::Export::TargetHandlerCreator;

// =============================================================================
// 배치 처리 결과 (호환성 - 이전 이름 유지)
// =============================================================================

using BatchProcessingResult = BatchTargetResult;

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
    
    /**
     * @brief 타겟 조회
     * @param name 타겟 이름
     * @return 타겟 정보
     */
    std::optional<DynamicTarget> getTarget(const std::string& name);
    
    /**
     * @brief 동적 타겟 리로드
     * @return 성공 시 true
     */
    bool reloadDynamicTargets();
    
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
    json expandConfigVariables(const json& config, const AlarmMessage& alarm);
    
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 타겟 목록 (shared_mutex로 보호)
    mutable std::shared_mutex targets_mutex_;
    std::vector<DynamicTarget> targets_;
    
    // 핸들러 맵
    std::unordered_map<std::string, std::unique_ptr<ITargetHandler>> handlers_;
    
    // 실패 방지기 맵
    std::unordered_map<std::string, std::unique_ptr<FailureProtector>> failure_protectors_;
    
    // 실행 상태
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // 백그라운드 스레드들
    std::unique_ptr<std::thread> health_check_thread_;
    std::unique_ptr<std::thread> metrics_thread_;
    std::unique_ptr<std::thread> cleanup_thread_;
    
    // 설정
    json global_settings_;
    
    // 통계 변수들
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> total_successes_{0};
    std::atomic<uint64_t> total_failures_{0};
    std::atomic<uint64_t> concurrent_requests_{0};
    std::atomic<uint64_t> peak_concurrent_requests_{0};
    std::atomic<uint64_t> total_bytes_sent_{0};
    std::atomic<uint64_t> total_response_time_ms_{0};
    std::chrono::system_clock::time_point startup_time_;
};

} // namespace CSP
} // namespace PulseOne

#endif // DYNAMIC_TARGET_MANAGER_H