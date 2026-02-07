/**
 * @file ExportLogRepository.h
 * @brief Export Log Repository - 고급 통계 기능 추가
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 2.0.0
 * 저장 위치: core/shared/include/Database/Repositories/ExportLogRepository.h
 */

#ifndef EXPORT_LOG_REPOSITORY_H
#define EXPORT_LOG_REPOSITORY_H

#include "Database/Entities/ExportLogEntity.h"
#include "Database/Repositories/IRepository.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using ExportLogEntity = PulseOne::Database::Entities::ExportLogEntity;

/**
 * @brief 시간대별 통계 구조체
 */
struct TimeBasedStats {
  std::string time_label; // "2025-10-21 14:00"
  int total_count;
  int success_count;
  int failed_count;
  double success_rate;
  double avg_processing_time_ms;
};

/**
 * @brief 에러 유형별 통계 구조체
 */
struct ErrorTypeStats {
  std::string error_code;
  std::string error_message;
  int occurrence_count;
  std::string first_occurred;
  std::string last_occurred;
};

/**
 * @brief 포인트별 성능 통계
 */
struct PointPerformanceStats {
  int point_id;
  int total_exports;
  int successful_exports;
  int failed_exports;
  double success_rate;
  double avg_processing_time_ms;
  std::string last_export_time;
};

/**
 * @brief 타겟 헬스 체크 결과
 */
struct TargetHealthCheck {
  int target_id;
  std::string health_status; // "healthy", "degraded", "critical", "down"
  double success_rate_1h;
  double success_rate_24h;
  double avg_response_time_ms;
  int consecutive_failures;
  std::string last_success_time;
  std::string last_failure_time;
  std::string last_error_message;
};

/**
 * @brief Export Log Repository 클래스 - 확장 버전
 *
 * 기존 기능:
 * - Export 전송 로그 관리
 * - 타겟별/상태별 로그 조회
 * - 기본 통계 집계
 *
 * 신규 기능:
 * - 시간대별 상세 통계
 * - 에러 유형 분석
 * - 포인트별 성능 분석
 * - 타겟 헬스 체크
 */
class ExportLogRepository : public IRepository<ExportLogEntity> {
public:
  // =======================================================================
  // 생성자 및 소멸자
  // =======================================================================

  ExportLogRepository();
  virtual ~ExportLogRepository();

  // =======================================================================
  // IRepository 인터페이스 구현 (기존)
  // =======================================================================

  std::vector<ExportLogEntity> findAll() override;
  std::optional<ExportLogEntity> findById(int id) override;
  bool save(ExportLogEntity &entity) override;
  bool update(const ExportLogEntity &entity) override;
  bool deleteById(int id) override;
  bool exists(int id) override;

  // =======================================================================
  // 벌크 연산 (기존)
  // =======================================================================

  std::vector<ExportLogEntity> findByIds(const std::vector<int> &ids) override;

  std::vector<ExportLogEntity> findByConditions(
      const std::vector<QueryCondition> &conditions,
      const std::optional<OrderBy> &order_by = std::nullopt,
      const std::optional<Pagination> &pagination = std::nullopt) override;

  int countByConditions(const std::vector<QueryCondition> &conditions) override;

  // =======================================================================
  // Log 전용 조회 메서드 (기존)
  // =======================================================================

  std::vector<ExportLogEntity> findByTargetId(int target_id, int limit = 100);
  std::vector<ExportLogEntity> findByStatus(const std::string &status,
                                            int limit = 100);
  std::vector<ExportLogEntity>
  findByTimeRange(const std::chrono::system_clock::time_point &start_time,
                  const std::chrono::system_clock::time_point &end_time);
  std::vector<ExportLogEntity> findRecent(int hours = 24, int limit = 100);
  std::vector<ExportLogEntity> findRecentFailures(int hours = 24,
                                                  int limit = 100);
  std::vector<ExportLogEntity> findByPointId(int point_id, int limit = 100);

  // =======================================================================
  // 기본 통계 (기존)
  // =======================================================================

  std::map<std::string, int> getTargetStatistics(int target_id, int hours = 24);
  std::map<std::string, int> getOverallStatistics(int hours = 24);
  double getAverageProcessingTime(int target_id, int hours = 24);

  // =======================================================================
  // ✨ 신규: 시간대별 고급 통계
  // =======================================================================

  /**
   * @brief 시간별 전송 통계 (hourly aggregation)
   * @param target_id 타겟 ID (0 = 전체)
   * @param hours 조회 시간 범위
   * @return 시간별 통계 목록
   */
  std::vector<TimeBasedStats> getHourlyStatistics(int target_id,
                                                  int hours = 24);

  /**
   * @brief 일별 전송 통계 (daily aggregation)
   * @param target_id 타겟 ID (0 = 전체)
   * @param days 조회 일 수
   * @return 일별 통계 목록
   */
  std::vector<TimeBasedStats> getDailyStatistics(int target_id, int days = 7);

  // =======================================================================
  // ✨ 신규: 에러 분석
  // =======================================================================

  /**
   * @brief 에러 유형별 통계
   * @param target_id 타겟 ID (0 = 전체)
   * @param hours 조회 시간 범위
   * @param limit 최대 개수
   * @return 에러 유형별 발생 횟수
   */
  std::vector<ErrorTypeStats>
  getErrorTypeStatistics(int target_id, int hours = 24, int limit = 10);

  /**
   * @brief 가장 빈번한 에러 조회
   * @param target_id 타겟 ID
   * @param hours 조회 시간 범위
   * @return 가장 많이 발생한 에러 정보
   */
  std::optional<ErrorTypeStats> getMostFrequentError(int target_id,
                                                     int hours = 24);

  // =======================================================================
  // ✨ 신규: 포인트 분석
  // =======================================================================

  /**
   * @brief 포인트별 성능 통계
   * @param target_id 타겟 ID
   * @param hours 조회 시간 범위
   * @param limit 최대 개수
   * @return 포인트별 통계 목록
   */
  std::vector<PointPerformanceStats>
  getPointPerformanceStats(int target_id, int hours = 24, int limit = 100);

  /**
   * @brief 문제가 있는 포인트 조회 (성공률 낮음)
   * @param target_id 타겟 ID
   * @param threshold 임계값 (예: 0.8 = 80% 미만)
   * @param hours 조회 시간 범위
   * @return 문제 포인트 목록
   */
  std::vector<PointPerformanceStats>
  getProblematicPoints(int target_id, double threshold = 0.8, int hours = 24);

  // =======================================================================
  // ✨ 신규: 타겟 헬스 체크
  // =======================================================================

  /**
   * @brief 타겟 상태 종합 진단
   * @param target_id 타겟 ID
   * @return 헬스 체크 결과
   */
  TargetHealthCheck getTargetHealthCheck(int target_id);

  /**
   * @brief 모든 타겟의 헬스 상태 조회
   * @return 타겟별 헬스 체크 결과 목록
   */
  std::vector<TargetHealthCheck> getAllTargetsHealthCheck();

  /**
   * @brief 연속 실패 횟수 조회
   * @param target_id 타겟 ID
   * @return 연속 실패 횟수
   */
  int getConsecutiveFailures(int target_id);

  // =======================================================================
  // 로그 정리 (기존)
  // =======================================================================

  int deleteOlderThan(int days);
  int deleteSuccessLogsOlderThan(int days);

  // =======================================================================
  // 캐시 관리 (기존)
  // =======================================================================

  std::map<std::string, int> getCacheStats() const override;

private:
  // =======================================================================
  // 내부 헬퍼 메서드들
  // =======================================================================

  ExportLogEntity mapRowToEntity(const std::map<std::string, std::string> &row);
  std::map<std::string, std::string>
  entityToParams(const ExportLogEntity &entity);
  bool validateLog(const ExportLogEntity &entity);
  bool ensureTableExists();

  // 새로운 헬퍼 메서드들
  TimeBasedStats
  mapToTimeBasedStats(const std::map<std::string, std::string> &row);
  ErrorTypeStats
  mapToErrorTypeStats(const std::map<std::string, std::string> &row);
  PointPerformanceStats
  mapToPointStats(const std::map<std::string, std::string> &row);
  TargetHealthCheck
  mapToHealthCheck(const std::map<std::string, std::string> &row);
  std::string determineHealthStatus(double success_rate_1h,
                                    double success_rate_24h,
                                    int consecutive_failures);
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_LOG_REPOSITORY_H