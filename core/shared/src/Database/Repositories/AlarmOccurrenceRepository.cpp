// =============================================================================
// collector/src/Database/Repositories/AlarmOccurrenceRepository.cpp
// PulseOne AlarmOccurrenceRepository 구현 - device_id 타입 완전 수정
// =============================================================================

/**
 * @file AlarmOccurrenceRepository.cpp
 * @brief PulseOne AlarmOccurrenceRepository - device_id INTEGER 타입 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-26
 *
 * 🔧 주요 수정사항:
 * - device_id 타입: string → optional<int> 완전 적용
 * - INSERT/UPDATE/매핑 로직 모두 정수형으로 수정
 * - cleared_by 필드 완전 지원
 */

#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Alarm/AlarmTypes.h"
#include "Database/ExtendedSQLQueries.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      LogManager::getInstance().log("AlarmOccurrenceRepository",
                                    LogLevel::LOG_ERROR,
                                    "findAll - Table creation failed");
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_ALL);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findAll - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                  "findAll - Found " +
                                      std::to_string(entities.size()) +
                                      " alarm occurrences");
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "findAll failed: " + std::string(e.what()));
    return {};
  }
}

std::optional<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findById(int id) {
  try {
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::DEBUG,
            "findById - Cache hit for ID: " + std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmOccurrence::FIND_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      LogManager::getInstance().log(
          "AlarmOccurrenceRepository", LogLevel::DEBUG,
          "findById - No alarm occurrence found for ID: " + std::to_string(id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                  "findById - Found alarm occurrence ID: " +
                                      std::to_string(id));
    return entity;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "findById failed: " + std::string(e.what()));
    return std::nullopt;
  }
}

bool AlarmOccurrenceRepository::save(AlarmOccurrenceEntity &entity) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto params = entityToParams(entity);

    // 발생 시간 설정 (엔티티에 없으면 현재 시간)
    if (entity.getOccurrenceTime().time_since_epoch().count() == 0) {
      params["occurrence_time"] =
          escapeString(timePointToString(std::chrono::system_clock::now()));
    }

    std::string query = SQL::AlarmOccurrence::INSERT_NAMED;
    query = RepositoryHelpers::replaceParametersInOrder(query, params);

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      auto id_results = db_layer.executeQuery(
          PulseOne::Database::SQL::Common::GET_LAST_INSERT_ID);
      if (!id_results.empty() && id_results[0].count("id")) {
        auto last_id = std::stoll(id_results[0].at("id"));
        if (last_id > 0) {
          entity.setId(static_cast<int>(last_id));
          entity.markSaved();
          if (isCacheEnabled()) {
            cacheEntity(entity);
          }
        }
      }
    }

    return success;
  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "save failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmOccurrenceRepository::update(const AlarmOccurrenceEntity &entity) {
  try {
    if (entity.getId() <= 0 || !ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto params = entityToParams(entity);

    std::string query = SQL::AlarmOccurrence::UPDATE;
    query = RepositoryHelpers::replaceParametersInOrder(query, params);

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(entity.getId());
      }
    }

    return success;
  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "update failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmOccurrenceRepository::deleteById(int id) {
  try {
    if (id <= 0 || !ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmOccurrence::DELETE_BY_ID, std::to_string(id));
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }

      LogManager::getInstance().log(
          "AlarmOccurrenceRepository", LogLevel::INFO,
          "deleteById - Deleted alarm occurrence ID: " + std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "deleteById failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmOccurrenceRepository::exists(int id) {
  try {
    if (id <= 0) {
      return false;
    }

    if (isCacheEnabled() && getCachedEntity(id).has_value()) {
      return true;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmOccurrence::EXISTS_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    return !results.empty() && std::stoi(results[0].at("count")) > 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "exists failed: " + std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 벌크 연산 구현 (동일)
// =============================================================================

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findByIds(const std::vector<int> &ids) {
  std::vector<AlarmOccurrenceEntity> results;

  if (ids.empty()) {
    return results;
  }

  try {
    if (!ensureTableExists()) {
      return results;
    }

    std::stringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ",";
      ids_ss << ids[i];
    }

    std::string query = SQL::AlarmOccurrence::FIND_ALL;
    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto query_results = db_layer.executeQuery(query);

    results.reserve(query_results.size());

    for (const auto &row : query_results) {
      try {
        results.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findByIds - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                  "findByIds - Found " +
                                      std::to_string(results.size()) +
                                      " alarm occurrences");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "findByIds failed: " + std::string(e.what()));
  }

  return results;
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  std::vector<AlarmOccurrenceEntity> results;

  try {
    if (!ensureTableExists()) {
      return results;
    }

    std::string query = SQL::AlarmOccurrence::FIND_ALL;
    query += RepositoryHelpers::buildWhereClause(conditions);
    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto query_results = db_layer.executeQuery(query);

    results.reserve(query_results.size());

    for (const auto &row : query_results) {
      try {
        results.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findByConditions - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::DEBUG,
                                  "findByConditions - Found " +
                                      std::to_string(results.size()) +
                                      " alarm occurrences");

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findByConditions failed: " + std::string(e.what()));
  }

  return results;
}

int AlarmOccurrenceRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = SQL::AlarmOccurrence::COUNT_ALL;
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "countByConditions failed: " + std::string(e.what()));
    return 0;
  }
}

std::optional<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findFirstByConditions(
    const std::vector<QueryCondition> &conditions) {

  auto pagination = Pagination{0, 1};
  auto results = findByConditions(conditions, std::nullopt, pagination);

  if (!results.empty()) {
    return results[0];
  }

  return std::nullopt;
}

int AlarmOccurrenceRepository::saveBulk(
    std::vector<AlarmOccurrenceEntity> &entities) {
  int saved_count = 0;
  for (auto &entity : entities) {
    if (save(entity)) {
      saved_count++;
    }
  }
  return saved_count;
}

int AlarmOccurrenceRepository::updateBulk(
    const std::vector<AlarmOccurrenceEntity> &entities) {
  int updated_count = 0;
  for (const auto &entity : entities) {
    if (update(entity)) {
      updated_count++;
    }
  }
  return updated_count;
}

int AlarmOccurrenceRepository::deleteByIds(const std::vector<int> &ids) {
  if (ids.empty()) {
    return 0;
  }

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::stringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ",";
      ids_ss << ids[i];
    }

    std::string query =
        "DELETE FROM alarm_occurrences WHERE id IN (" + ids_ss.str() + ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        for (int id : ids) {
          clearCacheForId(id);
        }
      }

      LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                    "deleteByIds - Deleted " +
                                        std::to_string(ids.size()) +
                                        " alarm occurrences");
      return static_cast<int>(ids.size());
    }

    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "deleteByIds failed: " + std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// AlarmOccurrence 전용 메서드들 (동일)
// =============================================================================

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findActive(std::optional<int> tenant_id) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::AlarmOccurrence::FIND_ACTIVE;

    if (tenant_id.has_value()) {
      size_t order_pos = query.find("ORDER BY");
      std::string tenant_clause =
          "AND tenant_id = " + std::to_string(tenant_id.value()) + " ";

      if (order_pos != std::string::npos) {
        query.insert(order_pos, tenant_clause);
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findActive - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                  "findActive - Found " +
                                      std::to_string(entities.size()) +
                                      " active alarm occurrences");
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findActive failed: " + std::string(e.what()));
    return {};
  }
}

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findByRuleId(int rule_id, bool active_only) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::AlarmOccurrence::FIND_BY_RULE_ID;
    query = RepositoryHelpers::replaceParameter(query, std::to_string(rule_id));

    if (active_only) {
      size_t order_pos = query.find("ORDER BY");
      std::string active_clause = "AND state = 'active' ";

      if (order_pos != std::string::npos) {
        query.insert(order_pos, active_clause);
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findByRuleId - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::INFO,
        "findByRuleId - Found " + std::to_string(entities.size()) +
            " alarm occurrences for rule: " + std::to_string(rule_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findByRuleId failed: " + std::string(e.what()));
    return {};
  }
}

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findByTenant(int tenant_id,
                                        const std::string &state_filter) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::AlarmOccurrence::FIND_ALL;

    std::string where_clause = "WHERE tenant_id = " + std::to_string(tenant_id);

    if (!state_filter.empty()) {
      where_clause += " AND state = '" + state_filter + "'";
    }
    where_clause += " ";

    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, where_clause);
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findByTenant - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::INFO,
        "findByTenant - Found " + std::to_string(entities.size()) +
            " alarm occurrences for tenant: " + std::to_string(tenant_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findByTenant failed: " + std::string(e.what()));
    return {};
  }
}

