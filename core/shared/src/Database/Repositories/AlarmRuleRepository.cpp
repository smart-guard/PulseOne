// =============================================================================
// collector/src/Database/Repositories/AlarmRuleRepository.cpp
// PulseOne AlarmRuleRepository 구현 - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmRuleRepository.cpp
 * @brief PulseOne AlarmRuleRepository - ScriptLibraryRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 *
 * 🎯 ScriptLibraryRepository 패턴 100% 적용:
 * - ExtendedSQLQueries.h 사용 (분리된 쿼리 파일)
 * - DbLib::DatabaseAbstractionLayer 패턴
 * - 표준 LogManager 사용법
 * - 벌크 연산 SQL 최적화
 * - 캐시 관리 완전 구현
 * - 모든 IRepository 메서드 override
 */

#include "Database/Repositories/AlarmRuleRepository.h"
#include "Alarm/AlarmTypes.h"
#include "Database/ExtendedSQLQueries.h" // 🔥 분리된 쿼리 파일 사용
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h" // 🔥 SQL::Common 네임스페이스용
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (ScriptLibraryRepository 패턴)
// =============================================================================

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                    "findAll - Table creation failed");
      return {};
    }
    LogManager::getInstance().setLogLevel(LogLevel::DEBUG);
    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h 사용
    auto results = db_layer.executeQuery(SQL::AlarmRule::FIND_ALL);

    std::vector<AlarmRuleEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "findAll - Failed to map row: " +
                                          std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmRuleRepository", LogLevel::INFO,
        "findAll - Found " + std::to_string(entities.size()) + " alarm rules");
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findAll failed: " + std::string(e.what()));
    return {};
  }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findById(int id) {
  try {
    // 캐시 확인
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                      "findById - Cache hit for ID: " +
                                          std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmRule::FIND_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                    "findById - No alarm rule found for ID: " +
                                        std::to_string(id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    // 캐시에 저장
    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                  "findById - Found alarm rule ID: " +
                                      std::to_string(id));
    return entity;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findById failed: " + std::string(e.what()));
    return std::nullopt;
  }
}

bool AlarmRuleRepository::save(AlarmRuleEntity &entity) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    if (!validateAlarmRule(entity)) {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                    "save - Invalid alarm rule data");
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
    auto params = entityToParams(entity);
    std::string query = RepositoryHelpers::replaceParametersInOrder(
        SQL::AlarmRule::INSERT, params);

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      // 새로 생성된 ID 조회
      auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
      if (!id_results.empty() &&
          id_results[0].find("id") != id_results[0].end()) {
        entity.setId(std::stoll(id_results[0].at("id")));
      }

      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "save - Saved alarm rule: " +
                                        entity.getName());
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "save failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmRuleRepository::update(const AlarmRuleEntity &entity) {
  try {
    if (entity.getId() <= 0 || !ensureTableExists()) {
      return false;
    }

    if (!validateAlarmRule(entity)) {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                    "update - Invalid alarm rule data");
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🔥 수정: UPDATE -> UPDATE_BY_ID (ExtendedSQLQueries.h 기준)
    auto params = entityToParams(entity);
    params["id"] = std::to_string(entity.getId()); // WHERE 절용
    std::string query = RepositoryHelpers::replaceParametersInOrder(
        SQL::AlarmRule::UPDATE, // 🔥 수정됨!
        params);

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      // 캐시 무효화
      if (isCacheEnabled()) {
        clearCacheForId(static_cast<int>(entity.getId()));
      }

      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "update - Updated alarm rule ID: " +
                                        std::to_string(entity.getId()));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "update failed: " + std::string(e.what()));
    return false;
  }
}

bool AlarmRuleRepository::deleteById(int id) {
  try {
    if (id <= 0 || !ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmRule::DELETE_BY_ID, std::to_string(id));
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      // 캐시 무효화
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }

      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "deleteById - Deleted alarm rule ID: " +
                                        std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "deleteById failed: " +
                                      std::string(e.what()));
    return false;
  }
}

bool AlarmRuleRepository::exists(int id) {
  try {
    if (id <= 0) {
      return false;
    }

    // 캐시 확인
    if (isCacheEnabled() && getCachedEntity(id).has_value()) {
      return true;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::AlarmRule::EXISTS_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    return !results.empty() && std::stoi(results[0].at("count")) > 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "exists failed: " + std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 벌크 연산 (SQL 최적화된 구현) - ScriptLibraryRepository 패턴
// =============================================================================

std::vector<AlarmRuleEntity>
AlarmRuleRepository::findByIds(const std::vector<int> &ids) {
  std::vector<AlarmRuleEntity> results;

  if (ids.empty()) {
    return results;
  }

  try {
    if (!ensureTableExists()) {
      return results;
    }

    // 🎯 SQL IN 절로 한 번에 조회 (성능 최적화)
    std::stringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ",";
      ids_ss << ids[i];
    }

    // 🎯 기본 쿼리에 WHERE 절 추가
    std::string query = SQL::AlarmRule::FIND_ALL;
    // ORDER BY 앞에 WHERE 절 삽입
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
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "findByIds - Failed to map row: " +
                                          std::string(e.what()));
      }
    }

    LogManager::getInstance().log(
        "AlarmRuleRepository", LogLevel::DEBUG,
        "findByIds - Found " + std::to_string(results.size()) +
            " alarm rules for " + std::to_string(ids.size()) + " IDs");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByIds failed: " + std::string(e.what()));
  }

  return results;
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  std::vector<AlarmRuleEntity> results;

  try {
    if (!ensureTableExists()) {
      return results;
    }

    // 🎯 RepositoryHelpers를 사용한 동적 쿼리 구성
    std::string query = SQL::AlarmRule::FIND_ALL;
    std::string where_clause = RepositoryHelpers::buildWhereClause(conditions);

    // ORDER BY 앞에 WHERE 절 삽입
    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, where_clause + " ");
    } else {
      query += where_clause;
    }

    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto query_results = db_layer.executeQuery(query);

    results.reserve(query_results.size());

    for (const auto &row : query_results) {
      try {
        results.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "findByConditions - Failed to map row: " +
                                          std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                  "findByConditions - Found " +
                                      std::to_string(results.size()) +
                                      " alarm rules");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByConditions failed: " +
                                      std::string(e.what()));
  }

  return results;
}

int AlarmRuleRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    // 🎯 ExtendedSQLQueries.h 상수 + RepositoryHelpers 패턴
    std::string query = SQL::AlarmRule::COUNT_ALL;
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "countByConditions failed: " +
                                      std::string(e.what()));
    return 0;
  }
}

std::optional<AlarmRuleEntity> AlarmRuleRepository::findFirstByConditions(
    const std::vector<QueryCondition> &conditions) {

  // 🎯 첫 번째 결과만 필요하므로 LIMIT 1 적용
  auto pagination = Pagination{0, 1}; // offset=0, limit=1
  auto results = findByConditions(conditions, std::nullopt, pagination);

  if (!results.empty()) {
    return results[0];
  }

  return std::nullopt;
}

int AlarmRuleRepository::saveBulk(std::vector<AlarmRuleEntity> &entities) {
  // 🔥 TODO: 실제 배치 INSERT 구현 필요 (성능 최적화)
  int saved_count = 0;
  for (auto &entity : entities) {
    if (save(entity)) {
      saved_count++;
    }
  }
  return saved_count;
}

int AlarmRuleRepository::updateBulk(
    const std::vector<AlarmRuleEntity> &entities) {
  // 🔥 TODO: 실제 배치 UPDATE 구현 필요 (성능 최적화)
  int updated_count = 0;
  for (const auto &entity : entities) {
    if (update(entity)) {
      updated_count++;
    }
  }
  return updated_count;
}

int AlarmRuleRepository::deleteByIds(const std::vector<int> &ids) {
  if (ids.empty()) {
    return 0;
  }

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    // 🎯 SQL IN 절로 한 번에 삭제 (성능 최적화)
    std::stringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ",";
      ids_ss << ids[i];
    }

    std::string query =
        "DELETE FROM alarm_rules WHERE id IN (" + ids_ss.str() + ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      // 캐시 무효화
      if (isCacheEnabled()) {
        for (int id : ids) {
          clearCacheForId(id);
        }
      }

      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                    "deleteByIds - Deleted " +
                                        std::to_string(ids.size()) +
                                        " alarm rules");
      return static_cast<int>(ids.size());
    }

    return 0;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "deleteByIds failed: " +
                                      std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// AlarmRule 전용 메서드들 - ExtendedSQLQueries.h 사용
// =============================================================================

std::vector<AlarmRuleEntity>
AlarmRuleRepository::findByTarget(const std::string &target_type, int target_id,
                                  bool enabled_only) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::AlarmRule::FIND_BY_TARGET;

    // 🎯 RepositoryHelpers로 파라미터 치환
    query = RepositoryHelpers::replaceParameter(query, "'" + target_type + "'");
    query =
        RepositoryHelpers::replaceParameter(query, std::to_string(target_id));

    if (!enabled_only) {
      // enabled 조건 제거
      size_t enabled_pos = query.find("AND is_enabled = 1");
      if (enabled_pos != std::string::npos) {
        query.erase(enabled_pos, 20); // "AND is_enabled = 1" 제거
      }
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<AlarmRuleEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "findByTarget - Failed to map row: " +
                                          std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                  "findByTarget - Found " +
                                      std::to_string(entities.size()) +
                                      " alarm rules for " + target_type + ":" +
                                      std::to_string(target_id));
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByTarget failed: " +
                                      std::string(e.what()));
    return {};
  }
}

std::vector<AlarmRuleEntity>
AlarmRuleRepository::findByTenant(int tenant_id, bool enabled_only) {
  try {
    std::vector<QueryCondition> conditions = {
        {"tenant_id", "=", std::to_string(tenant_id)}};

    if (enabled_only) {
      conditions.push_back({"is_enabled", "=", "1"});
    }

    return findByConditions(conditions);

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByTenant failed: " +
                                      std::string(e.what()));
    return {};
  }
}

std::vector<AlarmRuleEntity>
AlarmRuleRepository::findBySeverity(const std::string &severity,
                                    bool enabled_only) {
  try {
    std::vector<QueryCondition> conditions = {{"severity", "=", severity}};

    if (enabled_only) {
      conditions.push_back({"is_enabled", "=", "1"});
    }

    return findByConditions(conditions);

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findBySeverity failed: " +
                                      std::string(e.what()));
    return {};
  }
}

std::vector<AlarmRuleEntity>
AlarmRuleRepository::findByAlarmType(const std::string &alarm_type,
                                     bool enabled_only) {
  try {
    std::vector<QueryCondition> conditions = {{"alarm_type", "=", alarm_type}};

    if (enabled_only) {
      conditions.push_back({"is_enabled", "=", "1"});
    }

    return findByConditions(conditions);

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByAlarmType failed: " +
                                      std::string(e.what()));
    return {};
  }
}

std::vector<AlarmRuleEntity> AlarmRuleRepository::findAllEnabled() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 ExtendedSQLQueries.h 사용
    auto results = db_layer.executeQuery(SQL::AlarmRule::FIND_ENABLED);

    std::vector<AlarmRuleEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "findAllEnabled - Failed to map row: " +
                                          std::string(e.what()));
      }
    }

    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::INFO,
                                  "findAllEnabled - Found " +
                                      std::to_string(entities.size()) +
                                      " enabled alarm rules");
    return entities;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findAllEnabled failed: " +
                                      std::string(e.what()));
    return {};
  }
}

std::optional<AlarmRuleEntity>
AlarmRuleRepository::findByName(const std::string &name, int tenant_id,
                                int exclude_id) {
  try {
    std::vector<QueryCondition> conditions = {
        {"name", "=", name}, {"tenant_id", "=", std::to_string(tenant_id)}};

    if (exclude_id > 0) {
      conditions.push_back({"id", "!=", std::to_string(exclude_id)});
    }

    return findFirstByConditions(conditions);

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "findByName failed: " +
                                      std::string(e.what()));
    return std::nullopt;
  }
}

bool AlarmRuleRepository::isNameTaken(const std::string &name, int tenant_id,
                                      int exclude_id) {
  try {
    auto found = findByName(name, tenant_id, exclude_id);
    return found.has_value();
  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "isNameTaken failed: " +
                                      std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 내부 헬퍼 메서드들 - AlarmTypes.h 올바른 사용
// =============================================================================

AlarmRuleEntity AlarmRuleRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  AlarmRuleEntity entity;

  try {
    // 🔥 디버그 로그
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                  "mapRowToEntity - Processing " +
                                      std::to_string(row.size()) + " columns");

    // 🔥 헬퍼 함수
    auto getValue = [&row](const std::string &key) -> std::string {
      auto it = row.find(key);
      return (it != row.end()) ? it->second : "";
    };

    auto safeStringToInt = [this](const std::string &str,
                                  const std::string &field) -> int {
      if (str.empty() || str == "NULL")
        return 0;
      try {
        return std::stoi(str);
      } catch (...) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "Failed to parse " + field + ": '" + str +
                                          "'");
        return 0;
      }
    };

    auto safeStringToDouble = [this](const std::string &str,
                                     const std::string &field) -> double {
      if (str.empty() || str == "NULL")
        return 0.0;
      try {
        return std::stod(str);
      } catch (...) {
        LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                      "Failed to parse " + field + ": '" + str +
                                          "'");
        return 0.0;
      }
    };

    auto safeStringToBool = [](const std::string &str) -> bool {
      return (str == "1" || str == "true" || str == "TRUE");
    };

    // 🔥 1. 기본 필드들
    entity.setId(safeStringToInt(getValue("id"), "id"));
    entity.setTenantId(safeStringToInt(getValue("tenant_id"), "tenant_id"));
    entity.setName(getValue("name"));
    entity.setDescription(getValue("description"));

    // 🔥 2. TargetType enum 변환 (올바른 방법)
    std::string target_type_str = getValue("target_type");
    if (target_type_str == "data_point") {
      entity.setTargetType(PulseOne::Alarm::TargetType::DATA_POINT);
    } else if (target_type_str == "virtual_point") {
      entity.setTargetType(PulseOne::Alarm::TargetType::VIRTUAL_POINT);
    } else if (target_type_str == "group") {
      entity.setTargetType(PulseOne::Alarm::TargetType::GROUP);
    } else {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                    "Unknown target_type: '" + target_type_str +
                                        "', using DATA_POINT");
      entity.setTargetType(PulseOne::Alarm::TargetType::DATA_POINT);
    }

    // 🔥 3. target_id (optional 처리)
    std::string target_id_str = getValue("target_id");
    if (!target_id_str.empty() && target_id_str != "NULL") {
      entity.setTargetId(safeStringToInt(target_id_str, "target_id"));
    }

    // 🔥 4. AlarmType enum 변환
    std::string alarm_type_str = getValue("alarm_type");
    if (alarm_type_str == "analog") {
      entity.setAlarmType(PulseOne::Alarm::AlarmType::ANALOG);
    } else if (alarm_type_str == "digital") {
      entity.setAlarmType(PulseOne::Alarm::AlarmType::DIGITAL);
    } else if (alarm_type_str == "script") {
      entity.setAlarmType(PulseOne::Alarm::AlarmType::SCRIPT);
    } else {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                    "Unknown alarm_type: '" + alarm_type_str +
                                        "', using ANALOG");
      entity.setAlarmType(PulseOne::Alarm::AlarmType::ANALOG);
    }

    // 🔥 5. 임계값들 (optional 처리) - Extended Mapping Support
    std::string hh_limit_str = getValue("high_high_limit");
    if (!hh_limit_str.empty() && hh_limit_str != "NULL") {
      entity.setHighHighLimit(
          safeStringToDouble(hh_limit_str, "high_high_limit"));
    }

    std::string high_limit_str = getValue("high_limit");
    if (!high_limit_str.empty() && high_limit_str != "NULL") {
      entity.setHighLimit(safeStringToDouble(high_limit_str, "high_limit"));
    }

    std::string low_limit_str = getValue("low_limit");
    if (!low_limit_str.empty() && low_limit_str != "NULL") {
      entity.setLowLimit(safeStringToDouble(low_limit_str, "low_limit"));
    }

    std::string ll_limit_str = getValue("low_low_limit");
    if (!ll_limit_str.empty() && ll_limit_str != "NULL") {
      entity.setLowLowLimit(safeStringToDouble(ll_limit_str, "low_low_limit"));
    }

    // 💥 Legacy Compatibility: condition + threshold 필드를 새 Limit 필드에
    // 매핑
    std::string condition_legacy = getValue("condition");
    std::string threshold_str = getValue("threshold");
    if (!threshold_str.empty() && threshold_str != "NULL") {
      double threshold_val = safeStringToDouble(threshold_str, "threshold");
      if (condition_legacy == "low" && !entity.getLowLimit().has_value()) {
        entity.setLowLimit(threshold_val);
      } else if (condition_legacy == "high" &&
                 !entity.getHighLimit().has_value()) {
        entity.setHighLimit(threshold_val);
      } else if (condition_legacy == "low_low" &&
                 !entity.getLowLowLimit().has_value()) {
        entity.setLowLowLimit(threshold_val);
      } else if (condition_legacy == "high_high" &&
                 !entity.getHighHighLimit().has_value()) {
        entity.setHighHighLimit(threshold_val);
      }
    }

    std::string deadband_str = getValue("deadband");
    if (!deadband_str.empty() && deadband_str != "NULL") {
      entity.setDeadband(safeStringToDouble(deadband_str, "deadband"));
    }

    // 🔥 6. AlarmSeverity enum 변환 (올바른 네임스페이스)
    std::string severity_str = getValue("severity");
    if (severity_str == "critical") {
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::CRITICAL);
    } else if (severity_str == "high") {
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::HIGH);
    } else if (severity_str == "medium") {
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::MEDIUM);
    } else if (severity_str == "low") {
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::LOW);
    } else if (severity_str == "info") {
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::INFO);
    } else {
      LogManager::getInstance().log("AlarmRuleRepository", LogLevel::WARN,
                                    "Unknown severity: '" + severity_str +
                                        "', using MEDIUM");
      entity.setSeverity(PulseOne::Alarm::AlarmSeverity::MEDIUM);
    }

    // 🔥 7. DigitalTrigger enum 변환
    std::string trigger_str = getValue("trigger_condition");
    if (trigger_str == "on_true") {
      entity.setTriggerCondition(PulseOne::Alarm::DigitalTrigger::ON_TRUE);
    } else if (trigger_str == "on_false") {
      entity.setTriggerCondition(PulseOne::Alarm::DigitalTrigger::ON_FALSE);
    } else if (trigger_str == "on_change") {
      entity.setTriggerCondition(PulseOne::Alarm::DigitalTrigger::ON_CHANGE);
    } else {
      entity.setTriggerCondition(PulseOne::Alarm::DigitalTrigger::ON_CHANGE);
    }

    // 🔥 8. Boolean 필드들
    entity.setEnabled(safeStringToBool(getValue("is_enabled")));
    entity.setAutoClear(safeStringToBool(getValue("auto_clear")));
    entity.setNotificationEnabled(
        safeStringToBool(getValue("notification_enabled")));

    // 🔥 9. 기타 필드들
    entity.setPriority(safeStringToInt(getValue("priority"), "priority"));
    entity.setMessageTemplate(getValue("message_template"));
    entity.setConditionScript(getValue("condition_script"));
    entity.setMessageScript(getValue("message_script"));

    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::DEBUG,
                                  "Successfully mapped alarm rule: " +
                                      entity.getName());
    return entity;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "mapRowToEntity failed: " +
                                      std::string(e.what()));

    // 🔥 최소 동작 가능한 기본 엔티티 반환
    AlarmRuleEntity fallback_entity;
    fallback_entity.setId(0);
    fallback_entity.setName("ERROR_PARSING_ALARM");
    fallback_entity.setTenantId(1);
    fallback_entity.setTargetType(
        PulseOne::Alarm::TargetType::DATA_POINT); // 올바른 enum 사용
    fallback_entity.setAlarmType(
        PulseOne::Alarm::AlarmType::ANALOG); // 올바른 enum 사용
    fallback_entity.setSeverity(
        PulseOne::Alarm::AlarmSeverity::MEDIUM); // 올바른 enum 사용
    fallback_entity.setEnabled(false);

    return fallback_entity;
  }
}

std::map<std::string, std::string>
PulseOne::Database::Repositories::AlarmRuleRepository::entityToParams(
    const AlarmRuleEntity &entity) {

  std::map<std::string, std::string> params;

  // 기본 정보
  params["tenant_id"] = std::to_string(entity.getTenantId());
  params["name"] = escapeString(entity.getName());
  params["description"] = escapeString(entity.getDescription());

  // 대상 정보 - 🔥 AlarmTypes.h 변환 함수 사용
  params["target_type"] =
      escapeString(PulseOne::Alarm::targetTypeToString(entity.getTargetType()));

  // 🔥 std::optional 올바른 처리
  if (entity.getTargetId().has_value()) {
    params["target_id"] = std::to_string(entity.getTargetId().value());
  } else {
    params["target_id"] = "NULL";
  }

  params["target_group"] = escapeString(entity.getTargetGroup());

  // 알람 타입 - 🔥 AlarmTypes.h 변환 함수 사용
  params["alarm_type"] =
      escapeString(PulseOne::Alarm::alarmTypeToString(entity.getAlarmType()));

  // 아날로그 설정 - 🔥 std::optional 올바른 처리
  if (entity.getHighHighLimit().has_value()) {
    params["high_high_limit"] =
        std::to_string(entity.getHighHighLimit().value());
  } else {
    params["high_high_limit"] = "NULL";
  }

  if (entity.getHighLimit().has_value()) {
    params["high_limit"] = std::to_string(entity.getHighLimit().value());
  } else {
    params["high_limit"] = "NULL";
  }

  if (entity.getLowLimit().has_value()) {
    params["low_limit"] = std::to_string(entity.getLowLimit().value());
  } else {
    params["low_limit"] = "NULL";
  }

  if (entity.getLowLowLimit().has_value()) {
    params["low_low_limit"] = std::to_string(entity.getLowLowLimit().value());
  } else {
    params["low_low_limit"] = "NULL";
  }

  params["deadband"] = std::to_string(entity.getDeadband());
  params["rate_of_change"] = std::to_string(entity.getRateOfChange());

  // 디지털 설정 - 🔥 AlarmTypes.h 변환 함수 사용
  params["trigger_condition"] = escapeString(
      PulseOne::Alarm::digitalTriggerToString(entity.getTriggerCondition()));

  // 스크립트 설정 - 🔥 올바른 메서드명 사용
  params["condition_script"] = escapeString(entity.getConditionScript());
  params["message_script"] = escapeString(entity.getMessageScript());

  // 메시지 설정
  params["message_config"] = escapeString(entity.getMessageConfig());
  params["message_template"] = escapeString(entity.getMessageTemplate());

  // 우선순위 - 🔥 AlarmTypes.h 변환 함수 사용
  params["severity"] =
      escapeString(PulseOne::Alarm::severityToString(entity.getSeverity()));
  params["priority"] = std::to_string(entity.getPriority());

  // 자동 처리
  params["auto_acknowledge"] = entity.isAutoAcknowledge() ? "1" : "0";
  params["acknowledge_timeout_min"] =
      std::to_string(entity.getAcknowledgeTimeoutMin());
  params["auto_clear"] = entity.isAutoClear() ? "1" : "0";

  // 억제 규칙
  params["suppression_rules"] = escapeString(entity.getSuppressionRules());

  // 알림 설정
  params["notification_enabled"] = entity.isNotificationEnabled() ? "1" : "0";
  params["notification_delay_sec"] =
      std::to_string(entity.getNotificationDelaySec());
  params["notification_repeat_interval_min"] =
      std::to_string(entity.getNotificationRepeatIntervalMin());
  params["notification_channels"] =
      escapeString(entity.getNotificationChannels());
  params["notification_recipients"] =
      escapeString(entity.getNotificationRecipients());

  // 상태
  params["is_enabled"] = entity.isEnabled() ? "1" : "0";
  params["is_latched"] = entity.isLatched() ? "1" : "0";
  params["created_by"] = std::to_string(entity.getCreatedBy());

  // 템플릿 정보
  params["template_id"] = entity.getTemplateId().has_value()
                              ? std::to_string(entity.getTemplateId().value())
                              : "NULL";
  params["rule_group"] = escapeString(entity.getRuleGroup());
  params["created_by_template"] = entity.isCreatedByTemplate() ? "1" : "0";

  // 에스컬레이션 설정
  params["escalation_enabled"] = entity.isEscalationEnabled() ? "1" : "0";
  params["escalation_max_level"] =
      std::to_string(entity.getEscalationMaxLevel());
  params["escalation_rules"] = escapeString(entity.getEscalationRules());

  // 분류 및 태그
  params["category"] = escapeString(entity.getCategory());
  params["tags"] = escapeString(entity.getTags());

  return params;
}

bool AlarmRuleRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    // 🔥 ExtendedSQLQueries.h 사용
    bool success = db_layer.executeCreateTable(SQL::AlarmRule::CREATE_TABLE);
    if (success) {
      LogManager::getInstance().log(
          "AlarmRuleRepository", LogLevel::DEBUG,
          "ensureTableExists - Table creation/check completed");
    } else {
      LogManager::getInstance().log(
          "AlarmRuleRepository", LogLevel::LOG_ERROR,
          "ensureTableExists - Table creation failed");
    }
    return success;
  } catch (const std::exception &e) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "ensureTableExists failed: " +
                                      std::string(e.what()));
    return false;
  }
}

bool AlarmRuleRepository::validateAlarmRule(const AlarmRuleEntity &entity) {
  // 🔥 수정: UNKNOWN 체크 제거, 기본값으로 변경
  if (entity.getName().empty()) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "validateAlarmRule - Name cannot be empty");
    return false;
  }

  // 🔥 수정: TargetType 유효성 검사 개선
  auto target_type = entity.getTargetType();
  if (target_type != PulseOne::Alarm::TargetType::DATA_POINT &&
      target_type != PulseOne::Alarm::TargetType::VIRTUAL_POINT &&
      target_type != PulseOne::Alarm::TargetType::GROUP) {
    LogManager::getInstance().log("AlarmRuleRepository", LogLevel::LOG_ERROR,
                                  "validateAlarmRule - Invalid target type");
    return false;
  }

  // target_id 검증 (DATA_POINT, VIRTUAL_POINT의 경우 필수)
  if ((target_type == PulseOne::Alarm::TargetType::DATA_POINT ||
       target_type == PulseOne::Alarm::TargetType::VIRTUAL_POINT) &&
      (!entity.getTargetId().has_value() ||
       entity.getTargetId().value() <= 0)) {
    LogManager::getInstance().log(
        "AlarmRuleRepository", LogLevel::LOG_ERROR,
        "validateAlarmRule - Target ID required for DATA_POINT/VIRTUAL_POINT");
    return false;
  }

  // GROUP의 경우 target_group 필수
  if (target_type == PulseOne::Alarm::TargetType::GROUP &&
      entity.getTargetGroup().empty()) {
    LogManager::getInstance().log(
        "AlarmRuleRepository", LogLevel::LOG_ERROR,
        "validateAlarmRule - Target group required for GROUP type");
    return false;
  }

  // 알람 타입별 유효성 검사
  auto alarm_type = entity.getAlarmType();
  if (alarm_type == PulseOne::Alarm::AlarmType::ANALOG) {
    // 아날로그 알람은 최소 하나의 임계값 필요
    if (!entity.getHighHighLimit().has_value() &&
        !entity.getHighLimit().has_value() &&
        !entity.getLowLimit().has_value() &&
        !entity.getLowLowLimit().has_value()) {
      LogManager::getInstance().log(
          "AlarmRuleRepository", LogLevel::LOG_ERROR,
          "validateAlarmRule - Analog alarm requires at least one threshold");
      return false;
    }
  }

  if (alarm_type == PulseOne::Alarm::AlarmType::SCRIPT) {
    // 스크립트 알람은 condition_script 필수
    if (entity.getConditionScript().empty()) {
      LogManager::getInstance().log(
          "AlarmRuleRepository", LogLevel::LOG_ERROR,
          "validateAlarmRule - Script alarm requires condition script");
      return false;
    }
  }

  return true;
}

std::string AlarmRuleRepository::escapeString(const std::string &str) {
  std::string escaped = str;

  // 작은따옴표 이스케이프
  size_t pos = 0;
  while ((pos = escaped.find("'", pos)) != std::string::npos) {
    escaped.replace(pos, 1, "''");
    pos += 2;
  }

  return "'" + escaped + "'";
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne