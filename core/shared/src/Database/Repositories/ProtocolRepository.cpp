/**
 * @file ProtocolRepository.cpp
 * @brief PulseOne ProtocolRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-26
 *
 * DeviceRepository와 동일한 패턴:
 * - SQLQueries.h 상수 사용
 * - DbLib::DatabaseAbstractionLayer 패턴
 * - 캐싱 및 벌크 연산 지원
 * - 검증 및 에러 핸들링
 */

#include "Database/Repositories/ProtocolRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<ProtocolEntity> ProtocolRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error("ProtocolRepository::findAll - Table creation failed");
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_ALL);

    std::vector<ProtocolEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("ProtocolRepository::findAll - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("ProtocolRepository::findAll - Found " +
                  std::to_string(entities.size()) + " protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::optional<ProtocolEntity> ProtocolRepository::findById(int id) {
  try {
    // 캐시 확인 먼저
    if (isCacheEnabled()) {
      if (auto cached = getCachedEntity(id)) {
        logger_->Debug("ProtocolRepository::findById - Cache hit for ID: " +
                       std::to_string(id));
        return cached;
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::Protocol::FIND_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      auto entity = mapRowToEntity(results[0]);

      // 캐시에 저장
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Debug("ProtocolRepository::findById - Found protocol: " +
                     entity.getDisplayName());
      return entity;
    }

    logger_->Debug(
        "ProtocolRepository::findById - Protocol not found with ID: " +
        std::to_string(id));
    return std::nullopt;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findById failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

bool ProtocolRepository::save(ProtocolEntity &entity) {
  try {
    if (!validateProtocol(entity)) {
      logger_->Error("ProtocolRepository::save - Invalid protocol: " +
                     entity.getDisplayName());
      return false;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::map<std::string, std::string> data = entityToParams(entity);
    std::vector<std::string> primary_keys = {"id"};

    bool success = db_layer.executeUpsert("protocols", data, primary_keys);

    if (success) {
      // 새로 생성된 경우 ID 조회
      if (entity.getId() <= 0) {
        auto id_result =
            db_layer.executeQuery(SQL::Protocol::GET_LAST_INSERT_ID);
        if (!id_result.empty()) {
          entity.setId(std::stoi(id_result[0].at("id")));
        }
      }

      // 캐시 업데이트
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Info("ProtocolRepository::save - Saved protocol: " +
                    entity.getDisplayName() +
                    " (type: " + entity.getProtocolType() + ")");
    } else {
      logger_->Error("ProtocolRepository::save - Failed to save protocol: " +
                     entity.getDisplayName());
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::save failed: " + std::string(e.what()));
    return false;
  }
}

bool ProtocolRepository::update(const ProtocolEntity &entity) {
  ProtocolEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool ProtocolRepository::deleteById(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::Protocol::DELETE_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }
      logger_->Info("ProtocolRepository::deleteById - Deleted protocol ID: " +
                    std::to_string(id));
    } else {
      logger_->Error(
          "ProtocolRepository::deleteById - Failed to delete protocol ID: " +
          std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool ProtocolRepository::exists(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::Protocol::EXISTS_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::exists failed: " +
                   std::string(e.what()));
    return false;
  }
}

std::vector<ProtocolEntity>
ProtocolRepository::findByIds(const std::vector<int> &ids) {
  try {
    if (ids.empty()) {
      return {};
    }

    if (!ensureTableExists()) {
      return {};
    }

    // IN 절 구성
    std::ostringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ", ";
      ids_ss << ids[i];
    }

    // 기본 쿼리에 IN 절 추가
    std::string query = SQL::Protocol::FIND_ALL;
    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ProtocolEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("ProtocolRepository::findByIds - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("ProtocolRepository::findByIds - Found " +
                  std::to_string(entities.size()) + " protocols for " +
                  std::to_string(ids.size()) + " IDs");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByIds failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = SQL::Protocol::FIND_ALL;

    // ORDER BY 제거 후 조건 추가
    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query = query.substr(0, order_pos);
    }

    query += RepositoryHelpers::buildWhereClause(conditions);
    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ProtocolEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "ProtocolRepository::findByConditions - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Debug("ProtocolRepository::findByConditions - Found " +
                   std::to_string(entities.size()) + " protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByConditions failed: " +
                   std::string(e.what()));
    return {};
  }
}

int ProtocolRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = SQL::Protocol::COUNT_ALL;
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::countByConditions failed: " +
                   std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// Protocol 전용 메서드들
// =============================================================================

std::optional<ProtocolEntity>
ProtocolRepository::findByType(const std::string &protocol_type) {
  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameterWithQuotes(
        SQL::Protocol::FIND_BY_TYPE, protocol_type);
    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      auto entity = mapRowToEntity(results[0]);

      // 캐시에 저장
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Debug("ProtocolRepository::findByType - Found protocol: " +
                     entity.getDisplayName());
      return entity;
    }

    logger_->Debug(
        "ProtocolRepository::findByType - Protocol not found with type: " +
        protocol_type);
    return std::nullopt;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByType failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

std::vector<ProtocolEntity>
ProtocolRepository::findByCategory(const std::string &category) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameterWithQuotes(
        SQL::Protocol::FIND_BY_CATEGORY, category);
    auto results = db_layer.executeQuery(query);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findByCategory - Found " +
                  std::to_string(entities.size()) +
                  " protocols for category: " + category);
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByCategory failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findEnabledProtocols() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_ENABLED);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findEnabledProtocols - Found " +
                  std::to_string(entities.size()) + " enabled protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findEnabledProtocols failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findActiveProtocols() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_ACTIVE);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findActiveProtocols - Found " +
                  std::to_string(entities.size()) + " active protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findActiveProtocols failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity>
ProtocolRepository::findByOperation(const std::string &operation) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    // JSON 필드에서 검색하는 쿼리 (SQLite JSON1 확장 사용)
    std::string query = R"(
            SELECT * FROM protocols 
            WHERE json_extract(supported_operations, '$') LIKE '%")" +
                        operation + R"("%'
            AND is_enabled = 1
            ORDER BY display_name
        )";

    auto results = db_layer.executeQuery(query);
    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findByOperation - Found " +
                  std::to_string(entities.size()) +
                  " protocols supporting operation: " + operation);
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByOperation failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity>
ProtocolRepository::findByDataType(const std::string &data_type) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    // JSON 필드에서 검색하는 쿼리
    std::string query = R"(
            SELECT * FROM protocols 
            WHERE json_extract(supported_data_types, '$') LIKE '%")" +
                        data_type + R"("%'
            AND is_enabled = 1
            ORDER BY display_name
        )";

    auto results = db_layer.executeQuery(query);
    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findByDataType - Found " +
                  std::to_string(entities.size()) +
                  " protocols supporting data type: " + data_type);
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByDataType failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findSerialProtocols() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_SERIAL);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findSerialProtocols - Found " +
                  std::to_string(entities.size()) + " serial protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findSerialProtocols failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findBrokerProtocols() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_BROKER_REQUIRED);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findBrokerProtocols - Found " +
                  std::to_string(entities.size()) + " broker protocols");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findBrokerProtocols failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<ProtocolEntity> ProtocolRepository::findByPort(int port) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::Protocol::FIND_BY_PORT, std::to_string(port));
    auto results = db_layer.executeQuery(query);

    std::vector<ProtocolEntity> entities = mapResultToEntities(results);

    logger_->Info("ProtocolRepository::findByPort - Found " +
                  std::to_string(entities.size()) +
                  " protocols for port: " + std::to_string(port));
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::findByPort failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::map<std::string, std::vector<ProtocolEntity>>
ProtocolRepository::groupByCategory() {
  std::map<std::string, std::vector<ProtocolEntity>> grouped;

  try {
    auto protocols = findAll();
    for (const auto &protocol : protocols) {
      std::string category = protocol.getCategory().empty()
                                 ? "uncategorized"
                                 : protocol.getCategory();
      grouped[category].push_back(protocol);
    }
  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::groupByCategory failed: " +
                   std::string(e.what()));
  }

  return grouped;
}

// =============================================================================
// 벌크 연산
// =============================================================================

int ProtocolRepository::saveBulk(std::vector<ProtocolEntity> &entities) {
  int saved_count = 0;
  for (auto &entity : entities) {
    if (save(entity)) {
      saved_count++;
    }
  }
  logger_->Info("ProtocolRepository::saveBulk - Saved " +
                std::to_string(saved_count) + " protocols");
  return saved_count;
}

int ProtocolRepository::updateBulk(
    const std::vector<ProtocolEntity> &entities) {
  int updated_count = 0;
  for (const auto &entity : entities) {
    if (update(entity)) {
      updated_count++;
    }
  }
  logger_->Info("ProtocolRepository::updateBulk - Updated " +
                std::to_string(updated_count) + " protocols");
  return updated_count;
}

int ProtocolRepository::deleteByIds(const std::vector<int> &ids) {
  int deleted_count = 0;
  for (int id : ids) {
    if (deleteById(id)) {
      deleted_count++;
    }
  }
  logger_->Info("ProtocolRepository::deleteByIds - Deleted " +
                std::to_string(deleted_count) + " protocols");
  return deleted_count;
}

// =============================================================================
// 통계 및 분석
// =============================================================================

std::string ProtocolRepository::getProtocolStatistics() const {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    auto total_result = db_layer.executeQuery(SQL::Protocol::COUNT_ALL);
    int total_count = 0;
    if (!total_result.empty() &&
        total_result[0].find("count") != total_result[0].end()) {
      total_count = std::stoi(total_result[0].at("count"));
    }

    auto enabled_result = db_layer.executeQuery(SQL::Protocol::COUNT_ENABLED);
    int enabled_count = 0;
    if (!enabled_result.empty() &&
        enabled_result[0].find("count") != enabled_result[0].end()) {
      enabled_count = std::stoi(enabled_result[0].at("count"));
    }

    std::ostringstream oss;
    oss << "{ \"total\": " << total_count << ", \"enabled\": " << enabled_count
        << ", \"disabled\": " << (total_count - enabled_count) << " }";
    return oss.str();

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ProtocolRepository::getProtocolStatistics failed: " +
                     std::string(e.what()));
    }
    return "{ \"error\": \"Statistics calculation failed\" }";
  }
}

std::map<std::string, int> ProtocolRepository::getCategoryDistribution() const {
  std::map<std::string, int> distribution;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    auto results =
        db_layer.executeQuery(SQL::Protocol::GET_CATEGORY_DISTRIBUTION);

    for (const auto &row : results) {
      if (row.find("category") != row.end() && row.find("count") != row.end()) {
        std::string category = row.at("category");
        if (category.empty())
          category = "uncategorized";
        distribution[category] = std::stoi(row.at("count"));
      }
    }

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ProtocolRepository::getCategoryDistribution failed: " +
                     std::string(e.what()));
    }
  }

  return distribution;
}

std::vector<ProtocolEntity>
ProtocolRepository::findDeprecatedProtocols() const {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::FIND_DEPRECATED);

    std::vector<ProtocolEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        ProtocolEntity entity;
        auto it = row.find("display_name");
        if (it != row.end()) {
          entity.setDisplayName(it->second);
        }

        it = row.find("id");
        if (it != row.end()) {
          entity.setId(std::stoi(it->second));
        }

        it = row.find("protocol_type");
        if (it != row.end()) {
          entity.setProtocolType(it->second);
        }

        entities.push_back(entity);
      } catch (const std::exception &e) {
        if (logger_) {
          logger_->Warn("ProtocolRepository::findDeprecatedProtocols - Failed "
                        "to map row: " +
                        std::string(e.what()));
        }
      }
    }

    return entities;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ProtocolRepository::findDeprecatedProtocols failed: " +
                     std::string(e.what()));
    }
    return {};
  }
}

std::vector<std::map<std::string, std::string>>
ProtocolRepository::getApiProtocolList() const {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::Protocol::GET_API_LIST);

    std::vector<std::map<std::string, std::string>> api_list;
    api_list.reserve(results.size());

    for (const auto &row : results) {
      std::map<std::string, std::string> api_item;
      api_item["value"] =
          row.find("protocol_type") != row.end() ? row.at("protocol_type") : "";
      api_item["name"] =
          row.find("display_name") != row.end() ? row.at("display_name") : "";
      api_item["description"] =
          row.find("description") != row.end() ? row.at("description") : "";
      api_list.push_back(api_item);
    }

    return api_list;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ProtocolRepository::getApiProtocolList failed: " +
                     std::string(e.what()));
    }
    return {};
  }
}

int ProtocolRepository::getTotalCount() { return countByConditions({}); }

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

ProtocolEntity ProtocolRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  ProtocolEntity entity;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    // 기본 정보
    auto it = row.find("id");
    if (it != row.end()) {
      entity.setId(std::stoi(it->second));
    }

    if ((it = row.find("protocol_type")) != row.end())
      entity.setProtocolType(it->second);
    if ((it = row.find("display_name")) != row.end())
      entity.setDisplayName(it->second);
    if ((it = row.find("description")) != row.end())
      entity.setDescription(it->second);

    // 네트워크 정보
    it = row.find("default_port");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setDefaultPort(std::stoi(it->second));
    }

    it = row.find("uses_serial");
    if (it != row.end()) {
      entity.setUsesSerial(db_layer.parseBoolean(it->second));
    }

    it = row.find("requires_broker");
    if (it != row.end()) {
      entity.setRequiresBroker(db_layer.parseBoolean(it->second));
    }

    // 지원 기능 (JSON 문자열)
    if ((it = row.find("supported_operations")) != row.end())
      entity.setSupportedOperations(it->second);
    if ((it = row.find("supported_data_types")) != row.end())
      entity.setSupportedDataTypes(it->second);
    if ((it = row.find("connection_params")) != row.end())
      entity.setConnectionParams(it->second);

    // 설정 정보
    it = row.find("default_polling_interval");
    if (it != row.end() && !it->second.empty()) {
      entity.setDefaultPollingInterval(std::stoi(it->second));
    }

    it = row.find("default_timeout");
    if (it != row.end() && !it->second.empty()) {
      entity.setDefaultTimeout(std::stoi(it->second));
    }

    it = row.find("max_concurrent_connections");
    if (it != row.end() && !it->second.empty()) {
      entity.setMaxConcurrentConnections(std::stoi(it->second));
    }

    // 상태 정보
    it = row.find("is_enabled");
    if (it != row.end()) {
      entity.setEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("is_deprecated");
    if (it != row.end()) {
      entity.setDeprecated(db_layer.parseBoolean(it->second));
    }

    if ((it = row.find("min_firmware_version")) != row.end())
      entity.setMinFirmwareVersion(it->second);

    // 분류 정보
    if ((it = row.find("category")) != row.end())
      entity.setCategory(it->second);
    if ((it = row.find("vendor")) != row.end())
      entity.setVendor(it->second);
    if ((it = row.find("standard_reference")) != row.end())
      entity.setStandardReference(it->second);

    // 타임스탬프는 기본값 사용
    entity.setCreatedAt(std::chrono::system_clock::now());
    entity.setUpdatedAt(std::chrono::system_clock::now());

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
    throw;
  }
}

std::vector<ProtocolEntity> ProtocolRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>> &result) {

  std::vector<ProtocolEntity> entities;
  entities.reserve(result.size());

  for (const auto &row : result) {
    try {
      entities.push_back(mapRowToEntity(row));
    } catch (const std::exception &e) {
      logger_->Warn(
          "ProtocolRepository::mapResultToEntities - Failed to map row: " +
          std::string(e.what()));
    }
  }

  return entities;
}

std::map<std::string, std::string>
ProtocolRepository::entityToParams(const ProtocolEntity &entity) {
  DbLib::DatabaseAbstractionLayer db_layer;

  std::map<std::string, std::string> params;

  // 기본 정보
  params["protocol_type"] = entity.getProtocolType();
  params["display_name"] = entity.getDisplayName();
  params["description"] = entity.getDescription();

  // 네트워크 정보
  if (entity.getDefaultPort().has_value()) {
    params["default_port"] = std::to_string(entity.getDefaultPort().value());
  } else {
    params["default_port"] = "NULL";
  }

  params["uses_serial"] = db_layer.formatBoolean(entity.usesSerial());
  params["requires_broker"] = db_layer.formatBoolean(entity.requiresBroker());

  // 지원 기능
  params["supported_operations"] = entity.getSupportedOperations();
  params["supported_data_types"] = entity.getSupportedDataTypes();
  params["connection_params"] = entity.getConnectionParams();

  // 설정 정보
  params["default_polling_interval"] =
      std::to_string(entity.getDefaultPollingInterval());
  params["default_timeout"] = std::to_string(entity.getDefaultTimeout());
  params["max_concurrent_connections"] =
      std::to_string(entity.getMaxConcurrentConnections());

  // 상태 정보
  params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
  params["is_deprecated"] = db_layer.formatBoolean(entity.isDeprecated());
  params["min_firmware_version"] = entity.getMinFirmwareVersion();

  // 분류 정보
  params["category"] = entity.getCategory();
  params["vendor"] = entity.getVendor();
  params["standard_reference"] = entity.getStandardReference();
  // 🔥 시간 정보
  params["created_at"] = db_layer.getGenericTimestamp();
  params["updated_at"] = db_layer.getGenericTimestamp();

  return params;
}

bool ProtocolRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeCreateTable(SQL::Protocol::CREATE_TABLE);

    if (success) {
      logger_->Debug("ProtocolRepository::ensureTableExists - Table "
                     "creation/check completed");
    } else {
      logger_->Error(
          "ProtocolRepository::ensureTableExists - Table creation failed");
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("ProtocolRepository::ensureTableExists failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool ProtocolRepository::validateProtocol(const ProtocolEntity &entity) const {
  if (!entity.isValid()) {
    logger_->Warn("ProtocolRepository::validateProtocol - Invalid protocol: " +
                  entity.getDisplayName());
    return false;
  }

  if (entity.getProtocolType().empty()) {
    logger_->Warn(
        "ProtocolRepository::validateProtocol - Protocol type is empty");
    return false;
  }

  if (entity.getDisplayName().empty()) {
    logger_->Warn(
        "ProtocolRepository::validateProtocol - Display name is empty for: " +
        entity.getProtocolType());
    return false;
  }

  return true;
}

bool ProtocolRepository::jsonContains(const std::string &json_field,
                                      const std::string &search_value) const {
  try {
    if (json_field.empty() || json_field == "[]" || json_field == "{}") {
      return false;
    }

    // 간단한 문자열 검색 (완전한 JSON 파싱 대신)
    return json_field.find("\"" + search_value + "\"") != std::string::npos;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ProtocolRepository::jsonContains failed: " +
                     std::string(e.what()));
    }
    return false;
  }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne