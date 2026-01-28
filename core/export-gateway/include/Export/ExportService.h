/**
 * @file ExportService.h
 * @brief Export Gateway 메인 서비스 헤더 - Database 통합
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 2.1.0 (Database 통합)
 * 저장 위치: core/export-gateway/include/Export/ExportService.h
 *
 * 주요 변경:
 * - export_targets 테이블에서 타겟 로드
 * - export_target_mappings 테이블에서 매핑 로드
 * - export_logs 테이블에 결과 저장
 * - 통계를 DB에 업데이트
 */

#ifndef EXPORT_SERVICE_H
#define EXPORT_SERVICE_H

#include "Client/RedisClientImpl.h"
#include "Data/DataReader.h"
#include "Data/DataWriter.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

// Forward declarations
namespace PulseOne {
namespace CSP {
class HttpTargetHandler;
class S3TargetHandler;
class FileTargetHandler;
} // namespace CSP
namespace Shared {
namespace Data {
struct CurrentValue;
}
} // namespace Shared
namespace Database {
class RepositoryFactory;
namespace Entities {
struct ExportTargetEntity;
struct ExportTargetMappingEntity;
struct ExportLogEntity;
} // namespace Entities
} // namespace Database
} // namespace PulseOne

namespace PulseOne {
namespace Export {

/**
 * @brief Export 타겟 설정 구조체
 */
struct ExportTargetConfig {
  int id = 0;
  std::string name;
  std::string type; // "HTTP", "S3", "FILE", "MQTT"
  bool enabled = true;
  std::string endpoint;
  json config; // 타겟별 추가 설정

  // 상태 정보
  std::string status;
  std::chrono::system_clock::time_point last_update_time;
  uint64_t success_count = 0;
  uint64_t failure_count = 0;
};

/**
 * @brief Export Gateway 메인 서비스
 *
 * 역할:
 * 1. Database에서 활성 타겟 로드 (export_targets)
 * 2. Database에서 매핑 정보 로드 (export_target_mappings)
 * 3. Redis에서 실시간 데이터 읽기 (DataReader)
 * 4. 설정된 타겟으로 전송 (HTTP/S3/File)
 * 5. 전송 결과 DB 저장 (export_logs)
 * 6. 통계 업데이트 (export_targets 테이블)
 * 7. 재시도 및 에러 처리 (C# CSPManager 패턴)
 * 8. 실패 데이터 파일 저장
 */
class ExportService {
public:
  // ==========================================================================
  // 생성자 및 초기화
  // ==========================================================================

  ExportService();
  ~ExportService();

  // 복사/이동 생성자 비활성화
  ExportService(const ExportService &) = delete;
  ExportService &operator=(const ExportService &) = delete;
  ExportService(ExportService &&) = delete;
  ExportService &operator=(ExportService &&) = delete;

  /**
   * @brief 서비스 초기화
   * @return 성공 여부
   */
  bool Initialize();

  /**
   * @brief 서비스 시작
   * @return 성공 여부
   */
  bool Start();

  /**
   * @brief 서비스 중지
   */
  void Stop();

  /**
   * @brief 서비스 상태 확인
   */
  bool IsRunning() const { return is_running_.load(); }

  // ==========================================================================
  // Export Target 관리
  // ==========================================================================

  /**
   * @brief 활성화된 타겟 로드
   * @return 로드된 타겟 수
   */
  int LoadActiveTargets();

  /**
   * @brief 특정 타겟 활성화/비활성화
   */
  bool EnableTarget(int target_id, bool enable);

  /**
   * @brief 타겟 상태 업데이트
   */
  void UpdateTargetStatus(int target_id, const std::string &status);

  // ==========================================================================
  // 통계 및 모니터링
  // ==========================================================================

  struct Statistics {
    uint64_t total_exports = 0;
    uint64_t successful_exports = 0;
    uint64_t failed_exports = 0;
    uint64_t total_data_points = 0;
    std::chrono::steady_clock::time_point start_time;
  };

  Statistics GetStatistics() const;
  void ResetStatistics();

private:
  // ==========================================================================
  // 내부 메서드
  // ==========================================================================

  /**
   * @brief 메인 워커 스레드
   */
  void workerThread();

  /**
   * @brief 단일 타겟으로 데이터 전송 (재시도 포함)
   * @param target 타겟 설정
   * @param data 전송할 데이터
   * @return 성공 여부
   */
  bool
  exportToTargetWithRetry(const ExportTargetConfig &target,
                          const std::vector<Shared::Data::CurrentValue> &data);

  /**
   * @brief 단일 타겟으로 데이터 전송 (1회 시도)
   */
  bool exportToTarget(const ExportTargetConfig &target,
                      const std::vector<Shared::Data::CurrentValue> &data);

  /**
   * @brief 전송할 포인트 ID 수집
   */
  std::vector<int>
  collectPointIdsForExport(const std::vector<ExportTargetConfig> &targets);

  /**
   * @brief 전송 결과 로깅
   */
  void logExportResult(int target_id, const std::string &status, int data_count,
                       const std::string &error_message = "");

  /**
   * @brief 실패 데이터 파일 저장 (C# 패턴)
   */
  void saveFailedData(const ExportTargetConfig &target,
                      const std::vector<Shared::Data::CurrentValue> &data,
                      const std::string &error_message);

  // ==========================================================================
  // 멤버 변수
  // ==========================================================================

  // Redis 클라이언트 및 데이터 리더/라이터
  std::shared_ptr<RedisClient> redis_client_;
  std::unique_ptr<Shared::Data::DataReader> data_reader_;
  std::unique_ptr<Shared::Data::DataWriter> data_writer_;

  // Target 핸들러들
  std::unique_ptr<CSP::HttpTargetHandler> http_handler_;
  std::unique_ptr<CSP::S3TargetHandler> s3_handler_;
  std::unique_ptr<CSP::FileTargetHandler> file_handler_;

  // Export 타겟 설정
  std::vector<ExportTargetConfig> active_targets_;
  mutable std::mutex targets_mutex_;

  // 워커 스레드
  std::unique_ptr<std::thread> worker_thread_;
  std::atomic<bool> is_running_{false};

  // 통계
  Statistics stats_;
  mutable std::mutex stats_mutex_;

  // 설정
  int export_interval_ms_{1000}; // 1초마다 체크
  int batch_size_{100};          // 한 번에 처리할 데이터 수
  int max_retry_count_{3};       // 최대 재시도 횟수 (C# 기본값)
  int retry_delay_ms_{1000};     // 재시도 간격 (C# 기본값)
  std::string failed_data_dir_;  // 실패 데이터 저장 디렉토리
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_SERVICE_H