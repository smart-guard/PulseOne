/**
 * @file DynamicTargetManager.h (싱글턴 리팩토링 버전)
 * @brief 동적 타겟 관리자 - 싱글턴 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 6.2.2 - 컴파일 에러 완전 수정
 *
 * 🔧 주요 변경사항 (v6.2.1 → v6.2.2):
 * 1. ✅ ExportTargetEntity.h 헤더 include 추가
 * 2. ✅ export_target_repo_ 멤버 변수 제거
 * 3. ✅ RepositoryFactory를 통한 Repository 접근으로 변경
 *
 * 근본 원인:
 * - export_target_repo_가 멤버 변수로 선언되지 않음
 * - ExportTargetEntity가 불완전 타입 (forward declaration만 있음)
 * - PulseOne 프로젝트의 표준 패턴은 RepositoryFactory 사용
 *
 * 해결 방법:
 * - ExportTargetEntity.h 헤더 포함
 * - loadFromDatabase()에서 직접 RepositoryFactory 사용
 * - 멤버 변수 대신 필요할 때마다 Repository 인스턴스 가져오기
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

#include "CSP/AlarmMessage.h"
#include "CSP/FailureProtector.h"
#include "Client/RedisClient.h"
#include "Export/ExportTypes.h" // ← CSP/ITargetHandler.h 대체

// ✅ v6.2.2: ExportTargetEntity 헤더 포함 (필수!)
#include "CSP/DynamicTargetLoader.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;
// json은 헤더에서 제거됨 (메모리 절감)

namespace PulseOne {
namespace CSP {

// =============================================================================
// Export 네임스페이스 타입들을 CSP에서 사용
// =============================================================================

using PulseOne::Export::BatchTargetResult;
using PulseOne::Export::DynamicTarget;
using PulseOne::Export::FailureProtectorConfig;
using PulseOne::Export::FailureProtectorStats;
using PulseOne::Export::ITargetHandler;
using PulseOne::Export::TargetHandlerCreator;
using PulseOne::Export::TargetHandlerFactory;
using PulseOne::Export::TargetSendResult;

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
  static DynamicTargetManager &getInstance();

  // ✅ Helper methods moved to DynamicTargetLoader

  // Singleton instance
  static DynamicTargetManager *instance_;
  std::mutex mutex_;
  // 복사/이동/삭제 방지
  DynamicTargetManager(const DynamicTargetManager &) = delete;
  DynamicTargetManager &operator=(const DynamicTargetManager &) = delete;
  DynamicTargetManager(DynamicTargetManager &&) = delete;
  DynamicTargetManager &operator=(DynamicTargetManager &&) = delete;

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
  RedisClient *getPublishClient() { return publish_client_.get(); }

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
   *
   * @note RepositoryFactory를 통해 ExportTargetRepository 인스턴스 획득
   */
  bool loadFromDatabase();

  /**
   * @brief 게이트웨이 ID 설정 (필터링 및 하트비트용)
   * @param id 게이트웨이 ID
   */
  void setGatewayId(int id);

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
  std::optional<DynamicTarget> getTargetWithTemplate(const std::string &name);

  /**
   * @brief 타겟 조회
   * @param name 타겟 이름
   * @return 타겟 정보
   */
  std::optional<DynamicTarget> getTarget(const std::string &name);

  std::vector<DynamicTarget> getAllTargets();

  /**
   * @brief 할당된 디바이스 ID 목록 조회 (Selective Subscription용)
   * @return 디바이스 ID set
   */
  std::set<std::string> getAssignedDeviceIds() const;

  /**
   * @brief 타겟 동적 추가/수정
   * @param target 타겟 정보
   * @return 성공 시 true
   */
  bool addOrUpdateTarget(const DynamicTarget &target);

  /**
   * @brief 타겟 제거
   * @param name 타겟 이름
   * @return 성공 시 true
   */
  bool removeTarget(const std::string &name);

  /**
   * @brief 타겟 활성화/비활성화
   * @param name 타겟 이름
   * @param enabled 활성화 여부
   * @return 성공 시 true
   */
  bool setTargetEnabled(const std::string &name, bool enabled);

  /**
   * @brief 동적 타겟 리로드 (호환성)
   * @return 성공 시 true
   */
  bool reloadDynamicTargets();

  /**
   * @brief 중지 요청 여부 확인
   * @return true면 중지 요청됨
   * @note 내부 디버깅용
   */
  bool shouldStop() const {
    return should_stop_.load(std::memory_order_acquire);
  }

  // =======================================================================
  // 알람 전송
  // =======================================================================

  /**
   * @brief 단일 알람 전송 (모든 활성 타겟으로)
   * @param alarm 알람 메시지
   * @return 전송 결과 (타겟별 성공/실패)
   */
  std::vector<TargetSendResult> sendAlarmToTargets(const AlarmMessage &alarm);

  /**
   * @brief 특정 타겟으로 알람 전송
   * @param target_name 타겟 이름
   * @param alarm 알람 메시지
   * @return 전송 결과
   */
  TargetSendResult sendAlarmToTarget(const std::string &target_name,
                                     const AlarmMessage &alarm);

  /**
   * @brief 파일 전송 (모든 활성 타겟으로)
   * @param local_path 로컬 파일 경로
   * @return 전송 결과 목록
   */
  std::vector<TargetSendResult>
  sendFileToTargets(const std::string &local_path);

  /**
   * @brief 배치 알람 전송 (타겟 핸들러의 sendAlarmBatch 호출)
   * @return 배치 처리 결과
   */
  BatchTargetResult
  sendAlarmBatchToTargets(const std::vector<AlarmMessage> &alarms,
                          const std::string &specific_target = "");

  /**
   * @brief 배치 값 전송 (타겟 핸들러의 sendValueBatch 호출)
   * @return 배치 처리 결과
   */
  BatchTargetResult sendValueBatchToTargets(
      const std::vector<PulseOne::CSP::ValueMessage> &values,
      const std::string &type = "value",
      const std::string &specific_target = "");

  /**
   * @brief 비동기 알람 전송 (Future 반환)
   * @param alarm 알람 메시지
   * @return 전송 작업 Future
   */
  std::future<std::vector<TargetSendResult>>
  sendAlarmAsync(const AlarmMessage &alarm);

  // =======================================================================
  // Failure Protector 관리
  // =======================================================================

  /**
   * @brief Failure Protector 상태 조회
   * @param target_name 타겟 이름
   * @return 상태 정보
   */
  FailureProtectorStats
  getFailureProtectorStatus(const std::string &target_name) const;

  /**
   * @brief Failure Protector 리셋
   * @param target_name 타겟 이름
   */
  void resetFailureProtector(const std::string &target_name);

  /**
   * @brief 모든 Failure Protector 리셋
   */
  void resetAllFailureProtectors();

  /**
   * @brief Failure Protector 강제 OPEN
   * @param target_name 타겟 이름
   */
  void forceOpenFailureProtector(const std::string &target_name);

  /**
   * @brief 모든 Failure Protector 상태 조회
   * @return 타겟별 상태 맵
   */
  std::unordered_map<std::string, FailureProtectorStats>
  getFailureProtectorStats() const;

  // =======================================================================
  // 핸들러 관리
  // =======================================================================

  /**
   * @brief 커스텀 핸들러 등록
   * @param type_name 핸들러 타입 이름
   * @param handler 핸들러 인스턴스
   * @return 성공 시 true
   */
  bool registerHandler(const std::string &type_name,
                       std::unique_ptr<ITargetHandler> handler);

  /**
   * @brief 핸들러 제거
   * @param type_name 핸들러 타입 이름
   * @return 성공 시 true
   */
  bool unregisterHandler(const std::string &type_name);

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
  void updateGlobalSettings(const json &settings);

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

  std::vector<DynamicTarget>::iterator
  findTarget(const std::string &target_name);
  std::vector<DynamicTarget>::const_iterator
  findTarget(const std::string &target_name) const;

  bool processTargetByIndex(size_t index, const AlarmMessage &alarm,
                            TargetSendResult &result);
  json expandConfigVariables(const json &config, const AlarmMessage &alarm);

  // =======================================================================
  // 멤버 변수들
  // =======================================================================

  // ✅ PUBLISH 전용 Redis 클라이언트
  std::unique_ptr<RedisClient> publish_client_;

  // ✅ 게이트웨이 ID (필터링용)
  int gateway_id_{0};

  // ❌ export_target_repo_ 멤버 변수 제거!
  // → loadFromDatabase()에서 직접 RepositoryFactory 사용

  // 타겟 목록 (shared_mutex로 보호)
  mutable std::shared_mutex targets_mutex_;
  std::vector<DynamicTarget> targets_;

  // 핸들러 맵
  std::unordered_map<std::string, std::unique_ptr<ITargetHandler>> handlers_;

  // 실패 방지기 맵
  std::unordered_map<std::string, std::unique_ptr<FailureProtector>>
      failure_protectors_;

  // ✅ 매핑 캐시: target_id -> { point_id -> target_field_name }
  mutable std::shared_mutex mappings_mutex_;
  std::unordered_map<int, std::unordered_map<int, std::string>>
      target_point_mappings_;

  // ✅ 포인트별 Site ID 오버라이드 캐시: target_id -> { point_id -> site_id }
  std::unordered_map<int, std::unordered_map<int, int>>
      target_point_site_mappings_;

  // ✅ 포인트별 Scale/Offset 캐시: target_id -> { point_id -> value }
  std::unordered_map<int, std::unordered_map<int, double>> target_point_scales_;
  std::unordered_map<int, std::unordered_map<int, double>>
      target_point_offsets_;

  // ✅ 사이트 매핑 캐시: target_id -> { site_id -> external_building_id }
  std::unordered_map<int, std::unordered_map<int, std::string>>
      target_site_mappings_;

  // ✅ 할당된 디바이스 ID 목록 캐시 (Selective Subscription용)
  std::set<std::string> assigned_device_ids_;

  // 실행 상태
  std::atomic<bool> is_running_{false};
  std::atomic<bool> should_stop_{false};

  // 백그라운드 스레드들
  std::unique_ptr<std::thread> health_check_thread_;
  std::unique_ptr<std::thread> metrics_thread_;
  std::unique_ptr<std::thread> cleanup_thread_;

  // 스레드 제어용 CV
  std::mutex cv_mutex_;
  std::condition_variable cv_;

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