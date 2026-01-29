/**
 * @file ExportTargetMappingRepository.cpp
 * @brief Export Target Mapping Repository 구현 - std::map 순서 문제 수정 완료
 */

#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/ExportSQLQueries.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::ExportTargetMapping::FIND_ALL);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        if (logger_) {
          logger_->Error(
              "ExportTargetMappingRepository::findAll - Failed to map row: " +
              std::string(e.what()));
        }
      } catch (...) {
        if (logger_) {
          logger_->Error(
              "ExportTargetMappingRepository::findAll - Unknown exception");
        }
      }
    }

    return entities;
  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ExportTargetMappingRepository::findAll failed: " +
                     std::string(e.what()));
    }
    return {};
  }
}

std::optional<ExportTargetMappingEntity>
ExportTargetMappingRepository::findById(int id) {
  try {
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::FIND_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    return entity;
  } catch (...) {
    return std::nullopt;
  }
}

bool ExportTargetMappingRepository::save(ExportTargetMappingEntity &entity) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    auto params = entityToParams(entity);
    std::string query = SQL::ExportTargetMapping::INSERT;

    // ✅ INSERT 쿼리의 컬럼 순서대로 파라미터 치환
    // INSERT INTO export_target_mappings (
    //     target_id, point_id, target_field_name, target_description,
    //     conversion_config, is_enabled
    // ) VALUES (?, ?, ?, ?, ?, ?)

    std::vector<std::string> insert_order = {
        "target_id",         "point_id",           "site_id",
        "target_field_name", "target_description", "conversion_config",
        "is_enabled"};

    for (const auto &key : insert_order) {
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        query.replace(pos, 1, "'" + params[key] + "'");
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
    if (logger_) {
      logger_->Error("ExportTargetMappingRepository::save failed: " +
                     std::string(e.what()));
    }
    return false;
  }
}

bool ExportTargetMappingRepository::update(
    const ExportTargetMappingEntity &entity) {
  try {
    if (!ensureTableExists() || entity.getId() <= 0) {
      return false;
    }

    auto params = entityToParams(entity);
    std::string query = SQL::ExportTargetMapping::UPDATE;

    // ✅ UPDATE 쿼리의 SET 절 순서대로 파라미터 치환
    // UPDATE export_target_mappings SET
    //     target_id = ?,
    //     point_id = ?,
    //     target_field_name = ?,
    //     target_description = ?,
    //     conversion_config = ?,
    //     is_enabled = ?
    // WHERE id = ?

    std::vector<std::string> update_order = {
        "target_id",         "point_id",           "site_id",
        "target_field_name", "target_description", "conversion_config",
        "is_enabled"};

    for (const auto &key : update_order) {
      size_t pos = query.find('?');
      if (pos != std::string::npos) {
        query.replace(pos, 1, "'" + params[key] + "'");
      }
    }

    // WHERE id = ? 치환
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
    if (logger_) {
      logger_->Error("ExportTargetMappingRepository::update failed: " +
                     std::string(e.what()));
    }
    return false;
  }
}

bool ExportTargetMappingRepository::deleteById(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::DELETE_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCacheForId(id);
    }

    return success;
  } catch (...) {
    return false;
  }
}

bool ExportTargetMappingRepository::exists(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::EXISTS_BY_ID, std::to_string(id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count")) > 0;
    }

    return false;
  } catch (...) {
    return false;
  }
}

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findByIds(const std::vector<int> &ids) {

  try {
    if (ids.empty() || !ensureTableExists()) {
      return {};
    }

    std::ostringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ", ";
      ids_ss << ids[i];
    }

    std::string query = "SELECT * FROM export_target_mappings WHERE id IN (" +
                        ids_ss.str() + ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (...) {
      }
    }

    return entities;
  } catch (...) {
    return {};
  }
}

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = "SELECT * FROM export_target_mappings";
    query += RepositoryHelpers::buildWhereClause(conditions);
    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (...) {
      }
    }

    return entities;
  } catch (...) {
    return {};
  }
}

int ExportTargetMappingRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = "SELECT COUNT(*) as count FROM export_target_mappings";
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;
  } catch (...) {
    return 0;
  }
}

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findByTargetId(int target_id) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::FIND_BY_TARGET_ID, std::to_string(target_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        if (logger_) {
          logger_->Error("Failed to map mapping entity: " +
                         std::string(e.what()));
        }
      } catch (...) {
      }
    }

    if (logger_) {
      logger_->Info("ExportTargetMappingRepository::findByTargetId - Found " +
                    std::to_string(entities.size()) +
                    " mappings for target_id=" + std::to_string(target_id));
    }

    return entities;
  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("ExportTargetMappingRepository::findByTargetId failed: " +
                     std::string(e.what()));
    }
    return {};
  }
}

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findByPointId(int point_id) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::FIND_BY_POINT_ID, std::to_string(point_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (...) {
      }
    }

    return entities;
  } catch (...) {
    return {};
  }
}

std::optional<ExportTargetMappingEntity>
ExportTargetMappingRepository::findByTargetAndPoint(int target_id,
                                                    int point_id) {

  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    std::string query = RepositoryHelpers::replaceTwoParameters(
        SQL::ExportTargetMapping::FIND_BY_TARGET_AND_POINT,
        std::to_string(target_id), std::to_string(point_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      return std::nullopt;
    }

    return mapRowToEntity(results[0]);
  } catch (...) {
    return std::nullopt;
  }
}

std::vector<ExportTargetMappingEntity>
ExportTargetMappingRepository::findEnabledByTargetId(int target_id) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::FIND_ENABLED_BY_TARGET_ID,
        std::to_string(target_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<ExportTargetMappingEntity> entities;
    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (...) {
      }
    }

    return entities;
  } catch (...) {
    return {};
  }
}

int ExportTargetMappingRepository::deleteByTargetId(int target_id) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::DELETE_BY_TARGET_ID,
        std::to_string(target_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCache();
    }

    return success ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int ExportTargetMappingRepository::deleteByPointId(int point_id) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::DELETE_BY_POINT_ID, std::to_string(point_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success && isCacheEnabled()) {
      clearCache();
    }

    return success ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int ExportTargetMappingRepository::countByTargetId(int target_id) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::COUNT_BY_TARGET_ID,
        std::to_string(target_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;
  } catch (...) {
    return 0;
  }
}

int ExportTargetMappingRepository::countByPointId(int point_id) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = RepositoryHelpers::replaceParameter(
        SQL::ExportTargetMapping::COUNT_BY_POINT_ID, std::to_string(point_id));

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;
  } catch (...) {
    return 0;
  }
}

std::map<std::string, int>
ExportTargetMappingRepository::getCacheStats() const {
  std::map<std::string, int> stats;

  if (isCacheEnabled()) {
    stats["cache_enabled"] = 1;
    auto base_stats = IRepository<ExportTargetMappingEntity>::getCacheStats();
    stats.insert(base_stats.begin(), base_stats.end());
  } else {
    stats["cache_enabled"] = 0;
  }

  return stats;
}

ExportTargetMappingEntity ExportTargetMappingRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {

  ExportTargetMappingEntity entity;

  try {
    auto it = row.find("id");
    if (it != row.end() && !it->second.empty()) {
      entity.setId(std::stoi(it->second));
    }

    it = row.find("target_id");
    if (it != row.end() && !it->second.empty()) {
      entity.setTargetId(std::stoi(it->second));
    }

    it = row.find("point_id");
    if (it != row.end() && !it->second.empty()) {
      entity.setPointId(std::stoi(it->second));
    } else {
      entity.setPointId(std::nullopt);
    }

    it = row.find("site_id");
    if (it != row.end() && !it->second.empty()) {
      entity.setSiteId(std::stoi(it->second));
    } else {
      entity.setSiteId(std::nullopt);
    }

    it = row.find("target_field_name");
    if (it != row.end()) {
      entity.setTargetFieldName(it->second);
    }

    it = row.find("target_description");
    if (it != row.end()) {
      entity.setTargetDescription(it->second);
    }

    it = row.find("conversion_config");
    if (it != row.end()) {
      entity.setConversionConfig(it->second);
    }

    it = row.find("is_enabled");
    if (it != row.end() && !it->second.empty()) {
      entity.setEnabled(std::stoi(it->second) != 0);
    }

  } catch (const std::exception &e) {
    throw std::runtime_error(
        "Failed to map row to ExportTargetMappingEntity: " +
        std::string(e.what()));
  }

  return entity;
}

std::map<std::string, std::string>
ExportTargetMappingRepository::entityToParams(
    const ExportTargetMappingEntity &entity) {

  std::map<std::string, std::string> params;

  params["target_id"] = std::to_string(entity.getTargetId());
  params["point_id"] = entity.getPointId().has_value()
                           ? std::to_string(entity.getPointId().value())
                           : "NULL";
  params["site_id"] = entity.getSiteId().has_value()
                          ? std::to_string(entity.getSiteId().value())
                          : "NULL";
  params["target_field_name"] = entity.getTargetFieldName();
  params["target_description"] = entity.getTargetDescription();
  params["conversion_config"] = entity.getConversionConfig();
  params["is_enabled"] = entity.isEnabled() ? "1" : "0";

  return params;
}

bool ExportTargetMappingRepository::validateMapping(
    const ExportTargetMappingEntity &entity) {

  if (entity.getTargetId() <= 0)
    return false;
  if (entity.getPointId() <= 0)
    return false;
  if (entity.getTargetFieldName().empty())
    return false;

  return true;
}

bool ExportTargetMappingRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    return db_layer.executeNonQuery(SQL::ExportTargetMapping::CREATE_TABLE);
  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error(
          "ExportTargetMappingRepository::ensureTableExists failed: " +
          std::string(e.what()));
    }
    return false;
  }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne