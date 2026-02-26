#include "Database/Repositories/SystemSettingsRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "DatabaseAbstractionLayer.hpp"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

SystemSettingsRepository::SystemSettingsRepository()
    : IRepository<SystemSettingsEntity>("SystemSettingsRepository") {}

// -----------------------------------------------------------------------------
// findAll
// -----------------------------------------------------------------------------
std::vector<SystemSettingsEntity> SystemSettingsRepository::findAll() {
  try {
    DbLib::DatabaseAbstractionLayer db;
    auto results = db.executeQuery("SELECT id, key_name, value, category FROM "
                                   "system_settings ORDER BY key_name ASC");
    return mapResultToEntities(results);
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

// -----------------------------------------------------------------------------
// findById (id 컬럼 기준)
// -----------------------------------------------------------------------------
std::optional<SystemSettingsEntity> SystemSettingsRepository::findById(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    std::string query = "SELECT id, key_name, value, category FROM "
                        "system_settings WHERE id = " +
                        std::to_string(id);
    auto results = db.executeQuery(query);
    if (!results.empty())
      return mapRowToEntity(results[0]);
    return std::nullopt;
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::findById failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
// findByKey — key_name 으로 조회
// -----------------------------------------------------------------------------
std::optional<SystemSettingsEntity>
SystemSettingsRepository::findByKey(const std::string &key) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    std::string query = "SELECT id, key_name, value, category FROM "
                        "system_settings WHERE key_name = '" +
                        key + "'";
    auto results = db.executeQuery(query);
    if (!results.empty())
      return mapRowToEntity(results[0]);
    return std::nullopt;
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::findByKey failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
// getValue — key_name 으로 value 문자열만 반환
// -----------------------------------------------------------------------------
std::string
SystemSettingsRepository::getValue(const std::string &key,
                                   const std::string &defaultValue) {
  auto entity = findByKey(key);
  if (entity.has_value())
    return entity->getValue();
  return defaultValue;
}

// -----------------------------------------------------------------------------
// updateValue — upsert (없으면 INSERT, 있으면 UPDATE)
// -----------------------------------------------------------------------------
bool SystemSettingsRepository::updateValue(const std::string &key,
                                           const std::string &value) {
  try {
    DbLib::DatabaseAbstractionLayer db;

    // 존재 여부 확인
    auto existing = findByKey(key);

    if (existing.has_value()) {
      // UPDATE
      std::string query = "UPDATE system_settings SET value = '" + value +
                          "', updated_at = NOW() WHERE key_name = '" + key +
                          "'";
      return db.executeNonQuery(query);
    } else {
      // INSERT
      std::string query = "INSERT INTO system_settings (key_name, value, "
                          "updated_at) VALUES ('" +
                          key + "', '" + value + "', NOW())";
      return db.executeNonQuery(query);
    }
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::updateValue failed: " +
                   std::string(e.what()));
    return false;
  }
}

// -----------------------------------------------------------------------------
// save / update / deleteById / exists
// -----------------------------------------------------------------------------
bool SystemSettingsRepository::save(SystemSettingsEntity &entity) {
  return updateValue(entity.getKeyName(), entity.getValue());
}

bool SystemSettingsRepository::update(const SystemSettingsEntity &entity) {
  return updateValue(entity.getKeyName(), entity.getValue());
}

bool SystemSettingsRepository::deleteById(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    return db.executeNonQuery("DELETE FROM system_settings WHERE id = " +
                              std::to_string(id));
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool SystemSettingsRepository::exists(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    auto results = db.executeQuery(
        "SELECT COUNT(*) as count FROM system_settings WHERE id = " +
        std::to_string(id));
    if (!results.empty())
      return std::stoi(results[0].at("count")) > 0;
    return false;
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::exists failed: " +
                   std::string(e.what()));
    return false;
  }
}

// -----------------------------------------------------------------------------
// mapRowToEntity
// -----------------------------------------------------------------------------
SystemSettingsEntity SystemSettingsRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  SystemSettingsEntity entity;
  try {
    if (row.count("id"))
      entity.setId(std::stoi(row.at("id")));
    if (row.count("key_name"))
      entity.setKeyName(row.at("key_name"));
    if (row.count("value"))
      entity.setValue(row.at("value"));
    if (row.count("category"))
      entity.setCategory(row.at("category"));
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
  }
  return entity;
}

std::vector<SystemSettingsEntity> SystemSettingsRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>> &result) {
  std::vector<SystemSettingsEntity> entities;
  entities.reserve(result.size());
  for (const auto &row : result) {
    entities.push_back(mapRowToEntity(row));
  }
  return entities;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne
