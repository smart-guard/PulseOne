/**
 * @file DynamicTargetManager.h (싱글턴 리팩토링 버전)
 * @brief 동적 타겟 관리자 - 싱글턴 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-31
 * @version 6.2.0 (PUBLISH 전용 Redis 연결 추가)
 * 
 * 주요 변경사항:
 * - ✅ publish_client_ 멤버 추가 (PUBLISH 전용 Redis 연결)
 * - ✅ getPublishClient() 메서드 추가
 * - ✅ isRedisConnected() 메서드 추가
 * - 싱글턴 패턴 적용
 * - JSON 파일 로드 제거 (DB 전용)
 * - ExportTypes.h 사용 (CSPDynamicTargets.h 대체)
 * - Export 네임스페이스 타입을 CSP에서 사용
 * 
 * 사용법:
 *   auto& manager = DynamicTargetManager::getInstance();
 *   manager.start();
 *   
 *   // PUBLISH 전용 클라이언트 사용
 *   auto* publish_client = manager.getPublishClient();
 *   if (publish_client) {
 *       publish_client->publish("channel", "message");
 *   }
 */

#ifndef DYNAMIC_TARGET_MANAGER_H
#define DYNAMIC_TARGET_MANAGER_H

#include "Export/ExportTypes.h"  // ← CSP/ITargetHandler.h 대체
#include "CSP/AlarmMessage.h"
#include "CSP/FailureProtector.h"
#include "Client/RedisClient.h"  // ✅ 추가
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
    // ✅ Redis 연결 관리 (PUBLISH 전용)
    // =======================================================================
    
    /**
     * @brief PUBLISH 전용 Redis 클라이언트 가져오기
     * @return RedisClient 포인터 (nullptr 가능)
     * 
     * @note AlarmSubscriber는 SUBSCRIBE 모드로 Redis를 점유하므로
     *       별도의 PUBLISH 전용 연결이 필요함
     */
    RedisClient* getPublishClient() { return publish_client_.get(); }
    
    /**
     * @brief Redis 연결 상태 확인
     * @return 연결되어 있으면 true
     */
    bool isRedisConnected() const;
    
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
     * @brief 모든 타겟 조회
     * @return 타겟 목록
     */
    std::vector<DynamicTarget> getAllTargets();
    
    /**
     * @brief 타겟 동적 추가/수정
     * @param target 타겟 정보
     * @return 성공 시 true
     */
    bool addOrUpdateTarget(const DynamicTarget& target);
    
    /**
     * @brief 타겟 제거
     * @param name 타겟 이름
     * @return 성공 시 true
     */
    bool removeTarget(const std::string& name);
    
    /**
     * @brief 타겟 활성화/비활성화
     * @param name 타겟 이름
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool setTargetEnabled(const std::string& name, bool enabled);
    
    /**
     * @brief 동적 타겟 리로드 (호환성)
     * @return 성공 시 true
     */
    bool reloadDynamicTargets();
    
    // =======================================================================
    // 알람 전송
    // =======================================================================
    
    /**
     * @brief 단일 알람 전송 (모든 활성 타겟으로)
     * @param alarm 알람 메시지
     * @return 전송 결과 (타겟별 성공/실패)
     */
    std::vector<TargetSendResult> sendAlarmToTargets(const AlarmMessage& alarm);
    
    /**
     * @brief 특정 타겟으로 알람 전송
     * @param target_name 타겟 이름
     * @param alarm 알람 메시지
     * @return 전송 결과
     */
    TargetSendResult sendAlarmToTarget(const std::string& target_name, const AlarmMessage& alarm);
    
    /**
     * @brief 배치 알람 전송
     * @param alarms 알람 목록
     * @return 배치 처리 결과
     */
    BatchTargetResult sendBatchAlarms(const std::vector<AlarmMessage>& alarms);
    
    /**
     * @brief 비동기 알람 전송 (Future 반환)
     * @param alarm 알람 메시지
     * @return 전송 작업 Future
     */
    std::future<std::vector<TargetSendResult>> sendAlarmAsync(const AlarmMessage& alarm);
    
    // =======================================================================
    // Failure Protector 관리
    // =======================================================================
    
    /**
     * @brief Failure Protector 상태 조회
     * @param target_name 타겟 이름
     * @return 상태 정보
     */
    FailureProtectorStats getFailureProtectorStatus(const std::string& target_name) const;
    
    /**
     * @brief Failure Protector 리셋
     * @param target_name 타겟 이름
     */
    void resetFailureProtector(const std::string& target_name);
    
    /**
     * @brief 모든 Failure Protector 리셋
     */
    void resetAllFailureProtectors();
    
    /**
     * @brief Failure Protector 강제 OPEN
     * @param target_name 타겟 이름
     */
    void forceOpenFailureProtector(const std::string& target_name);
    
    /**
     * @brief 모든 Failure Protector 상태 조회
     * @return 타겟별 상태 맵
     */
    std::unordered_map<std::string, FailureProtectorStats> getFailureProtectorStats() const;
    
    // =======================================================================
    // 핸들러 관리
    // =======================================================================
    
    /**
     * @brief 커스텀 핸들러 등록
     * @param type_name 핸들러 타입 이름
     * @param handler 핸들러 인스턴스
     * @return 성공 시 true
     */
    bool registerHandler(const std::string& type_name, std::unique_ptr<ITargetHandler> handler);
    
    /**
     * @brief 핸들러 제거
     * @param type_name 핸들러 타입 이름
     * @return 성공 시 true
     */
    bool unregisterHandler(const std::string& type_name);
    
    /**
     * @brief 지원되는 핸들러 타입 조회
     * @return 타입 목록
     */
    std::vector<std::string> getSupportedHandlerTypes() const;
    
    // =======================================================================
    // 통계 및 모니터링
    // =======================================================================
    
    /**
     * @brief 전체 통계 조회
     * @return JSON 형식 통계
     */
    json getStatistics() const;
    
    /**
     * @brief 통계 리셋
     */
    void resetStatistics();
    
    /**
     * @brief 헬스체크
     * @return JSON 형식 상태 정보
     */
    json healthCheck() const;
    
    /**
     * @brief 글로벌 설정 조회
     * @return JSON 형식 설정
     */
    json getGlobalSettings() const { return global_settings_; }
    
    /**
     * @brief 글로벌 설정 업데이트
     * @param settings 새 설정
     */
    void updateGlobalSettings(const json& settings);
    
private:
    // =======================================================================
    // Private 생성자/소멸자 (싱글턴)
    // =======================================================================
    
    DynamicTargetManager();
    ~DynamicTargetManager();
    
    // =======================================================================
    // Private 초기화 메서드들
    // =======================================================================
    
    void registerDefaultHandlers();
    
    // ✅ Redis 초기화 (PUBLISH 전용)
    bool initializePublishClient();
    
    // =======================================================================
    // Private 백그라운드 스레드들
    // =======================================================================
    
    void startBackgroundThreads();
    void stopBackgroundThreads();
    
    void healthCheckThread();
    void metricsCollectorThread();
    void cleanupThread();
    
    // =======================================================================
    // Private 유틸리티 메서드
    // =======================================================================
    
    std::vector<DynamicTarget>::iterator findTarget(const std::string& target_name);
    std::vector<DynamicTarget>::const_iterator findTarget(const std::string& target_name) const;
    
    bool processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result);
    json expandConfigVariables(const json& config, const AlarmMessage& alarm);
    
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // ✅ PUBLISH 전용 Redis 클라이언트
    std::unique_ptr<RedisClient> publish_client_;
    
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