std::vector<AlarmOccurrenceEntity> AlarmOccurrenceRepository::findByTimeRange(
    const std::chrono::system_clock::time_point &start_time,
    const std::chrono::system_clock::time_point &end_time,
    std::optional<int> tenant_id) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::AlarmOccurrence::FIND_ALL;

    std::string start_str = timePointToString(start_time);
    std::string end_str = timePointToString(end_time);

    std::string where_clause = "WHERE occurrence_time >= '" + start_str +
                               "' AND occurrence_time <= '" + end_str + "'";

    if (tenant_id.has_value()) {
      where_clause += " AND tenant_id = " + std::to_string(tenant_id.value());
    }
    where_clause += " ";

    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, where_clause);
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findByTimeRange - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmOccurrenceRepository", LogLevel::INFO,
                                  "findByTimeRange - Found " +
                                      std::to_string(entities.size()) +
                                      " alarm occurrences in time range");
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findByTimeRange failed: " + std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 알람 상태 관리 메서드들 (동일)
// =============================================================================

bool AlarmOccurrenceRepository::acknowledge(int64_t occurrence_id,
                                            int acknowledged_by,
                                            const std::string &comment) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::string query = SQL::AlarmOccurrence::ACKNOWLEDGE;
    query = RepositoryHelpers::replaceParameter(
        query, std::to_string(acknowledged_by));
    query = RepositoryHelpers::replaceParameter(query, escapeString(comment));
    query = RepositoryHelpers::replaceParameter(query,
                                                std::to_string(occurrence_id));

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(static_cast<int>(occurrence_id));
      }

      LogManager::getInstance().log(
          "AlarmOccurrenceRepository", LogLevel::INFO,
          "acknowledge - Acknowledged alarm occurrence ID: " +
              std::to_string(occurrence_id));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "acknowledge failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmOccurrenceRepository::clear(int64_t occurrence_id, int cleared_by,
                                      const std::string &cleared_value,
                                      const std::string &comment) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // SQL::AlarmOccurrence::CLEAR 쿼리 사용
    // UPDATE alarm_occurrences SET state = 'cleared', cleared_time =
    // CURRENT_TIMESTAMP, cleared_value = ?, clear_comment = ?, cleared_by = ?,
    // updated_at = CURRENT_TIMESTAMP WHERE id = ?
    std::string query = SQL::AlarmOccurrence::CLEAR;
    query =
        RepositoryHelpers::replaceParameter(query, escapeString(cleared_value));
    query = RepositoryHelpers::replaceParameter(query, escapeString(comment));
    query =
        RepositoryHelpers::replaceParameter(query, std::to_string(cleared_by));
    query = RepositoryHelpers::replaceParameter(query,
                                                std::to_string(occurrence_id));

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(static_cast<int>(occurrence_id));
      }

      LogManager::getInstance().log(
          "AlarmOccurrenceRepository", LogLevel::INFO,
          "clear - Cleared alarm occurrence ID: " +
              std::to_string(occurrence_id) +
              " by user: " + std::to_string(cleared_by));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "clear failed: " + std::string(e.what()));
    return false;
  }
}

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findClearedByUser(int user_id) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::string query = R"(
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at,
                device_id, point_id, category, tags
            FROM alarm_occurrences 
            WHERE cleared_by = ?
            ORDER BY cleared_time DESC
        )";

    query = RepositoryHelpers::replaceParameter(query, std::to_string(user_id));
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findClearedByUser - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::INFO,
        "findClearedByUser - Found " + std::to_string(entities.size()) +
            " alarms cleared by user: " + std::to_string(user_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findClearedByUser failed: " + std::string(e.what()));
    return {};
  }
}

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findAcknowledgedByUser(int user_id) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::string query = R"(
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at,
                device_id, point_id, category, tags
            FROM alarm_occurrences 
            WHERE acknowledged_by = ?
            ORDER BY acknowledged_time DESC
        )";

    query = RepositoryHelpers::replaceParameter(query, std::to_string(user_id));
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findAcknowledgedByUser - Failed to map row: " +
                std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::INFO,
        "findAcknowledgedByUser - Found " + std::to_string(entities.size()) +
            " alarms acknowledged by user: " + std::to_string(user_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findAcknowledgedByUser failed: " + std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 통계 및 분석 메서드들 (동일)
// =============================================================================

std::map<std::string, int>
AlarmOccurrenceRepository::getAlarmStatistics(int tenant_id) {
  std::map<std::string, int> stats;

  try {
    if (!ensureTableExists()) {
      return stats;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::string query = R"(
            SELECT 
                COUNT(*) as total,
                COUNT(CASE WHEN state = 'active' THEN 1 END) as active,
                COUNT(CASE WHEN state = 'acknowledged' THEN 1 END) as acknowledged,
                COUNT(CASE WHEN state = 'cleared' THEN 1 END) as cleared
            FROM alarm_occurrences 
            WHERE tenant_id = )" +
                        std::to_string(tenant_id);

    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      const auto &row = results[0];
      stats["total"] = std::stoi(row.at("total"));
      stats["active"] = std::stoi(row.at("active"));
      stats["acknowledged"] = std::stoi(row.at("acknowledged"));
      stats["cleared"] = std::stoi(row.at("cleared"));
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::INFO,
        "getAlarmStatistics - Retrieved statistics for tenant: " +
            std::to_string(tenant_id));

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "getAlarmStatistics failed: " + std::string(e.what()));
  }

  return stats;
}

std::vector<AlarmOccurrenceEntity>
AlarmOccurrenceRepository::findActiveByRuleId(int rule_id) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::string query = R"(
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at,
                device_id, point_id, category, tags
            FROM alarm_occurrences 
            WHERE rule_id = ? AND state = 'active'
            ORDER BY occurrence_time DESC
        )";

    query = RepositoryHelpers::replaceParameter(query, std::to_string(rule_id));
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmOccurrenceEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "AlarmOccurrenceRepository", LogLevel::WARN,
            "findActiveByRuleId - Failed to map row: " + std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::DEBUG,
        "findActiveByRuleId - Found " + std::to_string(entities.size()) +
            " active alarms for rule: " + std::to_string(rule_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "findActiveByRuleId failed: " + std::string(e.what()));
    return {};
  }
}

int AlarmOccurrenceRepository::findMaxId() {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    auto results = db_layer.executeQuery(SQL::AlarmOccurrence::FIND_MAX_ID);

    if (!results.empty() && results[0].find("max_id") != results[0].end()) {
      const std::string &max_id_str = results[0].at("max_id");
      if (!max_id_str.empty() && max_id_str != "NULL") {
        return std::stoi(max_id_str);
      }
    }

    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmOccurrenceRepository",
                                  LogLevel::LOG_ERROR,
                                  "findMaxId failed: " + std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// 내부 헬퍼 메서드들 - device_id 정수형 처리
// =============================================================================

AlarmOccurrenceEntity AlarmOccurrenceRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  try {
    AlarmOccurrenceEntity entity;
    auto it = row.end();

    // 기본 정보
    it = row.find("id");
    if (it != row.end() && !it->second.empty()) {
      entity.setId(std::stoll(it->second));
    }

    it = row.find("rule_id");
    if (it != row.end() && !it->second.empty()) {
      entity.setRuleId(std::stoi(it->second));
    }

    it = row.find("tenant_id");
    if (it != row.end() && !it->second.empty()) {
      entity.setTenantId(std::stoi(it->second));
    }

    // 발생 시간
    it = row.find("occurrence_time");
    if (it != row.end() && !it->second.empty()) {
      entity.setOccurrenceTime(stringToTimePoint(it->second));
    }

    // 트리거 정보
    it = row.find("trigger_value");
    if (it != row.end()) {
      entity.setTriggerValue(it->second);
    }

    it = row.find("trigger_condition");
    if (it != row.end()) {
      entity.setTriggerCondition(it->second);
    }

    it = row.find("alarm_message");
    if (it != row.end()) {
      entity.setAlarmMessage(it->second);
    }

    // AlarmSeverity 변환
    it = row.find("severity");
    if (it != row.end() && !it->second.empty()) {
      entity.setSeverity(PulseOne::Alarm::stringToSeverity(it->second));
    }

    // AlarmState 변환
    it = row.find("state");
    if (it != row.end() && !it->second.empty()) {
      entity.setState(PulseOne::Alarm::stringToState(it->second));
    }

    // Acknowledge 정보
    it = row.find("acknowledged_time");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setAcknowledgedTime(stringToTimePoint(it->second));
    }

    it = row.find("acknowledged_by");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setAcknowledgedBy(std::stoi(it->second));
    }

    it = row.find("acknowledge_comment");
    if (it != row.end() && it->second != "NULL") {
      entity.setAcknowledgeComment(it->second);
    }

    // Clear 정보
    it = row.find("cleared_time");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setClearedTime(stringToTimePoint(it->second));
    }

    it = row.find("cleared_value");
    if (it != row.end() && it->second != "NULL") {
      entity.setClearedValue(it->second);
    }

    it = row.find("clear_comment");
    if (it != row.end() && it->second != "NULL") {
      entity.setClearComment(it->second);
    }

    // cleared_by 처리
    it = row.find("cleared_by");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setClearedBy(std::stoi(it->second));
    }

    // 알림 정보
    it = row.find("notification_sent");
    if (it != row.end() && !it->second.empty()) {
      entity.setNotificationSent(std::stoi(it->second) != 0);
    }

    it = row.find("notification_time");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setNotificationTime(stringToTimePoint(it->second));
    }

    it = row.find("notification_count");
    if (it != row.end() && !it->second.empty()) {
      entity.setNotificationCount(std::stoi(it->second));
    }

    it = row.find("notification_result");
    if (it != row.end() && it->second != "NULL") {
      entity.setNotificationResult(it->second);
    }

    // 컨텍스트 정보
    it = row.find("context_data");
    if (it != row.end() && it->second != "NULL") {
      entity.setContextData(it->second);
    }

    it = row.find("source_name");
    if (it != row.end() && it->second != "NULL") {
      entity.setSourceName(it->second);
    }

    it = row.find("location");
    if (it != row.end() && it->second != "NULL") {
      entity.setLocation(it->second);
    }

    // 시간 정보
    it = row.find("created_at");
    if (it != row.end() && !it->second.empty()) {
      entity.setCreatedAt(stringToTimePoint(it->second));
    }

    it = row.find("updated_at");
    if (it != row.end() && !it->second.empty()) {
      entity.setUpdatedAt(stringToTimePoint(it->second));
    }

    // 🔧 수정: device_id INTEGER 처리
    it = row.find("device_id");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setDeviceId(std::stoi(it->second));
    }

    it = row.find("point_id");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setPointId(std::stoi(it->second));
    }

    // 분류 시스템
    it = row.find("category");
    if (it != row.end() && it->second != "NULL") {
      entity.setCategory(it->second);
    }

    it = row.find("tags");
    if (it != row.end() && it->second != "NULL" && !it->second.empty()) {
      // JSON 배열 파싱
      std::vector<std::string> tags;
      std::string tags_str = it->second;

      if (tags_str.front() == '[' && tags_str.back() == ']') {
        tags_str = tags_str.substr(1, tags_str.length() - 2);

        std::stringstream ss(tags_str);
        std::string tag;

        while (std::getline(ss, tag, ',')) {
          if (tag.front() == '"' && tag.back() == '"') {
            tag = tag.substr(1, tag.length() - 2);
          }
          tags.push_back(tag);
        }
      }

      entity.setTags(tags);
    }

    return entity;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "mapRowToEntity failed: " + std::string(e.what()));
    throw;
  }
}

std::map<std::string, std::string>
AlarmOccurrenceRepository::entityToParams(const AlarmOccurrenceEntity &entity) {
  std::map<std::string, std::string> params;

  params["id"] = std::to_string(entity.getId());
  params["rule_id"] = std::to_string(entity.getRuleId());
  params["tenant_id"] = std::to_string(entity.getTenantId());

  // Time fields
  params["occurrence_time"] =
      escapeString(timePointToString(entity.getOccurrenceTime()));

  // String fields
  params["trigger_value"] = escapeString(entity.getTriggerValue());
  params["trigger_condition"] = escapeString(entity.getTriggerCondition());
  params["alarm_message"] = escapeString(entity.getAlarmMessage());
  params["severity"] = escapeString(entity.getSeverityString());
  params["state"] = escapeString(entity.getStateString());
  params["context_data"] = escapeString(entity.getContextData());
  params["source_name"] = escapeString(entity.getSourceName());
  params["location"] = escapeString(entity.getLocation());

  // Optional Time fields
  params["acknowledged_time"] = entity.getAcknowledgedTime().has_value()
                                    ? escapeString(timePointToString(
                                          entity.getAcknowledgedTime().value()))
                                    : "NULL";

  params["cleared_time"] =
      entity.getClearedTime().has_value()
          ? escapeString(timePointToString(entity.getClearedTime().value()))
          : "NULL";

  // Integer fields
  params["device_id"] = entity.getDeviceId().has_value()
                            ? std::to_string(entity.getDeviceId().value())
                            : "NULL";
  params["point_id"] = entity.getPointId().has_value()
                           ? std::to_string(entity.getPointId().value())
                           : "NULL";
  params["cleared_by"] = entity.getClearedBy().has_value()
                             ? std::to_string(entity.getClearedBy().value())
                             : "NULL";

  // Optional String fields
  params["category"] = entity.getCategory().has_value()
                           ? escapeString(entity.getCategory().value())
                           : "NULL";

  // tags JSON 변환
  if (!entity.getTags().empty()) {
    std::ostringstream tags_ss;
    tags_ss << "[";
    for (size_t i = 0; i < entity.getTags().size(); ++i) {
      if (i > 0)
        tags_ss << ",";
      tags_ss << "\"" << entity.getTags()[i] << "\"";
    }
    tags_ss << "]";
    params["tags"] = escapeString(tags_ss.str());
  } else {
    params["tags"] = "NULL";
  }

  return params;
}

bool AlarmOccurrenceRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    std::string check_query = "SELECT name FROM sqlite_master WHERE "
                              "type='table' AND name='alarm_occurrences'";
    auto results = db_layer.executeQuery(check_query);

    if (!results.empty()) {
      return true;
    }

    return db_layer.executeNonQuery(SQL::AlarmOccurrence::CREATE_TABLE);
  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "AlarmOccurrenceRepository", LogLevel::LOG_ERROR,
        "ensureTableExists failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmOccurrenceRepository::validateAlarmOccurrence(
    const AlarmOccurrenceEntity &entity) {
  if (entity.getRuleId() <= 0) {
    return false;
  }

  if (entity.getTenantId() <= 0) {
    return false;
  }

  if (entity.getAlarmMessage().empty()) {
    return false;
  }

  return true;
}

std::string AlarmOccurrenceRepository::escapeString(const std::string &str) {
  std::string escaped = str;

  size_t pos = 0;
  while ((pos = escaped.find("'", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "''");
    pos += 2;
  }

  return "'" + escaped + "'";
}

std::string AlarmOccurrenceRepository::timePointToString(
    const std::chrono::system_clock::time_point &time_point) const {
  auto time_t = std::chrono::system_clock::to_time_t(time_point);

  std::ostringstream oss;
  // Use localtime for KST since TZ=Asia/Seoul is set in container
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

  return oss.str();
}

std::chrono::system_clock::time_point
AlarmOccurrenceRepository::stringToTimePoint(
    const std::string &time_str) const {
  std::tm tm = {};
  std::istringstream ss(time_str);

  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

  auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  if (ss.peek() == '.') {
    ss.ignore(1);
    int ms;
    ss >> ms;
    time_point += std::chrono::milliseconds(ms);
  }

  return time_point;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne