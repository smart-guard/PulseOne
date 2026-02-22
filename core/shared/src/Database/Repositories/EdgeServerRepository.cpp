#include "Database/Repositories/EdgeServerRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

EdgeServerRepository::EdgeServerRepository()
    : IRepository<EdgeServerEntity>("EdgeServerRepository") {}

std::vector<EdgeServerEntity> EdgeServerRepository::findAll() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(SQL::EdgeServer::FIND_ALL);
    return mapResultToEntities(results);
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::optional<EdgeServerEntity> EdgeServerRepository::findById(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::EdgeServer::FIND_BY_ID, std::to_string(id));
    auto results = db_layer.executeQuery(query);

    if (!results.empty()) {
      return mapRowToEntity(results[0]);
    }
    return std::nullopt;
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::findById failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

bool EdgeServerRepository::save(EdgeServerEntity &entity) {
  // Basic implementation of save (UPSERT)
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    std::map<std::string, std::string> data;

    if (entity.getId() > 0) {
      data["id"] = std::to_string(entity.getId());
    }
    data["tenant_id"] = std::to_string(entity.getTenantId());
    data["server_name"] = entity.getName();
    data["factory_name"] = ""; // Placeholder if needed
    data["location"] = "";     // Placeholder if needed
    data["ip_address"] = entity.getIpAddress();
    data["port"] = std::to_string(entity.getPort());
    data["status"] = entity.isEnabled() ? "active" : "inactive";
    data["subscription_mode"] = entity.getSubscriptionMode();
    data["config"] = entity.getConfig().dump();
    data["updated_at"] =
        RepositoryHelpers::formatTimestamp(std::chrono::system_clock::now());
    data["is_deleted"] = "0";

    std::vector<std::string> primary_keys = {"id"};

    // If ID is provided and > 0, we try to insert with that ID.
    // Note: executeUpsert relies on primary keys to determine update vs insert.
    // If the ID doesn't exist, it will insert.
    bool success = db_layer.executeUpsert("edge_servers", data, primary_keys);

    if (success && entity.getId() <= 0) {
      auto id_result = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
      if (!id_result.empty()) {
        entity.setId(std::stoi(id_result[0].at("id")));
      }
    }
    return success;
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::save failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool EdgeServerRepository::update(const EdgeServerEntity &entity) {
  EdgeServerEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool EdgeServerRepository::deleteById(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query =
        "DELETE FROM edge_servers WHERE id = " + std::to_string(id);
    return db_layer.executeNonQuery(query);
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool EdgeServerRepository::exists(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query =
        "SELECT COUNT(*) as count FROM edge_servers WHERE id = " +
        std::to_string(id);
    auto results = db_layer.executeQuery(query);
    if (!results.empty()) {
      return std::stoi(results[0].at("count")) > 0;
    }
    return false;
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::exists failed: " +
                   std::string(e.what()));
    return false;
  }
}

EdgeServerEntity EdgeServerRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  EdgeServerEntity entity;
  try {
    if (row.count("id"))
      entity.setId(std::stoi(row.at("id")));
    if (row.count("tenant_id"))
      entity.setTenantId(std::stoi(row.at("tenant_id")));
    if (row.count("server_name"))
      entity.setName(row.at("server_name"));
    else if (row.count("name"))
      entity.setName(row.at("name")); // fallback for compatibility
    if (row.count("status"))
      entity.setEnabled(row.at("status") == "active");
    if (row.count("ip_address"))
      entity.setIpAddress(row.at("ip_address"));
    if (row.count("port"))
      entity.setPort(std::stoi(row.at("port")));
    if (row.count("subscription_mode"))
      entity.setSubscriptionMode(row.at("subscription_mode"));
    if (row.count("config") && !row.at("config").empty())
      entity.setConfig(json::parse(row.at("config")));
  } catch (const std::exception &e) {
    logger_->Error("EdgeServerRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
  }
  return entity;
}

std::vector<EdgeServerEntity> EdgeServerRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>> &result) {
  std::vector<EdgeServerEntity> entities;
  entities.reserve(result.size());
  for (const auto &row : result) {
    entities.push_back(mapRowToEntity(row));
  }
  return entities;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne
