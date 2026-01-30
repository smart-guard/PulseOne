/**
 * @file ExportLogRepository.cpp
 * @brief Export Log Repository 확장 구현 - 고급 통계 기능
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 2.0.0
 */

#include "Database/Repositories/ExportLogRepository.h"
#include "Database/ExtendedSQLQueries.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// ✨ 신규: 시간대별 고급 통계
// =============================================================================

std::vector<TimeBasedStats>
ExportLogRepository::getHourlyStatistics(int target_id, int hours) {

  std::vector<TimeBasedStats> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    // ✅ 정의된 쿼리 사용
    std::string query;
    if (target_id > 0) {
      query = RepositoryHelpers::replaceTwoParameters(
          SQL::ExportLog::GET_HOURLY_STATISTICS_BY_TARGET,
          std::to_string(hours), std::to_string(target_id));
    } else {
      query = RepositoryHelpers::replaceParameter(
          SQL::ExportLog::GET_HOURLY_STATISTICS_ALL, std::to_string(hours));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      try {
        stats.push_back(mapToTimeBasedStats(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map hourly stats: " + std::string(e.what()));
      }
    }

    logger_->Debug("getHourlyStatistics - Found " +
                   std::to_string(stats.size()) + " hours of data");

  } catch (const std::exception &e) {
    logger_->Error("getHourlyStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

std::vector<TimeBasedStats>
ExportLogRepository::getDailyStatistics(int target_id, int days) {

  std::vector<TimeBasedStats> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    // ✅ 정의된 쿼리 사용
    std::string query;
    if (target_id > 0) {
      query = RepositoryHelpers::replaceTwoParameters(
          SQL::ExportLog::GET_DAILY_STATISTICS_BY_TARGET, std::to_string(days),
          std::to_string(target_id));
    } else {
      query = RepositoryHelpers::replaceParameter(
          SQL::ExportLog::GET_DAILY_STATISTICS_ALL, std::to_string(days));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      try {
        stats.push_back(mapToTimeBasedStats(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map daily stats: " + std::string(e.what()));
      }
    }

    logger_->Debug("getDailyStatistics - Found " +
                   std::to_string(stats.size()) + " days of data");

  } catch (const std::exception &e) {
    logger_->Error("getDailyStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

// =============================================================================
// ✨ 신규: 에러 분석
// =============================================================================

std::vector<ErrorTypeStats>
ExportLogRepository::getErrorTypeStatistics(int target_id, int hours,
                                            int limit) {

  std::vector<ErrorTypeStats> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    // ✅ 직접 파라미터 교체 (replaceThreeParameters 없음)
    std::string query;
    if (target_id > 0) {
      query = SQL::ExportLog::GET_ERROR_TYPE_STATISTICS_BY_TARGET;
      // ? 를 값으로 교체
      size_t pos = 0;
      // 첫 번째 ? → hours
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(hours));
      }
      // 두 번째 ? → target_id
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(target_id));
      }
      // 세 번째 ? → limit
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(limit));
      }
    } else {
      query = SQL::ExportLog::GET_ERROR_TYPE_STATISTICS_ALL;
      // 첫 번째 ? → hours
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(hours));
      }
      // 두 번째 ? → limit
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(limit));
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      try {
        stats.push_back(mapToErrorTypeStats(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map error stats: " + std::string(e.what()));
      }
    }

    logger_->Debug("getErrorTypeStatistics - Found " +
                   std::to_string(stats.size()) + " error types");

  } catch (const std::exception &e) {
    logger_->Error("getErrorTypeStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

std::optional<ErrorTypeStats>
ExportLogRepository::getMostFrequentError(int target_id, int hours) {

  try {
    auto errors = getErrorTypeStatistics(target_id, hours, 1);
    if (!errors.empty()) {
      return errors[0];
    }
  } catch (const std::exception &e) {
    logger_->Error("getMostFrequentError failed: " + std::string(e.what()));
  }

  return std::nullopt;
}

// =============================================================================
// ✨ 신규: 포인트 분석
// =============================================================================

std::vector<PointPerformanceStats>
ExportLogRepository::getPointPerformanceStats(int target_id, int hours,
                                              int limit) {

  std::vector<PointPerformanceStats> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    // ✅ 직접 파라미터 교체
    std::string query;
    if (target_id > 0) {
      query = SQL::ExportLog::GET_POINT_PERFORMANCE_STATS_BY_TARGET;
      size_t pos = 0;
      // 첫 번째 ? → hours
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(hours));
      }
      // 두 번째 ? → target_id
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(target_id));
      }
      // 세 번째 ? → limit
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(limit));
      }
    } else {
      query = SQL::ExportLog::GET_POINT_PERFORMANCE_STATS_ALL;
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(hours));
      }
      pos = query.find('?', pos);
      if (pos != std::string::npos) {
        query.replace(pos, 1, std::to_string(limit));
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      try {
        stats.push_back(mapToPointStats(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map point stats: " + std::string(e.what()));
      }
    }

    logger_->Debug("getPointPerformanceStats - Found " +
                   std::to_string(stats.size()) + " points");

  } catch (const std::exception &e) {
    logger_->Error("getPointPerformanceStats failed: " + std::string(e.what()));
  }

  return stats;
}

std::vector<PointPerformanceStats>
ExportLogRepository::getProblematicPoints(int target_id, double threshold,
                                          int hours) {

  std::vector<PointPerformanceStats> problematic;

  try {
    auto all_stats = getPointPerformanceStats(target_id, hours, 1000);

    for (const auto &stat : all_stats) {
      if (stat.success_rate < threshold * 100.0) {
        problematic.push_back(stat);
      }
    }

    // 성공률 낮은 순으로 정렬
    std::sort(problematic.begin(), problematic.end(),
              [](const auto &a, const auto &b) {
                return a.success_rate < b.success_rate;
              });

    logger_->Info("getProblematicPoints - Found " +
                  std::to_string(problematic.size()) + " points below " +
                  std::to_string(threshold * 100.0) + "%");

  } catch (const std::exception &e) {
    logger_->Error("getProblematicPoints failed: " + std::string(e.what()));
  }

  return problematic;
}

// =============================================================================
// ✨ 신규: 타겟 헬스 체크
// =============================================================================

TargetHealthCheck ExportLogRepository::getTargetHealthCheck(int target_id) {
  TargetHealthCheck health;
  health.target_id = target_id;
  health.health_status = "unknown";

  try {
    if (!ensureTableExists()) {
      return health;
    }

    // ✅ 직접 파라미터 교체 (target_id 3번 사용)
    std::string query = SQL::ExportLog::GET_TARGET_HEALTH_CHECK;

    // 첫 번째 ?
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    // 두 번째 ?
    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    // 세 번째 ?
    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      health = mapToHealthCheck(results[0]);
      health.consecutive_failures = getConsecutiveFailures(target_id);
      health.health_status =
          determineHealthStatus(health.success_rate_1h, health.success_rate_24h,
                                health.consecutive_failures);
    }

    logger_->Debug("getTargetHealthCheck - Target " +
                   std::to_string(target_id) +
                   " status: " + health.health_status);

  } catch (const std::exception &e) {
    logger_->Error("getTargetHealthCheck failed: " + std::string(e.what()));
  }

  return health;
}

std::vector<TargetHealthCheck> ExportLogRepository::getAllTargetsHealthCheck() {
  std::vector<TargetHealthCheck> health_checks;

  try {
    if (!ensureTableExists()) {
      return health_checks;
    }

    // ✅ 정의된 쿼리 사용
    DbLib::DatabaseAbstractionLayer db_layer;
    auto results =
        db_layer.executeQuery(SQL::ExportLog::GET_DISTINCT_TARGET_IDS);

    for (const auto &row : results) {
      if (row.find("target_id") != row.end() && !row.at("target_id").empty()) {
        int target_id = std::stoi(row.at("target_id"));
        health_checks.push_back(getTargetHealthCheck(target_id));
      }
    }

    logger_->Info("getAllTargetsHealthCheck - Checked " +
                  std::to_string(health_checks.size()) + " targets");

  } catch (const std::exception &e) {
    logger_->Error("getAllTargetsHealthCheck failed: " + std::string(e.what()));
  }

  return health_checks;
}

int ExportLogRepository::getConsecutiveFailures(int target_id) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    // ✅ 정의된 쿼리 사용
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportLog::GET_CONSECUTIVE_FAILURES, std::to_string(target_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    int consecutive = 0;
    for (const auto &row : results) {
      if (row.find("status") != row.end()) {
        if (row.at("status") == "failed") {
          consecutive++;
        } else if (row.at("status") == "success") {
          break;
        }
      }
    }

    return consecutive;

  } catch (const std::exception &e) {
    logger_->Error("getConsecutiveFailures failed: " + std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// 내부 헬퍼 메서드들 - 새로운 매핑 함수들
// =============================================================================

TimeBasedStats ExportLogRepository::mapToTimeBasedStats(
    const std::map<std::string, std::string> &row) {

  TimeBasedStats stats;

  auto it = row.find("time_label");
  if (it != row.end()) {
    stats.time_label = it->second;
  }

  it = row.find("total_count");
  if (it != row.end() && !it->second.empty()) {
    stats.total_count = std::stoi(it->second);
  }

  it = row.find("success_count");
  if (it != row.end() && !it->second.empty()) {
    stats.success_count = std::stoi(it->second);
  }

  it = row.find("failed_count");
  if (it != row.end() && !it->second.empty()) {
    stats.failed_count = std::stoi(it->second);
  }

  it = row.find("success_rate");
  if (it != row.end() && !it->second.empty()) {
    stats.success_rate = std::stod(it->second);
  }

  it = row.find("avg_processing_time_ms");
  if (it != row.end() && !it->second.empty()) {
    stats.avg_processing_time_ms = std::stod(it->second);
  }

  return stats;
}

ErrorTypeStats ExportLogRepository::mapToErrorTypeStats(
    const std::map<std::string, std::string> &row) {

  ErrorTypeStats stats;

  auto it = row.find("error_code");
  if (it != row.end()) {
    stats.error_code = it->second;
  }

  it = row.find("error_message");
  if (it != row.end()) {
    stats.error_message = it->second;
  }

  it = row.find("occurrence_count");
  if (it != row.end() && !it->second.empty()) {
    stats.occurrence_count = std::stoi(it->second);
  }

  it = row.find("first_occurred");
  if (it != row.end()) {
    stats.first_occurred = it->second;
  }

  it = row.find("last_occurred");
  if (it != row.end()) {
    stats.last_occurred = it->second;
  }

  return stats;
}

PointPerformanceStats ExportLogRepository::mapToPointStats(
    const std::map<std::string, std::string> &row) {

  PointPerformanceStats stats;

  auto it = row.find("point_id");
  if (it != row.end() && !it->second.empty()) {
    stats.point_id = std::stoi(it->second);
  }

  it = row.find("total_exports");
  if (it != row.end() && !it->second.empty()) {
    stats.total_exports = std::stoi(it->second);
  }

  it = row.find("successful_exports");
  if (it != row.end() && !it->second.empty()) {
    stats.successful_exports = std::stoi(it->second);
  }

  it = row.find("failed_exports");
  if (it != row.end() && !it->second.empty()) {
    stats.failed_exports = std::stoi(it->second);
  }

  it = row.find("success_rate");
  if (it != row.end() && !it->second.empty()) {
    stats.success_rate = std::stod(it->second);
  }

  it = row.find("avg_processing_time_ms");
  if (it != row.end() && !it->second.empty()) {
    stats.avg_processing_time_ms = std::stod(it->second);
  }

  it = row.find("last_export_time");
  if (it != row.end()) {
    stats.last_export_time = it->second;
  }

  return stats;
}

TargetHealthCheck ExportLogRepository::mapToHealthCheck(
    const std::map<std::string, std::string> &row) {

  TargetHealthCheck health;

  auto it = row.find("target_id");
  if (it != row.end() && !it->second.empty()) {
    health.target_id = std::stoi(it->second);
  }

  it = row.find("success_rate_1h");
  if (it != row.end() && !it->second.empty()) {
    health.success_rate_1h = std::stod(it->second);
  }

  it = row.find("success_rate_24h");
  if (it != row.end() && !it->second.empty()) {
    health.success_rate_24h = std::stod(it->second);
  }

  it = row.find("avg_response_time_ms");
  if (it != row.end() && !it->second.empty()) {
    health.avg_response_time_ms = std::stod(it->second);
  }

  it = row.find("last_success_time");
  if (it != row.end()) {
    health.last_success_time = it->second;
  }

  it = row.find("last_failure_time");
  if (it != row.end()) {
    health.last_failure_time = it->second;
  }

  it = row.find("last_error_message");
  if (it != row.end()) {
    health.last_error_message = it->second;
  }

  return health;
}

std::string ExportLogRepository::determineHealthStatus(
    double success_rate_1h, double success_rate_24h, int consecutive_failures) {

  // 연속 10회 이상 실패 = DOWN
  if (consecutive_failures >= 10) {
    return "down";
  }

  // 최근 1시간 성공률 < 50% = CRITICAL
  if (success_rate_1h < 50.0) {
    return "critical";
  }

  // 최근 1시간 성공률 < 80% = DEGRADED
  if (success_rate_1h < 80.0) {
    return "degraded";
  }

  // 24시간 성공률 < 95% = DEGRADED
  if (success_rate_24h < 95.0) {
    return "degraded";
  }

  // 모든 조건 통과 = HEALTHY
  return "healthy";
}

// =============================================================================
// 기존 메서드들 (생략 - 이미 제공된 코드와 동일)
// =============================================================================

std::vector<ExportLogEntity> ExportLogRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error("ExportLogRepository::findAll - Table creation failed");
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::ExportLog::FIND_ALL);

    std::vector<ExportLogEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("ExportLogRepository::findAll - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("ExportLogRepository::findAll - Found " +
                  std::to_string(entities.size()) + " logs");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ExportLogRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

// ... (나머지 기존 메서드들은 이미 제공된 코드와 동일하므로 생략)

std::optional<ExportLogEntity> ExportLogRepository::findById(int id) {
  // 기존 코드와 동일
  try {
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        logger_->Debug("Cache hit for ID: " + std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportLog::FIND_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("findById failed: " + std::string(e.what()));
    return std::nullopt;
  }
}

bool ExportLogRepository::save(ExportLogEntity &entity) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    auto params = entityToParams(entity);
    std::string query = SQL::ExportLog::INSERT;

    // ✅ SQL INSERT 컬럼 순서와 정확히 일치시킴
    // INSERT INTO export_logs (
    //     log_type, service_id, target_id, mapping_id, point_id,
    //     source_value, converted_value, status, error_message, error_code,
    //     response_data, http_status_code, processing_time_ms, timestamp,
    //     client_info
    // ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)

    std::vector<std::string> insert_order = {
        "log_type",           "service_id",    "target_id",
        "mapping_id",         "point_id",      "source_value",
        "converted_value",    "status",        "error_message",
        "error_code",         "response_data", "http_status_code",
        "processing_time_ms", "timestamp",     "client_info"};

    // ✅ 순서대로 파라미터 치환
    for (const auto &key : insert_order) {
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        auto it = params.find(key);
        if (it != params.end() && !it->second.empty()) {
          query.replace(
              pos, 1, "'" + RepositoryHelpers::escapeString(it->second) + "'");
        } else {
          // 빈 값 처리
          if (key == "service_id" || key == "target_id" ||
              key == "mapping_id" || key == "point_id" ||
              key == "http_status_code" || key == "processing_time_ms") {
            // 정수형: NULL
            query.replace(pos, 1, "NULL");
          } else {
            // 문자열: 빈 문자열
            query.replace(pos, 1, "''");
          }
        }
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
      if (!results.empty() && results[0].find("id") != results[0].end()) {
        entity.setId(std::stoi(results[0].at("id")));
      }
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("save failed: " + std::string(e.what()));
    return false;
  }
}

bool ExportLogRepository::update(const ExportLogEntity &entity) {
  // 기존 코드 동일
  try {
    if (!ensureTableExists() || entity.getId() <= 0) {
      return false;
    }

    auto params = entityToParams(entity);
    std::string query = SQL::ExportLog::UPDATE;

    std::vector<std::string> update_order = {
        "log_type",           "service_id",    "target_id",
        "mapping_id",         "point_id",      "source_value",
        "converted_value",    "status",        "error_message",
        "error_code",         "response_data", "http_status_code",
        "processing_time_ms", "timestamp",     "client_info"};

    // ✅ 순서대로 파라미터 치환
    for (const auto &key : update_order) {
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        auto it = params.find(key);
        if (it != params.end() && !it->second.empty()) {
          query.replace(
              pos, 1, "'" + RepositoryHelpers::escapeString(it->second) + "'");
        } else {
          // 빈 값 처리
          if (key == "service_id" || key == "target_id" ||
              key == "mapping_id" || key == "point_id" ||
              key == "http_status_code" || key == "processing_time_ms") {
            query.replace(pos, 1, "NULL");
          } else {
            query.replace(pos, 1, "''");
          }
        }
      }
    }

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(entity.getId()));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCacheForId(entity.getId());
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("update failed: " + std::string(e.what()));
    return false;
  }
}

bool ExportLogRepository::deleteById(int id) {
  // 기존 코드 동일
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportLog::DELETE_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCacheForId(id);
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("deleteById failed: " + std::string(e.what()));
    return false;
  }
}

bool ExportLogRepository::exists(int id) {
  // 기존 코드 동일
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportLog::EXISTS_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count")) > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("exists failed: " + std::string(e.what()));
    return false;
  }
}

std::vector<ExportLogEntity>
ExportLogRepository::findByIds(const std::vector<int> &ids) {
  std::vector<ExportLogEntity> entities;

  if (ids.empty()) {
    return entities;
  }

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::ostringstream id_list;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        id_list << ",";
      id_list << ids[i];
    }

    std::string query =
        "SELECT * FROM export_logs WHERE id IN (" + id_list.str() + ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row in findByIds: " +
                      std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByIds failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity> ExportLogRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::ostringstream query;
    query << "SELECT * FROM export_logs";

    if (!conditions.empty()) {
      query << " WHERE ";
      for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0)
          query << " AND ";
        // ✅ 수정: op → operation
        query << conditions[i].field << " " << conditions[i].operation << " '"
              << conditions[i].value << "'";
      }
    }

    if (order_by.has_value()) {
      query << " ORDER BY " << order_by->field << " "
            << (order_by->ascending ? "ASC" : "DESC");
    }

    if (pagination.has_value()) {
      // ✅ 수정: page_size/page_number → limit/offset
      query << " LIMIT " << pagination->limit << " OFFSET "
            << pagination->offset;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query.str());

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByConditions failed: " + std::string(e.what()));
  }

  return entities;
}

int ExportLogRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::ostringstream query;
    query << "SELECT COUNT(*) as count FROM export_logs";

    if (!conditions.empty()) {
      query << " WHERE ";
      for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0)
          query << " AND ";
        // ✅ 수정: op → operation
        query << conditions[i].field << " " << conditions[i].operation << " '"
              << conditions[i].value << "'";
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query.str());

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

  } catch (const std::exception &e) {
    logger_->Error("countByConditions failed: " + std::string(e.what()));
  }

  return 0;
}

std::vector<ExportLogEntity> ExportLogRepository::findByTargetId(int target_id,
                                                                 int limit) {
  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::string query = SQL::ExportLog::FIND_BY_TARGET_ID;

    // 첫 번째 ? → target_id
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    // 두 번째 ? → limit
    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(limit));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByTargetId failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity>
ExportLogRepository::findByStatus(const std::string &status, int limit) {

  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::string query = SQL::ExportLog::FIND_BY_STATUS;

    // 첫 번째 ? → status
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, "'" + status + "'");
    }

    // 두 번째 ? → limit
    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(limit));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByStatus failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity> ExportLogRepository::findByTimeRange(
    const std::chrono::system_clock::time_point &start_time,
    const std::chrono::system_clock::time_point &end_time) {

  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    auto start_t = std::chrono::system_clock::to_time_t(start_time);
    auto end_t = std::chrono::system_clock::to_time_t(end_time);

    char start_buf[20], end_buf[20];
    std::strftime(start_buf, sizeof(start_buf), "%Y-%m-%d %H:%M:%S",
                  std::gmtime(&start_t));
    std::strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S",
                  std::gmtime(&end_t));

    std::ostringstream query;
    query << "SELECT * FROM export_logs WHERE timestamp BETWEEN '" << start_buf
          << "' AND '" << end_buf << "' ORDER BY timestamp DESC";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query.str());

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByTimeRange failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity> ExportLogRepository::findRecent(int hours,
                                                             int limit) {
  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::string query = SQL::ExportLog::FIND_RECENT;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(hours));
    }

    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(limit));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findRecent failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity>
ExportLogRepository::findRecentFailures(int hours, int limit) {
  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::string query = SQL::ExportLog::FIND_RECENT_FAILURES;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(hours));
    }

    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(limit));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findRecentFailures failed: " + std::string(e.what()));
  }

  return entities;
}

std::vector<ExportLogEntity> ExportLogRepository::findByPointId(int point_id,
                                                                int limit) {
  std::vector<ExportLogEntity> entities;

  try {
    if (!ensureTableExists()) {
      return entities;
    }

    std::string query = SQL::ExportLog::FIND_BY_POINT_ID;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(point_id));
    }

    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(limit));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    entities.reserve(results.size());
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("Failed to map row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("findByPointId failed: " + std::string(e.what()));
  }

  return entities;
}

std::map<std::string, int>
ExportLogRepository::getTargetStatistics(int target_id, int hours) {
  std::map<std::string, int> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    std::string query = SQL::ExportLog::GET_STATUS_DISTRIBUTION;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(hours));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      try {
        auto status_it = row.find("status");
        auto count_it = row.find("count");

        if (status_it != row.end() && count_it != row.end()) {
          stats[status_it->second] = std::stoi(count_it->second);
        }
      } catch (const std::exception &e) {
        logger_->Warn("Failed to parse stats row: " + std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("getTargetStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

std::map<std::string, int>
ExportLogRepository::getOverallStatistics(int hours) {
  std::map<std::string, int> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    std::string query = SQL::ExportLog::GET_OVERALL_STATISTICS;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(hours));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      const auto &row = results[0];

      try {
        if (row.find("total_exports") != row.end()) {
          stats["total_exports"] = std::stoi(row.at("total_exports"));
        }
        if (row.find("successful_exports") != row.end()) {
          stats["successful_exports"] = std::stoi(row.at("successful_exports"));
        }
        if (row.find("failed_exports") != row.end()) {
          stats["failed_exports"] = std::stoi(row.at("failed_exports"));
        }
        if (row.find("avg_export_time_ms") != row.end() &&
            !row.at("avg_export_time_ms").empty()) {
          stats["avg_export_time_ms"] =
              static_cast<int>(std::stod(row.at("avg_export_time_ms")));
        }
      } catch (const std::exception &e) {
        logger_->Warn("Failed to parse overall stats: " +
                      std::string(e.what()));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("getOverallStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

double ExportLogRepository::getAverageProcessingTime(int target_id, int hours) {
  try {
    if (!ensureTableExists()) {
      return 0.0;
    }

    std::string query = SQL::ExportLog::GET_AVERAGE_PROCESSING_TIME;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(target_id));
    }

    pos = query.find('?', pos);
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(hours));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("avg_time") != results[0].end()) {
      const std::string &avg_str = results[0].at("avg_time");
      if (!avg_str.empty()) {
        return std::stod(avg_str);
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("getAverageProcessingTime failed: " + std::string(e.what()));
  }

  return 0.0;
}

int ExportLogRepository::deleteOlderThan(int days) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = SQL::ExportLog::DELETE_OLDER_THAN;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(days));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCache();
    }

    return success ? 1 : 0;

  } catch (const std::exception &e) {
    logger_->Error("deleteOlderThan failed: " + std::string(e.what()));
    return 0;
  }
}

int ExportLogRepository::deleteSuccessLogsOlderThan(int days) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = SQL::ExportLog::DELETE_SUCCESS_LOGS_OLDER_THAN;

    size_t pos = query.find('?');
    if (pos != std::string::npos) {
      query.replace(pos, 1, std::to_string(days));
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCache();
    }

    return success ? 1 : 0;

  } catch (const std::exception &e) {
    logger_->Error("deleteSuccessLogsOlderThan failed: " +
                   std::string(e.what()));
    return 0;
  }
}

std::map<std::string, int> ExportLogRepository::getCacheStats() const {
  std::map<std::string, int> stats;

  if (isCacheEnabled()) {
    stats["cache_enabled"] = 1;

    // 부모 클래스의 getCacheStats 호출
    auto base_stats = IRepository<ExportLogEntity>::getCacheStats();

    // 부모 클래스 통계 병합
    for (const auto &[key, value] : base_stats) {
      stats[key] = value;
    }

  } else {
    stats["cache_enabled"] = 0;
    stats["cache_size"] = 0;
    stats["cache_hits"] = 0;
    stats["cache_misses"] = 0;
  }

  return stats;
}

/**
 * @brief DB 행을 ExportLogEntity로 매핑 (완전 수정본)
 * @param row DB 쿼리 결과의 한 행 (컬럼명 → 값)
 * @return ExportLogEntity 객체
 * @throws std::runtime_error 매핑 실패 시
 *
 * 주요 수정사항:
 * 1. safeStoi() 헬퍼 함수 추가 (안전한 정수 변환)
 * 2. 모든 정수 필드에 안전한 변환 적용
 * 3. 빈 문자열, NULL, "0" 처리 개선
 * 4. 예외 처리 강화
 *
 * 파일: core/shared/src/Database/Repositories/ExportLogRepository.cpp
 */
ExportLogEntity ExportLogRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {

  ExportLogEntity entity;

  try {
    // =================================================================
    // 안전한 정수 변환 헬퍼 함수
    // =================================================================
    /**
     * @brief 안전한 문자열 → 정수 변환
     * @param str 변환할 문자열
     * @param default_value 변환 실패 시 기본값 (기본: 0)
     * @return 변환된 정수 또는 기본값
     *
     * 처리 케이스:
     * - empty string → default_value
     * - "NULL", "null" → default_value
     * - "0" → 0 (정상 변환)
     * - 숫자 문자열 → 정수 (정상 변환)
     * - 잘못된 형식 → default_value (예외 catch)
     */
    auto safeStoi = [this](const std::string &str,
                           int default_value = 0) -> int {
      // 1. 빈 문자열 체크
      if (str.empty()) {
        return default_value;
      }

      // 2. NULL 문자열 체크 (대소문자 무관)
      std::string lower_str = str;
      std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                     ::tolower);
      if (lower_str == "null") {
        return default_value;
      }

      // 3. 안전한 stoi 변환 (예외 처리)
      try {
        return std::stoi(str);
      } catch (const std::invalid_argument &e) {
        // 형식 오류 (예: "abc")
        if (logger_) {
          logger_->Debug("ExportLogRepository::safeStoi - Invalid format: '" +
                         str +
                         "', using default: " + std::to_string(default_value));
        }
        return default_value;
      } catch (const std::out_of_range &e) {
        // 범위 초과 (예: "999999999999999999")
        if (logger_) {
          logger_->Warn("ExportLogRepository::safeStoi - Out of range: '" +
                        str +
                        "', using default: " + std::to_string(default_value));
        }
        return default_value;
      } catch (...) {
        // 기타 예외
        if (logger_) {
          logger_->Error(
              "ExportLogRepository::safeStoi - Unknown error for: '" + str +
              "', using default: " + std::to_string(default_value));
        }
        return default_value;
      }
    };

    // =================================================================
    // 필드 매핑 (안전한 변환 적용)
    // =================================================================

    auto it = row.end();

    // -----------------------------------------------------------------
    // 1. ID (Primary Key)
    // -----------------------------------------------------------------
    it = row.find("id");
    if (it != row.end()) {
      entity.setId(safeStoi(it->second, 0));
    }

    // -----------------------------------------------------------------
    // 2. log_type (필수 필드)
    // -----------------------------------------------------------------
    it = row.find("log_type");
    if (it != row.end()) {
      entity.setLogType(it->second);
    } else {
      // log_type이 없으면 경고 로그
      if (logger_) {
        logger_->Warn("ExportLogRepository::mapRowToEntity - Missing log_type");
      }
    }

    // -----------------------------------------------------------------
    // 3. Foreign Keys (nullable)
    // -----------------------------------------------------------------

    // service_id (nullable)
    it = row.find("service_id");
    if (it != row.end()) {
      entity.setServiceId(safeStoi(it->second, 0));
    }

    // target_id (nullable) - ✅ 핵심 수정!
    it = row.find("target_id");
    if (it != row.end()) {
      entity.setTargetId(safeStoi(it->second, 0));
    }

    // mapping_id (nullable)
    it = row.find("mapping_id");
    if (it != row.end()) {
      entity.setMappingId(safeStoi(it->second, 0));
    }

    // point_id (nullable)
    it = row.find("point_id");
    if (it != row.end()) {
      entity.setPointId(safeStoi(it->second, 0));
    }

    // -----------------------------------------------------------------
    // 4. 데이터 필드 (텍스트)
    // -----------------------------------------------------------------

    // source_value
    it = row.find("source_value");
    if (it != row.end()) {
      entity.setSourceValue(it->second);
    }

    // converted_value
    it = row.find("converted_value");
    if (it != row.end()) {
      entity.setConvertedValue(it->second);
    }

    // -----------------------------------------------------------------
    // 5. 결과 필드
    // -----------------------------------------------------------------

    // status (필수 필드)
    it = row.find("status");
    if (it != row.end()) {
      entity.setStatus(it->second);
    } else {
      // status가 없으면 경고
      if (logger_) {
        logger_->Warn("ExportLogRepository::mapRowToEntity - Missing status");
      }
      entity.setStatus("unknown"); // 기본값
    }

    // http_status_code (정수)
    it = row.find("http_status_code");
    if (it != row.end()) {
      entity.setHttpStatusCode(safeStoi(it->second, 0));
    }

    // -----------------------------------------------------------------
    // 6. 에러 정보 (텍스트)
    // -----------------------------------------------------------------

    // error_message
    it = row.find("error_message");
    if (it != row.end()) {
      entity.setErrorMessage(it->second);
    }

    // error_code
    it = row.find("error_code");
    if (it != row.end()) {
      entity.setErrorCode(it->second);
    }

    // response_data
    it = row.find("response_data");
    if (it != row.end()) {
      entity.setResponseData(it->second);
    }

    // -----------------------------------------------------------------
    // 7. 성능 정보 (정수)
    // -----------------------------------------------------------------

    // processing_time_ms
    it = row.find("processing_time_ms");
    if (it != row.end()) {
      entity.setProcessingTimeMs(safeStoi(it->second, 0));
    }

    // -----------------------------------------------------------------
    // 8. 메타 정보
    // -----------------------------------------------------------------

    // client_info (JSON 문자열)
    it = row.find("client_info");
    if (it != row.end()) {
      entity.setClientInfo(it->second);
    }

    // timestamp (날짜/시간 변환)
    it = row.find("timestamp");
    if (it != row.end() && !it->second.empty()) {
      try {
        // SQLite 기본 포맷: "YYYY-MM-DD HH:MM:SS"
        std::tm tm = {};
        std::istringstream ss(it->second);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        if (ss.fail()) {
          // 파싱 실패 시 현재 시각 사용
          if (logger_) {
            logger_->Warn("ExportLogRepository::mapRowToEntity - "
                          "Failed to parse timestamp: '" +
                          it->second + "', using current time");
          }
          entity.setTimestamp(std::chrono::system_clock::now());
        } else {
          // 파싱 성공
          auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
          entity.setTimestamp(tp);
        }
      } catch (const std::exception &e) {
        // 예외 발생 시 현재 시각 사용
        if (logger_) {
          logger_->Error("ExportLogRepository::mapRowToEntity - "
                         "Exception parsing timestamp: " +
                         std::string(e.what()));
        }
        entity.setTimestamp(std::chrono::system_clock::now());
      }
    } else {
      // timestamp가 없으면 현재 시각 사용
      entity.setTimestamp(std::chrono::system_clock::now());
    }

    // =================================================================
    // 매핑 완료 - 디버그 로그
    // =================================================================
    if (logger_ && logger_->getLogLevel() <= LogLevel::DEBUG_LEVEL) {
      logger_->Debug(
          "ExportLogRepository::mapRowToEntity - Mapped successfully: "
          "id=" +
          std::to_string(entity.getId()) +
          ", target_id=" + std::to_string(entity.getTargetId()) +
          ", status=" + entity.getStatus());
    }

  } catch (const std::exception &e) {
    // 최상위 예외 처리 - 매핑 실패
    std::string error_msg =
        "Failed to map row to ExportLogEntity: " + std::string(e.what());

    if (logger_) {
      logger_->Error(error_msg);
    }

    throw std::runtime_error(error_msg);
  } catch (...) {
    // 알 수 없는 예외
    std::string error_msg = "Unknown exception in mapRowToEntity";

    if (logger_) {
      logger_->Error(error_msg);
    }

    throw std::runtime_error(error_msg);
  }

  return entity;
}

std::map<std::string, std::string>
ExportLogRepository::entityToParams(const ExportLogEntity &entity) {

  std::map<std::string, std::string> params;

  params["log_type"] = entity.getLogType();
  params["service_id"] =
      entity.getServiceId() > 0 ? std::to_string(entity.getServiceId()) : "";
  params["target_id"] =
      entity.getTargetId() > 0 ? std::to_string(entity.getTargetId()) : "";
  params["mapping_id"] =
      entity.getMappingId() > 0 ? std::to_string(entity.getMappingId()) : "";
  params["point_id"] =
      entity.getPointId() > 0 ? std::to_string(entity.getPointId()) : "";
  params["source_value"] = entity.getSourceValue();
  params["converted_value"] = entity.getConvertedValue();
  params["status"] = entity.getStatus();
  params["error_message"] = entity.getErrorMessage();
  params["error_code"] = entity.getErrorCode();
  params["response_data"] = entity.getResponseData();
  params["http_status_code"] = std::to_string(entity.getHttpStatusCode());
  params["processing_time_ms"] = std::to_string(entity.getProcessingTimeMs());

  // ✅ 타임스탬프 (KST 기준 문자열로 변환)
  auto tp = entity.getTimestamp();
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::localtime(&tt); // 컨테이너 TZ=Asia/Seoul 반영
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  params["timestamp"] = oss.str();

  params["client_info"] = entity.getClientInfo();

  return params;
}

bool ExportLogRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    bool table_created = db_layer.executeNonQuery(SQL::ExportLog::CREATE_TABLE);
    bool indexes_created =
        db_layer.executeNonQuery(SQL::ExportLog::CREATE_INDEXES);
    return table_created && indexes_created;
  } catch (const std::exception &e) {
    logger_->Error("ensureTableExists failed: " + std::string(e.what()));
    return false;
  }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne